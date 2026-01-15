#include "secret_share.h"
#include "zmq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "util.h"
#include "types.h"

// 动态端点配置 - 全局变量定义
char RECEIVER_ENDPOINTS[SECRET_SHARES][64] = {0};

// 获取动态端点 - 默认实现（返回固定地址）
int get_dynamic_endpoints(char endpoints[SECRET_SHARES][64]) {
    // 默认使用固定地址，实际应该从委员会集成获取
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



// ================= GF(256) 基本运算 =================
uint8_t gf256_add(uint8_t a, uint8_t b) {
    return a ^ b;
}

uint8_t gf256_mul(uint8_t a, uint8_t b) {
    static const uint8_t log_table[256] = {
        0, 0, 1, 25, 2, 50, 26, 198, 3, 223, 51, 238, 27, 104, 199, 75,
        4, 100, 224, 14, 52, 141, 239, 129, 28, 193, 105, 248, 200, 8, 76, 113,
        5, 138, 101, 47, 225, 36, 15, 33, 53, 147, 142, 218, 240, 18, 130, 69,
        29, 181, 194, 125, 106, 39, 249, 185, 201, 154, 9, 120, 77, 228, 114, 166,
        6, 191, 139, 98, 102, 221, 48, 253, 226, 152, 37, 179, 16, 145, 34, 136,
        54, 208, 148, 206, 143, 150, 219, 189, 241, 210, 19, 92, 131, 56, 70, 64,
        30, 66, 182, 163, 195, 72, 126, 110, 107, 58, 40, 84, 250, 133, 186, 61,
        202, 94, 155, 159, 10, 21, 121, 43, 78, 212, 229, 172, 115, 243, 167, 87,
        7, 112, 192, 247, 140, 128, 99, 13, 103, 74, 222, 237, 49, 197, 254, 24,
        227, 165, 153, 119, 38, 184, 180, 124, 17, 68, 146, 217, 35, 32, 137, 46,
        55, 63, 209, 91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190, 97,
        242, 86, 211, 171, 20, 42, 93, 158, 132, 60, 57, 83, 71, 109, 65, 162,
        31, 45, 67, 216, 183, 123, 164, 118, 196, 23, 73, 236, 127, 12, 111, 246,
        108, 161, 59, 82, 41, 157, 85, 170, 251, 96, 134, 177, 187, 204, 62, 90,
        203, 89, 95, 176, 156, 169, 160, 81, 11, 245, 22, 235, 122, 117, 44, 215,
        79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168, 80, 88, 175
    };
    static const uint8_t exp_table[256] = {
        1, 2, 4, 8, 16, 32, 64, 128, 29, 58, 116, 232, 205, 135, 19, 38,
        76, 152, 45, 90, 180, 117, 234, 201, 143, 3, 6, 12, 24, 48, 96, 192,
        157, 39, 78, 156, 37, 74, 148, 53, 106, 212, 181, 119, 238, 193, 159, 35,
        70, 140, 5, 10, 20, 40, 80, 160, 93, 186, 105, 210, 185, 111, 222, 161,
        95, 190, 97, 194, 153, 47, 94, 188, 101, 202, 137, 15, 30, 60, 120, 240,
        253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163, 91, 182, 113, 226,
        217, 175, 67, 134, 17, 34, 68, 136, 13, 26, 52, 104, 208, 189, 103, 206,
        129, 31, 62, 124, 248, 237, 199, 147, 59, 118, 236, 197, 151, 51, 102, 204,
        133, 23, 46, 92, 184, 109, 218, 169, 79, 158, 33, 66, 132, 21, 42, 84,
        168, 77, 154, 41, 82, 164, 85, 170, 73, 146, 57, 114, 228, 213, 183, 115,
        230, 209, 191, 99, 198, 145, 63, 126, 252, 229, 215, 179, 123, 246, 241, 255,
        227, 219, 171, 75, 150, 49, 98, 196, 149, 55, 110, 220, 165, 87, 174, 65,
        130, 25, 50, 100, 200, 141, 7, 14, 28, 56, 112, 224, 221, 167, 83, 166,
        81, 162, 89, 178, 121, 242, 249, 239, 195, 155, 43, 86, 172, 69, 138, 9,
        18, 36, 72, 144, 61, 122, 244, 245, 247, 243, 251, 235, 203, 139, 11, 22,
        44, 88, 176, 125, 250, 233, 207, 131, 27, 54, 108, 216, 173, 71, 142, 1
    };
    if (a == 0 || b == 0) return 0;
    return exp_table[(log_table[a] + log_table[b]) % 255];
}

// ================= 拉格朗日插值（椭圆曲线阶版本）=================
// 在椭圆曲线阶上计算拉格朗日插值系数
// L_i(0) = ∏_{j≠i} (0 - x_j) / (x_i - x_j) = ∏_{j≠i} (-x_j) / (x_i - x_j)
int lagrange_coefficient_ec(bn_t result, int xi, int* x_coords, int k, bn_t order) {
    bn_set_dig(result, 1);  // 初始化为 1
    
    bn_t numerator, denominator, temp, inv;
    bn_new(numerator);
    bn_new(denominator);
    bn_new(temp);
    bn_new(inv);
    
    for (int j = 0; j < k; j++) {
        if (x_coords[j] != xi) {
            // numerator = -x_j (mod order)
            bn_set_dig(numerator, x_coords[j]);
            bn_neg(numerator, numerator);
            bn_mod(numerator, numerator, order);
            
            // denominator = x_i - x_j (mod order)
            bn_t x_j_bn;
            bn_new(x_j_bn);
            bn_set_dig(denominator, xi);
            bn_set_dig(x_j_bn, x_coords[j]);
            bn_sub(denominator, denominator, x_j_bn);
            bn_mod(denominator, denominator, order);
            bn_free(x_j_bn);
            
            // 如果 denominator == 0，跳过
            if (bn_is_zero(denominator)) {
                continue;
            }
            
            // 计算逆元：inv = denominator^(-1) (mod order)
            bn_mod_inv(inv, denominator, order);
            
            // temp = numerator * inv (mod order)
            bn_mul(temp, numerator, inv);
            bn_mod(temp, temp, order);
            
            // result = result * temp (mod order)
            bn_mul(result, result, temp);
            bn_mod(result, result, order);
        }
    }
    
    bn_free(numerator);
    bn_free(denominator);
    bn_free(temp);
    bn_free(inv);
    
    return 0;
}

// ================= 拉格朗日插值（GF(256)版本 - 保留用于兼容性）=================
uint8_t lagrange_coefficient(int xi, int* x_coords, int k) {
    uint8_t result = 1;
    for (int j = 0; j < k; j++) {
        if (x_coords[j] != xi) {
            uint8_t numerator = (uint8_t)x_coords[j];
            uint8_t denominator = gf256_add((uint8_t)x_coords[j], (uint8_t)xi);
            if (denominator == 0) continue;
            uint8_t inv = 1;
            for (int inv_i = 1; inv_i < 256; inv_i++) {
                if (gf256_mul(denominator, inv_i) == 1) {
                    inv = inv_i;
                    break;
                }
            }
            result = gf256_mul(result, gf256_mul(numerator, inv));
        }
    }
    return result;
}

// ================= 创建秘密分享（椭圆曲线阶版本 - 分块处理）=================
// 全局变量：存储多项式系数（用于验证）
// 系数在椭圆曲线阶上，支持多块
#define BLOCK_SIZE 30  // 每个块30字节（留2字节余量，确保不超过椭圆曲线阶）

static bn_t saved_coeffs[1000][THRESHOLD];  // 每个块的系数 [块索引][系数索引]
static size_t saved_num_blocks = 0;
static int coeffs_saved = 0;
static int coeffs_initialized = 0;

// 初始化系数数组（椭圆曲线阶版本 - 分块）
static void init_saved_coeffs(size_t num_blocks) {
    if (!coeffs_initialized) {
        for (size_t block_idx = 0; block_idx < 1000 && block_idx < num_blocks; block_idx++) {
            for (int i = 0; i < THRESHOLD; i++) {
                bn_new(saved_coeffs[block_idx][i]);
            }
        }
        coeffs_initialized = 1;
    }
    saved_num_blocks = num_blocks;
    coeffs_saved = 0;
}

// 清理系数数组
static void cleanup_saved_coeffs(void) {
    if (coeffs_initialized) {
        for (size_t block_idx = 0; block_idx < saved_num_blocks; block_idx++) {
            for (int i = 0; i < THRESHOLD; i++) {
                bn_free(saved_coeffs[block_idx][i]);
            }
        }
        coeffs_initialized = 0;
        saved_num_blocks = 0;
    }
}

int create_secret_shares(const uint8_t* secret, size_t secret_len, secret_share_t* shares, size_t* num_shares_out) {
    if (secret_len == 0) {
        fprintf(stderr, "Error: Secret length is zero\n");
        return -1;
    }
    
    // 计算块数量
    size_t num_blocks = (secret_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (num_blocks > 1000) {
        fprintf(stderr, "Error: Too many blocks (%zu > 1000), secret too large\n", num_blocks);
        return -1;
    }
    
    printf("[VSS][EC] Creating secret shares: secret_len=%zu, num_blocks=%zu, block_size=%d\n", 
           secret_len, num_blocks, BLOCK_SIZE);
    
    // 初始化系数数组
    init_saved_coeffs(num_blocks);
    
    // 获取椭圆曲线阶
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 使用基于secret内容的确定性种子，确保相同secret产生相同系数
    unsigned int seed = 0;
    for (size_t i = 0; i < secret_len && i < 4; i++) {
        seed = (seed << 8) | secret[i];
    }
    seed ^= (unsigned int)secret_len;
    srand(seed);
    
    // 为每个块生成分享
    size_t share_idx = 0;
    bn_t x_bn, share_value;
    bn_new(x_bn);
    bn_new(share_value);
    
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        // 计算当前块的大小
        size_t block_size = (block_idx == num_blocks - 1) ? 
                           (secret_len - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
        
        // 读取当前块
        bn_t block_bn;
        bn_new(block_bn);
        bn_read_bin(block_bn, secret + block_idx * BLOCK_SIZE, block_size);
        bn_mod(block_bn, block_bn, order);  // 确保在椭圆曲线阶内
        
        // 生成多项式系数（在椭圆曲线阶上）
        bn_t coeffs[THRESHOLD];
        for (int i = 0; i < THRESHOLD; i++) {
            bn_new(coeffs[i]);
        }
        
        // 常数项 = 块值
        bn_copy(coeffs[0], block_bn);
        
        // 随机系数（在椭圆曲线阶上）
        for (int i = 1; i < THRESHOLD; i++) {
            bn_rand_mod(coeffs[i], order);
        }
        
        // 保存系数（用于验证）
        for (int i = 0; i < THRESHOLD; i++) {
            bn_copy(saved_coeffs[block_idx][i], coeffs[i]);
        }
        
        // 为每个参与者计算分享值
        for (int participant = 0; participant < SECRET_SHARES; participant++) {
            bn_set_dig(x_bn, participant + 1);  // x = participant + 1
            
            // 使用Horner方法计算 f(x) = a₀ + a₁x + ... + a_{t-1}x^{t-1} (mod order)
            bn_zero(share_value);
            
            for (int k = THRESHOLD - 1; k >= 0; k--) {
                // share_value = share_value * x + coeffs[k] (mod order)
                bn_mul(share_value, share_value, x_bn);
                bn_mod(share_value, share_value, order);
                bn_add(share_value, share_value, coeffs[k]);
                bn_mod(share_value, share_value, order);
            }
            
            // 设置分享结构
            shares[share_idx].x = participant + 1;
            shares[share_idx].block_index = block_idx;
            shares[share_idx].data_length = secret_len;
            shares[share_idx].block_size = block_size;
            memset(shares[share_idx].y, 0, SHARE_SIZE);
            
            // 序列化分享值到y数组
            // 格式：[长度(4字节)][大整数字节]
            size_t share_size = bn_size_bin(share_value);
            if (share_size + 4 > SHARE_SIZE) {
                fprintf(stderr, "Error: Share value too large to serialize\n");
                // 清理资源
                for (int i = 0; i < THRESHOLD; i++) {
                    bn_free(coeffs[i]);
                }
                bn_free(block_bn);
                bn_free(x_bn);
                bn_free(share_value);
                bn_free(order);
                return -1;
            }
            
            // 写入长度（4字节，小端序）
            shares[share_idx].y[0] = (share_size) & 0xFF;
            shares[share_idx].y[1] = (share_size >> 8) & 0xFF;
            shares[share_idx].y[2] = (share_size >> 16) & 0xFF;
            shares[share_idx].y[3] = (share_size >> 24) & 0xFF;
            
            // 写入大整数字节
            bn_write_bin(shares[share_idx].y + 4, share_size, share_value);
            
            share_idx++;
        }
        
        // 清理当前块的系数
        for (int i = 0; i < THRESHOLD; i++) {
            bn_free(coeffs[i]);
        }
        bn_free(block_bn);
    }
    
    coeffs_saved = 1;
    
    // 清理资源
    bn_free(x_bn);
    bn_free(share_value);
    bn_free(order);
    
    // 返回实际生成的分享数量
    if (num_shares_out != NULL) {
        *num_shares_out = share_idx;
    }
    
    printf("[VSS][EC] Secret shares created successfully (blocked, on elliptic curve order)\n");
    printf("[VSS][EC] Created %zu shares (%zu blocks × %d participants), secret length: %zu bytes\n", 
           share_idx, num_blocks, SECRET_SHARES, secret_len);
    return 0;
}

// ================= 发送分享 =================
int send_shares_to_receivers(secret_share_t* shares, size_t num_shares, const char* msg_id, const char** receiver_endpoints) {
    printf("[VSS] send_shares_to_receivers called with msg_id: %s, num_shares: %zu\n", msg_id, num_shares);
    
    // 检查参数有效性
    if (shares == NULL) {
        fprintf(stderr, "[VSS] Error: shares is NULL\n");
        return -1;
    }
    if (receiver_endpoints == NULL) {
        fprintf(stderr, "[VSS] Error: receiver_endpoints is NULL\n");
        return -1;
    }
    
    void* context = zmq_ctx_new();
    if (!context) {
        fprintf(stderr, "[VSS] Error: could not create context for secret sharing\n");
        return -1;
    }
    
    // 为每个参与者发送所有块的分享
    int connected_count = 0;
    int total_sent = 0;
    
    for (int participant = 1; participant <= SECRET_SHARES; participant++) {
        // 检查端点
        int endpoint_idx = participant - 1;
        if (receiver_endpoints[endpoint_idx] == NULL) {
            printf("[VSS] Receiver endpoint[%d] is NULL, skipping\n", endpoint_idx);
            continue;
        }
        
        const char* endpoint = receiver_endpoints[endpoint_idx];
        if (endpoint[0] == '\0') {
            printf("[VSS] Receiver endpoint[%d] is empty, skipping\n", endpoint_idx);
            continue;
        }
        
        void* socket = zmq_socket(context, ZMQ_REQ);
        if (!socket) {
            fprintf(stderr, "Error: could not create socket for receiver %d\n", endpoint_idx);
            continue;
        }
        // 增加超时时间：处理531个块可能需要较长时间（保存到JSON文件）
        int timeout = 30000;  // 30秒超时
        zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        zmq_setsockopt(socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
        
        printf("[VSS] Attempting to connect to receiver %d at %s\n", endpoint_idx, endpoint);
        int rc = zmq_connect(socket, endpoint);
        if (rc != 0) {
            fprintf(stderr, "Error: could not connect to receiver %d at %s (rc=%d, errno=%d: %s)\n", 
                    endpoint_idx, endpoint, rc, errno, zmq_strerror(errno));
            zmq_close(socket);
            continue;
        }
        printf("[VSS] Successfully connected to receiver %d\n", endpoint_idx);
        connected_count++;
        
        // 收集该参与者的所有块的分享
        size_t participant_shares[1000];  // 存储该参与者的分享索引
        size_t participant_share_count = 0;
        size_t secret_len = 0;
        
        for (size_t share_idx = 0; share_idx < num_shares; share_idx++) {
            if (shares[share_idx].x == participant) {
                participant_shares[participant_share_count++] = share_idx;
                secret_len = shares[share_idx].data_length;  // 所有块都有相同的 data_length
            }
        }
        
        if (participant_share_count == 0) {
            printf("[VSS] No shares found for participant %d, skipping\n", participant);
            zmq_close(socket);
            continue;
        }
        
        // 计算打包消息的总大小
        size_t total_data_size = sizeof(int) + sizeof(size_t);  // x + num_blocks
        for (size_t i = 0; i < participant_share_count; i++) {
            size_t share_idx = participant_shares[i];
            size_t share_data_size = shares[share_idx].y[0] |
                                   (shares[share_idx].y[1] << 8) |
                                   (shares[share_idx].y[2] << 16) |
                                   (shares[share_idx].y[3] << 24);
            total_data_size += sizeof(size_t) + sizeof(size_t) + sizeof(size_t) + share_data_size + 4;  // block_index + block_size + data_length + share_value
        }
        
        const char* share_msg_type = msg_id;
        const unsigned share_msg_type_length = strlen(share_msg_type) + 1;
        const int total_share_msg_length = share_msg_type_length + total_data_size + (2 * sizeof(unsigned));
        
        message_t share_msg;
        message_null(share_msg);
        message_new(share_msg, share_msg_type_length, total_data_size);
        
        // 序列化：x | num_blocks | (block_index | block_size | data_length | share_value)...
        size_t data_offset = 0;
        memcpy(share_msg->data + data_offset, &participant, sizeof(int));
        data_offset += sizeof(int);
        memcpy(share_msg->data + data_offset, &participant_share_count, sizeof(size_t));
        data_offset += sizeof(size_t);
        
        // 序列化每个块的分享
        for (size_t i = 0; i < participant_share_count; i++) {
            size_t share_idx = participant_shares[i];
            size_t share_data_size = shares[share_idx].y[0] |
                                   (shares[share_idx].y[1] << 8) |
                                   (shares[share_idx].y[2] << 16) |
                                   (shares[share_idx].y[3] << 24);
            
            memcpy(share_msg->data + data_offset, &shares[share_idx].block_index, sizeof(size_t));
            data_offset += sizeof(size_t);
            memcpy(share_msg->data + data_offset, &shares[share_idx].block_size, sizeof(size_t));
            data_offset += sizeof(size_t);
            memcpy(share_msg->data + data_offset, &shares[share_idx].data_length, sizeof(size_t));
            data_offset += sizeof(size_t);
            memcpy(share_msg->data + data_offset, shares[share_idx].y, share_data_size + 4);
            data_offset += share_data_size + 4;
        }
        
        memcpy(share_msg->type, share_msg_type, share_msg_type_length);
        
        uint8_t* serialized_share_message = NULL;
        serialize_message(&serialized_share_message, share_msg, share_msg_type_length, total_data_size);
        zmq_msg_t share_zmq_msg;
        rc = zmq_msg_init_size(&share_zmq_msg, total_share_msg_length);
        if (rc == 0) {
            memcpy(zmq_msg_data(&share_zmq_msg), serialized_share_message, total_share_msg_length);
            rc = zmq_msg_send(&share_zmq_msg, socket, 0);
            if (rc == total_share_msg_length) {
                // REQ-REP 模式：必须等待响应才能发送下一条消息
                printf("[VSS] Waiting for ACK from receiver %d (participant=%d, %zu blocks)...\n", 
                       endpoint_idx, participant, participant_share_count);
                zmq_msg_t reply;
                zmq_msg_init(&reply);
                rc = zmq_msg_recv(&reply, socket, 0);
                if (rc >= 0) {
                    // 收到响应，检查响应内容
                    char* reply_data = (char*)zmq_msg_data(&reply);
                    size_t reply_size = zmq_msg_size(&reply);
                    if (reply_size > 0) {
                        // 确保字符串以null结尾
                        char reply_str[64] = {0};
                        size_t copy_len = (reply_size < sizeof(reply_str) - 1) ? reply_size : sizeof(reply_str) - 1;
                        memcpy(reply_str, reply_data, copy_len);
                        reply_str[copy_len] = '\0';
                        
                        if (strcmp(reply_str, "ACK") == 0) {
                            // 收到确认，发送成功
                            total_sent += participant_share_count;
                            printf("[VSS] Successfully sent %zu shares (participant=%d, blocks=0-%zu) to receiver at %s\n", 
                                   participant_share_count, participant, participant_share_count - 1, endpoint);
                        } else if (strcmp(reply_str, "SAVE_ERROR") == 0) {
                            fprintf(stderr, "[VSS] Error: Receiver %d reported SAVE_ERROR for participant %d\n", 
                                    endpoint_idx, participant);
                        } else {
                            printf("[VSS] Warning: Unexpected reply from receiver %d: %s\n", endpoint_idx, reply_str);
                            // 仍然认为发送成功（可能是旧格式的响应）
                            total_sent += participant_share_count;
                        }
                    } else {
                        printf("[VSS] Warning: Empty reply from receiver %d, assuming success\n", endpoint_idx);
                        total_sent += participant_share_count;
                    }
                    zmq_msg_close(&reply);
                } else {
                    // 接收响应失败（可能是超时）
                    fprintf(stderr, "[VSS] Error: failed to receive reply for participant %d (rc=%d, errno=%d: %s)\n", 
                            participant, rc, errno, zmq_strerror(errno));
                    fprintf(stderr, "[VSS] This may indicate that receiver %d is not running or processing too slowly\n", endpoint_idx);
                    zmq_msg_close(&reply);
                }
            } else {
                fprintf(stderr, "[VSS] Error: failed to send shares for participant %d to receiver at %s (rc=%d, expected=%d, errno=%d: %s)\n", 
                        participant, endpoint, rc, total_share_msg_length, errno, zmq_strerror(errno));
            }
            zmq_msg_close(&share_zmq_msg);
        } else {
            fprintf(stderr, "[VSS] Error: failed to initialize message for participant %d\n", participant);
        }
        if (share_msg != NULL) message_free(share_msg);
        if (serialized_share_message != NULL) free(serialized_share_message);
        
        printf("[VSS] Sent %zu shares to receiver %d\n", participant_share_count, endpoint_idx);
        zmq_close(socket);
    }
    zmq_ctx_destroy(context);
    
    if (connected_count == 0) {
        fprintf(stderr, "[VSS] Warning: No receivers connected successfully\n");
        return -1;
    }
    
    printf("[VSS] Successfully sent %d shares to %d out of %d receivers\n", total_sent, connected_count, SECRET_SHARES);
    return 0;
}

// ================= 解析分享消息 =================
int parse_share_message(uint8_t* data, size_t data_size, secret_share_t* share, char* msg_type) {
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
    share->is_valid = 1;
    share->received_time = time(NULL);
    strncpy(share->message_type, msg_type, sizeof(share->message_type) - 1);
    share->message_type[sizeof(share->message_type) - 1] = '\0';
    char* share_suffix = strstr(share->message_type, "_share_");
    if (share_suffix) {
        *share_suffix = '\0';
    }
    return 0;
}

// ================= receiver专用：收集器 =================
void init_share_collector(share_collector_t* collector) {
    memset(collector, 0, sizeof(share_collector_t));
    collector->share_count = 0;
    collector->threshold = THRESHOLD;
    collector->is_complete = 0;
    pthread_mutex_init(&collector->mutex, NULL);
    for (int i = 0; i < SECRET_SHARES; i++) {
        collector->shares[i].is_valid = 0;
    }
}

int add_share_to_collector(share_collector_t* collector, secret_share_t* share) {
    pthread_mutex_lock(&collector->mutex);
    if (collector->share_count == 0) {
        strncpy(collector->expected_msg_type, share->message_type, sizeof(collector->expected_msg_type) - 1);
        collector->expected_msg_type[sizeof(collector->expected_msg_type) - 1] = '\0';
    } else {
        if (strncmp(collector->expected_msg_type, share->message_type, sizeof(collector->expected_msg_type)) != 0) {
            fprintf(stderr, "Error: message type mismatch. Expected: %s, Got: %s\n", collector->expected_msg_type, share->message_type);
            pthread_mutex_unlock(&collector->mutex);
            return -1;
        }
    }
    for (int i = 0; i < SECRET_SHARES; i++) {
        if (collector->shares[i].is_valid && collector->shares[i].x == share->x) {
            fprintf(stderr, "Warning: duplicate share with x=%d, ignoring\n", share->x);
            pthread_mutex_unlock(&collector->mutex);
            return 0;
        }
    }
    int stored = 0;
    for (int i = 0; i < SECRET_SHARES; i++) {
        if (!collector->shares[i].is_valid) {
            memcpy(&collector->shares[i], share, sizeof(secret_share_t));
            collector->shares[i].is_valid = 1;
            collector->share_count++;
            stored = 1;
            break;
        }
    }
    if (!stored) {
        fprintf(stderr, "Error: no space to store share\n");
        pthread_mutex_unlock(&collector->mutex);
        return -1;
    }
    printf("Added share %d/%d (x=%d) for message type: %s\n", collector->share_count, collector->threshold, share->x, share->message_type);
    if (collector->share_count >= collector->threshold && !collector->is_complete) {
        collector->is_complete = 1;
        printf("Sufficient shares collected! Ready for reconstruction.\n");
    }
    pthread_mutex_unlock(&collector->mutex);
    return 0;
}

int reconstruct_secret(share_collector_t* collector, uint8_t* reconstructed_data, size_t* data_length) {
    if (collector->share_count < collector->threshold) {
        fprintf(stderr, "Error: insufficient shares for reconstruction (%d < %d)\n", collector->share_count, collector->threshold);
        return -1;
    }
    
    // 从第一个分享中读取原始秘密长度和块信息
    size_t secret_len = 0;
    size_t max_block_index = 0;
    for (int i = 0; i < SECRET_SHARES; i++) {
        if (collector->shares[i].is_valid) {
            secret_len = collector->shares[i].data_length;
            if (collector->shares[i].block_index > max_block_index) {
                max_block_index = collector->shares[i].block_index;
            }
        }
    }
    
    if (secret_len == 0) {
        fprintf(stderr, "Error: Could not determine secret length\n");
        return -1;
    }
    
    size_t num_blocks = max_block_index + 1;
    *data_length = secret_len;
    
    printf("[VSS][EC] Reconstructing secret: secret_len=%zu, num_blocks=%zu\n", secret_len, num_blocks);
    
    // 获取椭圆曲线阶
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 按块重构
    size_t reconstructed_offset = 0;
    bn_t lagrange_coeff, contribution;
    bn_new(lagrange_coeff);
    bn_new(contribution);
    
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        // 收集当前块的所有分享（按参与者分组）
        bn_t block_shares[THRESHOLD];
        int x_coords[THRESHOLD];
        int share_count = 0;
        
        // 为每个参与者查找对应块的分享
        for (int participant = 1; participant <= SECRET_SHARES && share_count < THRESHOLD; participant++) {
            for (int i = 0; i < SECRET_SHARES; i++) {
                if (collector->shares[i].is_valid &&
                    collector->shares[i].x == participant &&
                    collector->shares[i].block_index == block_idx) {
                    bn_new(block_shares[share_count]);
                    
                    // 从y数组读取分享值
                    size_t share_size = collector->shares[i].y[0] |
                                       (collector->shares[i].y[1] << 8) |
                                       (collector->shares[i].y[2] << 16) |
                                       (collector->shares[i].y[3] << 24);
                    
                    if (share_size == 0 || share_size > SHARE_SIZE - 4) {
                        fprintf(stderr, "Error: Invalid share size for block %zu: %zu\n", block_idx, share_size);
                        for (int j = 0; j < share_count; j++) {
                            bn_free(block_shares[j]);
                        }
                        bn_free(lagrange_coeff);
                        bn_free(contribution);
                        bn_free(order);
                        return -1;
                    }
                    
                    bn_read_bin(block_shares[share_count], collector->shares[i].y + 4, share_size);
                    x_coords[share_count] = participant;
                    share_count++;
                    break;
                }
            }
        }
        
        if (share_count < THRESHOLD) {
            fprintf(stderr, "Error: Not enough shares for block %zu (%d < %d)\n", 
                    block_idx, share_count, THRESHOLD);
            for (int j = 0; j < share_count; j++) {
                bn_free(block_shares[j]);
            }
            bn_free(lagrange_coeff);
            bn_free(contribution);
            bn_free(order);
            return -1;
        }
        
        // 使用Lagrange插值重构当前块
        bn_t reconstructed_block;
        bn_new(reconstructed_block);
        bn_zero(reconstructed_block);
        
        for (int i = 0; i < THRESHOLD; i++) {
            // 计算Lagrange系数
            lagrange_coefficient_ec(lagrange_coeff, x_coords[i], x_coords, THRESHOLD, order);
            
            // contribution = share_value * lagrange_coeff (mod order)
            bn_mul(contribution, block_shares[i], lagrange_coeff);
            bn_mod(contribution, contribution, order);
            
            // reconstructed_block = reconstructed_block + contribution (mod order)
            bn_add(reconstructed_block, reconstructed_block, contribution);
            bn_mod(reconstructed_block, reconstructed_block, order);
        }
        
        // 将重构的块写入结果
        size_t block_size = (block_idx == num_blocks - 1) ? 
                           (secret_len - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
        size_t reconstructed_block_size = bn_size_bin(reconstructed_block);
        
        if (reconstructed_block_size > block_size) {
            fprintf(stderr, "Error: Reconstructed block %zu too large: %zu > %zu\n", 
                    block_idx, reconstructed_block_size, block_size);
            for (int j = 0; j < THRESHOLD; j++) {
                bn_free(block_shares[j]);
            }
            bn_free(reconstructed_block);
            bn_free(lagrange_coeff);
            bn_free(contribution);
            bn_free(order);
            return -1;
        }
        
        // 写入重构的块（需要保留前导零）
        // 重要：bn_write_bin 会忽略前导零，所以需要从块末尾开始写入
        memset(reconstructed_data + reconstructed_offset, 0, block_size);
        // 从块末尾开始写入，保留前导零
        if (reconstructed_block_size > 0) {
            bn_write_bin(reconstructed_data + reconstructed_offset + block_size - reconstructed_block_size, 
                        reconstructed_block_size, reconstructed_block);
        }
        reconstructed_offset += block_size;
        
        // 清理当前块的资源
        for (int j = 0; j < THRESHOLD; j++) {
            bn_free(block_shares[j]);
        }
        bn_free(reconstructed_block);
    }
    
    // 清理资源
    bn_free(lagrange_coeff);
    bn_free(contribution);
    bn_free(order);
    
    printf("Successfully reconstructed secret message of %zu bytes (blocked, on elliptic curve order)\n", *data_length);
    return 0;
}

// ================= auditor专用：直接用分片数组重构（椭圆曲线阶版本 - 分块）=================
int reconstruct_secret_from_shares(secret_share_t* shares, int share_count, uint8_t* reconstructed_data, size_t* data_length) {
    if (share_count < THRESHOLD) {
        fprintf(stderr, "Error: insufficient shares for reconstruction\n");
        return -1;
    }
    
    // 从分享中读取原始秘密长度和块信息
    size_t secret_len = 0;
    size_t max_block_index = 0;
    for (int i = 0; i < share_count; i++) {
        if (shares[i].data_length > 0) {
            secret_len = shares[i].data_length;
        }
        if (shares[i].block_index > max_block_index) {
            max_block_index = shares[i].block_index;
        }
    }
    
    if (secret_len == 0) {
        fprintf(stderr, "Error: Could not determine secret length\n");
        return -1;
    }
    
    size_t num_blocks = max_block_index + 1;
    *data_length = secret_len;
    
    printf("[VSS][EC] Reconstructing from shares: secret_len=%zu, num_blocks=%zu, share_count=%d\n", 
           secret_len, num_blocks, share_count);
    
    // 获取椭圆曲线阶
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 按块重构
    size_t reconstructed_offset = 0;
    bn_t lagrange_coeff, contribution;
    bn_new(lagrange_coeff);
    bn_new(contribution);
    
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        // 收集当前块的所有分享
        bn_t block_shares[THRESHOLD];
        int x_coords[THRESHOLD];
        int share_count_for_block = 0;
        
        for (int i = 0; i < share_count && share_count_for_block < THRESHOLD; i++) {
            if (shares[i].block_index == block_idx) {
                bn_new(block_shares[share_count_for_block]);
                
                // 从y数组读取分享值
                size_t share_size = shares[i].y[0] |
                                   (shares[i].y[1] << 8) |
                                   (shares[i].y[2] << 16) |
                                   (shares[i].y[3] << 24);
                
                if (share_size == 0 || share_size > SHARE_SIZE - 4) {
                    fprintf(stderr, "Error: Invalid share size for block %zu: %zu\n", block_idx, share_size);
                    for (int j = 0; j < share_count_for_block; j++) {
                        bn_free(block_shares[j]);
                    }
                    bn_free(lagrange_coeff);
                    bn_free(contribution);
                    bn_free(order);
                    return -1;
                }
                
                bn_read_bin(block_shares[share_count_for_block], shares[i].y + 4, share_size);
                x_coords[share_count_for_block] = shares[i].x;
                share_count_for_block++;
            }
        }
        
        if (share_count_for_block < THRESHOLD) {
            fprintf(stderr, "Error: Not enough shares for block %zu (%d < %d)\n", 
                    block_idx, share_count_for_block, THRESHOLD);
            for (int j = 0; j < share_count_for_block; j++) {
                bn_free(block_shares[j]);
            }
            bn_free(lagrange_coeff);
            bn_free(contribution);
            bn_free(order);
            return -1;
        }
        
        // 使用Lagrange插值重构当前块
        bn_t reconstructed_block;
        bn_new(reconstructed_block);
        bn_zero(reconstructed_block);
        
        for (int i = 0; i < THRESHOLD; i++) {
            // 计算Lagrange系数
            lagrange_coefficient_ec(lagrange_coeff, x_coords[i], x_coords, THRESHOLD, order);
            
            // contribution = share_value * lagrange_coeff (mod order)
            bn_mul(contribution, block_shares[i], lagrange_coeff);
            bn_mod(contribution, contribution, order);
            
            // reconstructed_block = reconstructed_block + contribution (mod order)
            bn_add(reconstructed_block, reconstructed_block, contribution);
            bn_mod(reconstructed_block, reconstructed_block, order);
        }
        
        // 将重构的块写入结果
        size_t block_size = (block_idx == num_blocks - 1) ? 
                           (secret_len - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
        size_t reconstructed_block_size = bn_size_bin(reconstructed_block);
        
        if (reconstructed_block_size > block_size) {
            fprintf(stderr, "Error: Reconstructed block %zu too large: %zu > %zu\n", 
                    block_idx, reconstructed_block_size, block_size);
            for (int j = 0; j < THRESHOLD; j++) {
                bn_free(block_shares[j]);
            }
            bn_free(reconstructed_block);
            bn_free(lagrange_coeff);
            bn_free(contribution);
            bn_free(order);
            return -1;
        }
        
        // 写入重构的块（需要保留前导零）
        // 重要：bn_write_bin 会忽略前导零，所以需要从块末尾开始写入
        memset(reconstructed_data + reconstructed_offset, 0, block_size);
        // 从块末尾开始写入，保留前导零
        if (reconstructed_block_size > 0) {
            bn_write_bin(reconstructed_data + reconstructed_offset + block_size - reconstructed_block_size, 
                        reconstructed_block_size, reconstructed_block);
        }
        reconstructed_offset += block_size;
        
        // 清理当前块的资源
        for (int j = 0; j < THRESHOLD; j++) {
            bn_free(block_shares[j]);
        }
        bn_free(reconstructed_block);
    }
    
    // 清理资源
    bn_free(lagrange_coeff);
    bn_free(contribution);
    bn_free(order);
    
    printf("Successfully reconstructed secret message of %zu bytes (blocked, on elliptic curve order)\n", *data_length);
    return 0;
}

// ================= VSS (Verifiable Secret Sharing) 实现 - Feldman VSS =================

// 创建 VSS 承诺（Feldman VSS）
int create_vss_commitments(const uint8_t* secret, size_t secret_len, 
                          secret_share_t* shares, vss_commitment_t* commitment, const char* msgid) {
    if (secret_len > SHARE_SIZE) {
        fprintf(stderr, "Error: Secret too large for VSS\n");
        return -1;
    }
    
    // 计算块数量
    size_t num_blocks = (secret_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (num_blocks > MAX_BLOCKS) {
        fprintf(stderr, "[VSS][Feldman] Error: Too many blocks (%zu > %d)\n", num_blocks, MAX_BLOCKS);
        return -1;
    }
    
    // 初始化承诺结构
    memset(commitment, 0, sizeof(vss_commitment_t));
    commitment->secret_len = secret_len;
    commitment->num_blocks = num_blocks;
    commitment->timestamp = time(NULL);
    
    // 设置 msgid
    if (msgid != NULL && strlen(msgid) > 0) {
        strncpy(commitment->msgid, msgid, MSG_ID_MAXLEN - 1);
        commitment->msgid[MSG_ID_MAXLEN - 1] = '\0';
    } else {
        printf("[VSS] WARNING: msgid is NULL or empty, using timestamp as msgid\n");
        snprintf(commitment->msgid, MSG_ID_MAXLEN, "timestamp_%ld", commitment->timestamp);
    }
    printf("[VSS][Feldman] Creating Feldman VSS commitments with msgid: '%s' (num_blocks=%zu)\n", 
           commitment->msgid, num_blocks);
    
    // 获取椭圆曲线生成元和群阶
    ec_t g;
    bn_t order;
    ec_new(g);
    bn_new(order);
    ec_curve_get_gen(g);
    ec_curve_get_ord(order);
    
    printf("[VSS][Feldman] Using elliptic curve generator g for Feldman commitments\n");
    
    ec_t temp_point;
    ec_new(temp_point);
    bn_t coeff_bn;
    bn_new(coeff_bn);
    
    // 使用保存的椭圆曲线阶系数（来自create_secret_shares）
    if (!coeffs_saved) {
        fprintf(stderr, "[VSS][Feldman] Error: Coefficients not saved. Call create_secret_shares first.\n");
        ec_free(temp_point);
        bn_free(coeff_bn);
        ec_free(g);
        bn_free(order);
        return -1;
    }
    
    // 为每个块创建 Feldman 承诺
    int zero_coeff_count = 0;
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        // 为每个系数创建 Feldman 承诺：C_j = g^(a_j)
        for (int i = 0; i < THRESHOLD; i++) {
            // 系数已经是椭圆曲线阶上的大整数
            bn_copy(coeff_bn, saved_coeffs[block_idx][i]);
            bn_mod(coeff_bn, coeff_bn, order);
            
            // 检查系数是否为零
            if (bn_is_zero(coeff_bn)) {
                if (i == 0) {
                    // coefficient 0 为零是可能的（如果块数据是零）
                    // 分析该块对应的原始数据位置
                    size_t block_start_offset = block_idx * BLOCK_SIZE;
                    size_t block_end_offset = block_start_offset + BLOCK_SIZE;
                    if (block_end_offset > secret_len) {
                        block_end_offset = secret_len;
                    }
                    
                    // 检查原始数据是否真的全为零
                    int all_bytes_zero = 1;
                    for (size_t j = block_start_offset; j < block_end_offset && j < secret_len; j++) {
                        if (secret[j] != 0) {
                            all_bytes_zero = 0;
                            break;
                        }
                    }
                    
                    // 计算该块在打包数据中的位置范围
                    // 限制输出频率，避免日志过多
                    if (zero_coeff_count < 20 || (block_idx % 10 == 0)) {
                        printf("[VSS][Feldman] Warning: Block %zu, coefficient 0 is zero (block data may be zero)\n", block_idx);
                        printf("[VSS][Feldman]   Block %zu corresponds to original data offset: %zu-%zu bytes (out of %zu total)\n", 
                               block_idx, block_start_offset, block_end_offset - 1, secret_len);
                        
                        // 检查原始数据是否真的全为零
                        if (all_bytes_zero) {
                            printf("[VSS][Feldman]   [DEBUG] All bytes in this block are ZERO in original data\n");
                        } else {
                            printf("[VSS][Feldman]   [DEBUG] Some bytes in this block are NON-ZERO in original data (checking if this is padding...)\n");
                            // 显示前几个非零字节的位置
                            int non_zero_found = 0;
                            for (size_t j = block_start_offset; j < block_end_offset && j < secret_len && !non_zero_found; j++) {
                                if (secret[j] != 0) {
                                    printf("[VSS][Feldman]   [DEBUG] First non-zero byte at offset %zu: 0x%02x\n", j, secret[j]);
                                    non_zero_found = 1;
                                }
                            }
                        }
                        
                        // 分析该位置应该是什么内容（基于Tumbler打包结构）
                        // 典型结构：
                        // 0-32: g^abt (33 bytes)
                        // 33-?: ctx_alpha_beta_tau (variable, with length prefix)
                        // ?-?: auditor (+tau) (variable, with length prefix)
                        // ?-?: Alice presig (fixed)
                        // ?-?: Real sig (fixed)
                        // ?-?: escrow_id (variable)
                        // ?-?: escrow_tx_hash (variable)
                        // ~6000: g^(α+β) (33 bytes)
                        // ~6033-9032: ctx_α+β (3000 bytes, padded with zeros)
                        // ~9033-12032: auditor_ctx_α+β (3000 bytes, padded with zeros)
                        // ~12033+: Alice ZK proof (variable)
                        
                        if (block_start_offset >= 6033 && block_start_offset < 9033) {
                            size_t offset_in_ctx = block_start_offset - 6033;
                            printf("[VSS][Feldman]   This position corresponds to: ctx_α+β area (offset %zu within ctx_α+β, range 0-2999)\n", offset_in_ctx);
                            if (offset_in_ctx >= 1500) {
                                printf("[VSS][Feldman]   [DEBUG] This is LIKELY PADDING area (ctx_α+β data is only ~1500 bytes, rest is zero-padded to 3000 bytes)\n");
                                printf("[VSS][Feldman]   [DEBUG] Conclusion: Zero bytes are due to SIZE ALLOCATION (RLC_CL_CIPHERTEXT_SIZE=1500, padded to 3000), not actual zero data\n");
                            } else {
                                printf("[VSS][Feldman]   [DEBUG] This is DATA area (should contain ctx_α+β ciphertext data)\n");
                                if (all_bytes_zero) {
                                    printf("[VSS][Feldman]   [DEBUG] WARNING: Data area contains all zeros - this may indicate missing or empty ciphertext data!\n");
                                }
                            }
                        } else if (block_start_offset >= 9033 && block_start_offset < 12033) {
                            size_t offset_in_auditor = block_start_offset - 9033;
                            printf("[VSS][Feldman]   This position corresponds to: auditor_ctx_α+β area (offset %zu within auditor_ctx_α+β, range 0-2999)\n", offset_in_auditor);
                            if (offset_in_auditor >= 1500) {
                                printf("[VSS][Feldman]   [DEBUG] This is LIKELY PADDING area (auditor_ctx_α+β data is only ~1500 bytes, rest is zero-padded to 3000 bytes)\n");
                                printf("[VSS][Feldman]   [DEBUG] Conclusion: Zero bytes are due to SIZE ALLOCATION (RLC_CL_CIPHERTEXT_SIZE=1500, padded to 3000), not actual zero data\n");
                            } else {
                                printf("[VSS][Feldman]   [DEBUG] This is DATA area (should contain auditor_ctx_α+β ciphertext data)\n");
                                if (all_bytes_zero) {
                                    printf("[VSS][Feldman]   [DEBUG] WARNING: Data area contains all zeros - this may indicate missing or empty ciphertext data!\n");
                                }
                            }
                        } else if (block_start_offset < 6000) {
                            printf("[VSS][Feldman]   This position corresponds to: early data (g^abt, ctx_alpha_beta_tau, auditor, presig, sig, escrow_id, escrow_tx_hash)\n");
                            if (all_bytes_zero) {
                                printf("[VSS][Feldman]   [DEBUG] WARNING: Early data area contains all zeros - this may indicate missing data or padding!\n");
                            }
                        } else {
                            printf("[VSS][Feldman]   This position corresponds to: later data (likely Alice ZK proof or other variable-length fields)\n");
                            if (all_bytes_zero) {
                                printf("[VSS][Feldman]   [DEBUG] WARNING: Later data area contains all zeros - this may indicate missing data or padding!\n");
                            }
                        }
                    }
                } else {
                    // coefficient 1+ 为零是不正常的（随机系数不应该为零）
                    fprintf(stderr, "[VSS][Feldman] ERROR: Block %zu, coefficient %d is zero! Random coefficient should never be zero!\n",
                            block_idx, i);
                }
                zero_coeff_count++;
            }
            
            // 计算 Feldman 承诺：C_i = g^(coeff_i)
            // 注意：如果 coeff_bn 是零，g^0 = O（无穷远点）
            ec_mul_gen(temp_point, coeff_bn);
            
            // 压缩并存储椭圆曲线点
            uint8_t compressed[RLC_EC_SIZE_COMPRESSED];
            ec_write_bin(compressed, RLC_EC_SIZE_COMPRESSED, temp_point, 1);  // 1 = 压缩格式
            
            // 检查压缩点是否全零（这不应该发生，除非是特殊的无穷远点表示）
            int compressed_is_zero = 1;
            for (int j = 0; j < RLC_EC_SIZE_COMPRESSED; j++) {
                if (compressed[j] != 0) {
                    compressed_is_zero = 0;
                    break;
                }
            }
            
            // 如果系数是零，结果是无穷远点，但压缩表示可能不是全零
            // 如果压缩表示是全零，这可能表示无穷远点的特殊编码，或者是一个错误
            if (compressed_is_zero) {
                if (bn_is_zero(coeff_bn)) {
                    // 系数是零，压缩点也是全零，这可能是无穷远点的编码
                    // 但为了安全，我们使用一个标准的无穷远点表示
                    // 实际上，RELIC 的 ec_write_bin 应该正确处理无穷远点
                    if (i == 0) {
                        printf("[VSS][Feldman] Warning: Block %zu, coefficient 0 is zero, compressed point is all zeros (may be point at infinity encoding)\n", block_idx);
                    } else {
                        fprintf(stderr, "[VSS][Feldman] ERROR: Block %zu, coefficient %d is zero, compressed point is all zeros (unexpected for random coefficient!)\n",
                                block_idx, i);
                    }
                } else {
                    // 系数不是零，但压缩点是全零，这是错误
                    fprintf(stderr, "[VSS][Feldman] ERROR: Block %zu, coefficient %d is non-zero but produced all-zero compressed point (unexpected!)\n",
                            block_idx, i);
                }
            }
            
            memcpy(commitment->commitments[block_idx][i], compressed, RLC_EC_SIZE_COMPRESSED);
        }
        
        if (block_idx == 0 || block_idx == num_blocks - 1) {
            printf("[VSS][Feldman] Created commitments for block %zu (secret length: %zu)\n", 
                   block_idx, secret_len);
        }
    }
    
    ec_free(temp_point);
    bn_free(coeff_bn);
    ec_free(g);
    bn_free(order);
    
    printf("[VSS][Feldman] Created Feldman commitments for %zu blocks (secret length: %zu)\n", 
           num_blocks, secret_len);
    
    return 0;
}

// 验证分片与承诺的一致性（Feldman VSS验证）
int verify_share_with_commitment(const secret_share_t* share, 
                                const vss_commitment_t* commitment) {
    if (share->data_length != commitment->secret_len) {
        fprintf(stderr, "[VSS][Feldman] Error: Share length mismatch with commitment\n");
        return -1;
    }
    
    printf("[VSS][Feldman] Verifying share with x=%d, block_index=%zu using Feldman VSS\n", 
           share->x, share->block_index);
    
    // 检查块索引是否有效
    if (share->block_index >= commitment->num_blocks) {
        fprintf(stderr, "[VSS][Feldman] Error: Invalid block index %zu (max: %zu)\n", 
                share->block_index, commitment->num_blocks - 1);
        return -1;
    }
    
    // 获取椭圆曲线生成元和群阶
    ec_t g;
    bn_t order;
    ec_new(g);
    bn_new(order);
    ec_curve_get_gen(g);
    ec_curve_get_ord(order);
    
    // 从y数组读取分享值（bn_t的序列化形式）
    // 格式：[长度(4字节)][大整数字节]
    size_t share_size = share->y[0] |
                       (share->y[1] << 8) |
                       (share->y[2] << 16) |
                       (share->y[3] << 24);
    
    if (share_size == 0 || share_size > SHARE_SIZE - 4) {
        fprintf(stderr, "[VSS][Feldman] Error: Invalid share size: %zu\n", share_size);
        ec_free(g);
        bn_free(order);
        return -1;
    }
    
    bn_t share_value_bn;
    bn_new(share_value_bn);
    bn_read_bin(share_value_bn, share->y + 4, share_size);
    bn_mod(share_value_bn, share_value_bn, order);
    
    char share_str[256];
    bn_write_str(share_str, sizeof(share_str), share_value_bn, 10);
    printf("[VSS][Feldman] Verifying share value (x=%d, block=%zu): share_value=%s\n", 
           share->x, share->block_index, share_str);
    
    // 步骤1：验证当前块的所有承诺都是有效的椭圆曲线点
    uint8_t compressed[RLC_EC_SIZE_COMPRESSED];
    ec_t commitments[THRESHOLD];
    int all_valid = 1;
    
    printf("[VSS][Feldman] Step 1: Verifying all commitments for block %zu are valid EC points...\n", 
           share->block_index);
    for (int i = 0; i < THRESHOLD; i++) {
        ec_new(commitments[i]);
        memcpy(compressed, commitment->commitments[share->block_index][i], RLC_EC_SIZE_COMPRESSED);
        
        // 检查承诺数据是否全为零（未初始化）
        int is_all_zero = 1;
        for (int j = 0; j < RLC_EC_SIZE_COMPRESSED; j++) {
            if (compressed[j] != 0) {
                is_all_zero = 0;
                break;
            }
        }
        
        // 处理全零承诺（可能是无穷远点的编码，或者未初始化）
        if (is_all_zero) {
            if (i == 0) {
                // coefficient 0 的全零可能是正常的（块数据为零，g^0 = O）
                // 将全零解释为无穷远点
                ec_set_infty(commitments[i]);
                printf("[VSS][Feldman] ⚠️  Commitment C[0] for block %zu is all zeros (interpreting as point at infinity - block data may be zero)\n", 
                       share->block_index);
                // 不标记为无效，继续验证
            } else {
                // coefficient 1+ 的全零是不正常的
                fprintf(stderr, "[VSS][Feldman] ❌ Commitment C[%d] for block %zu is all zeros (uninitialized or invalid)\n", 
                        i, share->block_index);
                all_valid = 0;
                ec_free(commitments[i]);
                continue;
            }
        } else {
            // 使用 RLC_TRY/RLC_CATCH 捕获 ec_read_bin 可能抛出的错误
            RLC_TRY {
                ec_read_bin(commitments[i], compressed, RLC_EC_SIZE_COMPRESSED);
                
                // 验证点不是无穷远点
                // 注意：对于 coefficient 0，如果块数据是零，g^0 = O（无穷远点）是有效的
                // 但对于 coefficient 1+（随机系数），无穷远点是不正常的
                if (ec_is_infty(commitments[i]) == 1) {
                    if (i == 0) {
                        // coefficient 0 的无穷远点是允许的（块数据可能为零）
                        printf("[VSS][Feldman] ⚠️  Commitment C[0] for block %zu is point at infinity (block data may be zero, this is acceptable)\n", 
                               share->block_index);
                        // 不标记为无效，继续验证
                    } else {
                        // coefficient 1+ 的无穷远点是不正常的（随机系数不应该为零）
                        fprintf(stderr, "[VSS][Feldman] ❌ Commitment C[%d] for block %zu is the point at infinity (invalid - random coefficient should not be zero)\n", 
                                i, share->block_index);
                        all_valid = 0;
                    }
                } else {
                    printf("[VSS][Feldman] ✅ Commitment C[%d] for block %zu is valid EC point\n", i, share->block_index);
                }
            } RLC_CATCH_ANY {
                fprintf(stderr, "[VSS][Feldman] ❌ Commitment C[%d] for block %zu failed to read (invalid EC point data)\n", 
                        i, share->block_index);
                fprintf(stderr, "[VSS][Feldman] First 16 bytes of commitment: ");
                for (int j = 0; j < 16 && j < RLC_EC_SIZE_COMPRESSED; j++) {
                    fprintf(stderr, "%02x", compressed[j]);
                }
                fprintf(stderr, "\n");
                all_valid = 0;
                // 注意：在 RLC_CATCH 中，ec_new 已经分配了内存，但 ec_read_bin 失败了
                // 我们需要释放这个点，但可能需要先设置为无穷远点
                ec_set_infty(commitments[i]);
            }
        }
    }
    
    if (!all_valid) {
        fprintf(stderr, "[VSS][Feldman] ❌ Some commitments are invalid\n");
        // 清理资源
        for (int i = 0; i < THRESHOLD; i++) {
            ec_free(commitments[i]);
        }
        ec_free(g);
        bn_free(order);
        return -1;
    }
    
    // 步骤2：计算组合承诺 C_share = ∏ C_i^(x^i)
    // 在椭圆曲线群（加法群）中，公式转换为：C_share = C_0 + x*C_1 + x²*C_2 + ... + x^{t-1}*C_{t-1}
    // 其中 + 是椭圆曲线点加，x*C_i 是标量乘法
    printf("[VSS][Feldman] Step 2: Computing combined commitment C_share = C_0 + x*C_1 + x²*C_2 + ...\n");
    ec_t C_share;
    ec_new(C_share);
    ec_set_infty(C_share);  // 初始化为无穷远点（加法单位元）
    
    bn_t x_bn, x_power;
    bn_new(x_bn);
    bn_new(x_power);
    bn_set_dig(x_bn, share->x);
    bn_set_dig(x_power, 1);  // x^0 = 1
    
    for (int i = 0; i < THRESHOLD; i++) {
        // 计算 x^i * C_i（椭圆曲线标量乘法）
        ec_t C_i_scaled;
        ec_new(C_i_scaled);
        ec_mul(C_i_scaled, commitments[i], x_power);
        
        // 累加：C_share = C_share + x^i * C_i（椭圆曲线点加）
        ec_add(C_share, C_share, C_i_scaled);
        ec_norm(C_share, C_share);
        
        // 更新 x_power = x_power * x（用于下一次迭代：x^0, x^1, x^2, ...）
        if (i < THRESHOLD - 1) {
            bn_mul(x_power, x_power, x_bn);
            bn_mod(x_power, x_power, order);
        }
        
        ec_free(C_i_scaled);
    }
    
    printf("[VSS][Feldman] Combined commitment C_share computed\n");
    
    // 步骤3：验证 C_share 是否是有效的椭圆曲线点
    printf("[VSS][Feldman] Step 3: Verifying combined commitment C_share is valid...\n");
    if (ec_is_infty(C_share) == 1) {
        fprintf(stderr, "[VSS][Feldman] ❌ Combined commitment C_share is the point at infinity (invalid)\n");
        // 清理资源
        for (int i = 0; i < THRESHOLD; i++) {
            ec_free(commitments[i]);
        }
        ec_free(C_share);
        ec_free(g);
        bn_free(order);
        return -1;
    }
    printf("[VSS][Feldman] ✅ Combined commitment C_share is a valid EC point\n");
    
    // 步骤4：验证分享值是否与承诺一致（椭圆曲线阶版本）
    printf("[VSS][Feldman] Step 4: Verifying share value matches commitment (on elliptic curve order)...\n");
    
    // share_value_bn 已经在椭圆曲线阶上（从上面读取）
    ec_t C_share_value;
    ec_new(C_share_value);
    
    // 计算 C_share_value = g^share_value
    ec_mul_gen(C_share_value, share_value_bn);
    
    // 比较 C_share 和 C_share_value
    int cmp_result = ec_cmp(C_share, C_share_value);
    
    if (cmp_result == RLC_EQ) {
        printf("[VSS][Feldman] ✅ Share value matches commitment\n");
    } else {
        printf("[VSS][Feldman] ❌ Share value does not match commitment\n");
    }
    
    // 清理资源
    bn_free(share_value_bn);
    ec_free(C_share_value);
    
    for (int i = 0; i < THRESHOLD; i++) {
        ec_free(commitments[i]);
    }
    ec_free(C_share);
    bn_free(x_bn);
    bn_free(x_power);
    ec_free(g);
    bn_free(order);
    
    // 返回验证结果
    if (cmp_result == RLC_EQ) {
        printf("[VSS][Feldman] ✅ Share verification successful for x=%d\n", share->x);
        printf("[VSS][Feldman] ✅ C_share == C_share_value, share value is correct\n");
        return 0;
    } else {
        fprintf(stderr, "[VSS][Feldman] ❌ Share verification failed for x=%d\n", share->x);
        fprintf(stderr, "[VSS][Feldman] ❌ C_share != C_share_value, share value is incorrect\n");
        return -1;
    }
}

// 发送 VSS 承诺给 Auditor
int send_vss_commitment_to_auditor(const vss_commitment_t* commitment) {
    // 创建 ZMQ 上下文和套接字
    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_REQ);
    
    if (zmq_connect(socket, "tcp://localhost:5558") != 0) {
        fprintf(stderr, "Error: Failed to connect to auditor\n");
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return -1;
    }
    
    // 序列化承诺
    size_t msg_size = sizeof(vss_commitment_t);
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, msg_size);
    memcpy(zmq_msg_data(&msg), commitment, msg_size);
    
    // 发送承诺
    if (zmq_msg_send(&msg, socket, 0) == -1) {
        fprintf(stderr, "Error: Failed to send VSS commitment\n");
        zmq_msg_close(&msg);
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return -1;
    }
    
    // 等待确认
    zmq_msg_t reply;
    zmq_msg_init(&reply);
    if (zmq_msg_recv(&reply, socket, 0) == -1) {
        fprintf(stderr, "Error: Failed to receive confirmation\n");
        zmq_msg_close(&reply);
        zmq_msg_close(&msg);
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return -1;
    }
    
    printf("[VSS] Successfully sent commitment to auditor\n");
    
    // 清理
    zmq_msg_close(&reply);
    zmq_msg_close(&msg);
    zmq_close(socket);
    zmq_ctx_destroy(context);
    
    return 0;
}

// ================= 基于文件的 VSS 承诺存储 =================

// 将 VSS 承诺保存到文件
int save_vss_commitment_to_file(const vss_commitment_t* commitment) {
    if (commitment == NULL) {
        printf("[VSS][File] Error: NULL commitment\n");
        return -1;
    }
    
    // 创建文件名：vss_commitment_<msgid>.json
    char filename[256];
    if (strlen(commitment->msgid) == 0) {
        printf("[VSS][File] WARNING: msgid is empty, using timestamp as filename\n");
        snprintf(filename, sizeof(filename), "vss_commitments/vss_commitment_empty_%ld.json", commitment->timestamp);
    } else {
        snprintf(filename, sizeof(filename), "vss_commitments/vss_commitment_%s.json", commitment->msgid);
    }
    
    // 确保目录存在
    (void)system("mkdir -p vss_commitments");
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("[VSS][File] Error: Cannot create file %s\n", filename);
        return -1;
    }
    
    // 写入 JSON 格式的承诺数据（Feldman VSS - 分块版本）
    fprintf(file, "{\n");
    fprintf(file, "  \"msgid\": \"%s\",\n", commitment->msgid);
    fprintf(file, "  \"secret_len\": %zu,\n", commitment->secret_len);
    fprintf(file, "  \"num_blocks\": %zu,\n", commitment->num_blocks);
    fprintf(file, "  \"timestamp\": %ld,\n", commitment->timestamp);
    fprintf(file, "  \"commitment_type\": \"Feldman_VSS_Blocked\",\n");
    fprintf(file, "  \"commitments\": [\n");
    
    // 为每个块写入承诺
    for (size_t block_idx = 0; block_idx < commitment->num_blocks; block_idx++) {
        fprintf(file, "    [\n");  // 块的开始
        for (int i = 0; i < THRESHOLD; i++) {
            fprintf(file, "      \"");
            // 椭圆曲线压缩点格式：RLC_EC_SIZE_COMPRESSED 字节
            for (int j = 0; j < RLC_EC_SIZE_COMPRESSED; j++) {
                fprintf(file, "%02x", commitment->commitments[block_idx][i][j]);
            }
            fprintf(file, "\"");
            if (i < THRESHOLD - 1) fprintf(file, ",");
            fprintf(file, "\n");
        }
        fprintf(file, "    ]");
        if (block_idx < commitment->num_blocks - 1) fprintf(file, ",");
        fprintf(file, "\n");
    }
    
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    
    fclose(file);
    printf("[VSS][File] Successfully saved commitment to %s\n", filename);
    return 0;
}

// 从文件加载 VSS 承诺
int load_vss_commitment_from_file(const char* msgid, vss_commitment_t* commitment) {
    if (msgid == NULL || commitment == NULL) {
        printf("[VSS][File] Error: NULL parameters\n");
        return -1;
    }
    
    // 创建文件名
    char filename[256];
    snprintf(filename, sizeof(filename), "vss_commitments/vss_commitment_%s.json", msgid);
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("[VSS][File] Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    // 初始化承诺结构
    memset(commitment, 0, sizeof(vss_commitment_t));
    
    // 简单的 JSON 解析（实际应用中应使用专门的 JSON 库）
    char line[1024];
    size_t current_block = 0;
    int current_coeff = 0;
    int in_commitments_array = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // 解析 msgid
        if (strstr(line, "\"msgid\"")) {
            char* start = strchr(line, '"');
            if (start) {
                start++; // 跳过第一个引号
                char* end = strchr(start, '"');
                if (end) {
                    size_t len = end - start;
                    if (len < MSG_ID_MAXLEN) {
                        strncpy(commitment->msgid, start, len);
                        commitment->msgid[len] = '\0';
                    }
                }
            }
        }
        // 解析 secret_len
        else if (strstr(line, "\"secret_len\"")) {
            sscanf(line, "  \"secret_len\": %zu,", &commitment->secret_len);
        }
        // 解析 num_blocks
        else if (strstr(line, "\"num_blocks\"")) {
            sscanf(line, "  \"num_blocks\": %zu,", &commitment->num_blocks);
        }
        // 解析 timestamp
        else if (strstr(line, "\"timestamp\"")) {
            sscanf(line, "  \"timestamp\": %ld,", &commitment->timestamp);
        }
        // 解析 commitments 数组开始
        else if (strstr(line, "\"commitments\"")) {
            in_commitments_array = 1;
            continue;
        }
        // 解析块开始标记 "["
        else if (in_commitments_array && (strstr(line, "    [") || strstr(line, "  ["))) {
            // 新块开始，重置当前块的系数索引
            if (current_coeff > 0) {
                printf("[VSS][File] Warning: Block %zu started but previous block had %d/%d commitments\n", 
                       current_block, current_coeff, THRESHOLD);
            }
            current_coeff = 0;
        }
        // 解析块结束标记 "]"
        else if (in_commitments_array && (strstr(line, "    ]") || strstr(line, "  ]"))) {
            // 块结束，检查是否解析了足够的系数
            if (current_coeff != THRESHOLD) {
                printf("[VSS][File] Warning: Block %zu ended with %d/%d commitments parsed\n", 
                       current_block, current_coeff, THRESHOLD);
            }
            current_block++;  // 移动到下一个块
            current_coeff = 0;
        }
        // 解析承诺值（十六进制字符串）
        // 匹配格式：包含引号和十六进制字符串的行（更宽松的匹配）
        else if (in_commitments_array && strchr(line, '"')) {
            char* start = strchr(line, '"');
            if (start) {
                start++; // 跳过引号
                char* end = strchr(start, '"');
                if (end) {
                    size_t hex_len = end - start;
                    // Feldman VSS: 椭圆曲线压缩点格式，RLC_EC_SIZE_COMPRESSED 字节 = 33字节 = 66十六进制字符
                    size_t expected_hex_len = RLC_EC_SIZE_COMPRESSED * 2;
                    
                    // 检查是否是有效的十六进制字符串（长度匹配且只包含十六进制字符）
                    if (hex_len == expected_hex_len) {
                        // 验证是否为有效的十六进制字符串
                        int is_valid_hex = 1;
                        for (size_t k = 0; k < hex_len; k++) {
                            char c = start[k];
                            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                                is_valid_hex = 0;
                                break;
                            }
                        }
                        
                        if (is_valid_hex) {
                            // 新格式：椭圆曲线压缩点（66字符）
                            if (current_block < commitment->num_blocks && current_coeff < THRESHOLD) {
                                for (int i = 0; i < RLC_EC_SIZE_COMPRESSED; i++) {
                                    char hex_byte[3] = {start[i*2], start[i*2+1], '\0'};
                                    commitment->commitments[current_block][current_coeff][i] = (uint8_t)strtol(hex_byte, NULL, 16);
                                }
                                current_coeff++;
                            } else {
                                printf("[VSS][File] Warning: Skipping commitment (block=%zu >= %zu or coeff=%d >= %d)\n",
                                       current_block, commitment->num_blocks, current_coeff, THRESHOLD);
                            }
                        } else {
                            // 不是有效的十六进制字符串，可能是其他字段（如 msgid），忽略
                        }
                    } else if (hex_len > 0 && hex_len < 100) {
                        // 可能是其他字段（如 msgid），忽略
                    } else {
                        printf("[VSS][File] Warning: Unexpected hex length %zu (expected %zu) at block %zu, coeff %d\n",
                               hex_len, expected_hex_len, current_block, current_coeff);
                    }
                }
            }
        }
    }
    
    fclose(file);
    
    // 验证加载的数据
    if (commitment->num_blocks > 0 && commitment->secret_len > 0) {
        // 检查是否有未初始化的承诺（全零）
        int uninitialized_count = 0;
        int uninitialized_coeff0 = 0;  // coefficient 0 为零的数量
        int uninitialized_coeff1 = 0;  // coefficient 1 为零的数量
        
        for (size_t block_idx = 0; block_idx < commitment->num_blocks; block_idx++) {
            for (int coeff_idx = 0; coeff_idx < THRESHOLD; coeff_idx++) {
                int is_all_zero = 1;
                for (int j = 0; j < RLC_EC_SIZE_COMPRESSED; j++) {
                    if (commitment->commitments[block_idx][coeff_idx][j] != 0) {
                        is_all_zero = 0;
                        break;
                    }
                }
                if (is_all_zero) {
                    uninitialized_count++;
                    if (coeff_idx == 0) {
                        uninitialized_coeff0++;
                    } else {
                        uninitialized_coeff1++;
                    }
                    if (uninitialized_count <= 10) {  // 打印前10个
                        printf("[VSS][File] Warning: Block %zu, coefficient %d is uninitialized (all zeros)\n", 
                               block_idx, coeff_idx);
                    }
                }
            }
        }
        if (uninitialized_count > 0) {
            printf("[VSS][File] Warning: Found %d uninitialized commitments out of %zu total\n", 
                   uninitialized_count, commitment->num_blocks * THRESHOLD);
            printf("[VSS][File] Breakdown: coefficient 0: %d zeros, coefficient 1: %d zeros\n",
                   uninitialized_coeff0, uninitialized_coeff1);
            
            // 如果 coefficient 1（随机系数）有零，这是不正常的
            if (uninitialized_coeff1 > 0) {
                printf("[VSS][File] ERROR: Found %d zero commitments for coefficient 1 (random coefficient should never be zero!)\n",
                       uninitialized_coeff1);
            }
        }
        
        printf("[VSS][File] Successfully loaded commitment for msgid: %s (num_blocks=%zu)\n", 
               msgid, commitment->num_blocks);
        return 0;
    } else {
        printf("[VSS][File] Error: Incomplete commitment data\n");
        return -1;
    }
}

// 列出所有 VSS 承诺文件
int list_vss_commitment_files(void) {
    printf("[VSS][File] Listing VSS commitment files:\n");
    
    // 使用系统命令列出文件
    (void)system("ls -la vss_commitments/ 2>/dev/null || echo 'No vss_commitments directory found'");
    
    return 0;
} 