/* Headless 6.5.2C FM pitch update, 0x136d6..0x139ee. Qualify the
 * no-modulation path and complete port/table spans before effects. Four
 * operators share the same runtime frequency lookup; do not copy tables
 * into host constants. The caller configures native RN/FZ/DN once. */
#ifndef M8_AOT_FM_PITCH_H
#define M8_AOT_FM_PITCH_H
#include "fm_operator.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_FM_OPERATOR) && !defined(M8_AOT_DISABLE_FM_PITCH)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FMP_NOINLINE __attribute__((noinline))
#else
#define M8_FMP_NOINLINE
#endif
/* Inputs: R4/R5/R14/R15, S14/S15, XPSR/FPSCR. Outputs: R1/R2/R3/R14/R15,
 * S0/S7..S17, XPSR/FPSCR. Other words retain their entry values. Rejecting
 * inputs leaves every CPU/RAM byte unchanged. Success ends at 0x139ee. */
M8_FMP_NOINLINE static int m8_fm_pitch_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    uint32_t address=cpu->r[4],control_address=cpu->r[5];
    if (cpu->r[15]!=0x136d6u || cpu->s[14]!=0x3f000000u ||
        (cpu->fpscr&0x03c00010u)!=0x03000010u ||
        (cpu->xpsr&0x0700fc00u)!=0x01000000u ||
        !m8_fm_number(cpu->s[15],0x41000000u) || ((address|control_address)&3u) ||
        address>UINT32_MAX-0x203u || control_address>UINT32_MAX-11u ||
        mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
        mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u ||
        mem->itcm.base!=0u || mem->itcm.size>0x20000000u ||
        mem->flash.base!=0x60000000u || mem->flash.size>0x800000u) return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        const m8_region *region=regions[i];
        if (region->size && (!region->host || (uintptr_t)region->host>UINTPTR_MAX-region->size)) return 0;
        if (m8_fm_overlap(region->host,region->size,(const uint8_t *)cpu,sizeof(*cpu)) ||
            m8_fm_overlap(region->host,region->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
        for (unsigned j=0;j<i;j++)
            if (m8_fm_overlap(region->host,region->size,regions[j]->host,regions[j]->size)) return 0;
    }
    uint8_t *restrict object=m8_region_pointer(&mem->dtcm,address,0x204);
    uint8_t *restrict control=m8_region_pointer(&mem->ocram,control_address,12);
    const uint8_t *literal=m8_region_pointer(&mem->itcm,0x13964u,28);
    const uint8_t *helper_literal=m8_region_pointer(&mem->itcm,0x10588u,24);
    const uint8_t *zero=m8_region_pointer(&mem->itcm,0x13db4u,4);
    if (!object || !control || !literal || !helper_literal || !zero ||
        m8_le_read(object+0x200,4)!=control_address || m8_le_read(zero,4) ||
        !m8_fm_number(m8_le_read(object+0x1b4,4),0x41000000u) ||
        !m8_fm_number(m8_le_read(control+8,4),0x44000000u) ||
        memcmp(literal,helper_literal,24) || m8_le_read(literal,4) ||
        m8_le_read(literal+4,4)!=0x42fe0000u || m8_le_read(literal+8,4)!=0x42ee0000u ||
        m8_le_read(literal+20,4)!=0x39a377eeu || m8_le_read(literal+24,4)!=0x43800000u) return 0;
    uint32_t port_address=m8_le_read(object+0x70,4);
    if (port_address>UINT32_MAX-223u || (port_address&3u)) return 0;
    uint8_t *restrict ports=m8_region_pointer(&mem->ocram,port_address,224);
    if (!ports || m8_fm_overlap(ports,224,control,12)) return 0;
    for (unsigned i=0;i<8;i++)
        if (object[0x1f4+i]>1) return 0;
    for (unsigned i=0;i<4;i++) {
        const uint8_t *op=object+0x54+0x50*i;
        if (m8_le_read(op+0x1c,4)!=port_address+64*i ||
            m8_le_read(op+0x20,4)!=port_address+64*i+4u ||
            m8_le_read(op+0x24,4)!=port_address+64*i+28u ||
            !m8_fm_number(m8_le_read(ports+64*i+4u,4),0x41000000u) ||
            !m8_fm_number(m8_le_read(ports+64*i+28u,4),0x41000000u)) return 0;
    }
    uint32_t whole_address=m8_le_read(literal+12,4),fraction_address=m8_le_read(literal+16,4);
    if ((whole_address|fraction_address)&3u) return 0;
    const uint8_t *whole=m8_region_pointer(&mem->flash,whole_address,988);
    const uint8_t *fraction=m8_region_pointer(&mem->flash,fraction_address,1024);
    if (!whole || !fraction) return 0;
    /* Qualified finite sources bound this preflight to magnitude below 2^10.
     * It can raise only IXC, which is already sticky. Both selected table
     * words are checked before the first architectural effect. */
    float phase=m8_fp_float(cpu->s[15]),amplitude=m8_fm_load(object+0x1b4);
    float distance=fabsf(phase-0.5f);
    float triangle=fmaf(-distance,2.0f,1.0f);
    triangle=fmaf(triangle,2.0f,-1.0f);
    float pitch=fmaf(triangle,amplitude,m8_fm_load(control+8));
    float clamped=pitch<=0.0f ? 0.0f : pitch>=127.0f ? 127.0f : pitch;
    float shifted=clamped+119.0f;
    uint32_t index=(uint32_t)shifted;
    float remainder=shifted-(float)index;
    uint32_t fraction_index=(uint32_t)(remainder*256.0f);
    uint32_t first_bits=m8_le_read(whole+4u*index,4);
    uint32_t second_bits=m8_le_read(fraction+4u*fraction_index,4);
    if (!m8_fm_number(first_bits,0x4b800000u) || !m8_fm_number(second_bits,0x4b800000u)) return 0;
    /* The multiply chain remains normal or zero under these bounds. It can
     * raise only IXC. Read table values at runtime and keep both original
     * multiply roundings; all four scalar calls receive the same pitch. */
    float scale=m8_fp_float(0x39a377eeu);
    float frequency=m8_fp_float(first_bits)*scale;
    frequency=frequency*m8_fp_float(second_bits);
    float global_frequency=frequency,output=0.0f,temporary=0.0f,last_ratio=0.0f;
    uint32_t r1=object[0x1f7],r2=0,r3=0,lr=cpu->r[14];
    uint32_t s13=index,s14=m8_fp_bits32(pitch),s0=m8_fp_bits32(pitch);
    static const uint32_t returns[]={0x139c1u,0x138e9u,0x13851u,0x137b9u};
    m8_fm_store(object+0x1bc,phase);
    m8_fm_store(control+8,pitch);
    for (int i=3;i>=0;i--) {
        const uint8_t *op=object+0x54+0x50*i;
        uint8_t *port=ports+64*i;
        r2=i==3 ? 0u : object[0x1f4+i];
        temporary=m8_fm_load(port+4);
        float ratio=0.0f+temporary;
        last_ratio=ratio;output=ratio;
        if (ratio!=0.0f) {
            s0=m8_fp_bits32(frequency);s13=m8_fp_bits32((float)index);
            s14=0x39a377eeu;r1=index;r2=fraction_index;lr=returns[i];
            temporary=m8_fm_load(port+28);
            output=ratio+temporary;
            output=output*frequency;
        } else if (i!=3) s0=m8_fp_bits32(pitch);
        m8_fm_store(port,output);
        unsigned mode=op[0x3a];
        if (mode<=10) {
            temporary=output;r3=port_address+64u*i;
            if (output!=0.0f) global_frequency=output;
        } else r3=mode;
    }
    cpu->r[1]=r1;cpu->r[2]=r2;cpu->r[3]=r3;cpu->r[14]=lr;cpu->r[15]=0x139eeu;
    cpu->s[0]=s0;cpu->s[7]=m8_fp_bits32(amplitude);cpu->s[8]=0x42fe0000u;
    cpu->s[9]=0x42ee0000u;cpu->s[10]=0x39a377eeu;cpu->s[11]=0x40000000u;
    cpu->s[12]=m8_fp_bits32((float)index);cpu->s[13]=s13;cpu->s[14]=s14;
    cpu->s[15]=m8_fp_bits32(temporary);cpu->s[16]=m8_fp_bits32(global_frequency);
    cpu->s[17]=m8_fp_bits32(output);
    uint32_t compared=m8_fp_bits32(object[0x8e]>10 ? last_ratio : output);
    uint32_t comparison=!(compared&0x7fffffffu) ? 0x60000000u :
                        (compared>>31) ? 0x80000000u : 0x20000000u;
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|comparison;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|((object[0x8e]>10) ? 0x20000000u : comparison);
    return 1;
}
#undef M8_FMP_NOINLINE
#else
static inline int m8_fm_pitch_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
