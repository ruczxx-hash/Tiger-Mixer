#ifndef A2L_ECDSA_INCLUDE_AUDITOR
#define A2L_ECDSA_INCLUDE_AUDITOR

#include <stddef.h>
#include <string.h>
#include "/home/zxx/Config/relic/include/relic.h"
#include "zmq.h"
#include "types.h"
#include "secret_share.h"
#include "util.h" 

// Auditor 的主要功能是审计和重构秘密分享
// 它不需要处理复杂的协议消息，主要是读取和解析数据

typedef struct {
  ec_public_key_t alice_ec_pk;
  ec_public_key_t bob_ec_pk;
  ec_public_key_t tumbler_ec_pk;  // 新增：Tumbler的ECDSA公钥
  cl_public_key_t tumbler_cl_pk;  // 新增：Tumbler的CL公钥
  cl_secret_key_t auditor_cl_sk;
  cl_public_key_t auditor_cl_pk;
  cl_public_key_t auditor2_cl_pk; // 新增：第二把审计公钥
  cl_secret_key_t auditor2_cl_sk; // 新增：第二把审计私钥
  cl_params_t cl_params;
  char *second_msgid; // 新增：保存第二个分片的msgid
  // 解析缓存字段
  int has_alice_g_abt;
  uint8_t alice_g_abt[33];
  int has_alice_presig;
  uint8_t alice_presig_r[34];
  uint8_t alice_presig_s[34];
  uint8_t alice_presig_R[33];
  uint8_t alice_presig_pi_a[33];
  uint8_t alice_presig_pi_b[33];
  uint8_t alice_presig_pi_z[34];
  int has_alice_final;
  uint8_t alice_final_r[34];
  uint8_t alice_final_s[34];
  int has_bob_g_alpha;
  uint8_t bob_g_alpha[33];
  int has_bob_presig;
  uint8_t bob_presig_r[34];
  uint8_t bob_presig_s[34];
  uint8_t bob_presig_R[33];
  uint8_t bob_presig_pi_a[33];
  uint8_t bob_presig_pi_b[33];
  uint8_t bob_presig_pi_z[34];
  int has_bob_final;
  uint8_t bob_final_r[34];
  uint8_t bob_final_s[34];
  // Tumbler 预签名和最终签名缓存
  int has_tumbler_presig;
  uint8_t tumbler_presig_r[34];
  uint8_t tumbler_presig_s[34];
  uint8_t tumbler_presig_R[33];
  uint8_t tumbler_presig_pi_a[33];
  uint8_t tumbler_presig_pi_b[33];
  uint8_t tumbler_presig_pi_z[34];
  int has_tumbler_final;
  uint8_t tumbler_final_r[34];
  uint8_t tumbler_final_s[34];
  // ZK证明缓存
  int has_tumbler_zk_proof;
  zk_proof_comprehensive_puzzle_t tumbler_zk_proof;
  // 签名验证所需的托管ID和交易哈希
  int has_alice_escrow_info;
  char alice_escrow_id[67];      // Alice的托管ID
  char alice_escrow_tx_hash[67]; // Alice开托管交易哈希
  int has_tumbler_escrow_info;
  char tumbler_escrow_id[67];      // Tumbler的托管ID
  char tumbler_escrow_tx_hash[67]; // Tumbler开托管交易哈希
} auditor_state_st;

typedef auditor_state_st *auditor_state_t;

#define auditor_state_null(state) state = NULL;

#define auditor_state_new(state)                          \
  do {                                                    \
    printf("[DEBUG] Allocating auditor state memory\n");  \
    state = malloc(sizeof(auditor_state_st));             \
    if (state == NULL) {                                  \
      printf("[DEBUG] Failed to allocate auditor state memory\n"); \
      RLC_THROW(ERR_NO_MEMORY);                           \
    }                                                     \
    printf("[DEBUG] Allocated auditor state memory successfully\n"); \
    printf("[DEBUG] Initializing alice_ec_pk\n");         \
    ec_public_key_new((state)->alice_ec_pk);              \
    printf("[DEBUG] Initialized alice_ec_pk successfully\n"); \
    printf("[DEBUG] Initializing bob_ec_pk\n");           \
    ec_public_key_new((state)->bob_ec_pk);                \
    printf("[DEBUG] Initialized bob_ec_pk successfully\n"); \
    printf("[DEBUG] Initializing tumbler_ec_pk\n");       \
    ec_public_key_new((state)->tumbler_ec_pk);            \
    printf("[DEBUG] Initialized tumbler_ec_pk successfully\n"); \
    printf("[DEBUG] Initializing tumbler_cl_pk\n");       \
    cl_public_key_new((state)->tumbler_cl_pk);            \
    printf("[DEBUG] Initialized tumbler_cl_pk successfully\n"); \
    printf("[DEBUG] Initializing auditor_cl_sk\n");       \
    cl_secret_key_new((state)->auditor_cl_sk);            \
    printf("[DEBUG] Initialized auditor_cl_sk successfully\n"); \
    printf("[DEBUG] Initializing auditor_cl_pk\n");       \
    cl_public_key_new((state)->auditor_cl_pk);            \
    printf("[DEBUG] Initialized auditor_cl_pk successfully\n"); \
    cl_public_key_new((state)->auditor2_cl_pk);           \
    printf("[DEBUG] Initialized auditor2_cl_pk successfully\n"); \
    cl_secret_key_new((state)->auditor2_cl_sk);           \
    printf("[DEBUG] Initialized auditor2_cl_sk successfully\n"); \
    printf("[DEBUG] Initializing cl_params\n");           \
    cl_params_new((state)->cl_params);                    \
    printf("[DEBUG] Initialized cl_params successfully\n"); \
    (state)->second_msgid = NULL;                         \
    printf("[DEBUG] Initialized second_msgid successfully\n"); \
    (state)->has_alice_g_abt = 0;                         \
    (state)->has_alice_presig = 0;                        \
    (state)->has_alice_final = 0;                         \
    (state)->has_bob_g_alpha = 0;                         \
    (state)->has_bob_presig = 0;                          \
    (state)->has_bob_final = 0;                           \
    (state)->has_tumbler_presig = 0;                      \
    (state)->has_tumbler_final = 0;                       \
    (state)->has_tumbler_zk_proof = 0;                    \
    zk_proof_comprehensive_puzzle_new((state)->tumbler_zk_proof); \
    (state)->has_alice_escrow_info = 0;                   \
    (state)->has_tumbler_escrow_info = 0;                 \
    memset((state)->alice_escrow_id, 0, sizeof((state)->alice_escrow_id)); \
    memset((state)->alice_escrow_tx_hash, 0, sizeof((state)->alice_escrow_tx_hash)); \
    memset((state)->tumbler_escrow_id, 0, sizeof((state)->tumbler_escrow_id)); \
    memset((state)->tumbler_escrow_tx_hash, 0, sizeof((state)->tumbler_escrow_tx_hash)); \
    printf("[DEBUG] Auditor state initialization completed\n"); \
  } while (0)

#define auditor_state_free(state)                         \
  do {                                                    \
    ec_public_key_free((state)->alice_ec_pk);              \
    ec_public_key_free((state)->bob_ec_pk);                \
    ec_public_key_free((state)->tumbler_ec_pk);            \
    cl_public_key_free((state)->tumbler_cl_pk);           \
    cl_secret_key_free((state)->auditor_cl_sk);           \
    cl_public_key_free((state)->auditor_cl_pk);           \
    cl_secret_key_free((state)->auditor2_cl_sk);          \
    cl_params_free((state)->cl_params);                   \
    if ((state)->has_tumbler_zk_proof) {                  \
      zk_proof_comprehensive_puzzle_free((state)->tumbler_zk_proof); \
    }                                                     \
    if ((state)->second_msgid) {                          \
      free((state)->second_msgid);                        \
    }                                                     \
    free(state);                                          \
    state = NULL;                                         \
  } while (0)

// Auditor 的主要函数声明
int read_auditor_cl_keypair(cl_secret_key_t auditor_cl_sk, cl_public_key_t auditor_cl_pk);
void audit_message(char* msg_id, auditor_state_t state);

// 审计相关的辅助函数
int reconstruct_and_audit_secret_shares(const char* msg_id, uint8_t* reconstructed_data, size_t* data_length);
int parse_audited_data(const uint8_t* data, size_t data_length);

// 新增：公钥读取和签名验证函数
int read_alice_bob_tumbler_public_keys(auditor_state_t state);

// VSS 相关函数
int verify_share_with_stored_commitment(auditor_state_t state, const secret_share_t* share, const char* msgid);
int start_vss_commitment_server(auditor_state_t state);

#endif // A2L_ECDSA_INCLUDE_AUDITOR 