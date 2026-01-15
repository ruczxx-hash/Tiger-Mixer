#ifndef A2L_ECDSA_INCLUDE_TYPES
#define A2L_ECDSA_INCLUDE_TYPES

#include "/home/zxx/Config/relic/include/relic.h"
#include "pari/pari.h"

typedef struct {
  char *type;
  uint8_t *data;
} message_st;

typedef message_st *message_t;

#define message_null(message) message = NULL;

#define message_new(message, type_length, data_length)                  \
  do {                                                                  \
    message = malloc(sizeof(message_st));                               \
    if (message == NULL) {                                              \
      RLC_THROW(ERR_NO_MEMORY);                                         \
    }                                                                   \
    (message)->type = malloc(sizeof(char) * type_length);               \
    if ((message)->type == NULL) {                                      \
      RLC_THROW(ERR_NO_MEMORY);                                         \
    }                                                                   \
    (message)->data = malloc(sizeof(uint8_t) * data_length);            \
    if ((message)->data == NULL) {                                      \
      RLC_THROW(ERR_NO_MEMORY);                                         \
    }                                                                   \
  } while (0)

#define message_free(message)                                           \
  do {                                                                  \
    free((message)->type);                                              \
    free((message)->data);                                              \
    free(message);                                                      \
    message = NULL;                                                     \
  } while (0)

typedef struct {
  ec_t a;
  ec_t b;
  bn_t z;
} zk_proof_st;

typedef zk_proof_st *zk_proof_t;

#define zk_proof_null(proof) proof = NULL;

#define zk_proof_new(proof)                   \
  do {                                        \
    proof = malloc(sizeof(zk_proof_st));      \
    if (proof == NULL) {                      \
      RLC_THROW(ERR_NO_MEMORY);               \
    }                                         \
    ec_new((proof)->a);                       \
    ec_new((proof)->b);                       \
    bn_new((proof)->z);                       \
  } while (0)

#define zk_proof_free(proof)                  \
  do {                                        \
    ec_free((proof)->a);                      \
    ec_free((proof)->b);                      \
    bn_free((proof)->z);                      \
    free(proof);                              \
    proof = NULL;                             \
  } while (0)

typedef struct {
  GEN t1;
  ec_t t2;
  GEN t3;
  GEN u1;
  GEN u2;
} zk_proof_cldl_st;

typedef zk_proof_cldl_st *zk_proof_cldl_t;

#define zk_proof_cldl_null(proof) proof = NULL;

#define zk_proof_cldl_new(proof)              \
  do {                                        \
    proof = malloc(sizeof(zk_proof_cldl_st)); \
    if (proof == NULL) {                      \
      RLC_THROW(ERR_NO_MEMORY);               \
    }                                         \
    ec_new((proof)->t2);                      \
  } while (0)

#define zk_proof_cldl_free(proof)             \
  do {                                        \
    ec_free((proof)->t2);                     \
    free(proof);                              \
    proof = NULL;                             \
  } while (0)

// 新增：隐藏值关联性零知识证明结构
// 证明知道隐藏的(g^α, ctx_α, auditor_ctx_α)与公开的(g^{αβ}, ctx_αβ, auditor_ctx_αβ)用同一个β关联
typedef struct {
  // 承诺部分：隐藏原始值
  ec_t T_g_alpha;                  // g^α^r (承诺g^α)
  GEN T_ctx_alpha_c1;              // ctx_α.c1^r (承诺ctx_α.c1)
  GEN T_ctx_alpha_c2;              // ctx_α.c2^r (承诺ctx_α.c2)
  GEN T_auditor_ctx_alpha_c1;      // auditor_ctx_α.c1^r (承诺auditor_ctx_α.c1)
  GEN T_auditor_ctx_alpha_c2;      // auditor_ctx_α.c2^r (承诺auditor_ctx_α.c2)
  
  // 响应部分：证明知道β
  GEN u1;                          // response u1 = r + β * k
  GEN u2;                          // response u2 = r + β * k  
  GEN u3;                          // response u3 = r + β * k (for EC)
  
  // 验证部分：用于验证关联性
  GEN t1_c1;                       // t1_c1 for verification
  GEN t1_c2;                       // t1_c2 for verification
  GEN t2_c1;                       // t2_c1 for verification
  GEN t2_c2;                       // t2_c2 for verification
  ec_t t3;                         // t3 for EC verification
} zk_proof_malleability_st;

// Bob的谜题零知识证明结构体
// 证明知道见证(α*β, r₀*β)，使得：
// α = Enc(pk₀, r₀*β), β = Enc(pk₁, α*β), γ = g^(α*β), 谜题 = g^(α*β)
typedef struct {
  // 承诺部分
  GEN t_alpha_c1;             // ctx_α.c1^r (承诺ctx_α.c1)
  GEN t_alpha_c2;             // ctx_α.c2^r (承诺ctx_α.c2)
  GEN t_beta_c1;              // ctx_β.c1^r (承诺ctx_β.c1) 
  GEN t_beta_c2;              // ctx_β.c2^r (承诺ctx_β.c2)
  ec_t t_gamma;               // g^(α*β)^r (承诺g^(α*β))
  ec_t t_puzzle;              // g^(α*β)^r (承诺g^(α*β))
  
  // 响应部分
  GEN u1;                     // u1 = r + (α*β) * k (for ctx_α)
  GEN u2;                     // u2 = r + (α*β) * k (for ctx_β)
  bn_t u3;                    // u3 = r + (α*β) * k (for g^(α*β))
  bn_t u4;                    // u4 = r + (α*β) * k (for g^(α*β))
} zk_proof_bob_puzzle_st;


typedef zk_proof_bob_puzzle_st *zk_proof_bob_puzzle_t;

#define zk_proof_bob_puzzle_null(proof) proof = NULL;

#define zk_proof_bob_puzzle_new(proof)              \
  do {                                               \
    proof = malloc(sizeof(zk_proof_bob_puzzle_st)); \
    if (proof == NULL) {                             \
      RLC_THROW(ERR_NO_MEMORY);                      \
    }                                                \
    ec_null(proof->t_gamma);                        \
    ec_null(proof->t_puzzle);                       \
    bn_null(proof->u3);                             \
    bn_null(proof->u4);                             \
  } while (0)

#define zk_proof_bob_puzzle_free(proof)             \
  do {                                               \
    if (proof != NULL) {                             \
      ec_free(proof->t_gamma);                      \
      ec_free(proof->t_puzzle);                     \
      bn_free(proof->u3);                           \
      bn_free(proof->u4);                           \
      free(proof);                                   \
      proof = NULL;                                  \
    }                                                \
  } while (0)

typedef zk_proof_malleability_st *zk_proof_malleability_t;

#define zk_proof_malleability_null(proof) proof = NULL;

#define zk_proof_malleability_new(proof)              \
  do {                                               \
    proof = malloc(sizeof(zk_proof_malleability_st)); \
    if (proof == NULL) {                             \
      RLC_THROW(ERR_NO_MEMORY);                      \
    }                                                \
    ec_null(proof->T_g_alpha);                       \
    ec_null(proof->t3);                              \
  } while (0)

#define zk_proof_malleability_free(proof)             \
  do {                                               \
    if (proof != NULL) {                             \
      ec_free(proof->T_g_alpha);                     \
      ec_free(proof->t3);                            \
      free(proof);                                   \
      proof = NULL;                                  \
    }                                                \
  } while (0)



typedef struct {
  bn_t c;
  ec_t r;
} commit_st;

typedef commit_st *commit_t;

#define commit_null(commit) commit = NULL;

#define commit_new(commit)                    \
  do {                                        \
    commit = malloc(sizeof(commit_st));       \
    if (commit == NULL) {                     \
      RLC_THROW(ERR_NO_MEMORY);               \
    }                                         \
    bn_new((commit)->c);                      \
    ec_new((commit)->r);                      \
  } while (0)

#define commit_free(commit)                   \
  do {                                        \
    bn_free((commit)->c);                     \
    ec_free((commit)->r);                     \
    free(commit);                             \
    commit = NULL;                            \
  } while (0)

typedef struct {
  g1_t c;
} pedersen_com_st;

typedef pedersen_com_st *pedersen_com_t;

#define pedersen_com_null(com) com = NULL;

#define pedersen_com_new(com)                 \
  do {                                        \
    com = malloc(sizeof(pedersen_com_st));    \
    if (com == NULL) {                        \
      RLC_THROW(ERR_NO_MEMORY);               \
    }                                         \
    g1_new((com)->c);                         \
  } while (0)

#define pedersen_com_free(com)                \
  do {                                        \
    g1_free((com)->c);                        \
    free(com);                                \
    com = NULL;                               \
  } while (0)

typedef struct {
  bn_t r;
  bn_t m;
} pedersen_decom_st;

typedef pedersen_decom_st *pedersen_decom_t;

#define pedersen_decom_null(decom) decom = NULL;

#define pedersen_decom_new(decom)             \
  do {                                        \
    decom = malloc(sizeof(pedersen_decom_st));\
    if (decom == NULL) {                      \
      RLC_THROW(ERR_NO_MEMORY);               \
    }                                         \
    bn_new((decom)->r);                       \
    bn_new((decom)->m);                       \
  } while (0)

#define pedersen_decom_free(decom)            \
  do {                                        \
    bn_free((decom)->r);                      \
    bn_free((decom)->m);                      \
    free(decom);                              \
    decom = NULL;                             \
  } while (0)

typedef struct {
  pedersen_com_t c;
  bn_t u;
  bn_t v;
} pedersen_com_zk_proof_st;

typedef pedersen_com_zk_proof_st *pedersen_com_zk_proof_t;

#define pedersen_com_zk_proof_null(proof) proof = NULL;

#define pedersen_com_zk_proof_new(proof)              \
  do {                                                \
    proof = malloc(sizeof(pedersen_com_zk_proof_st)); \
    if (proof == NULL) {                              \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    pedersen_com_new((proof)->c);                     \
    bn_new((proof)->u);                               \
    bn_new((proof)->v);                               \
  } while (0)

#define pedersen_com_zk_proof_free(proof)             \
  do {                                                \
    pedersen_com_free((proof)->c);                    \
    bn_free((proof)->u);                              \
    bn_free((proof)->v);                              \
    free(proof);                                      \
    proof = NULL;                                     \
  } while (0)

typedef struct {
  GEN Delta_K;  // fundamental discriminant
  GEN E;        // the secp256k1 elliptic curve
  GEN q;        // the order of the elliptic curve
  GEN G;        // the generator of the elliptic curve group
  GEN g_q;      // the generator of G^q
  GEN bound;    // the bound for exponentiation
} cl_params_st;

typedef cl_params_st *cl_params_t;

#define cl_params_null(params) params = NULL;

#define cl_params_new(params)                         \
  do {                                                \
    params = malloc(sizeof(cl_params_st));            \
    if (params == NULL) {                             \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    (params)->Delta_K = gen_0;                        \
    (params)->E = gen_0;                              \
    (params)->q = gen_0;                              \
    (params)->G = gen_0;                              \
    (params)->g_q = gen_0;                            \
    (params)->bound = gen_0;                          \
  } while (0)

#define cl_params_free(params)                        \
  do {                                                \
    free(params);                                     \
    params = NULL;                                    \
  } while (0)

typedef struct {
  GEN c1;
  GEN c2;
  GEN r;
} cl_ciphertext_st;

typedef cl_ciphertext_st *cl_ciphertext_t;

#define cl_ciphertext_null(ciphertext) ciphertext = NULL;

#define cl_ciphertext_new(ciphertext)                 \
  do {                                                \
    ciphertext = malloc(sizeof(cl_ciphertext_st));    \
    if (ciphertext == NULL) {                         \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    (ciphertext)->c1 = gen_0;                         \
    (ciphertext)->c2 = gen_0;                         \
    (ciphertext)->r = gen_0;                          \
  } while (0)

#define cl_ciphertext_free(ciphertext)                \
  do {                                                \
    free(ciphertext);                                 \
    ciphertext = NULL;                                \
  } while (0)

typedef struct {
  GEN sk;
} cl_secret_key_st;

typedef cl_secret_key_st *cl_secret_key_t;

#define cl_secret_key_null(secret_key) secret_key = NULL;

#define cl_secret_key_new(secret_key)                 \
  do {                                                \
    secret_key = malloc(sizeof(cl_secret_key_st));    \
    if (secret_key == NULL) {                         \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    (secret_key)->sk = gen_0;                         \
  } while (0)

#define cl_secret_key_free(secret_key)                \
  do {                                                \
    free(secret_key);                                 \
    secret_key = NULL;                                \
  } while (0)


typedef struct {
  GEN pk;
} cl_public_key_st;

typedef cl_public_key_st *cl_public_key_t;

#define cl_public_key_null(public_key) public_key = NULL;

#define cl_public_key_new(public_key)                 \
  do {                                                \
    public_key = malloc(sizeof(cl_public_key_st));    \
    if (public_key == NULL) {                         \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    (public_key)->pk = gen_0;                         \
  } while (0)

#define cl_public_key_free(public_key)                \
  do {                                                \
    free(public_key);                                 \
    public_key = NULL;                                \
  } while (0)

typedef struct {
  bn_t sk;
} ec_secret_key_st;

typedef ec_secret_key_st *ec_secret_key_t;

#define ec_secret_key_null(secret_key) secret_key = NULL;

#define ec_secret_key_new(secret_key)                 \
  do {                                                \
    secret_key = malloc(sizeof(ec_secret_key_st));    \
    if (secret_key == NULL) {                         \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    bn_new((secret_key)->sk);                         \
  } while (0)

#define ec_secret_key_free(secret_key)                \
  do {                                                \
    bn_free((secret_key)->sk);                        \
    free(secret_key);                                 \
    secret_key = NULL;                                \
  } while (0)

typedef struct {
  ec_t pk;
} ec_public_key_st;

typedef ec_public_key_st *ec_public_key_t;

#define ec_public_key_null(public_key) public_key = NULL;

#define ec_public_key_new(public_key)                 \
  do {                                                \
    public_key = malloc(sizeof(ec_public_key_st));    \
    if (public_key == NULL) {                         \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    ec_new((public_key)->pk);                         \
  } while (0)

#define ec_public_key_free(public_key)                \
  do {                                                \
    ec_free((public_key)->pk);                        \
    free(public_key);                                 \
    public_key = NULL;                                \
  } while (0)

typedef struct {
  bn_t r;
  bn_t s;
  ec_t R;
  zk_proof_t pi;
} ecdsa_signature_st;

typedef ecdsa_signature_st *ecdsa_signature_t;

#define ecdsa_signature_null(signature) signature = NULL;

#define ecdsa_signature_new(signature)                \
  do {                                                \
    signature = malloc(sizeof(ecdsa_signature_st));   \
    if (signature == NULL) {                          \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    bn_new((signature)->r);                           \
    bn_new((signature)->s);                           \
    ec_new((signature)->R);                           \
    zk_proof_new((signature)->pi);                    \
  } while (0)

#define ecdsa_signature_free(signature)               \
  do {                                                \
    bn_free((signature)->r);                          \
    bn_free((signature)->s);                          \
    ec_free((signature)->R);                          \
    zk_proof_free((signature)->pi);                   \
    free(signature);                                  \
    signature = NULL;                                 \
  } while (0)

typedef struct {
  g1_t sigma_1;
  g1_t sigma_2;
} ps_signature_st;

typedef ps_signature_st *ps_signature_t;

#define ps_signature_null(signature) signature = NULL;

#define ps_signature_new(signature)                   \
  do {                                                \
    signature = malloc(sizeof(ps_signature_st));      \
    if (signature == NULL) {                          \
      RLC_THROW(ERR_NO_MEMORY);                       \
    }                                                 \
    g1_new((signature)->sigma_1);                     \
    g1_new((signature)->sigma_2);                     \
  } while (0)

#define ps_signature_free(signature)                  \
  do {                                                \
    g1_free((signature)->sigma_1);                    \
    g1_free((signature)->sigma_2);                    \
    free(signature);                                  \
    signature = NULL;                                 \
  } while (0)

typedef struct {
  g1_t Y_1;
  g2_t X_2;
  g2_t Y_2;
} ps_public_key_st;

typedef ps_public_key_st *ps_public_key_t;

#define ps_public_key_null(public_key) public_key = NULL;

#define ps_public_key_new(public_key)                   \
  do {                                                  \
    public_key = malloc(sizeof(ps_public_key_st));      \
    if (public_key == NULL) {                           \
      RLC_THROW(ERR_NO_MEMORY);                         \
    }                                                   \
    g1_new((public_key)->Y_1);                          \
    g2_new((public_key)->X_2);                          \
    g2_new((public_key)->Y_2);                          \
  } while (0)

#define ps_public_key_free(public_key)                  \
  do {                                                  \
    g1_free((public_key)->Y_1);                         \
    g2_free((public_key)->X_2);                         \
    g2_free((public_key)->Y_2);                         \
    free(public_key);                                   \
    public_key = NULL;                                  \
  } while (0)

typedef struct {
  g1_t X_1;
} ps_secret_key_st;

typedef ps_secret_key_st *ps_secret_key_t;

#define ps_secret_key_null(secret_key) secret_key = NULL;

#define ps_secret_key_new(secret_key)                   \
  do {                                                  \
    secret_key = malloc(sizeof(ps_secret_key_st));      \
    if (secret_key == NULL) {                           \
      RLC_THROW(ERR_NO_MEMORY);                         \
    }                                                   \
    g1_new((secret_key)->X_1);                          \
  } while (0)

#define ps_secret_key_free(secret_key)                  \
  do {                                                  \
    g1_free((secret_key)->X_1);                         \
    free(secret_key);                                   \
    secret_key = NULL;                                  \
  } while (0)

// ================= CL 乘法缩放型（变换型）一致性证明 =================
// 语义：证明存在 (r, m) 使得 c1 = g_q^r 且 c2 = pk^r * f^m。
// 该证明的挑战为 CRS-only（仅依赖 pk 与参数），因此可对 (T1,T3,z_r,z_m) 做公开缩放：
// scale_s(π) = (T1^s, T3^s, s*z_r, s*z_m)。对应 ct' = (c1^s, c2^s)。

typedef struct {
  GEN T1;     // 类群元素，对应 pk^{w_r} * f^{w_m}
  GEN T3;     // g_q^{w_r}
  bn_t z_r;   // 响应：w_r + e*r （mod q）
  GEN z_m;    // 响应：w_m + e*m
} cl_mul_eq_proof_st;

typedef cl_mul_eq_proof_st* cl_mul_eq_proof_t;

// 简易分配/释放宏
#define cl_mul_eq_proof_null(p) p = NULL;
#define cl_mul_eq_proof_new(p)                         \
  do {                                                 \
    p = (cl_mul_eq_proof_t)malloc(sizeof(cl_mul_eq_proof_st)); \
    if (p != NULL) {                                   \
      bn_new((p)->z_r);                                \
      (p)->T1 = NULL; (p)->T3 = NULL; (p)->z_m = NULL; \
    }                                                  \
  } while (0)
#define cl_mul_eq_proof_free(p)                        \
  do {                                                 \
    if ((p) != NULL) {                                 \
      bn_free((p)->z_r);                               \
      (p)->T1 = NULL; (p)->T3 = NULL; (p)->z_m = NULL; \
      free(p);                                         \
      (p) = NULL;                                      \
    }                                                  \
  } while (0)

// 新增：完整的三证明结构（P1+P2+P3）
typedef struct {
  // P1: 证明β'与β的同态关系
  zk_proof_t proof_homomorphic;
  
  // P2: 证明σ' = Enc(pk2, β')
  zk_proof_cldl_t proof_encryption;
  
  // P3: 证明σ''是σ'的盲化（通过Pedersen承诺）
  pedersen_com_zk_proof_t proof_blinding;
  g1_t commitment;  // σ''的承诺
  
  // 额外信息
  uint8_t inner_hash[RLC_MD_LEN];  // H(β') 用于验证
  cl_mul_eq_proof_t pi_cl_beta;    // CL缩放证明

  // 升级：分别对 outer.c1 与 outer.c2 的承诺与开口证明
  pedersen_com_zk_proof_t proof_c1;
  pedersen_com_zk_proof_t proof_c2;
  g1_t commitment_c1;
  g1_t commitment_c2;
} complete_nizk_proof_st;

typedef complete_nizk_proof_st* complete_nizk_proof_t;

#define complete_nizk_proof_null(p) p = NULL;

#define complete_nizk_proof_new(p) \
  do { \
    p = (complete_nizk_proof_t)malloc(sizeof(complete_nizk_proof_st)); \
    if (p) { \
      zk_proof_new((p)->proof_homomorphic); \
      zk_proof_cldl_new((p)->proof_encryption); \
      pedersen_com_zk_proof_new((p)->proof_blinding); \
      g1_new((p)->commitment); \
      cl_mul_eq_proof_new((p)->pi_cl_beta); \
      pedersen_com_zk_proof_new((p)->proof_c1); \
      pedersen_com_zk_proof_new((p)->proof_c2); \
      g1_new((p)->commitment_c1); \
      g1_new((p)->commitment_c2); \
    } \
  } while (0)

#define complete_nizk_proof_free(p) \
  do { \
    if (p) { \
      zk_proof_free((p)->proof_homomorphic); \
      zk_proof_cldl_free((p)->proof_encryption); \
      pedersen_com_zk_proof_free((p)->proof_blinding); \
      g1_free((p)->commitment); \
      cl_mul_eq_proof_free((p)->pi_cl_beta); \
      pedersen_com_zk_proof_free((p)->proof_c1); \
      pedersen_com_zk_proof_free((p)->proof_c2); \
      g1_free((p)->commitment_c1); \
      g1_free((p)->commitment_c2); \
      free(p); \
      (p) = NULL; \
    } \
  } while (0)

// ================= Joint-Feldman DKG 相关数据结构 =================

// DKG参与者状态
typedef struct {
  int participant_id;           // 参与者ID (1到n)
  int n_participants;           // 总参与者数量
  int threshold;                // 阈值t
  bn_t secret_share;            // 该参与者的秘密份额 s_i = ∑ s_{j→i}
  bn_t secret_poly_coeffs[16];  // 多项式系数 a_{i,0}, a_{i,1}, ..., a_{i,t} (最多支持t=16)
  GEN commitments[16];          // Feldman承诺 A_{i,0}, A_{i,1}, ..., A_{i,t} 其中 A_{i,j} = g_q^{a_{i,j}} (Class Group元素)
  GEN public_key;               // 最终生成的公钥 pk = ∏ y_i (y_i = A_{i,0}) (Class Group元素)
  bn_t private_key;             // 最终生成的私钥份额 sk_j = ∑ s_{i→j} (仅在重构时使用)
  int is_initialized;           // 是否已初始化
} dkg_participant_st;

typedef dkg_participant_st *dkg_participant_t;

#define dkg_participant_null(p) (p) = NULL;

#define dkg_participant_new(p) \
  do { \
    (p) = (dkg_participant_t)malloc(sizeof(dkg_participant_st)); \
    if ((p)) { \
      bn_new((p)->secret_share); \
      for (int i = 0; i < 16; i++) { \
        bn_new((p)->secret_poly_coeffs[i]); \
        (p)->commitments[i] = NULL; \
      } \
      (p)->public_key = NULL; \
      bn_new((p)->private_key); \
      (p)->is_initialized = 0; \
    } \
  } while (0)

#define dkg_participant_free(p) \
  do { \
    if ((p)) { \
      bn_free((p)->secret_share); \
      for (int i = 0; i < 16; i++) { \
        bn_free((p)->secret_poly_coeffs[i]); \
      } \
      bn_free((p)->private_key); \
      free((p)); \
      (p) = NULL; \
    } \
  } while (0)

// DKG协议状态 (Class Group 版本)
typedef struct {
  dkg_participant_t participants[32];  // 最多支持32个参与者
  int n_participants;                  // 实际参与者数量
  int threshold;                       // 阈值
  GEN generator_g;                     // 生成元 g_q (Class Group)
  cl_params_t cl_params;               // Class Group 参数
  bn_t order;                          // 群的阶
  int phase;                           // 协议阶段: 0=初始化, 1=承诺, 2=验证, 3=完成
  int is_initialized;                  // 是否已初始化
} dkg_protocol_st;

typedef dkg_protocol_st *dkg_protocol_t;

#define dkg_protocol_null(p) (p) = NULL;

#define dkg_protocol_new(p) \
  do { \
    (p) = (dkg_protocol_t)malloc(sizeof(dkg_protocol_st)); \
    if ((p)) { \
      (p)->generator_g = NULL; \
      (p)->cl_params = NULL; \
      bn_new((p)->order); \
      (p)->n_participants = 0; \
      (p)->threshold = 0; \
      (p)->phase = 0; \
      (p)->is_initialized = 0; \
      for (int i = 0; i < 32; i++) { \
        (p)->participants[i] = NULL; \
      } \
    } \
  } while (0)

#define dkg_protocol_free(p) \
  do { \
    if ((p)) { \
      bn_free((p)->order); \
      for (int i = 0; i < 32; i++) { \
        if ((p)->participants[i]) { \
          dkg_participant_free((p)->participants[i]); \
        } \
      } \
      free((p)); \
      (p) = NULL; \
    } \
  } while (0)

// DKG消息类型
typedef enum {
  DKG_MSG_COMMITMENT = 1,    // 承诺消息
  DKG_MSG_SHARE = 2,         // 份额消息
  DKG_MSG_COMPLAINT = 3,     // 投诉消息
  DKG_MSG_RESPONSE = 4,      // 响应消息
  DKG_MSG_VERIFICATION = 5   // 验证消息
} dkg_message_type_t;

// DKG消息结构
typedef struct {
  dkg_message_type_t type;
  int sender_id;
  int receiver_id;            // 0表示广播
  uint8_t *data;
  size_t data_len;
} dkg_message_st;

typedef dkg_message_st *dkg_message_t;

#define dkg_message_null(m) (m) = NULL;

#define dkg_message_new(m, msg_type, sender, receiver, data, len) \
  do { \
    (m) = (dkg_message_t)malloc(sizeof(dkg_message_st)); \
    if ((m)) { \
      (m)->type = (msg_type); \
      (m)->sender_id = (sender); \
      (m)->receiver_id = (receiver); \
      (m)->data_len = (len); \
      if ((len) > 0) { \
        (m)->data = (uint8_t*)malloc((len)); \
        if ((m)->data && (data)) { \
          memcpy((m)->data, (data), (len)); \
        } \
      } else { \
        (m)->data = NULL; \
      } \
    } \
  } while (0)

#define dkg_message_free(m) \
  do { \
    if ((m)) { \
      if ((m)->data) free((m)->data); \
      free((m)); \
      (m) = NULL; \
    } \
  } while (0)

#endif // A2L_ECDSA_INCLUDE_TYPES