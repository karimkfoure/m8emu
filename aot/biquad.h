/* Bounded specialization of Headless 6.5.2C instructions 0x6f08..0x6ff8.
 * One iteration processes four samples in each of two disjoint buffers.
 * The caller configures native FP once and resumes at 0x6ffa after success.
 */
#ifndef M8_AOT_BIQUAD_H
#define M8_AOT_BIQUAD_H
#include "runtime.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC)
#if defined(__GNUC__) || defined(__clang__)
#define M8_BIQUAD_INLINE static inline __attribute__((always_inline))
#define M8_BIQUAD_NOINLINE __attribute__((noinline))
#else
#define M8_BIQUAD_INLINE static inline
#define M8_BIQUAD_NOINLINE
#endif
M8_BIQUAD_INLINE float m8_biquad_mul(m8_cpu *cpu,float a,float b) {
    return m8_fp_float(m8_fp_native32(cpu,M8_FP_MUL,m8_fp_bits32(a),m8_fp_bits32(b),0));
}
M8_BIQUAD_INLINE float m8_biquad_fma(m8_cpu *cpu,float a,float b,float d) {
    return m8_fp_float(m8_fp_native32(cpu,M8_FP_FMA,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
M8_BIQUAD_INLINE float m8_biquad_load(const uint8_t *p) {
    return m8_fp_float(m8_le_read(p,4));
}
M8_BIQUAD_INLINE void m8_biquad_store(uint8_t *p,float value) {
    m8_le_write(p,4,m8_fp_bits32(value));
}
/* Return zero before effects when a complete span cannot be qualified.
 * Host disjointness is required even when the guest addresses differ. */
M8_BIQUAD_NOINLINE static int m8_biquad_kernel(m8_cpu *cpu,m8_mem *mem) {
    uint32_t count=cpu->r[12];
    if (!count || count>UINT32_MAX/16u) return 0;
    uint32_t size=count*16u;
    if (cpu->r[1]<16u || cpu->r[2]<16u ||
        cpu->r[1]>UINT32_MAX-size || cpu->r[2]>UINT32_MAX-size) return 0;
    uint8_t *p1=m8_pointer(mem,cpu->r[1]-16u,size);
    uint8_t *p2=m8_pointer(mem,cpu->r[2]-16u,size);
    if (!p1 || !p2) return 0;
    uintptr_t h1=(uintptr_t)p1,h2=(uintptr_t)p2;
    if ((h1<=h2 ? h2-h1 : h1-h2)<size) return 0;
    uint8_t *restrict left=p1;
    uint8_t *restrict right=p2;
    const float s0=m8_fp_float(cpu->s[0]),s1=m8_fp_float(cpu->s[1]);
    const float s2=m8_fp_float(cpu->s[2]),s3=m8_fp_float(cpu->s[3]),s4=m8_fp_float(cpu->s[4]);
    float s5=m8_fp_float(cpu->s[5]),s7=m8_fp_float(cpu->s[7]),s8=m8_fp_float(cpu->s[8]);
    float s9=m8_fp_float(cpu->s[9]),s10=m8_fp_float(cpu->s[10]),s11=m8_fp_float(cpu->s[11]);
    float s14=m8_fp_float(cpu->s[14]),s15=m8_fp_float(cpu->s[15]);
    float s12,s13,s16,s17,s18,s19,s20;
    for (uint32_t i=0;i<count;i++,left+=16,right+=16) {
        s12=m8_biquad_mul(cpu,s4,s5);          /* 0x6f08 */
        s18=m8_biquad_load(left);
        s11=m8_biquad_mul(cpu,s4,s11);
        s17=m8_biquad_load(right);
        s16=m8_biquad_mul(cpu,s4,s14);
        s12=m8_biquad_fma(cpu,s2,s8,s12);
        s11=m8_biquad_fma(cpu,s2,s7,s11);
        s13=m8_biquad_mul(cpu,s4,s15);
        s16=m8_biquad_fma(cpu,s2,s9,s16);
        s13=m8_biquad_fma(cpu,s2,s10,s13);
        s12=m8_biquad_fma(cpu,s1,s10,s12);
        s11=m8_biquad_fma(cpu,s1,s9,s11);
        s12=m8_biquad_fma(cpu,s3,s15,s12);
        s11=m8_biquad_fma(cpu,s3,s14,s11);
        s12=m8_biquad_fma(cpu,s0,s17,s12);
        s11=m8_biquad_fma(cpu,s0,s18,s11);
        m8_biquad_store(left,s11);
        m8_biquad_store(right,s12);
        s20=m8_biquad_load(right+4);
        s19=m8_biquad_load(left+4);
        s13=m8_biquad_fma(cpu,s0,s20,s13);
        s16=m8_biquad_fma(cpu,s0,s19,s16);
        s10=m8_biquad_mul(cpu,s1,s19);
        s5=m8_biquad_mul(cpu,s1,s20);
        s13=m8_biquad_fma(cpu,s1,s17,s13);
        s16=m8_biquad_fma(cpu,s1,s18,s16);
        s13=m8_biquad_fma(cpu,s3,s12,s13);
        s16=m8_biquad_fma(cpu,s3,s11,s16);
        m8_biquad_store(left+4,s16);
        m8_biquad_store(right+4,s13);
        s7=m8_biquad_load(left+8);
        s8=m8_biquad_load(right+8);
        s10=m8_biquad_fma(cpu,s0,s7,s10);
        s5=m8_biquad_fma(cpu,s0,s8,s5);
        s14=m8_biquad_mul(cpu,s1,s7);
        s15=m8_biquad_mul(cpu,s1,s8);
        s10=m8_biquad_fma(cpu,s2,s18,s10);
        s5=m8_biquad_fma(cpu,s2,s17,s5);
        s10=m8_biquad_fma(cpu,s4,s11,s10);
        s5=m8_biquad_fma(cpu,s4,s12,s5);
        s11=s10;
        s5=m8_biquad_fma(cpu,s3,s13,s5);
        s11=m8_biquad_fma(cpu,s3,s16,s11);
        m8_biquad_store(left+8,s11);
        m8_biquad_store(right+8,s5);
        s10=m8_biquad_load(right+12);
        s9=m8_biquad_load(left+12);
        s15=m8_biquad_fma(cpu,s0,s10,s15);
        s14=m8_biquad_fma(cpu,s0,s9,s14);
        s15=m8_biquad_fma(cpu,s2,s20,s15);
        s14=m8_biquad_fma(cpu,s2,s19,s14);
        s15=m8_biquad_fma(cpu,s4,s13,s15);
        s14=m8_biquad_fma(cpu,s4,s16,s14);
        s15=m8_biquad_fma(cpu,s3,s5,s15);
        s14=m8_biquad_fma(cpu,s3,s11,s14);
        m8_biquad_store(left+12,s14);
        m8_biquad_store(right+12,s15);         /* 0x6ff4 */
    }
    cpu->s[5]=m8_fp_bits32(s5);
    cpu->s[7]=m8_fp_bits32(s7); cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9); cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11); cpu->s[12]=m8_fp_bits32(s12);
    cpu->s[13]=m8_fp_bits32(s13); cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15); cpu->s[16]=m8_fp_bits32(s16);
    cpu->s[17]=m8_fp_bits32(s17); cpu->s[18]=m8_fp_bits32(s18);
    cpu->s[19]=m8_fp_bits32(s19); cpu->s[20]=m8_fp_bits32(s20);
    cpu->r[1]+=size; cpu->r[2]+=size; cpu->r[12]=0;
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|0x60000000u;
    return 1;
}
#undef M8_BIQUAD_INLINE
#undef M8_BIQUAD_NOINLINE
#endif
#endif
