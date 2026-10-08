/* Headless 6.5.2C stereo gain loops 0x955e..0x958a and 0x9c80..0x9cac.
 * Qualify both source and destination spans plus the gain word before effects.
 * Keep the left write before the right read, including exact buffer aliases.
 * Reject partial aliases, other FP modes, holes, wraps, and unsafe numbers.
 * The caller configures native FP. Success stops before the following branch.
 */
#ifndef M8_AOT_STEREO_MIX_H
#define M8_AOT_STEREO_MIX_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_STEREO_MIX)
#if defined(__GNUC__) || defined(__clang__)
#define M8_MIX_INLINE static inline __attribute__((always_inline))
#define M8_MIX_NOINLINE __attribute__((noinline))
#else
#define M8_MIX_INLINE static inline
#define M8_MIX_NOINLINE
#endif
/* CPU inputs: R1/R2/R3/R4/R5/R6/R7/R15, XPSR/FPSCR. R7 is used only
 * at 0x9c80; R1 only at 0x955e. Outputs: R1/R2/R4/R5/R7/R15,
 * S11/S14/S15, XPSR. R1/R7 retain their input in the other entry variant.
 * FPSCR retains its entry value; the caller collects sticky hardware flags.
 */
M8_MIX_INLINE int m8_mix_overlap(const uint8_t *a,uint32_t na,const uint8_t *b,uint32_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_MIX_INLINE int m8_mix_number(uint32_t bits,uint32_t maximum) {
    uint32_t magnitude=bits&0x7fffffffu;
    return !magnitude || (magnitude>=0x2f800000u && magnitude<=maximum);
}
M8_MIX_INLINE int m8_mix_numbers(const uint8_t *p,uint32_t size) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_MIX_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x2f800000u,0x2f800000u,0x2f800000u,0x2f800000u};
    const lanes range={0x18000000u,0x18000000u,0x18000000u,0x18000000u};
    lanes bad={0,0,0,0};uint32_t index=0;
    for (;index+16<=size;index+=16) {
        lanes words;memcpy(&words,p+index,16);words &= sign;
        bad |= (words!=0)&((words-minimum)>range);
    }
    if (bad[0]|bad[1]|bad[2]|bad[3]) return 0;
    for (;index<size;index+=4) if (!m8_mix_number(m8_le_read(p+index,4),0x47800000u)) return 0;
    return 1;
#else
    for (uint32_t index=0;index<size;index+=4)
        if (!m8_mix_number(m8_le_read(p+index,4),0x47800000u)) return 0;
    return 1;
#endif
}
M8_MIX_INLINE int m8_mix_buffers_numbers(uint8_t *const p[4],uint32_t size) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_MIX_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x2f800000u,0x2f800000u,0x2f800000u,0x2f800000u};
    const lanes range={0x18000000u,0x18000000u,0x18000000u,0x18000000u};
    lanes bad={0,0,0,0};uint32_t index=0;
    for (;index+16<=size;index+=16) {
        lanes a,b,c,d;
        memcpy(&a,p[0]+index,16);memcpy(&b,p[1]+index,16);
        memcpy(&c,p[2]+index,16);memcpy(&d,p[3]+index,16);
        a &= sign;b &= sign;c &= sign;d &= sign;
        bad |= (a!=0)&((a-minimum)>range);
        bad |= (b!=0)&((b-minimum)>range);
        bad |= (c!=0)&((c-minimum)>range);
        bad |= (d!=0)&((d-minimum)>range);
    }
    if (bad[0]|bad[1]|bad[2]|bad[3]) return 0;
    for (;index<size;index+=4) {
        for (unsigned i=0;i<4;i++)
            if (!m8_mix_number(m8_le_read(p[i]+index,4),0x47800000u)) return 0;
    }
    return 1;
#else
    for (unsigned i=0;i<4;i++) if (!m8_mix_numbers(p[i],size)) return 0;
    return 1;
#endif
}
M8_MIX_NOINLINE static int m8_stereo_mix_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    if (!mem || m8_mix_overlap((const uint8_t *)cpu,sizeof(*cpu),
                               (const uint8_t *)mem,sizeof(*mem)) ||
        (cpu->r[15]!=0x955eu && cpu->r[15]!=0x9c80u) ||
        (cpu->fpscr&0x03c00010u)!=0x03000010u ||
        (cpu->xpsr&0x0700fc00u)!=0x01000000u ||
        cpu->r[3]<=cpu->r[5] || cpu->r[3]-cpu->r[5]>256u ||
        cpu->r[6]>UINT32_MAX-67u) return 0;
    uint32_t size=cpu->r[3]-cpu->r[5];
    unsigned alternate=cpu->r[15]==0x9c80u;
    uint32_t addresses[]={cpu->r[5],cpu->r[4],cpu->r[alternate ? 7 : 2],
                          cpu->r[alternate ? 2 : 1]};
    uint32_t gain_address=cpu->r[6]+64u;
    if ((size|gain_address|addresses[0]|addresses[1]|addresses[2]|addresses[3])&3u) return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        uintptr_t host=(uintptr_t)regions[i]->host;
        if (host>UINTPTR_MAX-regions[i]->size ||
            m8_mix_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)cpu,sizeof(*cpu)) ||
            m8_mix_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
    }
    uint8_t *pointers[4];
    for (unsigned i=0;i<4;i++) {
        if (addresses[i]>UINT32_MAX-size) return 0;
        pointers[i]=m8_span_pointer(mem,addresses[i],size);
        if (!pointers[i] || m8_mix_overlap(pointers[i],size,(const uint8_t *)cpu,sizeof(*cpu)) ||
            m8_mix_overlap(pointers[i],size,(const uint8_t *)mem,sizeof(*mem))) return 0;
        for (unsigned j=0;j<i;j++) {
            if (pointers[i]!=pointers[j] && m8_mix_overlap(pointers[i],size,pointers[j],size)) return 0;
        }
    }
    uint8_t *gain_pointer=m8_pointer(mem,gain_address,4);
    if (!gain_pointer || m8_mix_overlap(gain_pointer,4,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_mix_overlap(gain_pointer,4,(const uint8_t *)mem,sizeof(*mem))) return 0;
    for (unsigned i=0;i<4;i++) {
        if (m8_mix_overlap(gain_pointer,4,pointers[i],size)) return 0;
    }
    uint32_t gain_bits=m8_le_read(gain_pointer,4);
    if (!m8_mix_number(gain_bits,0x43800000u) || !m8_mix_buffers_numbers(pointers,size)) return 0;
    /* Inputs are zero or normal in [2^-32,2^16], with gain in
     * [2^-32,2^8] or zero. A left cancellation against a nonzero destination
     * needs a product of at least 2^-33; its 24-by-24-bit exact product has
     * quantum at least 2^-81. Other left results are zero or at least 2^-64.
     * An aliased right source can therefore give a product as small as
     * 2^-113. Cancellation against an original destination is at least
     * 2^-81, and against an aliased left destination with an original source
     * is at least 2^-110. When both right inputs are the left result, the
     * factor 1+gain is zero or at least 2^-24. All nonzero results are normal.
     * Exact aliases have no cross-frame dependence. Both ordered FMAs remain
     * finite, so only sticky IXC needs collection at the enclosing call exit.
     */
    float gain=m8_fp_float(gain_bits),right_source=0.0f,right_result=0.0f;
    for (uint32_t offset=0;offset<size;offset+=4) {
        float left_source=m8_fp_float(m8_le_read(pointers[2]+offset,4));
        float left_destination=m8_fp_float(m8_le_read(pointers[0]+offset,4));
        float left_result=fmaf(left_source,gain,left_destination);
        m8_le_write(pointers[0]+offset,4,m8_fp_bits32(left_result));
        float right_destination=m8_fp_float(m8_le_read(pointers[1]+offset,4));
        right_source=m8_fp_float(m8_le_read(pointers[3]+offset,4));
        right_result=fmaf(right_source,gain,right_destination);
        m8_le_write(pointers[1]+offset,4,m8_fp_bits32(right_result));
    }
    cpu->r[4]+=size;cpu->r[5]+=size;cpu->r[2]+=size;
    if (alternate) {cpu->r[7]+=size;cpu->r[15]=0x9cacu;}
    else {cpu->r[1]+=size;cpu->r[15]=0x958au;}
    cpu->s[11]=m8_fp_bits32(right_source);cpu->s[14]=gain_bits;
    cpu->s[15]=m8_fp_bits32(right_result);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    return 1;
}
#undef M8_MIX_INLINE
#undef M8_MIX_NOINLINE
#else
static inline int m8_stereo_mix_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
