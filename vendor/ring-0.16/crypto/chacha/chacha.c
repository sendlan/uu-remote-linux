// LoongArch port: pure-C ChaCha20 (ctr32) fallback.
// ring 0.16 normally supplies GFp_ChaCha20_ctr32 only as perlasm per-arch;
// there is no loongarch64 perlasm, so build a generic C version here.
//
// ABI matches the perlasm entry points:
//   GFp_ChaCha20_ctr32(out, in, in_len, key, iv)
//   key: 32 bytes (8 little-endian u32 words)
//   iv : 16 bytes = 4-byte little-endian initial counter || 12-byte nonce
// in and out may overlap exactly.

#include <stdint.h>
#include <stddef.h>

#if defined(__loongarch64)

static uint32_t load32_le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static void store32_le(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

#define ROTL(v, c) (((v) << (c)) | ((v) >> (32 - (c))))
#define QR(a, b, c, d)                     \
  do {                                     \
    a += b; d ^= a; d = ROTL(d, 16);       \
    c += d; b ^= c; b = ROTL(b, 12);       \
    a += b; d ^= a; d = ROTL(d, 8);        \
    c += d; b ^= c; b = ROTL(b, 7);        \
  } while (0)

static void chacha20_block(uint32_t out[16], const uint32_t state[16]) {
  uint32_t x[16];
  int i;

  for (i = 0; i < 16; i++) {
    x[i] = state[i];
  }
  for (i = 0; i < 10; i++) {
    QR(x[0], x[4], x[8], x[12]);
    QR(x[1], x[5], x[9], x[13]);
    QR(x[2], x[6], x[10], x[14]);
    QR(x[3], x[7], x[11], x[15]);
    QR(x[0], x[5], x[10], x[15]);
    QR(x[1], x[6], x[11], x[12]);
    QR(x[2], x[7], x[8], x[13]);
    QR(x[3], x[4], x[9], x[14]);
  }
  for (i = 0; i < 16; i++) {
    out[i] = x[i] + state[i];
  }
}

void GFp_ChaCha20_ctr32(uint8_t *out, const uint8_t *in, size_t in_len,
                        const uint8_t key[32], const uint8_t iv[16]);

void GFp_ChaCha20_ctr32(uint8_t *out, const uint8_t *in, size_t in_len,
                        const uint8_t key[32], const uint8_t iv[16]) {
  static const uint32_t SIGMA[4] = {0x61707865, 0x3320646e, 0x79622d32,
                                    0x6b206574};
  uint32_t state[16], block[16];
  uint8_t keystream[64];
  size_t n;

  if (in_len == 0) {
    return;
  }

  for (int i = 0; i < 4; i++) {
    state[i] = SIGMA[i];
  }
  for (int i = 0; i < 8; i++) {
    state[4 + i] = load32_le(key + 4 * i);
  }
  state[12] = load32_le(iv);  // initial counter
  for (int i = 0; i < 3; i++) {
    state[13 + i] = load32_le(iv + 4 + 4 * i);
  }

  while (in_len > 0) {
    chacha20_block(block, state);
    for (int i = 0; i < 16; i++) {
      store32_le(keystream + 4 * i, block[i]);
    }

    n = in_len < 64 ? in_len : 64;
    for (size_t i = 0; i < n; i++) {
      out[i] = in[i] ^ keystream[i];
    }
    in += n;
    out += n;
    in_len -= n;
    state[12]++;  // 32-bit counter wrap is unreachable for sane lengths
  }
}

#endif  // defined(__loongarch64)
