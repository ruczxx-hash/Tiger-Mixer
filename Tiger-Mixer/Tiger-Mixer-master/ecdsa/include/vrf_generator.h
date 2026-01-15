#ifndef A2L_ECDSA_INCLUDE_VRF_GENERATOR
#define A2L_ECDSA_INCLUDE_VRF_GENERATOR

#include <stddef.h>
#include <stdint.h>
#include "/home/zxx/Config/secp256k1-vrf-master/include/secp256k1-vrf.h"
#include "/home/zxx/Config/relic/include/relic.h"
#include "util.h"

// VRF 生成器状态结构
typedef struct {
    secp256k1_context *ctx;           // secp256k1 上下文
    unsigned char seckey[32];          // 私钥（32字节）
    secp256k1_pubkey pubkey;           // 公钥对象
    unsigned char pk_serialized[33];   // 序列化的公钥（33字节，压缩格式）
} vrf_generator_state_st;

typedef vrf_generator_state_st *vrf_generator_state_t;

// 使用 RELIC 库生成 ECDSA 密钥对（类似 util.c 的方式）
// 使用 RELIC 库函数：ec_curve_get_ord, bn_rand_mod, ec_mul_gen
// 然后转换为 secp256k1 格式
// 输出：seckey[32] 私钥, pk_serialized[33] 压缩公钥
// 返回：1 成功，0 失败
int vrf_generate_ecdsa_keypair_relic(
    unsigned char seckey[32],
    unsigned char pk_serialized[33]
);

// 初始化 VRF 生成器（生成新的密钥对或从文件加载）
// 如果 key_file 为 NULL，则生成新密钥对
// 如果 key_file 不为 NULL，则从文件加载私钥
int vrf_generator_init(vrf_generator_state_t *state, const char *key_file);

// 释放 VRF 生成器状态
void vrf_generator_free(vrf_generator_state_t state);

// 生成 VRF 证明和随机数
// msg: 消息内容
// msglen: 消息长度
// proof: 输出，81字节的证明
// output: 输出，32字节的随机数
// 返回：1 成功，0 失败
int vrf_generator_prove(
    vrf_generator_state_t state,
    const void *msg,
    size_t msglen,
    unsigned char proof[81],
    unsigned char output[32]
);

// 验证 VRF 证明并提取随机数
// proof: 81字节的证明
// pk: 33字节的序列化公钥
// msg: 消息内容
// msglen: 消息长度
// output: 输出，32字节的随机数
// 返回：1 验证成功，0 验证失败
int vrf_generator_verify(
    const unsigned char proof[81],
    const unsigned char pk[33],
    const void *msg,
    size_t msglen,
    unsigned char output[32]
);

// 从证明中提取随机数（不安全，仅当证明已验证时使用）
// proof: 81字节的证明
// output: 输出，32字节的随机数
// 返回：1 成功，0 失败
int vrf_generator_proof_to_hash(
    const unsigned char proof[81],
    unsigned char output[32]
);

// 保存密钥对到文件
int vrf_generator_save_keys(vrf_generator_state_t state, const char *key_file);

// 从文件加载私钥
int vrf_generator_load_keys(vrf_generator_state_t state, const char *key_file);

// 将随机数转换为 uint64_t 范围内的值（0 到 max_value）
// output: 32字节的随机数
// max_value: 最大值
// 返回：0 到 max_value 之间的随机数
uint64_t vrf_random_to_range(const unsigned char output[32], uint64_t max_value);

// 将随机数转换为浮点数范围内的值（0.0 到 1.0）
double vrf_random_to_double(const unsigned char output[32]);

#endif // A2L_ECDSA_INCLUDE_VRF_GENERATOR

