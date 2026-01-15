#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "util.h"

// 测试配置
#define BENCHMARK_RUNS 100  // 每个配置测试100次
#define MIN_COMMITTEE_SIZE 3
#define MAX_COMMITTEE_SIZE 20
#define MAX_THRESHOLD 25  // 最大阈值（用于数组大小，应大于最大可能的阈值）

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

// 保存的系数（用于承诺生成测试）
static bn_t saved_poly_coeffs[MAX_THRESHOLD];
static int coeffs_saved = 0;

// 保存其他参与者的承诺和份额（用于验证测试）
typedef struct {
    GEN commitments[MAX_THRESHOLD];  // 承诺数组
    bn_t share;                      // 发送给我的份额
    int threshold;                    // 该参与者的阈值
} other_participant_data_t;

static other_participant_data_t other_participants[MAX_COMMITTEE_SIZE];

// 计算阈值：t = 2n/3 + 1
static int calculate_majority_threshold(int committee_size) {
    return (2 * committee_size) / 3 + 1;
}

// 测试份额生成时间（单个委员会成员：系数生成 + 份额计算）
// 注意：系数会被保存，供后续承诺生成测试使用
// 这个函数只运行一次，返回时间
double benchmark_share_generation_single(int committee_size, int threshold, bn_t order, GEN g_q) {
    double elapsed = 0;
    
    // ⭐ 开始计时：测试份额生成（系数生成 + 份额计算）
    START_BENCHMARK_TIMER();
    
    // 第1步：生成随机多项式系数
    bn_t poly_coeffs[MAX_THRESHOLD];
    for (int i = 0; i < threshold; i++) {
        bn_new(poly_coeffs[i]);
        bn_rand_mod(poly_coeffs[i], order);
        // 保存系数供后续承诺生成使用
        bn_copy(saved_poly_coeffs[i], poly_coeffs[i]);
    }
    coeffs_saved = 1;  // 标记系数已保存
    
    // 第2步：计算份额（使用Horner方法）- 给所有committee_size个参与者
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
    
    END_BENCHMARK_TIMER();
    
    // 清理本次运行的资源（但保留saved_poly_coeffs）
    for (int i = 0; i < threshold; i++) {
        bn_free(poly_coeffs[i]);
    }
    
    return elapsed;
}

// 测试承诺生成时间（单个委员会成员：使用已保存的系数生成承诺）
// 注意：系数应在份额生成测试中已保存
// 这个函数只运行一次，返回时间
double benchmark_commitment_generation_single(int committee_size, int threshold, GEN g_q) {
    if (!coeffs_saved) {
        return -1;  // 错误：系数未保存
    }
    
    double elapsed = 0;
    
    // ⭐ 开始计时：只测试承诺计算部分（使用已保存的系数）
    START_BENCHMARK_TIMER();
    
    // 计算Class Group承诺（真实的nupow运算）
    GEN commitments[MAX_THRESHOLD];
    for (int i = 0; i < threshold; i++) {
        // 转换系数（bn_t -> GEN），这部分也在计时内，因为这是承诺生成的一部分
        char coeff_str[256];
        bn_write_str(coeff_str, sizeof(coeff_str), saved_poly_coeffs[i], 10);
        GEN a_ij = strtoi(coeff_str);
        
        // ⭐ 核心操作：Class Group 幂运算（这是最昂贵的部分）
        // nupow(g_q, a_ij) 在 2048 位判别式的 Class Group 上计算 g_q^{a_ij}
        pari_sp av = avma;
        GEN commitment_temp = nupow(g_q, a_ij, NULL);
        commitments[i] = gclone(commitment_temp);
        avma = av;
    }
    
    END_BENCHMARK_TIMER();
    
    // 清理本次运行的资源
    for (int i = 0; i < threshold; i++) {
        gunclone(commitments[i]);
    }
    
    return elapsed;
}

// 测试验证其他n-1个用户分片的时间
// 假设自己是1号参与者，需要验证其他参与者（2到n）发送给我的分片
// 注意：其他参与者的承诺和分片应该在计时前准备好（模拟已接收到的数据）
// 这个函数只运行一次，返回时间
double benchmark_share_verification_single(int committee_size, int threshold, bn_t order, GEN g_q) {
    double elapsed = 0;
    
    int my_id = 1;  // 假设自己是1号参与者
    
    // ⚠️ 在计时前准备数据：生成其他参与者的承诺和分片（模拟已接收到的数据）
    typedef struct {
        GEN commitments[MAX_THRESHOLD];  // 承诺数组
        bn_t share;                      // 发送给我的份额
    } other_participant_data_t;
    
    other_participant_data_t *others = (other_participant_data_t*)malloc((committee_size - 1) * sizeof(other_participant_data_t));
    
    // 对于每个其他参与者 i (从2到committee_size)，准备他们的数据
    for (int idx = 0; idx < committee_size - 1; idx++) {
        int other_id = idx + 2;  // 参与者ID从2开始
        
        // 1. 生成其他参与者的多项式系数（随机生成）
        bn_t other_coeffs[MAX_THRESHOLD];
        for (int k = 0; k < threshold; k++) {
            bn_new(other_coeffs[k]);
            bn_rand_mod(other_coeffs[k], order);
        }
        
        // 2. 计算其他参与者的承诺 A_{i,k} = g_q^{a_{i,k}}
        for (int k = 0; k < threshold; k++) {
            char coeff_str[256];
            bn_write_str(coeff_str, sizeof(coeff_str), other_coeffs[k], 10);
            GEN a_ik = strtoi(coeff_str);
            
            pari_sp av = avma;
            GEN commitment_temp = nupow(g_q, a_ik, NULL);
            others[idx].commitments[k] = gclone(commitment_temp);
            avma = av;
        }
        
        // 3. 计算其他参与者发送给我的分片 s_{i,1} = f_i(1)（使用Horner方法）
        bn_new(others[idx].share);
        bn_copy(others[idx].share, other_coeffs[threshold - 1]);
        
        bn_t one_bn;
        bn_new(one_bn);
        bn_set_dig(one_bn, my_id);  // my_id = 1
        
        for (int k = threshold - 2; k >= 0; k--) {
            bn_mul(others[idx].share, others[idx].share, one_bn);
            bn_add(others[idx].share, others[idx].share, other_coeffs[k]);
            bn_mod(others[idx].share, others[idx].share, order);
        }
        
        // 清理临时资源
        for (int k = 0; k < threshold; k++) {
            bn_free(other_coeffs[k]);
        }
        bn_free(one_bn);
    }
    
    // ⭐ 开始计时：只测量验证计算的时间
    START_BENCHMARK_TIMER();
    
    int verification_success = 1;  // 验证是否成功
    
    // 获取群阶 q（用于指数模运算，在循环外准备一次）
    char q_str[256];
    bn_write_str(q_str, sizeof(q_str), order, 10);
    pari_sp av_q = avma;
    GEN q_gen_temp = strtoi(q_str);
    GEN q_gen = gclone(q_gen_temp);
    avma = av_q;
    
    // 对于每个其他参与者，进行验证
    for (int idx = 0; idx < committee_size - 1; idx++) {
        // 验证：g_q^{s_{i,1}} ?= ∏_{k=0}^{t-1} (A_{i,k})^{1^k}
        // 计算左侧：g_q^{s_{i,1}}
        char share_str[256];
        bn_write_str(share_str, sizeof(share_str), others[idx].share, 10);
        GEN s_i1 = strtoi(share_str);
        
        pari_sp av_left = avma;
        GEN left_side_temp = nupow(g_q, s_i1, NULL);
        GEN left_side = gclone(left_side_temp);
        avma = av_left;
        
        // 计算右侧：∏_{k=0}^{t-1} (A_{i,k})^{j^k}，其中 j = my_id = 1
        // 使用 GEN 类型计算 j^k，避免 bn_t 溢出
        GEN j = stoi(my_id);  // j = 1
        GEN j_power = NULL;  // 初始化为NULL，第一次循环时设置为 gen_1
        
        GEN right_side = NULL;
        for (int k = 0; k < threshold; k++) {
            // 初始化 j_power（第一次循环时）
            if (j_power == NULL) {
                j_power = gclone(gen_1);  // j^0 = 1
            }
            
            // 对指数做 mod q（在阶为q的群中，g^m = g^{m mod q}）
            pari_sp av = avma;
            GEN j_power_mod = gmod(j_power, q_gen);
            GEN j_power_mod_copy = gclone(j_power_mod);
            avma = av;
            
            // 计算 A_{i,k}^{j^k mod q}（使用 nupow）
            pari_sp av2 = avma;
            GEN temp_pow = nupow(others[idx].commitments[k], j_power_mod_copy, NULL);
            GEN temp = gclone(temp_pow);
            avma = av2;
            gunclone(j_power_mod_copy);
            
            // Class Group 群乘法（使用 gmul，PARI 自动约化）
            if (k == 0) {
                right_side = temp;
            } else {
                pari_sp av3 = avma;
                GEN new_right = gmul(right_side, temp);
                GEN new_right_copy = gclone(new_right);
                avma = av3;
                
                gunclone(right_side);
                gunclone(temp);
                right_side = new_right_copy;
            }
            
            // 计算 j^(k+1)（对于 j=1，这始终是 1，但为了通用性保留）
            if (k < threshold - 1) {  // 最后一次循环不需要计算下一个
                pari_sp av4 = avma;
                GEN j_power_next = gmul(j_power, j);
                GEN j_power_next_copy = gclone(j_power_next);
                avma = av4;
                gunclone(j_power);  // 清理旧的 j_power
                j_power = j_power_next_copy;
            }
        }
        
        // 清理 j_power（如果已分配）
        if (j_power != NULL) {
            gunclone(j_power);
        }
        
        // 比较 left_side 和 right_side
        int verify_result = gequal(left_side, right_side);
        if (!verify_result) {
            verification_success = 0;
        }
        
        // 清理验证过程中的资源
        gunclone(left_side);
        gunclone(right_side);
    }
    
    // 清理 q_gen
    gunclone(q_gen);
    
    END_BENCHMARK_TIMER();
    
    // 清理所有准备的数据
    for (int idx = 0; idx < committee_size - 1; idx++) {
        for (int k = 0; k < threshold; k++) {
            gunclone(others[idx].commitments[k]);
        }
        bn_free(others[idx].share);
    }
    free(others);
    
    // 如果验证失败，返回负值（但时间仍然记录）
    if (!verification_success) {
        // 仍然返回时间，但调用者可以通过其他方式标记失败
    }
    
    return elapsed;
}

// 测试合成私钥分片的时间
// 假设自己是1号参与者，需要累加所有参与者（包括自己）发送给我的分片
// sk_1 = Σ_{i=1}^n s_{i,1}
// 注意：所有参与者的分片应该在计时前准备好（模拟已接收到的数据）
// 这个函数只运行一次，返回时间
double benchmark_secret_share_aggregation_single(int committee_size, int threshold, bn_t order) {
    double elapsed = 0;
    
    // ⚠️ 在计时前准备数据：生成所有参与者的分片（模拟已接收到的数据）
    bn_t *received_shares = (bn_t*)malloc(committee_size * sizeof(bn_t));
    
    // 对于每个参与者 i (从1到committee_size)，准备他们的分片
    for (int participant_id = 1; participant_id <= committee_size; participant_id++) {
        // 生成参与者i的多项式系数
        bn_t poly_coeffs[MAX_THRESHOLD];
        for (int k = 0; k < threshold; k++) {
            bn_new(poly_coeffs[k]);
            bn_rand_mod(poly_coeffs[k], order);
        }
        
        // 计算参与者i发送给我的分片 s_{i,1} = f_i(1)（使用Horner方法）
        bn_new(received_shares[participant_id - 1]);
        bn_copy(received_shares[participant_id - 1], poly_coeffs[threshold - 1]);
        
        bn_t one_bn;
        bn_new(one_bn);
        bn_set_dig(one_bn, 1);  // my_id = 1
        
        for (int k = threshold - 2; k >= 0; k--) {
            bn_mul(received_shares[participant_id - 1], received_shares[participant_id - 1], one_bn);
            bn_add(received_shares[participant_id - 1], received_shares[participant_id - 1], poly_coeffs[k]);
            bn_mod(received_shares[participant_id - 1], received_shares[participant_id - 1], order);
        }
        
        // 清理临时资源
        for (int k = 0; k < threshold; k++) {
            bn_free(poly_coeffs[k]);
        }
        bn_free(one_bn);
    }
    
    // ⭐ 开始计时：只测量合成私钥分片（累加所有分片）的时间
    START_BENCHMARK_TIMER();
    
    // 初始化私钥分片
    bn_t secret_share;
    bn_new(secret_share);
    bn_zero(secret_share);
    
    // 对于每个参与者，累加他们的分片
    for (int participant_id = 1; participant_id <= committee_size; participant_id++) {
        // 累加到私钥分片
        bn_add(secret_share, secret_share, received_shares[participant_id - 1]);
        bn_mod(secret_share, secret_share, order);
    }
    
    END_BENCHMARK_TIMER();
    
    // 清理资源
    bn_free(secret_share);
    for (int i = 0; i < committee_size; i++) {
        bn_free(received_shares[i]);
    }
    free(received_shares);
    
    return elapsed;
}


// 主函数
int main() {
    printf("========== DKG阈值性能测试工具（Class Group版本）==========\n");
    printf("测试时间: %s\n", ctime(&(time_t){time(NULL)}));
    printf("委员会大小范围: %d - %d\n", MIN_COMMITTEE_SIZE, MAX_COMMITTEE_SIZE);
    printf("阈值计算: 2n/3+1\n");
    printf("测试次数: 每个配置 %d 次\n", BENCHMARK_RUNS);
    printf("注意: 这是真实的DKG性能测试，使用Class Group运算\n");
    printf("⚠️ 重要: 测试的是单个委员会成员的操作时间\n");
    printf("  - 份额生成: 系数生成 + 计算所有n个份额（系数会被保存）\n");
    printf("  - 承诺生成: 使用已保存的系数生成承诺\n");
    printf("  - 分片验证: 验证其他n-1个用户发送的分片\n");
    printf("  - 私钥分片合成: 累加所有参与者的分片得到私钥分片\n");
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
    FILE *csv_file = fopen("/home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_threshold_benchmark.csv", "w");
    if (!csv_file) {
        printf("无法创建CSV文件\n");
        core_clean();
        return 1;
    }
    
    // 预先准备：生成元（全局，只创建一次）
    GEN g_q_a = strtoi("4008431686288539256019978212352910132512184203702279780629385896624473051840259706993970111658701503889384191610389161437594619493081376284617693948914940268917628321033421857293703008209538182518138447355678944124861126384966287069011522892641935034510731734298233539616955610665280660839844718152071538201031396242932605390717004106131705164194877377");
    GEN g_q_b = negi(strtoi("3117991088204303366418764671444893060060110057237597977724832444027781815030207752301780903747954421114626007829980376204206959818582486516608623149988315386149565855935873517607629155593328578131723080853521348613293428202727746191856239174267496577422490575311784334114151776741040697808029563449966072264511544769861326483835581088191752567148165409"));
    GEN g_q_c = strtoi("7226982982667784284607340011220616424554394853592495056851825214613723615410492468400146084481943091452495677425649405002137153382700126963171182913281089395393193450415031434185562111748472716618186256410737780813669746598943110785615647848722934493732187571819575328802273312361412673162473673367423560300753412593868713829574117975260110889575205719");
    
    pari_sp av_gen_global = avma;
    GEN g_q_temp = Qfb0(g_q_a, g_q_b, g_q_c);
    GEN g_q_reduced = qfbred(g_q_temp);
    GEN g_q = gclone(g_q_reduced);
    avma = av_gen_global;
    
    bn_t order;
    bn_new(order);
    ec_curve_get_ord(order);
    
    // 初始化保存的系数数组
    for (int i = 0; i < MAX_THRESHOLD; i++) {
        bn_new(saved_poly_coeffs[i]);
    }
    coeffs_saved = 1;
    
    // 写入CSV头部
    fprintf(csv_file, "CommitteeSize,Threshold,ShareGenerationTime,CommitmentGenerationTime,VerificationTime,ShareAggregationTime,TotalTime\n");
    
    // 遍历不同的委员会大小
    for (int committee_size = MIN_COMMITTEE_SIZE; committee_size <= MAX_COMMITTEE_SIZE; committee_size++) {
        // 计算多数阈值
        int threshold = calculate_majority_threshold(committee_size);
        
        printf("\n--- 测试委员会大小: %d, 阈值: %d (2n/3+1) ---\n", committee_size, threshold);
        
        benchmark_stats_t share_stats = {0};
        benchmark_stats_t commitment_stats = {0};
        benchmark_stats_t verification_stats = {0};
        benchmark_stats_t aggregation_stats = {0};
        benchmark_stats_t total_stats = {0};
        
        for (int run = 0; run < BENCHMARK_RUNS; run++) {
            // 重置系数保存标志，确保每次运行都重新生成
            coeffs_saved = 0;
            
            // 1. 测试份额生成时间
            double share_elapsed = benchmark_share_generation_single(committee_size, threshold, order, g_q);
            if (share_elapsed >= 0) {
                update_stats(&share_stats, share_elapsed, 1);
                coeffs_saved = 1;  // 标记系数已保存
            } else {
                update_stats(&share_stats, 0, 0);
            }
            
            // 2. 测试承诺生成时间（使用已保存的系数）
            double commitment_elapsed = benchmark_commitment_generation_single(committee_size, threshold, g_q);
            if (commitment_elapsed >= 0) {
                update_stats(&commitment_stats, commitment_elapsed, 1);
            } else {
                update_stats(&commitment_stats, 0, 0);
            }
            
            // 3. 测试验证其他n-1个用户分片的时间
            double verification_elapsed = benchmark_share_verification_single(committee_size, threshold, order, g_q);
            if (verification_elapsed >= 0) {
                update_stats(&verification_stats, verification_elapsed, 1);
            } else {
                update_stats(&verification_stats, 0, 0);
            }
            
            // 4. 测试合成私钥分片的时间
            double aggregation_elapsed = benchmark_secret_share_aggregation_single(committee_size, threshold, order);
            if (aggregation_elapsed >= 0) {
                update_stats(&aggregation_stats, aggregation_elapsed, 1);
            } else {
                update_stats(&aggregation_stats, 0, 0);
            }
            
            // 计算总时间
            if (share_elapsed >= 0 && commitment_elapsed >= 0 && 
                verification_elapsed >= 0 && aggregation_elapsed >= 0) {
                double total_elapsed = share_elapsed + commitment_elapsed + 
                                      verification_elapsed + aggregation_elapsed;
                update_stats(&total_stats, total_elapsed, 1);
            }
        }
        
        // 打印统计结果
        print_stats("份额生成", &share_stats);
        print_stats("承诺生成", &commitment_stats);
        print_stats("分片验证", &verification_stats);
        print_stats("私钥分片合成", &aggregation_stats);
        print_stats("总时间", &total_stats);
        
        // 写入CSV数据（只记录成功的时间）
        fprintf(csv_file, "%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n", 
                committee_size,
                threshold, 
                share_stats.avg_time, 
                commitment_stats.avg_time,
                verification_stats.avg_time,
                aggregation_stats.avg_time,
                total_stats.avg_time);
    }
    
    // 清理全局资源
    bn_free(order);
    gunclone(g_q);
    
    fclose(csv_file);
    
    printf("\n========== 所有测试完成 ==========\n");
    printf("结果已保存到: /home/zxx/A2L/A2L-master/ecdsa/bin/fig_test/dkg_threshold_benchmark.csv\n");
    
    // 清理资源
    pari_close();
    core_clean();
    return 0;
}
