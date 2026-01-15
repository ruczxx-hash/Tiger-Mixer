#ifndef GS_SERIAL_H
#define GS_SERIAL_H

#include "relic.h"
#include "gs.h"

// Fixed sizes for GS components (compressed points, fixed BN)
// Some RELIC builds may not expose RLC_G1_SIZE_COMPRESSED; default to 33 bytes (secp-style) as fallback
#ifndef RLC_G1_SIZE_COMPRESSED
#define RLC_G1_SIZE_COMPRESSED 33
#endif

#ifndef RLC_BN_SIZE
#define RLC_BN_SIZE 34
#endif

// Size helpers
static inline size_t gs_size_crs() {
  return (size_t)(2 * RLC_G1_SIZE_COMPRESSED);
}

static inline size_t gs_size_commitment() {
  return (size_t)RLC_G1_SIZE_COMPRESSED;
}

static inline size_t gs_size_open_proof() {
  return (size_t)(RLC_G1_SIZE_COMPRESSED + 2 * RLC_BN_SIZE);
}

static inline size_t gs_size_lin_proof() {
  return (size_t)(RLC_G1_SIZE_COMPRESSED + 2 * RLC_BN_SIZE);
}

static inline size_t gs_size_eq_proof() {
  return (size_t)(RLC_G1_SIZE_COMPRESSED + RLC_BN_SIZE);
}

// Serialization APIs return bytes written/read
size_t gs_write_crs(uint8_t *out, const gs_crs_t crs);
size_t gs_read_crs(gs_crs_t crs, const uint8_t *in);

size_t gs_write_commitment(uint8_t *out, const gs_commitment_t com);
size_t gs_read_commitment(gs_commitment_t com, const uint8_t *in);

size_t gs_write_open_proof(uint8_t *out, const gs_open_proof_t proof);
size_t gs_read_open_proof(gs_open_proof_t proof, const uint8_t *in);

size_t gs_write_lin_proof(uint8_t *out, const gs_lin_proof_t proof);
size_t gs_read_lin_proof(gs_lin_proof_t proof, const uint8_t *in);

size_t gs_write_eq_proof(uint8_t *out, const gs_eq_proof_t proof);
size_t gs_read_eq_proof(gs_eq_proof_t proof, const uint8_t *in);

#endif // GS_SERIAL_H


