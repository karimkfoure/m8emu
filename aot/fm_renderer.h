/* Headless 6.5.2C FM renderer block 0x1417c..0x14302. Four mode-0
 * operators use contiguous port records and unit modulation. The caller
 * configures native FP. Reject before effects; success ends at 0x14302.
 */
#ifndef M8_AOT_FM_RENDERER_H
#define M8_AOT_FM_RENDERER_H
#include "fm_operator.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_FM_OPERATOR) && !defined(M8_AOT_DISABLE_FM_RENDERER)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FMR_INLINE static inline __attribute__((always_inline))
#define M8_FMR_NOINLINE __attribute__((noinline))
#else
#define M8_FMR_INLINE static inline
#define M8_FMR_NOINLINE
#endif
#if defined(M8_AOT_FM_STRUCTURAL)
#define m8_fm_renderer_kernel m8_fm_renderer_bounded_kernel
#endif
typedef struct m8_fmr_result {
    float s0,s7,s8,s9,s10,s11,s12,s13,s14,s15;
    uint32_t compare;
} m8_fmr_result;

/* The complete preflight supplies disjoint object/port spans. Intermediate
 * RAM writes stay in original order. CPU registers need one final publish.
 */
M8_FMR_INLINE m8_fmr_result m8_fmr_operator(uint8_t *restrict object,
                                          uint8_t *restrict ports,
                                          const uint8_t *pm,
                                          const uint8_t *constants) {
    float step=m8_fm_load(ports),s14=0.5f;
    float s15=m8_fm_load(object+4)+step;
    float s13=s15+s14;
    m8_fm_store(object+4,s15);
    int32_t rounded=(int32_t)s13;
    float s12=m8_fm_load(object+8)+step;
    s13=(float)rounded;
    m8_fm_store(object+8,s12);
    s15=s15-s13;
    m8_fm_store(object+4,s15);
    s13=m8_fm_load(pm);
    s15=s15+s13;
    s14=s15<0.0f ? s15-s14 : s15+s14;
    s14=(float)(int32_t)s14;
    s15=s15-s14;
    s14=m8_fm_load(ports+8);
    m8_fm_store_bits(ports+8,0);
    float s11=0.5f,s7=m8_fm_load(constants+8);
    s13=m8_fm_load(ports+20);
    s12=m8_fm_load(constants+12);
    s14=s14*s13;
    float s9=m8_fm_load(constants+4);
    float s8=m8_fm_load(constants+16);
    float s10=m8_fm_load(constants+24);
    s15=s14+s15;
    m8_fm_store(ports+20,s14);
    s14=0.5f;
    uint32_t compare=s15==0.0f ? 0x60000000u :
                     s15<0.0f ? 0x80000000u : 0x20000000u;
    s14=s15<0.0f ? s15-s14 : s15+s14;
    s14=(float)(int32_t)s14;
    s15=s15-s14;
    s14=m8_fm_load(constants);
    s13=fabsf(s15);
    s15=s15*s14;
    s14=m8_fm_load(constants+20);
    s11=s13-s11;
    s7=s13+s7;
    s12=s13-s12;
    s15=s11*s15;
    s9=fmaf(s7,s13,s9);
    s8=s13-s8;
    s14=s13+s14;
    s11=s10;
    s15=s12*s15;
    s11=fmaf(s8,s13,s11);
    s15=s14*s15;
    s15=s15*s9;
    s11=s15*s11;
    m8_fm_store(ports+8,s11);
    float s0=s11*m8_fm_load(ports+12);
    m8_fm_store(ports+8,s0);
    s15=m8_fm_load(ports+16);
    s0=s0*s15;
    m8_fm_store(ports+8,s0);
    s15=m8_fm_load(object+16);
    s0=s0+s15;
    m8_fm_store(ports+8,s0);
    object[0x38]=1;
    return (m8_fmr_result){s0,s7,s8,s9,s10,s11,s12,s13,s14,s15,compare};
}

M8_FMR_INLINE int m8_fmr_numbers(const uint8_t *object,const uint8_t *ports) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_FM_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const m8_fm_u4 maximum={0x41000000u,0x4b800000u,0x41000000u,0x41000000u};
    const m8_fm_u4 narrow={0x41000000u,0x41000000u,0x41000000u,0x41000000u};
    m8_fm_u4 values={m8_le_read(ports,4),m8_le_read(ports+8,4),
                     m8_le_read(ports+12,4),m8_le_read(ports+28,4)};
    m8_fm_u4 bad=m8_fm_bad_numbers(m8_fm_load4(object+4),maximum)|
                 m8_fm_bad_numbers(values,narrow);
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
#else
    return m8_fm_number(m8_le_read(object+4,4),0x41000000u) &&
        m8_fm_number(m8_le_read(object+8,4),0x4b800000u) &&
        m8_fm_number(m8_le_read(object+12,4),0x41000000u) &&
        m8_fm_number(m8_le_read(object+16,4),0x41000000u) &&
        m8_fm_number(m8_le_read(ports,4),0x41000000u) &&
        m8_fm_number(m8_le_read(ports+8,4),0x41000000u) &&
        m8_fm_number(m8_le_read(ports+12,4),0x41000000u) &&
        m8_fm_number(m8_le_read(ports+28,4),0x41000000u);
#endif
}

M8_FMR_NOINLINE static int m8_fm_renderer_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    uint32_t address=cpu->r[8],sp=cpu->r[13];
    if (cpu->r[15]!=0x1417cu || cpu->s[17] ||
        (cpu->fpscr & 0x03c00010u)!=0x03000010u ||
        (cpu->xpsr & 0x0700fc00u)!=0x01000000u ||
        address>UINT32_MAX-0x24bu || sp<24u || ((address|sp)&3u) ||
        cpu->r[9]!=address+0xf4u || cpu->r[10]!=address+0xa4u ||
        cpu->r[11]!=address+0x54u || cpu->s[25]!=address+0x144u ||
        mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
        mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(mem->ocram.host,mem->ocram.size,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,(const uint8_t *)mem,sizeof(*mem)) ||
        m8_fm_overlap(mem->ocram.host,mem->ocram.size,(const uint8_t *)mem,sizeof(*mem)) ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,mem->ocram.host,mem->ocram.size)) return 0;
    uint8_t *restrict object=m8_region_pointer(&mem->dtcm,address,0x24c);
    uint8_t *restrict stack=m8_region_pointer(&mem->dtcm,sp-12u,12);
    uint8_t *constants=m8_region_pointer(&mem->itcm,0x124a8u,28);
    if (!object || !stack || !constants || (uint32_t)(sp-address-1u)<=598u ||
        m8_fm_overlap(constants,28,mem->dtcm.host,mem->dtcm.size) ||
        m8_fm_overlap(constants,28,mem->ocram.host,mem->ocram.size) ||
        m8_fm_overlap(constants,28,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(constants,28,(const uint8_t *)mem,sizeof(*mem)) ||
        !m8_fm_literals(constants)) return 0;
    uint32_t base=m8_le_read(object+0x70,4);
    uint32_t matrix_address=m8_le_read(object+0x1f0,4);
    uint32_t modulation_address=m8_le_read(object+0x20c,4);
    if (base>UINT32_MAX-223u || matrix_address>UINT32_MAX-15u ||
        modulation_address>UINT32_MAX-3u ||
        ((base|matrix_address|modulation_address)&3u) ||
        (uint32_t)(matrix_address-base+15u)<=238u ||
        (uint32_t)(modulation_address-address+3u)<=590u ||
        (uint32_t)(modulation_address-(sp-12u)+3u)<=14u) return 0;
    uint8_t *restrict ports=m8_region_pointer(&mem->ocram,base,224);
    uint8_t *matrix=m8_region_pointer(&mem->ocram,matrix_address,16);
    uint8_t *modulation=m8_region_pointer(&mem->dtcm,modulation_address,4);
    if (!ports || !matrix || !modulation || m8_le_read(modulation,4)!=0x3f800000u) return 0;
    /* The per-operator matrix range below rejects most often. Check it once
     * up front, before the modulation-alias and record-shape validation. */
    for (unsigned i=0;i<4;i++) {
        if (!m8_fm_number(m8_le_read(matrix+4*i,4),0x41000000u)) return 0;
    }
    for (unsigned i=0;i<16;i++) {
        if (m8_le_read(object+0x20c+4*i,4)!=modulation_address) return 0;
    }
    for (unsigned i=0;i<4;i++) {
        unsigned offset=0x54+0x50*i;
        const uint8_t *op=object+offset;
        uint32_t op_address=address+offset,port_address=base+64*i;
        uint32_t pm_address=i==3 ? op_address+12u : port_address+72u;
        if (op[0x3a] || m8_le_read(op,4)!=port_address+8u ||
            m8_le_read(op+0x1c,4)!=port_address ||
            m8_le_read(op+0x24,4)!=port_address+28u ||
            m8_le_read(op+0x28,4)!=port_address+12u ||
            m8_le_read(op+0x2c,4)!=port_address+16u ||
            m8_le_read(op+0x30,4)!=port_address+20u ||
            m8_le_read(op+0x44,4)!=pm_address ||
            m8_le_read(op+0x48,4)!=op_address+16u ||
            !m8_fmr_numbers(op,ports+64*i)) return 0;
    }
    /* Nonzero sources are at least 2^-16 and at most 8. Unit modulation
     * keeps prepared gain at 1. The worst cancellation in prepared feedback
     * and phase remains above 2^-78; the final amplitude remains normal.
     * All round-zero operands are below 2^10. Sticky IXC lets the caller
     * collect hardware flags once. No boundary service occurs in this block.
     */
    float unit=m8_fm_load(modulation),gain=unit*unit;
    m8_fm_store_bits(stack,matrix_address);
    m8_fm_store_bits(stack+4,cpu->r[5]);
    m8_fm_store_bits(stack+8,cpu->r[6]);
    float sum=0.0f;
    m8_fmr_result result;
    for (int i=3;i>=0;i--) {
        uint8_t *op=object+0x54+0x50*i;
        uint8_t *port=ports+64*i;
        float depth=m8_fm_load(port+28);
        float feedback=fmaf(gain,m8_fm_load(matrix+4*i),depth+depth);
        m8_fm_store(port+16,gain);
        m8_fm_store(port+20,feedback);
        uint8_t *pm=i==3 ? op+12 : port+72;
        result=m8_fmr_operator(op,port,pm,constants);
        if (i==3) sum=op[0x39] ? 0.0f : result.s0;
        else if (!op[0x39]) sum=sum+result.s0;
    }
    cpu->r[0]=address+0x54u; cpu->r[1]=object[0x8d];
    cpu->r[2]=address+0x64u; cpu->r[3]=1; cpu->r[4]=matrix_address;
    cpu->r[14]=0x142f9u; cpu->r[15]=0x14302u;
    cpu->s[0]=m8_fp_bits32(result.s0); cpu->s[4]=m8_fp_bits32(sum);
    cpu->s[7]=m8_fp_bits32(result.s7); cpu->s[8]=m8_fp_bits32(result.s8);
    cpu->s[9]=m8_fp_bits32(result.s9); cpu->s[10]=m8_fp_bits32(result.s10);
    cpu->s[11]=m8_fp_bits32(result.s11); cpu->s[12]=m8_fp_bits32(result.s12);
    cpu->s[13]=m8_fp_bits32(result.s13); cpu->s[14]=m8_fp_bits32(result.s14);
    cpu->s[15]=m8_fp_bits32(result.s15);
    cpu->fpscr=(cpu->fpscr & 0x0fffffffu)|result.compare;
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|(result.compare & 0x30000000u);
    return 1;
}
#if defined(M8_AOT_FM_STRUCTURAL)
#undef m8_fm_renderer_kernel
#include "fm_structural.h"
M8_FMR_NOINLINE static int m8_fm_renderer_kernel(m8_cpu *cpu,m8_mem *mem) {
    if(m8_fm_renderer_bounded_kernel(cpu,mem)) return 1;
    return m8_fms_renderer_kernel(cpu,mem);
}
#endif
#undef M8_FMR_INLINE
#undef M8_FMR_NOINLINE
#else
static inline int m8_fm_renderer_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu; (void)mem; return 0;
}
#endif
#endif
