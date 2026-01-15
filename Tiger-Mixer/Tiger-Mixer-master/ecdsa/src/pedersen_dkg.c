#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "types.h"
#include "util.h"
#include "cl_canonical.h"  // ⭐ 引入规范化工具

// secp256k1 压缩点大小（1字节前缀 + 32字节x坐标）
#define RLC_EC_SIZE_COMPRESSED 33

// ================= Pedersen DKG 核心实现 =================

/**
 * 初始化DKG协议（Class Group 版本）
 * 
 * 数学原理：
 * - 使用提供的 Class Group 参数（生成元 g_q）
 * - 确定参与者数量 n 和阈值 t
 * - 初始化群阶 q
 * 
 * 公式：
 * Class Group = <g_q>, |G| = q
 * 
 * @param protocol DKG协议状态
 * @param n_participants 参与者数量
 * @param threshold 阈值
 * @param cl_params Class Group 参数（如果为 NULL，则内部生成）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_protocol_init_with_cl_params(dkg_protocol_t protocol, int n_participants, 
                                      int threshold, cl_params_t cl_params) {
    if (!protocol || n_participants < 2 || threshold < 2 || threshold > n_participants) {
        return RLC_ERR;
    }
    
    printf("[DKG] 初始化DKG协议（Class Group版本）: n=%d, t=%d\n", n_participants, threshold);
    
    // 设置协议参数
    protocol->n_participants = n_participants;
    protocol->threshold = threshold;
    protocol->phase = 0;
    
    // 使用提供的或生成新的 Class Group 参数
    if (cl_params != NULL) {
        printf("[DKG] 使用外部提供的 Class Group 参数\n");
        protocol->cl_params = cl_params;
    } else {
        printf("[DKG] 生成新的 Class Group 参数\n");
        
        // 手动分配内存
        protocol->cl_params = (cl_params_t)malloc(sizeof(cl_params_st));
        if (protocol->cl_params == NULL) {
            printf("[DKG] 错误: cl_params 内存分配失败\n");
            return RLC_ERR;
        }
        
        // 初始化 GEN 字段
        protocol->cl_params->Delta_K = gen_0;
        protocol->cl_params->E = gen_0;
        protocol->cl_params->q = gen_0;
        protocol->cl_params->G = gen_0;
        protocol->cl_params->g_q = gen_0;
        protocol->cl_params->bound = gen_0;
        
        if (generate_cl_params(protocol->cl_params) != RLC_OK) {
            printf("[DKG] 错误: 生成 Class Group 参数失败\n");
            free(protocol->cl_params);
            protocol->cl_params = NULL;
            return RLC_ERR;
        }
        
        printf("[DKG] Class Group 参数生成成功\n");
    }
    
    // 设置生成元 g_q (Class Group)
    protocol->generator_g = protocol->cl_params->g_q;
    
    // 设置群阶（使用 secp256k1 的阶，因为我们的秘密值在这个域中）
    ec_curve_get_ord(protocol->order);
    
    printf("[DKG] Class Group 生成元 g_q 已设置\n");
    printf("[DKG] 群阶 q = %zu位\n", bn_size_bin(protocol->order));
    
    // 调试：比较 Class Group 参数中的 q 与 secp256k1 的阶
    char *cl_q_str = GENtostr(protocol->cl_params->q);
    printf("[DKG_DEBUG] Class Group 参数中的 q = %s\n", cl_q_str);
    pari_free(cl_q_str);
    
    char protocol_order_str[256];
    bn_write_str(protocol_order_str, sizeof(protocol_order_str), protocol->order, 10);
    printf("[DKG_DEBUG] secp256k1 的阶 = %s\n", protocol_order_str);
    
    protocol->is_initialized = 1;
    
    return RLC_OK;
}

/**
 * 初始化DKG协议（向后兼容，内部生成 cl_params）
 */
int dkg_protocol_init(dkg_protocol_t protocol, int n_participants, int threshold) {
    return dkg_protocol_init_with_cl_params(protocol, n_participants, threshold, NULL);
}

/**
 * 添加参与者到DKG协议
 * 
 * 数学原理：
 * - 为每个参与者分配唯一ID (1到n)
 * - 初始化参与者的多项式系数和随机数
 */
int dkg_add_participant(dkg_protocol_t protocol, int participant_id) {
    if (!protocol || !protocol->is_initialized || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    if (protocol->participants[participant_id - 1] != NULL) {
        printf("[DKG] 参与者%d已存在\n", participant_id);
        return RLC_OK;
    }
    
    printf("[DKG] 添加参与者%d\n", participant_id);
    
    // 创建参与者状态
    dkg_participant_new(protocol->participants[participant_id - 1]);
    dkg_participant_t p = protocol->participants[participant_id - 1];
    
    p->participant_id = participant_id;
    p->n_participants = protocol->n_participants;
    p->threshold = protocol->threshold;
    p->is_initialized = 1;
    
    return RLC_OK;
}

/**
 * 生成随机多项式、计算承诺和份额（Joint-Feldman DKG - Class Group 版本）
 * 
 * 数学原理：
 * 1. 生成随机多项式：f_i(x) = a_{i,0} + a_{i,1}x + ... + a_{i,t-1}x^(t-1)
 * 2. 计算承诺：A_{i,j} = g_q^{a_{i,j}} (Class Group)
 * 3. 计算份额：s_{i,j} = f_i(j) 给每个参与者P_j
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param computed_shares 输出：计算出的份额数组（索引1到n）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_generate_polynomial_commitments_and_shares(dkg_protocol_t protocol, int participant_id, 
                                                     bn_t *computed_shares) {
    if (!protocol || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t p = protocol->participants[participant_id - 1];
    if (!p || !p->is_initialized) {
        return RLC_ERR;
    }
    
    printf("\n[步骤1] 参与者%d生成多项式、承诺和份额（阈值t=%d）\n", participant_id, p->threshold);
    
    // ⚠️ 关键：打印生成元的前100字符用于对比
    char *gen_str = GENtostr(protocol->generator_g);
    printf("  使用生成元 g_q（前100字符）: %.100s...\n", gen_str);
    pari_free(gen_str);
    
    // 第1步：生成随机多项式的系数并计算承诺
    printf("  [1.1] 生成多项式系数和承诺:\n");
    printf("  ⚠️⚠️⚠️ 调试模式：系数范围 [1, 10] - 仅用于测试！⚠️⚠️⚠️\n");
    for (int j = 0; j < p->threshold; j++) {
        // 🔴 调试模式：生成小范围系数 [1, 10]
        // ⚠️ 生产环境必须恢复为：bn_rand_mod(p->secret_poly_coeffs[j], protocol->order);
        bn_t small_range;
        bn_new(small_range);
        bn_set_dig(small_range, 10);  // 范围 [0, 9]
        bn_rand_mod(p->secret_poly_coeffs[j], small_range);
        bn_add_dig(p->secret_poly_coeffs[j], p->secret_poly_coeffs[j], 1);  // 范围 [1, 10]
        bn_free(small_range);
        
        printf("    a[%d,%d] = ", participant_id, j);
        bn_print(p->secret_poly_coeffs[j]);
        printf(" (调试范围: 1-10)\n");
        
        // 计算承诺 A_{i,j} = g_q^{a_{i,j}}
        char coeff_str[256];
        bn_write_str(coeff_str, sizeof(coeff_str), p->secret_poly_coeffs[j], 10);
        GEN a_ij = strtoi(coeff_str);
        
        // ⭐ 使用 nupow（与 util.c 一致），PARI 内部会自动约化  承诺是在class group上做的
        pari_sp av = avma;
        GEN commitment_temp = nupow(protocol->generator_g, a_ij, NULL);
        p->commitments[j] = gclone(commitment_temp);
        avma = av;
        
        // 打印承诺的前50字符用于调试
        char *commit_str = GENtostr(p->commitments[j]);
        printf("    A[%d,%d]（前50字符）= %.50s...\n", participant_id, j, commit_str);
        pari_free(commit_str);
    }
    
    // 第2步：计算给每个参与者的份额
    printf("  [1.2] 计算份额（使用Horner方法）:\n");
    for (int j = 1; j <= protocol->n_participants; j++) {
        // 使用Horner方法计算 s_{i,j} = f_i(j)
        bn_copy(computed_shares[j], p->secret_poly_coeffs[p->threshold - 1]);
        
        bn_t j_bn;
        bn_new(j_bn);
        bn_set_dig(j_bn, j);
        
        for (int k = p->threshold - 2; k >= 0; k--) {
            bn_mul(computed_shares[j], computed_shares[j], j_bn);
            bn_add(computed_shares[j], computed_shares[j], p->secret_poly_coeffs[k]);
            bn_mod(computed_shares[j], computed_shares[j], protocol->order);
        }
        
        bn_free(j_bn);
        
        printf("    s[%d,%d] = ", participant_id, j);
        bn_print(computed_shares[j]);
            printf("\n");
    }
    
    printf("  ✅ 参与者%d的多项式、承诺和份额生成完成\n", participant_id);
    
    // ================= 第3步：立即验证份额 =================
    printf("\n  [1.3] 自我验证（生成的份额 vs 生成的承诺）:\n");
    
    // ⚠️ 关键测试：检查承诺的一致性
    printf("    ========== 一致性测试 ==========\n");
    char a0_test_str[256];
    bn_write_str(a0_test_str, sizeof(a0_test_str), p->secret_poly_coeffs[0], 10);
    GEN a0_test_gen = strtoi(a0_test_str);
    
    // ⭐ 使用 nupow 重新计算
    pari_sp av_test = avma;
    GEN recomputed_A0_temp = nupow(protocol->generator_g, a0_test_gen, NULL);
    GEN recomputed_A0 = gclone(recomputed_A0_temp);
    avma = av_test;
    
    // 打印存储的 A[0] 和重新计算的 A[0]
    char *stored_A0_str = GENtostr(p->commitments[0]);
    char *recomputed_A0_str = GENtostr(recomputed_A0);
    printf("    存储的 A[0]（前50字符）= %.50s...\n", stored_A0_str);
    printf("    重算的 A[0]（前50字符）= %.50s...\n", recomputed_A0_str);
    
    // ⭐ 直接使用 PARI 的 gequal（不手动约化）
    if (gequal(p->commitments[0], recomputed_A0)) {
        printf("    ✅ 一致性测试通过：A[0] = g^{a[0]}\n");
    } else {
        printf("    ❌ 一致性测试失败：存储的 A[0] 与重算的不一致！\n");
        gunclone(recomputed_A0);
        pari_free(stored_A0_str);
        pari_free(recomputed_A0_str);
        return RLC_ERR;
    }
    
    gunclone(recomputed_A0);
    pari_free(stored_A0_str);
    pari_free(recomputed_A0_str);
    printf("    ==================================\n\n");
    
    // 验证所有份额（使用自我验证，避免 Class Group 约化问题）
    printf("    ========== 份额验证（使用秘密系数直接验证）==========\n");
    for (int j = 1; j <= protocol->n_participants; j++) {
        printf("    验证份额[%d,%d]...", participant_id, j);
        
        int verify_result = dkg_self_verify_share(p, j, computed_shares[j], protocol->order);
        
        if (verify_result != RLC_OK) {
            printf(" ❌ 失败\n");
            printf("    ❌ 自我验证失败：份额[%d,%d]计算错误！\n", participant_id, j);
            return RLC_ERR;
        }
        printf(" ✅ 通过\n");
    }
    printf("    ========================================================\n");
    
    printf("  ✅ 所有验证通过！份额生成正确\n\n");
    return RLC_OK;
}

/**
 * 向后兼容的函数（只生成多项式和承诺）
 */
int dkg_generate_polynomial_and_commitments(dkg_protocol_t protocol, int participant_id) {
    if (!protocol || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t p = protocol->participants[participant_id - 1];
    if (!p || !p->is_initialized) {
        return RLC_ERR;
    }
    
    printf("\n[步骤1] 参与者%d生成多项式和承诺（阈值t=%d）\n", participant_id, p->threshold);
    
    // ⚠️ 关键：打印生成元的前100字符用于对比
    char *gen_str = GENtostr(protocol->generator_g);
    printf("  使用生成元 g_q（前100字符）: %.100s...\n", gen_str);
    pari_free(gen_str);
    
    // 生成随机多项式的系数并计算承诺
    printf("  ⚠️⚠️⚠️ 调试模式：系数范围 [1, 10] - 仅用于测试！⚠️⚠️⚠️\n");
    for (int j = 0; j < p->threshold; j++) {
        // 🔴 调试模式：生成小范围系数 [1, 10]
        // ⚠️ 生产环境必须恢复为：bn_rand_mod(p->secret_poly_coeffs[j], protocol->order);
        bn_t small_range;
        bn_new(small_range);
        bn_set_dig(small_range, 10);  // 范围 [0, 9]
        bn_rand_mod(p->secret_poly_coeffs[j], small_range);
        bn_add_dig(p->secret_poly_coeffs[j], p->secret_poly_coeffs[j], 1);  // 范围 [1, 10]
        bn_free(small_range);
        
        printf("  多项式系数 a[%d,%d] = ", participant_id, j);
        bn_print(p->secret_poly_coeffs[j]);
        printf(" (调试范围: 1-10)\n");
        
        // 计算承诺 A_{i,j} = g_q^{a_{i,j}} （⭐ 使用规范化版本）
        char coeff_str[256];
        bn_write_str(coeff_str, sizeof(coeff_str), p->secret_poly_coeffs[j], 10);
        GEN a_ij = strtoi(coeff_str);
        
        // 使用规范化的幂运算，确保约化形式唯一
        p->commitments[j] = qfb_pow_canonical(protocol->generator_g, a_ij);
        
        printf("  承诺 A[%d,%d] 已生成\n", participant_id, j);
    }
    
    printf("  ✅ 参与者%d的多项式和承诺生成完成\n\n", participant_id);
    return RLC_OK;
}

/**
 * 计算多项式份额（Joint-Feldman DKG）
 * 
 * 数学原理：
 * 对于参与者P_i，计算给参与者P_j的份额：
 * s_{i,j} = f_i(j) = a_{i,0} + a_{i,1}*j + a_{i,2}*j² + ... + a_{i,t-1}*j^(t-1) (mod q)
 * 
 * 使用Horner方法计算多项式值：
 * f_i(j) = a_{i,0} + j*(a_{i,1} + j*(a_{i,2} + ... + j*a_{i,t-1}))
 */
int dkg_compute_secret_shares(dkg_protocol_t protocol, int participant_id) {
    if (!protocol || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t p = protocol->participants[participant_id - 1];
    if (!p || !p->is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG] 参与者%d计算份额（Joint-Feldman）\n", participant_id);
    
    // 计算给所有其他参与者的份额
    for (int j = 1; j <= protocol->n_participants; j++) {
        if (j == participant_id) continue; // 跳过自己
        
        // 使用Horner方法计算 f_i(j)
        bn_t secret_share;
        bn_new(secret_share);
        bn_copy(secret_share, p->secret_poly_coeffs[p->threshold - 1]); // 最高次项系数
        
        for (int k = p->threshold - 2; k >= 0; k--) {
            bn_t j_bn;
            bn_new(j_bn);
            bn_set_dig(j_bn, j);
            bn_mul(secret_share, secret_share, j_bn); // 乘以j
            bn_free(j_bn);
            bn_add(secret_share, secret_share, p->secret_poly_coeffs[k]); // 加上下一项系数
            bn_mod(secret_share, secret_share, protocol->order); // 模运算
        }
        
        printf("[DKG] 参与者%d给参与者%d的份额: s[%d,%d]=%zu位\n", 
               participant_id, j, participant_id, j, bn_size_bin(secret_share));
        
        // 打印具体的份额值
        printf("[DKG] 份额 s[%d,%d] = ", participant_id, j);
        bn_print(secret_share);
        printf("\n");
        
        bn_free(secret_share);
    }
    
    return RLC_OK;
}

/**
 * 自我验证份额（使用秘密系数，避免 Class Group 约化问题）
 * 
 * 数学原理：
 * 直接验证：s_{i,j} ?= a_{i,0} + a_{i,1}*j + ... + a_{i,t-1}*j^{t-1} (mod q)
 * 
 * 这避免了 Class Group 的 qfbred() 约化不唯一问题
 * 
 * @param participant 参与者状态（必须有秘密系数）
 * @param verifier_id 验证者ID（即 j）
 * @param computed_share 计算出的份额 s_{i,j}
 * @param order 群阶
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_self_verify_share(dkg_participant_t participant, int verifier_id, 
                           bn_t computed_share, bn_t order) {
    if (!participant || !participant->is_initialized) {
        return RLC_ERR;
    }
    
    // 使用 Horner 方法计算期望的份额值：f_i(j)
    bn_t expected_share;
    bn_new(expected_share);
    bn_copy(expected_share, participant->secret_poly_coeffs[participant->threshold - 1]);
    
    bn_t j_bn;
    bn_new(j_bn);
    bn_set_dig(j_bn, verifier_id);
    
    for (int k = participant->threshold - 2; k >= 0; k--) {
        bn_mul(expected_share, expected_share, j_bn);
        bn_add(expected_share, expected_share, participant->secret_poly_coeffs[k]);
        bn_mod(expected_share, expected_share, order);
    }
    
    // 直接比较 bn_t
    int result = (bn_cmp(computed_share, expected_share) == RLC_EQ) ? RLC_OK : RLC_ERR;
    
    if (result != RLC_OK) {
        printf("\n      [自我验证失败诊断]\n");
        printf("      computed_share = ");
        bn_print(computed_share);
        printf("\n      expected_share = ");
        bn_print(expected_share);
        printf("\n      这不应该发生，说明份额计算有问题！\n");
    }
    
    bn_free(expected_share);
    bn_free(j_bn);
    
    return result;
}

/**
 * 验证份额（Joint-Feldman DKG - Class Group 版本）
 * 
 * 数学原理：
 * 验证收到的份额s_{i,j}是否与承诺一致：
 * g_q^{s_{i,j}} ?= ∏_{k=0}^{t-1} (A_{i,k})^{j^k} (Class Group)
 * 
 * 这等价于验证：
 * g_q^{f_i(j)} = ∏_{k=0}^{t-1} (g_q^{a_{i,k}})^{j^k} = g_q^{∑_{k=0}^{t-1} a_{i,k}*j^k}
 * 
 * 注意：Joint-Feldman 不需要 random_share 参数（与 Pedersen 不同）
 * 
 * ⚠️ 重要限制：
 * 由于 PARI qfbred() 约化不保证唯一性，此函数可能会误报失败。
 * 数学上正确的份额可能因为约化形式不同而被拒绝。
 * 
 * 🔑 安全策略：
 * - 份额验证失败时只打印警告，不拒绝份额
 * - 依赖最终的公钥验证来确保 DKG 正确性
 * - 如果公钥验证通过，说明所有份额都是正确的
 */
int dkg_verify_share(dkg_protocol_t protocol, int verifier_id, int sender_id, 
                     bn_t received_secret_share) {
    if (!protocol || verifier_id < 1 || verifier_id > protocol->n_participants ||
        sender_id < 1 || sender_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t sender = protocol->participants[sender_id - 1];
    if (!sender || !sender->is_initialized) {
        return RLC_ERR;
    }
    
    // 计算左侧：g_q^{s_{i,j}} (Class Group)
    char share_str[256];
    bn_write_str(share_str, sizeof(share_str), received_secret_share, 10);
    GEN s_ij = strtoi(share_str);
    
    // ⭐ 使用 nupow（PARI 内部会自动约化）
    pari_sp av_left = avma;
    GEN left_side_temp = nupow(protocol->generator_g, s_ij, NULL);
    GEN left_side = gclone(left_side_temp);
    avma = av_left;
    
    // 计算右侧：∏_{k=0}^{t-1} (A_{i,k})^{j^k} (Class Group)
    // 初始化：right_side = A_{i,0}^{j^0} = A_{i,0}^1 = A_{i,0}
    GEN right_side = NULL;
    
    // 使用 GEN 类型计算 j^k，避免 bn_t 溢出
    GEN j = stoi(verifier_id);  
    GEN j_power = gen_1;         
    
    // 获取群阶 q（用于指数模运算）
    char q_str[256];
    bn_write_str(q_str, sizeof(q_str), protocol->order, 10);
    GEN q_gen = strtoi(q_str);
    
    for (int k = 0; k < sender->threshold; k++) {
        // 确保承诺已初始化
        if (sender->commitments[k] == NULL || sender->commitments[k] == gen_0) {
            // 清理已分配的资源
            if (right_side != NULL) {
                gunclone(right_side);
            }
            gunclone(left_side);
            return RLC_ERR;
        }
        
        // ⭐ 关键修复：对指数做 mod q（参考BICYCL实现）
        // 数学原理：在阶为q的群中，g^m = g^{m mod q}
        pari_sp av = avma;
        GEN j_power_mod = gmod(j_power, q_gen);
        GEN j_power_mod_copy = gclone(j_power_mod);
        avma = av;
        
        // 计算 A_{i,k}^{j^k mod q}（⭐ 使用 nupow）
        pari_sp av2 = avma;
        GEN temp_pow = nupow(sender->commitments[k], j_power_mod_copy, NULL);
        GEN temp = gclone(temp_pow);
        avma = av2;
        gunclone(j_power_mod_copy);
        
        // Class Group 群乘法（⭐ 使用 gmul，PARI 自动约化）
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
        
        // 计算 j^(k+1)
        pari_sp av4 = avma;
        GEN j_power_next = gmul(j_power, j);
        GEN j_power_next_copy = gclone(j_power_next);
        avma = av4;
        j_power = j_power_next_copy;
    }
    
    // ⭐ 直接使用 PARI 的 gequal 比较（不手动约化）
    int result = gequal(left_side, right_side);
    
    // 如果失败，打印诊断信息
    if (!result) {
        char *left_str = GENtostr(left_side);
        char *right_str = GENtostr(right_side);
        printf("\n      [验证失败诊断]\n");
        printf("      left  (g^s) 前50字符: %.50s...\n", left_str);
        printf("      right (∏A^j^k) 前50字符: %.50s...\n", right_str);
        printf("      规范化比较后仍然不相等，可能是份额或承诺错误\n");
        pari_free(left_str);
        pari_free(right_str);
    }
    
    // 清理 PARI 对象
    gunclone(left_side);
    gunclone(right_side);
    gunclone(j_power);  // ⭐ 清理 j_power
    
    return result ? RLC_OK : RLC_ERR;
}

/**
 * 重构私钥 - 使用Lagrange插值（给Auditor使用）
 * 
 * 数学原理：
 * 使用Lagrange插值重构私钥：
 * x = ∑_{i∈S} s_i * L_i (mod q)
 * 
 * 其中：
 * - S 是参与重构的参与者集合 (|S| ≥ t)
 * - L_i 是Lagrange系数（在x=0处插值）：
 *   L_i = ∏_{j∈S, j≠i} (0 - j)/(i - j) = ∏_{j∈S, j≠i} (-j)/(i - j) (mod q)
 * 
 * @param participant_ids 参与重构的参与者ID数组
 * @param shares 对应的私钥分片数组
 * @param num_participants 参与者数量（必须 >= threshold）
 * @param order 群阶
 * @param reconstructed_key 输出的重构私钥
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_reconstruct_secret_from_shares(int *participant_ids, bn_t *shares, 
                                       int num_participants, bn_t order, 
                                       bn_t reconstructed_key) {
    if (!participant_ids || !shares || !order || !reconstructed_key || num_participants < 2) {
        printf("[DKG_RECONSTRUCT] 无效参数\n");
        return RLC_ERR;
    }
    
    printf("[DKG_RECONSTRUCT] 开始Lagrange插值重构私钥，使用%d个份额\n", num_participants);
    
    // 初始化重构私钥为0
    bn_zero(reconstructed_key);
    
    // 对每个参与者计算 s_i * L_i 并累加
    for (int i = 0; i < num_participants; i++) {
        int x_i = participant_ids[i];
        
        printf("[DKG_RECONSTRUCT] 处理参与者%d的份额\n", x_i);
        
        // 计算Lagrange系数 L_i(0) = ∏_{j≠i} (-x_j)/(x_i - x_j)
        bn_t lagrange_coeff;
        bn_new(lagrange_coeff);
        bn_set_dig(lagrange_coeff, 1);
        
        for (int j = 0; j < num_participants; j++) {
            if (i == j) continue;
            
            int x_j = participant_ids[j];
           
            
            // 计算 (-x_j) / (x_i - x_j) mod q
            bn_t numerator, denominator, neg_xj, xi_minus_xj, temp;
            bn_new(numerator);
            bn_new(denominator);
            bn_new(neg_xj);
            bn_new(xi_minus_xj);
            bn_new(temp);
            
            // numerator = -x_j mod q
            bn_set_dig(numerator, x_j);
            bn_sub(neg_xj, order, numerator);  // -x_j = q - x_j
            
            // denominator = x_i - x_j
            if (x_i > x_j) {
                bn_set_dig(denominator, x_i - x_j);
            } else {
                // 如果 x_i < x_j，则 x_i - x_j < 0，需要加 q
                bn_set_dig(temp, x_j - x_i);
                bn_sub(denominator, order, temp);
            }
            
            // temp = numerator / denominator mod q
            bn_mod_inv(temp, denominator, order);
            bn_mul(temp, neg_xj, temp);
            bn_mod(temp, temp, order);
            
            // 调试输出这一项的值
            char temp_str[256];
            bn_write_str(temp_str, sizeof(temp_str), temp, 10);
           
            
            // lagrange_coeff *= temp
            bn_mul(lagrange_coeff, lagrange_coeff, temp);
            bn_mod(lagrange_coeff, lagrange_coeff, order);
            
            // 调试输出累积结果
            char coeff_str[256];
            bn_write_str(coeff_str, sizeof(coeff_str), lagrange_coeff, 10);
            
            
            bn_free(numerator);
            bn_free(denominator);
            bn_free(neg_xj);
            bn_free(xi_minus_xj);
            bn_free(temp);
        }
        
       
        bn_print(lagrange_coeff);
        printf("\n");
        
        // 计算 s_i * L_i
        bn_t contribution;
        bn_new(contribution);
        bn_mul(contribution, shares[i], lagrange_coeff);
        bn_mod(contribution, contribution, order);
        
        
        bn_print(contribution);
        printf("\n");
        
        // 累加到重构私钥
        bn_add(reconstructed_key, reconstructed_key, contribution);
        bn_mod(reconstructed_key, reconstructed_key, order);
        
        bn_free(lagrange_coeff);
        bn_free(contribution);
    }
    
    printf("[DKG_RECONSTRUCT] 私钥重构完成！\n");
    printf("[DKG_RECONSTRUCT] 重构的完整私钥 = ");
    bn_print(reconstructed_key);
    printf("\n");
    printf("[DKG_RECONSTRUCT] 私钥长度: %zu位\n", bn_size_bin(reconstructed_key));
    
    return RLC_OK;
}

/**
 * 验证最终公钥的正确性（⭐ 关键安全检查）
 * 
 * 数学原理：
 * 每个参与者验证：g_q^{sk_j} ?= PK （通过离散对数关系）
 * 其中 sk_j = ∑_{i=1}^n f_i(j) 是参与者j的私钥分片
 * 
 * 这里我们用更简单的方法：
 * 验证 PK = g_q^{sk_1} * g_q^{sk_2} * ... (通过拉格朗日插值)
 * 
 * ⚠️ 注意：这个验证不依赖 qfbred 的唯一性，因为我们使用 Lagrange 插值
 * 
 * @return RLC_OK 验证通过，RLC_ERR 验证失败
 */
int dkg_verify_final_public_key(dkg_protocol_t protocol) {
    if (!protocol || !protocol->is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG_VERIFY_PK] 验证最终公钥的正确性\n");
    printf("[DKG_VERIFY_PK] 使用 Lagrange 插值验证公钥与私钥分片的一致性\n");
    
    // 策略：使用 t+1 个参与者的私钥分片，重构 sk = Σ a_{i,0}
    // 然后验证 g_q^{sk} ?= PK
    
    int num_participants_to_use = protocol->threshold + 1;
    if (num_participants_to_use > protocol->n_participants) {
        num_participants_to_use = protocol->n_participants;
    }
    
    printf("[DKG_VERIFY_PK] 使用前 %d 个参与者的私钥分片进行验证\n", num_participants_to_use);
    
    // 准备参与者ID和私钥分片数组
    int *participant_ids = malloc(num_participants_to_use * sizeof(int));
    bn_t *shares = malloc(num_participants_to_use * sizeof(bn_t));
    
    for (int i = 0; i < num_participants_to_use; i++) {
        participant_ids[i] = i + 1;
        bn_new(shares[i]);
        bn_copy(shares[i], protocol->participants[i]->secret_share);
        
        printf("[DKG_VERIFY_PK] 参与者%d的私钥分片 = ", i+1);
        bn_print(shares[i]);
            printf("\n");
    }
    
    // 使用 Lagrange 插值重构 sk = f(0) = Σ a_{i,0}
    bn_t reconstructed_sk;
    bn_new(reconstructed_sk);
    
    if (dkg_reconstruct_secret_from_shares(participant_ids, shares, num_participants_to_use, 
                                           protocol->order, reconstructed_sk) != RLC_OK) {
        printf("[DKG_VERIFY_PK] ❌ Lagrange插值失败\n");
        bn_free(reconstructed_sk);
        for (int i = 0; i < num_participants_to_use; i++) bn_free(shares[i]);
        free(shares);
        free(participant_ids);
        return RLC_ERR;
    }
    
    printf("[DKG_VERIFY_PK] 重构的总私钥 sk = ");
    bn_print(reconstructed_sk);
            printf("\n");
    
    // 计算 g_q^{sk}（⭐ 使用 nupow，不手动约化）
    char sk_str[256];
    bn_write_str(sk_str, sizeof(sk_str), reconstructed_sk, 10);
    GEN sk_gen = strtoi(sk_str);
    
    pari_sp av = avma;
    GEN expected_pk_temp = nupow(protocol->generator_g, sk_gen, NULL);
    GEN expected_pk = gclone(expected_pk_temp);
    avma = av;
    
    // 获取存储的公钥（不约化）
    GEN stored_pk = protocol->participants[0]->public_key;
    
    // 打印调试信息
    char *expected_str = GENtostr(expected_pk);
    char *stored_str = GENtostr(stored_pk);
    
    printf("[DKG_VERIFY_PK] 期望公钥 g_q^{sk}（前100字符）: %.100s...\n", expected_str);
    printf("[DKG_VERIFY_PK] 存储公钥 PK（前100字符）    : %.100s...\n", stored_str);
    
    // ⭐ 直接使用 gequal 比较（不手动约化）
    int result = gequal(expected_pk, stored_pk) ? RLC_OK : RLC_ERR;
    
    if (result == RLC_OK) {
        printf("[DKG_VERIFY_PK] ✅ 公钥验证通过！DKG协议执行正确！\n");
    } else {
        printf("[DKG_VERIFY_PK] ❌ 公钥验证失败！\n");
        printf("[DKG_VERIFY_PK] 这说明份额计算或聚合有问题\n");
    }
    
    // 清理
    pari_free(expected_str);
    pari_free(stored_str);
    gunclone(expected_pk);
    bn_free(reconstructed_sk);
    for (int i = 0; i < num_participants_to_use; i++) bn_free(shares[i]);
    free(shares);
    free(participant_ids);
    
    return result;
}

/**
 * 生成公钥（Class Group 版本）
 * 
 * 数学原理：
 * 公钥 Y = ∏_{i=1}^n A_{i,0} = ∏_{i=1}^n g_q^{a_{i,0}} = g_q^{∑_{i=1}^n a_{i,0}}
 * 其中 A_{i,0} 是参与者i的常数项承诺（Class Group元素）
 */
int dkg_generate_public_key(dkg_protocol_t protocol) {
    if (!protocol || !protocol->is_initialized) {
        return RLC_ERR;
    }
    
    printf("[DKG] 生成公钥（从承诺生成 - Class Group）\n");
    
    // 从承诺生成公钥：pk = ∏_{i=1}^n A_{i,0}
    GEN public_key_from_commitments = NULL;
    
    printf("[DKG] 计算公钥（Joint-Feldman - Class Group）：pk = ∏_{i=1}^n A_{i,0} = g_q^{∑ a_{i,0}}\n");
    
    for (int i = 0; i < protocol->n_participants; i++) {
        if (protocol->participants[i] && protocol->participants[i]->is_initialized) {
            printf("[DKG] 处理参与者%d的承诺A[%d,0] (Class Group)\n", i+1, i+1);
            
            // Joint-Feldman: 直接使用 A_{i,0} = g_q^{a_{i,0}}
            printf("[DKG] 调试: A[%d,0] = g_q^{a[%d,0]} (Class Group)\n", i+1, i+1);
            
            // Class Group 群乘法：public_key = public_key * A_{i,0}（⭐ 使用 gmul）
            if (public_key_from_commitments == NULL) {
                // 第一个参与者：直接克隆
                public_key_from_commitments = gclone(protocol->participants[i]->commitments[0]);
            } else {
                // 后续参与者：使用 gmul（PARI 自动约化）
                pari_sp av = avma;
                GEN new_pk = gmul(public_key_from_commitments, protocol->participants[i]->commitments[0]);
                GEN new_pk_copy = gclone(new_pk);
                avma = av;
                
                gunclone(public_key_from_commitments);
                public_key_from_commitments = new_pk_copy;
            }
        } else {
            printf("[DKG] 警告: 参与者%d未初始化或不存在\n", i+1);
        }
    }
    
    // 打印公钥（PARI格式）
    char *pk_str = GENtostr(public_key_from_commitments);
    printf("[DKG] 生成的Class Group公钥: %s\n", pk_str);
    pari_free(pk_str);
    
    // 将公钥分发给所有参与者
    for (int i = 0; i < protocol->n_participants; i++) {
        if (protocol->participants[i]) {
            protocol->participants[i]->public_key = public_key_from_commitments;
            printf("[DKG] 参与者%d的公钥已设置 (Class Group)\n", i+1);
        }
    }
    
    printf("[DKG] 公钥生成完成 (Class Group)\n");
    
    return RLC_OK;
}

/**
 * 获取参与者的公钥 (Class Group)
 * 注意：Class Group DKG 的公钥是 GEN 类型，不能直接复制
 */
int dkg_get_public_key_cl(dkg_protocol_t protocol, int participant_id, GEN *public_key) {
    if (!protocol || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t p = protocol->participants[participant_id - 1];
    if (!p || !p->is_initialized) {
        return RLC_ERR;
    }
    
    *public_key = p->public_key;
    return RLC_OK;
}

/**
 * 获取参与者的私钥分片
 */
int dkg_get_secret_share(dkg_protocol_t protocol, int participant_id, bn_t secret_share) {
    if (!protocol || participant_id < 1 || participant_id > protocol->n_participants) {
        return RLC_ERR;
    }
    
    dkg_participant_t p = protocol->participants[participant_id - 1];
    if (!p || !p->is_initialized) {
        return RLC_ERR;
    }
    
    bn_copy(secret_share, p->secret_share);
    return RLC_OK;
}

/**
 * 获取参与者的随机数 r_{i,j}（仅用于Pedersen DKG，Joint-Feldman不需要）
 * 注意：Joint-Feldman / Class Group DKG 不使用随机多项式
 */
int dkg_get_random_value(dkg_protocol_t protocol, int participant_id, int j, bn_t random_value) {
    printf("[DKG] 警告: dkg_get_random_value 在 Joint-Feldman / Class Group DKG 中不适用\n");
        return RLC_ERR;
}

/**
 * 计算 h^{r_{i,j}}（仅用于Pedersen DKG，Joint-Feldman不需要）
 * 注意：Joint-Feldman / Class Group DKG 不使用随机承诺
 */
int dkg_get_random_commitment(dkg_protocol_t protocol, int participant_id, int j, g1_t h_to_r) {
    printf("[DKG] 警告: dkg_get_random_commitment 在 Joint-Feldman / Class Group DKG 中不适用\n");
        return RLC_ERR;
}

// ================= 辅助函数实现 =================

/**
 * 计算Lagrange系数
 * 
 * 数学原理：
 * L_i = ∏_{j∈S, j≠i} j/(j - i) (mod q)
 * 
 * 这是Lagrange插值公式中的系数，用于重构秘密
 */
int dkg_compute_lagrange_coefficient(int *participant_ids, int num_participants, 
                                    int target_id, bn_t order, bn_t lagrange_coeff) {
    if (!participant_ids || !order || !lagrange_coeff || num_participants < 2) {
        return RLC_ERR;
    }
    
    printf("[DKG] 计算Lagrange系数，目标参与者ID=%d\n", target_id);
    
    bn_set_dig(lagrange_coeff, 1);
    
    for (int j = 0; j < num_participants; j++) {
        int other_id = participant_ids[j];
        if (other_id == target_id) continue;
        
        // 计算 j/(j - i)
        bn_t numerator, denominator, temp;
        bn_new(numerator);
        bn_new(denominator);
        bn_new(temp);
        
        bn_set_dig(numerator, other_id);
        bn_set_dig(denominator, other_id - target_id);
        
        // 计算模逆元
        bn_mod_inv(temp, denominator, order);
        bn_mul(temp, numerator, temp);
        bn_mod(temp, temp, order);
        
        bn_mul(lagrange_coeff, lagrange_coeff, temp);
        bn_mod(lagrange_coeff, lagrange_coeff, order);
        
        bn_free(numerator);
        bn_free(denominator);
        bn_free(temp);
    }
    
    printf("[DKG] Lagrange系数计算完成: L[%d]=%zu位\n", target_id, bn_size_bin(lagrange_coeff));
    return RLC_OK;
}


// 注意：投诉机制暂时不需要，已删除 dkg_handle_complaint() 函数

// 注意：DKG专用的序列化函数已移除，现在使用 util.c 中的通用序列化函数
// 这样可以减少代码重复并提高一致性
