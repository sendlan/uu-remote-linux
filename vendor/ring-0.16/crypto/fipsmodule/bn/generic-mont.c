/* Copyright (c) 2026, the uu-remote-linux LoongArch port contributors.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * LoongArch port: portable Montgomery arithmetic for targets without
 * hand-written assembly. ring 0.16 only ships asm for x86/x86_64/arm/aarch64;
 * this file fills the gap for loongarch64 (compiled only for that target,
 * see build.rs). CIOS Montgomery multiplication over 64-bit limbs. */

#include "internal.h"
#include "../ec/ecp_nistz256.h"
#include "../../internal.h"
#include "../../limbs/limbs.inl"

#if defined(OPENSSL_64_BIT) && defined(OPENSSL_LOONGARCH64)

void GFp_bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp,
                     const BN_ULONG *np, const BN_ULONG *n0, size_t num) {
  BN_ULONG n0_ = n0[0];
  /* ring only ever calls this with |num| == 6 (P-384) or 8 (P-256). */
  BN_ULONG t[16 + 2];
  BN_ULONG sub[16];
  size_t i, j;

  for (i = 0; i < num + 2; i++) {
    t[i] = 0;
  }

  for (i = 0; i < num; i++) {
    BN_ULONG a_i = ap[i];
    BN_ULONG carry = 0;
    BN_ULLONG sum;

    /* t += a_i * b */
    for (j = 0; j < num; j++) {
      sum = (BN_ULLONG)t[j] + (BN_ULLONG)a_i * bp[j] + carry;
      t[j] = (BN_ULONG)sum;
      carry = (BN_ULONG)(sum >> BN_BITS2);
    }
    sum = (BN_ULLONG)t[num] + carry;
    t[num] = (BN_ULONG)sum;
    t[num + 1] = (BN_ULONG)(sum >> BN_BITS2);

    /* reduce one limb: t += m * n, discarding t[0] (it becomes zero) */
    BN_ULONG m = t[0] * n0_;
    carry = (BN_ULONG)(((BN_ULLONG)t[0] + (BN_ULLONG)m * np[0]) >> BN_BITS2);
    for (j = 1; j < num; j++) {
      sum = (BN_ULLONG)t[j] + (BN_ULLONG)m * np[j] + carry;
      t[j] = (BN_ULONG)sum;
      carry = (BN_ULONG)(sum >> BN_BITS2);
    }
    sum = (BN_ULLONG)t[num] + carry;
    t[num] = (BN_ULONG)sum;
    t[num + 1] += (BN_ULONG)(sum >> BN_BITS2);

    /* shift down one word */
    for (j = 0; j < num; j++) {
      t[j] = t[j + 1];
    }
    t[num] = t[num + 1];
    t[num + 1] = 0;
  }

  /* Conditional subtraction of |np| once. */
  BN_ULONG borrow = 0;
  for (j = 0; j < num; j++) {
    BN_ULLONG d = (BN_ULLONG)t[j] - np[j] - borrow;
    sub[j] = (BN_ULONG)d;
    borrow = (BN_ULONG)((d >> BN_BITS2) & 1);
  }
  /* Use the subtracted value if there was no borrow (t >= n) or the
   * multiplication overflowed into t[num]. */
  BN_ULONG use_sub = t[num] | (borrow ^ 1);
  for (j = 0; j < num; j++) {
    rp[j] = use_sub ? sub[j] : t[j];
  }

  /* Fully reduce: the hand-written asm implementations of |GFp_bn_mul_mont|
   * always return a value in [0, n), and callers (e.g. |LIMBS_sub_mod| inside
   * ecp_nistz256.c / gfp_p256.c) rely on that contract. The value above is
   * already < 2n, so one more conditional subtraction suffices. */
  LIMBS_reduce_once(rp, np, num);
}

/* P-256 field modulus, little-endian 64-bit limbs (256 bits = 4 limbs):
 * Q = 2^256 - 2^224 + 2^192 + 2^96 - 1. */
static const BN_ULONG P256_Q[P256_LIMBS] = {
    0xffffffffffffffff, 0x00000000ffffffff,
    0x0000000000000000, 0xffffffff00000001,
};

/* -Q^-1 mod 2^64 == 1 because Q ≡ -1 (mod 2^64). */
static const BN_ULONG P256_Q_N0[BN_MONT_CTX_N0_LIMBS] = {1};

void GFp_nistz256_mul_mont(Limb r[P256_LIMBS], const Limb a[P256_LIMBS],
                           const Limb b[P256_LIMBS]) {
  GFp_bn_mul_mont(r, a, b, P256_Q, P256_Q_N0, P256_LIMBS);
}

void GFp_nistz256_sqr_mont(Limb r[P256_LIMBS], const Limb a[P256_LIMBS]) {
  GFp_nistz256_mul_mont(r, a, a);
}

/* --- P-256 field / point ops normally provided by per-arch asm --- */

static void elem_dbl(Limb r[P256_LIMBS], const Limb a[P256_LIMBS]) {
  LIMBS_add_mod(r, a, a, P256_Q, P256_LIMBS);
}

/* res = a + b mod Q. Operands may be unreduced 4-limb values (< 2^256);
 * the result is fully reduced (< Q), matching the asm contract. */
void GFp_nistz256_add(Limb res[P256_LIMBS], const Limb a[P256_LIMBS],
                      const Limb b[P256_LIMBS]) {
  Limb t[P256_LIMBS + 1];
  static const Limb Q5[P256_LIMBS + 1] = {
      0xffffffffffffffff, 0x00000000ffffffff, 0x0000000000000000,
      0xffffffff00000001, 0,
  };
  t[P256_LIMBS] = limbs_add(t, a, b, P256_LIMBS);
  /* a + b < 2 * 2^256 < 3Q, so two conditional subtractions suffice. */
  LIMBS_reduce_once(t, Q5, P256_LIMBS + 1);
  LIMBS_reduce_once(t, Q5, P256_LIMBS + 1);
  limbs_copy(res, t, P256_LIMBS);
}

/* res = -a mod Q. Operands may be unreduced 4-limb values. */
void GFp_nistz256_neg(Limb res[P256_LIMBS], const Limb a[P256_LIMBS]) {
  static const Limb ZERO[P256_LIMBS] = {0, 0, 0, 0};
  Limb t[P256_LIMBS];
  limbs_copy(t, a, P256_LIMBS);
  LIMBS_reduce_once(t, P256_Q, P256_LIMBS);
  LIMBS_sub_mod(res, ZERO, t, P256_Q, P256_LIMBS);
}

/* Jacobian point doubling for P-256 (a = -3), dbl-2001-b.
 * All inputs/outputs are in Montgomery form. */
void GFp_nistz256_point_double(P256_POINT *r, const P256_POINT *a) {
  const Limb *X = a->X, *Y = a->Y, *Z = a->Z;
  Limb delta[P256_LIMBS], gamma[P256_LIMBS], beta[P256_LIMBS];
  Limb alpha[P256_LIMBS], t[P256_LIMBS], t2[P256_LIMBS];
  Limb b2[P256_LIMBS], b4[P256_LIMBS], b8[P256_LIMBS];

  /* delta = Z^2, gamma = Y^2, beta = X*gamma */
  GFp_nistz256_sqr_mont(delta, Z);
  GFp_nistz256_sqr_mont(gamma, Y);
  GFp_nistz256_mul_mont(beta, X, gamma);

  /* alpha = 3 * (X - delta) * (X + delta) */
  {
    Limb xmd[P256_LIMBS], xpd[P256_LIMBS];
    LIMBS_sub_mod(xmd, X, delta, P256_Q, P256_LIMBS);
    LIMBS_add_mod(xpd, X, delta, P256_Q, P256_LIMBS);
    GFp_nistz256_mul_mont(t, xmd, xpd);
    elem_dbl(t2, t);
    /* alpha = t + 2t = 3*(X - delta)*(X + delta) */
    LIMBS_add_mod(alpha, t, t2, P256_Q, P256_LIMBS);
  }

  /* b4 = 4*beta, b8 = 8*beta */
  elem_dbl(b2, beta);
  elem_dbl(b4, b2);
  elem_dbl(b8, b4);

  /* X3 = alpha^2 - 8*beta */
  {
    Limb alpha_sq[P256_LIMBS];
    GFp_nistz256_sqr_mont(alpha_sq, alpha);
    LIMBS_sub_mod(r->X, alpha_sq, b8, P256_Q, P256_LIMBS);
  }

  /* Z3 = (Y + Z)^2 - gamma - delta */
  {
    Limb yz[P256_LIMBS], yz2[P256_LIMBS];
    LIMBS_add_mod(yz, Y, Z, P256_Q, P256_LIMBS);
    GFp_nistz256_sqr_mont(yz2, yz);
    LIMBS_sub_mod(r->Z, yz2, gamma, P256_Q, P256_LIMBS);
    LIMBS_sub_mod(r->Z, r->Z, delta, P256_Q, P256_LIMBS);
  }

  /* Y3 = alpha * (4*beta - X3) - 8*gamma^2 */
  {
    Limb b4mx3[P256_LIMBS], g2[P256_LIMBS];
    LIMBS_sub_mod(b4mx3, b4, r->X, P256_Q, P256_LIMBS);
    GFp_nistz256_mul_mont(t, alpha, b4mx3);
    GFp_nistz256_sqr_mont(g2, gamma);
    elem_dbl(g2, g2);
    elem_dbl(g2, g2);
    elem_dbl(g2, g2);
    LIMBS_sub_mod(r->Y, t, g2, P256_Q, P256_LIMBS);
  }
}

#endif  /* OPENSSL_64_BIT && OPENSSL_LOONGARCH64 */
