
#ifndef A2L_ECDSA_INCLUDE_TUMBLER
#define A2L_ECDSA_INCLUDE_TUMBLER

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "zmq.h"
#include "types.h"
#include "gs.h"
#include "util.h"

// Include headers first, then use the types
#include "malleable_proof.h"
#include "composite_malleable_proof.h"

#define TUMBLER_ENDPOINT  "tcp://*:8181"

typedef enum {
  REGISTRATION,
  PROMISE_INIT,
  PAYMENT_INIT,
  LAYERED_PROOF_SHARE,
  BOB_CONFIRM_DONE,
} msgcode_t;

typedef struct {
  char *key;
  msgcode_t code;
} symstruct_t;

static symstruct_t msg_lookuptable[] = {
  { "registration", REGISTRATION },
  { "promise_init", PROMISE_INIT },
  { "payment_init", PAYMENT_INIT },
  { "layered_proof_share", LAYERED_PROOF_SHARE },
  { "bob_confirm_done", BOB_CONFIRM_DONE },
};

#define TOTAL_MESSAGES (sizeof(msg_lookuptable) / sizeof(symstruct_t))

typedef struct {
  ec_secret_key_t tumbler_ec_sk;
  ec_public_key_t tumbler_ec_pk;
  ec_public_key_t alice_ec_pk;
  ec_public_key_t bob_ec_pk;
  ps_secret_key_t tumbler_ps_sk;
  ps_public_key_t tumbler_ps_pk;
  cl_secret_key_t tumbler_cl_sk;
  cl_public_key_t tumbler_cl_pk;
  cl_public_key_t auditor_cl_pk; // 新增
  cl_public_key_t auditor2_cl_pk; // 新增：第二把审计公钥
  cl_params_t cl_params;
  bn_t gamma;
  bn_t alpha;
  bn_t r0; // 新增：用于auditor加密的随机数
  ec_t g_to_the_alpha;
  cl_ciphertext_t ctx_alpha;
  cl_ciphertext_t ctx_r0_auditor; // 新增：auditor 加密的 r0
  ecdsa_signature_t sigma_r;
  ecdsa_signature_t sigma_tr;
  ecdsa_signature_t sigma_s;
  ecdsa_signature_t sigma_ts;
  char alice_escrow_id[67]; // 新增：存储Alice的托管ID
  char current_bob_escrow_id[67]; // 新增：存储当前bob的托管ID
  char tumbler_escrow_id[67]; // 新增：存储Tumbler自己的托管ID
  char bob_address[67]; // Bob的以太坊地址
  char pool_label[16]; // 新增：Alice选择的固定面额池标识（如 "0.1"）
  char alice_escrow_tx_hash[67]; // 新增：Alice开托管交易哈希（registration中发送）
  char tumbler_escrow_tx_hash[67]; // 新增：Tumbler开托管交易哈希（promise_init发送给Bob）
  // GS commitments and CRS
  gs_crs_t gs_crs;
  gs_commitment_t C_alpha;
  gs_commitment_t C_r0;
  bn_t com_r_alpha; // opening randomness for C_alpha
  bn_t com_r_r0;    // opening randomness for C_r0
  // GS equality proof (EC sub-proof) for certificate-level transform
  gs_eq_proof_t pi0_ec;      // proof for gamma_gs_alpha = alpha * H1
  g1_t gamma_gs_alpha;       // gamma in G1 for GS verify/scale
  cl_mul_eq_proof_t pi0_cl;  // CL 乘法缩放型基准证明
  cl_ciphertext_t auditor_ctx_alpha_times_beta; // 保存Bob发来的 inner 密文
  uint8_t auditor2_tag_hash[RLC_MD_LEN]; // 保存 Bob 计算的 H(inner) 以便复用为 msgid
  // 可塑性证明系统
  malleable_proof_scheme_t malleable_proof; // Tumbler生成的可塑性证明
  composite_malleable_proof_t composite_proof; // Tumbler生成的复合可塑性证明 (图片中的完整关系)
  char *groth_sahai_proof_result; // 存储Groth-Sahai证明结果
  
  // 综合谜题零知识证明 (证明知道alpha和r0，使得ctx_alpha=Enc(pk_tumbler,alpha), ctx_r0_auditor=Enc(pk_auditor,r0), g_to_the_alpha=g^alpha成立)
  zk_proof_comprehensive_puzzle_t comprehensive_puzzle_zk_proof; // 综合谜题的零知识证明
  
  // 从Alice接收的数据
  zk_proof_puzzle_relation_t alice_puzzle_relation_zk_proof; // Alice的谜题关系零知识证明
  ec_t alice_g_to_the_alpha_times_beta; // 从Bob收到的g^(α+β)
  cl_ciphertext_t alice_ctx_alpha_times_beta; // 从Bob收到的ctx_α+β
  cl_ciphertext_t alice_auditor_ctx_alpha_times_beta; // 从Bob收到的auditor_ctx_α+β
  
  // Tornado Cash zkSNARK 证明数据（从 Bob 接收，用于验证）
  char tornado_proof_data[4608]; // proofData JSON: {"proof":{...},"publicSignals":[...]}
} tumbler_state_st;

typedef tumbler_state_st *tumbler_state_t;

#define tumbler_state_null(state) state = NULL;

#define tumbler_state_new(state)                          \
  do {                                                    \
    state = malloc(sizeof(tumbler_state_st));             \
    if (state == NULL) {                                  \
      RLC_THROW(ERR_NO_MEMORY);                           \
    }                                                     \
    ec_secret_key_new((state)->tumbler_ec_sk);            \
    ec_public_key_new((state)->tumbler_ec_pk);            \
    ec_public_key_new((state)->alice_ec_pk);              \
    ec_public_key_new((state)->bob_ec_pk);                \
    ps_secret_key_new((state)->tumbler_ps_sk);            \
    ps_public_key_new((state)->tumbler_ps_pk);            \
    cl_secret_key_new((state)->tumbler_cl_sk);            \
    cl_public_key_new((state)->tumbler_cl_pk);            \
    cl_public_key_new((state)->auditor_cl_pk);            \
    cl_public_key_new((state)->auditor2_cl_pk);           \
    cl_params_new((state)->cl_params);                    \
    bn_new((state)->gamma);                               \
    bn_new((state)->alpha);                               \
    bn_new((state)->r0);                                  \
    ec_new((state)->g_to_the_alpha);                      \
    cl_ciphertext_new((state)->ctx_alpha);                \
    cl_ciphertext_new((state)->ctx_r0_auditor);           \
    ecdsa_signature_new((state)->sigma_r);                \
    ecdsa_signature_new((state)->sigma_tr);               \
    ecdsa_signature_new((state)->sigma_s);                \
    ecdsa_signature_new((state)->sigma_ts);               \
    gs_crs_new((state)->gs_crs);                          \
    gs_commitment_new((state)->C_alpha);                  \
    gs_commitment_new((state)->C_r0);                     \
    bn_new((state)->com_r_alpha);                         \
    bn_new((state)->com_r_r0);                            \
    gs_eq_proof_new((state)->pi0_ec);                     \
    g1_new((state)->gamma_gs_alpha);                      \
    cl_mul_eq_proof_new((state)->pi0_cl);                 \
    cl_ciphertext_new((state)->auditor_ctx_alpha_times_beta); \
    malleable_proof_new((state)->malleable_proof);        \
    composite_malleable_proof_new((state)->composite_proof); \
    (state)->groth_sahai_proof_result = NULL;             \
    zk_proof_comprehensive_puzzle_new((state)->comprehensive_puzzle_zk_proof); \
    zk_proof_puzzle_relation_new((state)->alice_puzzle_relation_zk_proof); \
    ec_new((state)->alice_g_to_the_alpha_times_beta);     \
    cl_ciphertext_new((state)->alice_ctx_alpha_times_beta); \
    cl_ciphertext_new((state)->alice_auditor_ctx_alpha_times_beta); \
  } while (0)

#define tumbler_state_free(state)                         \
  do {                                                    \
    ec_secret_key_free((state)->tumbler_ec_sk);           \
    ec_public_key_free((state)->tumbler_ec_pk);           \
    ec_public_key_free((state)->alice_ec_pk);             \
    ec_public_key_free((state)->bob_ec_pk);               \
    ps_secret_key_free((state)->tumbler_ps_sk);           \
    ps_public_key_free((state)->tumbler_ps_pk);           \
    cl_secret_key_free((state)->tumbler_cl_sk);           \
    cl_public_key_free((state)->tumbler_cl_pk);           \
    cl_public_key_free((state)->auditor_cl_pk);           \
    cl_public_key_free((state)->auditor2_cl_pk);          \
    cl_params_free((state)->cl_params);                   \
    bn_free((state)->gamma);                              \
    bn_free((state)->alpha);                              \
    bn_free((state)->r0);                                 \
    ec_free((state)->g_to_the_alpha);                     \
    cl_ciphertext_free((state)->ctx_alpha);               \
    cl_ciphertext_free((state)->ctx_r0_auditor);          \
    ecdsa_signature_free((state)->sigma_r);               \
    ecdsa_signature_free((state)->sigma_tr);              \
    ecdsa_signature_free((state)->sigma_s);               \
    ecdsa_signature_free((state)->sigma_ts);              \
    gs_crs_free((state)->gs_crs);                         \
    gs_commitment_free((state)->C_alpha);                 \
    gs_commitment_free((state)->C_r0);                    \
    bn_free((state)->com_r_alpha);                        \
    bn_free((state)->com_r_r0);                           \
    gs_eq_proof_free((state)->pi0_ec);                    \
    g1_free((state)->gamma_gs_alpha);                     \
    cl_mul_eq_proof_free((state)->pi0_cl);                \
    cl_ciphertext_free((state)->auditor_ctx_alpha_times_beta); \
    malleable_proof_free((state)->malleable_proof);       \
    composite_malleable_proof_free((state)->composite_proof); \
    if ((state)->groth_sahai_proof_result != NULL) {     \
      free((state)->groth_sahai_proof_result);            \
    }                                                     \
    zk_proof_comprehensive_puzzle_free((state)->comprehensive_puzzle_zk_proof); \
    zk_proof_puzzle_relation_free((state)->alice_puzzle_relation_zk_proof); \
    ec_free((state)->alice_g_to_the_alpha_times_beta);    \
    cl_ciphertext_free((state)->alice_ctx_alpha_times_beta); \
    cl_ciphertext_free((state)->alice_auditor_ctx_alpha_times_beta); \
    free(state);                                          \
    state = NULL;                                         \
  } while (0)

typedef int (*msg_handler_t)(tumbler_state_t, void*, uint8_t*);

int get_message_type(char *key);
msg_handler_t get_message_handler(char *key);
int handle_message(tumbler_state_t state, void *socket, zmq_msg_t message);
int receive_message(tumbler_state_t state, void *socket);

int registration_handler(tumbler_state_t state, void *socket, uint8_t *data);
int promise_init_handler(tumbler_state_t state, void *socket, uint8_t *data);
int payment_init_handler(tumbler_state_t state, void *socket, uint8_t *data);
int layered_proof_share_handler(tumbler_state_t state, void *socket, uint8_t *data);

// 新增：处理Bob的完整ZK证明验证
int bob_confirm_done_handler(tumbler_state_t state, void *socket, uint8_t *data);

#endif // A2L_ECDSA_INCLUDE_TUMBLER
