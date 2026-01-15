#ifndef A2L_ECDSA_INCLUDE_DKG_INTEGRATION
#define A2L_ECDSA_INCLUDE_DKG_INTEGRATION

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "types.h"
#include "secret_share.h"
#include "pedersen_dkg.h"

// ================= DKG 集成函数声明 =================

/**
 * 初始化DKG委员会
 * 
 * 数学原理：
 * 在系统启动时，所有委员会成员（secret_share_receiver实例）需要：
 * 1. 初始化DKG协议
 * 2. 生成自己的多项式
 * 3. 计算承诺
 * 4. 分发秘密份额
 * 5. 验证其他参与者的份额
 * 6. 重构私钥并生成公钥
 * 
 * @param participant_id 该实例的参与者ID (1到n)
 * @param n_participants 总参与者数量
 * @param threshold 阈值
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_committee_init(int participant_id, int n_participants, int threshold);

/**
 * 执行DKG协议
 * 
 * 数学原理：
 * 1. 生成随机多项式 f_i(x) = a_{i,0} + a_{i,1}x + ... + a_{i,t-1}x^(t-1)
 * 2. 计算Pedersen承诺 C_{i,j} = g^{a_{i,j}} * h^{r_{i,j}}
 * 3. 计算秘密份额 s_{i,j} = f_i(j)
 * 4. 验证其他参与者的份额
 * 5. 重构私钥 x = ∑_{i=1}^n a_{i,0}
 * 6. 生成公钥 Y = g^x
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_committee_execute_protocol();

/**
 * 保存DKG密钥到文件
 * 
 * 文件格式：
 * - 私钥分片文件: ../keys/dkg_participant_X.key
 * - 公钥文件: ../keys/dkg_public.key
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_save_keys_to_files();

/**
 * 从文件加载DKG密钥
 * 
 * @param participant_id 参与者ID
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_load_keys_from_files(int participant_id);

/**
 * 获取DKG公钥
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param public_key 输出的公钥
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_get_public_key(dkg_protocol_t protocol, int participant_id, g1_t public_key);

/**
 * 获取DKG私钥分片
 * 
 * @param protocol DKG协议状态
 * @param participant_id 参与者ID
 * @param secret_share 输出的私钥分片
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_get_secret_share(dkg_protocol_t protocol, int participant_id, bn_t secret_share);

/**
 * DKG发送进程
 * 负责发送承诺和份额给其他参与者
 * 绑定到发送端口 (6001, 6002, 6003...)
 * 
 * @param participant_id 参与者ID
 * @return 0 成功，1 失败
 */
int dkg_sender_process(int participant_id);

/**
 * DKG接收进程
 * 负责接收其他参与者的承诺和份额
 * 绑定到接收端口 (7001, 7002, 7003...)
 * 
 * @param participant_id 参与者ID
 * @return 0 成功，1 失败
 */
int dkg_receiver_process(int participant_id);

/**
 * DKG双进程模式
 * 自动启动发送进程和接收进程
 * 
 * @param participant_id 参与者ID
 * @return 0 成功，1 失败
 */
int dkg_dual_process_mode(int participant_id);

/**
 * 验证DKG协议的正确性
 * 
 * 数学原理：
 * 验证最终生成的公钥是否与所有参与者的承诺一致：
 * Y = ∏_{i=1}^n C_{i,0} = ∏_{i=1}^n g^{a_{i,0}} * h^{r_{i,0}}
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_verify_protocol();

/**
 * 清理DKG委员会资源
 */
void dkg_committee_cleanup();

/**
 * 检查DKG密钥文件是否存在
 * 
 * @param participant_id 参与者ID
 * @return 1 存在，0 不存在
 */
int dkg_key_files_exist(int participant_id);

/**
 * 获取DKG委员会状态信息
 */
void dkg_committee_print_status();

// ================= 分布式DKG通信函数 =================

/**
 * 广播承诺给所有其他参与者
 * 
 * 数学原理：
 * 每个参与者需要将自己的承诺 C_{i,j} 广播给所有其他参与者
 * 使用ZMQ进行进程间通信
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_broadcast_commitments();

/**
 * 等待其他参与者的承诺
 * 
 * 数学原理：
 * 每个参与者需要接收来自其他参与者的承诺
 * 验证承诺的有效性
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_wait_for_commitments();

/**
 * 计算和分发秘密份额
 * 
 * 数学原理：
 * 对于每个其他参与者P_j，计算份额 s_{i,j} = f_i(j)
 * 通过ZMQ发送给对应的参与者
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_compute_and_distribute_shares();

/**
 * 验证收到的份额
 * 
 * 数学原理：
 * 验证收到的份额是否与承诺一致
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_verify_received_shares();

/**
 * 重构私钥和生成公钥
 * 
 * 数学原理：
 * 使用Lagrange插值重构私钥：x = ∑_{i∈S} s_i * L_i
 * 生成公钥：Y = g^x
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_reconstruct_keys();

/**
 * 广播承诺
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_broadcast_commitments();

/**
 * 等待其他参与者的承诺
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_wait_for_commitments();

/**
 * 计算并分发份额
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_compute_and_distribute_shares();

/**
 * 验证接收到的份额
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_verify_received_shares();

/**
 * 保存密钥到文件
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_save_keys_to_files();

/**
 * 接收其他参与者给自己的份额
 * 
 * 数学原理：
 * 每个参与者需要接收来自其他参与者的份额，并将它们相加得到最终私钥分片
 * 最终私钥分片 = s_{1,i} + s_{2,i} + ... + s_{n,i}
 * 
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_receive_shares_from_others();

/**
 * 发送份额给指定参与者
 * 
 * 数学原理：
 * 将计算出的份额通过网络发送给指定的参与者
 * 
 * @param target_participant_id 目标参与者ID
 * @param share 要发送的份额
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_send_share_to_participant(int target_participant_id, bn_t share);

/**
 * 发送两个份额给指定参与者
 * 
 * 数学原理：
 * 将计算出的两个份额通过网络发送给指定的参与者
 * 
 * @param target_participant_id 目标参与者ID
 * @param secret_share 私钥份额 s_{i→j}
 * @param random_share 随机数份额 t_{i→j}
 * @return RLC_OK 成功，RLC_ERR 失败
 */
int dkg_send_shares_to_participant(int target_participant_id, bn_t secret_share, bn_t random_share);


// 统一模式函数
int dkg_unified_mode(int participant_id);

// 重构后的DKG阶段函数
int dkg_setup_network_connections(int participant_id, void *pub_socket, void *sub_socket);
int dkg_generation_phase(int participant_id, void *pub_socket, void *sub_socket);
int dkg_verification_phase(int participant_id, void *sub_socket);
int dkg_key_reconstruction_phase(int participant_id);
int dkg_receive_commitments(int participant_id, void *sub_socket);
int dkg_receive_and_verify_shares(int participant_id, void *sub_socket);

#endif // A2L_ECDSA_INCLUDE_DKG_INTEGRATION
