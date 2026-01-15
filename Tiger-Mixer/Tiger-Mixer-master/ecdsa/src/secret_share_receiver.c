#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include "zmq.h"
#include "tumbler.h"
#include "types.h"
#include "util.h"
#include "secret_share.h"
#include "dkg_integration.h"
#include "committee_integration.h"  // 恢复委员会集成
#include "reputation_tracker.h"  // 声誉跟踪系统

#define SECRET_SHARES 3
#define THRESHOLD 2
#define SHARE_SIZE 17000
#define MAX_MESSAGE_SIZE 17000
// MSG_ID_MAXLEN is already defined in secret_share.h
#define SLICE_DIR "./SliceMessage"
#define COMMITTEE_MEMBERS_FILE "/home/zxx/A2L/A2L-master/ecdsa/committee_members.txt"
#define ADDRESS_CACHE_TTL 5  // 地址缓存时间（秒）

// 委员会集成全局变量
static char g_my_address[64] = {0};
static int g_my_position = -1;
static committee_state_t g_committee_state = {0};

// 地址读取缓存
typedef struct {
    char address[64];
    time_t last_read_time;
    int member_index;
    int valid;
} address_cache_t;

static address_cache_t g_address_cache = {0};

// 从文件读取委员会成员地址
// member_index: 1, 2, 3 (对应文件的第1、2、3行)
// 返回值: 0成功, -1失败
static int read_committee_member_address(int member_index, char* address_out, size_t address_size) {
    if (member_index < 1 || member_index > 3) {
        fprintf(stderr, "[ADDRESS] 无效的成员索引: %d (必须是1-3)\n", member_index);
        return -1;
    }
    
    // 检查缓存
    time_t now = time(NULL);
    if (g_address_cache.valid && 
        g_address_cache.member_index == member_index &&
        (now - g_address_cache.last_read_time) < ADDRESS_CACHE_TTL) {
        strncpy(address_out, g_address_cache.address, address_size - 1);
        address_out[address_size - 1] = '\0';
        return 0;
    }
    
    // 读取文件
    FILE* fp = fopen(COMMITTEE_MEMBERS_FILE, "r");
    if (!fp) {
        fprintf(stderr, "[ADDRESS] 无法打开委员会成员文件: %s\n", COMMITTEE_MEMBERS_FILE);
        perror("fopen");
        
        // 如果有缓存，返回缓存（容错）
        if (g_address_cache.valid && g_address_cache.member_index == member_index) {
            fprintf(stderr, "[ADDRESS] ⚠️  使用缓存的地址: %s\n", g_address_cache.address);
            strncpy(address_out, g_address_cache.address, address_size - 1);
            address_out[address_size - 1] = '\0';
            return 0;
        }
        return -1;
    }
    
    char line[256];
    int current_line = 0;
    char new_address[64] = {0};
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        current_line++;
        
        // 去除换行符
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }
        
        if (current_line == member_index) {
            // 验证地址格式（以0x开头，长度为42）
            if (len == 42 && line[0] == '0' && line[1] == 'x') {
                strncpy(new_address, line, sizeof(new_address) - 1);
                new_address[sizeof(new_address) - 1] = '\0';
                found = 1;
            } else {
                fprintf(stderr, "[ADDRESS] ⚠️  地址格式无效: %s (长度=%zu)\n", line, len);
            }
            break;
        }
    }
    
    fclose(fp);
    
    if (!found) {
        fprintf(stderr, "[ADDRESS] ❌ 未找到第%d行的地址\n", member_index);
        
        // 如果有缓存，返回缓存（容错）
        if (g_address_cache.valid && g_address_cache.member_index == member_index) {
            fprintf(stderr, "[ADDRESS] ⚠️  使用缓存的地址: %s\n", g_address_cache.address);
            strncpy(address_out, g_address_cache.address, address_size - 1);
            address_out[address_size - 1] = '\0';
            return 0;
        }
        return -1;
    }
    
    // 检测地址变更
    if (g_address_cache.valid && 
        g_address_cache.member_index == member_index &&
        strcmp(g_address_cache.address, new_address) != 0) {
        printf("🔄 [ADDRESS] 委员会成员 #%d 地址已更新\n", member_index);
        printf("   旧地址: %s\n", g_address_cache.address);
        printf("   新地址: %s\n", new_address);
    }
    
    // 更新缓存
    strncpy(g_address_cache.address, new_address, sizeof(g_address_cache.address) - 1);
    g_address_cache.address[sizeof(g_address_cache.address) - 1] = '\0';
    g_address_cache.last_read_time = now;
    g_address_cache.member_index = member_index;
    g_address_cache.valid = 1;
    
    // 输出结果
    strncpy(address_out, new_address, address_size - 1);
    address_out[address_size - 1] = '\0';
    
    return 0;
}

// 获取动态端点 - 委员会集成版本
int get_dynamic_endpoints_committee(char endpoints[SECRET_SHARES][64]) {
    if (!g_committee_state.system_initialized) {
        // 如果委员会未初始化，使用固定地址
        const char* default_addresses[] = {
            "tcp://localhost:5555",
            "tcp://localhost:5556", 
            "tcp://localhost:5557"
        };
        for (int i = 0; i < SECRET_SHARES; i++) {
            strncpy(endpoints[i], default_addresses[i], 63);
            endpoints[i][63] = '\0';
        }
        return 0;
    }
    
    for (int i = 0; i < SECRET_SHARES; i++) {
        if (g_committee_state.members[i].is_active) {
            snprintf(endpoints[i], 64, "tcp://%s:%d", 
                    g_committee_state.members[i].address, 
                    RECEIVER_PORTS[i]);
        } else {
            strcpy(endpoints[i], "");
        }
    }
    return 0;
}

// 秘密分享结构体
// typedef struct {
//     int x;                          // 分享点的x坐标
//     uint8_t y[SHARE_SIZE];         // 分享点的y坐标
//     size_t data_length;            // 数据长度
// } secret_share_t;

// 将y转为十六进制字符串
static void y_to_hex(const uint8_t* y, size_t len, char* out_hex, size_t out_size) {
    size_t i;
    for (i = 0; i < len && i * 2 + 1 < out_size; ++i) {
        sprintf(out_hex + i * 2, "%02x", y[i]);
    }
    out_hex[i * 2] = '\0';
}

// 保存分片到JSON文件（包含地址信息）
static int save_share_to_json_with_address(const char* json_filename, const char* msg_id, 
                                           const secret_share_t* share, const char* address) {
    printf("[SAVE] Attempting to save share to file: %s\n", json_filename);
    printf("[SAVE] Message ID: %s, x: %d, data_length: %zu, address: %s\n", 
           msg_id, share->x, share->data_length, address);
    
    FILE* fp = fopen(json_filename, "a");
    if (!fp) {
        fprintf(stderr, "[SAVE] Error: Could not open file %s for writing\n", json_filename);
        perror("fopen error");
        return -1;
    }
    
    char y_hex[SHARE_SIZE * 2 + 1];
    y_to_hex(share->y, share->data_length, y_hex, sizeof(y_hex));
    
    // 获取当前时间戳
    time_t now = time(NULL);
    
    // 追加为一行一个JSON对象，包含地址、时间戳、block_index和block_size
    int result = fprintf(fp, "{\"msg_id\":\"%s\",\"x\":%d,\"block_index\":%zu,\"block_size\":%zu,\"data_length\":%zu,\"y\":\"%s\",\"address\":\"%s\",\"timestamp\":%ld}\n", 
                        msg_id, share->x, share->block_index, share->block_size, share->data_length, y_hex, address, (long)now);
    
    if (result < 0) {
        fprintf(stderr, "[SAVE] Error: Failed to write to file\n");
        fclose(fp);
        return -1;
    }
    
    // 强制刷新缓冲区
    fflush(fp);
    fclose(fp);
    
    printf("[SAVE] Successfully saved share to file (with address info)\n");
    return 0;
}

// 保存分片到JSON文件（兼容旧版本，不包含地址）
static int save_share_to_json(const char* json_filename, const char* msg_id, const secret_share_t* share) {
    return save_share_to_json_with_address(json_filename, msg_id, share, "unknown");
}

// 从JSON文件查找分片（兼容旧格式，只加载第一个匹配的分享）
static int load_share_from_json(const char* json_filename, const char* msg_id, secret_share_t* share) {
    FILE* fp = fopen(json_filename, "r");
    if (!fp) {
        printf("[LOAD] Failed to open file: %s\n", json_filename);
        return -1;
    }
    
    printf("[LOAD] Looking for msgid: %s (length: %zu)\n", msg_id, strlen(msg_id));
    
    char line[65536];
    int line_count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_count++;
        char file_msgid[MSG_ID_MAXLEN] = {0};
        int x = 0;
        size_t block_index = 0;
        size_t block_size = 0;
        size_t data_length = 0;
        char y_hex[SHARE_SIZE * 2 + 1] = {0};
        
        // 尝试新格式（包含block_index和block_size）
        int scan_result = sscanf(line, "{\"msg_id\":\"%123[^\"]\",\"x\":%d,\"block_index\":%zu,\"block_size\":%zu,\"data_length\":%zu,\"y\":\"%[^\"]", 
                                 file_msgid, &x, &block_index, &block_size, &data_length, y_hex);
        
        // 如果新格式失败，尝试旧格式（兼容性）
        if (scan_result != 6) {
            scan_result = sscanf(line, "{\"msg_id\":\"%123[^\"]\",\"x\":%d,\"data_length\":%zu,\"y\":\"%[^\"]", 
                                 file_msgid, &x, &data_length, y_hex);
            if (scan_result == 4) {
                block_index = 0;  // 默认值
                block_size = data_length;  // 默认值
            }
        }
        
        if (scan_result == 4 || scan_result == 6) {
            // 只在前几行打印调试信息，避免输出太多
            if (line_count <= 5) {
                printf("[LOAD] Line %d: found msgid='%s' (len=%zu), x=%d\n", 
                       line_count, file_msgid, strlen(file_msgid), x);
            }
            
            if (strcmp(file_msgid, msg_id) == 0) {
                printf("[LOAD] Found matching share! x=%d, block_index=%zu, data_length=%zu\n", x, block_index, data_length);
                share->x = x;
                share->block_index = block_index;
                share->block_size = block_size;
                share->data_length = data_length;
                for (size_t i = 0; i < data_length && i < SHARE_SIZE; ++i) {
                    unsigned int byte;
                    sscanf(y_hex + i * 2, "%2x", &byte);
                    share->y[i] = (uint8_t)byte;
                }
                fclose(fp);
                return 0;
            }
        } else {
            if (line_count <= 5) {
                printf("[LOAD] Line %d: sscanf failed (result=%d)\n", line_count, scan_result);
            }
        }
    }
    
    printf("[LOAD] Scanned %d lines, no match found\n", line_count);
    fclose(fp);
    return -1;
}

// 从JSON文件加载所有块的分享（对于给定的msg_id和x）
static int load_all_shares_from_json(const char* json_filename, const char* msg_id, int x, 
                                     secret_share_t* shares, size_t max_shares, size_t* num_shares_out) {
    // 参数验证
    if (!json_filename || !msg_id || !shares || !num_shares_out || max_shares == 0) {
        printf("[LOAD_ALL] Error: Invalid parameters\n");
        return -1;
    }
    
    FILE* fp = fopen(json_filename, "r");
    if (!fp) {
        printf("[LOAD_ALL] Failed to open file: %s (errno=%d: %s)\n", json_filename, errno, strerror(errno));
        return -1;
    }
    
    printf("[LOAD_ALL] Looking for all shares with msgid: %s, x=%d in file: %s\n", msg_id, x, json_filename);
    
    char line[65536];
    size_t found_count = 0;
    int line_count = 0;
    
    while (fgets(line, sizeof(line), fp) && found_count < max_shares) {
        line_count++;
        
        // 限制读取长度，防止缓冲区溢出
        if (line_count % 1000 == 0) {
            // printf("[LOAD_ALL] Processed %d lines, found %zu shares so far...\n", line_count, found_count);
        }
        
        char file_msgid[MSG_ID_MAXLEN] = {0};
        int file_x = 0;
        size_t block_index = 0;
        size_t block_size = 0;
        size_t data_length = 0;
        char y_hex[SHARE_SIZE * 2 + 1] = {0};  // 确保有足够的空间
        
        // 尝试新格式（包含block_index和block_size）
        // 注意：限制y_hex的读取长度，防止缓冲区溢出（SHARE_SIZE * 2 = 34000）
        // 使用%34000[^\"]来限制读取长度
        int scan_result = sscanf(line, "{\"msg_id\":\"%123[^\"]\",\"x\":%d,\"block_index\":%zu,\"block_size\":%zu,\"data_length\":%zu,\"y\":\"%34000[^\"]", 
                                 file_msgid, &file_x, &block_index, &block_size, &data_length, y_hex);
        
        // 如果新格式失败，尝试旧格式（兼容性）
        if (scan_result != 6) {
            scan_result = sscanf(line, "{\"msg_id\":\"%123[^\"]\",\"x\":%d,\"data_length\":%zu,\"y\":\"%34000[^\"]", 
                                 file_msgid, &file_x, &data_length, y_hex);
            if (scan_result == 4) {
                block_index = found_count;  // 使用顺序索引
                block_size = data_length;  // 默认值
            }
        }
        
        // 确保y_hex以null结尾，防止后续操作越界
        y_hex[SHARE_SIZE * 2] = '\0';
        
        if ((scan_result == 4 || scan_result == 6) && 
            strcmp(file_msgid, msg_id) == 0 && file_x == x) {
            secret_share_t* share = &shares[found_count];
            share->x = file_x;
            share->block_index = block_index;
            share->block_size = block_size;
            share->data_length = data_length;
            
            // 解析y_hex：将hex字符串转换回字节数组
            size_t hex_len = strlen(y_hex);
            if (hex_len == 0 || hex_len % 2 != 0) {
                printf("[LOAD_ALL] Warning: Invalid hex length %zu for block %zu, skipping\n", hex_len, block_index);
                continue;
            }
            
            size_t bytes_to_read = hex_len / 2;
            if (bytes_to_read > SHARE_SIZE) {
                printf("[LOAD_ALL] Warning: Hex data too large (%zu > %d) for block %zu, truncating\n", 
                       bytes_to_read, SHARE_SIZE, block_index);
                bytes_to_read = SHARE_SIZE;
            }
            
            // 将hex字符串转换为字节数组
            for (size_t i = 0; i < bytes_to_read; ++i) {
                unsigned int byte;
                if (sscanf(y_hex + i * 2, "%2x", &byte) == 1) {
                    share->y[i] = (uint8_t)byte;
                } else {
                    printf("[LOAD_ALL] Warning: Failed to parse hex byte at position %zu for block %zu\n", i, block_index);
                    share->y[i] = 0;
                }
            }
            
            found_count++;
        }
    }
    
    fclose(fp);
    *num_shares_out = found_count;
    
    if (found_count == 0) {
        printf("[LOAD_ALL] No shares found for msgid: %s, x=%d\n", msg_id, x);
        return -1;
    }
    
    printf("[LOAD_ALL] Loaded %zu shares for msgid: %s, x=%d\n", found_count, msg_id, x);
    return 0;
}

// 解析接收到的分享消息
static int parse_share_message(uint8_t* data, size_t data_size, secret_share_t* share, char* msg_type, char* out_msgid) {
    // 新格式：x | block_index | data_length | block_size | y
    if (data_size < sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(size_t) + 4) {
        fprintf(stderr, "Error: share message too small\n");
        return -1;
    }
    
    size_t offset = 0;
    memcpy(&share->x, data + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&share->block_index, data + offset, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(&share->data_length, data + offset, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(&share->block_size, data + offset, sizeof(size_t));
    offset += sizeof(size_t);
    
    // 读取y数组的长度
    size_t share_data_size = data[offset] |
                            (data[offset + 1] << 8) |
                            (data[offset + 2] << 16) |
                            (data[offset + 3] << 24);
    offset += 4;
    
    if (share_data_size > SHARE_SIZE - 4) {
        fprintf(stderr, "Error: share data size too large (%zu > %d)\n", share_data_size, SHARE_SIZE - 4);
        return -1;
    }
    
    if (data_size < offset + share_data_size) {
        fprintf(stderr, "Error: insufficient data for share\n");
        return -1;
    }
    
    memcpy(share->y, data + offset - 4, share_data_size + 4);  // 包含长度字段
    // 提取msg_id
    char* sep = strchr(msg_type, '|');
    if (sep) {
        strncpy(out_msgid, sep + 1, MSG_ID_MAXLEN-1);
        out_msgid[MSG_ID_MAXLEN-1] = '\0';
    } else {
        strncpy(out_msgid, msg_type, MSG_ID_MAXLEN-1);
        out_msgid[MSG_ID_MAXLEN-1] = '\0';
    }
    return 0;
}

// 调用judge API判断是否需要审计
// 返回: 0=需要审计(返回1), 1=不需要审计(返回0), -1=API调用失败
static int call_judge_api(const char* pairs_summary_json) {
    if (!pairs_summary_json || strlen(pairs_summary_json) == 0) {
        printf("[JUDGE_API] pairs_summary_json为空，跳过API调用\n");
        return -1;
    }
    
    // 使用临时文件存储JSON数据，避免shell命令注入问题
    char tmp_file[256];
    snprintf(tmp_file, sizeof(tmp_file), "/tmp/judge_api_%d_%ld.json", getpid(), time(NULL));
    
    // 将JSON写入临时文件
    FILE* json_fp = fopen(tmp_file, "w");
    if (!json_fp) {
        fprintf(stderr, "[JUDGE_API] Error: 无法创建临时文件: %s\n", tmp_file);
        return -1;
    }
    
    fprintf(json_fp, "%s", pairs_summary_json);
    fclose(json_fp);
    
    // 构造curl命令，使用 -d @filename 方式读取JSON文件
    char curl_cmd[512];
    snprintf(curl_cmd, sizeof(curl_cmd), 
             "curl -s -X POST -H 'Content-Type: application/json' -d @%s http://127.0.0.1:8001/judge",
             tmp_file);
    
    printf("[JUDGE_API] 执行命令: curl -s -X POST -H 'Content-Type: application/json' -d @%s http://127.0.0.1:8001/judge\n", tmp_file);
    printf("[JUDGE_API] JSON数据: %s\n", pairs_summary_json);
    fflush(stdout);
    
    // 使用popen执行命令并获取输出
    FILE* fp = popen(curl_cmd, "r");
    if (!fp) {
        fprintf(stderr, "[JUDGE_API] Error: popen failed\n");
        unlink(tmp_file);  // 清理临时文件
        return -1;
    }
    
    char response[1024] = {0};
    size_t total_read = 0;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL && total_read < sizeof(response) - 1) {
        size_t len = strlen(buffer);
        if (total_read + len < sizeof(response) - 1) {
            memcpy(response + total_read, buffer, len);
            total_read += len;
        }
    }
    
    int status = pclose(fp);
    
    // 清理临时文件
    unlink(tmp_file);
    
    if (status != 0) {
        fprintf(stderr, "[JUDGE_API] Error: curl命令执行失败，返回码: %d\n", status);
        return -1;
    }
    
    printf("[JUDGE_API] API响应: %s\n", response);
    fflush(stdout);
    
    // 解析JSON响应: {"message":"0"} 或 {"message":"1"}
    // 简单解析：查找 "message":" 后面的值
    const char* message_key = "\"message\":\"";
    char* message_pos = strstr(response, message_key);
    if (!message_pos) {
        fprintf(stderr, "[JUDGE_API] Error: 无法在响应中找到message字段\n");
        return -1;
    }
    
    message_pos += strlen(message_key);
    if (*message_pos == '0') {
        printf("[JUDGE_API] 判断结果: 0 (不需要审计)\n");
        return 1;  // 返回1表示不需要审计
    } else if (*message_pos == '1') {
        printf("[JUDGE_API] 判断结果: 1 (需要审计)\n");
        return 0;  // 返回0表示需要审计
    } else {
        fprintf(stderr, "[JUDGE_API] Error: 未知的message值: %c\n", *message_pos);
        return -1;
    }
}

// 处理审计员请求
static void handle_audit_request(void* socket, int tag, const char* msg_id, const char* pairs_summary_json, const char* json_filename, int participant_id) {
    // tag: 0=第一个分片(Alice相关), 1=第二个分片(Bob相关)
    // pairs_summary_json: JSON格式的pairs_summary信息，格式为 {"id":"地址","record":"(0,1)(1,1)..."}
    // participant_id 由调用者传入（receiver的participant_id，1-based）
    printf("[AUDIT_REQUEST] DEBUG: Function entry, socket=%p, tag=%d\n", socket, tag);
    fflush(stdout);
    
    // 获取当前委员会成员地址（用于决策记录）
    char current_address[64] = {0};
    // 直接使用read_committee_member_address，因为participant_id是1-based
    if (read_committee_member_address(participant_id, current_address, sizeof(current_address)) != 0) {
        fprintf(stderr, "[AUDIT_REQUEST] WARNING: 无法获取当前地址，使用默认值\n");
        snprintf(current_address, sizeof(current_address), "unknown_%d", participant_id);
    }
    printf("[AUDIT_REQUEST] 当前委员会成员地址: %s (participant_id=%d)\n", current_address, participant_id);
    fflush(stdout);
    
    // 参数验证 - 分步检查，避免在printf中访问无效指针
    printf("[AUDIT_REQUEST] DEBUG: Checking parameters...\n");
    fflush(stdout);
    
    if (tag != 0 && tag != 1) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: Invalid tag: %d (must be 0 or 1)\n", tag);
        fflush(stderr);
        if (socket) {
            const char* response = "INVALID_TAG";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    
    // 提取用户地址（用于决策记录）
    char user_address[64] = {0};
    int has_user_address = 0;
    if (tag == 0 && pairs_summary_json && strlen(pairs_summary_json) > 0) {
        if (extract_address_from_json(pairs_summary_json, user_address, sizeof(user_address)) == 0) {
            has_user_address = 1;
            printf("[AUDIT_REQUEST] 提取用户地址: %s\n", user_address);
        }
    }
    
    // 构造request_id（用于决策记录）
    char request_id[256] = {0};
    if (msg_id) {
        time_t now = time(NULL);
        snprintf(request_id, sizeof(request_id), "%s_%ld", msg_id, now);
    }
    
    // 如果tag==0，先调用judge API判断是否需要审计
    // call_judge_api返回值: 1=不需要审计(API返回0), 0=需要审计(API返回1), -1=API调用失败
    int judge_result = -1;  // call_judge_api的返回值
    int api_raw_result = -1;  // API原始返回值: 0=不需要审计, 1=需要审计, -1=未调用
    if (tag == 0) {
        printf("[AUDIT_REQUEST] tag=0，调用judge API判断是否需要审计...\n");
        fflush(stdout);
        
        judge_result = call_judge_api(pairs_summary_json);
        
        // 将call_judge_api的返回值转换为API原始返回值
        // call_judge_api返回1表示API返回0（不需要审计）
        // call_judge_api返回0表示API返回1（需要审计）
        if (judge_result == 1) {
            api_raw_result = 0;  // API返回0，不需要审计
        } else if (judge_result == 0) {
            api_raw_result = 1;  // API返回1，需要审计
        } else {
            api_raw_result = -1;  // API调用失败
        }
        
        if (judge_result == 1) {
            // API返回0，不需要审计
            printf("[AUDIT_REQUEST] judge API返回0，不需要审计，跳过查找分片\n");
            fflush(stdout);
            const char* response = "NO_AUDIT_NEEDED";
            zmq_send(socket, response, strlen(response), 0);
            
            // 记录决策：不需要审计，发送NO_AUDIT_NEEDED
            // api_raw_result=0表示API返回0（不需要审计）
            if (has_user_address) {
                reputation_tracker_record_decision(request_id, user_address, current_address, 
                                                   api_raw_result, "no_audit_needed");  // api_raw_result=0
            }
            return;
        } else if (judge_result == -1) {
            // API调用失败，记录警告但继续执行（容错处理）
            printf("[AUDIT_REQUEST] WARNING: judge API调用失败，继续执行审计流程\n");
            fflush(stdout);
        } else {
            // judge_result == 0，API返回1，需要审计，继续执行
            printf("[AUDIT_REQUEST] judge API返回1，需要审计，继续查找分片\n");
            fflush(stdout);
        }
    } else {
        // tag == 1，直接查找分片，不需要调用API
        printf("[AUDIT_REQUEST] tag=1，直接查找分片，跳过judge API调用\n");
        fflush(stdout);
    }
    
    if (!msg_id) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: msg_id is NULL\n");
        fflush(stderr);
        if (socket) {
            const char* response = "INVALID_PARAMS";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    
    printf("[AUDIT_REQUEST] DEBUG: msg_id is valid: %s\n", msg_id);
    fflush(stdout);
    
    if (!json_filename) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: json_filename is NULL\n");
        fflush(stderr);
        if (socket) {
            const char* response = "INVALID_PARAMS";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    
    printf("[AUDIT_REQUEST] DEBUG: json_filename is valid: %s\n", json_filename);
    fflush(stdout);
    
    if (participant_id < 1 || participant_id > SECRET_SHARES) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: Invalid participant_id: %d (must be 1-%d)\n", 
                participant_id, SECRET_SHARES);
        fflush(stderr);
        if (socket) {
            const char* response = "INVALID_PARAMS";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    
    printf("[AUDIT_REQUEST] DEBUG: participant_id is valid: %d\n", participant_id);
    fflush(stdout);
    
    printf("[AUDIT_REQUEST] Handling audit request: tag=%d, msg_id=%s, participant_id=%d\n", 
           tag, msg_id, participant_id);
    printf("[AUDIT_REQUEST] JSON filename: %s\n", json_filename);
    if (pairs_summary_json != NULL && strlen(pairs_summary_json) > 0) {
        printf("[AUDIT_REQUEST] pairs_summary_json: %s\n", pairs_summary_json);
    }
    fflush(stdout);
    
    // 验证文件是否存在
    printf("[AUDIT_REQUEST] DEBUG: About to open file: %s\n", json_filename);
    fflush(stdout);
    
    FILE* test_fp = fopen(json_filename, "r");
    if (!test_fp) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: Cannot open file %s (errno=%d: %s)\n", 
                json_filename, errno, strerror(errno));
        fflush(stderr);
        if (socket) {
            const char* response = "FILE_NOT_FOUND";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    fclose(test_fp);
    
    printf("[AUDIT_REQUEST] DEBUG: File exists and is readable\n");
    fflush(stdout);
    
    // 加载所有块的分享
    // ⚠️ 重要：使用动态内存分配，避免栈溢出
    // secret_share_t 很大（包含 100KB 的 y 数组），1000 个结构体约 100MB，栈上分配会导致段错误
    const size_t max_shares = 1000;
    size_t share_array_size = sizeof(secret_share_t) * max_shares;
    printf("[AUDIT_REQUEST] DEBUG: Allocating shares array (size: %zu bytes = %.2f MB)...\n", 
           share_array_size, share_array_size / (1024.0 * 1024.0));
    fflush(stdout);
    
    secret_share_t* shares = (secret_share_t*)malloc(share_array_size);
    if (!shares) {
        fprintf(stderr, "[AUDIT_REQUEST] Error: Failed to allocate memory for shares array (%zu bytes)\n", share_array_size);
        fflush(stderr);
        if (socket) {
            const char* response = "MEMORY_ERROR";
            zmq_send(socket, response, strlen(response), 0);
        }
        return;
    }
    
    printf("[AUDIT_REQUEST] DEBUG: Shares array allocated at %p\n", shares);
    fflush(stdout);
    
    memset(shares, 0, share_array_size);  // 初始化为0
    printf("[AUDIT_REQUEST] DEBUG: Shares array initialized\n");
    fflush(stdout);
    
    size_t num_shares = 0;
    
    printf("[AUDIT_REQUEST] DEBUG: About to call load_all_shares_from_json\n");
    printf("[AUDIT_REQUEST] DEBUG: Parameters: json_filename=%s, msg_id=%s, participant_id=%d\n", 
           json_filename, msg_id, participant_id);
    fflush(stdout);
    
    int load_result = load_all_shares_from_json(json_filename, msg_id, participant_id, shares, max_shares, &num_shares);
    
    printf("[AUDIT_REQUEST] DEBUG: load_all_shares_from_json returned: %d, num_shares: %zu\n", load_result, num_shares);
    fflush(stdout);
    if (load_result == 0) {
        printf("[AUDIT_REQUEST] Successfully loaded %zu shares\n", num_shares);
        // 计算消息大小：participant_id + num_blocks + (每个块的 block_index + block_size + data_length + share_value)
        size_t msg_size = sizeof(int) + sizeof(size_t);
        for (size_t i = 0; i < num_shares; i++) {
            size_t share_data_size = shares[i].y[0] |
                                    (shares[i].y[1] << 8) |
                                    (shares[i].y[2] << 16) |
                                    (shares[i].y[3] << 24);
            msg_size += sizeof(size_t) * 3 + 4 + share_data_size;  // block_index + block_size + data_length + 4字节长度 + share_value
        }
        
        uint8_t* msg_data = malloc(msg_size);
        if (!msg_data) {
            const char* response = "MEMORY_ERROR";
            zmq_send(socket, response, strlen(response), 0);
            printf("Memory allocation failed for audit request\n");
            free(shares);  // 释放 shares 内存
            return;
        }
        
        // 打包消息：participant_id || num_blocks || (block_index || block_size || data_length || share_value)...
        size_t offset = 0;
        memcpy(msg_data + offset, &participant_id, sizeof(int));
        offset += sizeof(int);
        memcpy(msg_data + offset, &num_shares, sizeof(size_t));
        offset += sizeof(size_t);
        
        for (size_t i = 0; i < num_shares; i++) {
            memcpy(msg_data + offset, &shares[i].block_index, sizeof(size_t));
            offset += sizeof(size_t);
            memcpy(msg_data + offset, &shares[i].block_size, sizeof(size_t));
            offset += sizeof(size_t);
            memcpy(msg_data + offset, &shares[i].data_length, sizeof(size_t));
            offset += sizeof(size_t);
            
            // 复制share_value（包含4字节长度字段）
            size_t share_data_size = shares[i].y[0] |
                                    (shares[i].y[1] << 8) |
                                    (shares[i].y[2] << 16) |
                                    (shares[i].y[3] << 24);
            memcpy(msg_data + offset, shares[i].y, 4 + share_data_size);
            offset += 4 + share_data_size;
        }
        
        int send_result = zmq_send(socket, msg_data, msg_size, 0);
        if (send_result == msg_size) {
            printf("[AUDIT_REQUEST] Successfully sent %zu shares (blocks) for audit: %s (x=%d)\n", 
                   num_shares, msg_id, participant_id);
            
            // 记录决策：找到并发送了shares
            // api_raw_result=1表示API返回1（需要审计）
            if (tag == 0 && has_user_address && judge_result == 0) {
                reputation_tracker_record_decision(request_id, user_address, current_address, 
                                                   api_raw_result, "provided_shares");  // api_raw_result=1
            }
        } else {
            printf("[AUDIT_REQUEST] Error: Failed to send shares (sent %d, expected %zu)\n", 
                   send_result, msg_size);
        }
        free(msg_data);
    } else {
        const char* response = "NOT_FOUND";
        zmq_send(socket, response, strlen(response), 0);
        printf("[AUDIT_REQUEST] No shares found for audit: %s (participant_id=%d)\n", msg_id, participant_id);
        
        // 记录决策：未找到shares
        // api_raw_result=1表示API返回1（需要审计），但未找到shares
        if (tag == 0 && has_user_address && judge_result == 0) {
            reputation_tracker_record_decision(request_id, user_address, current_address, 
                                               api_raw_result, "not_found");  // api_raw_result=1
        }
    }
    
    // 释放动态分配的内存
    if (shares) {
        free(shares);
        printf("[AUDIT_REQUEST] DEBUG: Freed shares array memory\n");
        fflush(stdout);
    }
}

// 处理DKG私钥分片请求
static void handle_dkg_share_request(void* socket, int participant_id) {
    printf("[DKG_SHARE_REQUEST] 参与者%d处理私钥分片请求\n", participant_id);
    
    // 从文件加载DKG密钥
    char key_file_path[256];
    snprintf(key_file_path, sizeof(key_file_path), "../keys/dkg_participant_%d.key", participant_id);
    
    FILE *key_file = fopen(key_file_path, "rb");
    if (!key_file) {
        printf("[DKG_SHARE_REQUEST] 无法打开私钥文件: %s\n", key_file_path);
        const char* response = "DKG_KEY_NOT_FOUND";
        zmq_send(socket, response, strlen(response), 0);
        return;
    }
    
    // 读取文件头
    uint32_t header[4];
    if (fread(header, sizeof(uint32_t), 4, key_file) != 4) {
        printf("[DKG_SHARE_REQUEST] 读取文件头失败\n");
        fclose(key_file);
        const char* response = "DKG_READ_ERROR";
        zmq_send(socket, response, strlen(response), 0);
        return;
    }
    
    int file_participant_id = ntohl(header[0]);
    int file_n_participants = ntohl(header[1]);
    int file_threshold = ntohl(header[2]);
    int secret_share_len = ntohl(header[3]);
    
    printf("[DKG_SHARE_REQUEST] 文件信息: 参与者=%d, 总数=%d, 阈值=%d, 分片长度=%d\n",
           file_participant_id, file_n_participants, file_threshold, secret_share_len);
    
    // 读取私钥分片
    uint8_t secret_share_buf[RLC_BN_SIZE];
    if (fread(secret_share_buf, 1, secret_share_len, key_file) != secret_share_len) {
        printf("[DKG_SHARE_REQUEST] 读取私钥分片失败\n");
        fclose(key_file);
        const char* response = "DKG_READ_ERROR";
        zmq_send(socket, response, strlen(response), 0);
        return;
    }
    
    fclose(key_file);
    
    // 准备响应消息: [participant_id(4)] [secret_share_len(4)] [secret_share_data]
    size_t response_size = sizeof(int) * 2 + secret_share_len;
    uint8_t *response_data = malloc(response_size);
    if (!response_data) {
        printf("[DKG_SHARE_REQUEST] 内存分配失败\n");
        const char* response = "DKG_MEMORY_ERROR";
        zmq_send(socket, response, strlen(response), 0);
        return;
    }
    
    size_t offset = 0;
    memcpy(response_data + offset, &file_participant_id, sizeof(int));
    offset += sizeof(int);
    memcpy(response_data + offset, &secret_share_len, sizeof(int));
    offset += sizeof(int);
    memcpy(response_data + offset, secret_share_buf, secret_share_len);
    
    // 发送响应
    if (zmq_send(socket, response_data, response_size, 0) != response_size) {
        printf("[DKG_SHARE_REQUEST] 发送响应失败\n");
    } else {
        printf("[DKG_SHARE_REQUEST] 成功发送参与者%d的私钥分片，长度=%d\n", 
               file_participant_id, secret_share_len);
    }
    
    free(response_data);
}

// 获取当前接收者的地址（支持动态更新）
// 获取当前接收者的地址（支持动态更新）
static int get_my_current_address(int member_index, char* address_out, size_t address_size) {
    return read_committee_member_address(member_index, address_out, address_size);
}

// 接收者线程函数
static void* receiver_thread(void* arg) {
    // 提取receiver_id并立即释放传入的指针
    int receiver_id = *(int*)arg;
    free(arg);  // 释放堆分配的内存
    
    int port = RECEIVER_PORTS[receiver_id];
    int member_index = receiver_id + 1;  // receiver_id是0-2，member_index是1-3
    char json_filename[256];
    // 确保目录存在
    printf("[RECEIVER] Creating directory: %s\n", SLICE_DIR);
    if (mkdir(SLICE_DIR, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "[RECEIVER] Warning: Could not create directory %s: %s\n", SLICE_DIR, strerror(errno));
    } else {
        printf("[RECEIVER] Directory ready: %s\n", SLICE_DIR);
    }
    
    snprintf(json_filename, sizeof(json_filename), SLICE_DIR "/receiver_%d.json", receiver_id + 1);
    printf("[RECEIVER] Will save shares to: %s\n", json_filename);
    
    printf("Starting receiver %d on port %d\n", receiver_id + 1, port);
    printf("Local JSON file: %s\n", json_filename);
    
    // 创建 ZeroMQ 上下文和套接字
    void* context = zmq_ctx_new();
    if (!context) {
        fprintf(stderr, "Error: could not create context for receiver %d\n", receiver_id + 1);
        return NULL;
    }
    void* socket = zmq_socket(context, ZMQ_REP);
    if (!socket) {
        fprintf(stderr, "Error: could not create socket for receiver %d\n", receiver_id + 1);
        zmq_ctx_destroy(context);
        return NULL;
    }
    char endpoint[64];
    snprintf(endpoint, sizeof(endpoint), "tcp://*:%d", port);
    int rc = zmq_bind(socket, endpoint);
    if (rc != 0) {
        fprintf(stderr, "Error: could not bind receiver %d to %s\n", receiver_id + 1, endpoint);
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return NULL;
    }
    printf("Receiver %d listening on %s\n", receiver_id + 1, endpoint);
    
    // 消息接收循环
    while (1) {
        // 每次处理新消息前，读取最新的地址（支持动态轮换）
        char current_address[64];
        if (get_my_current_address(member_index, current_address, sizeof(current_address)) == 0) {
            // 地址读取成功，可以在需要时使用
            // 注意：这里只是读取并缓存，实际使用时再获取
        }
        
        zmq_msg_t zmq_message;
        rc = zmq_msg_init(&zmq_message);
        if (rc != 0) {
            fprintf(stderr, "Error: could not initialize message for receiver %d\n", receiver_id + 1);
            continue;
        }
        rc = zmq_msg_recv(&zmq_message, socket, 0);
        if (rc == -1) {
            fprintf(stderr, "Error: failed to receive message on receiver %d\n", receiver_id + 1);
            zmq_msg_close(&zmq_message);
            continue;
        }
        size_t msg_size = zmq_msg_size(&zmq_message);
        uint8_t* msg_data = (uint8_t*)zmq_msg_data(&zmq_message);
        printf("Receiver %d (地址: %s) received message of %zu bytes\n", 
               receiver_id + 1, current_address, msg_size);
        // 反序列化消息
        message_t received_msg;
        message_null(received_msg);
        unsigned msg_type_length;
        memcpy(&msg_type_length, msg_data, sizeof(unsigned));
        unsigned msg_data_length;
        memcpy(&msg_data_length, msg_data + sizeof(unsigned) + msg_type_length, sizeof(unsigned));
        deserialize_message(&received_msg, msg_data);
        printf("Receiver %d: Message type: %s\n", receiver_id + 1, received_msg->type);
        
        // 验证反序列化后的数据长度
        if (received_msg->data == NULL) {
            printf("[RECEIVER %d] WARNING: received_msg->data is NULL after deserialize\n", receiver_id + 1);
        }
        
        if (strcmp(received_msg->type, "AUDIT_REQUEST") == 0) {
            // 解析请求数据：格式为 [tag(1字节)] [msg_id(字符串)] ['\0'] [pairs_summary_json(字符串)] ['\0']
            int tag = 0;
            char msg_id_buf[MSG_ID_MAXLEN] = {0};
            char pairs_summary_json[2048] = {0};
            
            // 添加调试信息：打印原始数据
            printf("[RECEIVER %d] DEBUG: msg_data_length from message header=%u\n", receiver_id + 1, msg_data_length);
            printf("[RECEIVER %d] DEBUG: received_msg->data pointer=%p\n", receiver_id + 1, (void*)received_msg->data);
            if (received_msg->data && msg_data_length > 0) {
                printf("[RECEIVER %d] DEBUG: First 100 bytes of received_msg->data: ", receiver_id + 1);
                size_t debug_len = (msg_data_length < 100) ? msg_data_length : 100;
                for (size_t i = 0; i < debug_len; i++) {
                    if (received_msg->data[i] >= 32 && received_msg->data[i] < 127) {
                        printf("%c", received_msg->data[i]);
            } else {
                        printf("\\x%02x", (unsigned char)received_msg->data[i]);
                    }
                }
                printf("\n");
                
                // 也打印原始序列化数据中的data部分，用于对比
                const uint8_t *raw_data = msg_data + (2 * sizeof(unsigned)) + msg_type_length;
                printf("[RECEIVER %d] DEBUG: First 100 bytes of raw serialized data: ", receiver_id + 1);
                for (size_t i = 0; i < debug_len; i++) {
                    if (raw_data[i] >= 32 && raw_data[i] < 127) {
                        printf("%c", raw_data[i]);
                    } else {
                        printf("\\x%02x", (unsigned char)raw_data[i]);
                    }
                }
                printf("\n");
            } else {
                printf("[RECEIVER %d] ERROR: received_msg->data is NULL or msg_data_length is 0\n", receiver_id + 1);
            }
            
            if (received_msg->data && msg_data_length > 1) {
                // 提取tag（第一个字节）
                tag = (int)received_msg->data[0];
                printf("[RECEIVER %d] DEBUG: Extracted tag=%d\n", receiver_id + 1, tag);
                if (tag != 0 && tag != 1) {
                    fprintf(stderr, "[RECEIVER %d] Error: Invalid tag: %d (must be 0 or 1)\n", receiver_id + 1, tag);
                    const char* response = "INVALID_TAG";
                    zmq_send(socket, response, strlen(response), 0);
                    message_free(received_msg);
                    zmq_msg_close(&zmq_message);
                    continue;
                }
                
                // 提取msg_id（从第二个字节开始，直到遇到'\0'）
                size_t offset = 1;
                size_t msg_id_len = 0;
                printf("[RECEIVER %d] DEBUG: Starting to extract msg_id from offset=%zu\n", receiver_id + 1, offset);
                while (offset < msg_data_length && received_msg->data[offset] != '\0') {
                    if (msg_id_len < MSG_ID_MAXLEN - 1) {
                        msg_id_buf[msg_id_len++] = received_msg->data[offset];
                    } else {
                        // msg_id太长，截断
                        fprintf(stderr, "[RECEIVER %d] Warning: msg_id too long, truncating\n", receiver_id + 1);
                        break;
                    }
                    offset++;
                }
                msg_id_buf[msg_id_len] = '\0';
                printf("[RECEIVER %d] DEBUG: Extracted msg_id length=%zu, offset after msg_id=%zu\n", receiver_id + 1, msg_id_len, offset);
                
                // 检查是否成功提取了msg_id
                if (msg_id_len == 0) {
                    fprintf(stderr, "[RECEIVER %d] Error: msg_id is empty\n", receiver_id + 1);
                    const char* response = "INVALID_REQUEST";
                    zmq_send(socket, response, strlen(response), 0);
                    message_free(received_msg);
                    zmq_msg_close(&zmq_message);
                    continue;
                }
                
                // 跳过msg_id的结束符，提取pairs_summary_json（如果存在）
                offset++;  // 跳过msg_id的'\0'
                if (offset < msg_data_length) {
                    size_t json_len = 0;
                    while (offset < msg_data_length && received_msg->data[offset] != '\0' && json_len < sizeof(pairs_summary_json) - 1) {
                        pairs_summary_json[json_len++] = received_msg->data[offset];
                        offset++;
                    }
                    pairs_summary_json[json_len] = '\0';
                    
                    // 检查JSON是否被截断
                    if (offset < msg_data_length && received_msg->data[offset] != '\0' && json_len >= sizeof(pairs_summary_json) - 1) {
                        fprintf(stderr, "[RECEIVER %d] Warning: pairs_summary_json too long, truncating\n", receiver_id + 1);
                    }
                }
                
                printf("[RECEIVER %d] ========== 解析AUDIT_REQUEST ==========\n", receiver_id + 1);
                printf("[RECEIVER %d] tag=%d (0=Alice, 1=Bob)\n", receiver_id + 1, tag);
                printf("[RECEIVER %d] msg_id=%s (length: %zu)\n", receiver_id + 1, msg_id_buf, msg_id_len);
                if (strlen(pairs_summary_json) > 0) {
                    printf("[RECEIVER %d] pairs_summary_json: %s\n", receiver_id + 1, pairs_summary_json);
                } else {
                    printf("[RECEIVER %d] pairs_summary_json: (empty, 使用旧格式)\n", receiver_id + 1);
                }
                printf("[RECEIVER %d] ======================================\n", receiver_id + 1);
            } else {
                fprintf(stderr, "[RECEIVER %d] Error: Invalid data in AUDIT_REQUEST (length: %u, expected > 1)\n", 
                       receiver_id + 1, msg_data_length);
                const char* response = "INVALID_REQUEST";
                zmq_send(socket, response, strlen(response), 0);
                message_free(received_msg);
                zmq_msg_close(&zmq_message);
                continue;
            }
            
            // 添加详细的调试信息
            printf("[RECEIVER %d] DEBUG: About to call handle_audit_request\n", receiver_id + 1);
            printf("[RECEIVER %d] DEBUG: socket=%p, tag=%d, msg_id_buf=%p (\"%s\"), json_filename=%p (\"%s\"), member_index=%d\n", 
                   receiver_id + 1, socket, tag, msg_id_buf, msg_id_buf, json_filename, json_filename ? json_filename : "(NULL)", member_index);
            fflush(stdout);  // 强制刷新输出缓冲区
            
            handle_audit_request(socket, tag, msg_id_buf, pairs_summary_json, json_filename, member_index);  // member_index是1-based的participant_id
            
            printf("[RECEIVER %d] DEBUG: handle_audit_request returned\n", receiver_id + 1);
            fflush(stdout);
            message_free(received_msg);
            zmq_msg_close(&zmq_message);
            continue;
        }
        
        // 处理DKG私钥分片请求
        if (strcmp(received_msg->type, "DKG_SHARE_REQUEST") == 0) {
            printf("[RECEIVER %d] 收到DKG私钥分片请求\n", receiver_id + 1);
            handle_dkg_share_request(socket, receiver_id + 1);
            message_free(received_msg);
            zmq_msg_close(&zmq_message);
            continue;
        }
            // 解析分享（新格式：打包了多个块）
            char msg_id_buf[MSG_ID_MAXLEN];
            strncpy(msg_id_buf, received_msg->type, MSG_ID_MAXLEN - 1);
            msg_id_buf[MSG_ID_MAXLEN - 1] = '\0';
            
            // 解析消息：x | num_blocks | (block_index | block_size | data_length | share_value)...
            if (msg_data_length < sizeof(int) + sizeof(size_t)) {
                fprintf(stderr, "Receiver %d: Message too small\n", receiver_id + 1);
                const char* response = "PARSE_ERROR";
                zmq_send(socket, response, strlen(response), 0);
                message_free(received_msg);
                zmq_msg_close(&zmq_message);
                continue;
            }
            
            size_t offset = 0;
            int participant_id;
            size_t num_blocks;
            memcpy(&participant_id, received_msg->data + offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&num_blocks, received_msg->data + offset, sizeof(size_t));
            offset += sizeof(size_t);
            
            printf("Receiver %d: Received %zu blocks for participant %d (msg_id=%s)\n", 
                   receiver_id + 1, num_blocks, participant_id, msg_id_buf);
            
            int all_saved = 1;
            int saved_count = 0;
            
            // 解析每个块的分享
            for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
                if (offset + sizeof(size_t) + sizeof(size_t) + sizeof(size_t) + 4 > msg_data_length) {
                    fprintf(stderr, "Receiver %d: Insufficient data for block %zu\n", receiver_id + 1, block_idx);
                    all_saved = 0;
                    break;
                }
                
                secret_share_t share;
                memset(&share, 0, sizeof(share));
                share.x = participant_id;
                
                memcpy(&share.block_index, received_msg->data + offset, sizeof(size_t));
                offset += sizeof(size_t);
                memcpy(&share.block_size, received_msg->data + offset, sizeof(size_t));
                offset += sizeof(size_t);
                memcpy(&share.data_length, received_msg->data + offset, sizeof(size_t));
                offset += sizeof(size_t);
                
                // 读取share_value的大小
                size_t share_data_size = received_msg->data[offset] |
                                        (received_msg->data[offset + 1] << 8) |
                                        (received_msg->data[offset + 2] << 16) |
                                        (received_msg->data[offset + 3] << 24);
                offset += 4;
                
                if (offset + share_data_size > msg_data_length) {
                    fprintf(stderr, "Receiver %d: Insufficient data for block %zu share value\n", receiver_id + 1, block_idx);
                    all_saved = 0;
                    break;
                }
                
                if (share_data_size + 4 > SHARE_SIZE) {
                    fprintf(stderr, "Receiver %d: Share value too large for block %zu\n", receiver_id + 1, block_idx);
                    all_saved = 0;
                    break;
                }
                
                memcpy(share.y, received_msg->data + offset - 4, share_data_size + 4);
                offset += share_data_size;
                
                strncpy(share.message_type, msg_id_buf, sizeof(share.message_type) - 1);
                share.message_type[sizeof(share.message_type) - 1] = '\0';
                share.is_valid = 1;
                share.received_time = time(NULL);
                
                printf("Receiver %d: Parsed block %zu (x=%d, block_size=%zu, secret_len=%zu)\n", 
                       receiver_id + 1, share.block_index, share.x, share.block_size, share.data_length);
                
                // 保存到本地JSON文件（使用当前地址）
                int save_result = save_share_to_json_with_address(json_filename, msg_id_buf, &share, current_address);
                if (save_result == 0) {
                    saved_count++;
                } else {
                    fprintf(stderr, "Receiver %d: Failed to save block %zu\n", receiver_id + 1, block_idx);
                    all_saved = 0;
                }
            }
            
            if (all_saved && saved_count == num_blocks) {
                printf("Receiver %d: All %zu shares saved successfully (委员会地址: %s)\n", 
                       receiver_id + 1, saved_count, current_address);
                // 发送确认响应
                const char* response = "ACK";
                zmq_send(socket, response, strlen(response), 0);
            } else {
                fprintf(stderr, "Receiver %d: Failed to save all shares (%d/%zu saved)\n", 
                       receiver_id + 1, saved_count, num_blocks);
                const char* response = "SAVE_ERROR";
                zmq_send(socket, response, strlen(response), 0);
            }
            message_free(received_msg);
        zmq_msg_close(&zmq_message);
    }
    zmq_close(socket);
    zmq_ctx_destroy(context);
    return NULL;
}

int main(int argc, char* argv[]) {
    printf("Starting Secret Share Receiver System with DKG Integration\n");
    printf("Configured for %d shares with threshold %d\n", SECRET_SHARES, THRESHOLD);
    
    // 解析参与者ID（成员编号：1, 2, 3）
    int participant_id = 1;
    if (argc >= 2) {
        participant_id = atoi(argv[1]);
    }
    
    if (participant_id < 1 || participant_id > SECRET_SHARES) {
        printf("[MAIN] Invalid participant ID: %d (must be 1-%d)\n", participant_id, SECRET_SHARES);
        printf("Usage: %s <member_index>\n", argv[0]);
        printf("  member_index: 1, 2, or 3 (对应 committee_members.txt 的行号)\n");
        return 1;
    }
    
    // 从文件读取当前成员的地址
    printf("[MAIN] Reading address for member #%d from %s\n", participant_id, COMMITTEE_MEMBERS_FILE);
    if (read_committee_member_address(participant_id, g_my_address, sizeof(g_my_address)) != 0) {
        fprintf(stderr, "[MAIN] ❌ 无法读取委员会成员地址，退出\n");
        return 1;
    }
    
    printf("[MAIN] ✅ 使用地址: %s (成员 #%d)\n", g_my_address, participant_id);
    g_my_position = participant_id - 1;  // 位置从0开始
    
    // 初始化声誉跟踪系统
    if (reputation_tracker_init() == 0) {
        printf("[MAIN] ✅ 声誉跟踪系统初始化成功\n");
    } else {
        printf("[MAIN] ⚠️  声誉跟踪系统初始化失败（继续运行）\n");
    }
    
    // 可选：初始化委员会集成（如果需要与合约交互）
    // 注意：这里暂时注释掉，因为我们现在主要依赖文件读取
    /*
    if (committee_integration_init("0x8eB48E73Ef9DC08dEc9B67c0729Ce9F532a5e953", "http://localhost:8545") == 0) {
        if (get_committee_state(&g_committee_state) == 0) {
            printf("Committee system initialized successfully\n");
        }
    }
    */
    
    // 初始化密码学库
    printf("[MAIN] Initializing cryptographic libraries...\n");
    
    // ⭐ 首先初始化 PARI（Class Group DKG 需要）
    printf("[MAIN] Initializing PARI for Class Group operations...\n");
    pari_init(500000000, 2);  // ⚠️ 增加到 500MB 栈空间，避免 DKG 计算时栈溢出
    setrand(getwalltime());
    printf("[MAIN] PARI initialized with 500MB stack\n");
    
    if (core_init() != RLC_OK) {
        printf("[MAIN] Core library initialization failed\n");
        pari_close();
        return 1;
    }
    
    if (pc_param_set_any() != RLC_OK) {
        printf("[MAIN] Parameter setting failed\n");
        pari_close();
        core_clean();
        return 1;
    }
    
    // 设置 secp256k1 曲线
    if (ec_param_set_any() != RLC_OK) {
        printf("[MAIN] EC parameter setting failed\n");
        pari_close();
        core_clean();
        return 1;
    }
    ep_param_set(SECG_K256);
    
    pc_core_init();
    printf("[MAIN] Cryptographic libraries initialized successfully\n");
    
    // 检查是否已有DKG密钥文件
    if (dkg_key_files_exist(participant_id)) {
        printf("[MAIN] DKG密钥文件已存在，加载现有密钥...\n");
        if (dkg_load_keys_from_files(participant_id) != RLC_OK) {
            printf("[MAIN] 加载DKG密钥失败\n");
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        printf("[MAIN] DKG密钥加载成功\n");
        
        // 显示DKG状态
        dkg_committee_print_status();
        
        // 启动接收者线程（当前进程只负责一个参与者）
        printf("[MAIN] Starting receiver thread for participant %d...\n", participant_id);
        pthread_t receiver_thread_handle;
        
        // 使用堆分配避免作用域问题（虽然有pthread_join，但这样更安全）
        int *receiver_id_ptr = malloc(sizeof(int));
        if (!receiver_id_ptr) {
            fprintf(stderr, "Error: memory allocation failed\n");
            dkg_committee_cleanup();
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        *receiver_id_ptr = participant_id - 1; // receiver_thread内部会+1，所以这里-1
        
        if (pthread_create(&receiver_thread_handle, NULL, receiver_thread, receiver_id_ptr) != 0) {
            fprintf(stderr, "Error: failed to create receiver thread for participant %d\n", participant_id);
            free(receiver_id_ptr);
            dkg_committee_cleanup();
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        
        printf("[MAIN] Receiver thread started successfully for participant %d\n", participant_id);
        printf("[MAIN] System ready to receive secret shares and DKG share requests...\n");
        printf("[MAIN] Listening on port %d\n", RECEIVER_PORTS[participant_id - 1]);
        
        // 等待线程完成
        pthread_join(receiver_thread_handle, NULL);
        
        // 清理资源
        printf("[MAIN] Cleaning up resources...\n");
        dkg_committee_cleanup();
        pc_core_clean();
        pari_close();
        core_clean();
        
        printf("[MAIN] System shutdown complete\n");
        return 0;
    } else {
        printf("[MAIN] DKG密钥文件不存在，启动DKG统一模式...\n");
        
        // 启动统一模式：按照标准DKG流程执行
        int result = dkg_unified_mode(participant_id);
        
        if (result != 0) {
            printf("[MAIN] DKG统一模式执行失败\n");
            pc_core_clean();
            pari_close();
            core_clean();
            return result;
        }
        
        printf("[MAIN] DKG统一模式执行成功，密钥已生成\n");
        
        // DKG完成后，重新加载密钥并启动接收服务
        printf("[MAIN] 重新加载DKG密钥...\n");
        if (dkg_load_keys_from_files(participant_id) != RLC_OK) {
            printf("[MAIN] 重新加载DKG密钥失败\n");
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        
        // 显示DKG状态
        dkg_committee_print_status();
        
        // 启动接收者线程（当前进程只负责一个参与者）
        printf("[MAIN] Starting receiver thread for participant %d...\n", participant_id);
        pthread_t receiver_thread_handle;
        
        // 使用堆分配避免作用域问题
        int *receiver_id_ptr = malloc(sizeof(int));
        if (!receiver_id_ptr) {
            fprintf(stderr, "Error: memory allocation failed\n");
            dkg_committee_cleanup();
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        *receiver_id_ptr = participant_id - 1; // receiver_thread内部会+1，所以这里-1
        
        if (pthread_create(&receiver_thread_handle, NULL, receiver_thread, receiver_id_ptr) != 0) {
            fprintf(stderr, "Error: failed to create receiver thread for participant %d\n", participant_id);
            free(receiver_id_ptr);
            dkg_committee_cleanup();
            pc_core_clean();
            pari_close();
            core_clean();
            return 1;
        }
        
        printf("[MAIN] Receiver thread started successfully for participant %d\n", participant_id);
        printf("[MAIN] System ready to receive secret shares and DKG share requests...\n");
        printf("[MAIN] Listening on port %d\n", RECEIVER_PORTS[participant_id - 1]);
        
        // 等待线程完成
        pthread_join(receiver_thread_handle, NULL);
        
        // 清理资源
        printf("[MAIN] Cleaning up resources...\n");
        dkg_committee_cleanup();
        pc_core_clean();
        pari_close();
        core_clean();
        
        printf("[MAIN] System shutdown complete\n");
        return 0;
    }
}