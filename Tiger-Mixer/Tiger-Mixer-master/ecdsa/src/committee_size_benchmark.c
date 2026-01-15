#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "secret_share.h"
#include "util.h"

// 测试配置
#define BENCHMARK_RUNS 100
#define FIXED_MESSAGE_SIZE 15767
#define MIN_COMMITTEE_SIZE 3
#define MAX_COMMITTEE_SIZE 20

// 性能统计结构
typedef struct {
    double min_time;
    double max_time;
    double avg_time;
    double total_time;
    int success_count;
    int fail_count;
} benchmark_stats_t;

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

// 生成测试数据
void generate_test_data(uint8_t* data, size_t size) {
    srand(time(NULL));
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

// 支持动态委员会大小的VSS承诺生成函数需要的系数保存
// 注意：这个函数需要保存系数以便生成承诺
static bn_t saved_coeffs_dynamic[1000][THRESHOLD];  // [块索引][系数索引]
static int coeffs_saved_dynamic = 0;

void init_saved_coeffs_dynamic(size_t num_blocks) {
    if (coeffs_saved_dynamic) {
        // 清理旧的系数
        for (size_t i = 0; i < 1000; i++) {
            for (int j = 0; j < THRESHOLD; j++) {
                bn_free(saved_coeffs_dynamic[i][j]);
            }
        }
    }
    
    for (size_t i = 0; i < num_blocks && i < 1000; i++) {
        for (int j = 0; j < THRESHOLD; j++) {
            bn_new(saved_coeffs_dynamic[i][j]);
        }
    }
    coeffs_saved_dynamic = 1;
}

// 支持动态委员会大小的分片生成函数
// 基于椭圆曲线Feldman VSS，支持分块处理
int create_secret_shares_dynamic_committee(const uint8_t* secret, size_t secret_len, 
                                          secret_share_t* shares, size_t* num_shares_out, 
                                          int committee_size) {
    if (secret_len == 0 || secret == NULL || shares == NULL) {
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
        
        // 保存系数（用于后续生成承诺）
        for (int i = 0; i < THRESHOLD; i++) {
            bn_copy(saved_coeffs_dynamic[block_idx][i], coeffs[i]);
        }
        
        // 为每个参与者计算分享值（动态委员会大小）
        for (int participant = 0; participant < committee_size; participant++) {
            bn_set_dig(x_bn, participant + 1);  // x = participant + 1
            
            // 使用Horner方法计算 f(x) = a₀ + a₁x + ... + a_{t-1}x^{t-1} (mod order)
            bn_zero(share_value);
            
            for (int k = THRESHOLD - 1; k >= 0; k--) {
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
    
    // 清理资源
    bn_free(x_bn);
    bn_free(share_value);
    bn_free(order);
    
    *num_shares_out = share_idx;
    return 0;
}

// 支持动态委员会大小的VSS承诺生成函数
int create_vss_commitments_dynamic_committee(const uint8_t* secret, size_t secret_len,
                                            secret_share_t* shares, size_t num_shares,
                                            vss_commitment_t* commitment, const char* msgid,
                                            int committee_size) {
    if (secret_len == 0 || secret == NULL || shares == NULL || commitment == NULL) {
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
        snprintf(commitment->msgid, MSG_ID_MAXLEN, "timestamp_%ld", commitment->timestamp);
    }
    
    // 获取椭圆曲线生成元和群阶
    ec_t g;
    bn_t order;
    ec_new(g);
    bn_new(order);
    ec_curve_get_gen(g);
    ec_curve_get_ord(order);
    
    ec_t temp_point;
    ec_new(temp_point);
    bn_t coeff_bn;
    bn_new(coeff_bn);
    
    // 为每个块创建 Feldman 承诺
    for (size_t block_idx = 0; block_idx < num_blocks; block_idx++) {
        // 为每个系数创建 Feldman 承诺：C_j = g^(a_j)
        for (int i = 0; i < THRESHOLD; i++) {
            // 从保存的系数中读取
            bn_copy(coeff_bn, saved_coeffs_dynamic[block_idx][i]);
            bn_mod(coeff_bn, coeff_bn, order);
            
            // 计算 Feldman 承诺：C_i = g^(coeff_i)
            ec_mul_gen(temp_point, coeff_bn);
            
            // 压缩并存储椭圆曲线点
            uint8_t compressed[RLC_EC_SIZE_COMPRESSED];
            ec_write_bin(compressed, RLC_EC_SIZE_COMPRESSED, temp_point, 1);
            
            memcpy(commitment->commitments[block_idx][i], compressed, RLC_EC_SIZE_COMPRESSED);
        }
    }
    
    // 清理资源
    ec_free(g);
    ec_free(temp_point);
    bn_free(order);
    bn_free(coeff_bn);
    
    return 0;
}

// 更新统计信息
void update_stats(benchmark_stats_t* stats, double time, int success) {
    if (success) {
        stats->success_count++;
        if (stats->success_count == 1) {
            stats->min_time = time;
            stats->max_time = time;
        } else {
            if (time < stats->min_time) stats->min_time = time;
            if (time > stats->max_time) stats->max_time = time;
        }
        stats->total_time += time;
        stats->avg_time = stats->total_time / stats->success_count;
    } else {
        stats->fail_count++;
    }
}

// 打印统计信息
void print_stats(const char* test_name, benchmark_stats_t* stats) {
    printf("\n========== %s 统计结果 ==========\n", test_name);
    printf("成功次数: %d\n", stats->success_count);
    printf("失败次数: %d\n", stats->fail_count);
    printf("最小时间: %.3f ms\n", stats->min_time);
    printf("最大时间: %.3f ms\n", stats->max_time);
    printf("平均时间: %.3f ms\n", stats->avg_time);
    printf("总时间: %.3f ms\n", stats->total_time);
    printf("成功率: %.1f%%\n", (double)stats->success_count / (stats->success_count + stats->fail_count) * 100);
    printf("=====================================\n");
}

// 测试不同委员会大小的性能
// 使用动态委员会大小：分片数量 = 块数 × 委员会大小
void benchmark_committee_sizes() {
    printf("========== 委员会大小性能测试（基于椭圆曲线Feldman VSS）==========\n");
    printf("固定消息大小: %d 字节\n", FIXED_MESSAGE_SIZE);
    printf("委员会大小范围: %d - %d\n", MIN_COMMITTEE_SIZE, MAX_COMMITTEE_SIZE);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    printf("VSS类型: 椭圆曲线Feldman VSS（分块处理，动态委员会大小）\n");
    printf("阈值: %d (THRESHOLD)\n", THRESHOLD);
    printf("块大小: %d 字节 (BLOCK_SIZE)\n", BLOCK_SIZE);
    printf("==========================================\n");
    
    // 创建输出目录
    int mkdir_result = system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
    (void)mkdir_result; // 忽略返回值
    
    // 打开CSV文件
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/committee_size_benchmark.csv", "w");
    if (!csv_file) {
        printf("错误: 无法创建CSV文件\n");
        return;
    }
    
    // 写入CSV头部
    fprintf(csv_file, "CommitteeSize,ShareTime,CommitmentTime,TotalTime,TPS,ShareSuccessRate,CommitmentSuccessRate\n");
    
    // 分配测试数据
    uint8_t* test_data = malloc(FIXED_MESSAGE_SIZE);
    if (!test_data) {
        printf("错误: 内存分配失败\n");
        fclose(csv_file);
        return;
    }
    
    // 计算块数量
    size_t num_blocks = (FIXED_MESSAGE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // 计算最大可能的分片数（num_blocks * MAX_COMMITTEE_SIZE）
    size_t max_shares = num_blocks * MAX_COMMITTEE_SIZE;
    
    // 分配分片数组（使用最大可能的分片数）
    secret_share_t* shares = malloc(max_shares * sizeof(secret_share_t));
    if (!shares) {
        printf("错误: 无法分配分片内存\n");
        fclose(csv_file);
        free(test_data);
        return;
    }
    
    // 初始化系数保存数组
    init_saved_coeffs_dynamic(num_blocks);
    
    for (int committee_size = MIN_COMMITTEE_SIZE; committee_size <= MAX_COMMITTEE_SIZE; committee_size++) {
        printf("\n--- 测试委员会大小: %d ---\n", committee_size);
        
        // 计算该委员会大小下的分片数
        size_t expected_shares = num_blocks * committee_size;
        printf("预期分片数: %zu (块数: %zu × 委员会大小: %d)\n", expected_shares, num_blocks, committee_size);
        
        benchmark_stats_t share_stats = {0};
        benchmark_stats_t commitment_stats = {0};
        benchmark_stats_t total_stats = {0};
        
        for (int run = 0; run < BENCHMARK_RUNS; run++) {
            generate_test_data(test_data, FIXED_MESSAGE_SIZE);
            
            double share_elapsed = 0;
            double commitment_elapsed = 0;
            double total_elapsed = 0;
            double elapsed = 0;
            
            // 测试分片生成时间（基于椭圆曲线Feldman VSS，分块处理，动态委员会大小）
            // 注意：系数会在 create_secret_shares_dynamic_committee 中自动保存
            size_t num_shares_out = 0;
            START_BENCHMARK_TIMER();
            int share_result = create_secret_shares_dynamic_committee(test_data, FIXED_MESSAGE_SIZE, 
                                                                      shares, &num_shares_out, committee_size);
            END_BENCHMARK_TIMER();
            share_elapsed = elapsed;
            
            // 验证分片数量
            if (share_result == 0 && num_shares_out == expected_shares) {
                update_stats(&share_stats, share_elapsed, 1);
            } else {
                if (run == 0) {  // 只打印一次警告
                    printf("警告: 分片生成失败或数量不匹配 (result=%d, num_shares_out=%zu, expected=%zu)\n", 
                           share_result, num_shares_out, expected_shares);
                }
                update_stats(&share_stats, share_elapsed, 0);
            }
            
            if (share_result == 0 && num_shares_out == expected_shares) {
                // 测试VSS承诺生成时间（基于椭圆曲线Feldman VSS）
                vss_commitment_t commitment;
                START_BENCHMARK_TIMER();
                int commitment_result = create_vss_commitments_dynamic_committee(test_data, FIXED_MESSAGE_SIZE,
                                                                                  shares, num_shares_out,
                                                                                  &commitment, "test_msgid", 
                                                                                  committee_size);
                END_BENCHMARK_TIMER();
                commitment_elapsed = elapsed;
                
                if (commitment_result == 0) {
                    update_stats(&commitment_stats, commitment_elapsed, 1);
                    total_elapsed = share_elapsed + commitment_elapsed;
                    update_stats(&total_stats, total_elapsed, 1);
                } else {
                    if (run == 0) {  // 只打印一次警告
                        printf("警告: 承诺生成失败\n");
                    }
                    update_stats(&commitment_stats, commitment_elapsed, 0);
                }
            }
        }
        
        // 打印统计结果
        print_stats("分片生成（椭圆曲线Feldman VSS）", &share_stats);
        print_stats("承诺生成（椭圆曲线Feldman VSS）", &commitment_stats);
        print_stats("总时间", &total_stats);
        
        // 写入CSV数据
        double share_success_rate = (double)share_stats.success_count / BENCHMARK_RUNS * 100;
        double commitment_success_rate = (double)commitment_stats.success_count / BENCHMARK_RUNS * 100;
        
        // 计算TPS: 每秒可以完成的交易数 = 1000ms / 平均总时间(ms)
        double tps = 0.0;
        if (total_stats.avg_time > 0) {
            tps = 1000.0 / total_stats.avg_time;
        }
        
        fprintf(csv_file, "%d,%.3f,%.3f,%.3f,%.2f,%.1f,%.1f\n", 
                committee_size, 
                share_stats.avg_time, 
                commitment_stats.avg_time, 
                total_stats.avg_time,
                tps,
                share_success_rate,
                commitment_success_rate);
    }
    
    fclose(csv_file);
    free(shares);
    free(test_data);
    
    printf("\n========== 委员会大小测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/committee_size_benchmark.csv\n");
    printf("注意: 使用动态委员会大小，分片数量 = 块数 × 委员会大小\n");
}

// 主函数
int main() {
    printf("========== 委员会大小性能测试工具（椭圆曲线Feldman VSS）==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("==========================================\n");
    
    // 初始化 relic 库
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    // 设置椭圆曲线参数（secp256k1）
    if (ec_param_set_any() != RLC_OK) {
        printf("设置椭圆曲线参数失败\n");
        core_clean();
        return 1;
    }
    ep_param_set(SECG_K256);
    
    printf("椭圆曲线参数设置成功: secp256k1\n");
    
    // 运行委员会大小性能测试
    benchmark_committee_sizes();
    
    printf("\n========== 所有测试完成 ==========\n");
    
    core_clean();
    return 0;
}
