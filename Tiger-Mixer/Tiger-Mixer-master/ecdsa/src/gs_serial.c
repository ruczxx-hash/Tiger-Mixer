#include <string.h>
#include "relic.h"
#include "gs.h"
#include "gs_serial.h"

size_t gs_write_crs(uint8_t *out, const gs_crs_t crs) {
  size_t off = 0;
  g1_write_bin(out + off, RLC_G1_SIZE_COMPRESSED, crs->G1_base, 1); off += RLC_G1_SIZE_COMPRESSED;
  g1_write_bin(out + off, RLC_G1_SIZE_COMPRESSED, crs->H1_base, 1); off += RLC_G1_SIZE_COMPRESSED;
  return off;
}

size_t gs_read_crs(gs_crs_t crs, const uint8_t *in) {
  size_t off = 0;
  g1_read_bin(crs->G1_base, in + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
  g1_read_bin(crs->H1_base, in + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
  return off;
}

size_t gs_write_commitment(uint8_t *out, const gs_commitment_t com) {
  g1_write_bin(out, RLC_G1_SIZE_COMPRESSED, com->C, 1);
  return (size_t)RLC_G1_SIZE_COMPRESSED;
}

size_t gs_read_commitment(gs_commitment_t com, const uint8_t *in) {
  g1_read_bin(com->C, in, RLC_G1_SIZE_COMPRESSED);
  return (size_t)RLC_G1_SIZE_COMPRESSED;
}

size_t gs_write_open_proof(uint8_t *out, const gs_open_proof_t proof) {
  size_t off = 0;
  g1_write_bin(out + off, RLC_G1_SIZE_COMPRESSED, proof->T, 1); off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(out + off, RLC_BN_SIZE, proof->z_m); off += RLC_BN_SIZE;
  bn_write_bin(out + off, RLC_BN_SIZE, proof->z_r); off += RLC_BN_SIZE;
  return off;
}

size_t gs_read_open_proof(gs_open_proof_t proof, const uint8_t *in) {
  size_t off = 0;
  g1_read_bin(proof->T, in + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
  bn_read_bin(proof->z_m, in + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
  bn_read_bin(proof->z_r, in + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
  return off;
}

size_t gs_write_lin_proof(uint8_t *out, const gs_lin_proof_t proof) {
  size_t off = 0;
  g1_write_bin(out + off, RLC_G1_SIZE_COMPRESSED, proof->T, 1); off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(out + off, RLC_BN_SIZE, proof->z_m); off += RLC_BN_SIZE;
  bn_write_bin(out + off, RLC_BN_SIZE, proof->z_r); off += RLC_BN_SIZE;
  return off;
}

size_t gs_read_lin_proof(gs_lin_proof_t proof, const uint8_t *in) {
  size_t off = 0;
  g1_read_bin(proof->T, in + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
  bn_read_bin(proof->z_m, in + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
  bn_read_bin(proof->z_r, in + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
  return off;
}

size_t gs_write_eq_proof(uint8_t *out, const gs_eq_proof_t proof) {
  size_t off = 0;
  g1_write_bin(out + off, RLC_G1_SIZE_COMPRESSED, proof->T, 1); off += RLC_G1_SIZE_COMPRESSED;
  bn_write_bin(out + off, RLC_BN_SIZE, proof->z); off += RLC_BN_SIZE;
  return off;
}

size_t gs_read_eq_proof(gs_eq_proof_t proof, const uint8_t *in) {
  size_t off = 0;
  g1_read_bin(proof->T, in + off, RLC_G1_SIZE_COMPRESSED); off += RLC_G1_SIZE_COMPRESSED;
  bn_read_bin(proof->z, in + off, RLC_BN_SIZE); off += RLC_BN_SIZE;
  return off;
}


