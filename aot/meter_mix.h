/* Headless 6.5.2C gain/meter loops 0x9a1c..0x9a54 and 0x9c08..0x9c48.
 * Qualify complete spans, stable gain, numeric ranges, and aliases first.
 * Keep each loop's original memory order, including complete buffer aliases.
 * The caller configures native FP. Success stops before the following branch.
 */
#ifndef M8_AOT_METER_MIX_H
#define M8_AOT_METER_MIX_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_FIXED_MEMORY)
#include "canonical_memory.h"
#endif
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_METER_MIX)
#if defined(__GNUC__) || defined(__clang__)
#define M8_METER_INLINE static inline __attribute__((always_inline))
#define M8_METER_NOINLINE __attribute__((noinline))
#else
#define M8_METER_INLINE static inline
#define M8_METER_NOINLINE
#endif
/* 0x9a1c inputs: R1/R2/R3/R6/R15, S16/S17, XPSR/FPSCR.
 * Outputs: R2/R3/R15, S14/S15/S16/S17, XPSR.
 * 0x9c08 inputs: R2/R3/R4/R5/R6/R7/R15, S12/S13, XPSR/FPSCR.
 * Outputs: R2/R4/R5/R7/R15, S10/S11/S12/S13/S14/S15, XPSR.
 * FPSCR retains its entry value; the caller collects sticky hardware flags.
 */
M8_METER_INLINE int m8_meter_overlap(const uint8_t *a,uint32_t na,const uint8_t *b,uint32_t nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_METER_INLINE int m8_meter_number(uint32_t bits,uint32_t maximum) {
    uint32_t magnitude=bits&0x7fffffffu;
    return !magnitude || (magnitude>=0x2f800000u && magnitude<=maximum);
}
M8_METER_INLINE int m8_meter_peak(uint32_t bits) {
    return bits<=0x47800000u && (!bits || bits>=0x2f800000u);
}
M8_METER_INLINE int m8_meter_buffers_numbers(uint8_t *const p[4],uint32_t size,unsigned count) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_METER_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x2f800000u,0x2f800000u,0x2f800000u,0x2f800000u};
    const lanes range={0x18000000u,0x18000000u,0x18000000u,0x18000000u};
    lanes bad={0,0,0,0};uint32_t index=0;
    for (;index+16<=size;index+=16) {
        lanes a,b;memcpy(&a,p[0]+index,16);memcpy(&b,p[1]+index,16);
        a &= sign;b &= sign;
        bad |= (a!=0)&((a-minimum)>range);
        bad |= (b!=0)&((b-minimum)>range);
        if (count==4) {
            lanes c,d;memcpy(&c,p[2]+index,16);memcpy(&d,p[3]+index,16);
            c &= sign;d &= sign;
            bad |= (c!=0)&((c-minimum)>range);
            bad |= (d!=0)&((d-minimum)>range);
        }
    }
    if (bad[0]|bad[1]|bad[2]|bad[3]) return 0;
    for (;index<size;index+=4)
        for (unsigned i=0;i<count;i++) if (!m8_meter_number(m8_le_read(p[i]+index,4),0x47800000u)) return 0;
    return 1;
#else
    for (unsigned i=0;i<count;i++)
        for (uint32_t index=0;index<size;index+=4)
            if (!m8_meter_number(m8_le_read(p[i]+index,4),0x47800000u)) return 0;
    return 1;
#endif
}
M8_METER_NOINLINE static int m8_meter_mix_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    if (!mem || m8_meter_overlap((const uint8_t *)cpu,sizeof(*cpu),(const uint8_t *)mem,sizeof(*mem)) ||
        (cpu->r[15]!=0x9a1cu && cpu->r[15]!=0x9c08u) ||
#if defined(M8_NATIVE_FP_NO_STICKY)
        /* Result-only calls preserve IXC; admission cannot depend on it. */
        (cpu->fpscr&0x03c00000u)!=0x03000000u ||
#else
        (cpu->fpscr&0x03c00010u)!=0x03000010u ||
#endif
        (cpu->xpsr&0x0700fc00u)!=0x01000000u || cpu->r[6]>UINT32_MAX-67u) return 0;
    unsigned scale=cpu->r[15]==0x9a1cu;
    uint32_t first=cpu->r[scale ? 3 : 7],end=cpu->r[scale ? 1 : 3];
    if (end<=first || end-first>256u) return 0;
    uint32_t size=end-first;
    uint32_t addresses[4]={cpu->r[scale ? 3 : 5],cpu->r[scale ? 2 : 4],scale ? 0 : cpu->r[7],scale ? 0 : cpu->r[2]};
    uint32_t gain_address=cpu->r[6]+(scale ? 60u : 64u);
    unsigned count=scale ? 2 : 4,left_peak=scale ? 17 : 12,right_peak=scale ? 16 : 13;
    if ((size|gain_address|addresses[0]|addresses[1]|(!scale ? addresses[2]|addresses[3] : 0))&3u ||
        !m8_meter_peak(cpu->s[left_peak]) || !m8_meter_peak(cpu->s[right_peak])) return 0;
#if defined(M8_AOT_FIXED_MEMORY)
    if (m8_meter_overlap(mem->itcm.host,0x60800000u,(const uint8_t *)cpu,sizeof(*cpu))) return 0;
#else
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        if (m8_meter_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)cpu,sizeof(*cpu))) return 0;
        if ((uintptr_t)regions[i]->host>UINTPTR_MAX-regions[i]->size ||
            m8_meter_overlap(regions[i]->host,regions[i]->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
    }
#endif
    /* Fixed builds inherit the complete public-wrapper descriptor
     * proof. Direct callees keep metadata stable; standalone callers must
     * qualify it before entry. Every selected span and alias is still checked.
     */
    uint8_t *p[4];
    for (unsigned i=0;i<count;i++) {
        if (addresses[i]>UINT32_MAX-size) return 0;
#if defined(M8_AOT_FIXED_MEMORY)
        p[i]=m8_canonical_offset_pointer((uintptr_t)mem->itcm.host,addresses[i],size);
#else
        p[i]=m8_span_pointer(mem,addresses[i],size);
#endif
        if (!p[i]) return 0;
        for (unsigned j=0;j<i;j++)
            if (p[i]!=p[j] && m8_meter_overlap(p[i],size,p[j],size)) return 0;
    }
    uint8_t *gain_pointer;
#if defined(M8_AOT_FIXED_MEMORY)
    gain_pointer=m8_canonical_offset_pointer((uintptr_t)mem->itcm.host,gain_address,4);
#else
    gain_pointer=m8_pointer(mem,gain_address,4);
#endif
    if (!gain_pointer) return 0;
    for (unsigned i=0;i<count;i++) if (m8_meter_overlap(gain_pointer,4,p[i],size)) return 0;
    uint32_t gain_bits=m8_le_read(gain_pointer,4);
    if (!m8_meter_number(gain_bits,0x43800000u) || !m8_meter_buffers_numbers(p,size,count)) return 0;
    /* Two in-place multiplications give a nonzero result at least 2^-96.
     * In the add loop, both sources are read before the left store. Products
     * are at least 2^-64; cancellation against a normal original destination
     * stays above 2^-81. An aliased right destination can contain the left
     * sum; cancellation against it stays above 2^-110. All results are finite
     * and normal. Peak operands are nonnegative, including positive zero,
     * so ordered C comparisons preserve VMAXNM without NaN or zero ambiguity.
     */
    float gain=m8_fp_float(gain_bits),left_max=m8_fp_float(cpu->s[left_peak]);
    float right_max=m8_fp_float(cpu->s[right_peak]);
    float s10=0.0f,s11=0.0f,s14=0.0f,s15=0.0f;
    for (uint32_t offset=0;offset<size;offset+=4) {
        if (scale) {
            float left=m8_fp_float(m8_le_read(p[0]+offset,4))*gain;
            m8_le_write(p[0]+offset,4,m8_fp_bits32(left));
            float right=m8_fp_float(m8_le_read(p[1]+offset,4))*gain;
            m8_le_write(p[1]+offset,4,m8_fp_bits32(right));
            s15=fabsf(right);
            s14=fabsf(m8_fp_float(m8_le_read(p[0]+offset,4)));
            if (s15>right_max) right_max=s15;
            if (s14>left_max) left_max=s14;
        } else {
            float left_source=m8_fp_float(m8_le_read(p[2]+offset,4));
            float left_destination=m8_fp_float(m8_le_read(p[0]+offset,4));
            float left_product=left_source*gain;
            float right_source=m8_fp_float(m8_le_read(p[3]+offset,4));
            s14=gain*right_source;
            float left=left_destination+left_product;
            float left_absolute=fabsf(left_product);s10=fabsf(s14);
            m8_le_write(p[0]+offset,4,m8_fp_bits32(left));
            s11=m8_fp_float(m8_le_read(p[1]+offset,4));
            if (left_absolute>left_max) left_max=left_absolute;
            if (s10>right_max) right_max=s10;
            s15=s11+s14;
            m8_le_write(p[1]+offset,4,m8_fp_bits32(s15));
        }
    }
    cpu->r[2]+=size;cpu->s[left_peak]=m8_fp_bits32(left_max);cpu->s[right_peak]=m8_fp_bits32(right_max);
    cpu->s[14]=m8_fp_bits32(s14);cpu->s[15]=m8_fp_bits32(s15);
    if (scale) {cpu->r[3]+=size;cpu->r[15]=0x9a54u;}
    else {
        cpu->r[4]+=size;cpu->r[5]+=size;cpu->r[7]+=size;cpu->r[15]=0x9c48u;
        cpu->s[10]=m8_fp_bits32(s10);cpu->s[11]=m8_fp_bits32(s11);
    }
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    return 1;
}
#undef M8_METER_INLINE
#undef M8_METER_NOINLINE
#else
static inline int m8_meter_mix_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
