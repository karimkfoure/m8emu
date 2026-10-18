/* Default-off shared local SVF families1..4. The caller qualifies disjoint
 * object/control/literal spans. Every bounded arithmetic guard precedes
 * guest effects; final stores retain original20/24/28/2c order. On decline
 * the complete exact scalar core runs from the unchanged entry state. */
#ifndef M8_AOT_FM_SVF_LOCAL_H
#define M8_AOT_FM_SVF_LOCAL_H
#if defined(M8_AOT_FM_SVF_LOCAL)
#if !defined(M8_AOT_FM_STRUCTURAL) || !defined(M8_AOT_FM_SVF_EXACT) || !defined(M8_WEB_FM_OPERATOR_LOCAL)
#error "Local SVF requires the exact structural core and bounded operators"
#endif
/* Include after the structural local-operator API (m8_fmol_op). */
#include "fm_svf_exact.h"
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
#define M8_FMSVFL_BODY static inline __attribute__((always_inline))
#else
/* Keep speculative math after native host trap/mode/status admission. */
#define M8_FMSVFL_BODY static __attribute__((noinline))
#endif
static inline __attribute__((always_inline)) int m8_fmsvfl_op(int op,
        float a,float b,float d,float *result,int *reset_context) {
    if(!m8_fmol_op(op,a,b,d,result))return 0;
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
    /* Sticky fast operations preserve private context. Only the existing
     * zero ADD/SUB/MUL extension corresponds to a scalar software reset.
     * No callback occurs here, so publish that same reset after all guards. */
    if((unsigned)op<=M8_FP_MUL &&
       (!(m8_fp_bits32(a)&0x7fffffffu) || !(m8_fp_bits32(b)&0x7fffffffu)))
        *reset_context=1;
#else
    (void)reset_context;
#endif
    return 1;
}
M8_FMSVFL_BODY int m8_fmsvfl_math(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal,unsigned mode) {
    if(mode-1u>3u || cpu->r[1]!=mode || cpu->s[17] ||
       (cpu->fpscr&0x03c00010u)!=0x03000010u)return 0;
    int reset_context=0;
#define M8_FMSVFL_OP(DEST,OP,A,B,D) \
    do {if(!m8_fmsvfl_op(OP,A,B,D,&(DEST),&reset_context))return 0;} while(0)
    float s0=m8_fp_float(cpu->s[0]),s15=1.0f;
    float s13=m8_filter_read(control+0x44);
    float s10=m8_filter_read(control+0x48);
    M8_FMSVFL_OP(s13,M8_FP_MINNM,s13,s15,0);
    M8_FMSVFL_OP(s13,M8_FP_MAXNM,s13,m8_fp_float(cpu->s[17]),0);
    float s7=m8_filter_read(literal+0x20);
    s15=m8_filter_read(object+0x24);
    float s12=1.0f,s14=m8_filter_read(object+0x20),s11;
    M8_FMSVFL_OP(s11,M8_FP_SUB,s0,s15,0);
    float s6=m8_filter_read(literal+0x24);
    float s8=m8_filter_read(literal+0x28);
    float s9=m8_filter_read(literal+0x2c);
    M8_FMSVFL_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s14)^0x80000000u),s10,s11);
    M8_FMSVFL_OP(s14,M8_FP_FMA,s13,s11,s14);
    s11=s0;
    M8_FMSVFL_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_FMSVFL_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s14,s11);
    M8_FMSVFL_OP(s11,M8_FP_SUB,s11,s15,0);
    M8_FMSVFL_OP(s14,M8_FP_FMA,s13,s11,s14);
    M8_FMSVFL_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_FMSVFL_OP(s14,M8_FP_MAXNM,s14,s6,0);
    s13=s12;
    M8_FMSVFL_OP(s14,M8_FP_MINNM,s14,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s14)&0x7fffffffu);
    M8_FMSVFL_OP(s13,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s13);
    M8_FMSVFL_OP(s15,M8_FP_MAXNM,s15,s6,0);
    M8_FMSVFL_OP(s15,M8_FP_MINNM,s15,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    M8_FMSVFL_OP(s12,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s12);
    M8_FMSVFL_OP(s14,M8_FP_MUL,s14,s13,0);
    M8_FMSVFL_OP(s15,M8_FP_MUL,s15,s12,0);
    float filtered=s14,band=s15;
    if(mode==2) s15=s11;
    else if(mode==3) s15=filtered;
    else if(mode==4) M8_FMSVFL_OP(s15,M8_FP_SUB,s0,filtered,0);
    float previous=m8_filter_read(object+0x28);
    s14=m8_filter_read(object+0x2c);
    M8_FMSVFL_OP(s0,M8_FP_FMA,s14,s7,m8_fp_float(m8_fp_bits32(previous)^0x80000000u));
    M8_FMSVFL_OP(s0,M8_FP_ADD,s15,s0,0);
#undef M8_FMSVFL_OP
    /* Every guard passed before the first guest effect. Publish in the
     * original write order and preserve the scalar helper's clobber set. */
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
    if(reset_context) {
        softfloat_roundingMode=softfloat_round_near_even;
        softfloat_detectTininess=softfloat_tininess_beforeRounding;
        softfloat_exceptionFlags=0;
    }
#else
    (void)reset_context;
#endif
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    m8_filter_write(object+0x20,filtered);
    m8_filter_write(object+0x24,band);
    m8_filter_write(object+0x28,s15);
    m8_filter_write(object+0x2c,s0);
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[6]=m8_fp_bits32(s6);
    cpu->s[7]=m8_fp_bits32(s7);cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11);cpu->s[12]=m8_fp_bits32(s12);
    cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);
    return 1;
}
#undef M8_FMSVFL_BODY
static inline int m8_fmb_filter_svf_try_local(m8_cpu *cpu,
        uint8_t *object,uint8_t *control,const uint8_t *literal,unsigned mode) {
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
#if defined(M8_NATIVE_FP_NO_STICKY)
    /* Host mode is fixed per thread; no host test needed. */
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))return 0;
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    uint64_t native_mode,saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(native_mode));
    __asm__ volatile("mrs %0, fpsr" : "=r"(saved));
    if((native_mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))return 0;
#else
    return 0;
#endif
#endif
    int admitted=m8_fmsvfl_math(cpu,object,control,literal,mode);
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
    if(!admitted) {
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        _mm_setcsr(saved);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        __asm__ volatile("msr fpsr, %0" :: "r"(saved) : "memory");
#endif
    }
#endif
    return admitted;
}
static inline void m8_fmb_filter_svf_local(m8_cpu *cpu,
        uint8_t *object,uint8_t *control,const uint8_t *literal,unsigned mode) {
    if(m8_fmb_filter_svf_try_local(cpu,object,control,literal,mode))return;
    m8_fmb_filter_svf_exact(cpu,object,control,literal,mode);
}
#endif
#endif
