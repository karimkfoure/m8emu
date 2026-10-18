/* Exact effect-block FP for fixed RN/FZ/DN execution. Same host-FP policy
 * as the FM/HYPER fast engines: inline host arithmetic under the fixed
 * no-sticky mode, ARM default-NaN canonicalization, and a cold exact
 * helper at the minimum-normal boundary where x86 and ARM detect
 * underflow at different points. Web/AArch64 use the exact helpers. */
#ifndef M8_AOT_EFFECT_FAST_FP_H
#define M8_AOT_EFFECT_FAST_FP_H
#if defined(M8_AOT_EFFECT_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST)
static __attribute__((cold,noinline)) uint32_t m8_ef_tiny32(int op,
        uint32_t a,uint32_t b,uint32_t d) {
    m8_cpu mode={0};mode.fpscr=0x03000000u;
    return m8_fp_nosticky_fallback32(&mode,op,a,b,d);
}
#endif
static inline __attribute__((always_inline)) uint32_t m8_ef_op(
        m8_cpu *cpu,int op,uint32_t a,uint32_t b,uint32_t d) {
#if defined(M8_AOT_EFFECT_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST) && defined(__FMA__)
    if(op>=M8_FP_MAXNM)return m8_fp_nosticky_minmax32(cpu,op,a,b);
    float x=m8_fp_float(a),y=m8_fp_float(b),z=m8_fp_float(d),v;
    switch(op) {
    case M8_FP_ADD:v=x+y;break;
    case M8_FP_SUB:v=x-y;break;
    case M8_FP_MUL:v=x*y;break;
    case M8_FP_DIV:v=x/y;break;
    default:v=fmaf(x,y,z);break;
    }
    if(__builtin_expect(v!=v,0))return 0x7fc00000u;
    uint32_t result=m8_fp_bits32(v);
    if(__builtin_expect((result&0x7fffffffu)==0x00800000u,0))
        return m8_ef_tiny32(op,a,b,d);
    return result;
#elif defined(__aarch64__) || defined(__wasm__) || defined(M8_WEB_FP_TEST)
    return m8_fp_eval32(cpu,op,a,b,d);
#else
    return m8_fp_op32(cpu,op,a,b,d);
#endif
}
/* Fixed 32-bit conversions matching VCVT (round-to-zero, NaN and
 * saturation behavior preserved). The surrounding native boundary owns
 * the FP mode and cumulative flags. */
static inline __attribute__((always_inline)) uint32_t
m8_ef_to_int(m8_cpu *cpu,uint32_t bits,int uns) {
#if defined(M8_AOT_EFFECT_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST)
    (void)cpu;
    float value=m8_fp_float(bits);
    if(uns) {
        if(!(value>0.0f)) return 0;
        if(value>=0x1p32f) return UINT32_MAX;
        return (uint32_t)value;
    }
    if(value!=value) return 0;
    if(value>=0x1p31f) return INT32_MAX;
    if(value<=-0x1p31f) return 0x80000000u;
    return (uint32_t)(int32_t)value;
#elif defined(M8_AOT_EFFECT_FAST) && defined(__aarch64__) && defined(M8_AOT_NATIVE_FP) && !defined(M8_WEB_FP_TEST)
    (void)cpu;uint32_t result;float value=m8_fp_float(bits);
    if(uns) __asm__ volatile("fcvtzu %w0, %s1" : "=r"(result) : "w"(value));
    else __asm__ volatile("fcvtzs %w0, %s1" : "=r"(result) : "w"(value));
    return result;
#else
    return m8_fp_to_int(cpu,bits,0,uns,3,32,0);
#endif
}
static inline __attribute__((always_inline)) uint32_t
m8_ef_from_int(m8_cpu *cpu,uint32_t bits,int uns) {
#if defined(M8_AOT_EFFECT_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST)
    (void)cpu;return m8_fp_bits32(uns?(float)bits:(float)(int32_t)bits);
#elif defined(M8_AOT_EFFECT_FAST) && defined(__aarch64__) && defined(M8_AOT_NATIVE_FP) && !defined(M8_WEB_FP_TEST)
    (void)cpu;float result;
    if(uns) __asm__ volatile("ucvtf %s0, %w1" : "=w"(result) : "r"(bits));
    else __asm__ volatile("scvtf %s0, %w1" : "=w"(result) : "r"(bits));
    return m8_fp_bits32(result);
#else
    return (uint32_t)m8_fp_from_int(cpu,bits,0,uns,32,0,0);
#endif
}
/* VCMP/VCMPE flag update matching m8_fp_nosticky_cmp32 (NaN gives
 * N=0,Z=0,C=1,V=1; equal gives Z=1,C=1; less gives N=1; greater C=1),
 * written to private NZCV locals instead of a CPU FPSCR. */
static inline __attribute__((always_inline)) void
m8_ef_cmp(uint32_t *n,uint32_t *z,uint32_t *c,uint32_t *v,uint32_t aa,uint32_t bb) {
    float x=m8_fp_float(aa),y=m8_fp_float(bb);
    uint32_t un=(x!=x)||(y!=y);
    *n=!un&&(x<y);
    *z=!un&&(x==y);
    *c=un||!(x<y);
    *v=un;
}
#endif
