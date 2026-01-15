#ifndef A2L_ECDSA_INCLUDE_PEDERSEN_DKG
#define A2L_ECDSA_INCLUDE_PEDERSEN_DKG

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "types.h"

// ================= Joint-Feldman DKG 函数声明 =================

/**
 * 初始化DKG协议
 * 
 * 数学原理（Joint-Feldman DKG）：
 * - 设置生成元 g ∈ G₁
 * - 确定参与者数量 n 和阈值 t
 * - 初始化群阶 q
 * 
 * @param protocol DKG协议状态
 * @param n_participants 参与者数量
 * @param threshold 阈值
 * @return RLC_OK 成功，RLC_ERR 失败
 */
/**
 * 初始化DKG协议（使用外部提供的 Class Group 参数）
 * @param cl_params 外部提供的 Class Group 参数（如果为 NULL，则内部生成）
 */
int dkg_protocol_init_with_cl_params(dkg_protocol_t protocol, int n_participants, 
                                      int threshold, cl_params_t cl_params);

/**
 * 初始化DKG协议（向后兼容，内部生成 cl_params）
 */
int dkg_protocol_init(dkg_protocol_t protocol, int n_participants, int threshold);

/**
 * 添加参与者到DKG协议
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID (1到n)
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_add_participant(dkg_protocol_t protocol, int participant_id);

/**
 * 生成随机多项式、计算承诺和份额（Joint-Feldman DKG）
 * 
 * 数学原理：
 * 1. 生成随机多项式：f_i(x) = a_{i,0} + a_{i,1}x + ... + a_{i,t-1}x^(t-1)
 * 2. 计算承诺：A_{i,j} = g^{a_{i,j}}
 * 3. 计算份额：s_{i,j} = f_i(j) 给每个参与者P_j
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param computed_shares 输出：计算出的份额数组（索引1到n）
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_generate_polynomial_commitments_and_shares(dkg_protocol_t protocol, int participant_id, 
                                                     bn_t *computed_shares);

/**
 * 生成随机多项式并计算承诺（向后兼容）
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_generate_polynomial_and_commitments(dkg_protocol_t protocol, int participant_id);

/**
 * 计算秘密份额
 * 
 * 数学原理（Joint-Feldman DKG）：
 * 对于参与者P_i，计算给参与者P_j的份额：
 * s_{i,j} = f_i(j) = a_{i,0} + a_{i,1}*j + a_{i,2}*j² + ... + a_{i,t}*j^t (mod q)
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_compute_secret_shares(dkg_protocol_t protocol, int participant_id);

/**
 * 验证份额
 * 
 * 数学原理（Joint-Feldman DKG）：
 * 验证收到的份额s_{i→j}是否与承诺一致：
 * g^{s_{i→j}} = ∏_{k=0}^{t} (A_{i,k})^{j^k} (mod p)
 * 
 * @param protocol DKG协议状态
 * @param verifier_id 验证者ID
 * @param sender_id 发送者ID
 * @param received_secret_share 收到的秘密份额
 * @return RLC_OK 验证成功，RLC_ERR 验证失败
 */
int dkg_verify_share(dkg_protocol_t protocol, int verifier_id, int sender_id, 
                     bn_t received_secret_share);

/**
 * 自我验证份额（使用秘密系数，避免 Class Group 约化问题）
 * 
 * @param participant 参与者状态（必须有秘密系数）
 * @param verifier_id 验证者ID（即 j）
 * @param computed_share 计算出的份额
 * @param order 群阶
 * @return RLC_OK 验证成功，RLC_ERR 验证失败
 */
int dkg_self_verify_share(dkg_participant_t participant, int verifier_id, 
                           bn_t computed_share, bn_t order);

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
                                       bn_t reconstructed_key);

/**
 * 生成公钥
 * 
 * 数学原理：
 * 公钥 Y = g^x (mod p)
 * 
 * @param protocol DKG协议状态
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_generate_public_key(dkg_protocol_t protocol);

/**
 * 验证最终公钥的正确性（关键安全检查）
 * 
 * 使用 Lagrange 插值重构总私钥，然后验证 g_q^{sk} = PK
 * 
 * @param protocol DKG协议状态
 * @return RLC_OK 验证通过，RLC_ERR 验证失败
 */
int dkg_verify_final_public_key(dkg_protocol_t protocol);

/**
 * 完整的DKG协议执行
 * 
 * 数学原理：
 * 1. 每个参与者生成随机多项式 f_i(x)
 * 2. 计算并广播承诺 C_{i,j}
 * 3. 计算并分发秘密份额 s_{i,j}
 * 4. 验证收到的份额
 * 5. 重构私钥 x = ∑_{i=1}^n a_{i,0}
 * 6. 生成公钥 Y = g^x
 * 
 * @param protocol DKG协议状态
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_execute_protocol(dkg_protocol_t protocol);

/**
 * 获取参与者的公钥 (secp256k1)
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param public_key 输出的公钥
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_get_public_key(dkg_protocol_t protocol, int participant_id, ec_t public_key);

/**
 * 获取参与者的私钥分片
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param secret_share 输出的私钥分片
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_get_secret_share(dkg_protocol_t protocol, int participant_id, bn_t secret_share);

// ================= 辅助函数 =================

/**
 * 计算Lagrange系数
 * 
 * 数学原理：
 * L_i = ∏_{j∈S, j≠i} j/(j - i) (mod q)
 * 
 * @param participant_ids 参与者ID数组
 * @param num_participants 参与者数量
 * @param target_id 目标参与者ID
 * @param order 群阶
 * @param lagrange_coeff 输出的Lagrange系数
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_compute_lagrange_coefficient(int *participant_ids, int num_participants, 
                                    int target_id, bn_t order, bn_t lagrange_coeff);


// 注意：投诉机制暂时不需要，已删除 dkg_handle_complaint() 函数声明

/**
 * 序列化DKG消息
 * 
 * @param message DKG消息
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 序列化后的数据长度，-1表示失败
 */
int dkg_serialize_message(dkg_message_t message, uint8_t *buffer, size_t buffer_size);

/**
 * 反序列化DKG消息
 * 
 * @param buffer 输入缓冲区
 * @param buffer_size 缓冲区大小
 * @param message 输出的DKG消息
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_deserialize_message(const uint8_t *buffer, size_t buffer_size, dkg_message_t *message);

#endif // A2L_ECDSA_INCLUDE_PEDERSEN_DKG
