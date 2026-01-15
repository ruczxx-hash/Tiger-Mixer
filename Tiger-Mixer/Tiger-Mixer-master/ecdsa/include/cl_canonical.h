/**
 * Class Group 规范化工具 - 解决PARI qfbred约化不唯一问题
 * 
 * 问题：
 * PARI/GP的qfbred()可能对同一个类群元素返回不同的约化表示，
 * 导致数学上等价的元素在比较时失败。
 * 
 * 解决方案：
 * 在qfbred()基础上再添加一层规范化(canonicalization)，
 * 确保每个类群元素都有唯一的、确定性的表示。
 * 
 * 规范化规则（基于标准二次型理论）：
 * 对于约化二次型 Qfb(a, b, c)：
 * 1. 首先调用 qfbred() 进行标准约化
 * 2. 然后规范化 b 的符号：
 *    - 如果 b < 0，则 b = -b
 *    - 如果 b == 0 且 a < 0，则 a = -a
 * 3. 再次调用 qfbred() 确保仍然是约化形式
 */

#ifndef CL_CANONICAL_H
#define CL_CANONICAL_H

#include <pari/pari.h>

/**
 * 规范化二次型
 * 
 * 确保二次型有唯一的、确定性的表示。
 * 
 * @param qfb 输入的二次型（Qfb对象）
 * @return 规范化后的二次型（永久分配，需要调用者负责释放）
 */
GEN qfb_canonical(GEN qfb);

/**
 * 比较两个二次型是否等价（考虑规范化）
 * 
 * @param qfb1 第一个二次型
 * @param qfb2 第二个二次型
 * @return 1 如果等价，0 否则
 */
int qfb_is_equivalent(GEN qfb1, GEN qfb2);

/**
 * 二次型幂运算（自动规范化）
 * 
 * @param base 基底二次型
 * @param exponent 指数（GEN整数）
 * @return 规范化后的结果（永久分配）
 */
GEN qfb_pow_canonical(GEN base, GEN exponent);

/**
 * 二次型组合（自动规范化）
 * 
 * @param qfb1 第一个二次型
 * @param qfb2 第二个二次型
 * @return 规范化后的结果（永久分配）
 */
GEN qfb_comp_canonical(GEN qfb1, GEN qfb2);

/**
 * 从字符串反序列化二次型（自动规范化）
 * 
 * @param str 字符串表示
 * @return 规范化后的二次型（永久分配）
 */
GEN qfb_from_str_canonical(const char* str);

/**
 * 初始化规范化模块（可选，用于调试）
 */
void qfb_canonical_init();

/**
 * 清理规范化模块（可选，用于调试）
 */
void qfb_canonical_cleanup();

#endif // CL_CANONICAL_H

