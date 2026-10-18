/* Default-off local mode-1 SVF math with a complete scalar fallback. */
#ifndef M8_AOT_FM_SVF_NATIVE_LOCAL_H
#define M8_AOT_FM_SVF_NATIVE_LOCAL_H
#include "fm_operator_local.h"
#include "fm_svf_exact.h"
#if defined(M8_AOT_FM_SVF_NATIVE_LOCAL)
#if !defined(M8_AOT_FM_STRUCTURAL) || !defined(M8_AOT_FM_SVF_EXACT) || !defined(M8_WEB_FM_OPERATOR_LOCAL)
#error "The local native SVF requires structural exact FP and local operators"
#endif
#if defined(M8_AOT_NATIVE_FP) && !defined(M8_WEB_NATIVE_FP) && \
    (defined(__x86_64__) || defined(__aarch64__))
/* The caller qualifies disjoint object/control/literal spans and S17=+0.
 * Entry requires (FPSCR & 0x03c00010)==0x03000010, as structural prepare.
 * The same bounded scalar operations as the local operator prove that only
 * IXC can change. Require guest and host IXC before arithmetic. Every guard
 * precedes guest effects; the host wrapper restores failed speculation.
 * Native arithmetic never reads or writes private SoftFloat globals.
 * Keep this call opaque so no arithmetic crosses host trap/mode admission. */
static __attribute__((noinline)) int m8_svfn_math(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal) {
    if((cpu->fpscr&0x03c00010u)!=0x03000010u)return 0;
#define M8_SVFN_OP(DEST,OP,A,B,D) \
    do {if(!m8_fmol_op(OP,A,B,D,&(DEST)))return 0;} while(0)
    float s0=m8_fp_float(cpu->s[0]),s15=1.0f;
    float s13=m8_filter_read(control+0x44);
    float s10=m8_filter_read(control+0x48);
    M8_SVFN_OP(s13,M8_FP_MINNM,s13,s15,0);
    M8_SVFN_OP(s13,M8_FP_MAXNM,s13,m8_fp_float(cpu->s[17]),0);
    float s7=m8_filter_read(literal+0x20);
    s15=m8_filter_read(object+0x24);
    float s12=1.0f,s14=m8_filter_read(object+0x20),s11;
    M8_SVFN_OP(s11,M8_FP_SUB,s0,s15,0);
    float s6=m8_filter_read(literal+0x24);
    float s8=m8_filter_read(literal+0x28);
    float s9=m8_filter_read(literal+0x2c);
    M8_SVFN_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s14)^0x80000000u),s10,s11);
    M8_SVFN_OP(s14,M8_FP_FMA,s13,s11,s14);
    s11=s0;
    M8_SVFN_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_SVFN_OP(s11,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s14,s11);
    M8_SVFN_OP(s11,M8_FP_SUB,s11,s15,0);
    M8_SVFN_OP(s14,M8_FP_FMA,s13,s11,s14);
    M8_SVFN_OP(s15,M8_FP_FMA,s13,s14,s15);
    M8_SVFN_OP(s14,M8_FP_MAXNM,s14,s6,0);
    s13=s12;
    M8_SVFN_OP(s14,M8_FP_MINNM,s14,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s14)&0x7fffffffu);
    M8_SVFN_OP(s13,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s13);
    M8_SVFN_OP(s15,M8_FP_MAXNM,s15,s6,0);
    M8_SVFN_OP(s15,M8_FP_MINNM,s15,s8,0);
    s10=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    M8_SVFN_OP(s12,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s12);
    M8_SVFN_OP(s14,M8_FP_MUL,s14,s13,0);
    M8_SVFN_OP(s15,M8_FP_MUL,s15,s12,0);
    float filtered=s14;
    float previous=m8_filter_read(object+0x28);
    s14=m8_filter_read(object+0x2c);
    M8_SVFN_OP(s0,M8_FP_FMA,s14,s7,m8_fp_float(m8_fp_bits32(previous)^0x80000000u));
    M8_SVFN_OP(s0,M8_FP_ADD,s15,s0,0);
#undef M8_SVFN_OP
    /* Every guard passed before the first guest effect. Publish in the
     * original write order and preserve the scalar helper's clobber set. */
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
static inline int m8_fmb_filter_mode1_native_try_local(m8_cpu *cpu,
        uint8_t *object,uint8_t *control,const uint8_t *literal) {
#if defined(M8_NATIVE_FP_NO_STICKY)
    /* Host mode is fixed per thread; no host test needed. */
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))return 0;
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    uint64_t mode,saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(mode));
    __asm__ volatile("mrs %0, fpsr" : "=r"(saved));
    if((mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))return 0;
#endif
    int admitted=m8_svfn_math(cpu,object,control,literal);
    if(!admitted) {
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        _mm_setcsr(saved);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        __asm__ volatile("msr fpsr, %0" :: "r"(saved) : "memory");
#endif
    }
    return admitted;
}
#endif
static inline void m8_fmb_filter_mode1_native_local(m8_cpu *cpu,
        uint8_t *object,uint8_t *control,const uint8_t *literal) {
#if defined(M8_AOT_NATIVE_FP) && !defined(M8_WEB_NATIVE_FP) && \
    (defined(__x86_64__) || defined(__aarch64__))
    if(m8_fmb_filter_mode1_native_try_local(cpu,object,control,literal))return;
#endif
    m8_fmb_filter_svf_exact(cpu,object,control,literal,1);
}
#endif
#endif
