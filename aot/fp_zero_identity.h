/* Exact shared integer zero-identity prefix for software FP helpers.
 *
 * Default-off prototype (see the M8_FP_ZERO_IDENTITY hooks in aot/fp.h and
 * hosts/web/fp_wasm.h). Pure integer operations on raw register bits; no
 * FPSCR access, no SoftFloat/host-fenv use, no per-mode tables. Usable from
 * both the native software path and the WebAssembly software path without a
 * browser fork.
 *
 * Contract: when this prefix admits an (op, a, b, d) tuple, the returned
 * result bits are provably identical to the full software helper under every
 * supported rounding mode (RN/RP/RM/RZ), both flush-to-zero modes (FZ=0/1)
 * and both default-NaN modes (DN=0/1), and the full helper publishes no new
 * cumulative flags, input-denormal effects or NZCV changes for the tuple.
 * The caller therefore leaves FPSCR untouched on admit and routes every
 * decline to the existing exact helper unchanged.
 *
 * Why admission is exact: every admitted case computes an infinitely precise
 * result that is already representable, from operands the input-flush stage
 * cannot alter (exact zeros and finite normals only):
 * - ADD/SUB with exactly one zero operand: x+0 = x-0 = x, 0-x = -x. The
 *   result is the nonzero operand (never a signed-zero sum), so no rounding
 *   mode can change it. Both-zero ADD/SUB decline because a +0/-0 sum sign
 *   depends on the rounding mode.
 * - MUL with a zero factor and a zero/normal cofactor: an exact signed zero
 *   with xor sign. Infinities decline (they would raise invalid).
 * - DIV with a zero numerator and normal denominator: an exact signed zero.
 * - SQRT of a signed zero with an exactly-zero ignored operand: itself.
 * - FMA with a zero product factor, zero/normal cofactor and normal addend:
 *   the product is an exact signed zero and adding it to a nonzero finite
 *   addend is exact, so single-rounding returns the addend.
 * Ignored operands must be exactly zero for non-FMA ops (a subnormal ignored
 * word would publish an input-denormal flag in the full helper). NaN,
 * infinite and subnormal relevant operands always decline, as do MAXNM/MINNM
 * and unknown ops.
 */
#ifndef M8_FP_ZERO_IDENTITY_H
#define M8_FP_ZERO_IDENTITY_H
#include <stdint.h>

/* Operation numbers match the private FP encoding shared by aot/fp.h and
 * hosts/web/fp_wasm.h. */
enum {
    M8_ZID_ADD = 0,
    M8_ZID_SUB = 1,
    M8_ZID_MUL = 2,
    M8_ZID_DIV = 3,
    M8_ZID_SQRT = 4,
    M8_ZID_FMA = 5
};

static inline int m8_zid_is_zero32(uint32_t x) {
    return (x & 0x7fffffffu) == 0;
}

static inline int m8_zid_is_normal32(uint32_t x) {
    uint32_t mag = x & 0x7fffffffu;
    return mag >= 0x00800000u && mag < 0x7f800000u;
}

static inline int m8_zid_is_zero64(uint64_t x) {
    return (x & UINT64_C(0x7fffffffffffffff)) == 0;
}

static inline int m8_zid_is_normal64(uint64_t x) {
    uint64_t mag = x & UINT64_C(0x7fffffffffffffff);
    return mag >= UINT64_C(0x0010000000000000) &&
           mag < UINT64_C(0x7ff0000000000000);
}

/* Admit an exact zero-identity binary32 operation. Returns 1 with *result on
 * admit, 0 on decline (caller runs the full helper). */
static inline int m8_fp_zero_identity32(int op, uint32_t a, uint32_t b,
                                        uint32_t d, uint32_t *result) {
    int az, bz, an, bn;
    if (op < 0 || op > M8_ZID_FMA)
        return 0;
    if (op != M8_ZID_FMA && d != 0)
        return 0;
    az = m8_zid_is_zero32(a);
    bz = m8_zid_is_zero32(b);
    an = m8_zid_is_normal32(a);
    bn = m8_zid_is_normal32(b);
    switch (op) {
    case M8_ZID_ADD:
        if (az && bn) {
            *result = b;
            return 1;
        }
        if (an && bz) {
            *result = a;
            return 1;
        }
        return 0;
    case M8_ZID_SUB:
        if (an && bz) {
            *result = a;
            return 1;
        }
        if (az && bn) {
            *result = b ^ 0x80000000u;
            return 1;
        }
        return 0;
    case M8_ZID_MUL:
        if ((az && (bz || bn)) || (bz && (az || an))) {
            *result = (a ^ b) & 0x80000000u;
            return 1;
        }
        return 0;
    case M8_ZID_DIV:
        if (az && bn) {
            *result = (a ^ b) & 0x80000000u;
            return 1;
        }
        return 0;
    case M8_ZID_SQRT:
        if (b != 0)
            return 0;
        if (az) {
            *result = a;
            return 1;
        }
        return 0;
    default:
        if (((az && (bz || bn)) || (bz && (az || an))) &&
            m8_zid_is_normal32(d)) {
            *result = d;
            return 1;
        }
        return 0;
    }
}

/* Admit an exact zero-identity binary64 operation. The width-independent
 * argument above holds with the binary64 zero/normal classes. */
static inline int m8_fp_zero_identity64(int op, uint64_t a, uint64_t b,
                                        uint64_t d, uint64_t *result) {
    int az, bz, an, bn;
    if (op < 0 || op > M8_ZID_FMA)
        return 0;
    if (op != M8_ZID_FMA && d != 0)
        return 0;
    az = m8_zid_is_zero64(a);
    bz = m8_zid_is_zero64(b);
    an = m8_zid_is_normal64(a);
    bn = m8_zid_is_normal64(b);
    switch (op) {
    case M8_ZID_ADD:
        if (az && bn) {
            *result = b;
            return 1;
        }
        if (an && bz) {
            *result = a;
            return 1;
        }
        return 0;
    case M8_ZID_SUB:
        if (an && bz) {
            *result = a;
            return 1;
        }
        if (az && bn) {
            *result = b ^ UINT64_C(0x8000000000000000);
            return 1;
        }
        return 0;
    case M8_ZID_MUL:
        if ((az && (bz || bn)) || (bz && (az || an))) {
            *result = (a ^ b) & UINT64_C(0x8000000000000000);
            return 1;
        }
        return 0;
    case M8_ZID_DIV:
        if (az && bn) {
            *result = (a ^ b) & UINT64_C(0x8000000000000000);
            return 1;
        }
        return 0;
    case M8_ZID_SQRT:
        if (b != 0)
            return 0;
        if (az) {
            *result = a;
            return 1;
        }
        return 0;
    default:
        if (((az && (bz || bn)) || (bz && (az || an))) &&
            m8_zid_is_normal64(d)) {
            *result = d;
            return 1;
        }
        return 0;
    }
}

#endif
