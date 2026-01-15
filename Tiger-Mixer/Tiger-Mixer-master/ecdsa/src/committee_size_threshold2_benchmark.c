#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "secret_share.h"
#include "util.h"

// 时间测量宏（使用不同的名称避免与util.h冲突）
#define START_BENCHMARK_TIMER() \
    struct timeval start_time, end_time; \
    gettimeofday(&start_time, NULL);

#define END_BENCHMARK_TIMER() \
    do { \
        gettimeofday(&end_time, NULL); \
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + \
                  (end_time.tv_usec - start_time.tv_usec) / 1000.0; \
    } while(0)

// 测试参数
#define FIXED_MESSAGE_SIZE 15767
#define FIXED_THRESHOLD 2  // 固定阈值为2
#define MIN_COMMITTEE_SIZE 3
#define MAX_COMMITTEE_SIZE 20
#define BENCHMARK_RUNS 50

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

// GF(256)逆元函数（简单实现）
uint8_t gf256_inv(uint8_t a) {
    if (a == 0) return 0; // 0的逆元不存在，返回0
    
    // 使用扩展欧几里得算法计算逆元
    // 在GF(256)中，我们需要找到b使得 a * b = 1 (mod 256)
    // 这里使用一个简化的查找表方法
    for (uint8_t b = 1; b < 256; b++) {
        if (gf256_mul(a, b) == 1) {
            return b;
        }
    }
    return 1; // 如果找不到，返回1作为默认值
}

// 动态创建秘密分享函数（支持不同的委员会大小）
int create_secret_shares_dynamic_committee(const uint8_t* secret, size_t secret_len, 
                                         secret_share_t* shares, int committee_size, int threshold) {
    if (secret_len > SHARE_SIZE) {
        fprintf(stderr, "Error: Secret too large for sharing\n");
        return -1;
    }
    srand(time(NULL));
    for (size_t byte_idx = 0; byte_idx < secret_len; byte_idx++) {
        uint8_t secret_byte = secret[byte_idx];
        uint8_t coeffs[threshold];
        coeffs[0] = secret_byte;
        for (int i = 1; i < threshold; i++) {
            coeffs[i] = rand() % 256;
        }
        // 根据委员会大小生成分片
        for (int share_idx = 0; share_idx < committee_size; share_idx++) {
            int x = share_idx + 1;
            uint8_t y = coeffs[0];
            uint8_t x_power = x;
            for (int coeff_idx = 1; coeff_idx < threshold; coeff_idx++) {
                y = gf256_add(y, gf256_mul(coeffs[coeff_idx], x_power));
                x_power = gf256_mul(x_power, x);
            }
            shares[share_idx].x = x;
            shares[share_idx].y[byte_idx] = y;
            shares[share_idx].data_length = secret_len;
        }
    }
    return 0;
}

// 动态重构秘密函数（支持不同的委员会大小，使用固定阈值）
int reconstruct_secret_dynamic_committee(const secret_share_t* shares, int threshold, 
                                        uint8_t* secret, size_t* secret_len, int total_shares) {
    if (threshold <= 0 || threshold > total_shares) {
        fprintf(stderr, "Error: Invalid threshold %d (total shares: %d)\n", threshold, total_shares);
        return -1;
    }
    
    // 检查是否有足够的分片
    int valid_shares = 0;
    for (int i = 0; i < total_shares; i++) {
        if (shares[i].data_length > 0) {
            valid_shares++;
        }
    }
    
    if (valid_shares < threshold) {
        fprintf(stderr, "Error: Not enough shares for reconstruction (need %d, have %d)\n", 
                threshold, valid_shares);
        return -1;
    }
    
    // 使用前threshold个有效分片进行重构
    size_t data_len = shares[0].data_length;
    *secret_len = data_len;
    
    for (size_t byte_idx = 0; byte_idx < data_len; byte_idx++) {
        uint8_t secret_byte = 0;
        
        // 拉格朗日插值重构
        for (int i = 0; i < threshold; i++) {
            uint8_t lagrange_coeff = 1;
            for (int j = 0; j < threshold; j++) {
                if (i != j) {
                    // 计算拉格朗日系数: L_i = ∏(x - x_j)/(x_i - x_j) for j≠i
                    int xi = shares[i].x;
                    int xj = shares[j].x;
                    if (xi != xj) {
                        // 计算拉格朗日系数: L_i = ∏(x - x_j)/(x_i - x_j) for j≠i
                        // 在GF(256)中，除法等价于乘以逆元
                        uint8_t numerator = xj;
                        uint8_t denominator = gf256_add(xi, xj);
                        uint8_t inv_denominator = gf256_inv(denominator);
                        lagrange_coeff = gf256_mul(lagrange_coeff, 
                                                 gf256_mul(numerator, inv_denominator));
                    }
                }
            }
            secret_byte = gf256_add(secret_byte, 
                                   gf256_mul(shares[i].y[byte_idx], lagrange_coeff));
        }
        secret[byte_idx] = secret_byte;
    }
    
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

// 测试不同委员会大小下的重构性能（固定阈值为2）
void benchmark_committee_size_reconstruction() {
    printf("\n开始测试不同委员会大小下的重构性能（固定阈值=%d）...\n", FIXED_THRESHOLD);
    printf("固定消息大小: %d 字节\n", FIXED_MESSAGE_SIZE);
    printf("固定阈值: %d (需要%d个分片重构)\n", FIXED_THRESHOLD, FIXED_THRESHOLD);
    printf("委员会大小范围: %d - %d 个分片\n", MIN_COMMITTEE_SIZE, MAX_COMMITTEE_SIZE);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    printf("==========================================\n");
    
    // 创建输出目录
    int mkdir_result = system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
    (void)mkdir_result; // 忽略返回值
    
    // 打开CSV文件
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/committee_size_threshold2_benchmark.csv", "w");
    if (!csv_file) {
        printf("错误: 无法创建CSV文件\n");
        return;
    }
    fprintf(csv_file, "CommitteeSize,ReconstructionTime,SuccessRate,Throughput\n");
    
    uint8_t* test_data = malloc(FIXED_MESSAGE_SIZE);
    if (!test_data) {
        printf("内存分配失败\n");
        fclose(csv_file);
        return;
    }
    
    for (int committee_size = MIN_COMMITTEE_SIZE; committee_size <= MAX_COMMITTEE_SIZE; committee_size++) {
        printf("\n--- 测试委员会大小: %d 个分片 (阈值=%d) ---\n", committee_size, FIXED_THRESHOLD);
        
        // 创建分片数组（根据委员会大小动态分配）
        secret_share_t* shares = malloc(committee_size * sizeof(secret_share_t));
        if (!shares) {
            printf("错误: 无法分配分片内存\n");
            continue;
        }
        
        // 初始化分片
        for (int i = 0; i < committee_size; i++) {
            shares[i].x = i + 1;
            shares[i].data_length = 0; // 初始化为无效
            memset(shares[i].y, 0, SHARE_SIZE);
        }
        
        benchmark_stats_t reconstruction_stats = {0};
        
        for (int run = 0; run < BENCHMARK_RUNS; run++) {
            generate_test_data(test_data, FIXED_MESSAGE_SIZE);
            
            // 创建分片（使用当前委员会大小）
            int share_result = create_secret_shares_dynamic_committee(test_data, FIXED_MESSAGE_SIZE, 
                                                                    shares, committee_size, FIXED_THRESHOLD);
            if (share_result != 0) {
                update_stats(&reconstruction_stats, 0, 0);
                continue;
            }
            
            // 测试重构时间（只需要threshold个分片）
            uint8_t reconstructed_secret[FIXED_MESSAGE_SIZE];
            size_t reconstructed_len = 0;
            double reconstruction_elapsed = 0;
            double elapsed = 0;
            
            START_BENCHMARK_TIMER();
            int reconstruction_result = reconstruct_secret_dynamic_committee(shares, FIXED_THRESHOLD, 
                                                                           reconstructed_secret, &reconstructed_len, committee_size);
            END_BENCHMARK_TIMER();
            reconstruction_elapsed = elapsed;
            
            // 验证重构结果
            int success = 0;
            if (reconstruction_result == 0 && reconstructed_len == FIXED_MESSAGE_SIZE) {
                success = (memcmp(test_data, reconstructed_secret, FIXED_MESSAGE_SIZE) == 0);
            }
            
            update_stats(&reconstruction_stats, reconstruction_elapsed, success);
        }
        
        // 打印统计结果
        print_stats("重构", &reconstruction_stats);
        
        // 写入CSV数据
        double success_rate = (double)reconstruction_stats.success_count / reconstruction_stats.total_count * 100;
        double throughput = (FIXED_MESSAGE_SIZE / 1024.0 / 1024.0) / (reconstruction_stats.avg_time / 1000.0);
        fprintf(csv_file, "%d,%.3f,%.1f,%.2f\n", 
                committee_size, 
                reconstruction_stats.avg_time, 
                success_rate,
                throughput);
        
        // 清理分片内存
        free(shares);
    }
    
    fclose(csv_file);
    free(test_data);
    
    printf("\n========== 委员会大小重构测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/committee_size_threshold2_benchmark.csv\n");
}

int main() {
    printf("========== 委员会大小重构性能测试工具（阈值=2）==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("==========================================\n");
    
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    benchmark_committee_size_reconstruction();
    
    core_clean();
    return 0;
}

