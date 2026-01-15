
#ifndef A2L_ECDSA_INCLUDE_BOB
#define A2L_ECDSA_INCLUDE_BOB

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "zmq.h"
#include "types.h"
#include "util.h"
#include "gs.h"
#include "composite_malleable_proof.h"

#define TUMBLER_ENDPOINT  "tcp://localhost:8181"
// #define ALICE_ENDPOINT    "tcp://localhost:8182"
// #define BOB_ENDPOINT      "tcp://*:8183"

typedef enum {
  TOKEN_SHARE,
  PROMISE_DONE,
  PUZZLE_SHARE_DONE,
  PUZZLE_SOLUTION_SHARE,
  LAYERED_PROOF_SIGNED
} msgcode_t;

typedef struct {
  char *key;
  msgcode_t code;
} symstruct_t;

static symstruct_t msg_lookuptable[] = {
  { "token_share", TOKEN_SHARE },
  { "promise_done", PROMISE_DONE },
  { "puzzle_share_done", PUZZLE_SHARE_DONE },
  { "puzzle_solution_share", PUZZLE_SOLUTION_SHARE },
  { "layered_proof_signed", LAYERED_PROOF_SIGNED }
};

#define TOTAL_MESSAGES (sizeof(msg_lookuptable) / sizeof(symstruct_t))

typedef struct {
  ec_secret_key_t bob_ec_sk;
  ec_public_key_t bob_ec_pk;
  ec_public_key_t tumbler_ec_pk;
  ps_public_key_t tumbler_ps_pk;
  cl_public_key_t tumbler_cl_pk;
  cl_public_key_t auditor_cl_pk; // 新增
  cl_public_key_t auditor2_cl_pk; // 新增：第二把审计公钥
  cl_params_t cl_params;
  commit_t com;
  ec_t g_to_the_alpha;
  ec_t g_to_the_alpha_times_beta; // 新增：保存随机化后的 g_to_the_alpha
  cl_ciphertext_t ctx_alpha;
  cl_ciphertext_t ctx_alpha_times_beta;
  cl_ciphertext_t auditor_ctx_alpha; // 新增
  cl_ciphertext_t auditor_ctx_alpha_times_beta; // 新增：保存随机化后的 auditor 密文
  cl_ciphertext_t auditor2_ctx_alpha_times_beta; // 新增：第二把公钥下的 Enc_aud2(alpha*beta)
  g1_t auditor2_tag_commit; // 新增：对外层密文标签的Pedersen承诺
  pedersen_com_zk_proof_t auditor2_tag_com_proof; // 新增：承诺正确性的ZK证明
  zk_proof_cldl_t auditor2_outer_proof; // 新增：外层CLDL证明
  pedersen_decom_t auditor2_tag_decom; // 新增：承诺开口用于解盲
  bn_t auditor2_tag_msg; // 新增：承诺消息 m = H(outer)
  ecdsa_signature_t sigma_r;
  ecdsa_signature_t sigma_t;
  bn_t beta;
  bn_t tid;
  ps_signature_t sigma_tid;
  char tumbler_escrow_id[67]; // 新增：存储Tumbler的托管ID
  char bob_address[67]; // Bob的以太坊地址
  char pool_label[16]; // 新增：Alice选择的固定面额池标识（如 "0.1"）
  char confirm_escrow_tx_hash[67]; // 新增：存储confirmEscrow交易哈希（Bob自己执行的）
  char tumbler_escrow_tx_hash[67]; // 新增：存储Tumbler开托管交易哈希（从promise_done接收）
  zk_proof_cldl_t saved_pi_cldl; // 新增：保存从Tumbler接收的CLDL零知识证明
  zk_proof_malleability_t malleability_proof; // 新增：保存可延展性零知识证明
  // GS CRS and commitments from Hub
  gs_crs_t gs_crs;
  gs_commitment_t C_alpha;
  gs_commitment_t C_r0;
  // EC sub-proof (GS equality) for transform
  gs_eq_proof_t pi_ec_from_hub; // π0_ec 接收自 Tumbler
  gs_eq_proof_t pi_ec_beta;     // Bob 缩放后的 πβ_ec
  g1_t gamma_gs_alpha;          // 来自 Tumbler 的 gamma (G1)
  g1_t gamma_gs_alpha_beta;     // Bob 缩放后的 gamma^beta (G1)
  cl_mul_eq_proof_t pi0_cl;     // 从 Tumbler 接收的 π0_clmul
  cl_mul_eq_proof_t pi_cl_beta; // Bob 缩放后的 πβ_clmul
  
  // 新增：完整的三证明
  complete_nizk_proof_t complete_proof;
  ps_signature_t sigma_outer_blind; // 新增：接收 Tumbler 的盲签名
  
  // 复合可塑性零知识证明系统
  composite_malleable_proof_t received_composite_proof;  // 从Tumbler接收的原始复合证明
  composite_malleable_proof_t transformed_composite_proof; // Bob变换后的复合证明
  
  // 综合谜题零知识证明
  zk_proof_comprehensive_puzzle_t received_puzzle_zk_proof; // 从Tumbler接收的谜题零知识证明
  
  // Bob谜题关系零知识证明
  zk_proof_puzzle_relation_t puzzle_relation_zk_proof; // Bob生成的谜题关系零知识证明
  
  // 用于零知识证明的密文和随机性
  cl_ciphertext_t enc_beta; // enc_beta密文
  cl_ciphertext_t enc_beta_aud; // enc_beta_aud密文
  GEN enc_beta_r; // enc_beta的随机性
  GEN enc_beta_aud_r; // enc_beta_aud的随机性
  
  // Tumbler端点配置
  char tumbler_endpoint[64]; // Tumbler连接端点
  
  // Tornado Cash 相关字段（从 Alice 接收）
  uint8_t nullifier[31];      // nullifier (31 bytes)
  uint8_t secret[31];         // secret (31 bytes)
  char commitment[67];        // commitment (32 bytes hex string with 0x prefix)
  char escrow_tx_hash[67];   // escrow transaction hash (从 Alice 接收)
  char pool_contract[43];    // pool contract address (从 Alice 接收)
  
  // Tornado Cash zkSNARK 证明数据（Bob 生成，转发给 Tumbler）
  char tornado_proof_data[4608]; // proofData JSON: {"proof":{...},"publicSignals":[...]}
} bob_state_st;

typedef bob_state_st *bob_state_t;

#define bob_state_null(state) state = NULL;

#define bob_state_new(state)                                \
  do {                                                      \
    state = malloc(sizeof(bob_state_st));                   \
    if (state == NULL) {                                    \
      RLC_THROW(ERR_NO_MEMORY);                             \
    }                                                       \
    ec_secret_key_new((state)->bob_ec_sk);                  \
    ec_public_key_new((state)->bob_ec_pk);                  \
    ec_public_key_new((state)->tumbler_ec_pk);              \
    ps_public_key_new((state)->tumbler_ps_pk);              \
    cl_public_key_new((state)->tumbler_cl_pk);              \
    cl_public_key_new((state)->auditor_cl_pk);              \
    cl_public_key_new((state)->auditor2_cl_pk);             \
    cl_params_new((state)->cl_params);                      \
    commit_new((state)->com);                               \
    ec_new((state)->g_to_the_alpha);                        \
    ec_new((state)->g_to_the_alpha_times_beta);             \
    cl_ciphertext_new((state)->ctx_alpha);                  \
    cl_ciphertext_new((state)->ctx_alpha_times_beta);       \
    cl_ciphertext_new((state)->auditor_ctx_alpha);          \
    cl_ciphertext_new((state)->auditor_ctx_alpha_times_beta); \
    cl_ciphertext_new((state)->auditor2_ctx_alpha_times_beta); \
    g1_new((state)->auditor2_tag_commit);                     \
    pedersen_com_zk_proof_new((state)->auditor2_tag_com_proof); \
    zk_proof_cldl_new((state)->auditor2_outer_proof);          \
    pedersen_decom_new((state)->auditor2_tag_decom);           \
    bn_new((state)->auditor2_tag_msg);                         \
    ecdsa_signature_new((state)->sigma_r);                  \
    ecdsa_signature_new((state)->sigma_t);                  \
    bn_new((state)->beta);                                  \
    bn_new((state)->tid);                                   \
    ps_signature_new((state)->sigma_tid);                   \
    zk_proof_cldl_new((state)->saved_pi_cldl);              \
    zk_proof_malleability_new((state)->malleability_proof); \
    gs_crs_new((state)->gs_crs);                            \
    gs_commitment_new((state)->C_alpha);                    \
    gs_commitment_new((state)->C_r0);                       \
    gs_eq_proof_new((state)->pi_ec_from_hub);               \
    gs_eq_proof_new((state)->pi_ec_beta);                   \
    g1_new((state)->gamma_gs_alpha);                        \
    g1_new((state)->gamma_gs_alpha_beta);                   \
    cl_mul_eq_proof_new((state)->pi0_cl);                   \
    cl_mul_eq_proof_new((state)->pi_cl_beta);               \
    complete_nizk_proof_new((state)->complete_proof);       \
    ps_signature_new((state)->sigma_outer_blind);           \
    composite_malleable_proof_new((state)->received_composite_proof); \
    composite_malleable_proof_new((state)->transformed_composite_proof); \
    zk_proof_comprehensive_puzzle_new((state)->received_puzzle_zk_proof); \
    zk_proof_puzzle_relation_new((state)->puzzle_relation_zk_proof); \
    cl_ciphertext_new((state)->enc_beta); \
    cl_ciphertext_new((state)->enc_beta_aud); \
  } while (0)

#define bob_state_free(state)                               \
  do {                                                      \
    ec_secret_key_free((state)->bob_ec_sk);                 \
    ec_public_key_free((state)->bob_ec_pk);                 \
    ec_public_key_free((state)->tumbler_ec_pk);             \
    ps_public_key_free((state)->tumbler_ps_pk);             \
    cl_public_key_free((state)->tumbler_cl_pk);             \
    cl_public_key_free((state)->auditor_cl_pk);             \
    cl_public_key_free((state)->auditor2_cl_pk);            \
    cl_params_free((state)->cl_params);                     \
    commit_free((state)->com);                              \
    ec_free((state)->g_to_the_alpha);                       \
    ec_free((state)->g_to_the_alpha_times_beta);            \
    cl_ciphertext_free((state)->ctx_alpha);                 \
    cl_ciphertext_free((state)->ctx_alpha_times_beta);      \
    cl_ciphertext_free((state)->auditor_ctx_alpha);         \
    cl_ciphertext_free((state)->auditor_ctx_alpha_times_beta); \
    cl_ciphertext_free((state)->auditor2_ctx_alpha_times_beta); \
    g1_free((state)->auditor2_tag_commit);                    \
    pedersen_com_zk_proof_free((state)->auditor2_tag_com_proof); \
    zk_proof_cldl_free((state)->auditor2_outer_proof);         \
    pedersen_decom_free((state)->auditor2_tag_decom);          \
    bn_free((state)->auditor2_tag_msg);                        \
    ecdsa_signature_free((state)->sigma_r);                 \
    ecdsa_signature_free((state)->sigma_t);                 \
    bn_free((state)->beta);                                 \
    bn_free((state)->tid);                                  \
    ps_signature_free((state)->sigma_tid);                  \
    zk_proof_cldl_free((state)->saved_pi_cldl);             \
    zk_proof_malleability_free((state)->malleability_proof); \
    gs_crs_free((state)->gs_crs);                            \
    gs_commitment_free((state)->C_alpha);                    \
    gs_commitment_free((state)->C_r0);                       \
    gs_eq_proof_free((state)->pi_ec_from_hub);               \
    gs_eq_proof_free((state)->pi_ec_beta);                   \
    g1_free((state)->gamma_gs_alpha);                        \
    g1_free((state)->gamma_gs_alpha_beta);                   \
    cl_mul_eq_proof_free((state)->pi0_cl);                   \
    cl_mul_eq_proof_free((state)->pi_cl_beta);               \
    complete_nizk_proof_free((state)->complete_proof);       \
    ps_signature_free((state)->sigma_outer_blind);           \
    composite_malleable_proof_free((state)->received_composite_proof); \
    composite_malleable_proof_free((state)->transformed_composite_proof); \
    zk_proof_comprehensive_puzzle_free((state)->received_puzzle_zk_proof); \
    zk_proof_puzzle_relation_free((state)->puzzle_relation_zk_proof); \
    cl_ciphertext_free((state)->enc_beta); \
    cl_ciphertext_free((state)->enc_beta_aud); \
    free(state);                                            \
    state = NULL;                                           \
  } while (0)

typedef int (*msg_handler_t)(bob_state_t, void*, uint8_t*, transaction_t*);

int get_message_type(char *key);
msg_handler_t get_message_handler(char *key);
int handle_message(bob_state_t state, void *socket, zmq_msg_t message, transaction_t* tx_data);
int receive_message(bob_state_t state, void *socket, transaction_t* tx_data);

int token_share_handler(bob_state_t state, void *socet, uint8_t *data, transaction_t* tx_data);
int promise_init(bob_state_t state, void *socket, transaction_t* tx_data);
int promise_done_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);
int puzzle_share(bob_state_t state, void *socket, transaction_t* tx_data);
int puzzle_share_done_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);
int puzzle_solution_share_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);
int layered_proof_signed_handler(bob_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);

#endif // A2L_ECDSA_INCLUDE_BOB