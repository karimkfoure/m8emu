/* Private, default-off native FMA for bounded FM-local arithmetic. */
#ifndef M8_AOT_FM_FMA_NATIVE_H
#define M8_AOT_FM_FMA_NATIVE_H
#include "fp.h"
#include <float.h>
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128 || \
    DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024
#error "The native FM FMA requires IEEE binary32 and binary64"
#endif
#ifdef __FAST_MATH__
#error "The native FM FMA requires strict FP arithmetic"
#endif
#define M8_FMNF_INLINE static inline __attribute__((always_inline))
typedef struct m8_fmnf_context {uint64_t status;} m8_fmnf_context;
#if defined(M8_AOT_FM_NATIVE_FMA) && !defined(__wasm__) && !defined(M8_WEB_FP_TEST) && \
    (defined(__x86_64__) || defined(__aarch64__))
#define M8_FMNF_ENABLED 1
#if defined(__x86_64__)
/* The generic caller tests CPU support before this targeted function runs.
 * This does not require the rest of the binary to use the FMA ISA. */
#define M8_FMNF_TARGET __attribute__((target("fma")))
#else
/* Scalar FMA is part of the supported ARM64 FP instruction set. */
#define M8_FMNF_TARGET
#endif
M8_FMNF_INLINE M8_FMNF_TARGET int m8_fmnf_eval_local(const m8_fmnf_context *context,
        float a,float b,float d,float *result) {
    uint32_t am=m8_fp_bits32(a)&0x7fffffffu,bm=m8_fp_bits32(b)&0x7fffffffu,
        dm=m8_fp_bits32(d)&0x7fffffffu;
    if((am && am-0x00800000u>0x7effffffu) ||
       (bm && bm-0x00800000u>0x7effffffu) ||
       (dm && dm-0x00800000u>0x7effffffu))return 0;
    float value=__builtin_fmaf(a,b,d);
    uint32_t magnitude=m8_fp_bits32(value)&0x7fffffffu;
    /* Tiny results, the minimum-normal boundary and overflow need complete
     * original flag handling. A flushed zero can be a nonzero exact sum. */
    if(magnitude>=0x7f800000u || (magnitude && magnitude<=0x00800000u))goto decline;
    if(!magnitude) {
        /* The binary32 product is exact in binary64. Its exact sum with a
         * binary32 addend is zero iff this binary64 sum is zero. Nonzero
         * sums are at least 2^-298, so binary64 cannot round them to zero.
         * This slow check runs only for a zero binary32 result. */
        double sum=(double)a*(double)b+(double)d;
        if(sum!=0.0)goto decline;
    }
    *result=value;return 1;
decline:
#if defined(M8_NATIVE_FP_NO_STICKY)
    (void)context;
#elif defined(__x86_64__)
    _mm_setcsr((unsigned)context->status);
#else
    __asm__ volatile("msr fpsr, %0" :: "r"(context->status) : "memory");
#endif
    return 0;
}
/* A generic x86 caller can use this one bound call. A whole-math caller
 * compiled with the matching target can inline eval_local instead. Both
 * routes execute the same arithmetic body, not a library FMA call. */
static __attribute__((noinline)) M8_FMNF_TARGET int m8_fmnf_call_prepared(
        const m8_fmnf_context *context,float a,float b,float d,float *result) {
    return m8_fmnf_eval_local(context,a,b,d,result);
}
#endif
M8_FMNF_INLINE int m8_fmnf_available(void) {
#if defined(M8_FMNF_ENABLED) && defined(__x86_64__)
    return __builtin_cpu_supports("avx") && __builtin_cpu_supports("fma");
#elif defined(M8_FMNF_ENABLED) && defined(__aarch64__)
    return 1;
#else
    return 0;
#endif
}
/* Entry and result checks publish no CPU, guest RAM or private SoftFloat
 * state. Only result changes on success. On a decline, restore every saved
 * host status bit before the caller executes its original fallback. Keep
 * arithmetic in a separate function so it cannot precede host admission.
 * Compile with -frounding-math, -ffp-contract=off and -fno-math-errno. */
M8_FMNF_INLINE int m8_fmnf_prepare(uint32_t fpscr,m8_fmnf_context *context) {
#if defined(M8_FMNF_ENABLED)
    if((fpscr&0x03c00010u)!=0x03000010u || !m8_fmnf_available())return 0;
#if defined(M8_NATIVE_FP_NO_STICKY)
    context->status=0;return 1;
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))return 0;
#else
    uint64_t mode,saved;__asm__ volatile("mrs %0, fpcr" : "=r"(mode));
    __asm__ volatile("mrs %0, fpsr" : "=r"(saved));
    if((mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))return 0;
#endif
    context->status=saved;return 1;
#else
    (void)fpscr;(void)context;return 0;
#endif
}
/* prepare runs once for one uninterrupted, owned whole-math operation.
 * All intervening admitted operations can add only already-sticky IXC.
 * A caller must not reuse its context across services or FP-mode changes.
 * This convenience wrapper is for isolated admission/no-effect proofs. */
M8_FMNF_INLINE int m8_fmnf_try(uint32_t fpscr,float a,float b,float d,float *result) {
#if defined(M8_FMNF_ENABLED)
    m8_fmnf_context context;
    return m8_fmnf_prepare(fpscr,&context) && m8_fmnf_call_prepared(&context,a,b,d,result);
#else
    (void)fpscr;(void)a;(void)b;(void)d;(void)result;return 0;
#endif
}
#undef M8_FMNF_TARGET
#undef M8_FMNF_ENABLED
#undef M8_FMNF_INLINE
#endif
