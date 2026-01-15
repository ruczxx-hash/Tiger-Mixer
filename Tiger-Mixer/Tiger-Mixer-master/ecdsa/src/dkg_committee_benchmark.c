#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "util.h"

// 测试配置
#define BENCHMARK_RUNS 5  // 增加测试次数以获得更准确的性能数据
#define MIN_COMMITTEE_SIZE 3
#define MAX_COMMITTEE_SIZE 20  // 扩展到20个委员会成员
#define FIXED_THRESHOLD 2  // 固定阈值为2

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
    printf("成功率: %.2f%%\n", (double)stats->success_count / (stats->success_count + stats->fail_count) * 100);
    printf("最小时间: %.3f ms\n", stats->min_time);
    printf("最大时间: %.3f ms\n", stats->max_time);
    printf("平均时间: %.3f ms\n", stats->avg_time);
    printf("总时间: %.3f ms\n", stats->total_time);
    printf("吞吐量: %.2f 操作/秒\n", stats->success_count * 1000.0 / stats->total_time);
    printf("=====================================\n");
}

// 真实的DKG多项式生成性能测试（使用Class Group运算）
void benchmark_polynomial_generation(int committee_size, int threshold, benchmark_stats_t* stats) {
    printf("测试委员会大小 %d，阈值 %d 的多项式生成（Class Group版本）...\n", committee_size, threshold);
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        int success = 0;
        double elapsed = 0;
        
        printf("  [调试] 开始运行 %d/%d\n", run + 1, BENCHMARK_RUNS);
        START_BENCHMARK_TIMER();
        
        // 第1步：生成随机多项式系数
        printf("  [调试] 步骤1: 生成多项式系数\n");
        bn_t poly_coeffs[16]; // 最大阈值16
        bn_t order;
        bn_new(order);
        ec_curve_get_ord(order); // 使用secp256k1的阶
        
        for (int i = 0; i < threshold; i++) {
            bn_new(poly_coeffs[i]);
            // 生成随机多项式系数（使用完整的阶范围）
            bn_rand_mod(poly_coeffs[i], order);
        }
        printf("  [调试] 步骤1: 多项式系数生成完成\n");
        
        // 第2步：使用真实的Class Group生成元（2048位判别式）
        printf("  [调试] 步骤2: 创建真实Class Group生成元（2048位判别式）\n");
        // 使用与pedersen_dkg.c相同的真实参数
        GEN g_q_a = strtoi("4008431686288539256019978212352910132512184203702279780629385896624473051840259706993970111658701503889384191610389161437594619493081376284617693948914940268917628321033421857293703008209538182518138447355678944124861126384966287069011522892641935034510731734298233539616955610665280660839844718152071538201031396242932605390717004106131705164194877377");
        GEN g_q_b = negi(strtoi("3117991088204303366418764671444893060060110057237597977724832444027781815030207752301780903747954421114626007829980376204206959818582486516608623149988315386149565855935873517607629155593328578131723080853521348613293428202727746191856239174267496577422490575311784334114151776741040697808029563449966072264511544769861326483835581088191752567148165409"));
        GEN g_q_c = strtoi("7226982982667784284607340011220616424554394853592495056851825214613723615410492468400146084481943091452495677425649405002137153382700126963171182913281089395393193450415031434185562111748472716618186256410737780813669746598943110785615647848722934493732187571819575328802273312361412673162473673367423560300753412593868713829574117975260110889575205719");
        GEN g_q_temp = Qfb0(g_q_a, g_q_b, g_q_c);
        GEN g_q = qfbred(g_q_temp);  // 约化到标准形式
        printf("  [调试] 步骤2: 真实Class Group生成元创建完成\n");
        
        // 第3步：计算Class Group承诺（真实的nupow运算）
        printf("  [调试] 步骤3: 计算Class Group承诺\n");
        GEN commitments[16];
        for (int i = 0; i < threshold; i++) {
            char coeff_str[256];
            bn_write_str(coeff_str, sizeof(coeff_str), poly_coeffs[i], 10);
            GEN a_ij = strtoi(coeff_str);
            
            // 使用avma保护栈
            pari_sp av = avma;
            GEN commitment_temp = nupow(g_q, a_ij, NULL);
            commitments[i] = gclone(commitment_temp);
            avma = av;
        }
        printf("  [调试] 步骤3: Class Group承诺计算完成\n");
        
        // 第4步：计算份额（使用Horner方法）
        printf("  [调试] 步骤4: 计算份额\n");
        for (int j = 1; j <= committee_size; j++) {
            bn_t share;
            bn_new(share);
            
            // 使用Horner方法计算 s_{i,j} = f_i(j)
            bn_copy(share, poly_coeffs[threshold - 1]);
            
            bn_t j_bn;
            bn_new(j_bn);
            bn_set_dig(j_bn, j);
            
            for (int k = threshold - 2; k >= 0; k--) {
                bn_mul(share, share, j_bn);
                bn_add(share, share, poly_coeffs[k]);
                bn_mod(share, share, order);
            }
            
            bn_free(share);
            bn_free(j_bn);
        }
        printf("  [调试] 步骤4: 份额计算完成\n");
        
        // 清理资源
        printf("  [调试] 步骤5: 清理资源\n");
        for (int i = 0; i < threshold; i++) {
            bn_free(poly_coeffs[i]);
            gunclone(commitments[i]);
        }
        bn_free(order);
        gunclone(g_q);
        printf("  [调试] 步骤5: 资源清理完成\n");
        
        END_BENCHMARK_TIMER();
        
        success = 1;
        printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        update_stats(stats, elapsed, success);
    }
}
    
// 真实的DKG承诺生成性能测试（使用Class Group运算）
void benchmark_commitment_generation(int committee_size, int threshold, benchmark_stats_t* stats) {
    printf("测试委员会大小 %d，阈值 %d 的承诺生成（Class Group版本）...\n", committee_size, threshold);
    
    for (int run = 0; run < BENCHMARK_RUNS; run++) {
        int success = 0;
        double elapsed = 0;
        
        printf("  [调试] 开始运行 %d/%d\n", run + 1, BENCHMARK_RUNS);
        START_BENCHMARK_TIMER();
        
        // 第1步：生成多项式系数
        printf("  [调试] 步骤1: 生成多项式系数\n");
        bn_t poly_coeffs[16];
        bn_t order;
        bn_new(order);
        ec_curve_get_ord(order);
        
        for (int i = 0; i < threshold; i++) {
            bn_new(poly_coeffs[i]);
            bn_rand_mod(poly_coeffs[i], order);
        }
        printf("  [调试] 步骤1: 多项式系数生成完成\n");
        
        // 第2步：使用真实的Class Group生成元（2048位判别式）
        printf("  [调试] 步骤2: 创建真实Class Group生成元（2048位判别式）\n");
        // 使用与pedersen_dkg.c相同的真实参数
        GEN g_q_a = strtoi("4008431686288539256019978212352910132512184203702279780629385896624473051840259706993970111658701503889384191610389161437594619493081376284617693948914940268917628321033421857293703008209538182518138447355678944124861126384966287069011522892641935034510731734298233539616955610665280660839844718152071538201031396242932605390717004106131705164194877377");
        GEN g_q_b = negi(strtoi("3117991088204303366418764671444893060060110057237597977724832444027781815030207752301780903747954421114626007829980376204206959818582486516608623149988315386149565855935873517607629155593328578131723080853521348613293428202727746191856239174267496577422490575311784334114151776741040697808029563449966072264511544769861326483835581088191752567148165409"));
        GEN g_q_c = strtoi("7226982982667784284607340011220616424554394853592495056851825214613723615410492468400146084481943091452495677425649405002137153382700126963171182913281089395393193450415031434185562111748472716618186256410737780813669746598943110785615647848722934493732187571819575328802273312361412673162473673367423560300753412593868713829574117975260110889575205719");
        GEN g_q_temp = Qfb0(g_q_a, g_q_b, g_q_c);
        GEN g_q = qfbred(g_q_temp);  // 约化到标准形式
        printf("  [调试] 步骤2: 真实Class Group生成元创建完成\n");
        
        // 第3步：计算Class Group承诺
        printf("  [调试] 步骤3: 计算Class Group承诺\n");
        GEN commitments[16];
        for (int i = 0; i < threshold; i++) {
            char coeff_str[256];
            bn_write_str(coeff_str, sizeof(coeff_str), poly_coeffs[i], 10);
            GEN a_ij = strtoi(coeff_str);
            
            pari_sp av = avma;
            GEN commitment_temp = nupow(g_q, a_ij, NULL);
            commitments[i] = gclone(commitment_temp);
            avma = av;
        }
        printf("  [调试] 步骤3: Class Group承诺计算完成\n");
        
        // 第4步：计算份额（不包含验证，只生成份额）
        printf("  [调试] 步骤4: 计算份额\n");
        for (int j = 1; j <= committee_size; j++) {
            bn_t share;
            bn_new(share);
            
            // 使用Horner方法计算份额
            bn_copy(share, poly_coeffs[threshold - 1]);
            
            bn_t j_bn;
            bn_new(j_bn);
            bn_set_dig(j_bn, j);
            
            for (int k = threshold - 2; k >= 0; k--) {
                bn_mul(share, share, j_bn);
                bn_add(share, share, poly_coeffs[k]);
                bn_mod(share, share, order);
            }
            
            bn_free(share);
            bn_free(j_bn);
        }
        printf("  [调试] 步骤4: 份额计算完成\n");
        
        // 清理资源
        printf("  [调试] 步骤5: 清理资源\n");
        for (int i = 0; i < threshold; i++) {
            bn_free(poly_coeffs[i]);
            gunclone(commitments[i]);
        }
        bn_free(order);
        gunclone(g_q);
        printf("  [调试] 步骤5: 资源清理完成\n");
        
        END_BENCHMARK_TIMER();
        
        success = 1;
        printf("运行 %d: 成功, 时间: %.3f ms\n", run + 1, elapsed);
        update_stats(stats, elapsed, success);
    }
}

// 主函数
int main() {
    printf("========== DKG委员会大小性能测试工具（Class Group版本）==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("测试范围: 委员会大小 %d-%d\n", MIN_COMMITTEE_SIZE, MAX_COMMITTEE_SIZE);
    printf("固定阈值: %d\n", FIXED_THRESHOLD);
    printf("注意: 这是真实的DKG性能测试，使用Class Group运算\n");
    printf("=====================================\n");
    
    // 初始化 relic 库
    if (core_init() != RLC_OK) {
        printf("relic 库初始化失败\n");
        return 1;
    }
    
    // 初始化 PARI 库（Class Group DKG 需要）
    // 参考 secret_share_receiver.c 的初始化方式
    printf("初始化 PARI 库用于 Class Group 运算...\n");
    pari_init(500000000, 2);  // 500MB 栈空间，与 secret_share_receiver.c 保持一致
    setrand(getwalltime());
    printf("PARI 库初始化成功（500MB 栈空间）\n");
    
    // 设置配对参数（参考 secret_share_receiver.c 的初始化顺序）
    if (pc_param_set_any() != RLC_OK) {
        printf("PC parameter setting failed\n");
        pari_close();
        core_clean();
        return 1;
    }
    
    // 设置 secp256k1 曲线
    if (ec_param_set_any() != RLC_OK) {
        printf("EC parameter setting failed\n");
        pari_close();
        core_clean();
        return 1;
    }
    ep_param_set(SECG_K256);
    
    // 初始化配对核心（参考 secret_share_receiver.c）
    pc_core_init();
    printf("密码学库初始化成功\n");
    
    // 创建输出目录
    int mkdir_result = system("mkdir -p /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test");
    (void)mkdir_result; // 忽略返回值警告
    
    // 打开CSV文件
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_simple_benchmark.csv", "w");
    if (!csv_file) {
        printf("无法创建CSV文件\n");
        core_clean();
        return 1;
    }
    
    // 写入CSV头部
    fprintf(csv_file, "CommitteeSize,Threshold,CommitmentGenerationTime,ShareGenerationTime,TotalTime,Throughput\n");
    
    // 测试不同委员会大小
    for (int committee_size = MIN_COMMITTEE_SIZE; committee_size <= MAX_COMMITTEE_SIZE; committee_size++) {
        int threshold = FIXED_THRESHOLD; // 固定阈值为2
        
        printf("\n========== 测试委员会大小: %d, 阈值: %d ==========\n", 
               committee_size, threshold);
        
        // 测试承诺生成（包含多项式系数生成和Class Group承诺计算）
        benchmark_stats_t commitment_stats = {0};
        benchmark_polynomial_generation(committee_size, threshold, &commitment_stats);
        print_stats("承诺生成（多项式系数+Class Group承诺）", &commitment_stats);
        
        // 测试份额生成（包含多项式系数生成、Class Group承诺计算和份额计算）
        benchmark_stats_t share_stats = {0};
        benchmark_commitment_generation(committee_size, threshold, &share_stats);
        print_stats("份额生成（包含承诺+份额计算）", &share_stats);
        
        // 计算总时间
        double total_time = commitment_stats.avg_time + share_stats.avg_time;
        double throughput = committee_size * 1000.0 / total_time; // 参与者数/秒
        
        // 写入CSV
        fprintf(csv_file, "%d,%d,%.3f,%.3f,%.3f,%.2f\n", 
                committee_size, threshold, 
                commitment_stats.avg_time, share_stats.avg_time, 
                total_time, throughput);
        
        printf("委员会大小: %d, 阈值: %d, 承诺生成: %.3f ms, 份额生成: %.3f ms, 总时间: %.3f ms\n",
               committee_size, threshold, commitment_stats.avg_time, share_stats.avg_time, total_time);
    }
    
    fclose(csv_file);
    
    printf("\n========== 所有测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_simple_benchmark.csv\n");
    
    // 清理资源
    pari_close();
    core_clean();
    return 0;
}
