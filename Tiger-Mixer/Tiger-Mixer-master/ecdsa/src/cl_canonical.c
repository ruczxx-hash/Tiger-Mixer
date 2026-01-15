/**
 * Class Group 规范化工具实现
 */

#include "cl_canonical.h"
#include <stdio.h>
#include <string.h>

// 调试标志（可以通过环境变量控制）
static int debug_mode = 0;

void qfb_canonical_init() {
    debug_mode = 0; // 默认关闭调试
    printf("[CL_CANONICAL] 规范化模块已初始化\n");
}

void qfb_canonical_cleanup() {
    printf("[CL_CANONICAL] 规范化模块已清理\n");
}

/**
 * 规范化二次型
 * 
 * 规范化规则（确保唯一性）：
 * 1. 首先调用 qfbred() 进行标准约化
 * 2. 提取 [a, b, c]
 * 3. 规范化 b 的符号：
 *    - 如果 b < 0，则取反得到 Qfb(a, -b, c)
 *    - 如果 b == 0 且 a < 0，则取反得到 Qfb(-a, 0, -c)
 * 4. 再次调用 qfbred() 确保仍然约化
 * 5. 检查 |b| <= a <= c （标准约化形式）
 */
GEN qfb_canonical(GEN qfb) {
    if (qfb == NULL || qfb == gen_0) {
        printf("[CL_CANONICAL] 警告: 输入为NULL或gen_0\n");
        return NULL;
    }
    
    pari_sp av = avma;
    
    // 步骤1：标准约化
    GEN reduced = qfbred(qfb);
    
    // 步骤2：提取系数 [a, b, c]
    GEN a = gel(reduced, 1);
    GEN b = gel(reduced, 2);
    GEN c = gel(reduced, 3);
    
    // 步骤3：规范化 b 的符号
    GEN canonical_a = a;
    GEN canonical_b = b;
    GEN canonical_c = c;
    
    int b_sign = signe(b);
    int a_sign = signe(a);
    
    if (b_sign < 0) {
        // 情况1: b < 0，则 b = -b
        canonical_b = negi(b);
        
        if (debug_mode) {
            printf("[CL_CANONICAL] b < 0，取反: b = -b\n");
        }
    } else if (b_sign == 0 && a_sign < 0) {
        // 情况2: b == 0 且 a < 0，则 a = -a, c = -c
        canonical_a = negi(a);
        canonical_c = negi(c);
        
        if (debug_mode) {
            printf("[CL_CANONICAL] b == 0 且 a < 0，取反: a = -a, c = -c\n");
        }
    }
    
    // 步骤4：重新构造二次型
    GEN discriminant = gel(reduced, 4); // 判别式
    GEN canonical_qfb = mkqfb(canonical_a, canonical_b, canonical_c, discriminant);
    
    // 步骤5：再次约化（确保仍然是约化形式）
    GEN final_reduced = qfbred(canonical_qfb);
    
    // 步骤6：创建永久副本
    GEN result = gclone(final_reduced);
    
    // 恢复栈
    avma = av;
    
    if (debug_mode) {
        char *orig_str = GENtostr(qfb);
        char *result_str = GENtostr(result);
        printf("[CL_CANONICAL] 规范化: %s -> %s\n", orig_str, result_str);
        pari_free(orig_str);
        pari_free(result_str);
    }
    
    return result;
}

/**
 * 比较两个二次型是否等价
 */
int qfb_is_equivalent(GEN qfb1, GEN qfb2) {
    if (qfb1 == NULL || qfb2 == NULL) {
        return 0;
    }
    
    pari_sp av = avma;
    
    // 规范化后比较
    GEN canon1 = qfb_canonical(qfb1);
    GEN canon2 = qfb_canonical(qfb2);
    
    int result = gequal(canon1, canon2);
    
    // 清理
    gunclone(canon1);
    gunclone(canon2);
    avma = av;
    
    return result;
}

/**
 * 二次型幂运算（自动规范化）
 */
GEN qfb_pow_canonical(GEN base, GEN exponent) {
    if (base == NULL || base == gen_0) {
        printf("[CL_CANONICAL] 错误: qfb_pow_canonical 基底为NULL\n");
        return NULL;
    }
    
    pari_sp av = avma;
    
    // 1. 确保基底是规范化的
    GEN canonical_base = qfb_canonical(base);
    
    // 2. 执行幂运算
    GEN power_result = qfbpow(canonical_base, exponent);
    
    // 3. 规范化结果
    GEN canonical_result = qfb_canonical(power_result);
    
    // 4. 创建永久副本
    GEN result = gclone(canonical_result);
    
    // 清理临时对象
    gunclone(canonical_base);
    gunclone(canonical_result);
    avma = av;
    
    if (debug_mode) {
        char *base_str = GENtostr(base);
        char *exp_str = GENtostr(exponent);
        char *result_str = GENtostr(result);
        printf("[CL_CANONICAL] %s ^ %s = %s\n", base_str, exp_str, result_str);
        pari_free(base_str);
        pari_free(exp_str);
        pari_free(result_str);
    }
    
    return result;
}

/**
 * 二次型组合（自动规范化）
 */
GEN qfb_comp_canonical(GEN qfb1, GEN qfb2) {
    if (qfb1 == NULL || qfb2 == NULL) {
        printf("[CL_CANONICAL] 错误: qfb_comp_canonical 输入为NULL\n");
        return NULL;
    }
    
    pari_sp av = avma;
    
    // 1. 确保输入都是规范化的
    GEN canonical_qfb1 = qfb_canonical(qfb1);
    GEN canonical_qfb2 = qfb_canonical(qfb2);
    
    // 2. 执行组合
    GEN comp_result = qfbcomp(canonical_qfb1, canonical_qfb2);
    
    // 3. 规范化结果
    GEN canonical_result = qfb_canonical(comp_result);
    
    // 4. 创建永久副本
    GEN result = gclone(canonical_result);
    
    // 清理临时对象
    gunclone(canonical_qfb1);
    gunclone(canonical_qfb2);
    gunclone(canonical_result);
    avma = av;
    
    if (debug_mode) {
        char *qfb1_str = GENtostr(qfb1);
        char *qfb2_str = GENtostr(qfb2);
        char *result_str = GENtostr(result);
        printf("[CL_CANONICAL] %s * %s = %s\n", qfb1_str, qfb2_str, result_str);
        pari_free(qfb1_str);
        pari_free(qfb2_str);
        pari_free(result_str);
    }
    
    return result;
}

/**
 * 从字符串反序列化二次型（自动规范化）
 */
GEN qfb_from_str_canonical(const char* str) {
    if (str == NULL) {
        printf("[CL_CANONICAL] 错误: 输入字符串为NULL\n");
        return NULL;
    }
    
    pari_sp av = avma;
    
    // 1. 反序列化
    GEN qfb = gp_read_str(str);
    
    // 2. 规范化
    GEN canonical = qfb_canonical(qfb);
    
    // 3. 创建永久副本
    GEN result = gclone(canonical);
    
    // 清理
    gunclone(canonical);
    avma = av;
    
    if (debug_mode) {
        char *result_str = GENtostr(result);
        printf("[CL_CANONICAL] 反序列化: \"%s\" -> %s\n", str, result_str);
        pari_free(result_str);
    }
    
    return result;
}

