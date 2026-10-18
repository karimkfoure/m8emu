/* Optional native FP path. The caller must qualify RN, FZ and DN at entry.
 * Hardware flags are collected once at the native boundary. On x86, DAZ
 * does not expose ARM IDC, and FTZ detects tiny values after rounding.
 * Aggregated host UFC/IXC cannot distinguish flush-only underflow from an
 * unrelated inexact operation. The exact path in fp.h remains the default.
 */
#ifndef M8_AOT_FAST_FP_H
#define M8_AOT_FAST_FP_H
#include "arm64_fp.h"
#if defined(__GNUC__) || defined(__clang__)
#define M8_FP_NATIVE_INLINE static inline __attribute__((always_inline))
#define M8_FP_NATIVE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define M8_FP_NATIVE_INLINE static inline
#define M8_FP_NATIVE_UNLIKELY(x) (x)
#endif
#if defined(__x86_64__) || defined(__aarch64__)
/* These environments flush each hardware arithmetic lane. Other hosts
 * retain the per-operand software flush path, including in buffer loops. */
#define M8_FP_NATIVE_HARDWARE_FZ 1
#endif
static inline int m8_fp_native_supported(const m8_cpu *cpu) {
    return (cpu->fpscr & 0x03c00000u) == 0x03000000u;
}
static inline void m8_fp_native_configure(m8_cpu *cpu) {
#if defined(__x86_64__)
    unsigned mode=(cpu->fpscr >> 22) & 3u;
    unsigned xmode=mode==1 ? 2 : mode==2 ? 1 : mode;
    unsigned control=(_mm_getcsr() & ~(0xe040u)) | 0x1f80u;
    if (cpu->fpscr & (1u << 24)) control |= 0x8040u;
    _mm_setcsr(control | (xmode << 13));
#elif defined(__aarch64__)
    uint64_t control;
    __asm__ volatile("mrs %0, fpcr" : "=r"(control));
    control=(control & ~(UINT64_C(0x03c00000) | UINT64_C(0x9f00))) |
            (cpu->fpscr & 0x03c00000u);
    __asm__ volatile("msr fpcr, %0" :: "r"(control) : "memory");
#else
    fesetround(m8_fp_rounding(cpu->fpscr >> 22));
#endif
}
static inline m8_fp_env m8_fp_native_enter(m8_cpu *cpu) {
    m8_fp_env env;
    env.cpu=cpu;
#if defined(__x86_64__)
    env.control=_mm_getcsr();
#elif defined(__aarch64__)
    __asm__ volatile("mrs %0, fpcr" : "=r"(env.control));
    __asm__ volatile("mrs %0, fpsr" : "=r"(env.status));
#else
    feholdexcept(&env.saved);
#endif
    m8_fp_native_configure(cpu);
    m8_fp_clear();
    return env;
}
static inline void m8_fp_native_sync(m8_cpu *cpu) {
    /* Read the hardware flags once. Do not clear IXC when UFC is set: the
     * flags can contain a separate inexact operation from the same call. */
    cpu->fpscr |= m8_fp_flags();
}
static inline void m8_fp_native_leave(m8_fp_env env) {
    m8_fp_native_sync(env.cpu);
#if defined(__x86_64__)
    _mm_setcsr(env.control);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" :: "r"(env.control) : "memory");
    __asm__ volatile("msr fpsr, %0" :: "r"(env.status) : "memory");
#else
    fesetenv(&env.saved);
#endif
}
/* Preserve ARM invalid-operation flags on the rare NaN-result path. The
 * compiler can remove an overwritten host operation, and x86 FMA suppresses
 * 0*infinity invalid when the addend is a quiet NaN. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((cold, noinline))
#endif
static uint32_t m8_fp_native_nan32(m8_cpu *cpu, int op,
                                  uint32_t a, uint32_t b, uint32_t d) {
    int invalid=m8_fp_snan32(a) || m8_fp_snan32(b) ||
                (op==M8_FP_FMA && m8_fp_snan32(d));
    uint32_t am=a & 0x7fffffffu, bm=b & 0x7fffffffu, dm=d & 0x7fffffffu;
    if (cpu->fpscr & (1u << 24)) {
        if (am < 0x00800000u) am=0;
        if (bm < 0x00800000u) bm=0;
        if (dm < 0x00800000u) dm=0;
    }
    int ai=am==0x7f800000u, bi=bm==0x7f800000u;
    switch (op) {
    case M8_FP_ADD: invalid |= ai && bi && ((a ^ b) >> 31); break;
    case M8_FP_SUB: invalid |= ai && bi && !((a ^ b) >> 31); break;
    case M8_FP_MUL: invalid |= (ai && !bm) || (bi && !am); break;
    case M8_FP_DIV: invalid |= (!am && !bm) || (ai && bi); break;
    case M8_FP_SQRT: invalid |= (a >> 31) && am && am<=0x7f800000u; break;
    default:
        invalid |= (ai && !bm) || (bi && !am);
        invalid |= (ai || bi) && am<=0x7f800000u && bm<=0x7f800000u &&
                   dm==0x7f800000u && ((a ^ b ^ d) >> 31);
        break;
    }
    if (invalid) cpu->fpscr |= 1;
    return 0x7fc00000u;
}
/* MAXNM/MINNM need explicit NaN and signed-zero rules. Keep rare values
 * outside the common finite-normal comparison, without saving fenv. */
#if defined(__GNUC__) || defined(__clang__)
#define M8_FP_NATIVE_COLD __attribute__((cold, noinline))
#else
#define M8_FP_NATIVE_COLD
#endif
#define M8_FP_NATIVE_MINMAX(BITS, TYPE, UINT, SIGN, EXP, NORMAL, DNAN) \
M8_FP_NATIVE_COLD static UINT m8_fp_native_minmax_special##BITS( \
        m8_cpu *cpu, int op, UINT a, UINT b) { \
    a=m8_fp_input##BITS(cpu,a); b=m8_fp_input##BITS(cpu,b); \
    if (m8_fp_snan##BITS(a) || m8_fp_snan##BITS(b)) \
        return m8_fp_propagate##BITS(cpu,a,b,0); \
    if (m8_fp_nan##BITS(a)) return m8_fp_nan##BITS(b) ? m8_fp_propagate##BITS(cpu,a,b,0) : b; \
    if (m8_fp_nan##BITS(b)) return a; \
    TYPE x=m8_fp_##TYPE(a), y=m8_fp_##TYPE(b); \
    if (!(a & ~SIGN) && !(b & ~SIGN)) return op==M8_FP_MAXNM ? a & b : a | b; \
    return op==M8_FP_MAXNM ? (x>y ? a : b) : (x<y ? a : b); \
} \
M8_FP_NATIVE_INLINE UINT m8_fp_native_minmax##BITS( \
        m8_cpu *cpu, int op, UINT a, UINT b) { \
    UINT am=a & ~SIGN, bm=b & ~SIGN; \
    if (M8_FP_NATIVE_UNLIKELY(((am-NORMAL)>=EXP-NORMAL && am) || \
                              ((bm-NORMAL)>=EXP-NORMAL && bm))) \
        return m8_fp_native_minmax_special##BITS(cpu,op,a,b); \
    if (!(am | bm)) return op==M8_FP_MAXNM ? a & b : a | b; \
    TYPE x=m8_fp_##TYPE(a), y=m8_fp_##TYPE(b); \
    return op==M8_FP_MAXNM ? (x>y ? a : b) : (x<y ? a : b); \
}
M8_FP_NATIVE_MINMAX(32, float, uint32_t, 0x80000000u, 0x7f800000u, 0x00800000u, 0x7fc00000u)
M8_FP_NATIVE_MINMAX(64, double, uint64_t, UINT64_C(0x8000000000000000), UINT64_C(0x7ff0000000000000), UINT64_C(0x0010000000000000), UINT64_C(0x7ff8000000000000))
#undef M8_FP_NATIVE_MINMAX
M8_FP_NATIVE_INLINE uint32_t m8_fp_native32(m8_cpu *cpu, int op,
                                          uint32_t a, uint32_t b, uint32_t d) {
    if (op >= M8_FP_MAXNM) return m8_fp_native_minmax32(cpu,op,a,b);
#ifdef M8_FP_ARM64_HARDWARE
    return m8_fp_bits32(m8_fp_arm64_s(op,m8_fp_float(a),m8_fp_float(b),m8_fp_float(d)));
#else
#if !defined(__x86_64__) && !defined(__aarch64__)
    a=m8_fp_input32(cpu,a); b=m8_fp_input32(cpu,b);
    if (op==M8_FP_FMA) d=m8_fp_input32(cpu,d);
#else
    (void)cpu;
#endif
    float x=m8_fp_float(a), y=m8_fp_float(b), z=m8_fp_float(d), result;
    switch (op) {
    case M8_FP_ADD: result=x+y; break;
    case M8_FP_SUB: result=x-y; break;
    case M8_FP_MUL: result=x*y; break;
    case M8_FP_DIV: result=x/y; break;
    case M8_FP_SQRT: result=sqrtf(x); break;
    default: result=fmaf(x,y,z); break;
    }
    if (M8_FP_NATIVE_UNLIKELY(result != result)) {
        return m8_fp_native_nan32(cpu,op,a,b,d);
    }
    uint32_t bits=m8_fp_bits32(result);
#if !defined(__x86_64__) && !defined(__aarch64__)
    bits=m8_fp_output32(cpu,bits);
#endif
    return bits;
#endif
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((cold, noinline))
#endif
static uint64_t m8_fp_native_nan64(m8_cpu *cpu, int op,
                                  uint64_t a, uint64_t b, uint64_t d) {
    int invalid=m8_fp_snan64(a) || m8_fp_snan64(b) ||
                (op==M8_FP_FMA && m8_fp_snan64(d));
    uint64_t am=a & UINT64_C(0x7fffffffffffffff), bm=b & UINT64_C(0x7fffffffffffffff), dm=d & UINT64_C(0x7fffffffffffffff);
    if (cpu->fpscr & (1u << 24)) {
        if (am < UINT64_C(0x0010000000000000)) am=0;
        if (bm < UINT64_C(0x0010000000000000)) bm=0;
        if (dm < UINT64_C(0x0010000000000000)) dm=0;
    }
    int ai=am==UINT64_C(0x7ff0000000000000), bi=bm==UINT64_C(0x7ff0000000000000);
    switch (op) {
    case M8_FP_ADD: invalid |= ai && bi && ((a ^ b) >> 63); break;
    case M8_FP_SUB: invalid |= ai && bi && !((a ^ b) >> 63); break;
    case M8_FP_MUL: invalid |= (ai && !bm) || (bi && !am); break;
    case M8_FP_DIV: invalid |= (!am && !bm) || (ai && bi); break;
    case M8_FP_SQRT: invalid |= (a >> 63) && am && am<=UINT64_C(0x7ff0000000000000); break;
    default:
        invalid |= (ai && !bm) || (bi && !am);
        invalid |= (ai || bi) && am<=UINT64_C(0x7ff0000000000000) && bm<=UINT64_C(0x7ff0000000000000) &&
                   dm==UINT64_C(0x7ff0000000000000) && ((a ^ b ^ d) >> 63);
        break;
    }
    if (invalid) cpu->fpscr |= 1;
    return UINT64_C(0x7ff8000000000000);
}
M8_FP_NATIVE_INLINE uint64_t m8_fp_native64(m8_cpu *cpu, int op,
                                          uint64_t a, uint64_t b, uint64_t d) {
    if (op >= M8_FP_MAXNM) return m8_fp_native_minmax64(cpu,op,a,b);
#ifdef M8_FP_ARM64_HARDWARE
    return m8_fp_bits64(m8_fp_arm64_d(op,m8_fp_double(a),m8_fp_double(b),m8_fp_double(d)));
#else
#if !defined(__x86_64__) && !defined(__aarch64__)
    a=m8_fp_input64(cpu,a); b=m8_fp_input64(cpu,b);
    if (op==M8_FP_FMA) d=m8_fp_input64(cpu,d);
#endif
    double x=m8_fp_double(a), y=m8_fp_double(b), z=m8_fp_double(d), result;
    switch (op) {
    case M8_FP_ADD: result=x+y; break;
    case M8_FP_SUB: result=x-y; break;
    case M8_FP_MUL: result=x*y; break;
    case M8_FP_DIV: result=x/y; break;
    case M8_FP_SQRT: result=sqrt(x); break;
    default: result=fma(x,y,z); break;
    }
    if (M8_FP_NATIVE_UNLIKELY(result != result))
        return m8_fp_native_nan64(cpu,op,a,b,d);
    uint64_t bits=m8_fp_bits64(result);
#if !defined(__x86_64__) && !defined(__aarch64__)
    bits=m8_fp_output64(cpu,bits);
#endif
    return bits;
#endif
}
M8_FP_NATIVE_INLINE uint32_t m8_fp_native_to_int(m8_cpu *cpu, uint64_t a,
                   int dp, int uns, unsigned mode, unsigned bits, unsigned frac) {
    /* Directed and ties-away conversion opcodes use the complete software
     * rounding helper. The common VCVT round-zero form uses direct casts. */
    if (mode != 3) return m8_fp_to_int(cpu,a,dp,uns,mode,bits,frac);
    double x;
    if (dp) {
        a=m8_fp_input64(cpu,a);
        if (m8_fp_nan64(a)) { cpu->fpscr |= 1; return 0; }
        x=m8_fp_double(a);
    } else {
        uint32_t b=m8_fp_input32(cpu,(uint32_t)a);
        if (m8_fp_nan32(b)) { cpu->fpscr |= 1; return 0; }
        x=(double)m8_fp_float(b);
    }
#ifdef M8_AOT_FLOAT_INT_SCALE
    /* Every finite nonzero binary32 value stays exact and normal in binary64
     * after scaling by 2^1..2^32. Positive powers preserve infinities and
     * signed zero. NaNs have already returned above. */
    if (frac) {
        if (!dp && frac<=32)
            x*=m8_fp_double(((uint64_t)1023+frac)<<52);
        else
            x=ldexp(x,(int)frac);
    }
#else
    if (frac) x=ldexp(x,(int)frac);
#endif
#ifdef M8_AOT_FP_CONSTANT_BOUNDS
    /* These limits are exact normal doubles in every FP rounding mode.
     * Keep every other width on the original library path. */
    double upper=bits==32 ? (uns?0x1p32:0x1p31) : ldexp(1.0,(int)bits-(uns?0:1));
#else
    double upper=ldexp(1.0,(int)bits-(uns?0:1));
#endif
    double lower=uns?0.0:-upper;
    /* A value in (lower-1, lower) truncates to the valid lower endpoint. */
    if (x <= lower-1.0) {
        cpu->fpscr |= 1;
        return uns?0u:(uint32_t)(int32_t)lower;
    }
    if (x >= upper) {
        cpu->fpscr |= 1;
        return uns?(uint32_t)(upper-1.0):(uint32_t)(int32_t)(upper-1.0);
    }
    uint32_t result;
    if (uns) {
        result=(uint32_t)x;
        if ((double)result != x) cpu->fpscr |= 16;
    } else {
        int32_t integer=(int32_t)x;
        result=(uint32_t)integer;
        if ((double)integer != x) cpu->fpscr |= 16;
    }
    return result;
}
M8_FP_NATIVE_INLINE uint64_t m8_fp_native_from_int(m8_cpu *cpu, uint32_t a,
                      int dp, int uns, unsigned bits, unsigned frac, int nearest) {
    if (M8_FP_NATIVE_UNLIKELY(nearest && (cpu->fpscr & (3u << 22))))
        return m8_fp_from_int(cpu,a,dp,uns,bits,frac,nearest);
#ifdef M8_AOT_INT_FLOAT_SCALE
    if (!dp && frac<=32) {
        /* All 16/32-bit integers round to finite F32. Scaling by 2^-frac
         * keeps every nonzero result normal and is exact, so rounding the
         * integer before scaling equals rounding its exact scaled value.
         * Only the integer cast can add IXC. Keep the nearest-mode fallback
         * above: fixed VCVT uses RN even when FPSCR requests another mode. */
        float value;
        if (bits==16) value=uns?(float)(uint16_t)a:(float)(int16_t)a;
        else value=uns?(float)a:(float)(int32_t)a;
        float scale=m8_fp_float((127u-frac)<<23);
        return m8_fp_bits32(value*scale);
    }
#endif
    double x;
    if (bits==16) x=uns?(double)(uint16_t)a:(double)(int16_t)a;
    else x=uns?(double)a:(double)(int32_t)a;
    if (frac) x=ldexp(x,-(int)frac);
    if (dp) return m8_fp_bits64(x);
    return m8_fp_bits32((float)x);
}
M8_FP_NATIVE_INLINE uint64_t m8_fp_native_convert(m8_cpu *cpu, uint64_t a,
                                                int from_dp) {
    if (from_dp) {
        a=m8_fp_input64(cpu,a);
        if (m8_fp_nan64(a)) {
            if (m8_fp_snan64(a)) cpu->fpscr |= 1;
            return 0x7fc00000u;
        }
        return m8_fp_bits32((float)m8_fp_double(a));
    }
    uint32_t b=m8_fp_input32(cpu,(uint32_t)a);
    if (m8_fp_nan32(b)) {
        if (m8_fp_snan32(b)) cpu->fpscr |= 1;
        return UINT64_C(0x7ff8000000000000);
    }
    return m8_fp_bits64((double)m8_fp_float(b));
}
/* Keep existing generated code and harnesses on one public helper ABI. */
#define m8_fp_enter m8_fp_native_enter
#define m8_fp_leave m8_fp_native_leave
#define m8_fp_sync m8_fp_native_sync
#define m8_fp_configure m8_fp_native_configure
#define m8_fp_eval32 m8_fp_native32
#define m8_fp_op64 m8_fp_native64
#define m8_fp_to_int m8_fp_native_to_int
#define m8_fp_from_int m8_fp_native_from_int
#define m8_fp_convert m8_fp_native_convert
#undef M8_FP_NATIVE_COLD
#undef M8_FP_NATIVE_INLINE
#undef M8_FP_NATIVE_UNLIKELY
#endif
