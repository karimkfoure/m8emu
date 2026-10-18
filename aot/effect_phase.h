/* Headless 6.5.2C effect phase/index window 0xbea8..0xbfb6.
 * Keep the original four FMA chains, truncations, and left circular-index
 * branches. Read the mutable depth word only after all map checks.
 * The caller configures native FP; rejection leaves CPU/RAM unchanged.
 */
#ifndef M8_AOT_EFFECT_PHASE_H
#define M8_AOT_EFFECT_PHASE_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_FIXED_MEMORY)
#include "canonical_memory.h"
#endif
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_EFFECT_PHASE)
#if defined(__GNUC__) || defined(__clang__)
#define M8_PHASE_INLINE static inline __attribute__((always_inline))
#if defined(M8_AOT_NOINLINE_EFFECT_PHASE)
#define M8_PHASE_KERNEL static __attribute__((noinline))
#else
#define M8_PHASE_KERNEL M8_PHASE_INLINE
#endif
#else
#define M8_PHASE_INLINE static inline
#define M8_PHASE_KERNEL static inline
#endif
/* Inputs: R4/R5/R8/R15, S10/S12/S13/S14/S15/S16/S18, XPSR/FPSCR.
 * Outputs: R0/R1/R2/R3/R11/R12/R14/R15, S6..S15, XPSR.
 * FPSCR retains its entry value; IXC is already sticky under the guard.
 */
M8_PHASE_INLINE int m8_phase_overlap(const uint8_t *a,uint32_t na,const uint8_t *b,uint32_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_PHASE_INLINE int m8_phase_number(uint32_t word,uint32_t maximum) {
    uint32_t magnitude=word&0x7fffffffu;
    return !magnitude || (magnitude>=0x2f800000u && magnitude<=maximum);
}
M8_PHASE_INLINE int m8_phase_wave_numbers(const m8_cpu *cpu) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_PHASE_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    lanes words={cpu->s[10],cpu->s[13],cpu->s[14],cpu->s[15]};
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x2f800000u,0x2f800000u,0x2f800000u,0x2f800000u};
    const lanes range={0x10800000u,0x10800000u,0x10800000u,0x10800000u};
    words &= sign;lanes bad=(words!=0)&((words-minimum)>range);
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
#else
    return m8_phase_number(cpu->s[10],0x40000000u) &&
           m8_phase_number(cpu->s[13],0x40000000u) &&
           m8_phase_number(cpu->s[14],0x40000000u) &&
           m8_phase_number(cpu->s[15],0x40000000u);
#endif
}
M8_PHASE_INLINE uint32_t m8_phase_sub_flags(uint32_t a,uint32_t b) {
    uint32_t result=a-b;
    return (result&0x80000000u)|(!result ? 0x40000000u : 0)|
           (a>=b ? 0x20000000u : 0)|(((a^b)&(a^result)&0x80000000u)>>3);
}
M8_PHASE_INLINE uint32_t m8_phase_add_flags(uint32_t a,uint32_t b) {
    uint32_t result=a+b;
    return (result&0x80000000u)|(!result ? 0x40000000u : 0)|
           (result<a ? 0x20000000u : 0)|((~(a^b)&(a^result)&0x80000000u)>>3);
}
M8_PHASE_KERNEL int m8_effect_phase_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    if (!mem || m8_phase_overlap((const uint8_t *)cpu,sizeof(*cpu),
                                 (const uint8_t *)mem,sizeof(*mem)) ||
        cpu->r[15]!=0xbea8u || (cpu->xpsr&0x0700fc00u)!=0x01000000u ||
        (cpu->fpscr&0x03c00010u)!=0x03000010u ||
        cpu->r[4]<3u || cpu->r[4]>65535u || cpu->r[8]>=cpu->r[4] ||
        cpu->r[5]>UINT32_MAX-0xf9fu || (cpu->r[5]&3u) ||
        cpu->s[16]!=0x3f000000u ||
        cpu->s[12]!=m8_fp_bits32((float)cpu->r[8]) ||
        cpu->s[18]!=m8_fp_bits32(-(float)(cpu->r[4]-2u))) return 0;
    /* Fixed builds inherit the complete descriptor qualification
     * from the public generated wrapper. Direct callees keep that descriptor
     * stable. Standalone callers must perform the same check before entry.
     * Generic builds retain the host-wrap checks before pointer construction.
     */
#if defined(M8_AOT_FIXED_MEMORY)
    uint8_t *depth_pointer=m8_canonical_offset_pointer((uintptr_t)mem->itcm.host,cpu->r[5]+0xf9cu,4);
#else
    for (unsigned i=0;i<4;i++) {
        const m8_region *r=i==0 ? &mem->itcm : i==1 ? &mem->dtcm : i==2 ? &mem->ocram : &mem->flash;
        if ((uintptr_t)r->host>UINTPTR_MAX-r->size) return 0;
    }
    uint8_t *depth_pointer=m8_pointer(mem,cpu->r[5]+0xf9cu,4);
#endif
    if (!depth_pointer || m8_phase_overlap(depth_pointer,4,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_phase_overlap(depth_pointer,4,(const uint8_t *)mem,sizeof(*mem))) return 0;
    uint32_t depth_bits=m8_le_read(depth_pointer,4);
    if (!m8_phase_number(depth_bits,0x3f800000u) || !m8_phase_wave_numbers(cpu)) return 0;
    /* A nonzero first-stage cancellation is at least 2^-24. Cancellation
     * against 0.5 in the second stage needs a product of at least 0.25;
     * its 24-by-24-bit product has quantum at least 2^-50. The third stage
     * multiplies by an integer with magnitude at least one and adds an
     * integer base. All nonzero results stay normal. The position magnitude
     * is below 2^20, so truncations and integer-to-float conversions are
     * defined and cannot add IOC, OFC, UFC, DZC, or IDC.
     */
    float depth=m8_fp_float(depth_bits),base=m8_fp_float(cpu->s[12]),length=m8_fp_float(cpu->s[18]);
    float a=m8_fp_float(cpu->s[10]),b=m8_fp_float(cpu->s[13]);
    float c=m8_fp_float(cpu->s[15]),d=m8_fp_float(cpu->s[14]);
    float s6=fmaf(fabsf(a),4.0f,-1.0f);
    float s7=fmaf(fabsf(b),4.0f,-1.0f);
    float s10=fmaf(fabsf(c),4.0f,-1.0f);
    float s11=fmaf(fabsf(d),4.0f,-1.0f);
    float s13=fmaf(depth,s7,0.5f);
    float s9=fmaf(depth,s10,0.5f);
    float s8=fmaf(s6,depth,0.5f);
    s10=fmaf(depth,s11,0.5f);
    s11=fmaf(length,s13,base);
    float s14=fmaf(length,s8,base);
    float s12=fmaf(length,s10,base);
    s13=fmaf(length,s9,base);
    s9=s11-0.5f;s8=s14-0.5f;
    float s15=s12-0.5f;s10=s13-0.5f;
    uint32_t r3=(uint32_t)(int32_t)s8,r2=(uint32_t)(int32_t)s9;
    uint32_t r1=(uint32_t)(int32_t)s10,r0=(uint32_t)(int32_t)s15;
    s8=(float)(int32_t)r3;s9=(float)(int32_t)r2;
    s10=(float)(int32_t)r1;s15=(float)(int32_t)r0;
    uint32_t count=cpu->r[4],r12=r3-1u,r11,r14;
    s14=s14-s8;s11=s11-s9;s13=s13-s10;s12=s12-s15;
    if ((int32_t)r12<0) r12+=count;
    else if ((int32_t)r12>=(int32_t)count) r12-=count;
    r12&=65535u;
    if ((int32_t)r3<0) {
        r11=(count+r3)&65535u;r14=r3+1u;
        if (r14) r14=(r14+count)&65535u;
        else {if ((int32_t)count<=(int32_t)r14) r14-=count;r14&=65535u;}
    } else {
        r11=((int32_t)count>(int32_t)r3 ? r3 : r3-count)&65535u;
        r14=r3+1u;
        if ((int32_t)count<=(int32_t)r14) r14-=count;
        r14&=65535u;
    }
    uint32_t flags=m8_phase_add_flags(r3,2u);r3+=2u;
    if ((int32_t)r3<0) r3+=count;
    else {flags=m8_phase_sub_flags(r3,count);if ((int32_t)r3>=(int32_t)count) r3-=count;}
    r3&=65535u;
    cpu->r[0]=r0;cpu->r[1]=r1;cpu->r[2]=r2;cpu->r[3]=r3;
    cpu->r[11]=r11;cpu->r[12]=r12;cpu->r[14]=r14;cpu->r[15]=0xbfb6u;
    cpu->s[6]=m8_fp_bits32(s6);cpu->s[7]=m8_fp_bits32(s7);cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);cpu->s[11]=m8_fp_bits32(s11);
    cpu->s[12]=m8_fp_bits32(s12);cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);cpu->xpsr=(cpu->xpsr&0x0fffffffu)|flags;
    return 1;
}
#undef M8_PHASE_INLINE
#undef M8_PHASE_KERNEL
#else
static inline int m8_effect_phase_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
