/* Portable FPv5-D16 support. Raw register bits remain the ABI boundary. */
#ifndef M8_AOT_FP_H
#define M8_AOT_FP_H
#include "m8_aot.h"
#include "fp_zero_identity.h"
#include <fenv.h>
#include <math.h>
#include <string.h>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif
/* GCC requires -frounding-math; do not compile with -ffast-math. */
typedef struct m8_fp_env {
    fenv_t saved;
    m8_cpu *cpu;
#if defined(__x86_64__) || defined(__i386__)
    unsigned control;
#elif defined(__aarch64__)
    uint64_t control;
    uint64_t status;
#endif
} m8_fp_env;
static inline unsigned m8_fp_flags(void) {
#if defined(__x86_64__) || defined(__i386__)
    unsigned f = _mm_getcsr();
    return (f & 1) | ((f >> 1) & 2) |
           ((f >> 1) & 4) | ((f >> 1) & 8) | ((f >> 1) & 16);
#elif defined(__aarch64__)
    uint64_t f;
    __asm__ volatile("mrs %0, fpsr" : "=r"(f));
    return (unsigned)f & 0x9f;
#else
    int f = fetestexcept(FE_ALL_EXCEPT);
    return ((f & FE_INVALID) ? 1 : 0) | ((f & FE_DIVBYZERO) ? 2 : 0) |
           ((f & FE_OVERFLOW) ? 4 : 0) | ((f & FE_UNDERFLOW) ? 8 : 0) |
           ((f & FE_INEXACT) ? 16 : 0);
#endif
}
static inline void m8_fp_clear(void) {
#if defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(_mm_getcsr() & ~63u);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpsr, xzr" ::: "memory");
#else
    feclearexcept(FE_ALL_EXCEPT);
#endif
}
static inline int m8_fp_rounding(unsigned mode) {
    const int modes[] = { FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO };
    return modes[mode & 3];
}
static inline void m8_fp_configure(m8_cpu *cpu) {
    fesetround(m8_fp_rounding(cpu->fpscr >> 22));
#if defined(__x86_64__) || defined(__i386__)
    unsigned c = _mm_getcsr() & ~0x8040u;
    if (cpu->fpscr & (1u << 24)) c |= 0x8040u;
    _mm_setcsr(c);
#elif defined(__aarch64__)
    uint64_t c;
    __asm__ volatile("mrs %0, fpcr" : "=r"(c));
    c = (c & ~((UINT64_C(3) << 22) | (UINT64_C(3) << 24))) |
        (cpu->fpscr & ((3u << 22) | (3u << 24)));
    __asm__ volatile("msr fpcr, %0" :: "r"(c) : "memory");
#endif
}
static inline m8_fp_env m8_fp_enter(m8_cpu *cpu) {
    m8_fp_env e;
    e.cpu = cpu;
    fegetenv(&e.saved);
#if defined(__x86_64__) || defined(__i386__)
    e.control = _mm_getcsr();
#elif defined(__aarch64__)
    __asm__ volatile("mrs %0, fpcr" : "=r"(e.control));
#endif
    /* Disable host exception traps while executing guest operations. */
    feholdexcept(&e.saved);
    m8_fp_configure(cpu);
    m8_fp_clear();
    return e;
}
static inline void m8_fp_sync(m8_cpu *cpu) {
    unsigned f = m8_fp_flags();
    /* ARM flushes a tiny result before rounding, with UFC but without IXC. */
    if ((cpu->fpscr & (1u << 24)) && (f & 8)) f &= ~16u;
    cpu->fpscr |= f;
}
static inline void m8_fp_leave(m8_fp_env e) {
    m8_fp_sync(e.cpu);
    fesetenv(&e.saved);
#if defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(e.control);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" :: "r"(e.control) : "memory");
#endif
}
static inline void m8_fp_begin(m8_cpu *cpu) {
    /* Until IXC becomes sticky, each operation must distinguish flush-only
     * underflow from a separate inexact operation. Once IXC is set, defer
     * collection until VMRS or function exit. */
    if (!(cpu->fpscr & 16)) m8_fp_clear();
}
static inline void m8_fp_end(m8_cpu *cpu) {
    if (!(cpu->fpscr & 16)) m8_fp_sync(cpu);
}
static inline float m8_fp_float(uint32_t b) { float x; memcpy(&x,&b,4); return x; }
static inline double m8_fp_double(uint64_t b) { double x; memcpy(&x,&b,8); return x; }
static inline uint32_t m8_fp_bits32(float x) { uint32_t b; memcpy(&b,&x,4); return b; }
static inline uint64_t m8_fp_bits64(double x) { uint64_t b; memcpy(&b,&x,8); return b; }
static inline uint64_t m8_fp_get64(m8_cpu *c, unsigned d) {
    return (uint64_t)c->s[d*2] | ((uint64_t)c->s[d*2+1] << 32);
}
static inline void m8_fp_set64(m8_cpu *c, unsigned d, uint64_t v) {
    c->s[d*2] = (uint32_t)v; c->s[d*2+1] = (uint32_t)(v >> 32);
}
/* Encoded operation numbers are private to emit_fp.py. */
enum { M8_FP_ADD, M8_FP_SUB, M8_FP_MUL, M8_FP_DIV, M8_FP_SQRT,
       M8_FP_FMA, M8_FP_MAXNM, M8_FP_MINNM };
/* Same region direct-call contract as integer.h. The region generator enables
 * this only with --direct-calls (default off); all other users of this header
 * keep shared out-of-line copies. */
#ifndef M8_REGION_INLINE
#if defined(M8_NATIVE_REGION_DIRECT_CALLS) && M8_NATIVE_REGION_DIRECT_CALLS
#define M8_REGION_INLINE static inline __attribute__((always_inline))
#else
#define M8_REGION_INLINE static inline
#endif
#endif
/* Default-off exact zero-identity prefix (aot/fp_zero_identity.h). On admit
 * the result is exact in every rounding/flush mode with no flag effects, so
 * the full helper is skipped without touching FPSCR. */
#ifdef M8_FP_ZERO_IDENTITY
#define M8_FP_ZERO_IDENTITY_TRY(BITS, UINT, OP, A, B, D) \
    { UINT zi_result; \
      if (m8_fp_zero_identity##BITS((OP), (A), (B), (D), &zi_result)) \
          return zi_result; }
#else
#define M8_FP_ZERO_IDENTITY_TRY(BITS, UINT, OP, A, B, D)
#endif
#if defined(__GNUC__) || defined(__clang__)
#define M8_FP_TINY static __attribute__((noinline))
#else
#define M8_FP_TINY static
#endif
/* Recompute the boundary result after changing the host rounding mode.
 * A volatile result alone does not prevent GCC from reusing the preceding
 * RN expression, even with -frounding-math. Keep this rare probe separate
 * and load volatile operands so it must perform a second operation. */
#define M8_FP_DEFINE(BITS, TYPE, UINT, SIGN, EXP, FRAC, QUIET, DNAN, SQRT, FMA) \
M8_REGION_INLINE int m8_fp_nan##BITS(UINT a) { return (a & EXP) == EXP && (a & FRAC); } \
M8_REGION_INLINE int m8_fp_snan##BITS(UINT a) { return m8_fp_nan##BITS(a) && !(a & QUIET); } \
M8_REGION_INLINE UINT m8_fp_input##BITS(m8_cpu *c, UINT a) { \
    if ((c->fpscr & (1u << 24)) && !(a & EXP) && (a & FRAC)) { c->fpscr |= 128; return a & SIGN; } \
    return a; \
} \
M8_REGION_INLINE UINT m8_fp_output##BITS(m8_cpu *c, UINT a) { \
    if (m8_fp_nan##BITS(a)) return (c->fpscr & (1u << 25)) ? DNAN : a | QUIET; \
    if ((c->fpscr & (1u << 24)) && !(a & EXP) && (a & FRAC)) { c->fpscr |= 8; return a & SIGN; } \
    return a; \
} \
static inline UINT m8_fp_propagate##BITS(m8_cpu *c, UINT a, UINT b, UINT d) { \
    UINT v = m8_fp_snan##BITS(d) ? d : m8_fp_snan##BITS(a) ? a : \
             m8_fp_snan##BITS(b) ? b : m8_fp_nan##BITS(d) ? d : \
             m8_fp_nan##BITS(a) ? a : b; \
    if (m8_fp_snan##BITS(a) || m8_fp_snan##BITS(b) || m8_fp_snan##BITS(d)) c->fpscr |= 1; \
    return m8_fp_output##BITS(c, v | QUIET); \
} \
M8_FP_TINY int m8_fp_tiny##BITS(int op, TYPE x, TYPE y, TYPE z) { \
    fenv_t env; fegetenv(&env); m8_cpu mode = {0}; mode.fpscr = 3u << 22; \
    m8_fp_configure(&mode); m8_fp_clear(); volatile TYPE v; \
    volatile TYPE vx=x, vy=y, vz=z; \
    switch (op) { \
    case M8_FP_ADD: v=vx+vy;break;case M8_FP_SUB:v=vx-vy;break; \
    case M8_FP_MUL:v=vx*vy;break;case M8_FP_DIV:v=vx/vy;break; \
    case M8_FP_SQRT:v=SQRT(vx);break;case M8_FP_FMA:v=FMA(vx,vy,vz);break; \
    default:v=vx;break; \
    } \
    UINT bits=m8_fp_bits##BITS(v);fesetenv(&env);return !(bits & EXP); \
} \
static inline UINT m8_fp_op##BITS(m8_cpu *c, int op, UINT aa, UINT bb, UINT dd) { \
    M8_FP_ZERO_IDENTITY_TRY(BITS, UINT, op, aa, bb, dd) \
    UINT a = m8_fp_input##BITS(c, aa), b = m8_fp_input##BITS(c, bb), d = m8_fp_input##BITS(c, dd); \
    if (op == M8_FP_FMA && !m8_fp_snan##BITS(d) && (((a & ~SIGN) == EXP && !(b & ~SIGN)) || \
        ((b & ~SIGN) == EXP && !(a & ~SIGN)))) { c->fpscr |= 1; return DNAN; } \
    if (m8_fp_nan##BITS(a) || m8_fp_nan##BITS(b) || (op == M8_FP_FMA && m8_fp_nan##BITS(d))) { \
        if ((op == M8_FP_MAXNM || op == M8_FP_MINNM) && !m8_fp_snan##BITS(a) && !m8_fp_snan##BITS(b)) { \
            if (!m8_fp_nan##BITS(a)) { return a; } if (!m8_fp_nan##BITS(b)) { return b; } \
        } \
        return m8_fp_propagate##BITS(c, a, b, op == M8_FP_FMA ? d : 0); \
    } \
    TYPE x = m8_fp_##TYPE(a), y = m8_fp_##TYPE(b), z = m8_fp_##TYPE(d); \
    volatile TYPE v; \
    m8_fp_begin(c); \
    switch (op) { \
    case M8_FP_ADD: v = x + y; break; case M8_FP_SUB: v = x - y; break; \
    case M8_FP_MUL: v = x * y; break; case M8_FP_DIV: v = x / y; break; \
    case M8_FP_SQRT: v = SQRT(x); break; case M8_FP_FMA: v = FMA(x,y,z); break; \
    case M8_FP_MAXNM: v = x > y ? x : x < y ? y : m8_fp_##TYPE(a & b); break; \
    default: v = x < y ? x : x > y ? y : m8_fp_##TYPE(a | b); break; \
    } \
    UINT r = m8_fp_bits##BITS(v); \
    if (op <= M8_FP_FMA && (r & ~SIGN) == (QUIET << 1) && m8_fp_tiny##BITS(op,x,y,z)) { \
        c->fpscr |= 8; \
        if (c->fpscr & (1u<<24)) { c->fpscr |= m8_fp_flags() & ~16u;m8_fp_clear();return r & SIGN; } \
    } \
    m8_fp_end(c); return m8_fp_nan##BITS(r) ? DNAN : m8_fp_output##BITS(c,r); \
} \
M8_REGION_INLINE void m8_fp_cmp##BITS(m8_cpu *c, UINT aa, UINT bb, int signaling) { \
    UINT a = m8_fp_input##BITS(c,aa), b = m8_fp_input##BITS(c,bb); uint32_t flags; \
    if (m8_fp_nan##BITS(a) || m8_fp_nan##BITS(b)) { \
        flags = 0x30000000u; if (signaling || m8_fp_snan##BITS(a) || m8_fp_snan##BITS(b)) c->fpscr |= 1; \
    } else { TYPE x=m8_fp_##TYPE(a),y=m8_fp_##TYPE(b); flags=x==y?0x60000000u:x<y?0x80000000u:0x20000000u; } \
    c->fpscr=(c->fpscr & 0x0fffffffu)|flags; \
}
M8_FP_DEFINE(32, float, uint32_t, 0x80000000u, 0x7f800000u, 0x007fffffu, 0x00400000u, 0x7fc00000u, sqrtf, fmaf)
M8_FP_DEFINE(64, double, uint64_t, UINT64_C(0x8000000000000000), UINT64_C(0x7ff0000000000000), UINT64_C(0xfffffffffffff), UINT64_C(0x8000000000000), UINT64_C(0x7ff8000000000000), sqrt, fma)
#undef M8_FP_DEFINE
#undef M8_FP_TINY
/* Common firmware mode: finite normal/zero inputs, RN, FZ, DN and sticky
 * IXC. Keep rare values in the complete helper and keep each FP evaluation
 * volatile so an overwritten guest result still raises cumulative flags. */
#if defined(__GNUC__) || defined(__clang__)
#define M8_FP_EVAL_INLINE static inline __attribute__((always_inline))
#else
#define M8_FP_EVAL_INLINE static inline
#endif
static inline int m8_fp_common32(uint32_t a) {
    uint32_t mag=a & 0x7fffffffu;
    return mag<0x7f800000u && (mag==0 || mag>=0x00800000u);
}
M8_FP_EVAL_INLINE uint32_t m8_fp_eval32(m8_cpu *c,int op,uint32_t a,uint32_t b,uint32_t d) {
#ifdef M8_AOT_COMMON_FP
    if(op<=M8_FP_FMA && (c->fpscr & 0x03c00010u)==0x03000010u &&
       m8_fp_common32(a) && m8_fp_common32(b) && (op!=M8_FP_FMA || m8_fp_common32(d))) {
        float x=m8_fp_float(a),y=m8_fp_float(b),z=m8_fp_float(d); volatile float v;
        switch(op) {
            case M8_FP_ADD:v=x+y;break;
            case M8_FP_SUB:v=x-y;break;
            case M8_FP_MUL:v=x*y;break;
            case M8_FP_DIV:v=x/y;break;
            case M8_FP_SQRT:v=sqrtf(x);break;
            default:v=fmaf(x,y,z);break;
        }
        uint32_t r=m8_fp_bits32(v),mag=r & 0x7fffffffu;
        /* Cortex detects tiny results before rounding. The minimum normal
         * boundary needs the complete helper even with sticky IXC. */
        if(mag==0x00800000u) return m8_fp_op32(c,op,a,b,d);
        return mag>0x7f800000u ? 0x7fc00000u : r;
    }
#endif
    return m8_fp_op32(c,op,a,b,d);
}
#undef M8_FP_EVAL_INLINE
/* Rounding: 0..3 FPSCR modes; 4 is ties away from zero. */
static inline double m8_fp_round(double x, unsigned mode) {
    if (!isfinite(x)) return x;
    switch (mode) { case 1:return ceil(x); case 2:return floor(x); case 3:return trunc(x); case 4:return round(x); default: {
        double y=floor(x), f=x-y;
        if (f>0.5 || (f==0.5 && fmod(y,2.0)!=0.0)) y+=1;
        return copysign(y,x);
    } }
}
static inline uint32_t m8_fp_to_int_inner(m8_cpu *c, uint64_t a, int dp, int uns, unsigned mode, unsigned bits, unsigned frac) {
    double x;
    if (dp) { a=m8_fp_input64(c,a); if (m8_fp_nan64(a)) { c->fpscr|=1;return 0; } x=m8_fp_double(a); }
    else { a=m8_fp_input32(c,(uint32_t)a); if (m8_fp_nan32((uint32_t)a)) { c->fpscr|=1;return 0; } x=(double)m8_fp_float((uint32_t)a); }
    x=ldexp(x,(int)frac);
    double y=m8_fp_round(x,mode), lo=uns?0.0:-ldexp(1.0,(int)bits-1), hi=uns?ldexp(1.0,(int)bits)-1:ldexp(1.0,(int)bits-1)-1;
    if (y<lo) { c->fpscr|=1;return uns?0:(uint32_t)(int32_t)lo; }
    if (y>hi) { c->fpscr|=1;return (uint32_t)hi; }
    if (x!=y) c->fpscr|=16;
    return uns?(uint32_t)y:(uint32_t)(int32_t)y;
}
static inline uint32_t m8_fp_to_int(m8_cpu *c, uint64_t a, int dp, int uns, unsigned mode, unsigned bits, unsigned frac) {
    fenv_t env; fegetenv(&env);
    uint32_t result=m8_fp_to_int_inner(c,a,dp,uns,mode,bits,frac);
    fesetenv(&env); return result;
}
static inline uint64_t m8_fp_from_int(m8_cpu *c, uint32_t a, int dp, int uns, unsigned bits, unsigned frac, int nearest) {
    double x;
    if (bits==16) x=uns?(double)(uint16_t)a:(double)(int16_t)a;
    else x=uns?(double)a:(double)(int32_t)a;
    x=ldexp(x,-(int)frac);
    if (dp) return m8_fp_bits64(x);
    int old=fegetround(); if(nearest) fesetround(FE_TONEAREST);
    m8_fp_begin(c); volatile float y=(float)x; uint32_t r=m8_fp_bits32(y); m8_fp_end(c);
    if(nearest) fesetround(old);
    return m8_fp_output32(c,r);
}
static inline uint64_t m8_fp_convert(m8_cpu *c, uint64_t a, int from_dp) {
    if (from_dp) {
        a=m8_fp_input64(c,a);
        if(m8_fp_nan64(a)) { if(m8_fp_snan64(a))c->fpscr|=1; return (c->fpscr&(1u<<25))?0x7fc00000u:(uint32_t)((a>>32)&0x80000000u)|0x7fc00000u|(uint32_t)((a>>29)&0x3fffffu); }
        m8_fp_begin(c); volatile float y=(float)m8_fp_double(a); uint32_t r=m8_fp_bits32(y);
        if ((r & 0x7fffffffu)==0x00800000u) {
            fenv_t env;fegetenv(&env);m8_cpu mode={0};mode.fpscr=3u<<22;m8_fp_configure(&mode);
            volatile float down=(float)m8_fp_double(a);uint32_t bits=m8_fp_bits32(down);fesetenv(&env);
            if (!(bits & 0x7f800000u)) { c->fpscr|=8;if(c->fpscr & (1u<<24)){c->fpscr|=m8_fp_flags() & ~16u;m8_fp_clear();return r & 0x80000000u;} }
        }
        m8_fp_end(c);return m8_fp_output32(c,r);
    }
    uint32_t b=m8_fp_input32(c,(uint32_t)a);
    if(m8_fp_nan32(b)) { if(m8_fp_snan32(b))c->fpscr|=1; return (c->fpscr&(1u<<25))?UINT64_C(0x7ff8000000000000):((uint64_t)(b&0x80000000u)<<32)|UINT64_C(0x7ff8000000000000)|((uint64_t)(b&0x3fffffu)<<29); }
    return m8_fp_bits64((double)m8_fp_float(b));
}
static inline uint64_t m8_fp_rint_inner(m8_cpu *c, uint64_t a, int dp, unsigned mode, int exact) {
    double x;
    if(dp) { a=m8_fp_input64(c,a);if(m8_fp_nan64(a))return m8_fp_propagate64(c,a,0,0);x=m8_fp_double(a); }
    else {a=m8_fp_input32(c,(uint32_t)a);if(m8_fp_nan32((uint32_t)a))return m8_fp_propagate32(c,(uint32_t)a,0,0);x=m8_fp_float((uint32_t)a);}
    double y=m8_fp_round(x,mode);if(exact && x!=y)c->fpscr|=16;
    return dp?m8_fp_bits64(y):m8_fp_bits32((float)y);
}
static inline uint64_t m8_fp_rint(m8_cpu *c, uint64_t a, int dp, unsigned mode, int exact) {
    fenv_t env; fegetenv(&env);
    uint64_t result=m8_fp_rint_inner(c,a,dp,mode,exact);
    fesetenv(&env); return result;
}
#ifdef M8_AOT_NATIVE_FP
/* Exact entry points saved before native aliases for no-sticky fallback. */
static inline uint64_t m8_fp_op64_exact(m8_cpu *c, int op, uint64_t a,
        uint64_t b, uint64_t d) {
    return m8_fp_op64(c, op, a, b, d);
}
static inline uint32_t m8_fp_to_int_exact(m8_cpu *c, uint64_t a, int dp,
        int uns, unsigned mode, unsigned bits, unsigned frac) {
    return m8_fp_to_int(c, a, dp, uns, mode, bits, frac);
}
static inline uint64_t m8_fp_from_int_exact(m8_cpu *c, uint32_t a, int dp,
        int uns, unsigned bits, unsigned frac, int nearest) {
    return m8_fp_from_int(c, a, dp, uns, bits, frac, nearest);
}
static inline uint64_t m8_fp_convert_exact(m8_cpu *c, uint64_t a, int from_dp) {
    return m8_fp_convert(c, a, from_dp);
}
#include "fast_fp.h"
#else
static inline uint64_t m8_fp_op64_exact(m8_cpu *c, int op, uint64_t a,
        uint64_t b, uint64_t d) {
    return m8_fp_op64(c, op, a, b, d);
}
static inline uint64_t m8_fp_convert_exact(m8_cpu *c, uint64_t a, int from_dp) {
    return m8_fp_convert(c, a, from_dp);
}
#endif
#ifdef M8_NATIVE_FP_NO_STICKY
#include "fp_nosticky.h"
#undef m8_fp_enter
#undef m8_fp_leave
#undef m8_fp_sync
#undef m8_fp_configure
#undef m8_fp_eval32
#undef m8_fp_op64
#undef m8_fp_to_int
#undef m8_fp_from_int
#undef m8_fp_convert
#define m8_fp_enter m8_fp_nosticky_enter
#define m8_fp_leave m8_fp_nosticky_leave
#define m8_fp_sync m8_fp_nosticky_sync
#define m8_fp_configure(cpu) ((void)(cpu))
#define m8_fp_eval32 m8_fp_nosticky32
#define m8_fp_op64 m8_fp_nosticky64
#define m8_fp_to_int m8_fp_nosticky_to_int
#define m8_fp_from_int m8_fp_nosticky_from_int
#define m8_fp_convert m8_fp_nosticky_convert
#define m8_fp_cmp32 m8_fp_nosticky_cmp32
#define m8_fp_cmp64 m8_fp_nosticky_cmp64
#define m8_fp_rint m8_fp_nosticky_rint
#define m8_fp_begin(cpu) ((void)(cpu))
#define m8_fp_end(cpu) ((void)(cpu))
#define m8_fp_clear() ((void)0)
#define m8_fp_native32 m8_fp_nosticky32
#define m8_fp_native64 m8_fp_nosticky64
#define m8_fp_native_supported m8_fp_nosticky_supported
#define m8_fp_native_sync m8_fp_nosticky_sync
#if defined(__aarch64__)
#define m8_fp_native_enter m8_fp_nosticky_enter
#define m8_fp_native_leave m8_fp_nosticky_leave
#define m8_fp_native_configure(cpu) ((void)(cpu))
#define m8_fp_native_to_int m8_fp_nosticky_to_int
#define m8_fp_native_from_int m8_fp_nosticky_from_int
#define m8_fp_native_convert m8_fp_nosticky_convert
#define m8_fp_native_minmax32(cpu,op,a,b) m8_fp_nosticky32((cpu),(op),(a),(b),0)
#define m8_fp_native_minmax64(cpu,op,a,b) m8_fp_nosticky64((cpu),(op),(a),(b),0)
#endif
#endif
#undef M8_FP_ZERO_IDENTITY_TRY
#endif
