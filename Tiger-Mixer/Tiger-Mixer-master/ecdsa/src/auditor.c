#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>
#include <unistd.h>
#include "auditor.h"
#include "types.h"
#include "util.h"
#include "secret_share.h"
#include "pedersen_dkg.h"
#include <pari/pari.h>

// 函数声明
static void parse_tumbler_second_share(const uint8_t *buf, size_t len, auditor_state_t state);
static void perform_alice_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length);
static void perform_bob_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length);
static void perform_tumbler_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length);
static void perform_comprehensive_consistency_verification(auditor_state_t state);
void audit_message_with_info(char* msg_id, char* sender_address, char* pairs_summary_line, auditor_state_t state);


// VSS 相关函数实现 - 现在使用文件存储，不再需要内存存储

int verify_share_with_stored_commitment(auditor_state_t state, const secret_share_t* share, const char* msgid) {
    // 从文件加载 VSS 承诺
    vss_commitment_t commitment;
    if (load_vss_commitment_from_file(msgid, &commitment) != 0) {
        printf("[VSS][Auditor] Error: Cannot load VSS commitment from file for msgid: %s\n", msgid);
        return -1;
    }
    
    // 验证分片
    int result = verify_share_with_commitment(share, &commitment);
    if (result == 0) {
        printf("[VSS][Auditor] Share verification successful for msgid: %s, x=%d\n", msgid, share->x);
    } else {
        printf("[VSS][Auditor] Share verification failed for msgid: %s, x=%d\n", msgid, share->x);
    }
    
    return result;
}

// 不再需要 ZMQ 服务器，因为现在使用文件存储
int start_vss_commitment_server(auditor_state_t state) {
    printf("[VSS][Auditor] VSS commitment server disabled - using file-based storage\n");
    printf("[VSS][Auditor] Available VSS commitment files:\n");
    list_vss_commitment_files();
    return 0;
}

GEN saved_r0 = NULL;
int has_saved_r0 = 0;
GEN saved_r0_beta_tau = NULL;
int has_saved_r0_beta_tau = 0;

#define RLC_EC_SIZE_COMPRESSED 33
// 移除宏重定义，使用util.h中的定义

// 新增：读取椭圆曲线公钥的函数
int read_ec_public_key_from_file(const char* filename, ec_public_key_t public_key) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("[AUDITOR ERROR] 无法打开文件: %s\n", filename);
        return RLC_ERR;
    }
    
    // 读取公钥数据并解析
    uint8_t key_data[RLC_EC_SIZE_COMPRESSED];
    if (fread(key_data, 1, RLC_EC_SIZE_COMPRESSED, file) != RLC_EC_SIZE_COMPRESSED) {
        printf("[AUDITOR ERROR] 读取公钥数据失败: %s\n", filename);
        fclose(file);
        return RLC_ERR;
    }
    
    ec_read_bin(public_key->pk, key_data, RLC_EC_SIZE_COMPRESSED);
    fclose(file);
    printf("[AUDITOR] 成功读取公钥文件: %s\n", filename);
    return RLC_OK;
}

// 新增：读取Alice、Bob和Tumbler公钥的函数
int read_alice_bob_tumbler_public_keys(auditor_state_t state) {
    printf("[AUDITOR] 开始读取Alice、Bob和Tumbler的公钥...\n");
    
    int result_status = RLC_OK;
    uint8_t serialized_ec_pk[RLC_EC_SIZE_COMPRESSED];
    char serialized_cl_pk[RLC_CL_PUBLIC_KEY_SIZE];
    
    RLC_TRY {
        // === 读取Alice EC公钥 ===
        unsigned key_file_length = strlen(ALICE_KEY_FILE_PREFIX) + strlen(KEY_FILE_EXTENSION) + 10;
        char *key_file_name = malloc(key_file_length);
        if (key_file_name == NULL) {
            printf("[AUDITOR ERROR] malloc for alice key file name failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        snprintf(key_file_name, key_file_length, "../keys/%s.%s", ALICE_KEY_FILE_PREFIX, KEY_FILE_EXTENSION);
        printf("[AUDITOR] 打开Alice密钥文件: %s\n", key_file_name);
        FILE *file = fopen(key_file_name, "rb");
        if (file == NULL) {
            printf("[AUDITOR ERROR] 无法打开Alice密钥文件: %s\n", key_file_name);
            free(key_file_name);
            RLC_THROW(ERR_NO_FILE);
        }
        
        // 跳过私钥，读取公钥
        fseek(file, RLC_BN_SIZE, SEEK_SET);
        if (fread(serialized_ec_pk, sizeof(uint8_t), RLC_EC_SIZE_COMPRESSED, file) != RLC_EC_SIZE_COMPRESSED) {
            printf("[AUDITOR ERROR] 读取Alice EC公钥失败\n");
            fclose(file);
            free(key_file_name);
            RLC_THROW(ERR_NO_READ);
        }
        printf("[AUDITOR] ✅ 成功读取Alice EC公钥\n");
        ec_read_bin(state->alice_ec_pk->pk, serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        memzero(serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        fclose(file);
        free(key_file_name);
        
        // === 读取Bob EC公钥 ===
        key_file_length = strlen(BOB_KEY_FILE_PREFIX) + strlen(KEY_FILE_EXTENSION) + 10;
        key_file_name = malloc(key_file_length);
        if (key_file_name == NULL) {
            printf("[AUDITOR ERROR] malloc for bob key file name failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        snprintf(key_file_name, key_file_length, "../keys/%s.%s", BOB_KEY_FILE_PREFIX, KEY_FILE_EXTENSION);
        printf("[AUDITOR] 打开Bob密钥文件: %s\n", key_file_name);
        file = fopen(key_file_name, "rb");
        if (file == NULL) {
            printf("[AUDITOR ERROR] 无法打开Bob密钥文件: %s\n", key_file_name);
            free(key_file_name);
            RLC_THROW(ERR_NO_FILE);
        }
        
        // 跳过私钥，读取公钥
        fseek(file, RLC_BN_SIZE, SEEK_SET);
        if (fread(serialized_ec_pk, sizeof(uint8_t), RLC_EC_SIZE_COMPRESSED, file) != RLC_EC_SIZE_COMPRESSED) {
            printf("[AUDITOR ERROR] 读取Bob EC公钥失败\n");
            fclose(file);
            free(key_file_name);
            RLC_THROW(ERR_NO_READ);
        }
        printf("[AUDITOR] ✅ 成功读取Bob EC公钥\n");
        ec_read_bin(state->bob_ec_pk->pk, serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        memzero(serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        fclose(file);
        free(key_file_name);
        
        // === 读取Tumbler EC公钥和CL公钥 ===
        key_file_length = strlen(TUMBLER_KEY_FILE_PREFIX) + strlen(KEY_FILE_EXTENSION) + 10;
        key_file_name = malloc(key_file_length);
        if (key_file_name == NULL) {
            printf("[AUDITOR ERROR] malloc for tumbler key file name failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        snprintf(key_file_name, key_file_length, "../keys/%s.%s", TUMBLER_KEY_FILE_PREFIX, KEY_FILE_EXTENSION);
        printf("[AUDITOR] 打开Tumbler密钥文件: %s\n", key_file_name);
        file = fopen(key_file_name, "rb");
        if (file == NULL) {
            printf("[AUDITOR ERROR] 无法打开Tumbler密钥文件: %s\n", key_file_name);
            free(key_file_name);
            RLC_THROW(ERR_NO_FILE);
        }
        
        // 跳过私钥，读取EC公钥
        fseek(file, RLC_BN_SIZE, SEEK_SET);
        if (fread(serialized_ec_pk, sizeof(uint8_t), RLC_EC_SIZE_COMPRESSED, file) != RLC_EC_SIZE_COMPRESSED) {
            printf("[AUDITOR ERROR] 读取Tumbler EC公钥失败\n");
            fclose(file);
            free(key_file_name);
            RLC_THROW(ERR_NO_READ);
        }
        printf("[AUDITOR] ✅ 成功读取Tumbler EC公钥\n");
        ec_read_bin(state->tumbler_ec_pk->pk, serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        memzero(serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
        
        // 跳过CL私钥
        if (fread(serialized_cl_pk, sizeof(char), RLC_CL_SECRET_KEY_SIZE, file) != RLC_CL_SECRET_KEY_SIZE) {
            printf("[AUDITOR ERROR] 跳过Tumbler CL私钥失败\n");
            fclose(file);
            free(key_file_name);
            RLC_THROW(ERR_NO_READ);
        }
        
        // 读取Tumbler CL公钥
        if (fread(serialized_cl_pk, sizeof(char), RLC_CL_PUBLIC_KEY_SIZE, file) != RLC_CL_PUBLIC_KEY_SIZE) {
            printf("[AUDITOR ERROR] 读取Tumbler CL公钥失败\n");
            fclose(file);
            free(key_file_name);
            RLC_THROW(ERR_NO_READ);
        }
        printf("[AUDITOR] ✅ 成功读取Tumbler CL公钥\n");
        state->tumbler_cl_pk->pk = gp_read_str(serialized_cl_pk);
        
        fclose(file);
        free(key_file_name);
        
        printf("[AUDITOR] ========== 所有公钥读取成功 ==========\n");
        
    } RLC_CATCH_ANY {
        result_status = RLC_ERR;
    }
    
    return result_status;
}

// ========== 新增：按 msgid 请求并重构分片 ==========
static int fetch_shares_by_msgid(const char *msg_id, int tag, const char *pairs_summary_json, uint8_t **out_buf, size_t *out_len, auditor_state_t state) {
    if (msg_id == NULL || out_buf == NULL || out_len == NULL) return RLC_ERR;
    if (tag != 0 && tag != 1) {
        printf("[AUDITOR ERROR] Invalid tag: %d (must be 0 or 1)\n", tag);
        return RLC_ERR;
    }
    *out_buf = NULL;
    *out_len = 0;

    void* context = zmq_ctx_new();
    if (!context) {
        printf("[AUDITOR ERROR] zmq_ctx_new failed\n");
        return RLC_ERR;
    }

    // 动态分配数组以存储所有块的分享（每个参与者可能有多个块）
    secret_share_t *shares = NULL;
    int share_count = 0;
    size_t shares_capacity = 0;

    for (int i = 0; i < SECRET_SHARES; i++) {
        void* socket = zmq_socket(context, ZMQ_REQ);
        if (!socket) {
            printf("[AUDITOR ERROR] zmq_socket failed at %d\n", i);
            continue;
        }
        int rc = zmq_connect(socket, RECEIVER_ENDPOINTS[i]);
        if (rc != 0) {
            printf("[AUDITOR ERROR] connect %s failed: %s\n", RECEIVER_ENDPOINTS[i], zmq_strerror(zmq_errno()));
            zmq_close(socket);
            continue;
        }

        // 构造请求数据：tag(1字节) + msg_id(字符串) + '\0' + pairs_summary_json(字符串) + '\0'
        size_t msg_id_len = strlen(msg_id);
        size_t json_len = (pairs_summary_json != NULL) ? strlen(pairs_summary_json) : 0;
        size_t data_len = 1 + msg_id_len + 1 + json_len + 1;  // tag(1) + msg_id + '\0' + json + '\0'

        message_t request; message_null(request);
        message_new(request, strlen("AUDIT_REQUEST") + 1, data_len);
        strcpy(request->type, "AUDIT_REQUEST");
        
        // 初始化data为0，确保所有字节都被正确设置
        memset(request->data, 0, data_len);
        
        // 数据格式: [tag(1字节)] [msg_id(字符串)] ['\0'] [pairs_summary_json(字符串)] ['\0']
        size_t offset = 0;
        request->data[offset++] = (uint8_t)tag;  // tag
        memcpy(request->data + offset, msg_id, msg_id_len);
        offset += msg_id_len;
        request->data[offset++] = '\0';  // msg_id结束符
        if (pairs_summary_json != NULL && json_len > 0) {
            memcpy(request->data + offset, pairs_summary_json, json_len);
            offset += json_len;
        }
        request->data[offset] = '\0';  // json结束符
        
        // 添加调试信息：打印构造的数据
        printf("[AUDITOR] ========== 构造请求数据 ==========\n");
        printf("[AUDITOR] msg_id=%s (length: %zu)\n", msg_id, msg_id_len);
        printf("[AUDITOR] tag=%d, json_len=%zu, data_len=%zu\n", tag, json_len, data_len);
        printf("[AUDITOR] First 100 bytes of request->data: ");
        size_t debug_len = (data_len < 100) ? data_len : 100;
        for (size_t i = 0; i < debug_len; i++) {
            if (request->data[i] >= 32 && request->data[i] < 127) {
                printf("%c", request->data[i]);
            } else {
                printf("\\x%02x", (unsigned char)request->data[i]);
            }
        }
        printf("\n");
        if (pairs_summary_json != NULL) {
            printf("[AUDITOR] pairs_summary_json: %s\n", pairs_summary_json);
        }
        printf("[AUDITOR] ===================================\n");
        uint8_t *serialized = NULL;
        unsigned msg_type_length = strlen(request->type) + 1;
        unsigned msg_data_length = data_len;  // 使用计算好的数据长度
        serialize_message(&serialized, request, msg_type_length, msg_data_length);
        size_t serialized_len = msg_type_length + msg_data_length + 2 * sizeof(unsigned);

        int send_rc = zmq_send(socket, serialized, serialized_len, 0);
        if (send_rc != (int)serialized_len) {
            printf("[AUDITOR ERROR] send request failed to %s: %s\n", RECEIVER_ENDPOINTS[i], zmq_strerror(zmq_errno()));
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;
        }

        zmq_msg_t response; zmq_msg_init(&response);
        int recv_rc = zmq_msg_recv(&response, socket, 0);
        if (recv_rc < 0) {
            printf("[AUDITOR ERROR] recv response failed from %s: %s\n", RECEIVER_ENDPOINTS[i], zmq_strerror(zmq_errno()));
            zmq_msg_close(&response);
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;
        }

        uint8_t *data = zmq_msg_data(&response);
        size_t size = zmq_msg_size(&response);
        
        // 检查是否是 "NO_AUDIT_NEEDED" 响应（用户身份合法，不需要审计）
        // 注意：即使收到NO_AUDIT_NEEDED，我们仍然继续向其他成员发送请求，以便记录所有成员的决策
        // 这对于计算一致性和完整的决策历史很重要
        int is_no_audit_needed = (size > 0 && strncmp((char*)data, "NO_AUDIT_NEEDED", 15) == 0);
        if (is_no_audit_needed) {
            printf("[AUDITOR] Receiver %d: ✅ 用户身份合法，无需审计（但继续收集其他成员的决策）\n", i + 1);
            // 不直接返回，继续处理其他receiver，以便记录所有成员的决策
            // 标记为"不需要审计"，但继续循环
            zmq_msg_close(&response);
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;  // 继续处理下一个receiver，而不是直接返回
        }
        
        // 检查是否是错误响应
        if (size > 0 && strncmp((char*)data, "NOT_FOUND", 9) != 0 && 
            strncmp((char*)data, "INSUFFICIENT", 12) != 0 &&
            strncmp((char*)data, "MEMORY_ERROR", 12) != 0) {
            
            // 解析新格式：participant_id || num_blocks || (block_index || block_size || data_length || share_value)...
            if (size >= sizeof(int) + sizeof(size_t)) {
                int participant_id;
                size_t num_blocks;
                memcpy(&participant_id, data, sizeof(int));
                memcpy(&num_blocks, data + sizeof(int), sizeof(size_t));
                
                printf("[AUDITOR] Received %zu blocks from participant %d\n", num_blocks, participant_id);
                
                // 确保有足够的空间
                if (share_count + num_blocks > shares_capacity) {
                    shares_capacity = share_count + num_blocks + 100;  // 额外100个作为缓冲
                    shares = realloc(shares, shares_capacity * sizeof(secret_share_t));
                    if (!shares) {
                        printf("[AUDITOR ERROR] Memory allocation failed\n");
                        zmq_msg_close(&response);
                        zmq_close(socket);
                        free(serialized);
                        message_free(request);
                        continue;
                    }
                }
                
                // 解析每个块的分享
                size_t offset = sizeof(int) + sizeof(size_t);
                for (size_t block_idx = 0; block_idx < num_blocks && offset < size; block_idx++) {
                    if (offset + sizeof(size_t) * 3 + 4 > size) {
                        printf("[AUDITOR ERROR] Insufficient data for block %zu\n", block_idx);
                        break;
                    }
                    
                    secret_share_t *share = &shares[share_count];
                    memset(share, 0, sizeof(secret_share_t));
                    share->x = participant_id;
                    
                    memcpy(&share->block_index, data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    memcpy(&share->block_size, data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    memcpy(&share->data_length, data + offset, sizeof(size_t));
                    offset += sizeof(size_t);
                    
                    // 读取share_value的大小
                    size_t share_data_size = data[offset] |
                                            (data[offset + 1] << 8) |
                                            (data[offset + 2] << 16) |
                                            (data[offset + 3] << 24);
                    offset += 4;
                    
                    if (offset + share_data_size > size) {
                        printf("[AUDITOR ERROR] Insufficient data for block %zu share value\n", block_idx);
                        break;
                    }
                    
                    if (share_data_size + 4 > SHARE_SIZE) {
                        printf("[AUDITOR ERROR] Share value too large for block %zu\n", block_idx);
                        break;
                    }
                    
                    memcpy(share->y, data + offset - 4, 4 + share_data_size);
                    offset += share_data_size;
                    share->is_valid = 1;
                    share_count++;
                }
            } else {
                // 尝试旧格式（兼容性）
                secret_share_t *share;
                if (share_count >= shares_capacity) {
                    shares_capacity = share_count + 10;
                    shares = realloc(shares, shares_capacity * sizeof(secret_share_t));
                    if (!shares) {
                        printf("[AUDITOR ERROR] Memory allocation failed\n");
                        zmq_msg_close(&response);
                        zmq_close(socket);
                        free(serialized);
                        message_free(request);
                        continue;
                    }
                }
                share = &shares[share_count];
                memcpy(&share->x, data, sizeof(int));
                memcpy(&share->data_length, data + sizeof(int), sizeof(size_t));
                memcpy(share->y, data + sizeof(int) + sizeof(size_t), share->data_length);
                share->block_index = 0;  // 默认值
                share->block_size = share->data_length;  // 默认值
                share->is_valid = 1;
                share_count++;
            }
        }

        zmq_msg_close(&response);
        zmq_close(socket);
        free(serialized);
        message_free(request);
    }

    // 检查是否所有成员都返回了NO_AUDIT_NEEDED（用于判断是否真的不需要审计）
    // 如果所有成员都返回NO_AUDIT_NEEDED，则返回特殊状态
    // 注意：这里我们仍然需要检查是否有足够的shares来重构
    // 如果share_count < THRESHOLD，说明所有成员都返回了NO_AUDIT_NEEDED或NOT_FOUND

    int ret = RLC_ERR;
    if (share_count >= THRESHOLD) {
        // ===== VSS: 验证所有分片 =====
        printf("[VSS][Auditor] Verifying %d shares before reconstruction\n", share_count);
        int all_shares_valid = 1;
        for (int i = 0; i < share_count; i++) {
            if (verify_share_with_stored_commitment(state, &shares[i], msg_id) != 0) {
                printf("[VSS][Auditor] Share %d (x=%d, block_index=%zu) verification failed\n", 
                       i, shares[i].x, shares[i].block_index);
                all_shares_valid = 0;
                break;
            }
        }
        
        if (all_shares_valid) {
            printf("[VSS][Auditor] All shares verified successfully, proceeding with reconstruction\n");
            
            // 调试：打印分片信息（只打印前几个）
            printf("[VSS][Auditor] 分片详细信息 (显示前10个):\n");
            int print_count = (share_count < 10) ? share_count : 10;
            for (int i = 0; i < print_count; i++) {
                printf("  - Share %d: x=%d, block_index=%zu, data_length=%zu\n", 
                       i, shares[i].x, shares[i].block_index, shares[i].data_length);
                printf("    前32字节: ");
                size_t y_data_size = shares[i].y[0] |
                                    (shares[i].y[1] << 8) |
                                    (shares[i].y[2] << 16) |
                                    (shares[i].y[3] << 24);
                for (size_t j = 4; j < 36 && j < (4 + y_data_size); j++) {
                    printf("%02x", shares[i].y[j]);
                }
                printf("\n");
            }
            if (share_count > 10) {
                printf("  ... (还有 %d 个分享)\n", share_count - 10);
            }
            
            uint8_t *reconstructed = (uint8_t*)malloc(MAX_MESSAGE_SIZE);
            size_t data_length = 0;
            if (reconstruct_secret_from_shares(shares, share_count, reconstructed, &data_length) == 0) {
                printf("[VSS][Auditor] 重构成功，数据长度: %zu\n", data_length);
                printf("[VSS][Auditor] 重构数据前64字节: ");
                for (size_t i = 0; i < 64 && i < data_length; i++) {
                    printf("%02x", reconstructed[i]);
                }
                printf("\n");
                
                // 保存重构后的消息到文件（用于调试）
                {
                    char filename[256];
                    // 处理文件名：移除 0x 前缀，替换特殊字符
                    char safe_msgid[128];
                    if (msg_id && strlen(msg_id) > 0) {
                        // 移除 0x 前缀（如果存在）
                        if (strncmp(msg_id, "0x", 2) == 0) {
                            strncpy(safe_msgid, msg_id + 2, sizeof(safe_msgid) - 1);
                        } else {
                            strncpy(safe_msgid, msg_id, sizeof(safe_msgid) - 1);
                        }
                        safe_msgid[sizeof(safe_msgid) - 1] = '\0';
                        // 替换可能存在的特殊字符
                        for (char *p = safe_msgid; *p; p++) {
                            if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
                                *p = '_';
                            }
                        }
                    } else {
                        strcpy(safe_msgid, "unknown");
                    }
                    snprintf(filename, sizeof(filename), "auditor_reconstructed_msg_%s.txt", safe_msgid);
                    FILE *fp = fopen(filename, "w");
                    if (fp) {
                        fprintf(fp, "Auditor Reconstructed Message\n");
                        fprintf(fp, "Message ID: %s\n", msg_id);
                        fprintf(fp, "Length: %zu bytes\n", data_length);
                        fprintf(fp, "Hex dump:\n");
                        for (size_t i = 0; i < data_length; i++) {
                            fprintf(fp, "%02x", reconstructed[i]);
                            if ((i + 1) % 32 == 0) fprintf(fp, "\n");
                            else if ((i + 1) % 2 == 0) fprintf(fp, " ");
                        }
                        if (data_length % 32 != 0) fprintf(fp, "\n");
                        fclose(fp);
                        printf("[AUDITOR][DEBUG] Saved reconstructed message: %s (%zu bytes)\n", filename, data_length);
                    } else {
                        printf("[AUDITOR][DEBUG] Failed to save reconstructed message to %s\n", filename);
                    }
                }
                
                *out_buf = reconstructed;
                *out_len = data_length;
                ret = RLC_OK;
            } else {
                free(reconstructed);
                printf("[AUDITOR ERROR] reconstruct_secret_from_shares failed\n");
            }
        } else {
            printf("[VSS][Auditor] Share verification failed, skipping reconstruction\n");
        }
    } else {
        printf("[AUDITOR ERROR] Insufficient shares: %d < %d\n", share_count, THRESHOLD);
        // 如果share_count == 0，可能是所有成员都返回了NO_AUDIT_NEEDED
        // 这种情况下，我们返回特殊状态表示用户身份合法
        if (share_count == 0) {
            printf("[AUDITOR] 所有成员都判断用户身份合法，无需审计\n");
            *out_buf = NULL;
            *out_len = 0;
            ret = RLC_OK;  // 返回成功，但buf为NULL表示不需要审计
        }
    }

    if (shares) {
        free(shares);
    }
    zmq_ctx_destroy(context);
    return ret;
}

// ========== 新增：解析 Tumbler 的第二个分片（Bob 相关） ==========
static void parse_tumbler_second_share(const uint8_t *buf, size_t len, auditor_state_t state) {
    if (buf == NULL || len == 0) {
        printf("[AUDITOR ERROR] parse_tumbler_second_share: 无效的输入 (buf=%p, len=%zu)\n", buf, len);
        return;
    }
    if (state == NULL) {
        printf("[AUDITOR ERROR] parse_tumbler_second_share: state 为 NULL\n");
        return;
    }
    
    size_t off = 0;
    printf("\n[AUDITOR] ========== 解析 Tumbler 第二个分片 (Bob相关) ==========\n");
    printf("[AUDITOR] 总长度: %zu 字节\n", len);
    
    // ========== 新增：首先解析Tumbler综合谜题零知识证明 ==========
    printf("[AUDITOR] ========== 开始解析Tumbler综合谜题零知识证明 ==========\n");
    
    // 检查是否有足够的空间用于零知识证明
    size_t remaining_space = len - off;
    printf("[AUDITOR] 剩余空间: %zu bytes\n", remaining_space);
    
    if (remaining_space < 100) { // 至少需要100字节用于基本的零知识证明
        printf("[AUDITOR ERROR] buffer too small for Tumbler ZK proof (need at least 100, have %zu)\n", remaining_space); 
        return; 
    }
    
    printf("[AUDITOR DEBUG] 准备解析ZK证明，当前offset: %zu，剩余数据: %zu bytes\n", off, len - off);
    printf("[AUDITOR DEBUG] ZK证明数据前64字节: ");
    for (size_t i = 0; i < 64 && i < (len - off); i++) {
        printf("%02x", buf[off + i]);
    }
    printf("\n");
    
    // 使用标准的反序列化函数解析ZK证明
    printf("[AUDITOR] 使用标准反序列化函数解析ZK证明...\n");
    
    size_t zk_parsed = 0;
    if (zk_comprehensive_puzzle_deserialize(state->tumbler_zk_proof, buf + off, &zk_parsed) != RLC_OK) {
        printf("[AUDITOR ERROR] ZK证明反序列化失败!\n");
        return;
    }
    off += zk_parsed;
    state->has_tumbler_zk_proof = 1;
    printf("[AUDITOR] ZK证明反序列化成功，大小: %zu bytes，当前offset: %zu\n", zk_parsed, off);
    
    printf("[AUDITOR] ========== Tumbler综合谜题零知识证明解析完成 ==========\n");
    
    // ZK证明将在所有信息解析完成后进行验证
    
    // 1) g^alpha
    if (off + RLC_EC_SIZE_COMPRESSED > len) { 
        printf("[AUDITOR ERROR] buffer too small for g^alpha\n"); 
        return; 
    }
    uint8_t g_alpha[RLC_EC_SIZE_COMPRESSED];
    memcpy(g_alpha, buf + off, RLC_EC_SIZE_COMPRESSED); 
    off += RLC_EC_SIZE_COMPRESSED;
    
    // 缓存 g^alpha
    memcpy(state->bob_g_alpha, g_alpha, RLC_EC_SIZE_COMPRESSED);
    state->has_bob_g_alpha = 1;
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", g_alpha[i]);
    }
    printf("\n");
    
    // 2) ctx_alpha (c1|c2) - 使用长度前缀格式
    // 读取c1长度和数据
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_alpha c1 length\n"); 
        return; 
    }
    size_t ctx_alpha_c1_len;
    memcpy(&ctx_alpha_c1_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + ctx_alpha_c1_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_alpha c1 data\n"); 
        return; 
    }
    char *ctx_alpha_c1 = (char*)malloc(ctx_alpha_c1_len + 1);
    if (!ctx_alpha_c1) { 
        printf("[AUDITOR ERROR] malloc failed for ctx_alpha_c1\n"); 
        return; 
    }
    memcpy(ctx_alpha_c1, buf + off, ctx_alpha_c1_len); 
    ctx_alpha_c1[ctx_alpha_c1_len] = 0; 
    off += ctx_alpha_c1_len;
    
    // 读取c2长度和数据
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_alpha c2 length\n"); 
        return; 
    }
    size_t ctx_alpha_c2_len;
    memcpy(&ctx_alpha_c2_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + ctx_alpha_c2_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_alpha c2 data\n"); 
        return; 
    }
    char *ctx_alpha_c2 = (char*)malloc(ctx_alpha_c2_len + 1);
    if (!ctx_alpha_c2) { 
        printf("[AUDITOR ERROR] malloc failed for ctx_alpha_c2\n"); 
        return; 
    }
    memcpy(ctx_alpha_c2, buf + off, ctx_alpha_c2_len); 
    ctx_alpha_c2[ctx_alpha_c2_len] = 0; 
    off += ctx_alpha_c2_len;
   
    
    // 3) auditor_enc(r0) - 使用长度前缀格式
    // 读取c1长度和数据
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_r0 c1 length\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2);
        return; 
    }
    size_t ctx_r0_c1_len;
    memcpy(&ctx_r0_c1_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + ctx_r0_c1_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_r0 c1 data\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2);
        return; 
    }
    char *ctx_r0_c1 = (char*)malloc(ctx_r0_c1_len + 1);
    if (!ctx_r0_c1) { 
        printf("[AUDITOR ERROR] malloc failed for ctx_r0_c1\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2);
        return; 
    }
    memcpy(ctx_r0_c1, buf + off, ctx_r0_c1_len); 
    ctx_r0_c1[ctx_r0_c1_len] = 0; 
    off += ctx_r0_c1_len;
    
    // 读取c2长度和数据
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_r0 c2 length\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2); free(ctx_r0_c1);
        return; 
    }
    size_t ctx_r0_c2_len;
    memcpy(&ctx_r0_c2_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + ctx_r0_c2_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_r0 c2 data\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2); free(ctx_r0_c1);
        return; 
    }
    char *ctx_r0_c2 = (char*)malloc(ctx_r0_c2_len + 1);
    if (!ctx_r0_c2) { 
        printf("[AUDITOR ERROR] malloc failed for ctx_r0_c2\n"); 
        free(ctx_alpha_c1); free(ctx_alpha_c2); free(ctx_r0_c1);
        return; 
    }
    memcpy(ctx_r0_c2, buf + off, ctx_r0_c2_len); 
    ctx_r0_c2[ctx_r0_c2_len] = 0; 
    off += ctx_r0_c2_len;
    
    
    // 解密 ctx_r0_auditor
    {
        // 验证格式是否像 Qfb(...)
        int looks_qfb_1 = (strncmp(ctx_r0_c1, "Qfb(", 4) == 0);
        int looks_qfb_2 = (strncmp(ctx_r0_c2, "Qfb(", 4) == 0);
        if (!(looks_qfb_1 && looks_qfb_2)) {
            printf("[AUDITOR ERROR] ctx_r0 c1/c2 not in Qfb(...) format. Skip decrypt to avoid PARI error.\n");
            printf("[AUDITOR DEBUG] ctx_r0.c1 raw: %.64s...\n", ctx_r0_c1);
            printf("[AUDITOR DEBUG] ctx_r0.c2 raw: %.64s...\n", ctx_r0_c2);
        } else {
            cl_ciphertext_t ctx_r0;
            cl_ciphertext_new(ctx_r0);
            ctx_r0->c1 = gcopy(gp_read_str(ctx_r0_c1));  // ⚠️ 使用 gcopy 创建永久副本
            ctx_r0->c2 = gcopy(gp_read_str(ctx_r0_c2));  // ⚠️ 使用 gcopy 创建永久副本
            
            GEN r0_plain;
            if (cl_dec(&r0_plain, ctx_r0, state->auditor_cl_sk, state->cl_params) == RLC_OK) {
                printf("[AUDITOR] ✅ ctx_r0_auditor 解密成功: r0 = %s\n", GENtostr(r0_plain));
                // 保存 r0 供后续验证
                if (!has_saved_r0) {
                    saved_r0 = r0_plain;
                    has_saved_r0 = 1;
                    printf("[AUDITOR] 已保存 r0 供后续验证\n");
                }
            } else {
                printf("[AUDITOR ERROR] ctx_r0_auditor 解密失败\n");
            }
            cl_ciphertext_free(ctx_r0);
        }
    }
    
    // 4) 预签名完整结构：r|s|R|pi.a|pi.b|pi.z
    size_t presig_full_size = 2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE;
    if (off + presig_full_size > len) { 
        printf("[AUDITOR ERROR] buffer too small for full presignature (need %zu, have %zu)\n", presig_full_size, len - off); 
        return; 
    }
    
    uint8_t pre_r[RLC_BN_SIZE], pre_s[RLC_BN_SIZE];
    uint8_t pre_R[RLC_EC_SIZE_COMPRESSED], pre_pi_a[RLC_EC_SIZE_COMPRESSED], pre_pi_b[RLC_EC_SIZE_COMPRESSED];
    uint8_t pre_pi_z[RLC_BN_SIZE];
    
    // 解析各字段
    memcpy(pre_r, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    memcpy(pre_s, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    memcpy(pre_R, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_a, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_b, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_z, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    
    // 缓存 Tumbler 完整预签名结构（第二个分片包含的是Tumbler的数据）
    memcpy(state->tumbler_presig_r, pre_r, RLC_BN_SIZE);
    memcpy(state->tumbler_presig_s, pre_s, RLC_BN_SIZE);
    memcpy(state->tumbler_presig_R, pre_R, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->tumbler_presig_pi_a, pre_pi_a, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->tumbler_presig_pi_b, pre_pi_b, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->tumbler_presig_pi_z, pre_pi_z, RLC_BN_SIZE);
    state->has_tumbler_presig = 1;
    
    // 5) 最终签名 r|s
    if (off + 2 * RLC_BN_SIZE > len) { 
        printf("[AUDITOR ERROR] buffer too small for final-sig r|s\n"); 
        return; 
    }
    uint8_t fin_r[RLC_BN_SIZE], fin_s[RLC_BN_SIZE];
    memcpy(fin_r, buf + off, RLC_BN_SIZE); 
    off += RLC_BN_SIZE;
    memcpy(fin_s, buf + off, RLC_BN_SIZE); 
    off += RLC_BN_SIZE;
    for (int i = 0; i < 8 && i < (int)RLC_BN_SIZE; i++) printf("%02x", fin_r[i]);
    printf(" s[0..7]: ");
    for (int i = 0; i < 8 && i < (int)RLC_BN_SIZE; i++) printf("%02x", fin_s[i]);
    printf("\n");
    // 缓存 Tumbler 最终签名 r|s（第二个分片包含的是Tumbler的数据）
    memcpy(state->tumbler_final_r, fin_r, RLC_BN_SIZE);
    memcpy(state->tumbler_final_s, fin_s, RLC_BN_SIZE);
    state->has_tumbler_final = 1;
    
    // ========== 新增：解析Tumbler签名验证所需的信息 ==========
    printf("[AUDITOR] ========== 开始解析Tumbler签名验证信息 ==========\n");
    
    // 提取 tumbler_escrow_id（字符串，以 \0 结尾）
    if (off >= len) {
        printf("[AUDITOR ERROR] buffer too small for tumbler_escrow_id\n");
        return;
    }
    size_t tumbler_escrow_id_len = strnlen((const char*)(buf + off), len - off) + 1;
    if (off + tumbler_escrow_id_len > len) {
        printf("[AUDITOR ERROR] buffer too small for complete tumbler_escrow_id\n");
        return;
    }
    if (tumbler_escrow_id_len > sizeof(state->tumbler_escrow_id)) {
        printf("[AUDITOR ERROR] tumbler_escrow_id too long (max %zu, got %zu)\n", sizeof(state->tumbler_escrow_id), tumbler_escrow_id_len);
        return;
    }
    memcpy(state->tumbler_escrow_id, buf + off, tumbler_escrow_id_len);
    state->tumbler_escrow_id[sizeof(state->tumbler_escrow_id) - 1] = '\0'; // 确保终止
    off += tumbler_escrow_id_len;
    printf("[AUDITOR] 解析 tumbler_escrow_id: %s (长度: %zu)\n", state->tumbler_escrow_id, tumbler_escrow_id_len);
    
    // 提取 tumbler_escrow_tx_hash（字符串，以 \0 结尾）
    if (off >= len) {
        printf("[AUDITOR ERROR] buffer too small for tumbler_escrow_tx_hash\n");
        return;
    }
    size_t tumbler_escrow_tx_hash_len = strnlen((const char*)(buf + off), len - off) + 1;
    if (off + tumbler_escrow_tx_hash_len > len) {
        printf("[AUDITOR ERROR] buffer too small for complete tumbler_escrow_tx_hash\n");
        return;
    }
    if (tumbler_escrow_tx_hash_len > sizeof(state->tumbler_escrow_tx_hash)) {
        printf("[AUDITOR ERROR] tumbler_escrow_tx_hash too long (max %zu, got %zu)\n", sizeof(state->tumbler_escrow_tx_hash), tumbler_escrow_tx_hash_len);
        return;
    }
    memcpy(state->tumbler_escrow_tx_hash, buf + off, tumbler_escrow_tx_hash_len);
    state->tumbler_escrow_tx_hash[sizeof(state->tumbler_escrow_tx_hash) - 1] = '\0'; // 确保终止
    off += tumbler_escrow_tx_hash_len;
    printf("[AUDITOR] 解析 tumbler_escrow_tx_hash: %s (长度: %zu)\n", state->tumbler_escrow_tx_hash, tumbler_escrow_tx_hash_len);
    state->has_tumbler_escrow_info = 1;
    
    // 6) Bob confirm tx hash (现在在最后)
    size_t remaining = len - off;
    if (remaining > 0) {
        char *bob_tx = (char*)malloc(remaining + 1);
        memcpy(bob_tx, buf + off, remaining);
        bob_tx[remaining] = '\0';
        printf("[AUDITOR] Bob confirm txHash: %s\n", bob_tx);
        free(bob_tx);
        off += remaining; // 更新 offset 到数据末尾
    }
    
    
    printf("[AUDITOR] 第二个分片解析完成. total_used=%zu, total_len=%zu\n", off, len);
    
    // ========== 新增：执行ZK证明验证 ==========
    printf("[AUDITOR] ========== 开始执行ZK证明验证 ==========\n");
    
    // 检查是否有必要的解密数据
    if (!has_saved_r0) {
        printf("[AUDITOR ERROR] 无法验证ZK证明：缺少解密后的r0数据\n");
        return;
    }
    
    // 构建验证所需的参数
    // 1. g^alpha (从第二个分片解析得到)
    ec_t g_alpha_pt;
    ec_null(g_alpha_pt);
    ec_new(g_alpha_pt);
    ec_read_bin(g_alpha_pt, g_alpha, RLC_EC_SIZE_COMPRESSED);
    
    // 2. ctx_alpha (从第二个分片解析得到)
    cl_ciphertext_t ctx_alpha_pt;
    cl_ciphertext_new(ctx_alpha_pt);
    ctx_alpha_pt->c1 = gcopy(gp_read_str(ctx_alpha_c1));
    ctx_alpha_pt->c2 = gcopy(gp_read_str(ctx_alpha_c2));
    
    // 3. ctx_r0_auditor (从第二个分片解析得到)
    cl_ciphertext_t ctx_r0_auditor_pt;
    cl_ciphertext_new(ctx_r0_auditor_pt);
    ctx_r0_auditor_pt->c1 = gcopy(gp_read_str(ctx_r0_c1));
    ctx_r0_auditor_pt->c2 = gcopy(gp_read_str(ctx_r0_c2));
    
    // 4. 重新计算g^r0
    char *r0_str_verify = GENtostr(saved_r0);
    bn_t r0_bn_verify;
    bn_null(r0_bn_verify);
    bn_new(r0_bn_verify);
    bn_read_str(r0_bn_verify, r0_str_verify, strlen(r0_str_verify), 10);
    
    ec_t g_r0_verify;
    ec_null(g_r0_verify);
    ec_new(g_r0_verify);
    ec_mul_gen(g_r0_verify, r0_bn_verify);
    
    printf("[AUDITOR] 开始验证综合谜题零知识证明...\n");
    printf("[AUDITOR] 验证参数:\n");
    printf("  - g^alpha: 0x");
    uint8_t g_alpha_debug[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(g_alpha_debug, RLC_EC_SIZE_COMPRESSED, g_alpha_pt, 1);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", g_alpha_debug[i]);
    printf("\n");
    
    printf("  - g^r0: 0x");
    uint8_t g_r0_debug[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(g_r0_debug, RLC_EC_SIZE_COMPRESSED, g_r0_verify, 1);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", g_r0_debug[i]);
    printf("\n");
    
    // 执行验证
    if (zk_comprehensive_puzzle_verify(state->tumbler_zk_proof, g_alpha_pt, ctx_alpha_pt, ctx_r0_auditor_pt,
                                       state->tumbler_cl_pk, state->auditor_cl_pk, state->cl_params) != RLC_OK) {
        printf("[AUDITOR ERROR] 综合谜题零知识证明验证失败!\n");
    } else {
        printf("[AUDITOR] ✅ 综合谜题零知识证明验证成功!\n");
    }
    
    // 清理验证相关变量
    ec_free(g_alpha_pt);
    cl_ciphertext_free(ctx_alpha_pt);
    cl_ciphertext_free(ctx_r0_auditor_pt);
    ec_free(g_r0_verify);
    bn_free(r0_bn_verify);
    pari_free(r0_str_verify);
    
    printf("[AUDITOR] ========== ZK证明验证完成 ==========\n");
    
    printf("[AUDITOR] ========== Tumbler 第二个分片解析结束 ==========\n\n");
    
    // 清理动态分配的内存
    free(ctx_alpha_c1);
    free(ctx_alpha_c2);
    free(ctx_r0_c1);
    free(ctx_r0_c2);
}

static void parse_tumbler_share(const uint8_t *buf, size_t len, auditor_state_t state) {
    size_t off = 0;
    char *ctx_c1 = NULL, *ctx_c2 = NULL, *aud_c1 = NULL, *aud_c2 = NULL;
    
    printf("[AUDITOR DEBUG] parse_tumbler_share called with len=%zu\n", len);
    printf("[AUDITOR DEBUG] First 128 bytes of buffer: ");
    for (size_t i = 0; i < 128 && i < len; i++) {
        printf("%02x", buf[i]);
        if ((i + 1) % 32 == 0) printf(" ");
    }
    printf("\n");
    
    // 1) g^(alpha+beta+tau)
    if (off + RLC_EC_SIZE_COMPRESSED > len) { 
        printf("[AUDITOR ERROR] buffer too small for g^(a+b+t) (off=%zu, need %zu, have %zu)\n", 
               off, RLC_EC_SIZE_COMPRESSED, len); 
        goto cleanup; 
    }
    uint8_t g_abt[RLC_EC_SIZE_COMPRESSED];
    memcpy(g_abt, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    printf("[AUDITOR] g^(alpha+beta+tau) head: "); for (int i=0;i< (RLC_EC_SIZE_COMPRESSED<16?RLC_EC_SIZE_COMPRESSED:16); i++) printf("%02x", g_abt[i]); printf("\n");
    // 缓存 Alice g^(a+b+t)
    memcpy(state->alice_g_abt, g_abt, RLC_EC_SIZE_COMPRESSED);
    state->has_alice_g_abt = 1;
    // 调试：打印完整压缩点十六进制
    printf("[AUDITOR] g^(alpha+beta+tau) (compressed): 0x");
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", g_abt[i]);
    printf("\n");

    // 2) ctx_alpha_times_beta_times_tau (c1|c2) - 使用长度前缀格式
    // 读取c1长度和数据
    printf("[AUDITOR DEBUG] Before parsing ctx: off=%zu, len=%zu, remaining=%zu\n", off, len, len - off);
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx c1 length (off=%zu, len=%zu, need %zu)\n", 
               off, len, off + sizeof(size_t)); 
        goto cleanup; 
    }
    size_t ctx_c1_len;
    printf("[AUDITOR DEBUG] Reading ctx_c1_len from offset %zu, bytes: ", off);
    for (size_t i = 0; i < sizeof(size_t) && off + i < len; i++) {
        printf("%02x", buf[off + i]);
    }
    printf("\n");
    memcpy(&ctx_c1_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    printf("[AUDITOR DEBUG] ctx_c1_len = %zu (0x%zx), remaining buffer = %zu\n", ctx_c1_len, ctx_c1_len, len - off);
    
    // 验证长度是否合理（Class Group 密文字符串通常在几百到几千字节）
    if (ctx_c1_len > 10000 || ctx_c1_len == 0) {
        printf("[AUDITOR ERROR] ctx_c1_len (%zu) seems unreasonable (expected 100-10000 bytes)\n", ctx_c1_len);
        printf("[AUDITOR ERROR] This suggests the length field was read incorrectly\n");
        goto cleanup;
    }
    if (off + ctx_c1_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx c1 data (off=%zu, ctx_c1_len=%zu, len=%zu, need %zu)\n", 
               off, ctx_c1_len, len, off + ctx_c1_len); 
        goto cleanup; 
    }
    ctx_c1 = (char*)malloc(ctx_c1_len + 1);
    if (!ctx_c1) { printf("[AUDITOR ERROR] malloc failed for ctx_c1\n"); goto cleanup; }
    memcpy(ctx_c1, buf + off, ctx_c1_len); ctx_c1[ctx_c1_len] = 0; off += ctx_c1_len;
    
    printf("[AUDITOR DEBUG] After reading ctx_c1: off=%zu, ctx_c1_len=%zu\n", off, ctx_c1_len);
    printf("[AUDITOR DEBUG] ctx_c1 first 64 bytes (hex): ");
    for (size_t i = 0; i < 64 && i < ctx_c1_len; i++) {
        printf("%02x", (unsigned char)ctx_c1[i]);
    }
    printf("\n");
    printf("[AUDITOR DEBUG] ctx_c1 as string (first 100 chars): %.100s\n", ctx_c1);
    printf("[AUDITOR DEBUG] ctx_c1 length: %zu bytes\n", ctx_c1_len);
    printf("[AUDITOR DEBUG] ctx_c1 ends with: ");
    if (ctx_c1_len >= 10) {
        for (size_t i = ctx_c1_len - 10; i < ctx_c1_len; i++) {
            printf("%02x", (unsigned char)ctx_c1[i]);
        }
    }
    printf("\n");
    
    // 验证：检查下一个位置是否真的是长度字段
    // 如果 ctx_c1 是字符串格式，它应该以 null 结尾，但我们已经添加了
    // 现在 off 应该指向 ctx_c2_len（8字节的 size_t）
    printf("[AUDITOR DEBUG] Verifying ctx_c2_len position: off=%zu, expected len2 field starts here\n", off);
    printf("[AUDITOR DEBUG] Next 16 bytes at offset %zu (should be ctx_c2_len): ", off);
    for (size_t i = 0; i < 16 && off + i < len; i++) {
        printf("%02x", buf[off + i]);
    }
    printf("\n");
    
    // 检查：如果这些字节看起来像字符串（可打印ASCII），说明读取位置错误
    int looks_like_string = 1;
    for (size_t i = 0; i < 8 && off + i < len; i++) {
        unsigned char c = buf[off + i];
        if (c < 0x20 || c > 0x7E) {
            looks_like_string = 0;
            break;
        }
    }
    if (looks_like_string) {
        printf("[AUDITOR ERROR] Bytes at offset %zu look like string data, not a length field!\n", off);
        printf("[AUDITOR ERROR] This suggests ctx_c1 was not read correctly, or data format mismatch\n");
        printf("[AUDITOR ERROR] Expected: 8-byte size_t length field, Got: string-like data\n");
        // 尝试从 ctx_c1 的末尾向前查找，看看是否有长度字段
        printf("[AUDITOR DEBUG] Checking if ctx_c1_len was read incorrectly...\n");
        printf("[AUDITOR DEBUG] ctx_c1_len value: %zu (0x%zx)\n", ctx_c1_len, ctx_c1_len);
        goto cleanup;
    }
    
    // 读取c2长度和数据
    if (off + sizeof(size_t) > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx c2 length (off=%zu, len=%zu, need %zu)\n", 
               off, len, off + sizeof(size_t)); 
        goto cleanup; 
    }
    size_t ctx_c2_len;
    printf("[AUDITOR DEBUG] Reading ctx_c2_len from offset %zu, bytes: ", off);
    for (size_t i = 0; i < sizeof(size_t) && off + i < len; i++) {
        printf("%02x", buf[off + i]);
    }
    printf("\n");
    memcpy(&ctx_c2_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    printf("[AUDITOR DEBUG] ctx_c2_len = %zu (0x%zx), remaining buffer = %zu\n", ctx_c2_len, ctx_c2_len, len - off);
    
    // 验证长度是否合理（不应该超过剩余缓冲区大小）
    if (ctx_c2_len > len - off) {
        printf("[AUDITOR ERROR] ctx_c2_len (%zu) exceeds remaining buffer (%zu), likely parsing error\n", 
               ctx_c2_len, len - off);
        printf("[AUDITOR DEBUG] This suggests the length field was read from wrong position or data format mismatch\n");
        goto cleanup;
    }
    if (off + ctx_c2_len > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx c2 data (off=%zu, ctx_c2_len=%zu, len=%zu, need %zu)\n", 
               off, ctx_c2_len, len, off + ctx_c2_len); 
        goto cleanup; 
    }
    ctx_c2 = (char*)malloc(ctx_c2_len + 1);
    if (!ctx_c2) { printf("[AUDITOR ERROR] malloc failed for ctx_c2\n"); goto cleanup; }
    memcpy(ctx_c2, buf + off, ctx_c2_len); ctx_c2[ctx_c2_len] = 0; off += ctx_c2_len;
    
    printf("[AUDITOR] ctx_alpha_beta_tau.c1 (len=%zu, head32): %.32s\n", ctx_c1_len, ctx_c1);
    printf("[AUDITOR] ctx_alpha_beta_tau.c2 (len=%zu, head32): %.32s\n", ctx_c2_len, ctx_c2);

    // 3) auditor 最终密文 (+tau) - 使用长度前缀格式
    // 读取auditor c1长度和数据
    if (off + sizeof(size_t) > len) { printf("[AUDITOR ERROR] buffer too small for auditor c1 length\n"); goto cleanup; }
    size_t aud_c1_len;
    memcpy(&aud_c1_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + aud_c1_len > len) { printf("[AUDITOR ERROR] buffer too small for auditor c1 data\n"); goto cleanup; }
    aud_c1 = (char*)malloc(aud_c1_len + 1);
    if (!aud_c1) { printf("[AUDITOR ERROR] malloc failed for aud_c1\n"); goto cleanup; }
    memcpy(aud_c1, buf + off, aud_c1_len); aud_c1[aud_c1_len] = 0; off += aud_c1_len;
    
    // 读取auditor c2长度和数据
    if (off + sizeof(size_t) > len) { printf("[AUDITOR ERROR] buffer too small for auditor c2 length\n"); goto cleanup; }
    size_t aud_c2_len;
    memcpy(&aud_c2_len, buf + off, sizeof(size_t)); off += sizeof(size_t);
    if (off + aud_c2_len > len) { printf("[AUDITOR ERROR] buffer too small for auditor c2 data\n"); goto cleanup; }
    aud_c2 = (char*)malloc(aud_c2_len + 1);
    if (!aud_c2) { printf("[AUDITOR ERROR] malloc failed for aud_c2\n"); goto cleanup; }
    memcpy(aud_c2, buf + off, aud_c2_len); aud_c2[aud_c2_len] = 0; off += aud_c2_len;
    
    printf("[AUDITOR] auditor(+tau).c1 (len=%zu, head32): %.32s\n", aud_c1_len, aud_c1);
    printf("[AUDITOR] auditor(+tau).c2 (len=%zu, head32): %.32s\n", aud_c2_len, aud_c2);
    
    // 解密 auditor(+tau) 密文得到 (r0+β+τ)
    {
        // 验证格式是否像 Qfb(...)
        int looks_qfb_1 = (strncmp(aud_c1, "Qfb(", 4) == 0);
        int looks_qfb_2 = (strncmp(aud_c2, "Qfb(", 4) == 0);
        if (looks_qfb_1 && looks_qfb_2) {
            cl_ciphertext_t aud_ct;
            cl_ciphertext_new(aud_ct);
            aud_ct->c1 = gcopy(gp_read_str(aud_c1));  // ⚠️ 使用 gcopy 创建永久副本
            aud_ct->c2 = gcopy(gp_read_str(aud_c2));  // ⚠️ 使用 gcopy 创建永久副本
            
            GEN r0_beta_tau_plain;
            if (cl_dec(&r0_beta_tau_plain, aud_ct, state->auditor_cl_sk, state->cl_params) == RLC_OK) {
                char *r0bt_dec = GENtostr(r0_beta_tau_plain);
                printf("[AUDITOR] ✅ auditor(+tau) 解密成功: r0+β+τ (dec) = %s\n", r0bt_dec);
                // 同时打印 hex 形式
                bn_t r0bt_bn; bn_null(r0bt_bn); bn_new(r0bt_bn);
                bn_read_str(r0bt_bn, r0bt_dec, (int)strlen(r0bt_dec), 10);
                bn_t qhex; bn_null(qhex); bn_new(qhex); ec_curve_get_ord(qhex); bn_mod(r0bt_bn, r0bt_bn, qhex);
                char r0bt_hex[2 * RLC_BN_SIZE + 2]; bn_write_str(r0bt_hex, sizeof(r0bt_hex), r0bt_bn, 16);
                printf("[AUDITOR] r0+β+τ (hex) = %s\n", r0bt_hex);
                bn_free(qhex); bn_free(r0bt_bn);
                // 保存 (r0+β+τ) 供后续验证
                if (!has_saved_r0_beta_tau) {
                    saved_r0_beta_tau = r0_beta_tau_plain;
                    has_saved_r0_beta_tau = 1;
                    printf("[AUDITOR] 已保存 (r0+β+τ) 供后续验证\n");
                }
                // 注意：此处不覆盖 saved_r0_beta_tau。
                // 如运行期未保存，但用户提供了日志数值，将在调试对比函数中使用该常量计算 delta。
            } else {
                printf("[AUDITOR ERROR] auditor(+tau) 解密失败\n");
            }
            cl_ciphertext_free(aud_ct);
        } else {
            printf("[AUDITOR ERROR] auditor(+tau) c1/c2 not in Qfb(...) format. Skip decrypt.\n");
        }
    }

    // 4) Alice 预签名完整结构：r|s|R|pi.a|pi.b|pi.z
    size_t alice_presig_full_size = 2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE;
    if (off + alice_presig_full_size > len) { 
        printf("[AUDITOR ERROR] buffer too small for Alice full presignature (need %zu, have %zu)\n", alice_presig_full_size, len - off); 
        goto cleanup; 
    }
    
    uint8_t pre_r[RLC_BN_SIZE], pre_s[RLC_BN_SIZE];
    uint8_t pre_R[RLC_EC_SIZE_COMPRESSED], pre_pi_a[RLC_EC_SIZE_COMPRESSED], pre_pi_b[RLC_EC_SIZE_COMPRESSED];
    uint8_t pre_pi_z[RLC_BN_SIZE];
    
    // 解析各字段
    memcpy(pre_r, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    memcpy(pre_s, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    memcpy(pre_R, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_a, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_b, buf + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
    memcpy(pre_pi_z, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;

    
    // 缓存 Alice 完整预签名结构
    memcpy(state->alice_presig_r, pre_r, RLC_BN_SIZE);
    memcpy(state->alice_presig_s, pre_s, RLC_BN_SIZE);
    memcpy(state->alice_presig_R, pre_R, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->alice_presig_pi_a, pre_pi_a, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->alice_presig_pi_b, pre_pi_b, RLC_EC_SIZE_COMPRESSED);
    memcpy(state->alice_presig_pi_z, pre_pi_z, RLC_BN_SIZE);
    state->has_alice_presig = 1;
    

    // 5) 真实签名 r|s
    if (off + 2 * RLC_BN_SIZE > len) { printf("[AUDITOR ERROR] buffer too small for final-sig r|s\n"); goto cleanup; }
    uint8_t fin_r[RLC_BN_SIZE], fin_s[RLC_BN_SIZE];
    memcpy(fin_r, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    memcpy(fin_s, buf + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
    printf("[AUDITOR] final-sign r[0..7]: "); for (int i=0;i<8 && i<RLC_BN_SIZE;i++) printf("%02x", fin_r[i]); printf("  s[0..7]: "); for (int i=0;i<8 && i<RLC_BN_SIZE;i++) printf("%02x", fin_s[i]); printf("\n");
    // 缓存 Alice 最终签名 r|s
    memcpy(state->alice_final_r, fin_r, RLC_BN_SIZE);
    memcpy(state->alice_final_s, fin_s, RLC_BN_SIZE);
    state->has_alice_final = 1;
    
    // ========== 新增：解析Alice签名验证所需的信息 ==========
    printf("[AUDITOR] ========== 开始解析Alice签名验证信息 ==========\n");
    
    // 提取 alice_escrow_id（字符串，以 \0 结尾）
    if (off >= len) {
        printf("[AUDITOR ERROR] buffer too small for alice_escrow_id\n");
        goto cleanup;
    }
    size_t alice_escrow_id_len = strnlen((const char*)(buf + off), len - off) + 1;
    if (off + alice_escrow_id_len > len) {
        printf("[AUDITOR ERROR] buffer too small for complete alice_escrow_id\n");
        goto cleanup;
    }
    if (alice_escrow_id_len > sizeof(state->alice_escrow_id)) {
        printf("[AUDITOR ERROR] alice_escrow_id too long (max %zu, got %zu)\n", sizeof(state->alice_escrow_id), alice_escrow_id_len);
        goto cleanup;
    }
    memcpy(state->alice_escrow_id, buf + off, alice_escrow_id_len);
    state->alice_escrow_id[sizeof(state->alice_escrow_id) - 1] = '\0'; // 确保终止
    off += alice_escrow_id_len;
    printf("[AUDITOR] 解析 alice_escrow_id: %s (长度: %zu)\n", state->alice_escrow_id, alice_escrow_id_len);
    
    // 提取 alice_escrow_tx_hash（字符串，以 \0 结尾）
    if (off >= len) {
        printf("[AUDITOR ERROR] buffer too small for alice_escrow_tx_hash\n");
        goto cleanup;
    }
    size_t alice_escrow_tx_hash_len = strnlen((const char*)(buf + off), len - off) + 1;
    if (off + alice_escrow_tx_hash_len > len) {
        printf("[AUDITOR ERROR] buffer too small for complete alice_escrow_tx_hash\n");
        goto cleanup;
    }
    if (alice_escrow_tx_hash_len > sizeof(state->alice_escrow_tx_hash)) {
        printf("[AUDITOR ERROR] alice_escrow_tx_hash too long (max %zu, got %zu)\n", sizeof(state->alice_escrow_tx_hash), alice_escrow_tx_hash_len);
        goto cleanup;
    }
    memcpy(state->alice_escrow_tx_hash, buf + off, alice_escrow_tx_hash_len);
    state->alice_escrow_tx_hash[sizeof(state->alice_escrow_tx_hash) - 1] = '\0'; // 确保终止
    off += alice_escrow_tx_hash_len;
    printf("[AUDITOR] 解析 alice_escrow_tx_hash: %s (长度: %zu)\n", state->alice_escrow_tx_hash, alice_escrow_tx_hash_len);
    state->has_alice_escrow_info = 1;
    
    printf("[AUDITOR] ========== 第一个分片基本字段解析完成 ==========\n");

    // ========== 新增：解析从Bob收到的原始谜题数据 ==========
    printf("[AUDITOR] ========== 开始解析从Bob收到的原始谜题数据 ==========\n");
    
    // 1) g^(α+β) (从Bob收到的)
    if (off + RLC_EC_SIZE_COMPRESSED > len) { 
        printf("[AUDITOR ERROR] buffer too small for g^(α+β) from Bob\n"); 
        goto cleanup; 
    }
    uint8_t g_alpha_beta[RLC_EC_SIZE_COMPRESSED];
    memcpy(g_alpha_beta, buf + off, RLC_EC_SIZE_COMPRESSED); 
    off += RLC_EC_SIZE_COMPRESSED;
    printf("[AUDITOR] g^(α+β) from Bob (前16字节): ");
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", g_alpha_beta[i]);
    }
    printf("\n");
    
    // 2) ctx_α+β (从Bob收到的Tumbler密文)
    if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > len) { 
        printf("[AUDITOR ERROR] buffer too small for ctx_α+β from Bob\n"); 
        goto cleanup; 
    }
    char bob_ctx_c1[RLC_CL_CIPHERTEXT_SIZE + 1];
    char bob_ctx_c2[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(bob_ctx_c1, buf + off, RLC_CL_CIPHERTEXT_SIZE); 
    bob_ctx_c1[RLC_CL_CIPHERTEXT_SIZE] = 0; 
    off += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(bob_ctx_c2, buf + off, RLC_CL_CIPHERTEXT_SIZE); 
    bob_ctx_c2[RLC_CL_CIPHERTEXT_SIZE] = 0; 
    off += RLC_CL_CIPHERTEXT_SIZE;
    printf("[AUDITOR] ctx_α+β from Bob.c1 (前32字符): %.32s\n", bob_ctx_c1);
    printf("[AUDITOR] ctx_α+β from Bob.c2 (前32字符): %.32s\n", bob_ctx_c2);
    
    // 3) auditor_ctx_α+β (从Bob收到的Auditor密文)
    if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > len) { 
        printf("[AUDITOR ERROR] buffer too small for auditor_ctx_α+β from Bob\n"); 
        goto cleanup; 
    }
    char bob_aud_c1[RLC_CL_CIPHERTEXT_SIZE + 1];
    char bob_aud_c2[RLC_CL_CIPHERTEXT_SIZE + 1];
    memcpy(bob_aud_c1, buf + off, RLC_CL_CIPHERTEXT_SIZE); 
    bob_aud_c1[RLC_CL_CIPHERTEXT_SIZE] = 0; 
    off += RLC_CL_CIPHERTEXT_SIZE;
    memcpy(bob_aud_c2, buf + off, RLC_CL_CIPHERTEXT_SIZE); 
    bob_aud_c2[RLC_CL_CIPHERTEXT_SIZE] = 0; 
    off += RLC_CL_CIPHERTEXT_SIZE;
    printf("[AUDITOR] auditor_ctx_α+β from Bob.c1 (前32字符): %.32s\n", bob_aud_c1);
    printf("[AUDITOR] auditor_ctx_α+β from Bob.c2 (前32字符): %.32s\n", bob_aud_c2);
    
    // ========== 新增：解析Alice的零知识证明 ==========
    printf("[AUDITOR] ========== 开始解析Alice的零知识证明 ==========\n");
    
    // 检查是否有足够的空间用于零知识证明
    // 使用剩余的所有空间，而不是固定的估计值
    size_t remaining_space = len - off;
    printf("[AUDITOR] 剩余空间: %zu bytes\n", remaining_space);
    
    if (remaining_space < 100) { // 至少需要100字节用于基本的零知识证明
        printf("[AUDITOR ERROR] buffer too small for Alice ZK proof (need at least 100, have %zu)\n", remaining_space); 
        goto cleanup; 
    }
    
    // 反序列化Alice的零知识证明
    zk_proof_puzzle_relation_t alice_zk_proof;
    zk_proof_puzzle_relation_new(alice_zk_proof);
    size_t proof_read = 0;
    if (zk_puzzle_relation_deserialize(alice_zk_proof, buf + off, &proof_read) != RLC_OK) {
        printf("[AUDITOR ERROR] 无法反序列化Alice的零知识证明!\n");
        zk_proof_puzzle_relation_free(alice_zk_proof);
        goto cleanup;
    }
    off += proof_read;
    printf("[AUDITOR] Alice零知识证明反序列化成功，大小: %zu bytes，当前offset: %zu\n", proof_read, off);
    
    // 验证Alice的零知识证明
    printf("[AUDITOR] 开始验证Alice的零知识证明...\n");
    
    // 构建密文结构
    cl_ciphertext_t bob_ctx_alpha_beta, ctx_alpha_beta_tau, bob_auditor_ctx_alpha_beta, auditor_ctx_alpha_beta_tau;
    cl_ciphertext_new(bob_ctx_alpha_beta);
    cl_ciphertext_new(ctx_alpha_beta_tau);
    cl_ciphertext_new(bob_auditor_ctx_alpha_beta);
    cl_ciphertext_new(auditor_ctx_alpha_beta_tau);
    
    // 设置密文数据
    bob_ctx_alpha_beta->c1 = gp_read_str(bob_ctx_c1);
    bob_ctx_alpha_beta->c2 = gp_read_str(bob_ctx_c2);
    ctx_alpha_beta_tau->c1 = gp_read_str(ctx_c1);
    ctx_alpha_beta_tau->c2 = gp_read_str(ctx_c2);
    bob_auditor_ctx_alpha_beta->c1 = gp_read_str(bob_aud_c1);
    bob_auditor_ctx_alpha_beta->c2 = gp_read_str(bob_aud_c2);
    auditor_ctx_alpha_beta_tau->c1 = gp_read_str(aud_c1);
    auditor_ctx_alpha_beta_tau->c2 = gp_read_str(aud_c2);
    
    // 构建椭圆曲线点
    ec_t g_alpha_beta_pt, g_alpha_beta_tau_pt;
    ec_null(g_alpha_beta_pt); ec_new(g_alpha_beta_pt);
    ec_null(g_alpha_beta_tau_pt); ec_new(g_alpha_beta_tau_pt);
    ec_read_bin(g_alpha_beta_pt, g_alpha_beta, RLC_EC_SIZE_COMPRESSED);
    ec_read_bin(g_alpha_beta_tau_pt, g_abt, RLC_EC_SIZE_COMPRESSED);
    
    if (zk_puzzle_relation_verify(alice_zk_proof,
                                 g_alpha_beta_pt, // g^(α+β)
                                 g_alpha_beta_tau_pt, // g^(α+β+τ)
                                 bob_ctx_alpha_beta, // ctx_α+β
                                 ctx_alpha_beta_tau, // ctx_α+β+τ
                                 bob_auditor_ctx_alpha_beta, // auditor_ctx_α+β
                                 auditor_ctx_alpha_beta_tau, // auditor_ctx_α+β+τ
                                 state->tumbler_cl_pk, // pk_tumbler
                                 state->auditor_cl_pk, // pk_auditor
                                 state->cl_params) != RLC_OK) {
        printf("[AUDITOR ERROR] Alice的零知识证明验证失败!\n");
        zk_proof_puzzle_relation_free(alice_zk_proof);
        cl_ciphertext_free(bob_ctx_alpha_beta);
        cl_ciphertext_free(ctx_alpha_beta_tau);
        cl_ciphertext_free(bob_auditor_ctx_alpha_beta);
        cl_ciphertext_free(auditor_ctx_alpha_beta_tau);
        ec_free(g_alpha_beta_pt);
        ec_free(g_alpha_beta_tau_pt);
        goto cleanup;
    }
    
    // 清理临时变量
    cl_ciphertext_free(bob_ctx_alpha_beta);
    cl_ciphertext_free(ctx_alpha_beta_tau);
    cl_ciphertext_free(bob_auditor_ctx_alpha_beta);
    cl_ciphertext_free(auditor_ctx_alpha_beta_tau);
    ec_free(g_alpha_beta_pt);
    ec_free(g_alpha_beta_tau_pt);
    printf("[AUDITOR] Alice的零知识证明验证成功!\n");
    
    // 清理Alice的零知识证明
    zk_proof_puzzle_relation_free(alice_zk_proof);
    
    printf("[AUDITOR] ========== 从Bob收到的原始谜题数据和Alice零知识证明解析完成 ==========\n");

    // ========== 计算第二分片的 msgid: id = g^(alpha+β+τ) / g^(r0+β+τ) = g^(alpha - r0) ==========
    if (has_saved_r0_beta_tau) {
      bn_t q; bn_null(q); bn_new(q); ec_curve_get_ord(q);
      // 读取 g^(alpha+β+τ)
      ec_t g_abt_pt; ec_null(g_abt_pt); ec_new(g_abt_pt);
      ec_read_bin(g_abt_pt, state->alice_g_abt, RLC_EC_SIZE_COMPRESSED);
      
      // 【调试】打印 g^(alpha+β+τ)
      printf("[AUDITOR DEBUG] g^(alpha+β+τ) 来自第一个分片: 0x");
      for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) printf("%02x", state->alice_g_abt[i]);
      printf("\n");
      
      // 将 (r0+β+τ) 转为标量并约化
      bn_t r0bt_bn; bn_null(r0bt_bn); bn_new(r0bt_bn);
      char *r0bt_str = GENtostr(saved_r0_beta_tau);
      
      // 【调试】打印解密出的 r0+β+τ
      printf("[AUDITOR DEBUG] 解密出的 r0+β+τ (十进制): %s\n", r0bt_str);
      
      bn_read_str(r0bt_bn, r0bt_str, strlen(r0bt_str), 10);
      bn_mod(r0bt_bn, r0bt_bn, q);
      
      // 【调试】打印模运算后的值
      char r0bt_mod_str[512];
      bn_write_str(r0bt_mod_str, sizeof(r0bt_mod_str), r0bt_bn, 10);
      printf("[AUDITOR DEBUG] r0+β+τ mod q (十进制): %s\n", r0bt_mod_str);
      
      // 计算 g^(r0+β+τ)
      ec_t g_r0bt_pt; ec_null(g_r0bt_pt); ec_new(g_r0bt_pt);
      ec_mul_gen(g_r0bt_pt, r0bt_bn);
      // 打印第一个分片解密出的 r0+β+τ 以及据此计算得到的 g^(r0+β+τ)
      uint8_t r0bt_pt_bytes[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(r0bt_pt_bytes, RLC_EC_SIZE_COMPRESSED, g_r0bt_pt, 1);
      printf("[AUDITOR] from share: g^(r0+β+τ) (compressed): 0x");
      for (int i=0;i<RLC_EC_SIZE_COMPRESSED;i++) printf("%02x", r0bt_pt_bytes[i]);
      printf("\n");
      // 计算 id_point = g_abt_pt - g_r0bt_pt
      ec_t neg_g_r0bt; ec_null(neg_g_r0bt); ec_new(neg_g_r0bt);
      ec_neg(neg_g_r0bt, g_r0bt_pt);
      ec_t id_point; ec_null(id_point); ec_new(id_point);
      ec_add(id_point, g_abt_pt, neg_g_r0bt); ec_norm(id_point, id_point);
      // 压缩编码为 hex 字符串，保存为 second_msgid
      uint8_t id_bytes[RLC_EC_SIZE_COMPRESSED];
      ec_write_bin(id_bytes, RLC_EC_SIZE_COMPRESSED, id_point, 1);
      char *new_msgid = (char*)malloc(2 * RLC_EC_SIZE_COMPRESSED + 3);
      if (new_msgid) {
        static const char *hexd = "0123456789abcdef";
        new_msgid[0] = '0'; new_msgid[1] = 'x';
        for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
          new_msgid[2 + 2*i]     = hexd[(id_bytes[i] >> 4) & 0xF];
          new_msgid[2 + 2*i + 1] = hexd[id_bytes[i] & 0xF];
        }
        new_msgid[2 + 2 * RLC_EC_SIZE_COMPRESSED] = '\0';
        if (state->second_msgid) free(state->second_msgid);
        state->second_msgid = new_msgid;
        printf("[AUDITOR] 计算第二分片 msgid = g^(alpha+β+τ) / g^(r0+β+τ) = g^(alpha−r0)\n");
        printf("[AUDITOR] second_msgid = %s\n", state->second_msgid);
      }
      // 清理
      ec_free(g_abt_pt); ec_free(g_r0bt_pt); ec_free(neg_g_r0bt); ec_free(id_point);
      bn_free(r0bt_bn); bn_free(q);
      pari_free(r0bt_str);
    } else {
      printf("[AUDITOR] 警告：未能解密 (r0+β+τ)，无法按新规则计算 second_msgid，保持旧逻辑或跳过。\n");
    }

    printf("[AUDITOR] ========== 第一个分片解析完成 ==========\n");
    printf("[AUDITOR] 已解析字段总大小: %zu / %zu 字节\n", off, len);
    
cleanup:
    // 释放动态分配的内存
    if (ctx_c1) free(ctx_c1);
    if (ctx_c2) free(ctx_c2);
    if (aud_c1) free(aud_c1);
    if (aud_c2) free(aud_c2);
}

// 处理审计请求
void audit_message(char* msg_id, auditor_state_t state) {
    audit_message_with_info(msg_id, NULL, NULL, state);
}

void audit_message_with_info(char* msg_id, char* sender_address, char* pairs_summary_line, auditor_state_t state) {
    // 输出审计信息
    printf("[AUDITOR] ========== 审计信息 ==========\n");
    printf("[AUDITOR] 交易哈希: %s\n", msg_id);
    if (sender_address != NULL) {
        printf("[AUDITOR] 发送方地址: %s\n", sender_address);
    }
    if (pairs_summary_line != NULL) {
        printf("[AUDITOR] pairs_summary: %s\n", pairs_summary_line);
    }
    printf("[AUDITOR] ==============================\n\n");
    
    // 解析 pairs_summary_line 并构造 JSON
    // 格式: "地址,(0,1)(1,1)(2,1)..."
    char *pairs_summary_json = NULL;
    if (pairs_summary_line != NULL && strlen(pairs_summary_line) > 0) {
        // 查找第一个逗号，分隔地址和record
        char *comma_pos = strchr(pairs_summary_line, ',');
        if (comma_pos != NULL) {
            // 提取地址（逗号之前）
            size_t address_len = comma_pos - pairs_summary_line;
            char *address = (char*)malloc(address_len + 1);
            if (address) {
                memcpy(address, pairs_summary_line, address_len);
                address[address_len] = '\0';
                
                // 提取record（逗号之后）
                char *record = comma_pos + 1;
                
                // 构造JSON: {"id":"地址","record":"record部分"}
                // 需要转义JSON中的特殊字符
                size_t json_buf_size = 1024 + strlen(address) + strlen(record);
                pairs_summary_json = (char*)malloc(json_buf_size);
                if (pairs_summary_json) {
                    // 转义record中的双引号和反斜杠
                    char *escaped_record = (char*)malloc(strlen(record) * 2 + 1);
                    if (escaped_record) {
                        size_t j = 0;
                        for (size_t i = 0; i < strlen(record); i++) {
                            if (record[i] == '"') {
                                escaped_record[j++] = '\\';
                                escaped_record[j++] = '"';
                            } else if (record[i] == '\\') {
                                escaped_record[j++] = '\\';
                                escaped_record[j++] = '\\';
                            } else {
                                escaped_record[j++] = record[i];
                            }
                        }
                        escaped_record[j] = '\0';
                        
                        snprintf(pairs_summary_json, json_buf_size, 
                                "{\"id\":\"%s\",\"record\":\"%s\"}", 
                                address, escaped_record);
                        free(escaped_record);
                        printf("[AUDITOR] 构造的JSON: %s\n", pairs_summary_json);
                    } else {
                        free(pairs_summary_json);
                        pairs_summary_json = NULL;
                    }
                }
                free(address);
            }
        } else {
            printf("[AUDITOR] WARNING: pairs_summary_line格式错误，未找到逗号分隔符\n");
        }
    }
    
    // 第一步：获取并解析第一个分片
    uint8_t *buf = NULL; 
    size_t len = 0;
    // tag=0 表示请求第一个分片（Alice相关），tag=1 表示请求第二个分片（Bob相关）
    int fetch_result = fetch_shares_by_msgid(msg_id, 0, pairs_summary_json, &buf, &len, state);
    if (fetch_result == RLC_OK) {
        // 检查是否是 "NO_AUDIT_NEEDED" 的情况（buf为NULL表示用户身份合法，不需要审计）
        if (buf == NULL && len == 0) {
            printf("[AUDITOR] ===========================================\n");
            printf("[AUDITOR] ✅ 用户身份合法，审计流程终止\n");
            printf("[AUDITOR] Judge API已判断该用户交易行为正常，无需继续审计\n");
            printf("[AUDITOR] ===========================================\n");
            if (pairs_summary_json) {
                free(pairs_summary_json);
            }
            return;  // 直接返回，不进行后续审计
        }
        printf("[AUDITOR] fetched %zu bytes by msgid=%s (tag=0)\n", len, msg_id);
        parse_tumbler_share(buf, len, state);
        
        // 第二步：如果第一个分片解析成功且保存了第二个分片的msgid，则获取并解析第二个分片
        if (state->second_msgid != NULL) {
            printf("[AUDITOR] ========== 开始获取第二个分片 ==========\n");
            printf("[AUDITOR] 第二个分片的 msgid: %s\n", state->second_msgid);
            
            uint8_t *second_buf = NULL;
            size_t second_len = 0;
            // tag=1 表示请求第二个分片（Bob相关）
            int fetch_result = fetch_shares_by_msgid(state->second_msgid, 1, pairs_summary_json, &second_buf, &second_len, state);
            printf("[AUDITOR] fetch_shares_by_msgid 返回: %d (tag=1)\n", fetch_result);
            
            if (fetch_result == RLC_OK && second_buf != NULL && second_len > 0) {
              printf("[AUDITOR] 成功获取第二个分片，大小: %zu 字节\n", second_len);
              printf("[AUDITOR] 第二个分片前32字节: ");
              for (size_t i = 0; i < 32 && i < second_len; i++) {
                printf("%02x", second_buf[i]);
              }
              printf("\n");
              
              // 解析第二个分片
              parse_tumbler_second_share(second_buf, second_len, state);

                // 第三步：执行审计验证（使用已解析的缓存数据）
                printf("[AUDITOR] ========== 开始执行 Alice 审计验证 ==========\n");
                perform_alice_audit_verification(state, NULL, 0);
                printf("[AUDITOR] ========== Alice 审计验证完成 ==========\n");

                printf("[AUDITOR] ========== 开始执行 Bob 审计验证 ==========\n");
                perform_bob_audit_verification(state, NULL, 0);
                printf("[AUDITOR] ========== Bob 审计验证完成 ==========\n");

                

                free(second_buf);
            } else {
                printf("[AUDITOR ERROR] 使用 msgid=%s 获取第二个分片失败\n", state->second_msgid);
              printf("[AUDITOR ERROR] second_buf=%p, second_len=%zu\n", (void*)second_buf, second_len);
            }
            printf("[AUDITOR] ========== 第二个分片处理完成 ==========\n\n");
        } else {
            printf("[AUDITOR] 第一个分片解析完成，但没有保存第二个分片的msgid\n");
        }
        free(buf);
        if (pairs_summary_json) {
            free(pairs_summary_json);
        }
        return; // 新逻辑完成，直接返回
    } else {
        printf("[AUDITOR WARN] fetch_shares_by_msgid failed, fallback to legacy flow.\n");
        if (pairs_summary_json) {
            free(pairs_summary_json);
        }
    }
}

// （已删除）perform_comprehensive_audit_verification：该流程现已拆分为按分片分别校验

// Alice 审计验证
static void perform_alice_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length) {
    printf("[AUDITOR] Alice审计验证开始...\n");
    // 注意：reconstructed_data 参数在当前实现中不使用，因为数据已经缓存在 state 中
    // 保留参数以保持接口兼容性，但实际使用缓存的数据

    // 优先使用缓存，避免重复解析
    uint8_t g_abt_bytes[RLC_EC_SIZE_COMPRESSED];
    if (state->has_alice_g_abt) {
        memcpy(g_abt_bytes, state->alice_g_abt, RLC_EC_SIZE_COMPRESSED);
            } else {
        size_t off = 0;
        if (off + RLC_EC_SIZE_COMPRESSED > data_length) { printf("[AUDITOR ERROR] buffer too small for g^(a+b+t)\n"); return; }
        memcpy(g_abt_bytes, reconstructed_data + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
        if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for ctx_alpha_beta_tau\n"); return; }
        off += 2 * RLC_CL_CIPHERTEXT_SIZE;
        if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for auditor(+tau)\n"); return; }
        off += 2 * RLC_CL_CIPHERTEXT_SIZE;
        size_t alice_presig_len = 2 * RLC_BN_SIZE + 3 * RLC_EC_SIZE_COMPRESSED + RLC_BN_SIZE;
        if (off + alice_presig_len > data_length) { printf("[AUDITOR ERROR] buffer too small for alice presignature\n"); return; }
        memcpy(state->alice_presig_r, reconstructed_data + off, RLC_BN_SIZE);
        memcpy(state->alice_presig_s, reconstructed_data + off + RLC_BN_SIZE, RLC_BN_SIZE);
        state->has_alice_presig = 1; off += alice_presig_len;
        if (off + 2 * RLC_BN_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for alice final signature\n"); return; }
        memcpy(state->alice_final_r, reconstructed_data + off, RLC_BN_SIZE);
        memcpy(state->alice_final_s, reconstructed_data + off + RLC_BN_SIZE, RLC_BN_SIZE);
        state->has_alice_final = 1; off += 2 * RLC_BN_SIZE;
    }

    // 6) 构造 g^(alpha+beta+tau) 点
    ec_t g_abt; ec_null(g_abt); ec_new(g_abt);
    ec_read_bin(g_abt, g_abt_bytes, RLC_EC_SIZE_COMPRESSED);

    // 7) 构造签名消息：alice_escrow_id || alice_escrow_tx_hash
    uint8_t tx_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
    int tx_len = 64;
    
    // 检查是否有解析的 escrow 信息
    if (!state->has_alice_escrow_info) {
        printf("[AUDITOR ERROR] Alice escrow信息未解析，无法验证签名\n");
        ec_free(g_abt);
        return;
    }
    
    // 1. 将托管ID (state->alice_escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
    if (strlen(state->alice_escrow_id) < 2 || strncmp(state->alice_escrow_id, "0x", 2) != 0) {
        printf("[AUDITOR ERROR] Invalid alice_escrow_id format for verification: %s\n", state->alice_escrow_id);
        ec_free(g_abt);
        return;
    }
    
    // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *escrow_id_hex = state->alice_escrow_id + 2; // 跳过"0x"
    size_t escrow_id_hex_len = strlen(escrow_id_hex);
    if (escrow_id_hex_len != 64) {
        printf("[AUDITOR ERROR] alice_escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, state->alice_escrow_id);
        ec_free(g_abt);
        return;
    }
    
    // 将托管ID的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
        char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
        tx_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    // 2. 将交易哈希从十六进制字符串转换为32字节数组
    if (strlen(state->alice_escrow_tx_hash) < 2 || strncmp(state->alice_escrow_tx_hash, "0x", 2) != 0) {
        printf("[AUDITOR ERROR] Invalid alice_escrow_tx_hash format for verification: %s\n", state->alice_escrow_tx_hash);
        ec_free(g_abt);
        return;
    }
    
    // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *hash_hex = state->alice_escrow_tx_hash + 2; // 跳过"0x"
    size_t hash_hex_len = strlen(hash_hex);
    if (hash_hex_len != 64) {
        printf("[AUDITOR ERROR] alice_escrow_tx_hash should be 64 hex chars (32 bytes), got %zu: %s\n", hash_hex_len, state->alice_escrow_tx_hash);
        ec_free(g_abt);
        return;
    }
    
    // 将交易哈希的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
        char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
        tx_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    printf("[AUDITOR] 构造Alice签名消息: alice_escrow_id || alice_escrow_tx_hash (64字节)\n");
    printf("[AUDITOR] Escrow ID (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[i]);
    printf("\n");
    printf("[AUDITOR] Escrow TX Hash (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[32 + i]);
    printf("\n");
            
    // 8) 构造 Alice 完整预签名结构
    ecdsa_signature_t alice_presig_sig; ecdsa_signature_null(alice_presig_sig); ecdsa_signature_new(alice_presig_sig);
    bn_read_bin(alice_presig_sig->r, state->alice_presig_r, RLC_BN_SIZE);
    bn_read_bin(alice_presig_sig->s, state->alice_presig_s, RLC_BN_SIZE);
    ec_read_bin(alice_presig_sig->R, state->alice_presig_R, RLC_EC_SIZE_COMPRESSED);
    ec_read_bin(alice_presig_sig->pi->a, state->alice_presig_pi_a, RLC_EC_SIZE_COMPRESSED);
    ec_read_bin(alice_presig_sig->pi->b, state->alice_presig_pi_b, RLC_EC_SIZE_COMPRESSED);
    bn_read_bin(alice_presig_sig->pi->z, state->alice_presig_pi_z, RLC_BN_SIZE);

    // 9) 预签名验证

    printf("[AUDITOR DEBUG] 即将调用 Alice adaptor_ecdsa_preverify...\n");
    
    int pre_ok = adaptor_ecdsa_preverify(alice_presig_sig, tx_buf, tx_len, g_abt, state->alice_ec_pk);
    printf("[AUDITOR] Alice 预签名验证: %s\n", pre_ok == 1 ? "成功" : "失败");
    if (pre_ok != 1) {
        ecdsa_signature_free(alice_presig_sig);
        ec_free(g_abt);
            return;
        }

    // 10) 最终签名验证
    ecdsa_signature_t alice_final; ecdsa_signature_null(alice_final); ecdsa_signature_new(alice_final);
    bn_read_bin(alice_final->r, state->alice_final_r, RLC_BN_SIZE);
    bn_read_bin(alice_final->s, state->alice_final_s, RLC_BN_SIZE);
    
    // 输出Alice签名数据用于智能合约验证
    printf("\n[AUDITOR] ========== Alice签名数据输出 ==========\n");
    printf("[AUDITOR] Alice r值 (32字节): ");
    for (int i = 0; i < RLC_BN_SIZE; i++) {
        printf("%02x", state->alice_final_r[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] Alice s值 (32字节): ");
    for (int i = 0; i < RLC_BN_SIZE; i++) {
        printf("%02x", state->alice_final_s[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] 交易数据长度: %zu 字节\n", tx_len);
    printf("[AUDITOR] 交易数据 (前64字节): ");
    for (size_t i = 0; i < (tx_len < 64 ? tx_len : 64); i++) {
        printf("%02x", tx_buf[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] 交易数据 (完整): ");
    for (size_t i = 0; i < tx_len; i++) {
        printf("%02x", tx_buf[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] Alice公钥 (压缩格式): ");
    uint8_t alice_pk_compressed[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(alice_pk_compressed, RLC_EC_SIZE_COMPRESSED, state->alice_ec_pk->pk, 1);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", alice_pk_compressed[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] Alice公钥 (未压缩格式): ");
    uint8_t alice_pk_uncompressed[RLC_EC_SIZE_COMPRESSED * 2];
    ec_write_bin(alice_pk_uncompressed, RLC_EC_SIZE_COMPRESSED * 2, state->alice_ec_pk->pk, 0);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED * 2; i++) {
        printf("%02x", alice_pk_uncompressed[i]);
    }
    printf("\n");
    printf("[AUDITOR] =========================================\n\n");
    
    // 在验证前计算并输出消息哈希
    printf("[AUDITOR] ========== 消息哈希计算 ==========\n");
    
    // 计算SHA256哈希（对应RELIC的md_map）
    uint8_t sha256_hash[32];
    // 使用RELIC的md_map函数计算SHA256
    md_map(sha256_hash, tx_buf, tx_len);
    
    printf("[AUDITOR] 原始消息长度: %d 字节\n", (int)tx_len);
    printf("[AUDITOR] 原始消息 (前32字节): ");
    for (size_t i = 0; i < (tx_len < 32 ? tx_len : 32); i++) {
        printf("%02x", tx_buf[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] SHA256哈希 (32字节): ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", sha256_hash[i]);
    }
    printf("\n");
    
    // 计算keccak256哈希用于对比
    printf("[AUDITOR] Keccak256哈希 (32字节): ");
    uint8_t keccak_hash[32];
    // 这里我们需要手动计算keccak256，或者使用其他方法
    // 暂时输出占位符
    printf("需要keccak256计算\n");
    
    printf("[AUDITOR] ======================================\n");
    
    printf("[AUDITOR] 准备调用 cp_ecdsa_ver 函数...\n");
    printf("[AUDITOR] 参数检查:\n");
    printf("  - alice_final->r: %p\n", alice_final->r);
    printf("  - alice_final->s: %p\n", alice_final->s);
    printf("  - tx_buf: %p\n", tx_buf);
    printf("  - tx_len: %d\n", (int)tx_len);
    printf("  - state->alice_ec_pk->pk: %p\n", state->alice_ec_pk->pk);
    
    int fin_ok = cp_ecdsa_ver(alice_final->r, alice_final->s, tx_buf, tx_len, 0, state->alice_ec_pk->pk);
    printf("[AUDITOR] cp_ecdsa_ver 返回: %d\n", fin_ok);
    printf("[AUDITOR] Alice 最终签名验证: %s\n", fin_ok == 1 ? "成功" : "失败");
    if (fin_ok != 1) {
        ecdsa_signature_free(alice_presig_sig);
        ecdsa_signature_free(alice_final);
        ec_free(g_abt);
            return;
        }

    // 使用合约再次验证（通过 truffle 脚本），便于链上与链下对齐
    {
        // 1) 先声明所有变量（C89 标准要求）
        char r_hex[65] = {0};
        char s_hex[65] = {0};
        uint8_t r_bytes[32], s_bytes[32];
        size_t tx_hex_len = (size_t)tx_len * 2 + 1;
        char *tx_hex = NULL;
        uint8_t pk_uncompressed[RLC_EC_SIZE_COMPRESSED * 2];
        size_t pk_hex_len = (RLC_EC_SIZE_COMPRESSED * 2) * 2 + 1;
        char *pk_hex = NULL;
        char cmd[8192];
        int rc;
        FILE *f;
        char line[256];
        int printed = 0;
        int i;

        // 2) 准备十六进制参数
        bn_write_bin(r_bytes, 32, alice_final->r);
        bn_write_bin(s_bytes, 32, alice_final->s);
        for (i = 0; i < 32; i++) {
            sprintf(r_hex + 2*i, "%02x", r_bytes[i]);
            sprintf(s_hex + 2*i, "%02x", s_bytes[i]);
        }

        // tx_buf -> hex
        tx_hex = (char*)malloc(tx_hex_len);
        if (tx_hex == NULL) {
            printf("[AUDITOR ERROR] 分配tx_hex内存失败，跳过合约验证\n");
        } else {
            for (i = 0; i < tx_len; i++) {
                sprintf(tx_hex + 2*i, "%02x", tx_buf[i]);
            }
            tx_hex[tx_hex_len - 1] = '\0';

            // 未压缩公钥 -> hex（65字节，含0x04）
            ec_write_bin(pk_uncompressed, RLC_EC_SIZE_COMPRESSED * 2, state->alice_ec_pk->pk, 0);
            pk_hex = (char*)malloc(pk_hex_len);
            if (pk_hex == NULL) {
                printf("[AUDITOR ERROR] 分配pk_hex内存失败，跳过合约验证\n");
                free(tx_hex);
                tx_hex = NULL;
            } else {
                for (i = 0; i < (RLC_EC_SIZE_COMPRESSED * 2); i++) {
                    sprintf(pk_hex + 2*i, "%02x", pk_uncompressed[i]);
                }
                pk_hex[pk_hex_len - 1] = '\0';

                // 3) 调用 truffle 脚本进行合约验证
                snprintf(cmd, sizeof(cmd),
                    "cd /home/zxx/Config/truffleProject/truffletest && truffle exec scripts/verify_with_contract.js --message 0x%s --r 0x%s --s 0x%s --pubkey 0x%s > /tmp/a2l_contract_verify.out 2>&1",
                    tx_hex, r_hex, s_hex, pk_hex);

                rc = system(cmd);
                printf("[AUDITOR] 已调用合约验证脚本，返回码: %d\n", rc);

                // 4) 读取输出
                f = fopen("/tmp/a2l_contract_verify.out", "r");
                if (f != NULL) {
                    while (fgets(line, sizeof(line), f)) {
                        // 寻找true/false
                        if (strstr(line, "true") != NULL) {
                            printf("[AUDITOR] 合约验证结果: 成功\n");
                            printed = 1;
                        } else if (strstr(line, "false") != NULL) {
                            printf("[AUDITOR] 合约验证结果: 失败\n");
                            printed = 1;
                        }
                    }
                    if (!printed) {
                        printf("[AUDITOR] 合约验证输出已写入 /tmp/a2l_contract_verify.out\n");
                    }
                    fclose(f);
                } else {
                    printf("[AUDITOR WARN] 无法读取合约验证输出文件\n");
                }

                // 5) 释放内存
                free(pk_hex);
                free(tx_hex);
            }
        }
    }

    // 11) 秘密值提取：gamma = s_final^{-1} * s_hat mod q，并与谜题对比 g^gamma == g^(α+β+τ)
    bn_t q, s_inv, gamma; bn_null(q); bn_null(s_inv); bn_null(gamma); 
    ec_t g_gamma; ec_null(g_gamma);
    bn_new(q); bn_new(s_inv); bn_new(gamma);
    
    ec_curve_get_ord(q);
    
    // 输入验证
    if (bn_is_zero(q)) {
        printf("[AUDITOR ERROR] 椭圆曲线阶数 q 为零！\n");
        goto cleanup_gamma;
    }
    if (alice_final->s == NULL || bn_is_zero(alice_final->s)) {
        printf("[AUDITOR ERROR] alice_final->s 无效！\n");
        goto cleanup_gamma;
    }
    
    // 计算模逆元: s_inv = s^(q-2) mod q
    bn_t q_minus_2;
    bn_null(q_minus_2);
    bn_new(q_minus_2);
    bn_sub_dig(q_minus_2, q, 2);
    bn_mxp(s_inv, alice_final->s, q_minus_2, q);
    bn_free(q_minus_2);
    
    // 验证逆元: s * s_inv ≡ 1 (mod q)
    bn_t verify_result;
    bn_null(verify_result);
    bn_new(verify_result);
    bn_mul(verify_result, alice_final->s, s_inv);
    bn_mod(verify_result, verify_result, q);
    if (bn_cmp_dig(verify_result, 1) != RLC_EQ) {
        printf("[AUDITOR ERROR] 逆元计算验证失败\n");
        bn_free(verify_result);
        goto cleanup_gamma;
    }
    bn_free(verify_result);
    
    // 计算 gamma = s_final_inv * s_presig mod q
    bn_mul(gamma, s_inv, alice_presig_sig->s);
    bn_mod(gamma, gamma, q);

    ec_new(g_gamma);
    ec_mul_gen(g_gamma, gamma);
    int link_ok = ec_cmp(g_gamma, g_abt) == RLC_EQ;
    printf("[AUDITOR] Alice 谜题链接校验 g^gamma == g^(α+β+τ): %s\n", link_ok ? "通过" : "不通过");

    // 清理
    ecdsa_signature_free(alice_presig_sig);
    ecdsa_signature_free(alice_final);
    ec_free(g_gamma);
    bn_free(q); bn_free(s_inv); bn_free(gamma);
    ec_free(g_abt);
    return;

cleanup_gamma:
    // 错误清理路径
    if (q != NULL) { bn_free(q); }
    if (s_inv != NULL) { bn_free(s_inv); }
    if (gamma != NULL) { bn_free(gamma); }
    if (g_gamma != NULL) { ec_free(g_gamma); }
    ecdsa_signature_free(alice_presig_sig);
    ecdsa_signature_free(alice_final);
    ec_free(g_abt);
}

// Bob 审计验证
static void perform_bob_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length) {
    printf("[AUDITOR] Bob审计验证开始...\n");
    // 注意：reconstructed_data 参数在当前实现中不使用，因为数据已经缓存在 state 中
    // 保留参数以保持接口兼容性，但实际使用缓存的数据
        
    // 优先使用缓存
    uint8_t g_alpha_bytes[RLC_EC_SIZE_COMPRESSED];
    if (state->has_bob_g_alpha) {
        memcpy(g_alpha_bytes, state->bob_g_alpha, RLC_EC_SIZE_COMPRESSED);
        } else {
        size_t off = 0;
        if (off + RLC_EC_SIZE_COMPRESSED > data_length) { printf("[AUDITOR ERROR] buffer too small for g^alpha\n"); return; }
        memcpy(g_alpha_bytes, reconstructed_data + off, RLC_EC_SIZE_COMPRESSED); off += RLC_EC_SIZE_COMPRESSED;
        if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for ctx_alpha\n"); return; }
        off += 2 * RLC_CL_CIPHERTEXT_SIZE;
        if (off + 2 * RLC_CL_CIPHERTEXT_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for auditor_enc(r0)\n"); return; }
        off += 2 * RLC_CL_CIPHERTEXT_SIZE;
        if (off + 2 * RLC_BN_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for bob presignature\n"); return; }
        memcpy(state->bob_presig_r, reconstructed_data + off, RLC_BN_SIZE);
        memcpy(state->bob_presig_s, reconstructed_data + off + RLC_BN_SIZE, RLC_BN_SIZE);
        state->has_bob_presig = 1; off += 2 * RLC_BN_SIZE;
        if (off + 2 * RLC_BN_SIZE > data_length) { printf("[AUDITOR ERROR] buffer too small for bob final signature\n"); return; }
        memcpy(state->bob_final_r, reconstructed_data + off, RLC_BN_SIZE);
        memcpy(state->bob_final_s, reconstructed_data + off + RLC_BN_SIZE, RLC_BN_SIZE);
        state->has_bob_final = 1; off += 2 * RLC_BN_SIZE;
    }

    // 6) 构造 g^alpha
    ec_t g_alpha; ec_null(g_alpha); ec_new(g_alpha);
    ec_read_bin(g_alpha, g_alpha_bytes, RLC_EC_SIZE_COMPRESSED);

    // 7) 构造签名消息：tumbler_escrow_id || tumbler_escrow_tx_hash
    uint8_t tx_buf[64]; // 托管ID(32字节) + 交易哈希(32字节) = 64字节
    int tx_len = 64;
    
    // 检查是否有解析的 escrow 信息
    if (!state->has_tumbler_escrow_info) {
        printf("[AUDITOR ERROR] Tumbler escrow信息未解析，无法验证签名\n");
        ec_free(g_alpha);
        return;
    }
    
    // 1. 将托管ID (state->tumbler_escrow_id) 从十六进制字符串转换为32字节数组（bytes32格式）
    if (strlen(state->tumbler_escrow_id) < 2 || strncmp(state->tumbler_escrow_id, "0x", 2) != 0) {
        printf("[AUDITOR ERROR] Invalid tumbler_escrow_id format for verification: %s\n", state->tumbler_escrow_id);
        ec_free(g_alpha);
        return;
    }
    
    // 解析托管ID的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *escrow_id_hex = state->tumbler_escrow_id + 2; // 跳过"0x"
    size_t escrow_id_hex_len = strlen(escrow_id_hex);
    if (escrow_id_hex_len != 64) {
        printf("[AUDITOR ERROR] tumbler_escrow_id should be 64 hex chars (32 bytes), got %zu: %s\n", escrow_id_hex_len, state->tumbler_escrow_id);
        ec_free(g_alpha);
        return;
    }
    
    // 将托管ID的十六进制字符串转换为字节数组
    for (size_t i = 0; i < 32; i++) {
        char hex_byte[3] = {escrow_id_hex[i * 2], escrow_id_hex[i * 2 + 1], '\0'};
        tx_buf[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
    }
    
    // 2. 将交易哈希从十六进制字符串转换为32字节数组
    if (strlen(state->tumbler_escrow_tx_hash) < 2 || strncmp(state->tumbler_escrow_tx_hash, "0x", 2) != 0) {
        printf("[AUDITOR ERROR] Invalid tumbler_escrow_tx_hash format for verification: %s\n", state->tumbler_escrow_tx_hash);
        ec_free(g_alpha);
        return;
    }
    
    // 解析交易哈希的十六进制字符串为字节数组（跳过"0x"前缀）
    const char *hash_hex = state->tumbler_escrow_tx_hash + 2; // 跳过"0x"
    size_t hash_hex_len = strlen(hash_hex);
    if (hash_hex_len != 64 && hash_hex_len != 1) { // 允许 "0x0" (跳过模式)
        printf("[AUDITOR ERROR] tumbler_escrow_tx_hash should be 64 hex chars (32 bytes) or '0x0', got %zu: %s\n", hash_hex_len, state->tumbler_escrow_tx_hash);
        ec_free(g_alpha);
        return;
    }
    
    // 如果交易哈希是 "0x0"（跳过模式），使用全零
    if (hash_hex_len == 1 && hash_hex[0] == '0') {
        memset(tx_buf + 32, 0, 32);
    } else {
        // 将交易哈希的十六进制字符串转换为字节数组
        for (size_t i = 0; i < 32; i++) {
            char hex_byte[3] = {hash_hex[i * 2], hash_hex[i * 2 + 1], '\0'};
            tx_buf[32 + i] = (uint8_t)strtoul(hex_byte, NULL, 16);
        }
    }
    
    printf("[AUDITOR DEBUG] Bob审计使用的签名消息（与Tumbler相同）: tumbler_escrow_id || tumbler_escrow_tx_hash\n");
    printf("[AUDITOR] Escrow ID (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[i]);
    printf("\n");
    printf("[AUDITOR] Escrow TX Hash (hex): ");
    for (int i = 0; i < 32; i++) printf("%02x", tx_buf[32 + i]);
    printf("\n");
    printf("  - tx_len = %d\n", tx_len);

    // 8) 构造 Tumbler 完整预签名结构（第二个分片包含的是Tumbler数据）
    if (!state->has_tumbler_presig) {
        printf("[AUDITOR ERROR] Tumbler预签名数据未缓存，无法进行Bob审计验证\n");
        ec_free(g_alpha);
        return;
    }
    
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", state->tumbler_presig_R[i]);
    }
    printf("\n");
   
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", state->tumbler_presig_pi_a[i]);
    }
    printf("\n");
    
    for (int i = 0; i < 16 && i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", state->tumbler_presig_pi_b[i]);
    }
    printf("\n");
    
    ecdsa_signature_t tumbler_presig_sig; ecdsa_signature_null(tumbler_presig_sig); ecdsa_signature_new(tumbler_presig_sig);
    
    // 添加错误处理
    RLC_TRY {
        bn_read_bin(tumbler_presig_sig->r, state->tumbler_presig_r, RLC_BN_SIZE);
        bn_read_bin(tumbler_presig_sig->s, state->tumbler_presig_s, RLC_BN_SIZE);
        ec_read_bin(tumbler_presig_sig->R, state->tumbler_presig_R, RLC_EC_SIZE_COMPRESSED);
        ec_read_bin(tumbler_presig_sig->pi->a, state->tumbler_presig_pi_a, RLC_EC_SIZE_COMPRESSED);
        ec_read_bin(tumbler_presig_sig->pi->b, state->tumbler_presig_pi_b, RLC_EC_SIZE_COMPRESSED);
        bn_read_bin(tumbler_presig_sig->pi->z, state->tumbler_presig_pi_z, RLC_BN_SIZE);
    } RLC_CATCH_ANY {
        printf("[AUDITOR ERROR] 读取Tumbler预签名数据失败，数据可能损坏\n");
        ecdsa_signature_free(tumbler_presig_sig);
        ec_free(g_alpha);
        return;
    }
    
    printf("[AUDITOR] Bob 预签名验证开始...\n");
    
    int pre_ok = adaptor_ecdsa_preverify(tumbler_presig_sig, tx_buf, tx_len, g_alpha, state->tumbler_ec_pk);
    if (pre_ok != 1) { 
        printf("[AUDITOR ERROR] Tumbler 预签名验证失败，检查输入数据\n");
        ecdsa_signature_free(tumbler_presig_sig); 
        ec_free(g_alpha); 
        return; 
    }

    // 9) 最终签名验证（使用 Tumbler 公钥，与原逻辑一致）
    ecdsa_signature_t tumbler_final; ecdsa_signature_null(tumbler_final); ecdsa_signature_new(tumbler_final);
    bn_read_bin(tumbler_final->r, state->tumbler_final_r, RLC_BN_SIZE);
    bn_read_bin(tumbler_final->s, state->tumbler_final_s, RLC_BN_SIZE);
    
    // 输出Tumbler签名数据用于智能合约验证
    printf("\n[AUDITOR] ========== Tumbler签名数据输出 ==========\n");
    printf("[AUDITOR] Tumbler r值 (32字节): ");
    for (int i = 0; i < RLC_BN_SIZE; i++) {
        printf("%02x", state->tumbler_final_r[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] Tumbler s值 (32字节): ");
    for (int i = 0; i < RLC_BN_SIZE; i++) {
        printf("%02x", state->tumbler_final_s[i]);
    }
    printf("\n");
    
    printf("[AUDITOR] Tumbler公钥 (压缩格式): ");
    uint8_t tumbler_pk_compressed[RLC_EC_SIZE_COMPRESSED];
    ec_write_bin(tumbler_pk_compressed, RLC_EC_SIZE_COMPRESSED, state->tumbler_ec_pk->pk, 1);
    for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
        printf("%02x", tumbler_pk_compressed[i]);
    }
    printf("\n");
    printf("[AUDITOR] ===========================================\n\n");
    
    int fin_ok = cp_ecdsa_ver(tumbler_final->r, tumbler_final->s, tx_buf, tx_len, 0, state->tumbler_ec_pk->pk);
    printf("[AUDITOR] Tumbler 最终签名验证: %s\n", fin_ok == 1 ? "成功" : "失败");

    // 11) 秘密值提取：alpha = s_final^{-1} * s_presig mod q，并与谜题对比 g^alpha == g_alpha
    if (fin_ok == 1) {
        bn_t q, s_inv, alpha;
        bn_null(q); bn_null(s_inv); bn_null(alpha);
        bn_new(q); bn_new(s_inv); bn_new(alpha);
        
        ec_curve_get_ord(q);
        
        // 输入验证
        if (bn_is_zero(q) || tumbler_final->s == NULL || bn_is_zero(tumbler_final->s)) {
            printf("[AUDITOR ERROR] Tumbler谜题链接校验：输入数据无效\n");
            bn_free(q); bn_free(s_inv); bn_free(alpha);
            goto bob_cleanup;
        }
        
        // 计算模逆元: s_inv = s^(q-2) mod q
        bn_t q_minus_2;
        bn_null(q_minus_2);
        bn_new(q_minus_2);
        bn_sub_dig(q_minus_2, q, 2);
        bn_mxp(s_inv, tumbler_final->s, q_minus_2, q);
        bn_free(q_minus_2);
        
        // 验证逆元
        bn_t verify_result;
        bn_null(verify_result); bn_new(verify_result);
        bn_mul(verify_result, tumbler_final->s, s_inv);
        bn_mod(verify_result, verify_result, q);
        if (bn_cmp_dig(verify_result, 1) != RLC_EQ) {
            printf("[AUDITOR ERROR] Tumbler逆元计算验证失败\n");
            bn_free(verify_result); bn_free(q); bn_free(s_inv); bn_free(alpha);
            goto bob_cleanup;
        }
        bn_free(verify_result);
        
        // 计算 alpha = s_final_inv * s_presig mod q
        bn_mul(alpha, s_inv, tumbler_presig_sig->s);
        bn_mod(alpha, alpha, q);
        
        // 调试：打印计算过程的中间值
        char alpha_str[256], s_inv_str[256], s_presig_str[256];
        bn_write_str(alpha_str, sizeof(alpha_str), alpha, 10);
        bn_write_str(s_inv_str, sizeof(s_inv_str), s_inv, 10);
        bn_write_str(s_presig_str, sizeof(s_presig_str), tumbler_presig_sig->s, 10);
       
        
        // 调试：打印当前使用的交易数据
        printf("  - 当前使用的 tx_len = %d\n", tx_len);
        printf("  - 当前使用的 tx_buf (前64字节): ");
        for (int i = 0; i < 64 && i < tx_len; i++) printf("%02x", tx_buf[i]);
        printf("\n");
        
        // 验证 g^alpha == g_alpha
        ec_t g_alpha_computed;
        ec_null(g_alpha_computed); ec_new(g_alpha_computed);
        ec_mul_gen(g_alpha_computed, alpha);
        
        // 打印椭圆曲线点进行比较
        uint8_t g_alpha_computed_bytes[33], g_alpha_original_bytes[33];
        ec_write_bin(g_alpha_computed_bytes, 33, g_alpha_computed, 1);
        ec_write_bin(g_alpha_original_bytes, 33, g_alpha, 1);
        
        int tumbler_link_ok = ec_cmp(g_alpha_computed, g_alpha) == RLC_EQ;
        printf("[AUDITOR] Tumbler 谜题链接校验 g^alpha == g_alpha: %s\n", tumbler_link_ok ? "通过" : "不通过");
        
        // 清理
        ec_free(g_alpha_computed);
        bn_free(q); bn_free(s_inv); bn_free(alpha);
    }

bob_cleanup:
    ecdsa_signature_free(tumbler_presig_sig);
    ecdsa_signature_free(tumbler_final);
    ec_free(g_alpha);
}

// Tumbler 审计验证
static void perform_tumbler_audit_verification(auditor_state_t state, const uint8_t *reconstructed_data, size_t data_length) {
    printf("[AUDITOR] Tumbler审计验证待实现...\n");
    // TODO: 实现Tumbler的签名验证、椭圆曲线点验证等
}

// ========== 新增：读取DKG公钥文件（Class Group 版本）==========
/**
 * 从文件读取DKG生成的Class Group公钥
 * 
 * @param public_key 输出的公钥 (GEN)
 * @return RLC_OK 成功，RLC_ERR 失败
 */
static int read_dkg_public_key_cl(GEN *public_key) {
    const char* pub_key_file_path = "../keys/dkg_public.key";
    
    FILE *pub_key_file = fopen(pub_key_file_path, "r");
    if (!pub_key_file) {
        printf("[AUDITOR_DKG] 无法打开DKG公钥文件: %s\n", pub_key_file_path);
        return RLC_ERR;
    }
    
    // 读取文件大小
    fseek(pub_key_file, 0, SEEK_END);
    long file_size = ftell(pub_key_file);
    fseek(pub_key_file, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 1000000) {  // 限制最大1MB
        printf("[AUDITOR_DKG] 公钥文件大小异常: %ld字节\n", file_size);
        fclose(pub_key_file);
        return RLC_ERR;
    }
    
    // 读取公钥字符串
    char *public_key_str = (char*)malloc(file_size + 1);
    if (!public_key_str) {
        printf("[AUDITOR_DKG] 内存分配失败\n");
        fclose(pub_key_file);
        return RLC_ERR;
    }
    
    size_t bytes_read = fread(public_key_str, 1, file_size, pub_key_file);
    fclose(pub_key_file);
    
    if (bytes_read != file_size) {
        printf("[AUDITOR_DKG] 读取公钥数据失败\n");
        free(public_key_str);
        return RLC_ERR;
    }
    
    public_key_str[file_size] = '\0';
    
    printf("[AUDITOR_DKG] 公钥字符串长度: %zu\n", strlen(public_key_str));
    printf("[AUDITOR_DKG] 公钥字符串(前128字符): %.128s\n", public_key_str);
    
    // 解析公钥 (Class Group) - 使用 gclone 创建永久副本（深拷贝）
    pari_sp av = avma;
    GEN pk_temp = gp_read_str(public_key_str);
    *public_key = gclone(pk_temp);  // ⚠️ 使用 gclone 创建永久副本（深拷贝）
    avma = av;
    
    free(public_key_str);
    
    printf("[AUDITOR_DKG] ✅ 成功读取DKG Class Group公钥文件: %s\n", pub_key_file_path);
    
    return RLC_OK;
}

// ========== 新增：请求DKG私钥分片并重构完整私钥 ==========
/**
 * 从DKG委员会请求私钥分片并重构完整私钥
 * 
 * 数学原理：
 * 使用Lagrange插值从t个私钥分片重构完整私钥：
 * x = ∑_{i∈S} s_i * L_i(0) mod q
 * 
 * 其中：
 * - S 是参与者集合（|S| ≥ t）
 * - s_i 是参与者i的私钥分片
 * - L_i(0) 是Lagrange系数
 * 
 * @param state Auditor状态（用于访问CL参数）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
static int request_dkg_shares_and_reconstruct(auditor_state_t state) {
    printf("\n[AUDITOR_DKG] ========== 开始请求DKG私钥分片 ==========\n");
    
    // DKG参数（应该从配置文件读取，这里硬编码）
    const int n_participants = SECRET_SHARES;
    const int threshold = THRESHOLD;
    
    printf("[AUDITOR_DKG] DKG参数: n=%d, t=%d\n", n_participants, threshold);
    
    // 创建ZMQ上下文
    void* context = zmq_ctx_new();
    if (!context) {
        printf("[AUDITOR_DKG] 创建ZMQ上下文失败\n");
        return RLC_ERR;
    }
    
    // 存储收到的私钥分片
    typedef struct {
        int participant_id;
        bn_t share;
        int valid;
    } dkg_share_info_t;
    
    dkg_share_info_t shares[SECRET_SHARES];
    for (int i = 0; i < SECRET_SHARES; i++) {
        shares[i].participant_id = 0;
        bn_null(shares[i].share);
        bn_new(shares[i].share);
        shares[i].valid = 0;
    }
    
    int received_count = 0;
    
    // 向每个参与者请求私钥分片
    for (int i = 0; i < n_participants; i++) {
        printf("[AUDITOR_DKG] 请求参与者%d的私钥分片\n", i + 1);
        
        void* socket = zmq_socket(context, ZMQ_REQ);
        if (!socket) {
            printf("[AUDITOR_DKG] 创建套接字失败\n");
            continue;
        }
        
        // 连接到接收者
        int rc = zmq_connect(socket, RECEIVER_ENDPOINTS[i]);
        if (rc != 0) {
            printf("[AUDITOR_DKG] 连接%s失败: %s\n", 
                   RECEIVER_ENDPOINTS[i], zmq_strerror(zmq_errno()));
            zmq_close(socket);
            continue;
        }
        
        // 发送DKG私钥分片请求
        message_t request;
        message_null(request);
        message_new(request, strlen("DKG_SHARE_REQUEST") + 1, 1);
        strcpy(request->type, "DKG_SHARE_REQUEST");
        request->data[0] = '\0';  // 空数据
        
        uint8_t *serialized = NULL;
        unsigned msg_type_length = strlen(request->type) + 1;
        unsigned msg_data_length = 1;
        serialize_message(&serialized, request, msg_type_length, msg_data_length);
        size_t serialized_len = msg_type_length + msg_data_length + 2 * sizeof(unsigned);
        
        int send_rc = zmq_send(socket, serialized, serialized_len, 0);
        if (send_rc != (int)serialized_len) {
            printf("[AUDITOR_DKG] 发送请求失败: %s\n", zmq_strerror(zmq_errno()));
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;
        }
        
        // 接收响应
        zmq_msg_t response;
        zmq_msg_init(&response);
        int recv_rc = zmq_msg_recv(&response, socket, 0);
        if (recv_rc < 0) {
            printf("[AUDITOR_DKG] 接收响应失败: %s\n", zmq_strerror(zmq_errno()));
            zmq_msg_close(&response);
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;
        }
        
        uint8_t *data = zmq_msg_data(&response);
        size_t size = zmq_msg_size(&response);
        
        // 检查响应是否为错误消息
        if (size > 0 && strncmp((char*)data, "DKG_", 4) == 0) {
            printf("[AUDITOR_DKG] 参与者%d返回错误: %.*s\n", i + 1, (int)size, data);
            zmq_msg_close(&response);
            zmq_close(socket);
            free(serialized);
            message_free(request);
            continue;
        }
        
        // 解析响应: [participant_id(4)] [secret_share_len(4)] [secret_share_data]
        if (size >= sizeof(int) * 2) {
            int participant_id, secret_share_len;
            size_t offset = 0;
            
            memcpy(&participant_id, data + offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&secret_share_len, data + offset, sizeof(int));
            offset += sizeof(int);
            
            if (offset + secret_share_len <= size) {
                uint8_t secret_share_buf[RLC_BN_SIZE];
                memcpy(secret_share_buf, data + offset, secret_share_len);
                
                // 读取私钥分片
                bn_read_bin(shares[received_count].share, secret_share_buf, secret_share_len);
                shares[received_count].participant_id = participant_id;
                shares[received_count].valid = 1;
                
                printf("[AUDITOR_DKG] ✅ 成功接收参与者%d的私钥分片，长度=%d\n", 
                       participant_id, secret_share_len);
                printf("[AUDITOR_DKG] 私钥分片值: ");
                bn_print(shares[received_count].share);
                printf("\n");
                
                received_count++;
            } else {
                printf("[AUDITOR_DKG] 参与者%d响应数据不完整\n", i + 1);
            }
        } else {
            printf("[AUDITOR_DKG] 参与者%d响应格式错误\n", i + 1);
        }
        
        zmq_msg_close(&response);
        zmq_close(socket);
        free(serialized);
        message_free(request);
    }
    
    printf("[AUDITOR_DKG] 收到%d/%d个私钥分片\n", received_count, n_participants);
    
    // 检查是否收到足够的份额
    if (received_count < threshold) {
        printf("[AUDITOR_DKG] ❌ 私钥分片不足: %d < %d（阈值）\n", received_count, threshold);
        for (int i = 0; i < SECRET_SHARES; i++) {
            if (shares[i].valid) {
                bn_free(shares[i].share);
            }
        }
        zmq_ctx_destroy(context);
        return RLC_ERR;
    }
    
    // 使用Lagrange插值重构完整私钥
    printf("[AUDITOR_DKG] ========== 开始Lagrange插值重构 ==========\n");
    
    // 准备参与者ID数组和份额数组
    int participant_ids[SECRET_SHARES];
    bn_t share_values[SECRET_SHARES];
    
    for (int i = 0; i < received_count; i++) {
        participant_ids[i] = shares[i].participant_id;
        bn_null(share_values[i]);
        bn_new(share_values[i]);
        bn_copy(share_values[i], shares[i].share);
        
        printf("[AUDITOR_DKG] 使用参与者%d的份额\n", participant_ids[i]);
    }
    
    // 获取群阶（使用secp256k1的阶）
    bn_t order;
    bn_null(order);
    bn_new(order);
    ec_curve_get_ord(order);  // 使用secp256k1群的阶
    
    // 重构完整私钥
    bn_t reconstructed_key;
    bn_null(reconstructed_key);
    bn_new(reconstructed_key);
    
    if (dkg_reconstruct_secret_from_shares(participant_ids, share_values, received_count, 
                                           order, reconstructed_key) == RLC_OK) {
        printf("[AUDITOR_DKG] ========== 私钥重构成功！ ==========\n");
        printf("[AUDITOR_DKG] 🔑 完整私钥 = ");
        bn_print(reconstructed_key);
        printf("\n");
        
        
        // 6. 打印所有参与者的私钥分片值（Auditor收到的）
        printf("\n[AUDITOR_DEBUG] ========== Auditor收到的私钥分片 ==========\n");
        for (int i = 0; i < received_count; i++) {
            char share_hex[256];
            bn_write_str(share_hex, sizeof(share_hex), share_values[i], 16);
            printf("[AUDITOR_DEBUG] sk[%d] = %s\n", participant_ids[i], share_hex);
        }
        
        // 读取DKG委员会生成的Class Group公钥并进行比较
        printf("\n[AUDITOR_DKG] ========== 验证重构的私钥（Class Group版本）==========\n");
        GEN dkg_public_key_cl = NULL;
        
        if (read_dkg_public_key_cl(&dkg_public_key_cl) == RLC_OK) {
            // 打印DKG文件中的Class Group公钥
            char *dkg_pk_str = GENtostr(dkg_public_key_cl);
            printf("[AUDITOR_DEBUG] DKG文件中的Class Group公钥(前128字符): %.128s\n", dkg_pk_str);
            pari_free(dkg_pk_str);
            
            // 为生成Class Group公钥，需要CL参数（使用auditor state中已有的）
            GEN g_q = state->cl_params->g_q;
    
        
        } else {
            printf("[AUDITOR_DKG] ⚠️  无法读取DKG Class Group公钥文件，跳过公钥比对\n");
        }
        
        // ⭐ 将重构的私钥存储到state中（在成功分支内）
        printf("\n[AUDITOR_DKG] 将重构的私钥存储到Auditor state...\n");
        char sk_str_final[256];
        bn_write_str(sk_str_final, sizeof(sk_str_final), reconstructed_key, 10);
        state->auditor_cl_sk->sk = strtoi(sk_str_final);
        printf("[AUDITOR_DKG] ✅ 私钥已存储到state->auditor_cl_sk\n");
        
        // 清理资源
        ec_free(reconstructed_public_key);
        bn_free(a_1_0);
        bn_free(a_2_0);
        bn_free(a_3_0);
        bn_free(theoretical_sk);
        ec_free(pk_from_reconstructed);
        ec_free(pk_from_theoretical);
    } else {
        printf("[AUDITOR_DKG] ❌ 私钥重构失败\n");
    }
    
    // 清理资源
    bn_free(reconstructed_key);
    bn_free(order);
    for (int i = 0; i < received_count; i++) {
        bn_free(share_values[i]);
    }
    for (int i = 0; i < SECRET_SHARES; i++) {
        if (shares[i].valid) {
            bn_free(shares[i].share);
        }
    }
    zmq_ctx_destroy(context);
    
    printf("[AUDITOR_DKG] ========== DKG私钥重构完成 ==========\n\n");
    return RLC_OK;
}




// ========== CSV监控模式相关函数 ==========

// 存储已处理的交易哈希
typedef struct {
    char **hashes;
    int count;
    int capacity;
} processed_txhash_list_t;

// 初始化已处理列表
static void init_processed_list(processed_txhash_list_t *list) {
    list->capacity = 100;
    list->count = 0;
    list->hashes = (char**)malloc(list->capacity * sizeof(char*));
    for (int i = 0; i < list->capacity; i++) {
        list->hashes[i] = NULL;
    }
}

// 释放已处理列表
static void free_processed_list(processed_txhash_list_t *list) {
    for (int i = 0; i < list->count; i++) {
        if (list->hashes[i]) {
            free(list->hashes[i]);
        }
    }
    free(list->hashes);
}

// 检查是否已处理
static int is_processed(processed_txhash_list_t *list, const char *txhash) {
    for (int i = 0; i < list->count; i++) {
        if (list->hashes[i] && strcmp(list->hashes[i], txhash) == 0) {
            return 1;
        }
    }
    return 0;
}

// 添加到已处理列表
static void add_to_processed(processed_txhash_list_t *list, const char *txhash) {
    if (list->count >= list->capacity) {
        // 扩容
        list->capacity *= 2;
        list->hashes = (char**)realloc(list->hashes, list->capacity * sizeof(char*));
    }
    list->hashes[list->count] = (char*)malloc(strlen(txhash) + 1);
    strcpy(list->hashes[list->count], txhash);
    list->count++;
}

// 读取CSV文件中的新交易
static int read_new_transactions(const char *csv_file, processed_txhash_list_t *processed, 
                                 char ***new_txhashes, int *new_count) {
    FILE *file = fopen(csv_file, "r");
    if (!file) {
        return 0;  // 文件不存在或无法打开
    }
    
    char line[512];
    int capacity = 10;
    *new_count = 0;
    *new_txhashes = (char**)malloc(capacity * sizeof(char*));
    
    while (fgets(line, sizeof(line), file) != NULL) {
        // 跳过表头
        if (strncmp(line, "txhash", 6) == 0) {
            continue;
        }
        
        // 解析CSV行：txhash,address,time
        char txhash[128] = {0};
        char *p = line;
        
        // 跳过开始的引号
        if (*p == '"') p++;
        
        // 读取txhash（到第一个引号或逗号）
        int i = 0;
        while (*p && *p != '"' && *p != ',' && i < 127) {
            txhash[i++] = *p++;
        }
        txhash[i] = '\0';
        
        // 检查是否有效且未处理
        if (strlen(txhash) > 10 && !is_processed(processed, txhash)) {
            // 扩容
            if (*new_count >= capacity) {
                capacity *= 2;
                *new_txhashes = (char**)realloc(*new_txhashes, capacity * sizeof(char*));
            }
            
            // 添加新交易
            (*new_txhashes)[*new_count] = (char*)malloc(strlen(txhash) + 1);
            strcpy((*new_txhashes)[*new_count], txhash);
            (*new_count)++;
            
            // 添加到已处理列表
            add_to_processed(processed, txhash);
        }
    }
    
    fclose(file);
    return *new_count;
}

// 监控模式主循环
static void monitor_mode(auditor_state_t state) {
    const char *csv_file = "/home/zxx/A2L/A2L-master/ecdsa/bin/detect_transaction/suspicious_transactions.csv";
    processed_txhash_list_t processed;
    init_processed_list(&processed);
    
    printf("========================================\n");
    printf("审计员监控模式启动\n");
    printf("========================================\n");
    printf("[MONITOR] 监控文件: %s\n", csv_file);
    printf("[MONITOR] 等待可疑交易...\n\n");
    
    while (1) {
        char **new_txhashes = NULL;
        int new_count = 0;
        
        // 读取新交易
        if (read_new_transactions(csv_file, &processed, &new_txhashes, &new_count) > 0) {
            printf("\n========================================\n");
            printf("⚠️  检测到 %d 个新的可疑交易\n", new_count);
            printf("========================================\n\n");
            
            // 逐个审计
            for (int i = 0; i < new_count; i++) {
                printf("[MONITOR] 开始审计交易: %s\n", new_txhashes[i]);
                printf("========================================\n");
                
                audit_message(new_txhashes[i], state);
                
                printf("\n[MONITOR] 交易 %s 审计完成\n", new_txhashes[i]);
                printf("========================================\n\n");
                
                free(new_txhashes[i]);
            }
            
            free(new_txhashes);
        }
        
        // 等待5秒后继续检查
        sleep(5);
    }
    
    free_processed_list(&processed);
}

int main(int argc, char* argv[]) {
    // 初始化RECEIVER_ENDPOINTS
    get_dynamic_endpoints(RECEIVER_ENDPOINTS);
    
    // 判断运行模式
    int is_monitor_mode = 0;
    char *message_id = NULL;
    char *sender_address = NULL;
    char *pairs_summary_line = NULL;
    
    if (argc == 2) {
        if (strcmp(argv[1], "--monitor") == 0 || strcmp(argv[1], "-m") == 0) {
            is_monitor_mode = 1;
        } else {
            message_id = argv[1];
        }
    } else if (argc == 3) {
        // 新格式：auditor <tx_hash> <sender_address>
        message_id = argv[1];
        sender_address = argv[2];
    } else if (argc == 4) {
        // 新格式：auditor <tx_hash> <sender_address> <pairs_summary_line>
        message_id = argv[1];
        sender_address = argv[2];
        pairs_summary_line = argv[3];
    } else if (argc == 1) {
        fprintf(stderr, "Usage: \n");
        fprintf(stderr, "  手动模式（旧格式）: %s <message_id>\n", argv[0]);
        fprintf(stderr, "  手动模式（新格式）: %s <tx_hash> <sender_address> [pairs_summary_line]\n", argv[0]);
        fprintf(stderr, "  监控模式: %s --monitor 或 %s -m\n", argv[0], argv[0]);
        return 1;
    } else {
        fprintf(stderr, "Usage: \n");
        fprintf(stderr, "  手动模式（旧格式）: %s <message_id>\n", argv[0]);
        fprintf(stderr, "  手动模式（新格式）: %s <tx_hash> <sender_address> [pairs_summary_line]\n", argv[0]);
        fprintf(stderr, "  监控模式: %s --monitor 或 %s -m\n", argv[0], argv[0]);
        return 1;
    }
    
    if (!is_monitor_mode) {
        printf("========================================\n");
        printf("审计员手动模式启动\n");
        printf("========================================\n");
        printf("审计交易: %s\n\n", message_id);
    }
    
    // 初始化库
    if (init() != RLC_OK) {
        fprintf(stderr, "Failed to initialize libraries!\n");
        return 1;
    }
    
    // 初始化 auditor 状态
    auditor_state_t state;
    auditor_state_null(state);
   
    RLC_TRY {
        auditor_state_new(state);
       
        if (generate_cl_params(state->cl_params) != RLC_OK) {
            RLC_THROW(ERR_CAUGHT);
        }
        
        // ⭐ 从DKG公钥文件读取Auditor公钥（而不是从auditor.key读取）
        if (read_auditor_cl_pubkey(state->auditor_cl_pk) != 0) {
            fprintf(stderr, "Failed to load DKG auditor public key!\n");
            fprintf(stderr, "Please ensure DKG has been run and dkg_public.key exists.\n");
            RLC_THROW(ERR_CAUGHT);
        }
        printf("[AUDITOR] ✅ DKG 审计员公钥加载成功\n");
        
        // ⭐ 通过DKG重构Auditor私钥（而不是从auditor.key读取）
        printf("\n[AUDITOR] ========== 通过DKG重构审计员私钥 ==========\n");
        if (request_dkg_shares_and_reconstruct(state) != RLC_OK) {
            fprintf(stderr, "Failed to reconstruct auditor private key via DKG!\n");
            RLC_THROW(ERR_CAUGHT);
        }
        printf("[AUDITOR] ✅ DKG 审计员私钥重构成功\n");
        
        // 读取Alice、Bob和Tumbler的公钥（不包括Auditor密钥）
        if (read_alice_bob_tumbler_public_keys(state) != RLC_OK) {
            fprintf(stderr, "Failed to load public keys!\n");
            RLC_THROW(ERR_CAUGHT);
        }
        

        // 生成/读取第二把审计密钥对（若不存在则生成），并读取私钥供解密 outer
        if (read_auditor_cl_pubkey_named(state->auditor2_cl_pk, "auditor2") != 0) {
            printf("[WARN] auditor2.key not found, generating a new pair...\n");
            if (generate_auditor_keypair_named(state->cl_params, "auditor2") != RLC_OK) {
                fprintf(stderr, "Failed to generate auditor2 keypair!\n");
                RLC_THROW(ERR_CAUGHT);
            }
            if (read_auditor_cl_pubkey_named(state->auditor2_cl_pk, "auditor2") != 0) {
                fprintf(stderr, "Failed to load generated auditor2 public key!\n");
                RLC_THROW(ERR_CAUGHT);
            }
        }
        // 读取 auditor2 私钥
        if (read_auditor_cl_seckey_named(state->auditor2_cl_sk, "auditor2") != 0) {
            fprintf(stderr, "[WARN] Failed to load auditor2 secret key; outer may not be decryptable.\n");
        }
        
        // 根据模式执行不同的操作
        if (is_monitor_mode) {
            // 监控模式：持续监控CSV文件
            monitor_mode(state);
        } else {
            // 手动模式：执行单次审计
            if (sender_address != NULL || pairs_summary_line != NULL) {
                // 使用新格式，传入地址和pairs_summary信息
                audit_message_with_info(message_id, sender_address, pairs_summary_line, state);
            } else {
                // 使用旧格式，只传入message_id
            audit_message(message_id, state);
            }
        }
        
    } RLC_CATCH_ANY {
        fprintf(stderr, "Auditor execution failed!\n");
        return 1;
    } RLC_FINALLY {
        if (state != NULL) {
            auditor_state_free(state);
        }
    }
    
    // 清理库
    clean();
    return 0;
}
