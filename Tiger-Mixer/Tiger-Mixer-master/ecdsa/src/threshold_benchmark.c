#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "secret_share.h"
#include "util.h"

// 时间测量宏
#define START_BENCHMARK_TIMER() \
    struct timeval start_time, end_time; \
    gettimeofday(&start_time, NULL);

#define END_BENCHMARK_TIMER() \
    do { \
        gettimeofday(&end_time, NULL); \
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + \
                  (end_time.tv_usec - start_time.tv_usec) / 1000.0; \
    } while(0)

// 测试参数（Auditor 工作模拟）
#define VSS_MESSAGE_SIZE 15000  // 每个VSS的消息大小
#define NUM_VSS 2  // 两个VSS
#define MIN_COMMITTEE_SIZE 3
#define MAX_COMMITTEE_SIZE 20
#define MAX_THRESHOLD 25  // 最大阈值（用于数组大小，应大于最大可能的阈值）
#define BENCHMARK_RUNS 10

// 统计结构
typedef struct {
    double min_time;
    double max_time;
    double avg_time;
    int success_count;
    int total_count;
} benchmark_stats_t;

// 生成测试数据
void generate_test_data(uint8_t* data, size_t size) {
    srand(time(NULL));
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

// 支持动态阈值的分片生成函数（基于椭圆曲线Feldman VSS，支持分块处理）
int create_secret_shares_dynamic_threshold(const uint8_t* secret, size_t secret_len, 
                                          secret_share_t* shares, size_t* num_shares_out, 
                                          int committee_size, int threshold) {
    if (secret_len == 0 || secret == NULL || shares == NULL) {
        return -1;
    }
    
    if (threshold < 2 || threshold > MAX_THRESHOLD) {
        fprintf(stderr, "Error: Invalid threshold %d (must be between 2 and %d)\n", threshold, MAX_THRESHOLD);
        return -1;
    }
    
    // 计算块数量
    size_t num_blocks = (secret_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (num_blocks > MAX_BLOCKS) {
        fprintf(stderr, "Error: Too many blocks (%zu > %d)\n", num_blocks, MAX_BLOCKS);
        return -1;
    }
    
    // 获取椭圆曲线阶
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 使用基于secret内容的确定性种子
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
        bn_mod(block_bn, block_bn, order);
        
        // 生成多项式系数（在椭圆曲线阶上）
        bn_t coeffs[MAX_THRESHOLD];
        for (int i = 0; i < threshold; i++) {
            bn_new(coeffs[i]);
        }
        
        // 常数项 = 块值
        bn_copy(coeffs[0], block_bn);
        
        // 随机系数（在椭圆曲线阶上）
        for (int i = 1; i < threshold; i++) {
            bn_rand_mod(coeffs[i], order);
        }
        
        // 为每个参与者计算分享值
        for (int participant = 0; participant < committee_size; participant++) {
            bn_set_dig(x_bn, participant + 1);  // x = participant + 1
            
            // 使用Horner方法计算 f(x) = a₀ + a₁x + ... + a_{t-1}x^{t-1} (mod order)
            bn_zero(share_value);
            
            for (int k = threshold - 1; k >= 0; k--) {
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
            size_t share_size = bn_size_bin(share_value);
            if (share_size + 4 > SHARE_SIZE) {
                fprintf(stderr, "Error: Share value too large to serialize\n");
                // 清理资源
                for (int i = 0; i < threshold; i++) {
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
        for (int i = 0; i < threshold; i++) {
            bn_free(coeffs[i]);
        }
        bn_free(block_bn);
    }
    
    // 清理资源
    bn_free(x_bn);
    bn_free(share_value);
    bn_free(order);
    
    *num_shares_out = share_idx;
    return 0;
}

// 拉格朗日系数计算（椭圆曲线阶上，支持动态阈值）
int lagrange_coefficient_ec_dynamic(bn_t result, int xi, int* x_coords, int k, bn_t order) {
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
            bn_gcd_ext(temp, inv, NULL, denominator, order);
            if (bn_sign(inv) == RLC_NEG) {
                bn_add(inv, inv, order);
            }
            
            // result = result * numerator * inv (mod order)
            bn_mul(temp, result, numerator);
            bn_mod(temp, temp, order);
            bn_mul(result, temp, inv);
            bn_mod(result, result, order);
        }
    }
    
    bn_free(numerator);
    bn_free(denominator);
    bn_free(temp);
    bn_free(inv);
    
    return 0;
}

// 动态重构秘密函数（支持不同的阈值，基于椭圆曲线Feldman VSS，分块处理）
int reconstruct_secret_dynamic_threshold(const secret_share_t* shares, int threshold, 
                                         uint8_t* secret, size_t* secret_len, int total_shares) {
    if (threshold <= 0 || threshold > total_shares) {
        fprintf(stderr, "Error: Invalid threshold %d (total shares: %d)\n", threshold, total_shares);
        return -1;
    }
    
    // 检查是否有足够的分片
    int valid_shares = 0;
    size_t secret_len_from_shares = 0;
    size_t max_block_index = 0;
    
    for (int i = 0; i < total_shares; i++) {
        if (shares[i].data_length > 0) {
            valid_shares++;
            if (secret_len_from_shares == 0) {
                secret_len_from_shares = shares[i].data_length;
            }
            if (shares[i].block_index > max_block_index) {
                max_block_index = shares[i].block_index;
            }
        }
    }
    
    if (valid_shares < threshold) {
        fprintf(stderr, "Error: Not enough shares for reconstruction (need %d, have %d)\n", 
                threshold, valid_shares);
        return -1;
    }
    
    if (secret_len_from_shares == 0) {
        fprintf(stderr, "Error: Could not determine secret length\n");
        return -1;
    }
    
    size_t num_blocks = max_block_index + 1;
    *secret_len = secret_len_from_shares;
    
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
        // 收集当前块的 threshold 个分享
        bn_t block_shares[MAX_THRESHOLD];
        int x_coords[MAX_THRESHOLD];
        int share_count = 0;
        
        // 为每个参与者查找对应块的分享（使用前 threshold 个找到的分享）
        // 找出所有唯一的参与者ID，然后选择前 threshold 个
        int unique_participants[MAX_THRESHOLD];
        int unique_count = 0;
        
        // 首先找出所有唯一的参与者ID（从 shares 数组中）
        for (int i = 0; i < total_shares && unique_count < MAX_THRESHOLD; i++) {
            if (shares[i].data_length > 0 && shares[i].block_index == block_idx) {
                int participant = shares[i].x;
                int found = 0;
                for (int j = 0; j < unique_count; j++) {
                    if (unique_participants[j] == participant) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    unique_participants[unique_count++] = participant;
                }
            }
        }
        
        // 选择前 threshold 个参与者
        if (unique_count < threshold) {
            fprintf(stderr, "Error: Not enough unique participants for block %zu (%d < %d)\n", 
                    block_idx, unique_count, threshold);
            bn_free(lagrange_coeff);
            bn_free(contribution);
            bn_free(order);
            return -1;
        }
        
        // 为每个选中的参与者查找对应块的分享
        for (int p_idx = 0; p_idx < threshold && share_count < threshold; p_idx++) {
            int participant = unique_participants[p_idx];
            for (int i = 0; i < total_shares; i++) {
                if (shares[i].data_length > 0 &&
                    shares[i].x == participant &&
                    shares[i].block_index == block_idx) {
                    bn_new(block_shares[share_count]);
                    
                    // 从y数组读取分享值
                    size_t share_size = shares[i].y[0] |
                                       (shares[i].y[1] << 8) |
                                       (shares[i].y[2] << 16) |
                                       (shares[i].y[3] << 24);
                    
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
                    
                    bn_read_bin(block_shares[share_count], shares[i].y + 4, share_size);
                    x_coords[share_count] = participant;
                    share_count++;
                    break;
                }
            }
        }
        
        if (share_count < threshold) {
            fprintf(stderr, "Error: Not enough shares for block %zu (%d < %d)\n", 
                    block_idx, share_count, threshold);
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
        
        for (int i = 0; i < threshold; i++) {
            // 计算Lagrange系数
            lagrange_coefficient_ec_dynamic(lagrange_coeff, x_coords[i], x_coords, threshold, order);
            
            // contribution = share_value * lagrange_coeff (mod order)
            bn_mul(contribution, block_shares[i], lagrange_coeff);
            bn_mod(contribution, contribution, order);
            
            // reconstructed_block = reconstructed_block + contribution (mod order)
            bn_add(reconstructed_block, reconstructed_block, contribution);
            bn_mod(reconstructed_block, reconstructed_block, order);
        }
        
        // 将重构的块写入结果
        size_t block_size = (block_idx == num_blocks - 1) ? 
                           (secret_len_from_shares - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
        size_t reconstructed_block_size = bn_size_bin(reconstructed_block);
        
        if (reconstructed_block_size > block_size) {
            fprintf(stderr, "Error: Reconstructed block %zu too large: %zu > %zu\n", 
                    block_idx, reconstructed_block_size, block_size);
            for (int j = 0; j < threshold; j++) {
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
        memset(secret + reconstructed_offset, 0, block_size);
        if (reconstructed_block_size > 0) {
            bn_write_bin(secret + reconstructed_offset + block_size - reconstructed_block_size, 
                        reconstructed_block_size, reconstructed_block);
        }
        reconstructed_offset += block_size;
        
        // 清理当前块的分享
        for (int j = 0; j < threshold; j++) {
            bn_free(block_shares[j]);
        }
        bn_free(reconstructed_block);
    }
    
    // 清理资源
    bn_free(lagrange_coeff);
    bn_free(contribution);
    bn_free(order);
    
    return 0;
}

// 更新统计信息
void update_stats(benchmark_stats_t* stats, double time, int success) {
    if (success) {
        stats->success_count++;
        if (stats->success_count == 1) {
            stats->min_time = time;
            stats->max_time = time;
            stats->avg_time = time;
        } else {
            if (time < stats->min_time) stats->min_time = time;
            if (time > stats->max_time) stats->max_time = time;
            stats->avg_time = (stats->avg_time * (stats->success_count - 1) + time) / stats->success_count;
        }
    }
    stats->total_count++;
}

// 打印统计结果
void print_stats(const char* name, benchmark_stats_t* stats) {
    double success_rate = (double)stats->success_count / stats->total_count * 100;
    printf("%s: 成功 %d/%d (%.1f%%), 平均时间: %.3f ms, 最小: %.3f ms, 最大: %.3f ms\n",
           name, stats->success_count, stats->total_count, success_rate,
           stats->avg_time, stats->min_time, stats->max_time);
}

// 计算阈值：t = 2n/3 + 1
static int calculate_majority_threshold(int committee_size) {
    return (2 * committee_size) / 3 + 1;
}

// 测试验证VSS承诺的时间（Auditor工作步骤1）
// 注意：承诺和分片应该在计时前准备好（模拟已接收到的数据）
double benchmark_vss_commitment_verification(size_t secret_len, int committee_size, int threshold) {
    double elapsed = 0;
    
    // ⚠️ 在计时前准备数据：生成VSS的承诺和分片（模拟已接收到的数据）
    size_t num_blocks = (secret_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // 检查块数量是否合理
    if (num_blocks > 1000) {
        fprintf(stderr, "Error: Too many blocks (%zu > 1000)\n", num_blocks);
        return -1;
    }
    
    size_t max_shares = num_blocks * committee_size;
    
    secret_share_t* shares = malloc(max_shares * sizeof(secret_share_t));
    if (!shares) {
        return -1;
    }
    
    // 使用动态内存分配承诺和系数数组（避免栈溢出）
    ec_t** commitments = (ec_t**)malloc(num_blocks * sizeof(ec_t*));
    bn_t** coeffs = (bn_t**)malloc(num_blocks * sizeof(bn_t*));
    if (!commitments || !coeffs) {
        free(shares);
        if (commitments) free(commitments);
        if (coeffs) free(coeffs);
        return -1;
    }
    
    // 为每个块分配承诺和系数数组
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        commitments[block_idx] = (ec_t*)malloc(threshold * sizeof(ec_t));
        coeffs[block_idx] = (bn_t*)malloc(threshold * sizeof(bn_t));
        if (!commitments[block_idx] || !coeffs[block_idx]) {
            // 清理已分配的资源
            for (size_t i = 0; i < block_idx; i++) {
                free(commitments[i]);
                free(coeffs[i]);
            }
            free(commitments);
            free(coeffs);
            free(shares);
            return -1;
        }
    }
    
    // 获取椭圆曲线生成元和群阶
    ec_t g;
    bn_t order;
    ec_new(g);
    bn_new(order);
    ec_curve_get_gen(g);
    ec_curve_get_ord(order);
    
    // 生成测试数据
    uint8_t* test_data = malloc(secret_len);
    if (!test_data) {
        // 清理资源
        for (size_t i = 0; i < num_blocks; i++) {
            free(commitments[i]);
            free(coeffs[i]);
        }
        free(commitments);
        free(coeffs);
        free(shares);
        return -1;
    }
    generate_test_data(test_data, secret_len);
    
    // 生成系数和承诺（确保承诺和分片匹配）
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        size_t block_size = (block_idx == num_blocks - 1) ? 
                           (secret_len - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
        
        bn_t block_bn;
        bn_new(block_bn);
        bn_read_bin(block_bn, test_data + block_idx * BLOCK_SIZE, block_size);
        bn_mod(block_bn, block_bn, order);
        
        for (int i = 0; i < threshold; i++) {
            bn_new(coeffs[block_idx][i]);
            if (i == 0) {
                bn_copy(coeffs[block_idx][i], block_bn);
            } else {
                bn_rand_mod(coeffs[block_idx][i], order);
            }
            
            // 生成承诺：C_i = g^(coeff_i)
            ec_new(commitments[block_idx][i]);
            ec_mul_gen(commitments[block_idx][i], coeffs[block_idx][i]);
        }
        bn_free(block_bn);
    }
    
    // 使用相同的系数生成分片（确保匹配）
    size_t share_idx = 0;
    bn_t x_bn, share_value;
    bn_new(x_bn);
    bn_new(share_value);
    
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        for (int participant = 0; participant < committee_size; participant++) {
            bn_set_dig(x_bn, participant + 1);
            
            // 使用Horner方法计算 f(x) = a₀ + a₁x + ... + a_{t-1}x^{t-1} (mod order)
            bn_zero(share_value);
            for (int k = threshold - 1; k >= 0; k--) {
                bn_mul(share_value, share_value, x_bn);
                bn_mod(share_value, share_value, order);
                bn_add(share_value, share_value, coeffs[block_idx][k]);
                bn_mod(share_value, share_value, order);
            }
            
            // 设置分享结构
            shares[share_idx].x = participant + 1;
            shares[share_idx].block_index = block_idx;
            shares[share_idx].data_length = secret_len;
            shares[share_idx].block_size = (block_idx == num_blocks - 1) ? 
                                         (secret_len - block_idx * BLOCK_SIZE) : BLOCK_SIZE;
            memset(shares[share_idx].y, 0, SHARE_SIZE);
            
            // 序列化分享值
            size_t share_size = bn_size_bin(share_value);
            if (share_size + 4 > SHARE_SIZE) {
                // 清理资源
                for (size_t b = 0; b < num_blocks; b++) {
                    for (int i = 0; i < threshold; i++) {
                        ec_free(commitments[b][i]);
                        bn_free(coeffs[b][i]);
                    }
                    free(commitments[b]);
                    free(coeffs[b]);
                }
                free(commitments);
                free(coeffs);
                bn_free(x_bn);
                bn_free(share_value);
                free(shares);
                free(test_data);
                ec_free(g);
                bn_free(order);
                return -1;
            }
            
            shares[share_idx].y[0] = (share_size) & 0xFF;
            shares[share_idx].y[1] = (share_size >> 8) & 0xFF;
            shares[share_idx].y[2] = (share_size >> 16) & 0xFF;
            shares[share_idx].y[3] = (share_size >> 24) & 0xFF;
            bn_write_bin(shares[share_idx].y + 4, share_size, share_value);
            
            share_idx++;
        }
    }
    
    size_t num_shares = share_idx;
    bn_free(x_bn);
    bn_free(share_value);
    free(test_data);
    
    // ⭐ 开始计时：只测量验证计算的时间
    START_BENCHMARK_TIMER();
    
    int verification_success = 1;
    
    // 验证每个参与者的每个块的分片
    for (int participant = 1; participant <= committee_size; participant++) {
        for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
            // 找到该参与者的该块的分片
            secret_share_t* share = NULL;
            for (size_t i = 0; i < num_shares; i++) {
                if (shares[i].x == participant && shares[i].block_index == block_idx) {
                    share = &shares[i];
                    break;
                }
            }
            
            if (!share) {
                verification_success = 0;
                continue;
            }
            
            // 读取分片值
            size_t share_size = share->y[0] |
                               (share->y[1] << 8) |
                               (share->y[2] << 16) |
                               (share->y[3] << 24);
            
            if (share_size == 0 || share_size > SHARE_SIZE - 4) {
                verification_success = 0;
                continue;
            }
            
            bn_t share_value_bn;
            bn_new(share_value_bn);
            bn_read_bin(share_value_bn, share->y + 4, share_size);
            bn_mod(share_value_bn, share_value_bn, order);
            
            // 计算组合承诺：C_share = C_0 + x*C_1 + x²*C_2 + ...
            ec_t C_share;
            ec_new(C_share);
            ec_set_infty(C_share);
            
            bn_t x_bn, x_power;
            bn_new(x_bn);
            bn_new(x_power);
            bn_set_dig(x_bn, participant);
            bn_set_dig(x_power, 1);
            
            for (int i = 0; i < threshold; i++) {
                ec_t C_i_scaled;
                ec_new(C_i_scaled);
                ec_mul(C_i_scaled, commitments[block_idx][i], x_power);
                ec_add(C_share, C_share, C_i_scaled);
                ec_norm(C_share, C_share);
                
                if (i < threshold - 1) {
                    bn_mul(x_power, x_power, x_bn);
                    bn_mod(x_power, x_power, order);
                }
                
                ec_free(C_i_scaled);
            }
            
            // 计算 C_share_value = g^share_value
            ec_t C_share_value;
            ec_new(C_share_value);
            ec_mul_gen(C_share_value, share_value_bn);
            
            // 比较
            int cmp_result = ec_cmp(C_share, C_share_value);
            if (cmp_result != RLC_EQ) {
                verification_success = 0;
            }
            
            // 清理
            ec_free(C_share);
            ec_free(C_share_value);
            bn_free(share_value_bn);
            bn_free(x_bn);
            bn_free(x_power);
        }
    }
    
    END_BENCHMARK_TIMER();
    
    // 清理资源
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        for (int i = 0; i < threshold; i++) {
            ec_free(commitments[block_idx][i]);
            bn_free(coeffs[block_idx][i]);
        }
        free(commitments[block_idx]);
        free(coeffs[block_idx]);
    }
    free(commitments);
    free(coeffs);
    free(shares);
    ec_free(g);
    bn_free(order);
    
    if (!verification_success) {
        // 验证失败，但时间仍然记录
    }
    
    return elapsed;
}

// 测试合成VSS分片的时间（Auditor工作步骤2）
// 注意：分片应该在计时前准备好（模拟已接收到的数据）
double benchmark_vss_share_reconstruction(size_t secret_len, int committee_size, int threshold) {
    double elapsed = 0;
    
    // ⚠️ 在计时前准备数据：生成分片（模拟已接收到的数据）
    size_t num_blocks = (secret_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t max_shares = num_blocks * committee_size;
    
    secret_share_t* shares = malloc(max_shares * sizeof(secret_share_t));
    if (!shares) {
        return -1;
    }
    
    uint8_t* test_data = malloc(secret_len);
    generate_test_data(test_data, secret_len);
    
    size_t num_shares = 0;
    create_secret_shares_dynamic_threshold(test_data, secret_len, shares, &num_shares, 
                                           committee_size, threshold);
    
    // 选择前 threshold 个参与者的所有块的分片
    size_t max_selected_shares = threshold * num_blocks;
    secret_share_t* selected_shares = malloc(max_selected_shares * sizeof(secret_share_t));
    int selected_count = 0;
    
    for (int participant = 1; participant <= threshold; participant++) {
        for (size_t i = 0; i < num_shares; i++) {
            if (shares[i].x == participant) {
                if ((size_t)selected_count < max_selected_shares) {
                    selected_shares[selected_count++] = shares[i];
                }
            }
        }
    }
    
    free(test_data);
    free(shares);
    
    // ⭐ 开始计时：只测量重构计算的时间
    START_BENCHMARK_TIMER();
    
    uint8_t* reconstructed_secret = malloc(secret_len);
    size_t reconstructed_len = 0;
    int reconstruction_result = reconstruct_secret_dynamic_threshold(selected_shares, threshold, 
                                                                   reconstructed_secret, &reconstructed_len, selected_count);
    
    END_BENCHMARK_TIMER();
    
    // 清理资源
    free(selected_shares);
    free(reconstructed_secret);
    
    if (reconstruction_result != 0) {
        // 重构失败，但时间仍然记录
    }
    
    return elapsed;
}

// 测试合成完整DKG私钥的时间（Auditor工作步骤3）
// 注意：私钥分片应该在计时前准备好（模拟已接收到的数据）
double benchmark_dkg_private_key_reconstruction(int committee_size, int threshold, bn_t order) {
    double elapsed = 0;
    
    // ⚠️ 在计时前准备数据：生成所有参与者的私钥分片（模拟已接收到的数据）
    bn_t* private_key_shares = (bn_t*)malloc(committee_size * sizeof(bn_t));
    
    // 对于每个参与者，生成他们的私钥分片 sk_i = Σ_{j=1}^n s_{j,i}
    for (int participant_id = 1; participant_id <= committee_size; participant_id++) {
        bn_new(private_key_shares[participant_id - 1]);
        bn_zero(private_key_shares[participant_id - 1]);
        
        // 累加所有参与者发送给该参与者的分片
        for (int sender_id = 1; sender_id <= committee_size; sender_id++) {
            // 生成发送者sender_id的多项式系数
            bn_t poly_coeffs[MAX_THRESHOLD];
            for (int k = 0; k < threshold; k++) {
                bn_new(poly_coeffs[k]);
                bn_rand_mod(poly_coeffs[k], order);
            }
            
            // 计算发送者发送给该参与者的分片 s_{sender_id, participant_id} = f_{sender_id}(participant_id)
            bn_t share_sij;
            bn_new(share_sij);
            bn_copy(share_sij, poly_coeffs[threshold - 1]);
            
            bn_t x_bn;
            bn_new(x_bn);
            bn_set_dig(x_bn, participant_id);
            
            for (int k = threshold - 2; k >= 0; k--) {
                bn_mul(share_sij, share_sij, x_bn);
                bn_add(share_sij, share_sij, poly_coeffs[k]);
                bn_mod(share_sij, share_sij, order);
            }
            
            // 累加到私钥分片
            bn_add(private_key_shares[participant_id - 1], private_key_shares[participant_id - 1], share_sij);
            bn_mod(private_key_shares[participant_id - 1], private_key_shares[participant_id - 1], order);
            
            // 清理
            for (int k = 0; k < threshold; k++) {
                bn_free(poly_coeffs[k]);
            }
            bn_free(share_sij);
            bn_free(x_bn);
        }
    }
    
    // ⭐ 开始计时：只测量合成完整私钥的时间（使用Lagrange插值）
    START_BENCHMARK_TIMER();
    
    // 使用前 threshold 个参与者的私钥分片重构完整私钥
    bn_t full_private_key;
    bn_new(full_private_key);
    bn_zero(full_private_key);
    
    // 获取椭圆曲线阶
    bn_t order_check;
    bn_new(order_check);
    ec_curve_get_ord(order_check);
    
    // 使用Lagrange插值重构：S = Σ_{i=1}^t L_i(0) * sk_i
    int x_coords[MAX_THRESHOLD];
    for (int i = 0; i < threshold; i++) {
        x_coords[i] = i + 1;  // 参与者ID从1开始
    }
    
    bn_t lagrange_coeff, contribution;
    bn_new(lagrange_coeff);
    bn_new(contribution);
    
    for (int i = 0; i < threshold; i++) {
        // 计算Lagrange系数 L_i(0)
        lagrange_coefficient_ec_dynamic(lagrange_coeff, x_coords[i], x_coords, threshold, order);
        
        // contribution = sk_i * L_i(0) (mod order)
        bn_mul(contribution, private_key_shares[i], lagrange_coeff);
        bn_mod(contribution, contribution, order);
        
        // full_private_key = full_private_key + contribution (mod order)
        bn_add(full_private_key, full_private_key, contribution);
        bn_mod(full_private_key, full_private_key, order);
    }
    
    END_BENCHMARK_TIMER();
    
    // 清理资源
    bn_free(full_private_key);
    bn_free(lagrange_coeff);
    bn_free(contribution);
    bn_free(order_check);
    for (int i = 0; i < committee_size; i++) {
        bn_free(private_key_shares[i]);
    }
    free(private_key_shares);
    
    return elapsed;
}

// 测试Auditor的完整工作流程
void benchmark_auditor_workflow() {
    printf("\n========== Auditor工作流程性能测试 ==========\n");
    printf("VSS消息大小: %d 字节（每个VSS）\n", VSS_MESSAGE_SIZE);
    printf("VSS数量: %d 个\n", NUM_VSS);
    printf("委员会大小范围: %d - %d\n", MIN_COMMITTEE_SIZE, MAX_COMMITTEE_SIZE);
        printf("阈值计算: 2n/3+1\n");
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    printf("Auditor工作步骤:\n");
    printf("  1. 验证两个VSS的承诺（每个15000字节）\n");
    printf("  2. 验证后合成分片（两次，每个VSS一次）\n");
    printf("  3. 合成完整的DKG私钥\n");
    printf("==========================================\n");
    
    // 创建输出目录
    int mkdir_result = system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
    (void)mkdir_result; // 忽略返回值
    
    // 打开CSV文件
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/auditor_benchmark.csv", "w");
    if (!csv_file) {
        printf("错误: 无法创建CSV文件\n");
        return;
    }
    fprintf(csv_file, "CommitteeSize,Threshold,VSSVerificationTime,VSSReconstructionTime,DKGKeyReconstructionTime,TotalTime\n");
    
    // 获取椭圆曲线阶（用于DKG私钥重构）
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 遍历不同的委员会大小
    for (int committee_size = MIN_COMMITTEE_SIZE; committee_size <= MAX_COMMITTEE_SIZE; committee_size++) {
        int threshold = calculate_majority_threshold(committee_size);
        
        printf("\n--- 测试委员会大小: %d, 阈值: %d (2n/3+1) ---\n", committee_size, threshold);
        
        benchmark_stats_t vss_commitment_stats = {0};
        benchmark_stats_t vss_share_stats = {0};
        benchmark_stats_t dkg_private_key_stats = {0};
        benchmark_stats_t total_stats = {0};
        
        for (int run = 0; run < BENCHMARK_RUNS; run++) {
            // 步骤1：验证两个VSS的承诺
            double vss_commitment_time = 0;
            for (int vss_idx = 0; vss_idx < NUM_VSS; vss_idx++) {
                double elapsed = benchmark_vss_commitment_verification(VSS_MESSAGE_SIZE, committee_size, threshold);
                if (elapsed >= 0) {
                    vss_commitment_time += elapsed;
                }
            }
            update_stats(&vss_commitment_stats, vss_commitment_time, vss_commitment_time >= 0);
            
            // 步骤2：验证后合成分片（两次，每个VSS一次）
            double vss_share_time = 0;
            for (int vss_idx = 0; vss_idx < NUM_VSS; vss_idx++) {
                double elapsed = benchmark_vss_share_reconstruction(VSS_MESSAGE_SIZE, committee_size, threshold);
                if (elapsed >= 0) {
                    vss_share_time += elapsed;
                }
            }
            update_stats(&vss_share_stats, vss_share_time, vss_share_time >= 0);
            
            // 步骤3：合成完整的DKG私钥
            double dkg_private_key_time = benchmark_dkg_private_key_reconstruction(committee_size, threshold, order);
            update_stats(&dkg_private_key_stats, dkg_private_key_time, dkg_private_key_time >= 0);
            
            // 计算总时间
            if (vss_commitment_time >= 0 && vss_share_time >= 0 && dkg_private_key_time >= 0) {
                double total_time = vss_commitment_time + vss_share_time + dkg_private_key_time;
                update_stats(&total_stats, total_time, 1);
            }
        }
        
        // 打印统计结果
        print_stats("VSS承诺验证", &vss_commitment_stats);
        print_stats("VSS分片重构", &vss_share_stats);
        print_stats("DKG私钥重构", &dkg_private_key_stats);
        print_stats("总时间", &total_stats);
        
        // 写入CSV数据（只记录成功的时间）
        fprintf(csv_file, "%d,%d,%.3f,%.3f,%.3f,%.3f\n", 
                committee_size,
                threshold,
                vss_commitment_stats.avg_time,
                vss_share_stats.avg_time,
                dkg_private_key_stats.avg_time,
                total_stats.avg_time);
    }
    
    bn_free(order);
    fclose(csv_file);
    
    printf("\n========== Auditor工作流程测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/auditor_benchmark.csv\n");
}

int main() {
    printf("========== Auditor工作流程性能测试工具 ==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("==========================================\n");
    
    // 初始化 relic 库
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    // 设置椭圆曲线参数为 secp256k1
    ec_param_set_any();
    ep_param_set(SECG_K256);
    
    benchmark_auditor_workflow();
    
    core_clean();
    return 0;
}
