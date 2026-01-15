#ifndef COMPOSITE_MALLEABLE_PROOF_H
#define COMPOSITE_MALLEABLE_PROOF_H

#include "relic.h"
#include "gs.h"
#include "types.h"

// 复合可塑性证明结构 - 证明图片中的复杂关系
typedef struct {
    // 基础组件
    gs_crs_t crs;                          // 公共参考字符串
    
    // 见证承诺 (针对双见证)
    gs_commitment_t commitment_r0;         // Com(r₀; rand_r0)
    gs_commitment_t commitment_alpha;      // Com(α; rand_alpha)
    
    // 开知证明 (证明知道承诺的开口)
    gs_open_proof_t opening_proof_r0;     // 证明知道r₀和随机数
    gs_open_proof_t opening_proof_alpha;  // 证明知道α和随机数
    
    // 关系证明组件
    // 1. CL加密一致性证明: β ≡ CL_Enc(tumbler_pk, α)
    gs_lin_proof_t cl_consistency_proof;
    
    // 2. Auditor加密一致性证明: α ≡ Auditor_Enc(auditor_pk, r₀)  
    gs_lin_proof_t auditor_consistency_proof;
    
    // 3. 椭圆曲线关系证明: γ ≡ g^α
    gs_eq_proof_t ec_relation_proof;
    
    // Fiat-Shamir挑战
    bn_t challenge;
    
    // 响应 (针对复合见证)
    bn_t response_r0;        // 对r₀的响应
    bn_t response_alpha;     // 对α的响应
    bn_t response_rand_r0;   // 对r₀随机数的响应
    bn_t response_rand_alpha; // 对α随机数的响应
    
    // 元数据
    uint8_t proof_id[32];
    uint32_t proof_version;
    
    // 可塑性变换函数
    bn_t transform_factor_alpha;  // α的变换因子
    bn_t transform_factor_r0;     // r₀的变换因子
    
} composite_malleable_proof_st;

typedef composite_malleable_proof_st *composite_malleable_proof_t;

// 内存管理宏
#define composite_malleable_proof_null(proof) proof = NULL;

#define composite_malleable_proof_new(proof)                    \
  do {                                                          \
    proof = malloc(sizeof(composite_malleable_proof_st));       \
    if (proof == NULL) {                                        \
      RLC_THROW(ERR_NO_MEMORY);                                 \
    }                                                           \
    gs_crs_new((proof)->crs);                                  \
    gs_commitment_new((proof)->commitment_r0);                 \
    gs_commitment_new((proof)->commitment_alpha);              \
    gs_open_proof_new((proof)->opening_proof_r0);              \
    gs_open_proof_new((proof)->opening_proof_alpha);           \
    gs_lin_proof_new((proof)->cl_consistency_proof);           \
    gs_lin_proof_new((proof)->auditor_consistency_proof);      \
    gs_eq_proof_new((proof)->ec_relation_proof);               \
    bn_new((proof)->challenge);                                \
    bn_new((proof)->response_r0);                              \
    bn_new((proof)->response_alpha);                           \
    bn_new((proof)->response_rand_r0);                         \
    bn_new((proof)->response_rand_alpha);                      \
    bn_new((proof)->transform_factor_alpha);                   \
    bn_new((proof)->transform_factor_r0);                      \
    (proof)->proof_version = 1;                                \
  } while (0)

#define composite_malleable_proof_free(proof)                  \
  do {                                                          \
    if (proof != NULL) {                                       \
      gs_crs_free((proof)->crs);                               \
      gs_commitment_free((proof)->commitment_r0);              \
      gs_commitment_free((proof)->commitment_alpha);           \
      gs_open_proof_free((proof)->opening_proof_r0);           \
      gs_open_proof_free((proof)->opening_proof_alpha);        \
      gs_lin_proof_free((proof)->cl_consistency_proof);        \
      gs_lin_proof_free((proof)->auditor_consistency_proof);   \
      gs_eq_proof_free((proof)->ec_relation_proof);            \
      bn_free((proof)->challenge);                             \
      bn_free((proof)->response_r0);                           \
      bn_free((proof)->response_alpha);                        \
      bn_free((proof)->response_rand_r0);                      \
      bn_free((proof)->response_rand_alpha);                   \
      bn_free((proof)->transform_factor_alpha);                \
      bn_free((proof)->transform_factor_r0);                   \
      free(proof);                                             \
      proof = NULL;                                            \
    }                                                          \
  } while (0)

// 复合关系定义
typedef struct {
    bn_t witness_r0;              // 见证r₀
    bn_t witness_alpha;           // 见证α
    cl_ciphertext_t stmt_auditor_enc;  // 语句: Auditor_Enc(r₀)
    cl_ciphertext_t stmt_cl_enc;       // 语句: CL_Enc(α)
    ec_t stmt_ec_point;               // 语句: g^α
} composite_relation_st;

typedef composite_relation_st *composite_relation_t;

// 函数声明
int composite_malleable_prove(composite_malleable_proof_t proof,
                             const bn_t witness_r0,
                             const bn_t witness_alpha,
                             const cl_ciphertext_t auditor_enc_r0,
                             const cl_ciphertext_t cl_enc_alpha,
                             const ec_t g_to_alpha,
                             const cl_public_key_t auditor_pk,
                             const cl_public_key_t tumbler_pk,
                             const cl_params_t cl_params,
                             const gs_crs_t crs);

int composite_malleable_verify(const composite_malleable_proof_t proof,
                              const cl_ciphertext_t auditor_enc_r0,
                              const cl_ciphertext_t cl_enc_alpha,
                              const ec_t g_to_alpha,
                              const cl_public_key_t auditor_pk,
                              const cl_public_key_t tumbler_pk,
                              const cl_params_t cl_params,
                              const gs_crs_t crs);

int composite_malleable_zkeval(composite_malleable_proof_t proof_out,
                              const composite_malleable_proof_t proof_in,
                              const bn_t beta_factor,
                              const cl_ciphertext_t new_auditor_enc,
                              const cl_ciphertext_t new_cl_enc,
                              const ec_t new_g_to_alpha_beta,
                              const cl_public_key_t auditor_pk,
                              const cl_public_key_t tumbler_pk,
                              const cl_params_t cl_params,
                              const gs_crs_t crs);

// 辅助函数
int compute_composite_fiat_shamir_challenge(bn_t challenge,
                                          const composite_malleable_proof_t proof,
                                          const cl_ciphertext_t auditor_enc_r0,
                                          const cl_ciphertext_t cl_enc_alpha,
                                          const ec_t g_to_alpha,
                                          const gs_crs_t crs);

#endif // COMPOSITE_MALLEABLE_PROOF_H
