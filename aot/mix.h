/* Headless 6.5.2C fixed smoothing block 0xd2f6..0xd384.
 * The caller configures native FP and chooses 0xcc28 or 0xd388 after success,
 * according to final r3. The kernel leaves PC unchanged.
 */
#ifndef M8_AOT_MIX_H
#define M8_AOT_MIX_H
#include "runtime.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC)
#if defined(__GNUC__) || defined(__clang__)
#define M8_MIX_INLINE static inline __attribute__((always_inline))
#else
#define M8_MIX_INLINE static inline
#endif
M8_MIX_INLINE float m8_mix_op(m8_cpu *c,int op,float a,float b,float d) {
    return m8_fp_float(m8_fp_native32(c,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_mix_kernel(m8_cpu *cpu,m8_mem *mem) {
    if (cpu->r[4]>UINT32_MAX-0xccu || cpu->r[13]>UINT32_MAX-0x14u) return 0;
    uint8_t *fields=m8_pointer(mem,cpu->r[4]+0x8cu,0x40u);
    uint8_t *scratch=m8_pointer(mem,cpu->r[13]+0x10u,4u);
    if (!fields || !scratch) return 0;
    uintptr_t f=(uintptr_t)fields,t=(uintptr_t)scratch,c=(uintptr_t)cpu;
    if ((f<=t ? t-f<0x40u : f-t<4u) ||
        (f<=c ? c-f<0x40u : f-c<sizeof(*cpu)) ||
        (t<=c ? c-t<4u : t-c<sizeof(*cpu))) return 0;
    uint8_t *restrict object=fields;
    m8_cpu *restrict state=cpu;
    float s5=m8_fp_float(state->s[5]),s17=m8_fp_float(state->s[17]);
    float s18=m8_fp_float(state->s[18]),s31=m8_fp_float(state->s[31]);
    float s15=m8_fp_float(m8_le_read(object+0x3c,4));
    float s11=m8_fp_float(m8_le_read(object+0x28,4));
    float s13=state->r[3]==0 ? s31 : s17;
    s13=m8_mix_op(state,M8_FP_SUB,s13,s15,0);
    float s8=m8_fp_float(m8_le_read(object+0x0c,4));
    float s6=m8_fp_float(m8_le_read(object+0x2c,4));
    s15=m8_mix_op(state,M8_FP_FMA,s13,s18,s15);
    float s9=m8_fp_float(m8_le_read(object+0x30,4));
    s8=m8_mix_op(state,M8_FP_SUB,s8,s11,0);
    float s14=m8_fp_float(m8_le_read(object+0x38,4));
    float s10=m8_fp_float(m8_le_read(object+0x34,4));
    float s7=m8_fp_float(m8_le_read(object+0x18,4));
    s11=m8_mix_op(state,M8_FP_FMA,s8,s18,s11);
    float s2=m8_fp_float(m8_le_read(object+0x20,4));
    float s3=m8_fp_float(m8_le_read(object+0x1c,4));
    s7=m8_mix_op(state,M8_FP_SUB,s7,s9,0);
    float s4=m8_mix_op(state,M8_FP_SUB,s17,s15,0);
    m8_le_write(object+0x3c,4,m8_fp_bits32(s15));
    s8=m8_mix_op(state,M8_FP_SUB,s15,s6,0);
    uint32_t r3=m8_le_read(scratch,4);
    s2=m8_mix_op(state,M8_FP_SUB,s2,s14,0);
    s13=m8_fp_float(m8_le_read(object+4,4));
    s3=m8_mix_op(state,M8_FP_SUB,s3,s10,0);
    float s12=m8_fp_float(m8_le_read(object,4));
    s8=m8_mix_op(state,M8_FP_FMA,s4,s5,s8);
    m8_le_write(object+0x28,4,m8_fp_bits32(s11));
    s15=s6;
    s9=m8_mix_op(state,M8_FP_FMA,s7,s18,s9);
    s14=m8_mix_op(state,M8_FP_FMA,s2,s18,s14);
    s10=m8_mix_op(state,M8_FP_FMA,s3,s18,s10);
    s15=m8_mix_op(state,M8_FP_FMA,s8,s18,s15);
    m8_le_write(object+0x30,4,m8_fp_bits32(s9));
    m8_le_write(object+0x38,4,m8_fp_bits32(s14));
    m8_le_write(object+0x34,4,m8_fp_bits32(s10));
    m8_le_write(object+0x2c,4,m8_fp_bits32(s15));
    state->s[2]=m8_fp_bits32(s2);state->s[3]=m8_fp_bits32(s3);
    state->s[4]=m8_fp_bits32(s4);state->s[6]=m8_fp_bits32(s6);
    state->s[7]=m8_fp_bits32(s7);state->s[8]=m8_fp_bits32(s8);
    state->s[9]=m8_fp_bits32(s9);state->s[10]=m8_fp_bits32(s10);
    state->s[11]=m8_fp_bits32(s11);state->s[12]=m8_fp_bits32(s12);
    state->s[13]=m8_fp_bits32(s13);state->s[14]=m8_fp_bits32(s14);
    state->s[15]=m8_fp_bits32(s15);state->r[3]=r3;
    state->xpsr=(state->xpsr & 0x0fffffffu)|0x20000000u|(r3 & 0x80000000u)|(!r3 ? 0x40000000u : 0u);
    return 1;
}
#undef M8_MIX_INLINE
#endif
#endif
