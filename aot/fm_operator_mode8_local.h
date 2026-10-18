/* Private default-off whole mode-8 arithmetic candidate. The structural
 * caller qualifies disjoint object/ports/stack and readable PM/bias words.
 * All guards run before guest effects. Local shadows model the prefix and
 * late bias reads; success publishes the original ordered stores and state.
 */
#ifndef M8_AOT_FM_OPERATOR_MODE8_LOCAL_H
#define M8_AOT_FM_OPERATOR_MODE8_LOCAL_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_MODE8) && defined(M8_AOT_FM_MODE8_LOCAL)
#include <fp_fast_wasm.h>
#define M8_FM8L_INLINE static inline __attribute__((always_inline))
#if defined(M8_FM_MODE8_LOCAL_TEST)
static uint64_t m8_fm8l_counts[6];
#endif
M8_FM8L_INLINE int m8_fm8l_op(int op,float a,float b,float d,float *result) {
    m8_cpu known={0};known.fpscr=0x03000010u;
    uint32_t bits;int admitted;
#if defined(__clang__)
    [[clang::always_inline]]
#endif
    admitted=m8_web_fp_sticky32(&known,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d),&bits);
    if(admitted){*result=m8_fp_float(bits);return 1;}
    uint32_t am=m8_fp_bits32(a)&0x7fffffffu,bm=m8_fp_bits32(b)&0x7fffffffu;
    /* The exact sticky helper excludes zero ADD/SUB/MUL operands. Extend
     * only those same proved ranges to zeros, including signed zero. */
    if(!m8_fp_bits32(d) && (!am || !bm) &&
       (((op==M8_FP_ADD || op==M8_FP_SUB) &&
         (!am || am-0x10000000u<=0x5fffffffu) && (!bm || bm-0x10000000u<=0x5fffffffu)) ||
        (op==M8_FP_MUL &&
         (!am || am-0x20800000u<=0x3e7fffffu) && (!bm || bm-0x20800000u<=0x3e7fffffu)))) {
        *result=op==M8_FP_ADD?a+b:op==M8_FP_SUB?a-b:a*b;return 1;
    }
    return 0;
}
M8_FM8L_INLINE int m8_fm8l_integer(float value,float *result) {
    uint32_t magnitude=m8_fp_bits32(value)&0x7fffffffu;
    if(magnitude>0x4b000000u || (magnitude && magnitude<0x00800000u))return 0;
    *result=(float)(int32_t)value;return 1;
}
M8_FM8L_INLINE uint32_t m8_fm8l_compare(float a,float b) {
    return a==b?0x60000000u:a<b?0x80000000u:0x20000000u;
}
M8_FM8L_INLINE int m8_fm8l_partial(const uint8_t *p,const uint8_t *word) {
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)word;
    return a!=b && (a-b<4u || b-a<4u);
}
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
#define M8_FM8L_BODY M8_FM8L_INLINE
#else
/* Keep speculative native arithmetic after host-status admission. */
#define M8_FM8L_BODY static __attribute__((noinline))
#endif
M8_FM8L_BODY int m8_fm8l_math(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
    (void)constants;
    if((cpu->fpscr&0x03c00010u)!=0x03000010u)return 0;
    const uint8_t *prefix[]={object+4,object+8,stack,stack+4,stack+8};
    const uint8_t *bias_writes[]={object+4,object+8,stack,stack+4,stack+8,ports+8,ports+20};
    for(unsigned i=0;i<5;i++)if(m8_fm8l_partial(pm,prefix[i]))return 0;
    for(unsigned i=0;i<7;i++)if(m8_fm8l_partial(bias,bias_writes[i]))return 0;
#define M8_FM8L_OP(DEST,OP,A,B,D) do{if(!m8_fm8l_op(OP,A,B,D,&(DEST)))return 0;}while(0)
#define M8_FM8L_INT(DEST,A) do{if(!m8_fm8l_integer(A,&(DEST)))return 0;}while(0)
#define M8_FM8L_READ(P) m8_fp_float(m8_le_read(P,4))
#define M8_FM8L_NEG(A) m8_fp_float(m8_fp_bits32(A)^0x80000000u)
    float s0,s9,s10,s11,s12,s13,s14=0.5f,s15;
    unsigned paths=0;float step=M8_FM8L_READ(ports);
    M8_FM8L_OP(s15,M8_FP_ADD,M8_FM8L_READ(object+4),step,0);float raw_phase=s15;
    M8_FM8L_OP(s13,M8_FP_ADD,s15,s14,0);M8_FM8L_INT(s13,s13);
    M8_FM8L_OP(s12,M8_FP_ADD,M8_FM8L_READ(object+8),step,0);float total=s12;
    M8_FM8L_OP(s15,M8_FP_SUB,s15,s13,0);float wrapped=s15;
    uint32_t pm_bits=pm==object+4?m8_fp_bits32(wrapped):pm==object+8?m8_fp_bits32(total):
        pm==stack?matrix_address:pm==stack+4?cpu->r[5]:pm==stack+8?cpu->r[6]:m8_le_read(pm,4);
    s13=m8_fp_float(pm_bits);M8_FM8L_OP(s15,M8_FP_ADD,s15,s13,0);
    uint32_t compare=m8_fm8l_compare(s15,0);
    M8_FM8L_OP(s14,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    M8_FM8L_INT(s14,s14);M8_FM8L_OP(s15,M8_FP_SUB,s15,s14,0);
    s14=M8_FM8L_READ(ports+8);s9=0.5f;s10=-1.0f;s13=M8_FM8L_READ(ports+20);
    M8_FM8L_OP(s14,M8_FP_MUL,s14,s13,0);float feedback=s14;
    M8_FM8L_OP(s15,M8_FP_ADD,s14,s15,0);s14=0.5f;
    compare=m8_fm8l_compare(s15,0);
    M8_FM8L_OP(s14,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    M8_FM8L_INT(s14,s14);M8_FM8L_OP(s15,M8_FP_SUB,s15,s14,0);s14=1.0f;
    compare=m8_fm8l_compare(s15,0);M8_FM8L_OP(s12,M8_FP_ADD,s15,s9,0);
    s11=m8_fm8_gt(compare)?s14:s10;float wave_first=s11;
    s13=step;compare=m8_fm8l_compare(s13,s12);
    if(m8_fm8_gt(compare)) {
        paths|=1;M8_FM8L_OP(s10,M8_FP_DIV,s12,s13,0);s13=2.0f;
        M8_FM8L_OP(s13,M8_FP_SUB,s13,s10,0);
        M8_FM8L_OP(s14,M8_FP_FMA,M8_FM8L_NEG(s10),s13,s14);
        M8_FM8L_OP(s11,M8_FP_ADD,s11,s14,0);
    } else {
        M8_FM8L_OP(s14,M8_FP_SUB,s14,s13,0);compare=m8_fm8l_compare(s12,s14);
        if(m8_fm8_gt(compare)) {
            paths|=2;M8_FM8L_OP(s9,M8_FP_SUB,s15,s9,0);s14=2.0f;
            M8_FM8L_OP(s12,M8_FP_DIV,s9,s13,0);
            M8_FM8L_OP(s14,M8_FP_ADD,s12,s14,0);
            M8_FM8L_OP(s10,M8_FP_FMA,M8_FM8L_NEG(s12),s14,s10);
            M8_FM8L_OP(s11,M8_FP_ADD,s11,s10,0);
        }
    }
    compare=m8_fm8l_compare(s15,0);float wave_second=s11;
    if(!m8_fm8_gt(compare)){s14=1.0f;M8_FM8L_OP(s15,M8_FP_ADD,s15,s14,0);}
    s14=step;compare=m8_fm8l_compare(s14,s15);
    if(m8_fm8_gt(compare)) {
        paths|=4;M8_FM8L_OP(s13,M8_FP_DIV,s15,s14,0);
        s14=2.0f;s15=1.0f;M8_FM8L_OP(s15,M8_FP_SUB,s11,s15,0);
        M8_FM8L_OP(s14,M8_FP_SUB,s14,s13,0);
        M8_FM8L_OP(s15,M8_FP_FMA,s13,s14,s15);s11=s15;
    } else {
        s13=1.0f;M8_FM8L_OP(s12,M8_FP_SUB,s13,s14,0);compare=m8_fm8l_compare(s15,s12);
        if(m8_fm8_gt(compare)) {
            paths|=8;M8_FM8L_OP(s15,M8_FP_SUB,s15,s13,0);s12=2.0f;
            M8_FM8L_OP(s13,M8_FP_ADD,s11,s13,0);
            M8_FM8L_OP(s10,M8_FP_DIV,s15,s14,0);
            M8_FM8L_OP(s15,M8_FP_ADD,s10,s12,0);
            M8_FM8L_OP(s13,M8_FP_FMA,s10,s15,s13);s11=s13;
        }
    }
    M8_FM8L_OP(s0,M8_FP_MUL,s11,M8_FM8L_READ(ports+12),0);float gain1=s0;
    s15=M8_FM8L_READ(ports+16);M8_FM8L_OP(s0,M8_FP_MUL,s0,s15,0);float gain2=s0;
    uint32_t bias_bits=bias==object+4?m8_fp_bits32(wrapped):bias==object+8?m8_fp_bits32(total):
        bias==stack?matrix_address:bias==stack+4?cpu->r[5]:bias==stack+8?cpu->r[6]:
        bias==ports+8?m8_fp_bits32(gain2):bias==ports+20?m8_fp_bits32(feedback):m8_le_read(bias,4);
    s15=m8_fp_float(bias_bits);M8_FM8L_OP(s0,M8_FP_ADD,s0,s15,0);
#undef M8_FM8L_READ
#undef M8_FM8L_INT
#undef M8_FM8L_OP
#undef M8_FM8L_NEG
    m8_fm_store_bits(stack,matrix_address);
    m8_fm_store_bits(stack+4,cpu->r[5]);m8_fm_store_bits(stack+8,cpu->r[6]);
    m8_fm_store_bits(object+4,m8_fp_bits32(raw_phase));m8_fm_store_bits(object+8,m8_fp_bits32(total));
    m8_fm_store_bits(object+4,m8_fp_bits32(wrapped));m8_fm_store_bits(ports+8,0);
    m8_fm_store_bits(ports+20,m8_fp_bits32(feedback));
    m8_fm_store_bits(ports+8,m8_fp_bits32(wave_first));m8_fm_store_bits(ports+8,m8_fp_bits32(wave_second));
    m8_fm_store_bits(ports+8,m8_fp_bits32(s11));m8_fm_store_bits(ports+8,m8_fp_bits32(gain1));
    m8_fm_store_bits(ports+8,m8_fp_bits32(gain2));m8_fm_store_bits(ports+8,m8_fp_bits32(s0));object[0x38]=1;
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11);cpu->s[12]=m8_fp_bits32(s12);cpu->s[13]=m8_fp_bits32(s13);
    cpu->s[14]=m8_fp_bits32(s14);cpu->s[15]=m8_fp_bits32(s15);
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(compare&0x30000000u);
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
    /* The last exact integer-to-float conversion leaves this state. All
     * later admitted operations use the bounded path and preserve it. */
    softfloat_roundingMode=softfloat_round_near_even;
    softfloat_detectTininess=softfloat_tininess_beforeRounding;softfloat_exceptionFlags=0;
#endif
#if defined(M8_FM_MODE8_LOCAL_TEST)
    m8_fm8l_counts[0]++;for(unsigned i=0;i<4;i++)if(paths&(1u<<i))m8_fm8l_counts[2+i]++;
#else
    (void)paths;
#endif
    return 1;
}
#undef M8_FM8L_BODY
M8_FM8L_INLINE int m8_fm8l_try(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
#if defined(M8_NATIVE_FP_NO_STICKY)
    /* Host mode is fixed per thread; no host test needed. */
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))return 0;
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    uint64_t mode,saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(mode));__asm__ volatile("mrs %0, fpsr" : "=r"(saved));
    if((mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))return 0;
#else
    return 0;
#endif
#endif
    int admitted=m8_fm8l_math(cpu,object,ports,pm,constants,stack,matrix_address,bias);
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
    if(!admitted){
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        _mm_setcsr(saved);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        __asm__ volatile("msr fpsr, %0" :: "r"(saved) : "memory");
#endif
    }
#endif
    return admitted;
}
M8_FM8L_INLINE void m8_fm8l_operator(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
    if(m8_fm8l_try(cpu,object,ports,pm,constants,stack,matrix_address,bias))return;
#if defined(M8_FM_MODE8_LOCAL_TEST)
    m8_fm8l_counts[1]++;
#endif
    m8_fms_operator_mode8(cpu,object,ports,pm,constants,stack,matrix_address,bias);
}
#define m8_fms_operator_mode8 m8_fm8l_operator
#undef M8_FM8L_INLINE
#endif
#endif
