/* Headless 6.5.2C FM render block 0x1409c..0x14302. Interpolate
 * nine control values and stereo pan, then run four mode-0 operators.
 * Qualify the complete operation before effects. Reject other layouts,
 * weights, modes, aliases, and unsafe numeric inputs without CPU/RAM changes.
 * The caller configures native FP. Success ends at 0x14302.
 */
#ifndef M8_AOT_FM_QUANTUM_H
#define M8_AOT_FM_QUANTUM_H
#include "fm_renderer.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_FM_OPERATOR) && !defined(M8_AOT_DISABLE_FM_RENDERER) && \
    !defined(M8_AOT_DISABLE_FM_QUANTUM)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FMQ_INLINE static inline __attribute__((always_inline))
#define M8_FMQ_NOINLINE __attribute__((noinline))
#else
#define M8_FMQ_INLINE static inline
#define M8_FMQ_NOINLINE
#endif
/* CPU inputs: R1/R2/R5/R6/R8/R9/R10/R11/R13/R15, S14/S15/S17/S25,
 * XPSR/FPSCR. CPU outputs: R0/R1/R2/R3/R4/R7/R14/R15, S0..S16/S23/S24,
 * XPSR/FPSCR. All other fields retain their entry values.
 */
M8_FMQ_INLINE int m8_fmq_control_numbers(const uint8_t *p) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_FM_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const m8_fm_u4 maximum={0x41000000u,0x41000000u,0x41000000u,0x41000000u};
    m8_fm_u4 target_a=m8_fm_load4(p+4),old_a=m8_fm_load4(p+0x54);
    target_a[1]=0;old_a[1]=0; /* Pitch is outside this interpolation. */
    m8_fm_u4 tail={m8_le_read(p+0x24,4),m8_le_read(p+0x28,4),
                   m8_le_read(p+0x74,4),m8_le_read(p+0x78,4)};
    m8_fm_u4 bad=m8_fm_bad_numbers(target_a,maximum)|m8_fm_bad_numbers(old_a,maximum)|
                 m8_fm_bad_numbers(m8_fm_load4(p+0x14),maximum)|
                 m8_fm_bad_numbers(m8_fm_load4(p+0x64),maximum)|
                 m8_fm_bad_numbers(tail,maximum);
    if (bad[0]|bad[1]|bad[2]|bad[3]) return 0;
#else
    static const unsigned offsets[]={4,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28};
    for (unsigned i=0;i<9;i++) {
        if (!m8_fm_number(m8_le_read(p+offsets[i],4),0x41000000u) ||
            !m8_fm_number(m8_le_read(p+offsets[i]+0x50,4),0x41000000u)) return 0;
    }
#endif
    uint32_t target=m8_le_read(p+0x28,4),old=m8_le_read(p+0x78,4);
    return ((target&0x7fffffffu)<=0x3f800000u && (!(target>>31) || !(target<<1))) &&
           ((old&0x7fffffffu)<=0x3f800000u && (!(old>>31) || !(old<<1)));
}
M8_FMQ_NOINLINE static int m8_fm_quantum_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    uint32_t address=cpu->r[8],sp=cpu->r[13];
    if (cpu->r[15]!=0x1409cu || cpu->s[17] ||
        (cpu->fpscr & 0x03c00010u)!=0x03000010u ||
        (cpu->xpsr & 0x0700fc00u)!=0x01000000u ||
        address>UINT32_MAX-0x24bu || sp<24u || ((address|sp)&3u) ||
        cpu->r[9]!=address+0xf4u || cpu->r[10]!=address+0xa4u ||
        cpu->r[11]!=address+0x54u || cpu->s[25]!=address+0x144u ||
        mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
        mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u ||
        mem->itcm.base!=0u || mem->itcm.size>0x20000000u) return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        const m8_region *r=regions[i];
        if (m8_fm_overlap(r->host,r->size,(const uint8_t *)cpu,sizeof(*cpu)) ||
            m8_fm_overlap(r->host,r->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
        for (unsigned j=0;j<i;j++) {
            if (m8_fm_overlap(r->host,r->size,regions[j]->host,regions[j]->size)) return 0;
        }
    }
    uint8_t *restrict object=m8_region_pointer(&mem->dtcm,address,0x24c);
    uint8_t *restrict stack=m8_region_pointer(&mem->dtcm,sp-12u,12);
    uint8_t *constants=m8_region_pointer(&mem->itcm,0x124a8u,28);
    if (!object || !stack || !constants || (uint32_t)(sp-address-1u)<=598u ||
        !m8_fm_literals(constants)) return 0;
    static const uint32_t weights[]={0,0x3e000000u,0x3e800000u,0x3ec00000u,
                                    0x3f000000u,0x3f200000u,0x3f400000u,0x3f600000u};
    static const uint32_t inverse[]={0x3f800000u,0x3f600000u,0x3f400000u,0x3f200000u,
                                    0x3f000000u,0x3ec00000u,0x3e800000u,0x3e000000u};
    unsigned phase=object[0x194];
    if (phase>7 || cpu->s[15]!=weights[phase] || cpu->s[14]!=inverse[phase] ||
        cpu->r[2]!=(phase<7 ? phase+1u : 7u) ||
        cpu->r[1]!=(phase<7 ? phase+1u : 0u)) return 0;
    uint32_t base=m8_le_read(object+0x70,4);
    uint32_t control_address=m8_le_read(object+0x200,4);
    uint32_t matrix_address=m8_le_read(object+0x1f0,4);
    uint32_t modulation_address=m8_le_read(object+0x20c,4);
    if (base>UINT32_MAX-223u || matrix_address>UINT32_MAX-15u ||
        control_address>UINT32_MAX-127u ||
        modulation_address>UINT32_MAX-3u ||
        ((base|matrix_address|modulation_address|control_address)&3u) ||
        (uint32_t)(matrix_address-base+15u)<=238u ||
        (uint32_t)(control_address-base+127u)<=350u ||
        (uint32_t)(matrix_address-control_address+15u)<=142u ||
        (uint32_t)(modulation_address-address+3u)<=590u ||
        (uint32_t)(modulation_address-(sp-12u)+3u)<=14u) return 0;
    uint8_t *restrict ports=m8_region_pointer(&mem->ocram,base,224);
    uint8_t *matrix=m8_region_pointer(&mem->ocram,matrix_address,16);
    uint8_t *restrict control=m8_region_pointer(&mem->ocram,control_address,128);
    uint8_t *modulation=m8_region_pointer(&mem->dtcm,modulation_address,4);
    if (!ports || !matrix || !modulation || !control ||
        !m8_fmq_control_numbers(control) || m8_le_read(modulation,4)!=0x3f800000u) return 0;
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
            !m8_fmr_numbers(op,ports+64*i) ||
            !m8_fm_number(m8_le_read(matrix+4*i,4),0x41000000u)) return 0;
    }
    /* All spans and numeric inputs are now qualified. Control sources have
     * magnitude at most 8, and exact eighth-step weights cannot create tiny
     * results. The existing mode-0 arithmetic keeps its original operation
     * order. Unit modulation retains the operator kernel's numeric bound.
     */
    float weight=m8_fp_float(cpu->s[15]),complement=m8_fp_float(cpu->s[14]);
    float raw_pan=m8_fm_load(control+0x28)*weight;
    float volume=m8_fm_load(control+4)*weight;
    float feedback=m8_fm_load(control+0x18)*weight;
    float cutoff=m8_fm_load(control+0x1c)*weight;
    float resonance=m8_fm_load(control+0x20)*weight;
    float extra=m8_fm_load(control+0x24)*weight;
    float mix_c=m8_fm_load(control+0xc)*weight;
    float mix_10=m8_fm_load(control+0x10)*weight;
    float mix_14=m8_fm_load(control+0x14)*weight;
    float old_pan=m8_fm_load(control+0x78);
    float old_volume=m8_fm_load(control+0x54);
    float old_mix_c=m8_fm_load(control+0x5c);
    float old_mix_10=m8_fm_load(control+0x60);
    float old_mix_14=m8_fm_load(control+0x64);
    float old_feedback=m8_fm_load(control+0x68);
    float old_cutoff=m8_fm_load(control+0x6c);
    float old_resonance=m8_fm_load(control+0x70);
    float old_extra=m8_fm_load(control+0x74);
    raw_pan=fmaf(complement,old_pan,raw_pan);
    volume=fmaf(complement,old_volume,volume);
    resonance=fmaf(complement,old_resonance,resonance);
    extra=fmaf(complement,old_extra,extra);
    mix_c=fmaf(complement,old_mix_c,mix_c);
    mix_10=fmaf(complement,old_mix_10,mix_10);
    mix_14=fmaf(complement,old_mix_14,mix_14);
    cutoff=fmaf(complement,old_cutoff,cutoff);
    feedback=fmaf(complement,old_feedback,feedback);
    object[0x194]=(uint8_t)cpu->r[1];
    m8_fm_store(control+0x50,raw_pan); m8_fm_store(control+0x2c,volume);
    m8_fm_store(control+0x48,resonance); m8_fm_store(control+0x4c,extra);
    m8_fm_store(control+0x34,mix_c); m8_fm_store(control+0x38,mix_10);
    m8_fm_store(control+0x3c,mix_14); m8_fm_store(control+0x44,cutoff);
    m8_fm_store(control+0x40,feedback);
    float pan_right,pan_left;
    if (raw_pan<0.5f) {pan_right=raw_pan+raw_pan;pan_left=1.0f;}
    else {pan_right=1.0f;pan_left=fmaf(0.5f-raw_pan,2.0f,1.0f);}
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
    cpu->r[7]=control_address;
    cpu->r[14]=0x142f9u; cpu->r[15]=0x14302u;
    cpu->s[0]=m8_fp_bits32(result.s0); cpu->s[4]=m8_fp_bits32(sum);
    cpu->s[1]=m8_fp_bits32(old_extra); cpu->s[2]=m8_fp_bits32(old_mix_c);
    cpu->s[3]=m8_fp_bits32(old_mix_10); cpu->s[5]=m8_fp_bits32(old_feedback);
    cpu->s[6]=0x3f000000u; cpu->s[16]=m8_fp_bits32(pan_right);
    cpu->s[23]=m8_fp_bits32(pan_left); cpu->s[24]=m8_fp_bits32(old_cutoff);
    cpu->s[7]=m8_fp_bits32(result.s7); cpu->s[8]=m8_fp_bits32(result.s8);
    cpu->s[9]=m8_fp_bits32(result.s9); cpu->s[10]=m8_fp_bits32(result.s10);
    cpu->s[11]=m8_fp_bits32(result.s11); cpu->s[12]=m8_fp_bits32(result.s12);
    cpu->s[13]=m8_fp_bits32(result.s13); cpu->s[14]=m8_fp_bits32(result.s14);
    cpu->s[15]=m8_fp_bits32(result.s15);
    cpu->fpscr=(cpu->fpscr & 0x0fffffffu)|result.compare;
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|(result.compare & 0x30000000u);
    return 1;
}
#undef M8_FMQ_INLINE
#undef M8_FMQ_NOINLINE
#else
static inline int m8_fm_quantum_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu; (void)mem; return 0;
}
#endif
#endif
