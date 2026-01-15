#include <stdlib.h>
#include <string.h>
#include "relic.h"
#include "gs.h"

static void fs_hash_challenge(bn_t e, const g1_t T, const g1_t Ca, const g1_t Cb, const g1_t Cs) {
  int sT = g1_size_bin(T, 1);
  int sA = g1_size_bin(Ca, 1);
  int sB = g1_size_bin(Cb, 1);
  int sS = g1_size_bin(Cs, 1);
  size_t total = (size_t)(sT + sA + sB + sS);
  uint8_t *buf = (uint8_t*)malloc(total);
  size_t off = 0;
  g1_write_bin(buf + off, sT, T, 1); off += (size_t)sT;
  g1_write_bin(buf + off, sA, Ca, 1); off += (size_t)sA;
  g1_write_bin(buf + off, sB, Cb, 1); off += (size_t)sB;
  g1_write_bin(buf + off, sS, Cs, 1); off += (size_t)sS;
  uint8_t h[RLC_MD_LEN];
  md_map(h, buf, off);
  free(buf);
  bn_read_bin(e, h, RLC_MD_LEN);
  bn_t q; bn_null(q); bn_new(q); g1_get_ord(q); bn_mod(e, e, q); bn_free(q);
}

int gs_crs_setup(gs_crs_t crs) {
  int st = RLC_OK;
  bn_t h_scalar; bn_null(h_scalar);
  RLC_TRY {
    if (crs == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(crs->G1_base);
    g1_new(crs->H1_base);
    bn_new(h_scalar);
    
    // Use standard generator for G1_base
    g1_get_gen(crs->G1_base);
    
    // Generate H1_base as a different generator using deterministic scalar
    // Use a fixed scalar derived from "gs-h1" to ensure consistency
    uint8_t tag[] = { 'g','s','-','h','1' };
    bn_read_bin(h_scalar, tag, sizeof(tag));
    g1_mul_gen(crs->H1_base, h_scalar);
    
    // Verify both points are valid
    if (!g1_is_valid(crs->G1_base)) {
      printf("[ERROR] gs_crs_setup: G1_base is invalid\n");
      RLC_THROW(ERR_CAUGHT);
    }
    if (!g1_is_valid(crs->H1_base)) {
      printf("[ERROR] gs_crs_setup: H1_base is invalid\n");
      RLC_THROW(ERR_CAUGHT);
    }
    
    printf("[DEBUG] gs_crs_setup: Both generators are valid\n");
  } RLC_CATCH_ANY { 
    printf("[ERROR] gs_crs_setup: Exception caught\n");
    st = RLC_ERR; 
  } RLC_FINALLY { 
    bn_free(h_scalar); 
  }
  return st;
}

int gs_commit(gs_commitment_t com, const bn_t m, const bn_t r, const gs_crs_t crs) {
  int st = RLC_OK;
  g1_t mH, rG;
  g1_null(mH); g1_null(rG);
  RLC_TRY {
    if (com == NULL || crs == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(com->C); g1_new(mH); g1_new(rG);
    g1_mul(mH, crs->H1_base, m);
    g1_mul(rG, crs->G1_base, r);
    g1_add(com->C, mH, rG);
    g1_norm(com->C, com->C);
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { g1_free(mH); g1_free(rG); }
  return st;
}

int gs_open_verify(const gs_commitment_t com, const bn_t m, const bn_t r, const gs_crs_t crs) {
  int st = RLC_ERR;
  g1_t mH, rG, sum;
  g1_null(mH); g1_null(rG); g1_null(sum);
  RLC_TRY {
    if (com == NULL || crs == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(mH); g1_new(rG); g1_new(sum);
    g1_mul(mH, crs->H1_base, m);
    g1_mul(rG, crs->G1_base, r);
    g1_add(sum, mH, rG); g1_norm(sum, sum);
    st = (g1_cmp(sum, com->C) == RLC_EQ) ? RLC_OK : RLC_ERR;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { g1_free(mH); g1_free(rG); g1_free(sum); }
  return st;
}

int gs_open_prove(gs_open_proof_t proof, const bn_t m, const bn_t r, const gs_commitment_t com, const gs_crs_t crs) {
  int st = RLC_OK;
  bn_t q, w_m, w_r, e; bn_null(q); bn_null(w_m); bn_null(w_r); bn_null(e);
  g1_t lhs, rhs; g1_null(lhs); g1_null(rhs);
  RLC_TRY {
    if (proof == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(proof->T); bn_new(proof->z_m); bn_new(proof->z_r);
    bn_new(q); bn_new(w_m); bn_new(w_r); bn_new(e);
    g1_new(lhs); g1_new(rhs);
    g1_get_ord(q);
    bn_rand_mod(w_m, q); bn_rand_mod(w_r, q);
    // T = w_m*H + w_r*G
    g1_mul(lhs, crs->H1_base, w_m);
    g1_mul(rhs, crs->G1_base, w_r);
    g1_add(proof->T, lhs, rhs); g1_norm(proof->T, proof->T);
    // e = H(T || C)
    bn_t e_local; bn_null(e_local); bn_new(e_local);
    fs_hash_challenge(e_local, proof->T, com->C, com->C, com->C);
    bn_copy(e, e_local); bn_free(e_local);
    // responses
    // z_m = w_m + e*m ; z_r = w_r + e*r
    bn_mul(proof->z_m, e, m); bn_mod(proof->z_m, proof->z_m, q); bn_add(proof->z_m, proof->z_m, w_m); bn_mod(proof->z_m, proof->z_m, q);
    bn_mul(proof->z_r, e, r); bn_mod(proof->z_r, proof->z_r, q); bn_add(proof->z_r, proof->z_r, w_r); bn_mod(proof->z_r, proof->z_r, q);
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(q); bn_free(w_m); bn_free(w_r); bn_free(e); g1_free(lhs); g1_free(rhs);} 
  return st;
}

int gs_open_verify_proof(const gs_open_proof_t proof, const gs_commitment_t com, const gs_crs_t crs) {
  int st = RLC_ERR;
  bn_t e; bn_null(e); g1_t left, right, zH, zG, eC;
  g1_null(left); g1_null(right); g1_null(zH); g1_null(zG); g1_null(eC);
  RLC_TRY {
    bn_new(e); g1_new(left); g1_new(right); g1_new(zH); g1_new(zG); g1_new(eC);
    fs_hash_challenge(e, proof->T, com->C, com->C, com->C);
    // left = T + e*C
    g1_mul(eC, com->C, e); g1_add(left, proof->T, eC); g1_norm(left, left);
    // right = z_m*H + z_r*G
    g1_mul(zH, crs->H1_base, proof->z_m);
    g1_mul(zG, crs->G1_base, proof->z_r);
    g1_add(right, zH, zG); g1_norm(right, right);
    st = (g1_cmp(left, right) == RLC_EQ) ? RLC_OK : RLC_ERR;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(e); g1_free(left); g1_free(right); g1_free(zH); g1_free(zG); g1_free(eC);} 
  return st;
}

int gs_lin_prove(gs_lin_proof_t proof,
                 const bn_t m_a, const bn_t r_a,
                 const bn_t m_b, const bn_t r_b,
                 const bn_t m_s, const bn_t r_s,
                 const gs_commitment_t C_a,
                 const gs_commitment_t C_b,
                 const gs_commitment_t C_sum,
                 const gs_crs_t crs) {
  int st = RLC_OK;
  bn_t q, w_m, w_r, e;
  g1_t T, lhs, rhs;
  bn_null(q); bn_null(w_m); bn_null(w_r); bn_null(e);
  g1_null(T); g1_null(lhs); g1_null(rhs);
  RLC_TRY {
    if (proof == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(proof->T); bn_new(proof->z_m); bn_new(proof->z_r);
    bn_new(q); bn_new(w_m); bn_new(w_r); bn_new(e);
    g1_new(T); g1_new(lhs); g1_new(rhs);

    g1_get_ord(q);
    bn_rand_mod(w_m, q); bn_rand_mod(w_r, q);
    // T = w_m*H + w_r*G
    g1_mul(lhs, crs->H1_base, w_m);
    g1_mul(rhs, crs->G1_base, w_r);
    g1_add(proof->T, lhs, rhs); g1_norm(proof->T, proof->T);

    // Fiat-Shamir over (T, C_a, C_b, C_sum)
    fs_hash_challenge(e, proof->T, C_a->C, C_b->C, C_sum->C);

    // Relation: C_sum ?= C_a + C_b (component-wise in G1)
    // Prove z_m = w_m + e*(m_s - m_a - m_b), z_r = w_r + e*(r_s - r_a - r_b)
    bn_t tmp; bn_null(tmp); bn_new(tmp);
    // z_m
    bn_add(tmp, m_a, m_b); bn_mod(tmp, tmp, q);
    bn_sub(tmp, m_s, tmp); bn_mod(tmp, tmp, q);
    bn_mul(tmp, tmp, e); bn_mod(tmp, tmp, q);
    bn_add(proof->z_m, w_m, tmp); bn_mod(proof->z_m, proof->z_m, q);
    // z_r
    bn_add(tmp, r_a, r_b); bn_mod(tmp, tmp, q);
    bn_sub(tmp, r_s, tmp); bn_mod(tmp, tmp, q);
    bn_mul(tmp, tmp, e); bn_mod(tmp, tmp, q);
    bn_add(proof->z_r, w_r, tmp); bn_mod(proof->z_r, proof->z_r, q);
    bn_free(tmp);

    st = RLC_OK;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY {
    bn_free(q); bn_free(w_m); bn_free(w_r); bn_free(e);
    g1_free(T); g1_free(lhs); g1_free(rhs);
  }
  return st;
}

int gs_lin_verify(const gs_lin_proof_t proof,
                  const gs_commitment_t C_a,
                  const gs_commitment_t C_b,
                  const gs_commitment_t C_sum,
                  const gs_crs_t crs) {
  int st = RLC_ERR;
  bn_t q, e; bn_null(q); bn_null(e);
  g1_t C_ab, left, right, z_part_m, z_part_r, e_part;
  g1_null(C_ab); g1_null(left); g1_null(right); g1_null(z_part_m); g1_null(z_part_r); g1_null(e_part);
  RLC_TRY {
    if (proof == NULL) RLC_THROW(ERR_CAUGHT);
    bn_new(q); bn_new(e);
    g1_new(C_ab); g1_new(left); g1_new(right); g1_new(z_part_m); g1_new(z_part_r); g1_new(e_part);
    g1_get_ord(q);

    // e = H(T || C_a || C_b || C_sum)
    fs_hash_challenge(e, proof->T, C_a->C, C_b->C, C_sum->C);

    // left = T + e*(C_sum - C_a - C_b)
    g1_add(C_ab, C_a->C, C_b->C); g1_norm(C_ab, C_ab);
    g1_sub(left, C_sum->C, C_ab); g1_norm(left, left);
    g1_mul(left, left, e);
    g1_add(left, left, proof->T); g1_norm(left, left);

    // right = z_m*H + z_r*G
    g1_mul(z_part_m, crs->H1_base, proof->z_m);
    g1_mul(z_part_r, crs->G1_base, proof->z_r);
    g1_add(right, z_part_m, z_part_r); g1_norm(right, right);

    st = (g1_cmp(left, right) == RLC_EQ) ? RLC_OK : RLC_ERR;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY {
    bn_free(q); bn_free(e);
    g1_free(C_ab); g1_free(left); g1_free(right); g1_free(z_part_m); g1_free(z_part_r); g1_free(e_part);
  }
  return st;
}

int gs_link_prove(gs_link_proof_t proof,
                  const bn_t m_b, const bn_t r_b,
                  const gs_commitment_t C_base,
                  const gs_commitment_t C_sum,
                  const gs_crs_t crs) {
  // Equivalent to linear proof where C_sum = C_base + Com(m_b,r_b)
  // We implement directly as Schnorr on delta = C_sum - C_base
  int st = RLC_OK;
  bn_t q, w_m, w_r, e; bn_null(q); bn_null(w_m); bn_null(w_r); bn_null(e);
  g1_t delta, lhs, rhs; g1_null(delta); g1_null(lhs); g1_null(rhs);
  RLC_TRY {
    if (proof == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(proof->T); bn_new(proof->z_m); bn_new(proof->z_r);
    bn_new(q); bn_new(w_m); bn_new(w_r); bn_new(e);
    g1_new(delta); g1_new(lhs); g1_new(rhs);
    g1_get_ord(q);

    g1_sub(delta, C_sum->C, C_base->C); g1_norm(delta, delta);
    // commit
    bn_rand_mod(w_m, q); bn_rand_mod(w_r, q);
    g1_mul(lhs, crs->H1_base, w_m); g1_mul(rhs, crs->G1_base, w_r);
    g1_add(proof->T, lhs, rhs); g1_norm(proof->T, proof->T);
    // hash(T, delta, delta)
    fs_hash_challenge(e, proof->T, delta, delta, delta);
    // responses: z = w + e * witness
    bn_mul(proof->z_m, e, m_b); bn_mod(proof->z_m, proof->z_m, q); bn_add(proof->z_m, proof->z_m, w_m); bn_mod(proof->z_m, proof->z_m, q);
    bn_mul(proof->z_r, e, r_b); bn_mod(proof->z_r, proof->z_r, q); bn_add(proof->z_r, proof->z_r, w_r); bn_mod(proof->z_r, proof->z_r, q);
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(q); bn_free(w_m); bn_free(w_r); bn_free(e); g1_free(delta); g1_free(lhs); g1_free(rhs);} 
  return st;
}

int gs_link_verify(const gs_link_proof_t proof,
                   const gs_commitment_t C_base,
                   const gs_commitment_t C_sum,
                   const gs_crs_t crs) {
  int st = RLC_ERR;
  bn_t e; bn_null(e);
  g1_t delta, left, right, zH, zG, eDelta; g1_null(delta); g1_null(left); g1_null(right); g1_null(zH); g1_null(zG); g1_null(eDelta);
  RLC_TRY {
    bn_new(e); g1_new(delta); g1_new(left); g1_new(right); g1_new(zH); g1_new(zG); g1_new(eDelta);
    g1_sub(delta, C_sum->C, C_base->C); g1_norm(delta, delta);
    fs_hash_challenge(e, proof->T, delta, delta, delta);
    // left = T + e*delta
    g1_mul(eDelta, delta, e); g1_add(left, proof->T, eDelta); g1_norm(left, left);
    // right = z_m*H + z_r*G
    g1_mul(zH, crs->H1_base, proof->z_m);
    g1_mul(zG, crs->G1_base, proof->z_r);
    g1_add(right, zH, zG); g1_norm(right, right);
    st = (g1_cmp(left, right) == RLC_EQ) ? RLC_OK : RLC_ERR;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(e); g1_free(delta); g1_free(left); g1_free(right); g1_free(zH); g1_free(zG); g1_free(eDelta);} 
  return st;
}


// ===================== Re-randomizable GS-style equality proof (EC only) =====================
// CRS-only challenge: e = H(G1_base || H1_base) to enable public scaling.
static void fs_hash_challenge_crs_only(bn_t e, const gs_crs_t crs) {
  uint8_t h[RLC_MD_LEN];
  int sG = g1_size_bin(crs->G1_base, 1);
  int sH = g1_size_bin(crs->H1_base, 1);
  size_t total = (size_t)(sG + sH);
  uint8_t *buf = (uint8_t*)malloc(total);
  size_t off = 0;
  g1_write_bin(buf + off, sG, crs->G1_base, 1); off += (size_t)sG;
  g1_write_bin(buf + off, sH, crs->H1_base, 1); off += (size_t)sH;
  md_map(h, buf, off);
  free(buf);
  bn_read_bin(e, h, RLC_MD_LEN);
  bn_t q; bn_null(q); bn_new(q); g1_get_ord(q); bn_mod(e, e, q); bn_free(q);
}

int gs_eq_prove(gs_eq_proof_t proof, const bn_t alpha, const g1_t gamma, const gs_crs_t crs) {
  int st = RLC_OK;
  bn_t q, w, e; bn_null(q); bn_null(w); bn_null(e);
  g1_t zH, lhs; g1_null(zH); g1_null(lhs);
  RLC_TRY {
    if (proof == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(proof->T); bn_new(proof->z);
    bn_new(q); bn_new(w); bn_new(e);
    g1_new(zH); g1_new(lhs);
    g1_get_ord(q);
    // commit T = w*H
    bn_rand_mod(w, q);
    g1_mul(proof->T, crs->H1_base, w);
    // CRS-only challenge
    fs_hash_challenge_crs_only(e, crs);
    // response z = w + e*alpha
    bn_mul(proof->z, e, alpha); bn_mod(proof->z, proof->z, q);
    bn_add(proof->z, proof->z, w); bn_mod(proof->z, proof->z, q);
    st = RLC_OK;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(q); bn_free(w); bn_free(e); g1_free(zH); g1_free(lhs); }
  return st;
}

int gs_eq_verify(const gs_eq_proof_t proof, const g1_t gamma, const gs_crs_t crs) {
  int st = RLC_ERR;
  bn_t e; bn_null(e);
  g1_t left, right, zH, eGamma; g1_null(left); g1_null(right); g1_null(zH); g1_null(eGamma);
  RLC_TRY {
    bn_new(e); g1_new(left); g1_new(right); g1_new(zH); g1_new(eGamma);
    // CRS-only challenge
    fs_hash_challenge_crs_only(e, crs);
    // left = T + e*gamma
    g1_mul(eGamma, gamma, e); g1_add(left, proof->T, eGamma); g1_norm(left, left);
    // right = z*H
    g1_mul(right, crs->H1_base, proof->z);
    st = (g1_cmp(left, right) == RLC_EQ) ? RLC_OK : RLC_ERR;
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY { bn_free(e); g1_free(left); g1_free(right); g1_free(zH); g1_free(eGamma);} 
  return st;
}

int gs_eq_scale(gs_eq_proof_t out, const gs_eq_proof_t in, const bn_t factor) {
  int st = RLC_OK;
  RLC_TRY {
    if (out == NULL || in == NULL) RLC_THROW(ERR_CAUGHT);
    g1_new(out->T); bn_new(out->z);
    g1_mul(out->T, in->T, factor);
    // Scale response modulo group order to keep bounded length
    bn_t q; bn_null(q); bn_new(q); g1_get_ord(q);
    bn_mul(out->z, in->z, factor);
    bn_mod(out->z, out->z, q);
    bn_free(q);
  } RLC_CATCH_ANY { st = RLC_ERR; } RLC_FINALLY {}
  return st;
}


