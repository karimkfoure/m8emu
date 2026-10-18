/* Default-off FM-local FMA. Compile with -ffp-contract=off, never fast-math. */
#ifndef M8_AOT_FM_FMA_H
#define M8_AOT_FM_FMA_H
#include "fp.h"
#include <float.h>
#include <math.h>
#include <string.h>
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128 || \
    DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024
#error "The FM FMA candidate requires IEEE binary32 and binary64"
#endif
#ifdef __FAST_MATH__
#error "The FM FMA candidate requires strict IEEE arithmetic"
#endif
#if defined(__GNUC__) || defined(__clang__)
#define M8_FM_FMA_INLINE static inline __attribute__((always_inline))
#else
#define M8_FM_FMA_INLINE static inline
#endif

/* The caller must establish host round-to-nearest, ties-to-even. Wasm scalar
 * arithmetic has that mode. Native FM callers already own the FP boundary.
 * This is the existing fp_fast_wasm.h FMA proof applied to float locals:
 * a binary32 product is exact in binary64. The binary64 addition cannot
 * cross a binary32 rounding boundary unless its result is exactly a
 * binary32 midpoint. Reject that case instead of assuming double rounding
 * is universally equivalent to FMA. Nonzero exact sums are at least 2^-298,
 * so the binary64 operations cannot underflow. Reject binary32 tiny results,
 * the minimum-normal boundary and overflow before publishing a result.
 * Zero signs follow the same multiply/add rules under nearest rounding.
 * No guest state or private SoftFloat state changes on either outcome. */
M8_FM_FMA_INLINE int m8_fm_fma_try_rn(float a,float b,float d,float *result) {
#if defined(M8_FM_EXACT_FMA)
    uint32_t aa,bb,dd;
    memcpy(&aa,&a,4);memcpy(&bb,&b,4);memcpy(&dd,&d,4);
    uint32_t am=aa&0x7fffffffu,bm=bb&0x7fffffffu,dm=dd&0x7fffffffu;
    if((am && am-0x00800000u>0x7effffffu) ||
       (bm && bm-0x00800000u>0x7effffffu) ||
       (dm && dm-0x00800000u>0x7effffffu)) return 0;
    double sum=(double)a*(double)b+(double)d;
    uint64_t wide;memcpy(&wide,&sum,8);
    if((wide&UINT64_C(0x1fffffff))==UINT64_C(0x10000000)) return 0;
    float value=(float)sum;
    uint32_t bits;memcpy(&bits,&value,4);
    uint32_t magnitude=bits&0x7fffffffu;
    if(magnitude>=0x7f800000u || (magnitude && magnitude<=0x00800000u) ||
       (!magnitude && sum!=0.0)) return 0;
    *result=value;return 1;
#else
    (void)a;(void)b;(void)d;(void)result;return 0;
#endif
}

/* Raw FM arithmetic already has an admitted RN boundary. Its fallback is
 * libc fmaf, which preserves the private SoftFloat globals. */
M8_FM_FMA_INLINE float m8_fm_fma_raw_rn(float a,float b,float d) {
    float result;
    if(m8_fm_fma_try_rn(a,b,d,&result)) return result;
    return fmaf(a,b,d);
}

/* An exact guest operation owns a different fallback contract. Keep the
 * original helper for rejected modes/values, including its private-context
 * effects. Sticky IXC is required because the fast path publishes no flags.
 * FZ and DN can vary: admitted inputs/results need neither fixup. */
M8_FM_FMA_INLINE uint32_t m8_fm_fma_exact32(m8_cpu *cpu,
        uint32_t a,uint32_t b,uint32_t d) {
    if((cpu->fpscr&0x00c00010u)==16u) {
        float result;
        if(m8_fm_fma_try_rn(m8_fp_float(a),m8_fp_float(b),m8_fp_float(d),&result))
            return m8_fp_bits32(result);
    }
    return m8_fp_eval32(cpu,M8_FP_FMA,a,b,d);
}
#undef M8_FM_FMA_INLINE
#endif
