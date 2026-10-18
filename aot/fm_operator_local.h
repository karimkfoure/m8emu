/* Default-off complete mode-0 operator FP boundary for the structural path. */
#ifndef M8_AOT_FM_OPERATOR_LOCAL_H
#define M8_AOT_FM_OPERATOR_LOCAL_H
#include "fm_structural.h"
#if defined(M8_AOT_FM_STRUCTURAL)
#if defined(M8_WEB_FM_OPERATOR_LOCAL) && (defined(__wasm__) || defined(M8_WEB_FP_TEST) || defined(__x86_64__) || defined(__aarch64__))
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
#if !defined(M8_WEB_NATIVE_FP) || !defined(M8_WEB_EXACT_FP_FAST)
#error "The local FM operator requires the exact Web FP profile"
#endif
#else
#define M8_FMOL_HARDWARE 1
#endif
#include <fp_fast_wasm.h>
#define M8_FMOL_INLINE static inline __attribute__((always_inline))
M8_FMOL_INLINE int m8_fmol_op(int op,float a,float b,float d,float *result) {
    m8_cpu known;known.fpscr=0x03000010u;
    uint32_t bits;int admitted;
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    admitted=m8_web_fp_sticky32(&known,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d),&bits);
    if(admitted){*result=m8_fp_float(bits);return 1;}
    uint32_t am=m8_fp_bits32(a)&0x7fffffffu,bm=m8_fp_bits32(b)&0x7fffffffu;
    if(!m8_fp_bits32(d) && (!am || !bm) &&
       (((op==M8_FP_ADD || op==M8_FP_SUB) &&
         (!am || am-0x10000000u<=0x5fffffffu) && (!bm || bm-0x10000000u<=0x5fffffffu)) ||
        (op==M8_FP_MUL &&
         (!am || am-0x20800000u<=0x3e7fffffu) && (!bm || bm-0x20800000u<=0x3e7fffffu)))) {
        *result=op==M8_FP_ADD?a+b:op==M8_FP_SUB?a-b:a*b;return 1;
    }
    return 0;
}
/* In this domain truncation cannot saturate and the integer is exactly
 * representable in binary32. The complete conversion pair ends with RN,
 * tininess-before and zero private flags. IXC is already sticky. */
M8_FMOL_INLINE int m8_fmol_integer(float value,float *result) {
    uint32_t magnitude=m8_fp_bits32(value)&0x7fffffffu;
    if(magnitude>0x4b000000u || (magnitude && magnitude<0x00800000u))return 0;
    *result=(float)(int32_t)value;return 1;
}
M8_FMOL_INLINE uint32_t m8_fmol_compare(float value) {
    uint32_t bits=m8_fp_bits32(value);
    return !(bits&0x7fffffffu)?0x60000000u:(bits&0x80000000u)?0x80000000u:0x20000000u;
}
M8_FMOL_INLINE int m8_fmol_partial_word(const uint8_t *pm,const uint8_t *word) {
    uintptr_t a=(uintptr_t)pm,b=(uintptr_t)word;
    return a!=b && (a-b<4u || b-a<4u);
}

/* The structural caller qualifies disjoint object/ports/stack/constants
 * spans and a readable PM word. It writes the parent R5 byte before entry.
 * PM may alias any RAM. Model the original prefix stores before loading PM:
 * wrapped phase, total phase, and three stack words. A partial-word alias
 * declines before effects. Other PM addresses cannot observe those stores.
 * Every FP guard runs before real guest effects, so rejection never replays
 * a guest write. This interval contains no callback or observable service. */
#if defined(M8_FMOL_HARDWARE)
#define M8_FMOL_BODY static __attribute__((noinline))
#define M8_FMOL_BODY_NAME m8_fmol_native_math
#else
#define M8_FMOL_BODY M8_FMOL_INLINE
#define M8_FMOL_BODY_NAME m8_fms_operator_try_local
#endif
M8_FMOL_BODY int M8_FMOL_BODY_NAME(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
#else
        uint8_t *stack,uint32_t matrix_address) {
#endif
    if((cpu->fpscr&0x03c00010u)!=0x03000010u)return 0;
    const uint8_t *prefix[]={object+4,object+8,stack,stack+4,stack+8};
    for(unsigned i=0;i<5;i++)if(m8_fmol_partial_word(pm,prefix[i]))return 0;
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    /* The bias word is read at 123e0, after the prefix, feedback and gain2
     * stores. A partial overlap cannot use the whole-word shadow below.
     * Decline before effects; the exact scalar path keeps byte load order. */
    const uint8_t *bias_writes[]={object+4,object+8,stack,stack+4,stack+8,ports+8,ports+20};
    for(unsigned i=0;i<7;i++)if(m8_fmol_partial_word(bias,bias_writes[i]))return 0;
#endif
#define M8_FMOL_OP(DEST,OP,A,B,D) do{if(!m8_fmol_op(OP,A,B,D,&(DEST)))return 0;}while(0)
#define M8_FMOL_INT(DEST,A) do{if(!m8_fmol_integer(A,&(DEST)))return 0;}while(0)
#define M8_FMOL_READ(P) m8_fp_float(m8_le_read(P,4))
    float s0,s7,s8,s9,s10,s11,s12,s13,s14=0.5f,s15;
    float step=M8_FMOL_READ(ports);
    M8_FMOL_OP(s15,M8_FP_ADD,M8_FMOL_READ(object+4),step,0);
    float raw_phase=s15;
    M8_FMOL_OP(s13,M8_FP_ADD,s15,s14,0);
    M8_FMOL_INT(s13,s13);
    M8_FMOL_OP(s12,M8_FP_ADD,M8_FMOL_READ(object+8),step,0);
    float total=s12;
    M8_FMOL_OP(s15,M8_FP_SUB,s15,s13,0);
    float wrapped=s15;
    uint32_t pm_bits=pm==object+4?m8_fp_bits32(wrapped):pm==object+8?m8_fp_bits32(total):
        pm==stack?matrix_address:pm==stack+4?cpu->r[5]:pm==stack+8?cpu->r[6]:m8_le_read(pm,4);
    s13=m8_fp_float(pm_bits);
    M8_FMOL_OP(s15,M8_FP_ADD,s15,s13,0);
    uint32_t compare=m8_fmol_compare(s15);
    M8_FMOL_OP(s14,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    M8_FMOL_INT(s14,s14);
    M8_FMOL_OP(s15,M8_FP_SUB,s15,s14,0);
    s14=M8_FMOL_READ(ports+8);
    s11=0.5f;s7=M8_FMOL_READ(constants+8);
    s13=M8_FMOL_READ(ports+20);s12=M8_FMOL_READ(constants+12);
    M8_FMOL_OP(s14,M8_FP_MUL,s14,s13,0);
    float feedback=s14;
    s9=M8_FMOL_READ(constants+4);s8=M8_FMOL_READ(constants+16);
    s10=M8_FMOL_READ(constants+24);
    M8_FMOL_OP(s15,M8_FP_ADD,s14,s15,0);s14=0.5f;
    compare=m8_fmol_compare(s15);
    M8_FMOL_OP(s14,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    M8_FMOL_INT(s14,s14);
    M8_FMOL_OP(s15,M8_FP_SUB,s15,s14,0);
    s14=M8_FMOL_READ(constants);s13=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    M8_FMOL_OP(s15,M8_FP_MUL,s15,s14,0);s14=M8_FMOL_READ(constants+20);
    M8_FMOL_OP(s11,M8_FP_SUB,s13,s11,0);
    M8_FMOL_OP(s7,M8_FP_ADD,s13,s7,0);M8_FMOL_OP(s12,M8_FP_SUB,s13,s12,0);
    M8_FMOL_OP(s15,M8_FP_MUL,s11,s15,0);
    M8_FMOL_OP(s9,M8_FP_FMA,s7,s13,s9);M8_FMOL_OP(s8,M8_FP_SUB,s13,s8,0);
    M8_FMOL_OP(s14,M8_FP_ADD,s13,s14,0);s11=s10;
    M8_FMOL_OP(s15,M8_FP_MUL,s12,s15,0);
    M8_FMOL_OP(s11,M8_FP_FMA,s8,s13,s11);
    M8_FMOL_OP(s15,M8_FP_MUL,s14,s15,0);
    M8_FMOL_OP(s15,M8_FP_MUL,s15,s9,0);
    M8_FMOL_OP(s11,M8_FP_MUL,s15,s11,0);float polynomial=s11;
    M8_FMOL_OP(s0,M8_FP_MUL,s11,M8_FMOL_READ(ports+12),0);float gain1=s0;
    s15=M8_FMOL_READ(ports+16);
    M8_FMOL_OP(s0,M8_FP_MUL,s0,s15,0);float gain2=s0;
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    /* Original 123c0 loads a stable op+48 pointer after the polynomial store.
     * Original 123e0 reads that word after both gain stores. The caller proves
     * the pointer-source field cannot overlap earlier admitted writes. Model
     * the late value without publishing speculative guest stores. A bias
     * alias of the current flag still reads its entry value: flag=1 follows
     * this final ADD. Other operators' earlier writes already exist in RAM. */
    uint32_t bias_bits=bias==object+4?m8_fp_bits32(wrapped):bias==object+8?m8_fp_bits32(total):
        bias==stack?matrix_address:bias==stack+4?cpu->r[5]:bias==stack+8?cpu->r[6]:
        bias==ports+8?m8_fp_bits32(gain2):bias==ports+20?m8_fp_bits32(feedback):m8_le_read(bias,4);
    s15=m8_fp_float(bias_bits);M8_FMOL_OP(s0,M8_FP_ADD,s0,s15,0);
#else
    s15=M8_FMOL_READ(object+16);M8_FMOL_OP(s0,M8_FP_ADD,s0,s15,0);
#endif
#undef M8_FMOL_READ
#undef M8_FMOL_INT
#undef M8_FMOL_OP
    /* Preserve the original store sequence, including overwritten values. */
    m8_fm_store_bits(stack,matrix_address);
    m8_fm_store_bits(stack+4,cpu->r[5]);m8_fm_store_bits(stack+8,cpu->r[6]);
    m8_fm_store_bits(object+4,m8_fp_bits32(raw_phase));m8_fm_store_bits(object+8,m8_fp_bits32(total));
    m8_fm_store_bits(object+4,m8_fp_bits32(wrapped));m8_fm_store_bits(ports+8,0);
    m8_fm_store_bits(ports+20,m8_fp_bits32(feedback));
    m8_fm_store_bits(ports+8,m8_fp_bits32(polynomial));m8_fm_store_bits(ports+8,m8_fp_bits32(gain1));
    m8_fm_store_bits(ports+8,m8_fp_bits32(gain2));m8_fm_store_bits(ports+8,m8_fp_bits32(s0));object[0x38]=1;
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[7]=m8_fp_bits32(s7);cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);cpu->s[11]=m8_fp_bits32(s11);
    cpu->s[12]=m8_fp_bits32(s12);cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(compare&0x30000000u);
    #if !defined(M8_FMOL_HARDWARE)
    /* The final bounded integer-to-float conversion resets these globals;
     * all later admitted FP operations preserve them or reset them to zero. */
    softfloat_roundingMode=softfloat_round_near_even;
    softfloat_detectTininess=softfloat_tininess_beforeRounding;softfloat_exceptionFlags=0;
    #endif
    return 1;
}
#undef M8_FMOL_BODY
#undef M8_FMOL_BODY_NAME
#if defined(M8_FMOL_HARDWARE)
/* The native caller already entered the FP environment. Require host IXC
 * too: all admitted operations can only add IXC, so success preserves every
 * host status bit. Failed speculation can raise other flags; restore the
 * exact saved status before scalar fallback. The noinline math call keeps
 * speculative arithmetic after this host-mode/trap admission boundary.
 * No native operation reads or writes private SoftFloat context. */
M8_FMOL_INLINE int m8_fms_operator_try_local(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
#else
        uint8_t *stack,uint32_t matrix_address) {
#endif
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
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    int admitted=m8_fmol_native_math(cpu,object,ports,pm,constants,stack,matrix_address,bias);
#else
    int admitted=m8_fmol_native_math(cpu,object,ports,pm,constants,stack,matrix_address);
#endif
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
#undef M8_FMOL_INLINE
#endif
static inline void m8_fms_operator_local(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
#else
        uint8_t *stack,uint32_t matrix_address) {
#endif
#if defined(M8_WEB_FM_OPERATOR_LOCAL) && (defined(__wasm__) || defined(M8_WEB_FP_TEST) || defined(__x86_64__) || defined(__aarch64__))
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    if(m8_fms_operator_try_local(cpu,object,ports,pm,constants,stack,matrix_address,bias))return;
#else
    if(m8_fms_operator_try_local(cpu,object,ports,pm,constants,stack,matrix_address))return;
#endif
#endif
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    m8_fms_operator(cpu,object,ports,pm,constants,stack,matrix_address,bias);
#else
    m8_fms_operator(cpu,object,ports,pm,constants,stack,matrix_address);
#endif
}
#undef M8_FMOL_HARDWARE
#endif
#endif
