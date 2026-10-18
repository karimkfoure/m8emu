/* Exact mixer-block FP for fixed RN/FZ/DN execution. Same host-FP policy
 * as the FM/HYPER fast engines: inline host arithmetic under the fixed
 * no-sticky mode, ARM default-NaN canonicalization, and a cold exact
 * helper at the minimum-normal boundary where x86 and ARM detect
 * underflow at different points. Web/AArch64 use the exact helpers. */
#ifndef M8_AOT_MIXER_FAST_FP_H
#define M8_AOT_MIXER_FAST_FP_H
#if defined(M8_AOT_MIXER_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST)
static __attribute__((cold,noinline)) uint32_t m8_mxf_tiny32(int op,
        uint32_t a,uint32_t b,uint32_t d) {
    m8_cpu mode={0};mode.fpscr=0x03000000u;
    return m8_fp_nosticky_fallback32(&mode,op,a,b,d);
}
#endif
static inline __attribute__((always_inline)) uint32_t m8_mxf_op(
        m8_cpu *cpu,int op,uint32_t a,uint32_t b,uint32_t d) {
#if defined(M8_AOT_MIXER_FAST) && defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__) && !defined(M8_WEB_FP_TEST) && defined(__FMA__)
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
        return m8_mxf_tiny32(op,a,b,d);
    return result;
#elif defined(__aarch64__) || defined(__wasm__) || defined(M8_WEB_FP_TEST)
    return m8_fp_eval32(cpu,op,a,b,d);
#else
    return m8_fp_op32(cpu,op,a,b,d);
#endif
}
#endif
