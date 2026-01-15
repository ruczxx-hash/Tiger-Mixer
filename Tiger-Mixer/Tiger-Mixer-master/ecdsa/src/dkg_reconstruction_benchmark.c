#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "util.h"

// 测试配置
#define BENCHMARK_RUNS 10  // 每个阈值测试10次
#define FIXED_COMMITTEE_SIZE 20  // 固定委员会大小为20个成员
#define MIN_THRESHOLD 2
#define MAX_THRESHOLD 19  // 阈值从2到19

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
    gettimeofday(&end_time, NULL); \
    elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + \
              (end_time.tv_usec - start_time.tv_usec) / 1000.0;

// 更新统计信息
void update_stats(benchmark_stats_t* stats, double time, int success) {
    if (success) {
        stats->success_count++;
        if (stats->success_count == 1) {
            stats->min_time = stats->max_time = time;
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

// 打印统计结果
void print_stats(const char* test_name, benchmark_stats_t* stats) {
    printf("\n========== %s 性能测试结果 ==========\n", test_name);
    printf("成功次数: %d\n", stats->success_count);
    printf("失败次数: %d\n", stats->fail_count);
    if (stats->success_count > 0) {
        printf("成功率: %.2f%%\n", (double)stats->success_count / (stats->success_count + stats->fail_count) * 100);
        printf("最小时间: %.3f ms\n", stats->min_time);
        printf("最大时间: %.3f ms\n", stats->max_time);
        printf("平均时间: %.3f ms\n", stats->avg_time);
        printf("总时间: %.3f ms\n", stats->total_time);
        printf("吞吐量: %.2f 重构/秒\n", stats->success_count * 1000.0 / stats->total_time);
    }
    printf("=====================================\n");
}

/**
 * 计算拉格朗日系数
 * L_i(0) = ∏_{j≠i} (0-j)/(i-j) = ∏_{j≠i} (-j)/(i-j) = ∏_{j≠i} j/(j-i)
 * 
 * @param result 输出：拉格朗日系数
 * @param participants 参与重构的参与者ID数组
 * @param num_participants 参与者数量
 * @param current_participant 当前参与者的索引（在participants数组中）
 * @param order 群的阶
 */
void compute_lagrange_coefficient(bn_t result, int* participants, int num_participants, 
                                   int current_participant, bn_t order) {
    bn_t numerator, denominator, temp, temp2, inv;
    bn_new(numerator);
    bn_new(denominator);
    bn_new(temp);
    bn_new(temp2);
    bn_new(inv);
    
    // 初始化为1
    bn_set_dig(numerator, 1);
    bn_set_dig(denominator, 1);
    
    int i = participants[current_participant];
    
    // 计算 ∏_{j≠i} j/(j-i)
    for (int k = 0; k < num_participants; k++) {
        if (k == current_participant) continue;
        
        int j = participants[k];
        
        // 分子 *= j
        bn_mul_dig(temp, numerator, j);
        bn_mod(numerator, temp, order);
        
        // 分母 *= (j - i)
        // 注意：如果j < i，需要处理负数
        if (j > i) {
            bn_mul_dig(temp, denominator, j - i);
        } else {
            // (j - i) < 0，需要加上order
            bn_mul_dig(temp, denominator, i - j);
            bn_neg(temp, temp);
            bn_mod(temp, temp, order);
        }
        bn_mod(denominator, temp, order);
    }
    
    // 计算 numerator / denominator = numerator * denominator^(-1) mod order
    bn_gcd_ext(temp, inv, NULL, denominator, order);
    if (bn_cmp_dig(temp, 1) != RLC_EQ) {
        // gcd != 1，无法计算逆元
        bn_zero(result);
    } else {
        bn_mul(temp2, numerator, inv);
        bn_mod(result, temp2, order);
    }
    
    bn_free(numerator);
    bn_free(denominator);
    bn_free(temp);
    bn_free(temp2);
    bn_free(inv);
}

/**
 * 私钥重构性能测试
 * 
 * 数学原理：
 * 秘密 s = ∑_{i∈S} s_i * L_i(0)
 * 其中 S 是参与重构的参与者集合，|S| ≥ threshold
 * L_i(0) 是拉格朗日系数
 * 
 * @param committee_size 委员会大小
 * @param threshold 阈值
 * @param stats 性能统计
 */
void benchmark_secret_reconstruction(int committee_size, int threshold, benchmark_stats_t* stats) {
    printf("测试委员会大小 %d，阈值 %d 的私钥重构...\n", committee_size, threshold);
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        int success = 0;
        double elapsed = 0;
        
        printf("  [调试] 开始运行 %d/%d\n", run + 1, BENCHMARK_RUNS);
        START_BENCHMARK_TIMER();
        
        // 第1步：生成模拟的秘密份额
        printf("  [调试] 步骤1: 生成模拟的秘密份额\n");
        bn_t shares[20];  // 最多20个份额
        bn_t order;
        bn_new(order);
        ec_curve_get_ord(order);
        
        // 生成随机的秘密份额（模拟DKG生成的份额）
        for (int i = 0; i < threshold; i++) {
            bn_new(shares[i]);
            bn_rand_mod(shares[i], order);
        }
        printf("  [调试] 步骤1: 生成了 %d 个秘密份额\n", threshold);
        
        // 第2步：选择参与重构的参与者（使用前threshold个参与者）
        printf("  [调试] 步骤2: 选择参与重构的参与者\n");
        int participants[20];
        for (int i = 0; i < threshold; i++) {
            participants[i] = i + 1;  // 参与者ID从1开始
        }
        printf("  [调试] 步骤2: 选择了 %d 个参与者进行重构\n", threshold);
        
        // 第3步：计算拉格朗日系数
        printf("  [调试] 步骤3: 计算拉格朗日系数\n");
        bn_t lagrange_coeffs[20];
        for (int i = 0; i < threshold; i++) {
            bn_new(lagrange_coeffs[i]);
            compute_lagrange_coefficient(lagrange_coeffs[i], participants, threshold, i, order);
        }
        printf("  [调试] 步骤3: 拉格朗日系数计算完成\n");
        
        // 第4步：重构秘密 s = ∑_{i∈S} s_i * L_i(0)
        printf("  [调试] 步骤4: 重构秘密\n");
        bn_t reconstructed_secret;
        bn_new(reconstructed_secret);
        bn_zero(reconstructed_secret);
        
        bn_t temp;
        bn_new(temp);
        
        for (int i = 0; i < threshold; i++) {
            // temp = s_i * L_i(0)
            bn_mul(temp, shares[i], lagrange_coeffs[i]);
            bn_mod(temp, temp, order);
            
            // reconstructed_secret += temp
            bn_add(reconstructed_secret, reconstructed_secret, temp);
            bn_mod(reconstructed_secret, reconstructed_secret, order);
        }
        printf("  [调试] 步骤4: 秘密重构完成\n");
        
        // 清理资源
        printf("  [调试] 步骤5: 清理资源\n");
        for (int i = 0; i < threshold; i++) {
            bn_free(shares[i]);
            bn_free(lagrange_coeffs[i]);
        }
        bn_free(reconstructed_secret);
        bn_free(temp);
        bn_free(order);
        printf("  [调试] 步骤5: 资源清理完成\n");
        
        END_BENCHMARK_TIMER();
        
        success = 1;
        printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        update_stats(stats, elapsed, success);
    }
}

// 主函数
int main() {
    printf("========== DKG私钥重构性能测试工具 ==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("委员会大小: %d（固定）\n", FIXED_COMMITTEE_SIZE);
    printf("阈值范围: %d-%d\n", MIN_THRESHOLD, MAX_THRESHOLD);
    printf("注意: 测试使用拉格朗日插值重构私钥\n");
    printf("=====================================\n");
    
    // 初始化 relic 库
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    // 设置 secp256k1 曲线
    if (ec_param_set_any() != RLC_OK) {
        printf("EC parameter setting failed\n");
        core_clean();
        return 1;
    }
    ep_param_set(SECG_K256);
    
    printf("密码学库初始化成功\n");
    
    // 创建输出目录
    int mkdir_result = system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
    (void)mkdir_result; // 忽略返回值警告
    
    // 打开CSV文件
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_reconstruction_benchmark.csv", "w");
    if (!csv_file) {
        printf("无法创建CSV文件\n");
        core_clean();
        return 1;
    }
    
    // 写入CSV头部
    fprintf(csv_file, "CommitteeSize,Threshold,ReconstructionTime,Throughput\n");
    
    // 测试不同阈值
    for (int threshold = MIN_THRESHOLD; threshold <= MAX_THRESHOLD; threshold++) {
        printf("\n========== 测试阈值: %d, 委员会大小: %d ==========\n", 
               threshold, FIXED_COMMITTEE_SIZE);
        
        // 测试私钥重构
        benchmark_stats_t recon_stats = {0};
        benchmark_secret_reconstruction(FIXED_COMMITTEE_SIZE, threshold, &recon_stats);
        print_stats("私钥重构", &recon_stats);
        
        // 计算吞吐量
        double throughput = 1000.0 / recon_stats.avg_time; // 重构/秒
        
        // 写入CSV
        fprintf(csv_file, "%d,%d,%.3f,%.2f\n", 
                FIXED_COMMITTEE_SIZE, threshold, 
                recon_stats.avg_time, throughput);
        
        printf("委员会大小: %d, 阈值: %d, 重构时间: %.3f ms, 吞吐量: %.2f 重构/秒\n",
               FIXED_COMMITTEE_SIZE, threshold, recon_stats.avg_time, throughput);
    }
    
    fclose(csv_file);
    
    printf("\n========== 所有测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_reconstruction_benchmark.csv\n");
    
    // 清理资源
    core_clean();
    return 0;
}

