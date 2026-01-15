// Malleable Proof Scheme Implementation
// Based on Groth-Sahai proof system with Witness Indistinguishability
// 
// This implementation follows the paper's specification:
// ΠMP = (CRSSetup, Vry, Prove, ZKEval)
//
// Author: Implementation for AuditPCH project
// Date: 2025

#pragma once

#define MALLEABLE_PROOF_H_INCLUDED

#include <stdint.h>
#include "relic.h"
#include "gs.h"
#include "types.h"

// ========== Malleable Proof Scheme Core Structures ==========

// Language definition: L := {x | ∃w such that (x,w) ∈ R}
typedef struct {
    ec_t statement;        // x: public statement
    bn_t witness;          // w: private witness  
} language_relation_st;

typedef language_relation_st *language_relation_t;

// Transformation functions T_wit and T_stmt
typedef struct {
    bn_t randomization_factor;  // r1: for witness transformation
    bn_t statement_factor;      // r2: for statement transformation
} transformation_functions_st;

typedef transformation_functions_st *transformation_functions_t;

// Complete Malleable Proof structure
typedef struct {
    // Core GS proof components
    gs_crs_t crs;                          // Common Reference String
    gs_commitment_t commitment;             // Commitment to witness
    gs_open_proof_t opening_proof;         // Proof of opening
    gs_lin_proof_t linear_proof;           // Linear relation proof
    
    // Malleability components
    transformation_functions_t transform_funcs;  // T_wit, T_stmt functions
    
    // Additional proof elements for WI property
    g1_t converted_witness_commitment;     // Commitment to w'
    bn_t challenge;                        // Fiat-Shamir challenge
    bn_t response_witness;                 // Response for witness part
    bn_t response_randomness;              // Response for randomness part
    
    // Metadata
    uint8_t proof_id[32];                  // Unique proof identifier
    uint32_t proof_version;                // For versioning
} malleable_proof_scheme_st;

typedef malleable_proof_scheme_st *malleable_proof_scheme_t;

// ========== Memory Management Macros ==========

#define malleable_proof_null(mp) mp = NULL;

#define malleable_proof_new(mp)                          \
    do {                                                 \
        mp = malloc(sizeof(malleable_proof_scheme_st));  \
        if ((mp) == NULL) {                              \
            RLC_THROW(ERR_NO_MEMORY);                    \
        }                                                \
        gs_crs_null((mp)->crs);                         \
        gs_commitment_null((mp)->commitment);            \
        gs_open_proof_null((mp)->opening_proof);         \
        gs_lin_proof_null((mp)->linear_proof);           \
        (mp)->transform_funcs = NULL;                    \
        g1_null((mp)->converted_witness_commitment);     \
        bn_null((mp)->challenge);                        \
        bn_null((mp)->response_witness);                 \
        bn_null((mp)->response_randomness);              \
        (mp)->proof_version = 1;                         \
    } while (0)

#define malleable_proof_free(mp)                         \
    do {                                                 \
        if ((mp) != NULL) {                              \
            gs_crs_free((mp)->crs);                      \
            gs_commitment_free((mp)->commitment);         \
            gs_open_proof_free((mp)->opening_proof);      \
            gs_lin_proof_free((mp)->linear_proof);        \
            if ((mp)->transform_funcs != NULL) {          \
                bn_free((mp)->transform_funcs->randomization_factor); \
                bn_free((mp)->transform_funcs->statement_factor);     \
                free((mp)->transform_funcs);              \
            }                                            \
            g1_free((mp)->converted_witness_commitment);  \
            bn_free((mp)->challenge);                     \
            bn_free((mp)->response_witness);              \
            bn_free((mp)->response_randomness);           \
            free(mp);                                    \
            (mp) = NULL;                                 \
        }                                                \
    } while (0)

// ========== Core API Functions ==========

/**
 * CRSSetup(λ) → crs
 * Generates the Common Reference String for the malleable proof scheme
 * 
 * @param crs: Output CRS structure
 * @param security_param: Security parameter λ
 * @return: RLC_OK on success, RLC_ERR on failure
 */
int malleable_crs_setup(gs_crs_t crs, int security_param);

/**
 * Prove(w, crs, x) → π
 * Generates a malleable proof for witness w and statement x
 * 
 * @param proof: Output proof structure
 * @param witness: Private witness w
 * @param statement: Public statement x  
 * @param crs: Common Reference String
 * @return: RLC_OK on success, RLC_ERR on failure
 */
int malleable_prove(malleable_proof_scheme_t proof,
                   const bn_t witness,
                   const ec_t statement,
                   const gs_crs_t crs);

/**
 * Vry(π, crs, x) → {0, 1}
 * Verifies a malleable proof
 * 
 * @param proof: Proof to verify
 * @param statement: Public statement x
 * @param crs: Common Reference String
 * @return: RLC_OK if valid, RLC_ERR if invalid
 */
int malleable_verify(const malleable_proof_scheme_t proof,
                    const ec_t statement,
                    const gs_crs_t crs);

/**
 * ZKEval(crs, x, {T_wit, T_stmt}, π) → π'
 * Transforms a proof π into a new proof π' for the converted statement
 * This is the core malleability operation
 * 
 * @param proof_out: Output transformed proof
 * @param proof_in: Input original proof
 * @param transform_funcs: Transformation functions {T_wit, T_stmt}
 * @param statement_in: Original statement x
 * @param statement_out: Transformed statement x'
 * @param crs: Common Reference String
 * @return: RLC_OK on success, RLC_ERR on failure
 */
int malleable_zkeval(malleable_proof_scheme_t proof_out,
                    const malleable_proof_scheme_t proof_in,
                    const transformation_functions_t transform_funcs,
                    const ec_t statement_in,
                    ec_t statement_out,
                    const gs_crs_t crs);

// ========== Transformation Function Utilities ==========

/**
 * Initialize transformation functions
 */
int transformation_functions_init(transformation_functions_t *tf);

/**
 * Set randomization factor for witness transformation: w' = T_wit(w)
 */
int transformation_set_witness_factor(transformation_functions_t tf, const bn_t factor);

/**
 * Set factor for statement transformation: x' = T_stmt(x) 
 */
int transformation_set_statement_factor(transformation_functions_t tf, const bn_t factor);

/**
 * Apply witness transformation: w' = T_wit(w) = w + r1
 */
int transform_witness(bn_t witness_out, const bn_t witness_in, 
                     const transformation_functions_t tf);

/**
 * Apply statement transformation: x' = T_stmt(x) = x^r2
 */
int transform_statement(ec_t statement_out, const ec_t statement_in,
                       const transformation_functions_t tf);

// ========== Advanced Features ==========

/**
 * Batch verification of multiple malleable proofs
 */
int malleable_batch_verify(const malleable_proof_scheme_t* proofs,
                          const ec_t* statements,
                          int num_proofs,
                          const gs_crs_t crs);

/**
 * Proof composition: combine multiple proofs into one
 */
int malleable_compose_proofs(malleable_proof_scheme_t composed_proof,
                           const malleable_proof_scheme_t* input_proofs,
                           int num_proofs,
                           const gs_crs_t crs);

/**
 * Generate randomized proof (for enhanced privacy)
 */
int malleable_randomize_proof(malleable_proof_scheme_t randomized_proof,
                             const malleable_proof_scheme_t original_proof,
                             const gs_crs_t crs);

// ========== Utility Functions ==========

/**
 * Compute Fiat-Shamir challenge from proof components
 */
int compute_fiat_shamir_challenge(bn_t challenge,
                                 const malleable_proof_scheme_t proof,
                                 const ec_t statement,
                                 const gs_crs_t crs);

/**
 * Serialize proof for network transmission
 */
int malleable_proof_serialize(uint8_t* buffer, size_t* length,
                             const malleable_proof_scheme_t proof);

/**
 * Deserialize proof from network data
 */
int malleable_proof_deserialize(malleable_proof_scheme_t proof,
                               const uint8_t* buffer, size_t length);

/**
 * Check Witness Indistinguishability property
 */
int check_witness_indistinguishability(const malleable_proof_scheme_t proof1,
                                      const malleable_proof_scheme_t proof2,
                                      const gs_crs_t crs);

// ========== Constants and Configuration ==========

#define MALLEABLE_PROOF_VERSION         1
#define FIAT_SHAMIR_CHALLENGE_LENGTH   32
#define PROOF_ID_LENGTH                32
#define MIN_SECURITY_PARAMETER         128

// Error codes specific to malleable proofs
#define ERR_MALLEABLE_INVALID_TRANSFORM  (ERR_CAUGHT + 100)
#define ERR_MALLEABLE_WI_VIOLATION      (ERR_CAUGHT + 101) 
#define ERR_MALLEABLE_PROOF_INVALID     (ERR_CAUGHT + 102)

