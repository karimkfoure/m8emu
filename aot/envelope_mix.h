/* Headless 6.5.2C constant-step envelope mixer, 0x7f7c/0x80b6.
 * Only a zero envelope step is accepted. Qualify every span, alias, and
 * numeric operand before effects. Reject writes to executable backing.
 * Multiply and add remain separate.
 * Native FP is active on entry. Canonical/fixed callers must first qualify
 * the complete public memory descriptor; callees inherit stable metadata.
 */
#ifndef M8_AOT_ENVELOPE_MIX_H
#define M8_AOT_ENVELOPE_MIX_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_FIXED_MEMORY)
#include "canonical_memory.h"
#endif
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_ENVELOPE_MIX)
#if defined(__GNUC__) || defined(__clang__)
#define M8_ENVELOPE_INLINE static inline __attribute__((always_inline))
#define M8_ENVELOPE_NOINLINE __attribute__((noinline))
#else
#define M8_ENVELOPE_INLINE static inline
#define M8_ENVELOPE_NOINLINE
#endif
/* Inputs: R0/R1/R4/R5/R7/R12/R15, S12, XPSR/FPSCR.
 * R7 is read only for 0x7f7c; R5 only for 0x80b6.
 * Outputs: R0/R1/R12/R15, S11/S13/S15, XPSR/FPSCR.
 * Success exits at 0x80a6 or 0x83b4. R12 becomes 64. NZCV becomes 0110
 * in both status words. FPSCR sticky bits retain their entry values.
 */
M8_ENVELOPE_INLINE int m8_envelope_overlap(const uint8_t *a,uint32_t na,const uint8_t *b,uint32_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_ENVELOPE_INLINE int m8_envelope_number(uint32_t bits,uint32_t minimum,uint32_t maximum) {
    uint32_t magnitude=bits&0x7fffffffu;
    return !magnitude || (magnitude>=minimum && magnitude<=maximum);
}
M8_ENVELOPE_INLINE int m8_envelope_peak(uint32_t bits) {
    return bits<=0x47800000u && (!bits || bits>=0x00800000u);
}
M8_ENVELOPE_INLINE int m8_envelope_buffer_numbers(uint8_t *a,uint8_t *b,uint32_t size) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_ENVELOPE_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x1f800000u,0x1f800000u,0x1f800000u,0x1f800000u};
    const lanes range={0x28000000u,0x28000000u,0x28000000u,0x28000000u};
    lanes bad={0,0,0,0};
    for (uint32_t index=0;index<size;index+=16) {
        lanes x,y;memcpy(&x,a+index,16);memcpy(&y,b+index,16);
        x &= sign;y &= sign;
        bad |= (x!=0)&((x-minimum)>range);
        bad |= (y!=0)&((y-minimum)>range);
    }
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
#else
    for (uint32_t index=0;index<size;index+=4)
        if (!m8_envelope_number(m8_le_read(a+index,4),0x1f800000u,0x47800000u) ||
            !m8_envelope_number(m8_le_read(b+index,4),0x1f800000u,0x47800000u)) return 0;
    return 1;
#endif
}
M8_ENVELOPE_INLINE uint8_t *m8_envelope_pointer(m8_mem *mem,uint32_t address,uint32_t size) {
#if defined(M8_AOT_FIXED_MEMORY)
    return m8_canonical_offset_pointer((uintptr_t)mem->itcm.host,address,size);
#else
    return m8_span_pointer(mem,address,size);
#endif
}
M8_ENVELOPE_NOINLINE static int m8_envelope_mix_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    if (!mem || m8_envelope_overlap((const uint8_t *)cpu,sizeof(*cpu),(const uint8_t *)mem,sizeof(*mem)) ||
        (cpu->r[15]!=0x7f7cu && cpu->r[15]!=0x80b6u) ||
        (cpu->s[12]&0x7fffffffu) || cpu->r[12]>=64u || (cpu->r[12]&7u) ||
#if defined(M8_NATIVE_FP_NO_STICKY)
        /* Result-only calls preserve IXC; admission cannot depend on it. */
        (cpu->fpscr&0x03c00000u)!=0x03000000u ||
#else
        (cpu->fpscr&0x03c00010u)!=0x03000010u ||
#endif
        (cpu->xpsr&0x0700fc00u)!=0x01000000u) return 0;
    uint32_t size=(64u-cpu->r[12])*4u;
    uint32_t peak_address=cpu->r[cpu->r[15]==0x7f7cu ? 7 : 5];
    if ((cpu->r[0]|cpu->r[1]|cpu->r[4]|peak_address)&3u ||
        cpu->r[0]>UINT32_MAX-size-4u || cpu->r[1]>UINT32_MAX-size-4u ||
        cpu->r[4]>UINT32_MAX-4u || peak_address>UINT32_MAX-4u) return 0;
#if defined(M8_AOT_FIXED_MEMORY)
    if (m8_envelope_overlap(mem->itcm.host,0x60800000u,(const uint8_t *)cpu,sizeof(*cpu))) return 0;
#else
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        if (m8_envelope_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)cpu,sizeof(*cpu))) return 0;
        if ((uintptr_t)regions[i]->host>UINTPTR_MAX-regions[i]->size ||
            m8_envelope_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
    }
#endif
    uint8_t *source=m8_envelope_pointer(mem,cpu->r[0]+4u,size);
    uint8_t *destination=m8_envelope_pointer(mem,cpu->r[1]+4u,size);
    uint8_t *gain_pointer=m8_envelope_pointer(mem,cpu->r[4],4);
    uint8_t *peak_pointer=m8_envelope_pointer(mem,peak_address,4);
    if (!source || !destination || !gain_pointer || !peak_pointer ||
        (source!=destination && m8_envelope_overlap(source,size,destination,size)) ||
        m8_envelope_overlap(gain_pointer,4,source,size) ||
        m8_envelope_overlap(gain_pointer,4,destination,size) ||
        m8_envelope_overlap(peak_pointer,4,source,size) ||
        m8_envelope_overlap(peak_pointer,4,destination,size) ||
        m8_envelope_overlap(gain_pointer,4,peak_pointer,4) ||
        m8_envelope_overlap(destination,size,mem->itcm.host,mem->itcm.size) ||
        m8_envelope_overlap(destination,size,mem->flash.host,mem->flash.size) ||
        m8_envelope_overlap(peak_pointer,4,mem->itcm.host,mem->itcm.size) ||
        m8_envelope_overlap(peak_pointer,4,mem->flash.host,mem->flash.size)) return 0;
    uint32_t gain_bits=m8_le_read(gain_pointer,4),peak_bits=m8_le_read(peak_pointer,4);
    if (!m8_envelope_number(gain_bits,0x2f800000u,0x43800000u) || !m8_envelope_peak(peak_bits) ||
        !m8_envelope_buffer_numbers(source,destination,size)) return 0;
    /* Every nonzero product is at least 2^-96. Subtraction from an original
     * normal destination stays at least 2^-113 or becomes exact zero. All
     * results are finite and normal. IXC is already sticky; no other sticky
     * flag can change. Peak operands are nonnegative, including positive zero.
     */
    float gain=m8_fp_float(gain_bits),peak=m8_fp_float(peak_bits),previous_peak=peak,last_sample=0.0f;
    for (uint32_t offset=0;offset<size;offset+=4) {
        float sample=m8_fp_float(m8_le_read(source+offset,4));
        last_sample=sample;
        float previous=m8_fp_float(m8_le_read(destination+offset,4));
        float product=sample*gain;
        float sum=previous+product;
        m8_le_write(destination+offset,4,m8_fp_bits32(sum));
        float magnitude=fabsf(product);previous_peak=peak;
        if (magnitude>peak) peak=magnitude;
    }
    m8_le_write(peak_pointer,4,m8_fp_bits32(peak));
    cpu->r[0]+=size;cpu->r[1]+=size;cpu->r[12]=64u;
    cpu->r[15]=cpu->r[15]==0x7f7cu ? 0x80a6u : 0x83b4u;
    cpu->s[11]=m8_fp_bits32(last_sample);cpu->s[13]=m8_fp_bits32(previous_peak);cpu->s[15]=m8_fp_bits32(peak);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|0x60000000u;
    return 1;
}
#undef M8_ENVELOPE_INLINE
#undef M8_ENVELOPE_NOINLINE
#else
static inline int m8_envelope_mix_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
