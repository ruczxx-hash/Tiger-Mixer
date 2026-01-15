#ifndef A2L_ECDSA_INCLUDE_ALICE
#define A2L_ECDSA_INCLUDE_ALICE

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "zmq.h"
#include "types.h"
#include "util.h"
#include "gs.h"
#include "composite_malleable_proof.h"

#define TUMBLER_ENDPOINT  "tcp://localhost:8181"
// #define ALICE_ENDPOINT    "tcp://*:8182"
// #define BOB_ENDPOINT      "tcp://localhost:8183"

typedef enum {
  REGISTRATION_DONE,
  PUZZLE_SHARE,
  PAYMENT_DONE,
} msgcode_t;

typedef struct {
  char *key;
  msgcode_t code;
} symstruct_t;

static symstruct_t msg_lookuptable[] = {
  { "registration_done", REGISTRATION_DONE },
  { "puzzle_share", PUZZLE_SHARE },
  { "payment_done", PAYMENT_DONE }
};

#define TOTAL_MESSAGES (sizeof(msg_lookuptable) / sizeof(symstruct_t))

typedef struct {
  ec_secret_key_t alice_ec_sk;
  ec_public_key_t alice_ec_pk;
  ec_public_key_t tumbler_ec_pk;
  ps_public_key_t tumbler_ps_pk;
  cl_public_key_t tumbler_cl_pk;
  cl_public_key_t auditor_cl_pk; // 新增
  cl_public_key_t auditor2_cl_pk; // 新增：第二把审计公钥
  cl_params_t cl_params;
  commit_t com;
  ec_t g_to_the_alpha_times_beta;
  ec_t g_to_the_alpha_times_beta_times_tau;
  cl_ciphertext_t ctx_alpha_times_beta;
  cl_ciphertext_t ctx_alpha_times_beta_times_tau;
  cl_ciphertext_t auditor_ctx_alpha_times_beta;
  cl_ciphertext_t auditor_ctx_alpha_times_beta_times_tau;
  cl_ciphertext_t auditor2_ctx_alpha_times_beta; // 新增：外层密文 Enc_aud2(H(inner))
  ecdsa_signature_t sigma_hat_s;
  ecdsa_signature_t sigma_s;
  ps_signature_t sigma_outer; // 新增：解盲后的外层标签PS签名
  bn_t tau;
  bn_t alpha_hat;
  bn_t tid;
  ps_signature_t sigma_tid;
  pedersen_com_t pcom;
  pedersen_decom_t pdecom;
  char alice_address[67]; // Alice的以太坊地址
  char escrow_tx_hash[67]; // 托管交易哈希
  char alice_escrow_id[67]; // Alice的托管ID（随机生成的0x开头的64位十六进制字符串）
  zk_proof_malleability_t malleability_proof; // Alice自己的可延展性零知识证明
  zk_proof_malleability_t bob_malleability_proof; // Bob发送的可延展性零知识证明
  composite_malleable_proof_t bob_composite_proof; // Bob发送的复合可塑性零知识证明
  // GS data for verifying Bob's link proofs
  gs_crs_t gs_crs;
  gs_commitment_t C_alpha;
  gs_commitment_t C_r0;
  gs_commitment_t C_beta;
  gs_commitment_t C_alpha_plus_beta;
  gs_commitment_t C_r0_plus_beta;
  gs_lin_proof_t bob_link_alpha;
  gs_lin_proof_t bob_link_r0;
  // EC sub-proof (GS equality) for transform
  gs_eq_proof_t pi_ec_beta;     // Bob -> Alice 的 πβ_ec
  gs_eq_proof_t pi_ec_beta_tau; // Alice 缩放后的 πβτ_ec
  g1_t gamma_gs_alpha_beta;     // Bob 的 gamma^beta (G1)
  g1_t gamma_gs_alpha_beta_tau; // Alice 的 gamma^(beta*tau) (G1)
  cl_mul_eq_proof_t pi_cl_beta;     // Bob -> Alice 的 πβ_clmul
  cl_mul_eq_proof_t pi_cl_beta_tau; // Alice 缩放后的 πβτ_clmul
  // 固定面额池选择（例如 "0_1"、"1"、"10"、"100"）
  char pool_label[16];
  // 可选：记录本次 openEscrow 使用的池合约地址（压缩存储）
  char pool_contract[43];
  
  // Tornado Cash 相关字段
  uint8_t nullifier[31];      // nullifier (31 bytes)
  uint8_t secret[31];         // secret (31 bytes)
  char commitment[67];        // commitment (32 bytes hex string with 0x prefix)
  char nullifier_hash[67];   // nullifierHash (32 bytes hex string with 0x prefix)
  char note[256];            // note string (format: a2l-eth-{amount}-{netId}-0x{...})
  // Tornado Cash 取款证明相关字段
  char merkle_root[67];      // Merkle tree root (32 bytes hex string with 0x prefix)
  uint32_t leaf_index;       // commitment 在 Merkle 树中的索引
  char merkle_proof_path[2048]; // Merkle 路径证明（JSON 格式）
  char zk_proof[4096];        // zkSNARK 证明（JSON 格式）
  char public_signals[512];   // 公共信号（JSON 格式）
  
  // Alice谜题关系零知识证明
  zk_proof_puzzle_relation_t puzzle_relation_zk_proof; // Alice生成的谜题关系零知识证明
} alice_state_st;

typedef alice_state_st *alice_state_t;

#define alice_state_null(state) state = NULL;

#define alice_state_new(state)                              \
  do {                                                      \
    state = malloc(sizeof(alice_state_st));                 \
    if (state == NULL) {                                    \
      RLC_THROW(ERR_NO_MEMORY);                             \
    }                                                       \
    ec_secret_key_new((state)->alice_ec_sk);                \
    ec_public_key_new((state)->alice_ec_pk);                \
    ec_public_key_new((state)->tumbler_ec_pk);              \
    ps_public_key_new((state)->tumbler_ps_pk);              \
    cl_public_key_new((state)->tumbler_cl_pk);              \
    cl_public_key_new((state)->auditor_cl_pk);              \
    cl_public_key_new((state)->auditor2_cl_pk);             \
    cl_params_new((state)->cl_params);                      \
    commit_new((state)->com);                               \
    ec_new((state)->g_to_the_alpha_times_beta);             \
    ec_new((state)->g_to_the_alpha_times_beta_times_tau);   \
    cl_ciphertext_new((state)->ctx_alpha_times_beta);       \
    cl_ciphertext_new((state)->ctx_alpha_times_beta_times_tau); \
    cl_ciphertext_new((state)->auditor_ctx_alpha_times_beta); \
    cl_ciphertext_new((state)->auditor_ctx_alpha_times_beta_times_tau); \
    cl_ciphertext_new((state)->auditor_ctx_alpha_times_beta); \
    cl_ciphertext_new((state)->auditor2_ctx_alpha_times_beta); \
    ecdsa_signature_new((state)->sigma_hat_s);              \
    ecdsa_signature_new((state)->sigma_s);                  \
    ps_signature_new((state)->sigma_outer);                 \
    bn_new((state)->alpha_hat);                             \
    bn_new((state)->tid);                                   \
    bn_new((state)->tau);                                   \
    ps_signature_new((state)->sigma_tid);                   \
    pedersen_com_new((state)->pcom);                        \
    pedersen_decom_new((state)->pdecom);                    \
    zk_proof_malleability_new((state)->malleability_proof); \
    zk_proof_malleability_new((state)->bob_malleability_proof); \
    gs_crs_new((state)->gs_crs);                            \
    gs_commitment_new((state)->C_alpha);                    \
    gs_commitment_new((state)->C_r0);                       \
    gs_commitment_new((state)->C_beta);                     \
    gs_commitment_new((state)->C_alpha_plus_beta);          \
    gs_commitment_new((state)->C_r0_plus_beta);             \
    gs_lin_proof_new((state)->bob_link_alpha);              \
    gs_lin_proof_new((state)->bob_link_r0);                 \
    gs_eq_proof_new((state)->pi_ec_beta);                   \
    gs_eq_proof_new((state)->pi_ec_beta_tau);               \
    g1_new((state)->gamma_gs_alpha_beta);                   \
    g1_new((state)->gamma_gs_alpha_beta_tau);               \
    cl_mul_eq_proof_new((state)->pi_cl_beta);               \
    cl_mul_eq_proof_new((state)->pi_cl_beta_tau);           \
    composite_malleable_proof_new((state)->bob_composite_proof); \
    zk_proof_puzzle_relation_new((state)->puzzle_relation_zk_proof); \
  } while (0)
  

#define alice_state_free(state)                             \
  do {                                                      \
    ec_secret_key_free((state)->alice_ec_sk);               \
    ec_public_key_free((state)->alice_ec_pk);               \
    ec_public_key_free((state)->tumbler_ec_pk);             \
    ps_public_key_free((state)->tumbler_ps_pk);             \
    cl_public_key_free((state)->tumbler_cl_pk);             \
    cl_public_key_free((state)->auditor_cl_pk);             \
    cl_public_key_free((state)->auditor2_cl_pk);            \
    cl_params_free((state)->cl_params);                     \
    commit_free((state)->com);                              \
    ec_free((state)->g_to_the_alpha_times_beta);            \
    ec_free((state)->g_to_the_alpha_times_beta_times_tau);  \
    cl_ciphertext_free((state)->ctx_alpha_times_beta);      \
    cl_ciphertext_free((state)->ctx_alpha_times_beta_times_tau); \
    cl_ciphertext_free((state)->auditor_ctx_alpha_times_beta_times_tau); \
    cl_ciphertext_free((state)->auditor_ctx_alpha_times_beta); \
    cl_ciphertext_free((state)->auditor2_ctx_alpha_times_beta); \
    ecdsa_signature_free((state)->sigma_hat_s);             \
    ecdsa_signature_free((state)->sigma_s);                 \
    ps_signature_free((state)->sigma_outer);                \
    bn_free((state)->alpha_hat);                            \
    bn_free((state)->tid);                                  \
    bn_free((state)->tau);                                  \
    ps_signature_free((state)->sigma_tid);                  \
    pedersen_com_free((state)->pcom);                       \
    pedersen_decom_free((state)->pdecom);                   \
    zk_proof_malleability_free((state)->malleability_proof); \
    zk_proof_malleability_free((state)->bob_malleability_proof); \
    composite_malleable_proof_free((state)->bob_composite_proof); \
    gs_crs_free((state)->gs_crs);                            \
    gs_commitment_free((state)->C_alpha);                    \
    gs_commitment_free((state)->C_r0);                       \
    gs_commitment_free((state)->C_beta);                     \
    gs_commitment_free((state)->C_alpha_plus_beta);          \
    gs_commitment_free((state)->C_r0_plus_beta);             \
    gs_lin_proof_free((state)->bob_link_alpha);              \
    gs_lin_proof_free((state)->bob_link_r0);                 \
    gs_eq_proof_free((state)->pi_ec_beta);                   \
    gs_eq_proof_free((state)->pi_ec_beta_tau);               \
    g1_free((state)->gamma_gs_alpha_beta);                   \
    g1_free((state)->gamma_gs_alpha_beta_tau);               \
    cl_mul_eq_proof_free((state)->pi_cl_beta);               \
    cl_mul_eq_proof_free((state)->pi_cl_beta_tau);           \
    zk_proof_puzzle_relation_free((state)->puzzle_relation_zk_proof); \
    free(state);                                            \
    state = NULL;                                           \
  } while (0)
  

typedef int (*msg_handler_t)(alice_state_t, void*, uint8_t*, transaction_t*);

int get_message_type(char *key);
msg_handler_t get_message_handler(char *key);
int handle_message(alice_state_t state, void *socket, zmq_msg_t message, transaction_t* tx_data);
int receive_message(alice_state_t state, void *socket, transaction_t* tx_data);

int registration(alice_state_t state, void *socket, const char *alice_escrow_id);
int registration_done_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);
int token_share(alice_state_t state, void *socket,const char *alice_escrow_id);
int puzzle_share_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t* tx_data);
int payment_init(alice_state_t state, void *socket, transaction_t *tx_data);
int payment_done_handler(alice_state_t state, void *socket, uint8_t *data, transaction_t *tx_data);
int puzzle_solution_share(alice_state_t state, void *socket);

#endif // A2L_ECDSA_INCLUDE_ALICE