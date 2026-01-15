
#include "vrf_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 使用 RELIC 库生成 ECDSA 密钥对（类似 util.c 中 generate_keys_and_write_to_file 的方式）
// 步骤：
// 1. 初始化 RELIC 库（如果未初始化）
// 2. 获取曲线阶 q
// 3. 生成随机私钥：bn_rand_mod(ec_sk, q) - 在 [0, q) 范围内
// 4. 计算公钥：ec_mul_gen(ec_pk, ec_sk) - 公钥 = 私钥 × 生成元
// 5. 序列化：ec_write_bin() - 压缩格式
// 6. 转换为 secp256k1 格式
int vrf_generate_ecdsa_keypair_relic(
    unsigned char seckey[32],
    unsigned char pk_serialized[33]
) {
    if (!seckey || !pk_serialized) {
        return 0;
    }

    // 初始化 RELIC 库（类似 keygen.c 和 util.c 的方式）
    // 注意：RELIC 库的 core_init() 可以被多次调用，如果已经初始化会返回 RLC_OK
    // 这里每次调用时初始化，确保 RELIC 已准备好
    if (core_init() != RLC_OK) {
        return 0;
    }
    
    // 设置 secp256k1 曲线参数（类似 util.c:35）
    // 这些函数可以安全地多次调用
    ep_param_set_any();
    ep_param_set(SECG_K256);

    bn_t q, ec_sk;
    ec_t ec_pk;
    uint8_t serialized_ec_sk[RLC_BN_SIZE];
    uint8_t serialized_ec_pk[RLC_EC_SIZE_COMPRESSED];

    // 初始化 RELIC 结构
    bn_null(q);
    bn_null(ec_sk);
    ec_null(ec_pk);
    
    bn_new(q);
    bn_new(ec_sk);
    ec_new(ec_pk);

    // 步骤1：获取曲线阶（类似 util.c 中的 ec_curve_get_ord(q)）
    ec_curve_get_ord(q);

    // 步骤2：生成随机私钥（类似 util.c 中的 bn_rand_mod(ec_sk_alice, q)）
    // 在 [0, q) 范围内生成随机数
    bn_rand_mod(ec_sk, q);

    // 步骤3：计算公钥（类似 util.c 中的 ec_mul_gen(ec_pk_alice, ec_sk_alice)）
    // 公钥 = 私钥 × 生成元 G
    ec_mul_gen(ec_pk, ec_sk);

    // 步骤4：序列化私钥（转换为字节）
    bn_write_bin(serialized_ec_sk, RLC_BN_SIZE, ec_sk);
    
    // RELIC 的 bn_write_bin 可能会填充到 RLC_BN_SIZE (通常大于32字节)
    // 提取后32字节（如果小于32字节则前面补0）
    if (RLC_BN_SIZE >= 32) {
        // 取最后32字节
        memcpy(seckey, serialized_ec_sk + (RLC_BN_SIZE - 32), 32);
    } else {
        // 如果小于32字节，前面补0
        memset(seckey, 0, 32);
        memcpy(seckey + (32 - RLC_BN_SIZE), serialized_ec_sk, RLC_BN_SIZE);
    }

    // 步骤5：序列化公钥为压缩格式（类似 util.c 中的 ec_write_bin(..., 1)）
    // 1 表示压缩格式
    ec_write_bin(serialized_ec_pk, RLC_EC_SIZE_COMPRESSED, ec_pk, 1);

    // 复制到输出（RELIC 的压缩格式也是33字节）
    if (RLC_EC_SIZE_COMPRESSED == 33) {
        memcpy(pk_serialized, serialized_ec_pk, 33);
    } else {
        // 如果长度不匹配，返回错误
        bn_free(q);
        bn_free(ec_sk);
        ec_free(ec_pk);
        memset(seckey, 0, 32);
        return 0;
    }

    // 清理
    memzero(serialized_ec_sk, RLC_BN_SIZE);
    memzero(serialized_ec_pk, RLC_EC_SIZE_COMPRESSED);
    bn_free(q);
    bn_free(ec_sk);
    ec_free(ec_pk);

    // 注意：不清理 RELIC core，因为可能被其他地方使用
    // 如果需要完全清理，可以在程序退出时调用 core_clean()
    
    return 1;
}


// 初始化 VRF 生成器
int vrf_generator_init(vrf_generator_state_t *state, const char *key_file) {
    if (!state) {
        return 0;
    }

    // 分配内存
    *state = (vrf_generator_state_t)malloc(sizeof(vrf_generator_state_st));
    if (!*state) {
        return 0;
    }
    memset(*state, 0, sizeof(vrf_generator_state_st));

    // 创建上下文
    (*state)->ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!(*state)->ctx) {
        free(*state);
        *state = NULL;
        return 0;
    }

    // 生成新密钥对或从文件加载
    if (key_file && strlen(key_file) > 0) {
        // 从文件加载私钥
        if (vrf_generator_load_keys(*state, key_file) != 1) {
            secp256k1_context_destroy((*state)->ctx);
            free(*state);
            *state = NULL;
            return 0;
        }
        
        // 从已加载的私钥创建 secp256k1 公钥对象和序列化公钥
        if (secp256k1_ec_pubkey_create((*state)->ctx, &((*state)->pubkey), (*state)->seckey) != 1) {
            secp256k1_context_destroy((*state)->ctx);
            memset((*state)->seckey, 0, 32);
            free(*state);
            *state = NULL;
            return 0;
        }
        
        // 序列化公钥为压缩格式（33字节）
        size_t pk_len = 33;
        if (secp256k1_ec_pubkey_serialize((*state)->ctx, (*state)->pk_serialized, &pk_len, 
                                          &((*state)->pubkey), SECP256K1_EC_COMPRESSED) != 1) {
            secp256k1_context_destroy((*state)->ctx);
            memset((*state)->seckey, 0, 32);
            free(*state);
            *state = NULL;
            return 0;
        }
    } else {
        // 生成新的密钥对（使用 RELIC 库，类似 util.c 的方式）
        // 调用 RELIC 库生成密钥对
        if (vrf_generate_ecdsa_keypair_relic((*state)->seckey, 
                                             (*state)->pk_serialized) != 1) {
            secp256k1_context_destroy((*state)->ctx);
            free(*state);
            *state = NULL;
            return 0;
        }

        // 从已生成的私钥创建 secp256k1 公钥对象（用于 VRF 操作）
        // 注意：这里需要将 RELIC 格式的私钥转换为 secp256k1 格式
        // RELIC 和 secp256k1 使用相同的 secp256k1 曲线，但内部表示可能不同
        if (secp256k1_ec_pubkey_create((*state)->ctx, &((*state)->pubkey), (*state)->seckey) != 1) {
            secp256k1_context_destroy((*state)->ctx);
            memset((*state)->seckey, 0, 32);
            free(*state);
            *state = NULL;
            return 0;
        }
    }

    return 1;
}

// 释放 VRF 生成器状态
void vrf_generator_free(vrf_generator_state_t state) {
    if (!state) {
        return;
    }

    if (state->ctx) {
        secp256k1_context_destroy(state->ctx);
    }

    // 清除私钥
    memset(state->seckey, 0, 32);
    memset(state->pk_serialized, 0, 33);

    free(state);
}

// 生成 VRF 证明和随机数
int vrf_generator_prove(
    vrf_generator_state_t state,
    const void *msg,
    size_t msglen,
    unsigned char proof[81],
    unsigned char output[32]
) {
    if (!state || !msg || msglen == 0 || !proof || !output) {
        return 0;
    }

    // 生成证明
    if (secp256k1_vrf_prove(proof, state->seckey, &(state->pubkey), msg, (unsigned int)msglen) != 1) {
        return 0;
    }

    // 从证明中提取随机数
    if (secp256k1_vrf_proof_to_hash(output, proof) != 1) {
        return 0;
    }

    return 1;
}

// 验证 VRF 证明并提取随机数
int vrf_generator_verify(
    const unsigned char proof[81],
    const unsigned char pk[33],
    const void *msg,
    size_t msglen,
    unsigned char output[32]
) {
    if (!proof || !pk || !msg || msglen == 0 || !output) {
        return 0;
    }

    // 验证并提取随机数
    return secp256k1_vrf_verify(output, proof, pk, msg, (unsigned int)msglen);
}

// 从证明中提取随机数
int vrf_generator_proof_to_hash(
    const unsigned char proof[81],
    unsigned char output[32]
) {
    if (!proof || !output) {
        return 0;
    }

    return secp256k1_vrf_proof_to_hash(output, proof);
}

// 保存密钥对到文件（保存私钥，公钥可以从私钥重新计算）
int vrf_generator_save_keys(vrf_generator_state_t state, const char *key_file) {
    if (!state || !key_file) {
        return 0;
    }

    FILE *f = fopen(key_file, "wb");
    if (!f) {
        return 0;
    }

    // 写入私钥（32字节）
    size_t written = fwrite(state->seckey, 1, 32, f);
    fclose(f);

    return (written == 32) ? 1 : 0;
}

// 从文件加载私钥
int vrf_generator_load_keys(vrf_generator_state_t state, const char *key_file) {
    if (!state || !key_file) {
        return 0;
    }

    FILE *f = fopen(key_file, "rb");
    if (!f) {
        return 0;
    }

    // 读取私钥（32字节）
    size_t read = fread(state->seckey, 1, 32, f);
    fclose(f);

    if (read != 32) {
        memset(state->seckey, 0, 32);
        return 0;
    }

    // 验证私钥有效性
    secp256k1_context *temp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    int valid = secp256k1_ec_seckey_verify(temp_ctx, state->seckey);
    secp256k1_context_destroy(temp_ctx);

    if (!valid) {
        memset(state->seckey, 0, 32);
        return 0;
    }

    return 1;
}

// 将随机数转换为范围内的值
uint64_t vrf_random_to_range(const unsigned char output[32], uint64_t max_value) {
    if (max_value == 0) {
        return 0;
    }

    // 将32字节的随机数转换为 uint64_t
    uint64_t random_value;
    memcpy(&random_value, output, sizeof(uint64_t));

    // 使用模运算映射到 0 到 max_value
    return random_value % (max_value + 1);
}

// 将随机数转换为浮点数（0.0 到 1.0）
double vrf_random_to_double(const unsigned char output[32]) {
    // 将32字节的随机数转换为 uint64_t
    uint64_t random_value;
    memcpy(&random_value, output, sizeof(uint64_t));

    // 转换为 0.0 到 1.0 之间的浮点数
    // 使用最大 uint64_t 值作为分母
    const uint64_t max_uint64 = 0xFFFFFFFFFFFFFFFFULL;
    return (double)random_value / (double)max_uint64;
}

