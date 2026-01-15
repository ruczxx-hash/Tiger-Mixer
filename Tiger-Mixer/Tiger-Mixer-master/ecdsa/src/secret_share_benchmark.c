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
#define TEST_DATA_SIZE 15767
#define SECRET_SHARES 3
#define THRESHOLD 2

// 检查数据大小是否在限制内
#define MAX_TEST_SIZE 100000

// 性能统计结构
typedef struct {
    double min_time;
    double max_time;
    double avg_time;
    double total_time;
    int success_count;
    int fail_count;
} benchmark_stats_t;

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

// 生成测试数据
void generate_test_data(uint8_t* data, size_t size) {
    srand(time(NULL));
    for (size_t i = 0; i < size; i++) {
        data[i] = rand() % 256;
    }
}

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
    printf("成功率: %.2f%%\n", (double)stats->success_count / (stats->success_count + stats->fail_count) * 100);
    printf("最小时间: %.3f ms\n", stats->min_time);
    printf("最大时间: %.3f ms\n", stats->max_time);
    printf("平均时间: %.3f ms\n", stats->avg_time);
    printf("总时间: %.3f ms\n", stats->total_time);
    printf("吞吐量: %.2f 操作/秒\n", stats->success_count * 1000.0 / stats->total_time);
    printf("=====================================\n");
}

// 测试1: 分片生成性能
void benchmark_share_creation() {
    printf("\n开始测试分片生成性能...\n");
    printf("数据大小: %d 字节\n", TEST_DATA_SIZE);
    printf("分片数量: %d\n", SECRET_SHARES);
    printf("阈值: %d\n", THRESHOLD);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    
    // 检查数据大小是否在限制内
    if (TEST_DATA_SIZE > MAX_TEST_SIZE) {
        printf("错误: 测试数据大小 %d 超过限制 %d\n", TEST_DATA_SIZE, MAX_TEST_SIZE);
        return;
    }
    
    benchmark_stats_t stats = {0};
    uint8_t* test_data = malloc(TEST_DATA_SIZE);
    if (!test_data) {
        printf("内存分配失败\n");
        return;
    }
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        // 生成测试数据
        generate_test_data(test_data, TEST_DATA_SIZE);
        
        // 创建分片（需要分配足够的空间）
        size_t num_blocks = (TEST_DATA_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        size_t max_shares = num_blocks * SECRET_SHARES;
        secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
        if (shares == NULL) {
            printf("运行 %d: 内存分配失败\n", run + 1);
            continue;
        }
        
        size_t num_shares = 0;
        int success = 0;
        double elapsed = 0;
        
        START_BENCHMARK_TIMER();
        int result = create_secret_shares(test_data, TEST_DATA_SIZE, shares, &num_shares);
        END_BENCHMARK_TIMER();
        
        if (result == 0) {
            success = 1;
            printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        } else {
            printf("运行 %d: 失败, 错误码: %d\n", run + 1, result);
        }
        
        update_stats(&stats, elapsed, success);
        free(shares);
    }
    
    free(test_data);
    print_stats("分片生成", &stats);
}

// 测试2: 分片重构性能
void benchmark_share_reconstruction() {
    printf("\n开始测试分片重构性能...\n");
    printf("数据大小: %d 字节\n", TEST_DATA_SIZE);
    printf("分片数量: %d\n", SECRET_SHARES);
    printf("阈值: %d\n", THRESHOLD);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    
    // 检查数据大小是否在限制内
    if (TEST_DATA_SIZE > MAX_TEST_SIZE) {
        printf("错误: 测试数据大小 %d 超过限制 %d\n", TEST_DATA_SIZE, MAX_TEST_SIZE);
        return;
    }
    
    benchmark_stats_t stats = {0};
    uint8_t* test_data = malloc(TEST_DATA_SIZE);
    if (!test_data) {
        printf("内存分配失败\n");
        return;
    }
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        // 生成测试数据
        generate_test_data(test_data, TEST_DATA_SIZE);
        
        // 创建分片（需要分配足够的空间）
        size_t num_blocks = (TEST_DATA_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        size_t max_shares = num_blocks * SECRET_SHARES;
        secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
        if (shares == NULL) {
            printf("运行 %d: 内存分配失败\n", run + 1);
            continue;
        }
        
        size_t num_shares = 0;
        int create_result = create_secret_shares(test_data, TEST_DATA_SIZE, shares, &num_shares);
        if (create_result != 0) {
            printf("运行 %d: 分片创建失败\n", run + 1);
            free(shares);
            update_stats(&stats, 0, 0);
            continue;
        }
        
        // 重构秘密
        uint8_t* reconstructed_data = malloc(TEST_DATA_SIZE);
        size_t reconstructed_length = 0;
        int success = 0;
        double elapsed = 0;
        
        START_BENCHMARK_TIMER();
        int result = reconstruct_secret_from_shares(shares, num_shares, reconstructed_data, &reconstructed_length);
        END_BENCHMARK_TIMER();
        
        if (result == 0 && reconstructed_length == TEST_DATA_SIZE) {
            // 验证重构的数据是否正确
            if (memcmp(test_data, reconstructed_data, TEST_DATA_SIZE) == 0) {
                success = 1;
                printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
            } else {
                printf("运行 %d: 数据不匹配\n", run + 1);
            }
        } else {
            printf("运行 %d: 重构失败, 错误码: %d\n", run + 1, result);
        }
        
        free(reconstructed_data);
        update_stats(&stats, elapsed, success);
    }
    
    free(test_data);
    print_stats("分片重构", &stats);
}

// 测试3: 网络传输性能（模拟）
void benchmark_network_simulation() {
    printf("\n开始测试网络传输性能（模拟）...\n");
    printf("数据大小: %d 字节\n", TEST_DATA_SIZE);
    printf("分片数量: %d\n", SECRET_SHARES);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    
    // 检查数据大小是否在限制内
    if (TEST_DATA_SIZE > MAX_TEST_SIZE) {
        printf("错误: 测试数据大小 %d 超过限制 %d\n", TEST_DATA_SIZE, MAX_TEST_SIZE);
        return;
    }
    
    benchmark_stats_t stats = {0};
    uint8_t* test_data = malloc(TEST_DATA_SIZE);
    if (!test_data) {
        printf("内存分配失败\n");
        return;
    }
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        // 生成测试数据
        generate_test_data(test_data, TEST_DATA_SIZE);
        
        // 创建分片（需要分配足够的空间）
        size_t num_blocks = (TEST_DATA_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        size_t max_shares = num_blocks * SECRET_SHARES;
        secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
        if (shares == NULL) {
            printf("运行 %d: 内存分配失败\n", run + 1);
            continue;
        }
        
        size_t num_shares = 0;
        int create_result = create_secret_shares(test_data, TEST_DATA_SIZE, shares, &num_shares);
        if (create_result != 0) {
            printf("运行 %d: 分片创建失败\n", run + 1);
            free(shares);
            update_stats(&stats, 0, 0);
            continue;
        }
        
        // 模拟网络传输（序列化和反序列化）
        int success = 0;
        double elapsed = 0;
        
        START_BENCHMARK_TIMER();
        
        // 模拟序列化所有分片
        for (size_t i = 0; i < num_shares; i++) {
            // 模拟网络延迟（微秒级）
            usleep(100); // 0.1ms 延迟
        }
        
        // 模拟反序列化
        secret_share_t* received_shares = (secret_share_t*)malloc(sizeof(secret_share_t) * num_shares);
        for (size_t i = 0; i < num_shares; i++) {
            received_shares[i] = shares[i];
        }
        // 使用received_shares避免未使用变量警告
        (void)received_shares;
        
        END_BENCHMARK_TIMER();
        
        success = 1;
        printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        update_stats(&stats, elapsed, success);
        free(shares);
        free(received_shares);
    }
    
    free(test_data);
    print_stats("网络传输（模拟）", &stats);
}

// 测试4: VSS承诺生成性能
void benchmark_vss_commitment() {
    printf("\n开始测试VSS承诺生成性能...\n");
    printf("数据大小: %d 字节\n", TEST_DATA_SIZE);
    printf("测试次数: %d\n", BENCHMARK_RUNS);
    
    // 检查数据大小是否在限制内
    if (TEST_DATA_SIZE > MAX_TEST_SIZE) {
        printf("错误: 测试数据大小 %d 超过限制 %d\n", TEST_DATA_SIZE, MAX_TEST_SIZE);
        return;
    }
    
    benchmark_stats_t stats = {0};
    uint8_t* test_data = malloc(TEST_DATA_SIZE);
    if (!test_data) {
        printf("内存分配失败\n");
        return;
    }
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        // 生成测试数据
        generate_test_data(test_data, TEST_DATA_SIZE);
        
        // 创建分片（需要分配足够的空间）
        size_t num_blocks = (TEST_DATA_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
        size_t max_shares = num_blocks * SECRET_SHARES;
        secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
        if (shares == NULL) {
            printf("运行 %d: 内存分配失败\n", run + 1);
            continue;
        }
        
        size_t num_shares = 0;
        int create_result = create_secret_shares(test_data, TEST_DATA_SIZE, shares, &num_shares);
        if (create_result != 0) {
            printf("运行 %d: 分片创建失败\n", run + 1);
            free(shares);
            update_stats(&stats, 0, 0);
            continue;
        }
        
        // 测试VSS承诺生成
        vss_commitment_t commitment;
        int success = 0;
        double elapsed = 0;
        
        START_BENCHMARK_TIMER();
        int result = create_vss_commitments(test_data, TEST_DATA_SIZE, shares, &commitment, "test_msgid");
        END_BENCHMARK_TIMER();
        
        if (result == 0) {
            success = 1;
            printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        } else {
            printf("运行 %d: 失败, 错误码: %d\n", run + 1, result);
        }
        
        update_stats(&stats, elapsed, success);
        free(shares);
    }
    
    free(test_data);
    print_stats("VSS承诺生成", &stats);
}

// 测试5: 不同数据大小的性能对比
void benchmark_different_sizes() {
    printf("\n开始测试不同数据大小的性能...\n");
    
    // 初始化CSV文件
    FILE *csv_file = fopen("benchmark_results.csv", "w");
    if (csv_file) {
        fprintf(csv_file, "DataSize,ShareTime,CommitmentTime,TotalTime,Throughput\n");
        fclose(csv_file);
    }
    
    int test_sizes[] = {1024, 2048, 4096, 8192, 15767,16384, 32768, 65536};
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for (int size_idx = 0; size_idx < num_sizes; size_idx++) {
        int data_size = test_sizes[size_idx];
        
        // 检查数据大小是否在限制内
        if (data_size > MAX_TEST_SIZE) {
            printf("\n--- 跳过数据大小: %d 字节 (超过限制 %d) ---\n", data_size, MAX_TEST_SIZE);
            continue;
        }
        
        printf("\n--- 测试数据大小: %d 字节 ---\n", data_size);
        
        benchmark_stats_t stats = {0};
        benchmark_stats_t share_stats = {0};  // 分片生成统计
        benchmark_stats_t commitment_stats = {0};  // 承诺生成统计
        uint8_t* test_data = malloc(data_size);
        if (!test_data) {
            printf("内存分配失败\n");
            continue;
        }
        
        for (int run = 0; run < 50; run++) { // 减少测试次数以节省时间
            generate_test_data(test_data, data_size);
            
            // 分配足够的shares空间
            size_t num_blocks = (data_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
            size_t max_shares = num_blocks * SECRET_SHARES;
            secret_share_t* shares = (secret_share_t*)malloc(sizeof(secret_share_t) * max_shares);
            if (shares == NULL) {
                printf("数据大小 %d: 内存分配失败\n", data_size);
                continue;
            }
            
            int success = 0;
            double share_elapsed = 0;
            double commitment_elapsed = 0;
            double total_elapsed = 0;
            double elapsed = 0; // 声明elapsed变量
            
            // 测试分片生成时间
            size_t num_shares = 0;
            START_BENCHMARK_TIMER();
            int share_result = create_secret_shares(test_data, data_size, shares, &num_shares);
            END_BENCHMARK_TIMER();
            share_elapsed = elapsed;
            update_stats(&share_stats, share_elapsed, share_result == 0);
            
            if (share_result == 0) {
                // 测试VSS承诺生成时间
                vss_commitment_t commitment;
                START_BENCHMARK_TIMER();
                int commitment_result = create_vss_commitments(test_data, data_size, shares, &commitment, "test_msgid");
                END_BENCHMARK_TIMER();
                commitment_elapsed = elapsed;
                update_stats(&commitment_stats, commitment_elapsed, commitment_result == 0);
                
                if (commitment_result == 0) {
                    success = 1;
                    total_elapsed = share_elapsed + commitment_elapsed;
                }
            }
            free(shares);
            
            update_stats(&stats, total_elapsed, success);
        }
        
        printf("数据大小: %d 字节\n", data_size);
        printf("总平均时间: %.3f ms\n", stats.avg_time);
        printf("分片生成平均时间: %.3f ms\n", share_stats.avg_time);
        printf("承诺生成平均时间: %.3f ms\n", commitment_stats.avg_time);
        printf("吞吐量: %.2f MB/s\n", (data_size / 1024.0 / 1024.0) / (stats.avg_time / 1000.0));
        
        // 创建输出目录
        // system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
        
        // 保存数据到CSV文件用于后续绘图
        FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/benchmark_results.csv", "a");
        if (csv_file) {
            // 检查文件是否为空，如果为空则写入列名
            fseek(csv_file, 0, SEEK_END);
            long file_size = ftell(csv_file);
            if (file_size == 0) {
                fprintf(csv_file, "DataSize,ShareTime,CommitmentTime,TotalTime,Throughput\n");
            }
            
            // 使用实际测量的时间
            fprintf(csv_file, "%d,%.3f,%.3f,%.3f,%.2f\n", data_size, share_stats.avg_time, commitment_stats.avg_time, stats.avg_time,
                   (data_size / 1024.0 / 1024.0) / (stats.avg_time / 1000.0));
            fclose(csv_file);
        }
        
        free(test_data);
    }
}

// 主函数
int main() {
    printf("========== 秘密分享性能测试工具 ==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("==========================================\n");
    
    // 初始化 relic 库
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    // 运行各种性能测试
    benchmark_share_creation();
    benchmark_share_reconstruction();
    benchmark_network_simulation();
    benchmark_vss_commitment();
    benchmark_different_sizes();
    
    printf("\n========== 所有测试完成 ==========\n");
    
    core_clean();
    return 0;
}
