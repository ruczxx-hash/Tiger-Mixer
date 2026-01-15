#ifndef A2L_ECDSA_INCLUDE_UTIL
#define A2L_ECDSA_INCLUDE_UTIL

#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "types.h"
#include "gs.h"

// 时间测量结构
typedef struct {
    char name[128];
    double duration_ms;
} timing_record_t;

// 全局时间记录数组
extern timing_record_t timing_records[50];
extern int timing_count;

// 时间测量宏和函数
#define START_TIMER(name) \
    struct timeval start_##name; \
    gettimeofday(&start_##name, NULL); \
    printf("[TIMER] 开始测量: %s\n", #name);

#define END_TIMER(name) \
    struct timeval end_##name; \
    gettimeofday(&end_##name, NULL); \
    double duration_##name = (end_##name.tv_sec - start_##name.tv_sec) * 1000.0 + \
                            (end_##name.tv_usec - start_##name.tv_usec) / 1000.0; \
    printf("[TIMER] %s 耗时: %.2f ms\n", #name, duration_##name); \
    record_timing(#name, duration_##name);

// 函数声明
void record_timing(const char* name, double duration_ms);
void output_timing_to_excel(const char* filename);
void print_timing_summary(void);
double get_timer_value(const char* timer_name);

#define RLC_EC_SIZE_COMPRESSED 33
#define RLC_G1_SIZE_COMPRESSED 33
#define RLC_G2_SIZE_COMPRESSED 65
#define RLC_CL_SECRET_KEY_SIZE 290
#define RLC_CL_PUBLIC_KEY_SIZE 1070
#define RLC_CL_CIPHERTEXT_SIZE 1500
#define RLC_CLDL_PROOF_T1_SIZE 1070
#define RLC_CLDL_PROOF_T2_SIZE 33
#define RLC_CLDL_PROOF_T3_SIZE 1070
#define RLC_CLDL_PROOF_U1_SIZE 315
#define RLC_CLDL_PROOF_U2_SIZE 80

#define CLOCK_PRECISION 1E9

#define ALICE_KEY_FILE_PREFIX "alice"
#define BOB_KEY_FILE_PREFIX "bob"
#define TUMBLER_KEY_FILE_PREFIX "tumbler"
#define AUDITOR_KEY_FILE_PREFIX "auditor"
#define KEY_FILE_EXTENSION "key"

// static uint8_t tx[2] = { 116, 120 }; // "tx"

typedef struct {
    char hash[66];      // 假设hash为64位十六进制+前缀0x
    char from[43];      // 假设地址为42位+前缀0x
    char to[43];
    char value[32];     // 具体长度可根据实际需求调整
    char gasPrice[32];
    char type[10];
    char timestamp[20];
} transaction_t;

int init();
int clean();

void memzero(void *ptr, size_t len);
long long cpucycles(void);
long long ttimer(void);

int serialize_transaction(const transaction_t *tx, uint8_t *out, size_t out_size);

int load_transaction_from_csv(const char* filename, int index, transaction_t* tx);

void serialize_message(uint8_t **serialized,
											 const message_t message,
											 const unsigned msg_type_length,
											 const unsigned msg_data_length);
void deserialize_message(message_t *deserialized_message, const uint8_t *serialized);

int generate_keys_and_write_to_file(const cl_params_t params);
int read_keys_from_file_alice_bob(const char *name,
																	ec_secret_key_t ec_sk,
																	ec_public_key_t ec_pk,
																	ec_public_key_t tumbler_ec_pk,
																	ps_public_key_t tumbler_ps_pk,
																	cl_public_key_t tumbler_cl_pk);
int read_keys_from_file_tumbler(ec_secret_key_t tumbler_ec_sk,
																ec_public_key_t tumbler_ec_pk,
																ps_secret_key_t tumbler_ps_sk,
																ps_public_key_t tumbler_ps_pk,
																cl_secret_key_t tumbler_cl_sk,
																cl_public_key_t tumbler_cl_pk,
																ec_public_key_t alice_ec_pk,
																ec_public_key_t bob_ec_pk);

int read_keys_from_file_auditor(cl_secret_key_t auditor_cl_sk,
                                cl_public_key_t auditor_cl_pk,
                                ec_public_key_t alice_ec_pk,
                                ec_public_key_t bob_ec_pk,
                                ec_public_key_t tumbler_ec_pk,
                                cl_public_key_t tumbler_cl_pk);

int read_auditor_cl_pubkey(cl_public_key_t auditor_cl_pk);
// 生成额外的审计方密钥对并写入 ../keys/<basename>.key
int generate_auditor_keypair_named(const cl_params_t params, const char *basename);
// 从 ../keys/<basename>.key 读取审计方公钥
int read_auditor_cl_pubkey_named(cl_public_key_t auditor_cl_pk, const char *basename);
int read_auditor_cl_seckey_named(cl_secret_key_t auditor_cl_sk, const char *basename);

int generate_cl_params(cl_params_t params);
int cl_enc(cl_ciphertext_t ciphertext,
					 const GEN plaintext,
					 const cl_public_key_t public_key,
					 const cl_params_t params);
int cl_dec(GEN *plaintext,
					 const cl_ciphertext_t ciphertext,
					 const cl_secret_key_t secret_key,
					 const cl_params_t params);

// 公开同态操作（仅依据公参，对密文执行明文加法）
// 语义：返回 c' 满足 Dec(c') = Dec(c) + delta
// 注意：实现为 c2' = c2 * f^{delta}，c1 保持不变
int cl_add_plaintext(cl_ciphertext_t out,
                    const cl_ciphertext_t in,
                    const GEN delta,
                    const cl_params_t params);

// 公开同态加法多次：Dec(c') = Dec(c) + delta1 + delta2
int cl_add_two_plaintexts(cl_ciphertext_t out,
                         const cl_ciphertext_t in,
                         const GEN delta1,
                         const GEN delta2,
                         const cl_params_t params);

// ================= CL 乘法缩放型（变换型）一致性证明 =================
// 注意：cl_mul_eq_proof_t 的定义已移至 types.h 以解决循环依赖

// 生成基准证明：需要明文 m 以及密文 ct（其 r 已记录在 ct->r 中）
int cl_mul_eq_prove(cl_mul_eq_proof_t proof,
                    const GEN m,
                    const cl_ciphertext_t ct,
                    const cl_public_key_t pk,
                    const cl_params_t params);

// 验证：不需要明文，使用公开的 (ct, pk, params)
int cl_mul_eq_verify(const cl_mul_eq_proof_t proof,
                     const cl_ciphertext_t ct,
                     const cl_public_key_t pk,
                     const cl_params_t params);

// 公开缩放：根据因子 s（bn_t），输出缩放后的证明，满足对 ct^s 验证通过
int cl_mul_eq_scale(cl_mul_eq_proof_t out,
                    const cl_mul_eq_proof_t in,
                    const bn_t s);

int ps_blind_sign(ps_signature_t signature,
									const pedersen_com_t com, 
									const ps_secret_key_t secret_key);
int ps_unblind(ps_signature_t signature,
							 const pedersen_decom_t decom);
int ps_verify(const ps_signature_t signature,
							bn_t message,
						 	const ps_public_key_t public_key);

int adaptor_ecdsa_sign(ecdsa_signature_t signature,
											 uint8_t *msg,
											 size_t len,
											 const ec_t Y,
											 const ec_secret_key_t secret_key);
int adaptor_ecdsa_preverify(ecdsa_signature_t signature,
														uint8_t *msg,
														size_t len,
														const ec_t Y,
														const ec_public_key_t public_key);

int pedersen_commit(pedersen_com_t com,
										pedersen_decom_t decom,
										g1_t h,
										bn_t x);
int commit(commit_t com, const ec_t x);
int decommit(const commit_t com, const ec_t x);

int zk_pedersen_com_prove(pedersen_com_zk_proof_t proof,
													g1_t h,
													const pedersen_com_t com,
													const pedersen_decom_t decom);
int zk_pedersen_com_verify(const pedersen_com_zk_proof_t proof,
													 g1_t h,
													 const pedersen_com_t com);
int zk_cldl_prove(zk_proof_cldl_t proof,
									const GEN x,
									const cl_ciphertext_t ciphertext,
									const cl_public_key_t public_key,
									const cl_params_t params);
int zk_cldl_verify(const zk_proof_cldl_t proof,
									 const ec_t Q,
									 const cl_ciphertext_t ciphertext,
									 const cl_public_key_t public_key,
									 const cl_params_t params);

int zk_dlog_prove(zk_proof_t proof, const ec_t h, const bn_t w);
int zk_dlog_verify(const zk_proof_t proof, const ec_t h);

int zk_dhtuple_prove(zk_proof_t proof, const ec_t h, const ec_t u, const ec_t v, const bn_t w);
int zk_dhtuple_verify(const zk_proof_t proof, const ec_t h, const ec_t u, const ec_t v);

// 新增：Bob随机化可延展性零知识证明函数
int zk_malleability_prove(zk_proof_malleability_t proof,
                          const bn_t beta,
                          const ec_t g_alpha,
                          const ec_t g_alpha_beta,
                          const cl_ciphertext_t ctx_alpha,
                          const cl_ciphertext_t ctx_alpha_beta,
                          const cl_ciphertext_t auditor_ctx_alpha,
                          const cl_ciphertext_t auditor_ctx_alpha_beta,
                          const cl_public_key_t tumbler_cl_pk,
                          const cl_public_key_t auditor_cl_pk,
                          const cl_params_t params);

// 新增：证明CL同态运算的正确性（P1证明）
int zk_cl_homomorphic_prove(zk_proof_t proof,
                            const bn_t beta,
                            const cl_ciphertext_t ct_in,
                            const cl_ciphertext_t ct_out,
                            const cl_public_key_t pk,
                            const cl_params_t params);

int zk_cl_homomorphic_verify(const zk_proof_t proof,
                             const cl_ciphertext_t ct_in,
                             const cl_ciphertext_t ct_out,
                             const cl_public_key_t pk,
                             const cl_params_t params);

int zk_malleability_verify(const zk_proof_malleability_t proof,
                           const ec_t g_alpha,
                           const ec_t g_alpha_beta,
                           const cl_ciphertext_t ctx_alpha,
                           const cl_ciphertext_t ctx_alpha_beta,
                           const cl_ciphertext_t auditor_ctx_alpha,
                           const cl_ciphertext_t auditor_ctx_alpha_beta,
                           const cl_public_key_t tumbler_cl_pk,
                           const cl_public_key_t auditor_cl_pk,
                           const cl_params_t params);

typedef struct {
  zk_proof_cldl_st alpha_proof;   // 证明 g^alpha 与 ctx_alpha 一致
  zk_proof_cldl_st r0_proof;      // 证明 g^r0 与 auditor_ctx_r0 一致
} zk_proof_puzzle_st;

typedef zk_proof_puzzle_st *zk_proof_puzzle_t;

// 综合谜题零知识证明结构
// 证明知道(alpha, r0)，使得：
// 1. ctx_alpha = Enc(pk_tumbler, alpha) 
// 2. ctx_r0_auditor = Enc(pk_auditor, r0)
// 注意：g^alpha 离散对数证明已包含在 alpha_enc_proof 中
typedef struct {
  zk_proof_cldl_st alpha_enc_proof;    // 证明 ctx_alpha 是对 alpha 的正确加密（包含g^alpha离散对数证明）
  zk_proof_cldl_st r0_enc_proof;      // 证明 ctx_r0_auditor 是对 r0 的正确加密（简化版本，只验证条件1）
} zk_proof_comprehensive_puzzle_st;

typedef zk_proof_comprehensive_puzzle_st *zk_proof_comprehensive_puzzle_t;

#define zk_proof_comprehensive_puzzle_null(proof) proof = NULL;

#define zk_proof_comprehensive_puzzle_new(proof)              \
  do {                                                         \
    proof = malloc(sizeof(zk_proof_comprehensive_puzzle_st));  \
    if (proof == NULL) {                                       \
      RLC_THROW(ERR_NO_MEMORY);                                \
    }                                                          \
    ec_new(proof->alpha_enc_proof.t2);                        \
    ec_new(proof->r0_enc_proof.t2);                           \
  } while (0)

#define zk_proof_comprehensive_puzzle_free(proof)               \
  do {                                                         \
    if (proof != NULL) {                                       \
      ec_free(proof->alpha_enc_proof.t2);                     \
      ec_free(proof->r0_enc_proof.t2);                        \
      free(proof);                                             \
      proof = NULL;                                            \
    }                                                          \
  } while (0)

// 生成：输入 alpha 与 r0 的明文（GEN）、各自的密文、对应的公钥
int zk_puzzle_prove(zk_proof_puzzle_t proof,
                    const GEN alpha,
                    const GEN r0,
                    const ec_t g_alpha,
                    const ec_t g_r0,
                    const cl_ciphertext_t ctx_alpha,
                    const cl_ciphertext_t ctx_r0_aud,
                    const cl_public_key_t pk_tumbler,
                    const cl_public_key_t pk_auditor,
                    const cl_params_t params);

// 验证：输入公开的 g^alpha、g^r0、两段密文与相应公钥
int zk_puzzle_verify(const zk_proof_puzzle_t proof,
                     const ec_t g_alpha,
                     const ec_t g_r0,
                     const cl_ciphertext_t ctx_alpha,
                     const cl_ciphertext_t ctx_r0_aud,
                     const cl_public_key_t pk_tumbler,
                     const cl_public_key_t pk_auditor,
                     const cl_params_t params);

// 综合谜题零知识证明函数
int zk_comprehensive_puzzle_prove(zk_proof_comprehensive_puzzle_t proof,
                                  const GEN alpha,
                                  const GEN r0,
                                  const ec_t g_to_the_alpha,
                                  const cl_ciphertext_t ctx_alpha,
                                  const cl_ciphertext_t ctx_r0_auditor,
                                  const cl_public_key_t pk_tumbler,
                                  const cl_public_key_t pk_auditor,
                                  const cl_params_t params);

int zk_comprehensive_puzzle_verify(const zk_proof_comprehensive_puzzle_t proof,
                                  const ec_t g_to_the_alpha,
                                  const cl_ciphertext_t ctx_alpha,
                                  const cl_ciphertext_t ctx_r0_auditor,
                                  const cl_public_key_t pk_tumbler,
                                  const cl_public_key_t pk_auditor,
                                  const cl_params_t params);

// 综合谜题零知识证明序列化函数
size_t zk_comprehensive_puzzle_serialized_size();
int zk_comprehensive_puzzle_serialize(uint8_t *dst, size_t *written, const zk_proof_comprehensive_puzzle_t proof);
int zk_comprehensive_puzzle_deserialize(zk_proof_comprehensive_puzzle_t proof, const uint8_t *src, size_t *read);

// Bob谜题关系零知识证明结构
// 简化版谜题关系证明：直接复用两份 CLDL 证明
typedef struct {
  // 证明 Enc_tumbler(beta) 正确
  zk_proof_cldl_t tumbler_proof;
  // 证明 Enc_auditor(beta) 正确
  zk_proof_cldl_t auditor_proof;
} zk_proof_puzzle_relation_st;

typedef zk_proof_puzzle_relation_st *zk_proof_puzzle_relation_t;

#define zk_proof_puzzle_relation_null(proof) proof = NULL;

#define zk_proof_puzzle_relation_new(proof)              \
  do {                                                   \
    proof = malloc(sizeof(zk_proof_puzzle_relation_st)); \
    if (proof == NULL) {                                 \
      RLC_THROW(ERR_NO_MEMORY);                         \
    }                                                    \
    zk_proof_cldl_new((proof)->tumbler_proof);          \
    zk_proof_cldl_new((proof)->auditor_proof);          \
  } while (0)

#define zk_proof_puzzle_relation_free(proof)            \
  do {                                                  \
    if (proof != NULL) {                               \
      zk_proof_cldl_free((proof)->tumbler_proof);      \
      zk_proof_cldl_free((proof)->auditor_proof);      \
      free(proof);                                     \
      proof = NULL;                                    \
    }                                                   \
  } while (0)

// Bob谜题关系零知识证明函数
int zk_puzzle_relation_prove(zk_proof_puzzle_relation_t proof,
                             const bn_t beta,
                             const cl_ciphertext_t enc_beta,
                             const cl_ciphertext_t enc_beta_aud,
                             const ec_t g_alpha,
                             const ec_t g_alpha_beta,
                             const cl_ciphertext_t ctx_alpha,
                             const cl_ciphertext_t ctx_alpha_beta,
                             const cl_ciphertext_t auditor_ctx_alpha,
                             const cl_ciphertext_t auditor_ctx_alpha_beta,
                             const cl_public_key_t pk_tumbler,
                             const cl_public_key_t pk_auditor,
                             const cl_params_t params);
int zk_puzzle_relation_verify(const zk_proof_puzzle_relation_t proof,
                              const ec_t g_alpha,
                              const ec_t g_alpha_beta,
                              const cl_ciphertext_t ctx_alpha,
                              const cl_ciphertext_t ctx_alpha_beta,
                              const cl_ciphertext_t auditor_ctx_alpha,
                              const cl_ciphertext_t auditor_ctx_alpha_beta,
                             const cl_public_key_t pk_tumbler,
                             const cl_public_key_t pk_auditor,
                             const cl_params_t params);

// 序列化和反序列化puzzle_relation证明
size_t zk_puzzle_relation_serialized_size();
int zk_puzzle_relation_serialize(uint8_t *dst, size_t *written, const zk_proof_puzzle_relation_t proof);
int zk_puzzle_relation_deserialize(zk_proof_puzzle_relation_t out, const uint8_t *src, size_t *read);

// ========== 新增：Bob的承诺证明（不需要原始数据） ==========
int zk_bob_commitment_prove(zk_proof_malleability_t proof,
                            const bn_t beta,
                            const ec_t g_alpha_beta,
                            const cl_ciphertext_t ctx_alpha_beta,
                            const cl_ciphertext_t auditor_ctx_alpha_beta,
                            const cl_public_key_t tumbler_cl_pk,
                            const cl_public_key_t auditor_cl_pk,
                            const cl_params_t params);

int zk_bob_commitment_verify(const zk_proof_malleability_t proof,
                             const ec_t g_alpha_beta,
                             const cl_ciphertext_t ctx_alpha_beta,
                             const cl_ciphertext_t auditor_ctx_alpha_beta,
                             const cl_public_key_t tumbler_cl_pk,
                             const cl_public_key_t auditor_cl_pk,
                             const cl_params_t params);

// ===== 可随机化（re-randomizable）证明接口（占位实现，后续以配对GS等式证明替换） =====
int zk_base_prove(zk_proof_malleability_t proof,
                  const cl_ciphertext_t ctx,
                  const ec_t P,
                  const cl_public_key_t pk,
                  const cl_params_t params);

int zk_eval_scale(zk_proof_malleability_t proof_out,
                  const zk_proof_malleability_t proof_in,
                  const bn_t factor,
                  const cl_ciphertext_t ctx_in,
                  cl_ciphertext_t ctx_out,
                  const ec_t P_in,
                  ec_t P_out,
                  const cl_public_key_t pk,
                  const cl_params_t params);

int zk_base_verify(const zk_proof_malleability_t proof,
                   const cl_ciphertext_t ctx,
                   const ec_t P,
                   const cl_public_key_t pk,
                   const cl_params_t params);

// ===== 可缩放GS等式证明（EC子式）轻量封装 =====
// 说明：这些函数仅包装 gs.c 中的 EC 等式证明，便于 Tumbler/Bob/Alice 调用。
int zk_gs_ec_eq_prove(gs_eq_proof_t proof, const bn_t alpha, const gs_crs_t crs, g1_t gamma_out);
int zk_gs_ec_eq_verify(const gs_eq_proof_t proof, const gs_crs_t crs, const g1_t gamma);
int zk_gs_ec_eq_scale(gs_eq_proof_t out, const gs_eq_proof_t in, const bn_t factor);

// 统一计算 m = H(tid || pool_label) 并约化到 G1 群阶
int hash_tid_with_pool(bn_t out_m, const bn_t tid, const char* pool_label);

// 统一编码函数 E(·)：将 CL 密文 (c1,c2) 规范编码到 bn（按 RLC_MD_LEN 哈希后 mod 曲线阶）
// 用途：把不可公开的密文对象转换为可用于“已知消息”证明的消息域元素
// 输入：ct 为 CL 密文对象（其 c1,c2 以 GEN 字符串形式可读）
// 输出：out 为约化到曲线阶的标量
int cl_ciphertext_encode_to_bn(bn_t out, const cl_ciphertext_t ct);

// ===== 方案一：联合证明（σ' 与 β'' 通过同一 m=E(β') 绑定） =====
typedef struct {
  cl_ciphertext_t C_m;        // Enc_pk1(m)
  cl_ciphertext_t C_r2;       // Enc_pk1(r2)
  zk_proof_cldl_t pi_m;       // CLDL：C_m 加密 m（pk1）
  zk_proof_cldl_t pi_r2;      // CLDL：C_r2 加密 r2（pk1）
  zk_proof_cldl_t pi_sigma;   // CLDL：sigma' 加密 m（pk2）
  uint8_t transcript_hash[RLC_MD_LEN];
} zk_sigma_link_proof_st;

typedef zk_sigma_link_proof_st* zk_sigma_link_proof_t;

#define zk_sigma_link_proof_new(p)                  \
  do {                                              \
    p = (zk_sigma_link_proof_t)malloc(sizeof(zk_sigma_link_proof_st)); \
    if (p) {                                        \
      cl_ciphertext_null((p)->C_m);                 \
      cl_ciphertext_null((p)->C_r2);                \
      cl_ciphertext_new((p)->C_m);                  \
      cl_ciphertext_new((p)->C_r2);                 \
      zk_proof_cldl_null((p)->pi_m);                \
      zk_proof_cldl_null((p)->pi_r2);               \
      zk_proof_cldl_null((p)->pi_sigma);            \
      zk_proof_cldl_new((p)->pi_m);                 \
      zk_proof_cldl_new((p)->pi_r2);                \
      zk_proof_cldl_new((p)->pi_sigma);             \
      memset((p)->transcript_hash, 0, RLC_MD_LEN);  \
    }                                               \
  } while (0)

#define zk_sigma_link_proof_free(p)                 \
  do {                                              \
    if (p) {                                        \
      cl_ciphertext_free((p)->C_m);                 \
      cl_ciphertext_free((p)->C_r2);                \
      zk_proof_cldl_free((p)->pi_m);                \
      zk_proof_cldl_free((p)->pi_r2);               \
      zk_proof_cldl_free((p)->pi_sigma);            \
      free(p);                                      \
      (p) = NULL;                                   \
    }                                               \
  } while (0)

int zk_sigma_link_prove(
  zk_sigma_link_proof_t out,
  const GEN m,
  const GEN r2,
  const cl_ciphertext_t sigma_prime,
  const cl_public_key_t pk1,
  const cl_public_key_t pk2,
  const cl_params_t params
);

int zk_sigma_link_verify(
  const zk_sigma_link_proof_t proof,
  const cl_ciphertext_t sigma_prime,
  const cl_ciphertext_t beta_double_prime,
  const cl_public_key_t pk1,
  const cl_public_key_t pk2,
  const cl_params_t params
);

// 序列化辅助（固定槽位，和现有 GENtostr 写入方式一致）
size_t zk_sigma_link_serialized_size();
int zk_sigma_link_serialize(uint8_t *dst, size_t *written, const zk_sigma_link_proof_t proof);
int zk_sigma_link_deserialize(zk_sigma_link_proof_t out, const uint8_t *src, size_t *read);

// ===== 承诺层联合证明（不公开 β′）：证明 E(β′′) = E(β′) + τ =====
typedef struct {
  g1_t C_m;           // Com(m), m = E(β′)
  g1_t C_mprime;      // Com(m') = Com(m) + Com(τ)
  gs_lin_proof_t link;// 证明同一 τ：C_m -> C_mprime 的链接证明（参数=τ）
} zk_commit_link_st;

typedef zk_commit_link_st* zk_commit_link_t;

#define zk_commit_link_new(p)                    \
  do {                                           \
    p = (zk_commit_link_t)malloc(sizeof(zk_commit_link_st)); \
    if (p) {                                     \
      g1_null((p)->C_m); g1_new((p)->C_m);       \
      g1_null((p)->C_mprime); g1_new((p)->C_mprime); \
      gs_lin_proof_new((p)->link);               \
      g1_new((p)->link->T); bn_new((p)->link->z_m); bn_new((p)->link->z_r); \
    }                                            \
  } while (0)

#define zk_commit_link_free(p)                   \
  do {                                           \
    if (p) {                                     \
      g1_free((p)->C_m); g1_free((p)->C_mprime); \
      gs_lin_proof_free((p)->link);              \
      free(p); (p) = NULL;                       \
    }                                            \
  } while (0)

// 生成：输入 m=E(β′) 与 τ（bn），输出 C_m、C_m' 以及 link 证明（使用 gs_crs）
int zk_commit_link_prove(zk_commit_link_t out, const bn_t m, const bn_t tau, const gs_crs_t crs);
// 验证：校验 link 证明 C_m -> C_m'
int zk_commit_link_verify(const zk_commit_link_t in, const gs_crs_t crs);
// 序列化：C_m | C_m' | link(T,z_m,z_r)
size_t zk_commit_link_serialized_size();
int zk_commit_link_serialize(uint8_t *dst, size_t *written, const zk_commit_link_t in);
int zk_commit_link_deserialize(zk_commit_link_t out, const uint8_t *src, size_t *read);

// ========== Shell 工具：链上状态检查 ==========
void query_escrow_status_by_id(const char *escrow_id);
void check_tx_mined(const char *tx_hash);

// ========== 区块链交易查询辅助函数 ==========
int extract_json_field(const char *json, const char *field_name, char *output, size_t max_len);
void query_and_save_transaction_details(const char *tx_hash);

// 组合证明生成：
int prove_layered_cipher_relations(
  pedersen_com_zk_proof_t pedersen_proof_out,
  zk_proof_cldl_t outer_proof_out,
  cl_mul_eq_proof_t pi_cl_beta_out,
  const cl_mul_eq_proof_t pi0_cl_in,
  const cl_ciphertext_t inner_aud_alpha,        // 原始 Enc_aud(alpha)
  const cl_ciphertext_t inner_aud_alpha_beta,   // 变换后 Enc_aud(alpha*beta)
  const cl_ciphertext_t outer_aud2_tag,
  const bn_t beta,
  const ps_public_key_t tumbler_ps_pk,
  const cl_public_key_t auditor2_cl_pk,
  const cl_params_t params,
  g1_t commitment_out
);

// ===== 隐藏外层密文的联合证明对象（不公开 outer 本体） =====
typedef struct {
  cl_mul_eq_proof_t pi_cl_beta;           // 1) 缩放一致性证明
  zk_proof_cldl_t   outer_proof;          // 2) outer = Enc_aud2(H(inner)) 的CLDL证明
  pedersen_com_zk_proof_t pedersen_proof; // 3) C = Com(H(outer)) 的ZK
  g1_t commitment;                        // 承诺点 C
  uint8_t tag_hash[RLC_MD_LEN];           // 记录 H(inner_aud_alpha_beta)（公开可发给验证方，不泄露outer）
} zk_layered_proof_st;

typedef zk_layered_proof_st* zk_layered_proof_t;

#define zk_layered_proof_new(p)                        \
  do {                                                 \
    p = (zk_layered_proof_t)malloc(sizeof(zk_layered_proof_st)); \
    if (p) {                                           \
      cl_mul_eq_proof_new((p)->pi_cl_beta);            \
      zk_proof_cldl_new((p)->outer_proof);             \
      pedersen_com_zk_proof_new((p)->pedersen_proof);  \
      g1_new((p)->commitment);                         \
    }                                                  \
  } while (0)

#define zk_layered_proof_free(p)                       \
  do {                                                 \
    if (p) {                                           \
      cl_mul_eq_proof_free((p)->pi_cl_beta);           \
      zk_proof_cldl_free((p)->outer_proof);            \
      pedersen_com_zk_proof_free((p)->pedersen_proof); \
      g1_free((p)->commitment);                        \
      free(p);                                         \
      (p) = NULL;                                      \
    }                                                  \
  } while (0)

int zk_layered_prove_hidden(
  zk_layered_proof_t out,
  const cl_mul_eq_proof_t pi0_cl_in,
  const cl_ciphertext_t inner_aud_alpha,
  const cl_ciphertext_t inner_aud_alpha_beta,
  const cl_ciphertext_t outer_aud2_tag,
  const bn_t beta,
  const ps_public_key_t tumbler_ps_pk,
  const cl_public_key_t auditor2_cl_pk,
  const cl_params_t params
);

int zk_layered_verify_hidden(
  const zk_layered_proof_t proof,
  const cl_ciphertext_t maybe_outer_aud2_tag, // 自检/本地可传 outer；对外为隐藏模式可传 NULL（仅验 Pedersen）
  const ps_public_key_t tumbler_ps_pk,
  const cl_public_key_t auditor2_cl_pk,
  const cl_params_t params
);

// 隐藏外层密文的联合证明（不公开 outer），把 CLDL + Pedersen 合成一次性证明
typedef struct {
  // 子证明打包
  zk_proof_cldl_st outer_proof;         // 证明存在 tag 使 outer=Enc_aud2(tag)
  pedersen_com_zk_proof_st com_proof;   // 证明承诺绑定 H(outer)
  // Fiat–Shamir 合成挑战（占位）
  uint8_t transcript_hash[RLC_MD_LEN];
} layered_hidden_proof_st;

typedef layered_hidden_proof_st* layered_hidden_proof_t;

int prove_layered_cipher_relations_hidden_outer(
  layered_hidden_proof_t proof_out,
  const cl_ciphertext_t inner_aud_alpha_beta,
  const cl_ciphertext_t outer_aud2_tag,
  const ps_public_key_t tumbler_ps_pk,
  const cl_public_key_t auditor2_cl_pk,
  const cl_params_t params,
  const g1_t commitment_C
);

int verify_layered_cipher_relations_hidden_outer(
  const layered_hidden_proof_t proof,
  const cl_ciphertext_t inner_aud_alpha_beta,
  const ps_public_key_t tumbler_ps_pk,
  const cl_public_key_t auditor2_cl_pk,
  const cl_params_t params,
  const g1_t commitment_C
);

/*
 * 联合证明（隐藏 outer）：
 * 目标：存在 σ' 使得 σ' = Enc(pk2, β')，且 σ'' 是 σ' 的重随机化，并且公开承诺 C 绑定 σ''。
 * 注意：outer 本体不公开；仅公开 C 与联合证明。
 */
int zk_outer_link_prove(
  complete_nizk_proof_t proof_out,                 /* 输出：承诺与开口证明填入结构体 */
  const cl_ciphertext_t ct_beta_prime,             /* β' 的密文（inner） */
  const cl_ciphertext_t ct_sigma_double_prime,     /* σ''（outer，本地可见，不对外发送） */
  const ps_public_key_t ps_pk,
  const cl_public_key_t pk2,
  const cl_params_t params,
  const pedersen_decom_t decom_c1,                 /* C1 的开口（outer.c1 的承诺） */
  const pedersen_decom_t decom_c2                  /* C2 的开口（outer.c2 的承诺） */
);

int zk_outer_link_verify(
  const complete_nizk_proof_t proof,               /* 包含 C1/C2 与 Pedersen 证明等 */
  const cl_ciphertext_t ct_beta_prime,             /* β' 的密文（公开） */
  const g1_t C1,                                   /* 对 outer.c1 的承诺（公开） */
  const g1_t C2,                                   /* 对 outer.c2 的承诺（公开） */
  const ps_public_key_t ps_pk,
  const cl_public_key_t pk2,
  const cl_params_t params
);

// 新增：完整的三证明生成和验证函数
int complete_nizk_prove(
  complete_nizk_proof_t proof,
  const bn_t beta,                              // Bob的随机数
  const cl_ciphertext_t ct_beta,               // β = Enc(pk1, r0)
  const cl_ciphertext_t ct_beta_prime,         // β' = β^beta
  const cl_ciphertext_t ct_sigma_prime,        // σ' = Enc(pk2, β')
  const cl_ciphertext_t ct_sigma_double_prime, // σ'' = 盲化的σ'
  const ps_public_key_t ps_pk,
  const cl_public_key_t cl_pk1,
  const cl_public_key_t cl_pk2,
  const cl_params_t params,
  const pedersen_decom_t decom                 // 用于P3的decommitment
);

int complete_nizk_verify(
  const complete_nizk_proof_t proof,
  const cl_ciphertext_t ct_beta,
  const cl_ciphertext_t ct_beta_prime,
  const ps_public_key_t ps_pk,
  const cl_public_key_t cl_pk1,
  const cl_public_key_t cl_pk2,
  const cl_params_t params
);

#endif // A2L_ECDSA_INCLUDE_UTIL