/* Headless 6.5.2C mode-6 FM render interval, 0x1409c to 0x143ea or
 * 0x145bc. Qualify stable layouts and whole output buffers once. Retain the
 * control-update callback boundary. An intermediate numeric rejection ends
 * at the exact next 0x1409c with all preceding effects preserved. */
#ifndef M8_AOT_FM_BATCH_H
#define M8_AOT_FM_BATCH_H
#include "fm_quantum.h"
#include "filter_kernel.h"
#include "integer.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_FM_OPERATOR) && !defined(M8_AOT_DISABLE_FM_RENDERER) && \
    !defined(M8_AOT_DISABLE_FM_QUANTUM) && !defined(M8_AOT_DISABLE_FM_BATCH)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FMB_INLINE static inline __attribute__((always_inline))
#define M8_FMB_NOINLINE __attribute__((noinline))
#else
#define M8_FMB_INLINE static inline
#define M8_FMB_NOINLINE
#endif
/* Restrict writes to the direct RAM regions. A write to ITCM could change
 * later guest instructions; the interval cannot prove that code stays fixed. */
M8_FMB_INLINE uint8_t *m8_fmb_ram(m8_mem *mem,uint32_t address,unsigned size) {
    uint8_t *pointer=m8_region_pointer(&mem->dtcm,address,size);
    return pointer ? pointer : m8_region_pointer(&mem->ocram,address,size);
}
/* The original filter at 0x144f6 uses S18/S19 as the saved D9 constant.
 * This function preserves all clobbered S words, including temporary halves
 * of double registers. Native helpers keep the existing NaN/FZ contract. */
M8_FMB_INLINE void m8_fmb_filter(m8_cpu *cpu,uint8_t *object,uint8_t *control,const uint8_t *literal) {
    float s0=m8_fp_float(cpu->s[0]),s15=m8_fm_load(control+0x44),s9=1.0f;
    float s8=m8_fm_load(literal+0x30),s6=m8_fm_load(literal+0x34);
    float s11=m8_fm_load(object+0x28),s12=m8_fm_load(object+0x20);
    s15=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s15),cpu->s[22],0));
    s15=m8_filter_op(cpu,M8_FP_MUL,s15,m8_fp_float(cpu->s[21]),0);
    float s7=m8_filter_op(cpu,M8_FP_SUB,s0,s11,0);
    float s10=m8_fm_load(control+0x48),s13=m8_fm_load(object+0x24);
    uint64_t d7=m8_filter_widen(cpu,s15);
    uint64_t d9=(uint64_t)cpu->s[18]|((uint64_t)cpu->s[19]<<32);
    d7=m8_fp_op64(cpu,M8_FP_MUL,d7,d9,0);
    float s5=m8_fm_load(literal+0x38);
    s10=m8_filter_op(cpu,M8_FP_SUB,s9,s10,0);
    float s14=m8_filter_narrow(cpu,d7);
    s15=s8;
    s10=m8_filter_op(cpu,M8_FP_MUL,s10,s5,0);
    s8=m8_filter_op(cpu,M8_FP_MUL,s14,s14,0);
    s15=m8_filter_op(cpu,M8_FP_FMA,s14,s6,s15);
    s8=m8_filter_op(cpu,M8_FP_MUL,s8,s14,0);
    s14=m8_filter_op(cpu,M8_FP_FMA,s8,s15,s14);
    s8=m8_filter_op(cpu,M8_FP_ADD,s14,s9,0);
    s15=m8_filter_op(cpu,M8_FP_DIV,s14,s8,0);
    s14=s12;
    s14=m8_filter_op(cpu,M8_FP_FMA,s15,s7,s14);
    s5=m8_filter_op(cpu,M8_FP_ADD,s15,s15,0);
    s6=m8_filter_op(cpu,M8_FP_MUL,s15,s15,0);
    s11=m8_filter_op(cpu,M8_FP_ADD,s11,s14,0);
    s14=m8_filter_op(cpu,M8_FP_SUB,s7,s14,0);
    s7=m8_filter_op(cpu,M8_FP_SUB,s6,s15,0);
    s8=m8_filter_op(cpu,M8_FP_SUB,s11,s13,0);
    s12=m8_filter_op(cpu,M8_FP_FMA,s14,s5,s12);
    s14=s9;s9=s8;
    s14=m8_filter_op(cpu,M8_FP_FMA,s10,s7,s14);
    s9=m8_filter_op(cpu,M8_FP_FMA,-s15,s11,s9);
    s15=m8_filter_op(cpu,M8_FP_MUL,s15,s12,0);
    m8_fm_store(object+0x20,s12);
    s15=m8_filter_op(cpu,M8_FP_FMA,s6,s0,s15);
    s13=m8_filter_op(cpu,M8_FP_FMA,s9,s5,s13);
    s15=m8_filter_op(cpu,M8_FP_ADD,s13,s15,0);
    m8_fm_store(object+0x24,s13);
    s13=m8_filter_op(cpu,M8_FP_DIV,s15,s14,0);
    s10=m8_filter_op(cpu,M8_FP_MUL,s10,s13,0);
    s0=s13;
    m8_fm_store(object+0x28,s10);
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[5]=m8_fp_bits32(s5);
    cpu->s[6]=m8_fp_bits32(s6);cpu->s[7]=m8_fp_bits32(s7);
    cpu->s[8]=m8_fp_bits32(s8);cpu->s[9]=m8_fp_bits32(s9);
    cpu->s[10]=m8_fp_bits32(s10);cpu->s[11]=m8_fp_bits32(s11);
    cpu->s[12]=m8_fp_bits32(s12);cpu->s[13]=m8_fp_bits32(s13);
    cpu->s[14]=m8_fp_bits32(s14);cpu->s[15]=m8_fp_bits32(s15);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
}
#define M8_FMB_QUANTUM_RESULT void
M8_FMB_INLINE M8_FMB_QUANTUM_RESULT m8_fmb_quantum(m8_cpu *cpu,uint8_t *object,uint8_t *stack,
    uint8_t *ports,uint8_t *matrix,uint8_t *control,uint8_t *modulation,
    uint8_t *constants,uint32_t address,uint32_t matrix_address,uint32_t control_address) {
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
}
/* CPU inputs: R1/R2/R5/R6/R8/R9/R10/R11/R13/R15, S14/S15/S17..S22/S25,
 * XPSR/FPSCR. Outputs: R0/R1/R2/R3/R4/R6/R7/R14/R15, S0..S16/S23/S24,
 * XPSR/FPSCR. Other CPU words retain their entry values. A result of zero
 * retains every CPU/RAM byte. Success returns at 143ea, 145bc, or 1409c.
 * Count/diagnostic builds retain the original scalar instruction path. */
M8_FMB_NOINLINE static int m8_fm_batch_kernel(m8_cpu *restrict output,m8_mem *restrict mem) {
    m8_cpu state=*output; m8_cpu *cpu=&state;
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
        if (r->size && (!r->host || (uintptr_t)r->host>UINTPTR_MAX-r->size)) return 0;
        if (m8_fm_overlap(r->host,r->size,(const uint8_t *)output,sizeof(*output)) ||
            m8_fm_overlap(r->host,r->size,(const uint8_t *)cpu,sizeof(*cpu)) ||
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

    if (address>UINT32_MAX-0x253u || sp>UINT32_MAX-7u) return 0;
    object=m8_region_pointer(&mem->dtcm,address,0x254);
    stack=m8_region_pointer(&mem->dtcm,sp-12u,20);
    uint8_t *literal=m8_pointer(mem,0x143f8u,64);
    if (!object || !stack || !literal || object[0x40]!=6 || object[0x41]>3 ||
        cpu->s[18]!=m8_le_read(literal,4) || cpu->s[19]!=m8_le_read(literal+4,4) ||
        cpu->s[20]!=m8_le_read(literal+12,4) || cpu->s[21]!=m8_le_read(literal+20,4) ||
        cpu->s[22]!=m8_le_read(literal+16,4)) return 0;
    uint32_t left_address=cpu->r[6],right_address=m8_le_read(stack+12,4);
    uint32_t end=m8_le_read(stack+16,4);
    if (end<=left_address || ((end-left_address)&3u)) return 0;
    unsigned count=(end-left_address)/4u;
    if (count>8u-phase) count=8u-phase;
    if (count<2 || ((left_address|right_address)&3u) ||
        left_address>UINT32_MAX-count*4u || right_address>UINT32_MAX-count*4u) return 0;
    uint8_t *left=m8_fmb_ram(mem,left_address,count*4u);
    uint8_t *right=m8_fmb_ram(mem,right_address,count*4u);
    uint32_t ring_address=m8_le_read(object+0x250,4);
    uint8_t *ring=m8_fmb_ram(mem,ring_address,640);
    uint32_t callback_address=m8_le_read(object+0x50,4);
    uint8_t *callback=m8_pointer(mem,callback_address,4);
    uint8_t *vtable=callback ? m8_pointer(mem,m8_le_read(callback,4),4) : NULL;
    if (!left || !right || !ring || ring_address>UINT32_MAX-639u ||
        !callback || !vtable || m8_le_read(vtable,4)!=0x1fa5u) return 0;
    /* Whole host regions are already disjoint from one another, both CPU
     * objects and the descriptor. In the ordinary layout, only spans within
     * one owner can alias. Other valid layouts keep the complete matrix.
     * Pointer qualification above plus these guest windows proves each
     * owner; Flash cannot override a successful higher-priority RAM span. */
    const uint8_t *spans[]={object,stack,ports,matrix,control,modulation,
                           constants,literal,left,right,ring,callback,vtable};
    const unsigned sizes[]={0x254,20,224,16,128,4,28,64,count*4,count*4,640,4,4};
    for (unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++) {
        if (m8_fm_overlap(spans[i],sizes[i],(uint8_t *)output,sizeof(*output)) ||
            m8_fm_overlap(spans[i],sizes[i],(uint8_t *)mem,sizeof(*mem))) return 0;
        for (unsigned j=0;j<i;j++) {
            if (m8_fm_overlap(spans[i],sizes[i],spans[j],sizes[j])) return 0;
        }
    }
    uint32_t cursor_bits=m8_le_read(object+0x44,4),step_bits=m8_le_read(object+0x48,4);
    if ((cursor_bits>>31) || !m8_fm_number(cursor_bits,0x44000000u) ||
        (step_bits>>31) || !m8_fm_number(step_bits,0x3f800000u) ||
        m8_le_read(literal+24,4)!=0x43a00000u || m8_le_read(literal+28,4)!=0x42fe0000u) return 0;
#if defined(__x86_64__)
    /* Native DAZ cannot report ARM IDC. Reject external subnormal filter
     * inputs before effects, as the existing bounded filter kernel does. */
    static const unsigned input_offsets[]={0x20,0x24,0x28,0x19c};
    for (unsigned i=0;i<sizeof(input_offsets)/sizeof(*input_offsets);i++) {
        uint32_t bits=m8_le_read(object+input_offsets[i],4);
        if ((bits&0x7fffffffu) && !(bits&0x7f800000u)) return 0;
    }
    for (unsigned offset=8;offset<64;offset+=4) {
        uint32_t bits=m8_le_read(literal+offset,4);
        if ((bits&0x7fffffffu) && !(bits&0x7f800000u)) return 0;
    }
    uint64_t wide=(uint64_t)cpu->s[18]|((uint64_t)cpu->s[19]<<32);
    if ((wide&UINT64_C(0x7fffffffffffffff)) && !(wide&UINT64_C(0x7ff0000000000000))) return 0;
#endif
    /* No callback can change the qualified pointers before 0x143ea. These
     * spans are disjoint; ring and output stores cannot change later inputs.
     * Operator arithmetic changes its numeric state, so qualify that state
     * before each subsequent quantum and preserve preceding effects on exit.
     */
    for (unsigned iteration=0;iteration<count;iteration++) {
        if (iteration) {
            for (unsigned i=0;i<4;i++) {
                if (!m8_fmr_numbers(object+0x54+0x50*i,ports+64*i)) {
                    *output=state;return 1;
                }
            }
        }
        m8_fmb_quantum(cpu,object,stack,ports,matrix,control,modulation,constants,
                       address,matrix_address,control_address);
        float scale=m8_filter_op(cpu,M8_FP_MUL,m8_fm_load(object+0x19c),m8_fp_float(cpu->s[20]),0);
        float sample=m8_filter_op(cpu,M8_FP_MUL,scale,m8_fp_float(cpu->s[4]),0);
        cpu->s[15]=m8_fp_bits32(scale);cpu->s[1]=m8_le_read(control+0x4c,4);
        sample=m8_filter_op(cpu,M8_FP_MUL,sample,m8_fp_float(cpu->s[1]),0);
        sample=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(sample),0xbf800000u,0));
        sample=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(sample),0x3f800000u,0));
        cpu->s[0]=m8_fp_bits32(sample);
        m8_fmb_filter(cpu,object,control,literal);
        float cursor=m8_fm_load(object+0x44),limit=m8_fm_load(literal+24);
        m8_fp_cmp32(cpu,m8_fp_bits32(cursor),m8_fp_bits32(limit),1);
        float scaled=m8_filter_op(cpu,M8_FP_MUL,m8_fp_float(cpu->s[0]),m8_fm_load(literal+28),0);
        m8_fp_sync(cpu);
        cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
        uint32_t ring_value=m8_filter_to_int(cpu,scaled,0);
        uint32_t ring_offset;
        if (cursor<limit) {
            uint32_t index=m8_filter_to_int(cpu,cursor,1);
            cpu->s[14]=index;cpu->r[3]=index;
            ring_offset=(index&65535u)*2u;
        } else {
            cursor=m8_fp_float(cpu->s[17]);ring_offset=0;
        }
        float next_cursor=m8_filter_op(cpu,M8_FP_ADD,m8_fm_load(object+0x48),cursor,0);
        cpu->r[0]=ring_address;cpu->r[1]=ring_offset;
        cpu->r[2]=0x1fa5u;cpu->r[4]=5;cpu->r[14]=0x14325u;
        m8_filter_write16(ring+ring_offset,ring_value);
        m8_fm_store(object+0x44,next_cursor);
        float volume=m8_fm_load(control+0x2c);
        sample=m8_filter_op(cpu,M8_FP_MUL,m8_fp_float(cpu->s[0]),volume,0);
        float out_left=m8_filter_op(cpu,M8_FP_MUL,sample,m8_fp_float(cpu->s[23]),0);
        float out_right=m8_filter_op(cpu,M8_FP_MUL,sample,m8_fp_float(cpu->s[16]),0);
        m8_fm_store(left+iteration*4u,out_left);
        m8_fm_store(right+iteration*4u,out_right);
        cpu->r[6]+=4u;right_address+=4u;
        m8_fm_store_bits(stack+12,right_address);
        cpu->r[3]=end;
        cpu->s[0]=m8_fp_bits32(out_right);cpu->s[13]=m8_fp_bits32(cursor);
        cpu->s[14]=m8_fp_bits32(next_cursor);cpu->s[15]=m8_fp_bits32(volume);
        cpu->s[23]=m8_fp_bits32(out_left);
        (void)m8_sub_flags(cpu,cpu->r[6],end,0);
        if (cpu->r[6]==end) {cpu->r[15]=0x145bcu;*output=state;return 1;}
        cpu->r[2]=object[0x194];
        if (!cpu->r[2]) {cpu->r[15]=0x143eau;*output=state;return 1;}
        unsigned next_phase=cpu->r[2];
        cpu->s[15]=weights[next_phase];cpu->s[14]=inverse[next_phase];
        (void)m8_sub_flags(cpu,next_phase,6u,0);
        if (next_phase<=6) {
            cpu->r[2]=next_phase+1u;cpu->r[1]=cpu->r[2];
            (void)m8_add_flags(cpu,next_phase,1u,0);
        } else {
            cpu->r[1]=0;
            cpu->xpsr=(cpu->xpsr&0x3fffffffu)|0x40000000u;
        }
        cpu->r[15]=0x1409cu;
    }
    *output=state;return 1;
}
#undef M8_FMB_INLINE
#undef M8_FMB_NOINLINE
#undef M8_FMB_QUANTUM_RESULT
#else
static inline int m8_fm_batch_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#endif
#endif
