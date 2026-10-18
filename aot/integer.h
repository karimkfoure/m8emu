/* Portable ARMv7E-M integer helpers. Arithmetic uses unsigned storage. */
#ifndef M8_AOT_INTEGER_H
#define M8_AOT_INTEGER_H
#include <stdint.h>
#include <string.h>
#include "m8_aot.h"
#if defined(__GNUC__) || defined(__clang__)
#define M8_SHIFT_INLINE static inline __attribute__((always_inline))
#else
#define M8_SHIFT_INLINE static inline
#endif
/* Region direct-call builds inline the hot flag helpers at every guest site
 * instead of sharing one out-of-line copy. The region generator enables this
 * only with --direct-calls (default off); all other users of this header keep
 * shared out-of-line copies. */
#ifndef M8_REGION_INLINE
#if defined(M8_NATIVE_REGION_DIRECT_CALLS) && M8_NATIVE_REGION_DIRECT_CALLS
#define M8_REGION_INLINE static inline __attribute__((always_inline))
#else
#define M8_REGION_INLINE static inline
#endif
#endif

/* Copy the guest bit pattern into the matching signed type. This lets the
 * compiler use sign-extension instructions without an out-of-range cast. */
static inline int32_t m8_i32(uint32_t x) {
    int32_t value;
    memcpy(&value, &x, sizeof(value));
    return value;
}
static inline int64_t m8_s32(uint32_t x) {
    return m8_i32(x);
}
static inline int64_t m8_signed(uint32_t x, unsigned bits) {
#if (-1 >> 1) == -1
    /* All supported hosts use arithmetic signed shifts. Keep the portable
     * subtraction below for a compiler with another signed-shift rule.
     * bits is in [1,32]; moving the sign bit first discards upper bits. */
    unsigned shift = 32u - bits;
    return m8_i32(x << shift) >> shift;
#else
    uint32_t sign = UINT32_C(1) << (bits - 1);
    return (int64_t)(x & (sign - 1)) - (int64_t)(x & sign);
#endif
}
M8_SHIFT_INLINE uint32_t m8_asr(uint32_t x, unsigned n) {
#if (-1 >> 1) == -1
    /* ARM shifts by 32 or more replicate bit 31. Clamp before the C shift
     * so every count is defined, including zero and counts above 255. */
    return (uint32_t)(m8_i32(x) >> (n < 32u ? n : 31u));
#else
    if (!n) return x;
    if (n >= 32) return (x >> 31) ? UINT32_MAX : 0;
    return (x >> n) | ((x >> 31) ? (UINT32_MAX << (32 - n)) : 0);
#endif
}
/* Shift identifiers match Capstone 5 ARM_SFT_*. Register counts use bits 7:0. */
M8_SHIFT_INLINE uint32_t m8_shift(uint32_t x, unsigned type, unsigned n, uint32_t *c) {
    uint32_t ignored = 0; if (!c) c = &ignored;
    if (type >= 6) { type -= 5; n &= 255; }
    if (!type || (!n && type != 5)) return x;
    switch (type) {
    case 1: *c = (x >> (n >= 32 ? 31 : n - 1)) & 1; return m8_asr(x, n);
    case 2:
        *c = n <= 32 ? ((x >> (32 - n)) & 1) : 0;
        return n < 32 ? x << n : 0;
    case 3:
        *c = n <= 32 ? ((x >> (n - 1)) & 1) : 0;
        return n < 32 ? x >> n : 0;
    case 4:
        n &= 31;
        x = n ? (x >> n) | (x << (32 - n)) : x;
        *c = x >> 31; return x;
    case 5: { uint32_t result = (*c << 31) | (x >> 1); *c = x & 1; return result; }
    default: return x;
    }
}
static inline uint32_t m8_ror(uint32_t x, unsigned n) {
    n &= 31; return n ? (x >> n) | (x << (32 - n)) : x;
}
M8_REGION_INLINE void m8_nz(m8_cpu *cpu, uint32_t x) {
    cpu->xpsr = (cpu->xpsr & ~UINT32_C(0xc0000000)) | (x & UINT32_C(0x80000000)) | (x ? 0 : UINT32_C(0x40000000));
}
M8_REGION_INLINE void m8_logic_flags(m8_cpu *cpu, uint32_t x, uint32_t carry) {
    m8_nz(cpu,x); cpu->xpsr = (cpu->xpsr & ~UINT32_C(0x20000000)) | (carry << 29);
}
M8_REGION_INLINE uint32_t m8_add_flags(m8_cpu *cpu, uint32_t a, uint32_t b, uint32_t carry) {
    uint64_t sum = (uint64_t)a + b + carry;
    uint32_t r = (uint32_t)sum;
    m8_nz(cpu,r);
    cpu->xpsr = (cpu->xpsr & ~UINT32_C(0x30000000)) | ((uint32_t)(sum >> 32) << 29) | ((~(a ^ b) & (a ^ r) & UINT32_C(0x80000000)) >> 3);
    return r;
}
M8_REGION_INLINE uint32_t m8_sub_flags(m8_cpu *cpu, uint32_t a, uint32_t b, uint32_t borrow) {
    uint64_t sub = (uint64_t)b + borrow;
    uint32_t r = a - b - borrow;
    m8_nz(cpu,r);
    cpu->xpsr = (cpu->xpsr & ~UINT32_C(0x30000000)) | ((uint32_t)((uint64_t)a >= sub) << 29) | (((a ^ b) & (a ^ r) & UINT32_C(0x80000000)) >> 3);
    return r;
}
M8_REGION_INLINE int m8_condition(uint32_t p, unsigned cc) {
    unsigned n = p >> 31, z = (p >> 30) & 1, c = (p >> 29) & 1, v = (p >> 28) & 1;
    switch (cc) {
    case 1: return z; case 2: return !z; case 3: return c; case 4: return !c;
    case 5: return n; case 6: return !n; case 7: return v; case 8: return !v;
    case 9: return c && !z; case 10: return !c || z; case 11: return n == v;
    case 12: return n != v; case 13: return !z && n == v; case 14: return z || n != v;
    default: return 1;
    }
}
M8_REGION_INLINE uint32_t m8_sat_signed(m8_cpu *cpu, int64_t value, unsigned bits) {
    int64_t limit = INT64_C(1) << (bits - 1);
    if (value < -limit) { cpu->xpsr |= UINT32_C(0x08000000); value = -limit; }
    if (value >= limit) { cpu->xpsr |= UINT32_C(0x08000000); value = limit - 1; }
    return (uint32_t)value;
}
M8_REGION_INLINE uint32_t m8_sat_unsigned(m8_cpu *cpu, int64_t value, unsigned bits) {
    int64_t limit = (INT64_C(1) << bits) - 1;
    if (value < 0) { cpu->xpsr |= UINT32_C(0x08000000); value = 0; }
    if (value > limit) { cpu->xpsr |= UINT32_C(0x08000000); value = limit; }
    return (uint32_t)value;
}
M8_REGION_INLINE uint32_t m8_qresult(m8_cpu *cpu, int64_t value) {
    if (value < -INT64_C(0x80000000) || value > INT64_C(0x7fffffff)) cpu->xpsr |= UINT32_C(0x08000000);
    return (uint32_t)value;
}
M8_REGION_INLINE uint32_t m8_clz(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return x ? (uint32_t)__builtin_clz(x) : 32u;
#else
    unsigned n = 0; if (!x) return 32;
    while (!(x & UINT32_C(0x80000000))) { x <<= 1; ++n; } return n;
#endif
}
M8_REGION_INLINE uint32_t m8_rbit(uint32_t x) {
    x = ((x >> 1) & UINT32_C(0x55555555)) | ((x & UINT32_C(0x55555555)) << 1);
    x = ((x >> 2) & UINT32_C(0x33333333)) | ((x & UINT32_C(0x33333333)) << 2);
    x = ((x >> 4) & UINT32_C(0x0f0f0f0f)) | ((x & UINT32_C(0x0f0f0f0f)) << 4);
    x = ((x >> 8) & UINT32_C(0x00ff00ff)) | ((x & UINT32_C(0x00ff00ff)) << 8);
    return (x >> 16) | (x << 16);
}
/* mode: signed/unsigned normal=0/1, saturated=2/3, halved=4/5.
 * kind: add=0, sub=1, ASX=2, SAX=3. Packed saturation does not set Q. */
static inline uint32_t m8_parallel(m8_cpu *cpu, uint32_t a, uint32_t b, unsigned bits, unsigned kind, unsigned mode) {
    uint32_t mask = (UINT32_C(1) << bits) - 1, r = 0, ge = 0;
    for (unsigned lane=0; lane<32/bits; ++lane) {
        unsigned j = kind >= 2 ? (lane ^ 1) : lane;
        uint32_t av = (a >> (lane * bits)) & mask, bv = (b >> (j * bits)) & mask;
        int sub = kind == 1 || (kind == 2 && lane == 0) || (kind == 3 && lane == 1);
        int64_t aa = mode & 1 ? av : m8_signed(av,bits), bb = mode & 1 ? bv : m8_signed(bv,bits);
        int64_t value = sub ? aa - bb : aa + bb;
        if (mode < 2) {
            int flag = mode == 0 ? value >= 0 : (sub ? value >= 0 : value > mask);
            if (flag) ge |= ((bits == 16 ? 3u : 1u) << (lane * (bits / 8)));
        } else if (mode < 4) {
            int64_t lo = mode & 1 ? 0 : -(INT64_C(1) << (bits - 1));
            int64_t hi = mode & 1 ? mask : (INT64_C(1) << (bits - 1)) - 1;
            if (value < lo) value = lo;
            if (value > hi) value = hi;
        } else {
            /* Unsigned subtraction keeps the borrow bit for halving. */
            uint64_t wide_mask = (UINT64_C(1) << (bits + 1)) - 1;
            value = (int64_t)(((uint64_t)value & wide_mask) >> 1);
        }
        r |= ((uint32_t)value & mask) << (lane * bits);
    }
    if (mode < 2) cpu->xpsr = (cpu->xpsr & ~UINT32_C(0x000f0000)) | (ge << 16);
    return r;
}
M8_REGION_INLINE uint32_t m8_sel(uint32_t xpsr, uint32_t a, uint32_t b) {
    uint32_t mask = 0;
    for (unsigned i = 0; i < 4; ++i) if (xpsr & (UINT32_C(1) << (16 + i))) mask |= UINT32_C(255) << (8*i);
    return (a & mask) | (b & ~mask);
}
M8_REGION_INLINE uint32_t m8_usad8(uint32_t a, uint32_t b) {
    uint32_t sum=0;
    for (unsigned i=0;i<4;++i) { int x=(a>>(8*i))&255, y=(b>>(8*i))&255; sum+=(uint32_t)(x>y?x-y:y-x); }
    return sum;
}
#undef M8_SHIFT_INLINE
#endif
