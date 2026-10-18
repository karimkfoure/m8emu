/* Optional complete mode-1 FM filter boundary; no shared FP API changes. */
#ifndef M8_AOT_FM_SVF_MODE1_FP_H
#define M8_AOT_FM_SVF_MODE1_FP_H
#include "fm_svf_mode1.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#if defined(M8_WEB_FM_MODE1_FP) && (defined(__wasm__) || defined(M8_WEB_FP_TEST))
#if !defined(M8_WEB_NATIVE_FP) || !defined(M8_WEB_EXACT_FP_FAST)
#error "The mode-1 local filter requires the exact Web FP profile"
#endif
#include <fp_fast_wasm.h>
#define M8_SVF1_INLINE static inline __attribute__((always_inline))

/* The whole filter proves RN/FZ/DN/sticky IXC once. A constant mode object
 * makes the existing scalar admission checks independent of live CPU state.
 * Its unused words are never read. Keep the unchanged shared FMA proof and
 * midpoint rejection; this candidate does not replace raw libc fmaf.
 *
 * Zero ADD/SUB/MUL operands extend the existing bounded cases exactly. The
 * original helper resets its private context on those exact operations.
 * There are no callbacks within this interval. Other admitted operations
 * preserve that context, so one deferred zero-flags reset has the same final
 * state. Any unproved operation declines before guest writes or CPU changes. */
M8_SVF1_INLINE int m8_svf1_local_op(int op,float a,float b,float d,
        float *result,int *reset_context) {
    m8_cpu known_mode;known_mode.fpscr=0x03000010u;
    uint32_t bits;
    int admitted;
    [[clang::always_inline]]
    admitted=m8_web_fp_sticky32(&known_mode,op,
        m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d),&bits);
    if(admitted){*result=m8_fp_float(bits);return 1;}
    if(m8_fp_bits32(d))return 0;
    uint32_t am=m8_fp_bits32(a)&0x7fffffffu,bm=m8_fp_bits32(b)&0x7fffffffu;
    if((!am || !bm) &&
       (((op==M8_FP_ADD || op==M8_FP_SUB) &&
         (!am || am-0x10000000u<=0x5fffffffu) &&
         (!bm || bm-0x10000000u<=0x5fffffffu)) ||
        (op==M8_FP_MUL &&
         (!am || am-0x20800000u<=0x3e7fffffu) &&
         (!bm || bm-0x20800000u<=0x3e7fffffu)))) {
        *reset_context=1;
        *result=op==M8_FP_ADD?a+b:op==M8_FP_SUB?a-b:a*b;
        return 1;
    }
    return 0;
}

/* Caller predicates match the scalar helper: R1==1, S17==+0 and disjoint
 * qualified object/control/literal spans. No guest read is speculative: all
 * reads below are existing direct RAM/literal reads within those spans.
 * Pure local arithmetic can be discarded before the first guest effect. */
M8_SVF1_INLINE int m8_fmb_filter_mode1_try_local(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal) {
    if((cpu->fpscr&0x03c00010u)!=0x03000010u)return 0;
    int reset_context=0;
#define M8_SVF1_OP(DEST,OP,A,B,D) \
    do {if(!m8_svf1_local_op(OP,A,B,D,&(DEST),&reset_context))return 0;} while(0)
    float s0=m8_fp_float(cpu->s[0]),s15=1.0f;
    float s13=m8_filter_read(control+0x44);
    float s10=m8_filter_read(control+0x48);
    M8_SVF1_OP(s13,M8_FP_MINNM,s13,s15,0);
    M8_SVF1_OP(s13,M8_FP_MAXNM,s13,m8_fp_float(cpu->s[17]),0);
    float s7=m8_filter_read(literal+0x20);
    s15=m8_filter_read(object+0x24);
    float s12=1.0f,s14=m8_filter_read(object+0x20),s11;
    M8_SVF1_OP(s11,M8_FP_SUB,s0,s15,0);
    float s6=m8_filter_read(literal+0x24);
    float s8=m8_filter_read(literal+0x28);
    float s9=m8_filter_read(literal+0x2c);
    M8_SVF1_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s14)^0x80000000u),s10,s11);
    M8_SVF1_OP(s14,M8_FP_FMA,s13,s11,s14);
    s11=s0;
    M8_SVF1_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_SVF1_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s14,s11);
    M8_SVF1_OP(s11,M8_FP_SUB,s11,s15,0);
    M8_SVF1_OP(s14,M8_FP_FMA,s13,s11,s14);
    M8_SVF1_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_SVF1_OP(s14,M8_FP_MAXNM,s14,s6,0);
    s13=s12;
    M8_SVF1_OP(s14,M8_FP_MINNM,s14,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s14)&0x7fffffffu);
    M8_SVF1_OP(s13,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s13);
    M8_SVF1_OP(s15,M8_FP_MAXNM,s15,s6,0);
    M8_SVF1_OP(s15,M8_FP_MINNM,s15,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    M8_SVF1_OP(s12,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s12);
    M8_SVF1_OP(s14,M8_FP_MUL,s14,s13,0);
    M8_SVF1_OP(s15,M8_FP_MUL,s15,s12,0);
    float filtered=s14;
    float previous=m8_filter_read(object+0x28);
    s14=m8_filter_read(object+0x2c);
    M8_SVF1_OP(s0,M8_FP_FMA,s14,s7,m8_fp_float(m8_fp_bits32(previous)^0x80000000u));
    M8_SVF1_OP(s0,M8_FP_ADD,s15,s0,0);
#undef M8_SVF1_OP
    /* Every guard passed before the first guest effect. Publish in the
     * original write order and preserve the scalar helper's clobber set. */
    if(reset_context) {
        softfloat_roundingMode=softfloat_round_near_even;
        softfloat_detectTininess=softfloat_tininess_beforeRounding;
        softfloat_exceptionFlags=0;
    }
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    m8_filter_write(object+0x20,filtered);
    m8_filter_write(object+0x24,s15);
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
#undef M8_SVF1_INLINE
#endif

static inline void m8_fmb_filter_mode1_specialized(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal) {
#if defined(M8_WEB_FM_MODE1_FP) && (defined(__wasm__) || defined(M8_WEB_FP_TEST))
    if(m8_fmb_filter_mode1_try_local(cpu,object,control,literal))return;
#endif
    m8_fmb_filter_mode1(cpu,object,control,literal);
}
#endif
#endif
