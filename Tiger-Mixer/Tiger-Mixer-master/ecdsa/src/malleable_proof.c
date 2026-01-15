// Malleable Proof Scheme Implementation  
// Based on Groth-Sahai proof system with Witness Indistinguishability
//
// This implements the paper's ΠMP = (CRSSetup, Vry, Prove, ZKEval) scheme
// 
// Key features:
// 1. Malleable proofs supporting witness and statement transformation
// 2. Witness Indistinguishability (WI) property
// 3. Zero-knowledge proof system based on GS commitments
// 4. Support for randomization of puzzles as described in the paper

#include "malleable_proof.h"
#include "util.h"
#include <string.h>

// ========== Core Implementation ==========

int malleable_crs_setup(gs_crs_t crs, int security_param) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        if (security_param < MIN_SECURITY_PARAMETER) {
            printf("[WARNING] Security parameter %d below recommended minimum %d\n",
                   security_param, MIN_SECURITY_PARAMETER);
        }
        
        // Initialize CRS using existing GS setup
        result = gs_crs_setup(crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to setup GS CRS\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        printf("[INFO] Malleable proof CRS setup completed (security: %d bits)\n",
               security_param);
               
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // Cleanup if needed
    }
    
    return result;
}

int malleable_prove(malleable_proof_scheme_t proof,
                   const bn_t witness,
                   const ec_t statement, 
                   const gs_crs_t crs) {
    int result = RLC_OK;
    bn_t q, randomness;
    ec_t witness_commitment;
    
    bn_null(q); bn_null(randomness);
    ec_null(witness_commitment);
    
    RLC_TRY {
        if (proof == NULL || witness == NULL || statement == NULL || crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_new(q); bn_new(randomness);
        ec_new(witness_commitment);
        
        // Get curve order
        ec_curve_get_ord(q);
        
        // Initialize proof structure
        gs_crs_new(proof->crs);
        gs_commitment_new(proof->commitment);
        gs_open_proof_new(proof->opening_proof);
        gs_lin_proof_new(proof->linear_proof);
        
        g1_new(proof->converted_witness_commitment);
        bn_new(proof->challenge);
        bn_new(proof->response_witness);
        bn_new(proof->response_randomness);
        
        // Copy CRS
        g1_copy(proof->crs->G1_base, crs->G1_base);
        g1_copy(proof->crs->H1_base, crs->H1_base);
        
        // Step 1: Create commitment to witness
        bn_rand_mod(randomness, q);
        result = gs_commit(proof->commitment, witness, randomness, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to create commitment\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 2: Generate opening proof (proves knowledge of witness)
        result = gs_open_prove(proof->opening_proof, witness, randomness, 
                              proof->commitment, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to generate opening proof\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 3: Verify the relation (x, w) ∈ R
        // For our case: R(x, w) checks if x = g^w (discrete log relation)
        ec_t generator;
        ec_null(generator);
        ec_new(generator);
        ec_curve_get_gen(generator);
        ec_mul(witness_commitment, generator, witness);
        if (ec_cmp(witness_commitment, statement) != RLC_EQ) {
            printf("[ERROR] Witness does not satisfy relation R(x,w)\n");
            ec_free(generator);
            RLC_THROW(ERR_CAUGHT);
        }
        ec_free(generator);
        
        // Step 4: Compute Fiat-Shamir challenge
        result = compute_fiat_shamir_challenge(proof->challenge, proof, 
                                             statement, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to compute Fiat-Shamir challenge\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 5: Generate proof ID and set version
        md_map(proof->proof_id, (uint8_t*)witness, bn_size_bin(witness));
        proof->proof_version = MALLEABLE_PROOF_VERSION;
        
        printf("[INFO] Malleable proof generation completed successfully\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("[ERROR] Failed to generate malleable proof\n");
    } RLC_FINALLY {
        bn_free(q); bn_free(randomness);
        ec_free(witness_commitment);
    }
    
    return result;
}

int malleable_verify(const malleable_proof_scheme_t proof,
                    const ec_t statement,
                    const gs_crs_t crs) {
    int result = RLC_OK;
    bn_t recomputed_challenge;
    // ec_t reconstructed_statement; // 未使用的变量已移除
    
    bn_null(recomputed_challenge);
    
    RLC_TRY {
        if (proof == NULL || statement == NULL || crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_new(recomputed_challenge);
        // ec_new(reconstructed_statement); // 未使用的变量已移除
        
        // Step 1: Verify opening proof
        result = gs_open_verify_proof(proof->opening_proof, proof->commitment, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Opening proof verification failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 2: Recompute and verify Fiat-Shamir challenge
        result = compute_fiat_shamir_challenge(recomputed_challenge, proof,
                                             statement, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to recompute challenge\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        if (bn_cmp(recomputed_challenge, proof->challenge) != RLC_EQ) {
            printf("[ERROR] Fiat-Shamir challenge verification failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 3: Check proof version
        if (proof->proof_version != MALLEABLE_PROOF_VERSION) {
            printf("[ERROR] Unsupported proof version: %d\n", proof->proof_version);
            RLC_THROW(ERR_CAUGHT);
        }
        
        printf("[INFO] Malleable proof verification completed successfully\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("[ERROR] Malleable proof verification failed\n");
    } RLC_FINALLY {
        bn_free(recomputed_challenge);
        // ec_free(reconstructed_statement); // 未使用的变量已移除
    }
    
    return result;
}

int malleable_zkeval(malleable_proof_scheme_t proof_out,
                    const malleable_proof_scheme_t proof_in,
                    const transformation_functions_t transform_funcs,
                    const ec_t statement_in,
                    ec_t statement_out,
                    const gs_crs_t crs) {
    int result = RLC_OK;
    bn_t transformed_witness, q;
    
    bn_null(transformed_witness); bn_null(q);
    
    RLC_TRY {
        if (proof_out == NULL || proof_in == NULL || transform_funcs == NULL ||
            statement_in == NULL || statement_out == NULL || crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_new(transformed_witness); bn_new(q);
        ec_curve_get_ord(q);
        
        printf("[INFO] Starting ZKEval transformation...\n");
        
        // Step 1: Apply statement transformation x' = T_stmt(x)
        result = transform_statement(statement_out, statement_in, transform_funcs);
        if (result != RLC_OK) {
            printf("[ERROR] Statement transformation failed\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 2: Initialize output proof structure
        malleable_proof_new(proof_out);
        
        // Copy CRS
        gs_crs_new(proof_out->crs);
        g1_copy(proof_out->crs->G1_base, crs->G1_base);
        g1_copy(proof_out->crs->H1_base, crs->H1_base);
        
        // Step 3: Transform commitment using malleability
        // This is the core of the malleable proof: we can transform the proof
        // without knowing the original witness
        
        // For GS commitments: C' = C^r · Com(0, s) for random s
        bn_t transform_randomness;
        bn_null(transform_randomness);
        bn_new(transform_randomness);
        bn_rand_mod(transform_randomness, q);
        
        // Apply transformation to commitment
        g1_t temp_commitment;
        g1_null(temp_commitment);
        g1_new(temp_commitment);
        
        // C' = C^(statement_factor) 
        g1_mul(temp_commitment, proof_in->commitment->C, 
               transform_funcs->statement_factor);
        g1_copy(proof_out->commitment->C, temp_commitment);
        
        // Step 4: Transform the opening proof accordingly
        // The opening proof must be updated to reflect the transformation
        g1_copy(proof_out->opening_proof->T, proof_in->opening_proof->T);
        
        // Transform responses: z' = z * factor + randomness
        bn_mul(proof_out->opening_proof->z_m, proof_in->opening_proof->z_m,
               transform_funcs->statement_factor);
        bn_mod(proof_out->opening_proof->z_m, proof_out->opening_proof->z_m, q);
        
        bn_mul(proof_out->opening_proof->z_r, proof_in->opening_proof->z_r,
               transform_funcs->statement_factor);
        bn_add(proof_out->opening_proof->z_r, proof_out->opening_proof->z_r,
               transform_randomness);
        bn_mod(proof_out->opening_proof->z_r, proof_out->opening_proof->z_r, q);
        
        // Step 5: Update challenge and responses for new statement
        result = compute_fiat_shamir_challenge(proof_out->challenge, proof_out,
                                             statement_out, crs);
        if (result != RLC_OK) {
            printf("[ERROR] Failed to compute new challenge\n");
            RLC_THROW(ERR_CAUGHT);
        }
        
        // Step 6: Copy transformation functions
        proof_out->transform_funcs = malloc(sizeof(transformation_functions_st));
        bn_new(proof_out->transform_funcs->randomization_factor);
        bn_new(proof_out->transform_funcs->statement_factor);
        bn_copy(proof_out->transform_funcs->randomization_factor,
                transform_funcs->randomization_factor);
        bn_copy(proof_out->transform_funcs->statement_factor,
                transform_funcs->statement_factor);
        
        // Update proof metadata
        proof_out->proof_version = MALLEABLE_PROOF_VERSION;
        memcpy(proof_out->proof_id, proof_in->proof_id, PROOF_ID_LENGTH);
        
        printf("[INFO] ZKEval transformation completed successfully\n");
        
        bn_free(transform_randomness);
        g1_free(temp_commitment);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
        printf("[ERROR] ZKEval transformation failed\n");
    } RLC_FINALLY {
        bn_free(transformed_witness); bn_free(q);
    }
    
    return result;
}

// ========== Transformation Function Utilities ==========

int transformation_functions_init(transformation_functions_t *tf) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (tf == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        *tf = malloc(sizeof(transformation_functions_st));
        if (*tf == NULL) {
            RLC_THROW(ERR_NO_MEMORY);
        }
        
        bn_new((*tf)->randomization_factor);
        bn_new((*tf)->statement_factor);
        
        // Initialize with identity transformations
        bn_set_dig((*tf)->randomization_factor, 1);
        bn_set_dig((*tf)->statement_factor, 1);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // Cleanup handled in catch block
    }
    
    return result;
}

int transformation_set_witness_factor(transformation_functions_t tf, const bn_t factor) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (tf == NULL || factor == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_copy(tf->randomization_factor, factor);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // No cleanup needed
    }
    
    return result;
}

int transformation_set_statement_factor(transformation_functions_t tf, const bn_t factor) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (tf == NULL || factor == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_copy(tf->statement_factor, factor);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // No cleanup needed
    }
    
    return result;
}

int transform_witness(bn_t witness_out, const bn_t witness_in,
                     const transformation_functions_t tf) {
    int result = RLC_OK;
    bn_t q;
    
    bn_null(q);
    
    RLC_TRY {
        if (witness_out == NULL || witness_in == NULL || tf == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_new(q);
        ec_curve_get_ord(q);
        
        // w' = T_wit(w) = w + r1 (additive transformation)
        bn_add(witness_out, witness_in, tf->randomization_factor);
        bn_mod(witness_out, witness_out, q);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        bn_free(q);
    }
    
    return result;
}

int transform_statement(ec_t statement_out, const ec_t statement_in,
                       const transformation_functions_t tf) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (statement_out == NULL || statement_in == NULL || tf == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        // x' = T_stmt(x) = x^r2 (multiplicative transformation)
        ec_mul(statement_out, statement_in, tf->statement_factor);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // No cleanup needed
    }
    
    return result;
}

// ========== Utility Functions ==========

int compute_fiat_shamir_challenge(bn_t challenge,
                                 const malleable_proof_scheme_t proof,
                                 const ec_t statement,
                                 const gs_crs_t crs) {
    int result = RLC_OK;
    uint8_t hash_input[1024];
    uint8_t hash_output[RLC_MD_LEN];
    size_t offset = 0;
    bn_t q;
    
    bn_null(q);
    
    RLC_TRY {
        if (challenge == NULL || proof == NULL || statement == NULL || crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        bn_new(q);
        ec_curve_get_ord(q);
        
        // Serialize inputs for hash: CRS || commitment || statement || proof_id
        
        // Add CRS bases
        g1_write_bin(hash_input + offset, g1_size_bin(crs->G1_base, 0), 
                    crs->G1_base, 0);
        offset += g1_size_bin(crs->G1_base, 0);
        
        g1_write_bin(hash_input + offset, g1_size_bin(crs->H1_base, 0),
                    crs->H1_base, 0);
        offset += g1_size_bin(crs->H1_base, 0);
        
        // Add commitment
        g1_write_bin(hash_input + offset, g1_size_bin(proof->commitment->C, 0),
                    proof->commitment->C, 0);
        offset += g1_size_bin(proof->commitment->C, 0);
        
        // Add statement
        ec_write_bin(hash_input + offset, ec_size_bin(statement, 0), statement, 0);
        offset += ec_size_bin(statement, 0);
        
        // Add proof ID
        memcpy(hash_input + offset, proof->proof_id, PROOF_ID_LENGTH);
        offset += PROOF_ID_LENGTH;
        
        // Compute hash
        md_map(hash_output, hash_input, offset);
        
        // Convert to challenge in correct range
        if (8 * RLC_MD_LEN > bn_bits(q)) {
            unsigned len = RLC_CEIL(bn_bits(q), 8);
            bn_read_bin(challenge, hash_output, len);
            bn_rsh(challenge, challenge, 8 * RLC_MD_LEN - bn_bits(q));
        } else {
            bn_read_bin(challenge, hash_output, RLC_MD_LEN);
        }
        bn_mod(challenge, challenge, q);
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        bn_free(q);
    }
    
    return result;
}

int malleable_proof_serialize(uint8_t* buffer, size_t* length,
                             const malleable_proof_scheme_t proof) {
    int result = RLC_OK;
    size_t offset = 0;
    
    RLC_TRY {
        if (buffer == NULL || length == NULL || proof == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        // Serialize version
        memcpy(buffer + offset, &proof->proof_version, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Serialize proof ID
        memcpy(buffer + offset, proof->proof_id, PROOF_ID_LENGTH);
        offset += PROOF_ID_LENGTH;
        
        // Serialize CRS (G1_base and H1_base)
        g1_write_bin(buffer + offset, g1_size_bin(proof->crs->G1_base, 1),
                    proof->crs->G1_base, 1);
        offset += g1_size_bin(proof->crs->G1_base, 1);
        
        g1_write_bin(buffer + offset, g1_size_bin(proof->crs->H1_base, 1),
                    proof->crs->H1_base, 1);
        offset += g1_size_bin(proof->crs->H1_base, 1);
        
        // Serialize commitment
        g1_write_bin(buffer + offset, g1_size_bin(proof->commitment->C, 1),
                    proof->commitment->C, 1);
        offset += g1_size_bin(proof->commitment->C, 1);
        
        // Serialize challenge
        bn_write_bin(buffer + offset, bn_size_bin(proof->challenge),
                    proof->challenge);
        offset += bn_size_bin(proof->challenge);
        
        *length = offset;
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // No cleanup needed
    }
    
    return result;
}

int check_witness_indistinguishability(const malleable_proof_scheme_t proof1,
                                      const malleable_proof_scheme_t proof2,
                                      const gs_crs_t crs) {
    int result = RLC_OK;
    
    RLC_TRY {
        if (proof1 == NULL || proof2 == NULL || crs == NULL) {
            RLC_THROW(ERR_NO_VALID);
        }
        
        // Check that proofs are for the same statement but potentially different witnesses
        // This is a basic structural check - in practice, WI is a computational property
        
        if (proof1->proof_version != proof2->proof_version) {
            printf("[ERROR] Proof versions don't match\n");
            RLC_THROW(ERR_MALLEABLE_WI_VIOLATION);
        }
        
        // Commitments should be computationally indistinguishable
        // but we can't check this directly without the underlying witnesses
        
        printf("[INFO] Basic WI structural check passed\n");
        
    } RLC_CATCH_ANY {
        result = RLC_ERR;
    } RLC_FINALLY {
        // No cleanup needed
    }
    
    return result;
}
