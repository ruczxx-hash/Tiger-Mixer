// Minimal pairing-group commitment and linear relation proof (GS-style API)
// This module introduces:
// - gs_crs_t: public parameters for commitments in G1
// - gs_commitment_t: Pedersen-style commitment C = m*H + r*G in G1
// - gs_commit/gs_open: create and open commitments
// - gs_lin_prove/gs_lin_verify: zero-knowledge proof that
//   C_sum commits to (m_a + m_b, r_a + r_b) consistent with C_a and C_b

#pragma once

#include <stdint.h>
#include "relic.h"

// Public CRS for commitments in G1
typedef struct {
  g1_t G1_base;   // generator for randomness
  g1_t H1_base;   // generator for message
} gs_crs_st;

typedef gs_crs_st *gs_crs_t;

#define gs_crs_null(crs) crs = NULL;
#define gs_crs_new(crs)                 \
  do {                                  \
    crs = malloc(sizeof(gs_crs_st));    \
    if ((crs) == NULL) {                \
      RLC_THROW(ERR_NO_MEMORY);         \
    }                                   \
    g1_null((crs)->G1_base);            \
    g1_null((crs)->H1_base);            \
  } while (0)

#define gs_crs_free(crs)                \
  do {                                  \
    if ((crs) != NULL) {                \
      g1_free((crs)->G1_base);          \
      g1_free((crs)->H1_base);          \
      free(crs);                         \
      (crs) = NULL;                     \
    }                                   \
  } while (0)

// Commitment object
typedef struct {
  g1_t C; // commitment point in G1
} gs_commitment_st;

typedef gs_commitment_st *gs_commitment_t;

#define gs_commitment_null(c) c = NULL;
#define gs_commitment_new(c)            \
  do {                                  \
    c = malloc(sizeof(gs_commitment_st));\
    if ((c) == NULL) {                  \
      RLC_THROW(ERR_NO_MEMORY);         \
    }                                   \
    g1_null((c)->C);                    \
  } while (0)

#define gs_commitment_free(c)           \
  do {                                  \
    if ((c) != NULL) {                  \
      g1_free((c)->C);                  \
      free(c);                          \
      (c) = NULL;                       \
    }                                   \
  } while (0)

// CRS setup: choose independent bases in G1
int gs_crs_setup(gs_crs_t crs);

// C = m*H + r*G
int gs_commit(gs_commitment_t com, const bn_t m, const bn_t r, const gs_crs_t crs);

// Verify an opening: check C == m*H + r*G
int gs_open_verify(const gs_commitment_t com, const bn_t m, const bn_t r, const gs_crs_t crs);

// ZK proof of knowledge of opening (m,r) for C = m*H + r*G
typedef struct {
  g1_t T;  // commitment
  bn_t z_m;
  bn_t z_r;
} gs_open_proof_st;

typedef gs_open_proof_st *gs_open_proof_t;

#define gs_open_proof_null(p) p = NULL;
#define gs_open_proof_new(p)                \
  do {                                      \
    p = malloc(sizeof(gs_open_proof_st));    \
    if ((p) == NULL) {                       \
      RLC_THROW(ERR_NO_MEMORY);              \
    }                                        \
    g1_null((p)->T);                         \
    bn_null((p)->z_m);                       \
    bn_null((p)->z_r);                       \
  } while (0)

#define gs_open_proof_free(p)               \
  do {                                      \
    if ((p) != NULL) {                       \
      g1_free((p)->T);                       \
      bn_free((p)->z_m);                     \
      bn_free((p)->z_r);                     \
      free(p);                               \
      (p) = NULL;                            \
    }                                        \
  } while (0)

int gs_open_prove(gs_open_proof_t proof, const bn_t m, const bn_t r, const gs_commitment_t com, const gs_crs_t crs);
int gs_open_verify_proof(const gs_open_proof_t proof, const gs_commitment_t com, const gs_crs_t crs);

// Linear-relation ZK proof that C_sum is the commitment to (m_a+m_b, r_a+r_b)
// with respect to C_a and C_b, without revealing the witnesses.
// Proof structure: Schnorr-style in G1
typedef struct {
  g1_t T;  // commitment to randomness
  bn_t z_m; // response for message part
  bn_t z_r; // response for randomness part
} gs_lin_proof_st;

typedef gs_lin_proof_st *gs_lin_proof_t;

#define gs_lin_proof_null(p) p = NULL;
#define gs_lin_proof_new(p)              \
  do {                                   \
    p = malloc(sizeof(gs_lin_proof_st)); \
    if ((p) == NULL) {                   \
      RLC_THROW(ERR_NO_MEMORY);          \
    }                                    \
    g1_null((p)->T);                     \
    bn_null((p)->z_m);                   \
    bn_null((p)->z_r);                   \
  } while (0)

#define gs_lin_proof_free(p)             \
  do {                                   \
    if ((p) != NULL) {                   \
      g1_free((p)->T);                   \
      bn_free((p)->z_m);                 \
      bn_free((p)->z_r);                 \
      free(p);                           \
      (p) = NULL;                        \
    }                                    \
  } while (0)

// Prover knows (m_a,r_a),(m_b,r_b),(m_s,r_s) s.t. C_a=Com(m_a,r_a), C_b=Com(m_b,r_b), C_sum=Com(m_s,r_s) and m_s=m_a+m_b, r_s=r_a+r_b
int gs_lin_prove(gs_lin_proof_t proof,
                 const bn_t m_a, const bn_t r_a,
                 const bn_t m_b, const bn_t r_b,
                 const bn_t m_s, const bn_t r_s,
                 const gs_commitment_t C_a,
                 const gs_commitment_t C_b,
                 const gs_commitment_t C_sum,
                 const gs_crs_t crs);

int gs_lin_verify(const gs_lin_proof_t proof,
                  const gs_commitment_t C_a,
                  const gs_commitment_t C_b,
                  const gs_commitment_t C_sum,
                  const gs_crs_t crs);

// Link proof: prove knowledge of (m_b,r_b) such that C_sum - C_base = m_b*H + r_b*G
typedef gs_lin_proof_st gs_link_proof_st; // reuse structure
typedef gs_link_proof_st *gs_link_proof_t;

int gs_link_prove(gs_link_proof_t proof,
                  const bn_t m_b, const bn_t r_b,
                  const gs_commitment_t C_base,
                  const gs_commitment_t C_sum,
                  const gs_crs_t crs);

int gs_link_verify(const gs_link_proof_t proof,
                   const gs_commitment_t C_base,
                   const gs_commitment_t C_sum,
                   const gs_crs_t crs);


// ===================== Re-randomizable GS-style equality proof (EC only) =====================
// Proves knowledge of alpha such that gamma = alpha * H1_base (additive form, i.e., g^alpha).
// Challenge e is derived only from CRS, enabling certificate-level scaling:
//   scale by factor f: T' = f*T, z' = f*z, gamma' = f*gamma preserves verification.
typedef struct {
  g1_t T;   // commitment in G1
  bn_t z;   // response
} gs_eq_proof_st;

typedef gs_eq_proof_st *gs_eq_proof_t;

#define gs_eq_proof_null(p) p = NULL;
#define gs_eq_proof_new(p)                 \
  do {                                    \
    p = malloc(sizeof(gs_eq_proof_st));    \
    if ((p) == NULL) {                     \
      RLC_THROW(ERR_NO_MEMORY);            \
    }                                      \
    g1_null((p)->T);                       \
    bn_null((p)->z);                       \
    g1_new((p)->T);                        \
    bn_new((p)->z);                        \
  } while (0)

#define gs_eq_proof_free(p)                \
  do {                                    \
    if ((p) != NULL) {                    \
      g1_free((p)->T);                    \
      bn_free((p)->z);                    \
      free(p);                            \
      (p) = NULL;                         \
    }                                     \
  } while (0)

// Prove: gamma = alpha * H1_base, using CRS-only challenge
int gs_eq_prove(gs_eq_proof_t proof, const bn_t alpha, const g1_t gamma, const gs_crs_t crs);
// Verify with current gamma only
int gs_eq_verify(const gs_eq_proof_t proof, const g1_t gamma, const gs_crs_t crs);
// Public scaling (certificate-level transform): out = f * in
int gs_eq_scale(gs_eq_proof_t out, const gs_eq_proof_t in, const bn_t factor);

