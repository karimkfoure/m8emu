/* Default-off complete callback arithmetic in C locals. The scalar checked
 * callbacks remain the fallback. The parent qualifies callback2284's table
 * and literals before entry; both table words stay at their original late
 * positions. No guest state or private FP state changes before all guards
 * pass. The existing bounded operator adapter proves each arithmetic step.
 */
#ifndef M8_AOT_FM_CALLBACK_LOCAL_H
#define M8_AOT_FM_CALLBACK_LOCAL_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_CALLBACK_LOCAL)
#if !defined(M8_WEB_FM_OPERATOR_LOCAL)
#error "The local callbacks require the shared bounded FM operator adapter"
#endif
#include "fm_operator_local.h"
#include "fm_callback_2284.h"
#if defined(M8_AOT_FM_CALLBACK_1FBC)
#include "fm_callback_1fbc.h"
#endif
#define M8_FMCL_INLINE static inline __attribute__((always_inline))
#if defined(M8_FM_CALLBACK_LOCAL_TEST)
static uint64_t m8_fmcl_counts[5];
static unsigned char m8_fmcl_indices[512];
#endif
M8_FMCL_INLINE int m8_fmcl_cpu(const m8_cpu *cpu) {
    return (cpu->xpsr&0x0700fc00u)==0x01000000u &&
        (cpu->fpscr&0x03c00010u)==0x03000010u;
}
M8_FMCL_INLINE uint32_t m8_fmcl_compare(float a,float b) {
    return a==b?0x60000000u:a<b?0x80000000u:0x20000000u;
}
M8_FMCL_INLINE int m8_fmcl_sint(float value,int32_t *integer) {
    uint32_t magnitude=m8_fp_bits32(value)&0x7fffffffu;
    if(magnitude>0x4b000000u || (magnitude && magnitude<0x00800000u))return 0;
    *integer=(int32_t)value;return 1;
}
M8_FMCL_INLINE void m8_fmcl_private_conversion(void) {
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
    /* The last exact integer-to-float conversion resets this context. Every
     * later admitted operation preserves it or performs an exact zero op. */
    softfloat_roundingMode=softfloat_round_near_even;
    softfloat_detectTininess=softfloat_tininess_beforeRounding;
    softfloat_exceptionFlags=0;
#endif
}
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
#define M8_FMCL_BODY M8_FMCL_INLINE
#else
/* Keep native speculation behind the checked host-mode/status boundary. */
#define M8_FMCL_BODY static __attribute__((noinline))
#endif
#define M8_FMCL_OP(DEST,OP,A,B) do{if(!m8_fmol_op(OP,A,B,0,&(DEST)))return 0;}while(0)
#if defined(M8_AOT_FM_CALLBACK_2284)
M8_FMCL_BODY int m8_fmcl_math2284(m8_cpu *cpu,const m8_fmcb2284_context *context) {
    float s0=m8_fp_float(cpu->s[0]),s1=m8_fp_float(cpu->s[1]);
    float s13=m8_fp_float(cpu->s[13]),s14=m8_fp_float(cpu->s[14]);
    float s15=m8_fp_float(0x3fc70386u);
    uint32_t r1=cpu->r[1],r2=cpu->r[2],r3=cpu->r[3];
    M8_FMCL_OP(s1,M8_FP_MUL,s1,s15);M8_FMCL_OP(s0,M8_FP_MUL,s1,s0);
    uint32_t compare=m8_fmcl_compare(s0,0),apsr=compare;
    int converted=0;
    if(compare&0x80000000u) {
        s15=m8_fp_float(0xb44c02cdu);compare=m8_fmcl_compare(s0,s15);apsr=compare;
        if(!(compare&0x80000000u))goto finish;
        s15=m8_fp_float(0x3e22f983u);M8_FMCL_OP(s0,M8_FP_MUL,s0,s15);
        int32_t integer;if(!m8_fmcl_sint(s0,&integer) || integer<=-8388608)return 0;
        m8_cpu flags={0};flags.xpsr=(cpu->xpsr&0x0fffffffu)|apsr;
        r3=m8_sub_flags(&flags,(uint32_t)integer,1,0);apsr=flags.xpsr&0xf0000000u;
        s15=(float)(int32_t)r3;
    } else {
        s15=m8_fp_float(0x3e22f983u);M8_FMCL_OP(s0,M8_FP_MUL,s0,s15);
        int32_t integer;if(!m8_fmcl_sint(s0,&integer))return 0;
        s15=(float)integer;
    }
    s13=512.0f;r2=0x20005b80u;s14=1.0f;
    M8_FMCL_OP(s0,M8_FP_SUB,s0,s15);M8_FMCL_OP(s0,M8_FP_MUL,s0,s13);
    uint32_t magnitude=m8_fp_bits32(s0)&0x7fffffffu;
    if((m8_fp_bits32(s0)>>31 && magnitude) || magnitude>0x4b000000u ||
       (magnitude && magnitude<0x00800000u))return 0;
    r3=(uint32_t)s0&511u;r1=r2+r3*4u;s15=(float)r3;
    s13=m8_fp_float(m8_le_read(context->table+r3*4u,4));
    M8_FMCL_OP(s15,M8_FP_SUB,s0,s15);
    s0=m8_fp_float(m8_le_read(context->table+r3*4u+4u,4));
    M8_FMCL_OP(s14,M8_FP_SUB,s14,s15);
    M8_FMCL_OP(s15,M8_FP_MUL,s15,s0);M8_FMCL_OP(s0,M8_FP_MUL,s14,s13);
    M8_FMCL_OP(s0,M8_FP_ADD,s0,s15);converted=1;
finish:
    cpu->r[1]=r1;cpu->r[2]=r2;cpu->r[3]=r3;cpu->r[15]=cpu->r[14]&~1u;
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[1]=m8_fp_bits32(s1);
    cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);cpu->s[15]=m8_fp_bits32(s15);
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|apsr;
    if(converted)m8_fmcl_private_conversion();
#if defined(M8_FM_CALLBACK_LOCAL_TEST)
    m8_fmcl_counts[0]++;m8_fmcl_counts[2]+=!converted;
    if(converted)m8_fmcl_indices[r3]=1;
#endif
    return 1;
}
#endif
#if defined(M8_AOT_FM_CALLBACK_1FBC)
M8_FMCL_BODY int m8_fmcl_math1fbc(m8_cpu *cpu) {
    float s0=m8_fp_float(cpu->s[0]),s1=m8_fp_float(cpu->s[1]),s15=0.5f;
    M8_FMCL_OP(s1,M8_FP_MUL,s1,s15);M8_FMCL_OP(s0,M8_FP_MUL,s1,s0);
    uint32_t compare=m8_fmcl_compare(s0,0);
    M8_FMCL_OP(s15,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s0,s15);
    int32_t integer;if(!m8_fmcl_sint(s15,&integer))return 0;s15=(float)integer;
    M8_FMCL_OP(s0,M8_FP_SUB,s0,s15);M8_FMCL_OP(s0,M8_FP_ADD,s0,s0);
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[1]=m8_fp_bits32(s1);cpu->s[15]=m8_fp_bits32(s15);
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|compare;cpu->r[15]=cpu->r[14]&~1u;
    m8_fmcl_private_conversion();
#if defined(M8_FM_CALLBACK_LOCAL_TEST)
    m8_fmcl_counts[3]++;
#endif
    return 1;
}
#endif
#undef M8_FMCL_OP
#undef M8_FMCL_BODY
M8_FMCL_INLINE int m8_fmcl_host(uint64_t *saved) {
#if defined(M8_NATIVE_FP_NO_STICKY)
    (void)saved;return 1;
#elif defined(__wasm__) || defined(M8_WEB_FP_TEST)
    (void)saved;return 1;
#elif defined(__x86_64__)
    unsigned status=_mm_getcsr();*saved=status;
    return (status&0x0000ffc0u)==0x00009fc0u && (status&0x20u);
#elif defined(__aarch64__)
    uint64_t mode;__asm__ volatile("mrs %0, fpcr" : "=r"(mode));
    __asm__ volatile("mrs %0, fpsr" : "=r"(*saved));
    return (mode&UINT64_C(0x03c09f00))==UINT64_C(0x03000000) && (*saved&16u);
#else
    (void)saved;return 0;
#endif
}
M8_FMCL_INLINE void m8_fmcl_restore(uint64_t saved) {
#if defined(M8_NATIVE_FP_NO_STICKY)
    (void)saved;
#elif !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    _mm_setcsr((unsigned)saved);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpsr, %0" :: "r"(saved) : "memory");
#endif
#else
    (void)saved;
#endif
}
#if defined(M8_AOT_FM_CALLBACK_2284)
M8_FMCL_INLINE int m8_fmcb2284_try_local(m8_cpu *cpu,const m8_fmcb2284_context *context) {
    uint64_t saved=0;if(!m8_fmcl_cpu(cpu) || !m8_fmcl_host(&saved))return 0;
    int admitted=m8_fmcl_math2284(cpu,context);if(!admitted)m8_fmcl_restore(saved);return admitted;
}
M8_FMCL_INLINE void m8_fmcb2284_apply_local(m8_cpu *cpu,const m8_fmcb2284_context *context) {
    if(m8_fmcb2284_try_local(cpu,context))return;
#if defined(M8_FM_CALLBACK_LOCAL_TEST)
    m8_fmcl_counts[1]++;
#endif
    m8_fmcb2284_apply(cpu,context);
}
#define m8_fmcb2284_apply m8_fmcb2284_apply_local
#endif
#if defined(M8_AOT_FM_CALLBACK_1FBC)
M8_FMCL_INLINE int m8_fmcb1fbc_try_local(m8_cpu *cpu) {
    uint64_t saved=0;if(!m8_fmcl_cpu(cpu) || !m8_fmcl_host(&saved))return 0;
    int admitted=m8_fmcl_math1fbc(cpu);if(!admitted)m8_fmcl_restore(saved);return admitted;
}
M8_FMCL_INLINE void m8_fmcb1fbc_apply_local(m8_cpu *cpu) {
    if(m8_fmcb1fbc_try_local(cpu))return;
#if defined(M8_FM_CALLBACK_LOCAL_TEST)
    m8_fmcl_counts[4]++;
#endif
    m8_fmcb1fbc_apply(cpu);
}
#define m8_fmcb1fbc_apply m8_fmcb1fbc_apply_local
#endif
#undef M8_FMCL_INLINE
#endif
#endif
