/* Exact mode1/mode6 FM batch fallback. Include after the existing bounded
 * batch body. The public wrapper always attempts that bounded body first. */
#ifndef M8_AOT_FM_STRUCTURAL_BATCH_H
#define M8_AOT_FM_STRUCTURAL_BATCH_H
#if defined(M8_AOT_FM_STRUCTURAL)
/* These names select native hardware operations in normal native code.
 * The broader domain needs the complete conversions and FP64
 * operation. Keep the override local to these new helpers. */
#pragma push_macro("m8_fp_op64")
#pragma push_macro("m8_fp_convert")
#pragma push_macro("m8_fp_eval32")
#pragma push_macro("m8_filter_op")
#if !defined(M8_AOT_FM_FAST) || \
    (!defined(M8_NATIVE_FP_NO_STICKY) && !defined(__aarch64__) && \
     !defined(__wasm__) && !defined(M8_WEB_FP_TEST))
#undef m8_fp_op64
#undef m8_fp_convert
#endif
#undef m8_fp_eval32
#undef m8_filter_op
#define m8_fp_eval32 m8_fms_op
#define m8_filter_op m8_fms_filter_op
M8_FMS_INLINE float m8_fms_filter_op(m8_cpu *cpu,int op,float a,float b,float d) {
    return m8_fp_float(m8_fms_op(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
#include "fm_svf_mode1_fp.h"
M8_FMS_INLINE void m8_fms_filter_mode6(m8_cpu *cpu,uint8_t *object,uint8_t *control,const uint8_t *literal) {
    float s0=m8_fp_float(cpu->s[0]),s15=m8_fm_load(control+0x44),s9=1.0f;
    float s8=m8_fm_load(literal+0x30),s6=m8_fm_load(literal+0x34);
    float s11=m8_fm_load(object+0x28),s12=m8_fm_load(object+0x20);
    s15=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s15),cpu->s[22],0));
    s15=m8_fms_filter_op(cpu,M8_FP_MUL,s15,m8_fp_float(cpu->s[21]),0);
    float s7=m8_fms_filter_op(cpu,M8_FP_SUB,s0,s11,0);
    float s10=m8_fm_load(control+0x48),s13=m8_fm_load(object+0x24);
    uint64_t d7=m8_fp_convert(cpu,m8_fp_bits32(s15),0);
    uint64_t d9=(uint64_t)cpu->s[18]|((uint64_t)cpu->s[19]<<32);
    d7=m8_fp_op64(cpu,M8_FP_MUL,d7,d9,0);
    float s5=m8_fm_load(literal+0x38);
    s10=m8_fms_filter_op(cpu,M8_FP_SUB,s9,s10,0);
    float s14=m8_fp_float((uint32_t)m8_fp_convert(cpu,d7,1));
    s15=s8;
    s10=m8_fms_filter_op(cpu,M8_FP_MUL,s10,s5,0);
    s8=m8_fms_filter_op(cpu,M8_FP_MUL,s14,s14,0);
    s15=m8_fms_filter_op(cpu,M8_FP_FMA,s14,s6,s15);
    s8=m8_fms_filter_op(cpu,M8_FP_MUL,s8,s14,0);
    s14=m8_fms_filter_op(cpu,M8_FP_FMA,s8,s15,s14);
    s8=m8_fms_filter_op(cpu,M8_FP_ADD,s14,s9,0);
    s15=m8_fms_filter_op(cpu,M8_FP_DIV,s14,s8,0);
    s14=s12;
    s14=m8_fms_filter_op(cpu,M8_FP_FMA,s15,s7,s14);
    s5=m8_fms_filter_op(cpu,M8_FP_ADD,s15,s15,0);
    s6=m8_fms_filter_op(cpu,M8_FP_MUL,s15,s15,0);
    s11=m8_fms_filter_op(cpu,M8_FP_ADD,s11,s14,0);
    s14=m8_fms_filter_op(cpu,M8_FP_SUB,s7,s14,0);
    s7=m8_fms_filter_op(cpu,M8_FP_SUB,s6,s15,0);
    s8=m8_fms_filter_op(cpu,M8_FP_SUB,s11,s13,0);
    s12=m8_fms_filter_op(cpu,M8_FP_FMA,s14,s5,s12);
    s14=s9;s9=s8;
    s14=m8_fms_filter_op(cpu,M8_FP_FMA,s10,s7,s14);
    s9=m8_fms_filter_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s15)^0x80000000u),s11,s9);
    s15=m8_fms_filter_op(cpu,M8_FP_MUL,s15,s12,0);
    m8_fm_store(object+0x20,s12);
    s15=m8_fms_filter_op(cpu,M8_FP_FMA,s6,s0,s15);
    s13=m8_fms_filter_op(cpu,M8_FP_FMA,s9,s5,s13);
    s15=m8_fms_filter_op(cpu,M8_FP_ADD,s13,s15,0);
    m8_fm_store(object+0x24,s13);
    s13=m8_fms_filter_op(cpu,M8_FP_DIV,s15,s14,0);
    s10=m8_fms_filter_op(cpu,M8_FP_MUL,s10,s13,0);
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
#pragma pop_macro("m8_filter_op")
#pragma pop_macro("m8_fp_eval32")
#pragma pop_macro("m8_fp_convert")
#pragma pop_macro("m8_fp_op64")
/* Callback metadata is read only. It may live in pinned code/literal banks;
 * it never supplies a mutable destination. Whole-span disjointness below
 * prevents output writes from changing the qualified callback/vtable. */
M8_FMS_INLINE uint8_t *m8_fms_read(m8_mem *mem,uint32_t address,unsigned size) {
    uint8_t *p=m8_fms_ram(mem,address,size);
    if(!p) p=m8_region_pointer(&mem->itcm,address,size);
    return p ? p : m8_region_pointer(&mem->flash,address,size);
}
#if defined(M8_AOT_FM_FAST)
#include "fm_fast_batch.h"
#else
M8_FMS_NOINLINE int m8_fms_batch_kernel(m8_cpu *output,m8_mem *mem) {
    m8_cpu state=*output,*cpu=&state;m8_fms_context c;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++)
        if(m8_fm_overlap(regions[i]->host,regions[i]->size,(uint8_t *)cpu,sizeof(*cpu))) return 0;
    if(!m8_fms_prepare(output,mem,0x1409cu,&c) || !m8_fms_phase(output,c.object)) return 0;
    uint32_t sp=output->r[13],address=c.address;
    if(address>UINT32_MAX-0x253u || sp>UINT32_MAX-7u) return 0;
    c.object=m8_region_pointer(&mem->dtcm,address,0x254);
    c.stack=m8_region_pointer(&mem->dtcm,sp-12u,20);
    uint8_t *literal=m8_region_pointer(&mem->itcm,0x143f8u,64);
    if(!c.object || !c.stack || !literal ||
       (c.object[0x40]!=1 && c.object[0x40]!=6) || c.object[0x41]>3 ||
       output->s[18]!=m8_le_read(literal,4) || output->s[19]!=m8_le_read(literal+4,4) ||
       output->s[20]!=m8_le_read(literal+12,4) || output->s[21]!=m8_le_read(literal+20,4) ||
       output->s[22]!=m8_le_read(literal+16,4)) return 0;
    uint32_t left_address=output->r[6],right_address=m8_le_read(c.stack+12,4);
    uint32_t end=m8_le_read(c.stack+16,4);
    if(end<=left_address || ((end-left_address)&3u)) return 0;
    unsigned count=(end-left_address)/4u;
    if(count>8u-c.object[0x194]) count=8u-c.object[0x194];
    if(!count || ((left_address|right_address)&3u)) return 0;
    uint8_t *left=m8_fms_ram(mem,left_address,count*4u);
    uint8_t *right=m8_fms_ram(mem,right_address,count*4u);
    uint32_t ring_address=m8_le_read(c.object+0x250,4);
    uint8_t *ring=m8_fms_ram(mem,ring_address,640);
    uint32_t callback_address=m8_le_read(c.object+0x50,4);
    uint8_t *callback=m8_fms_read(mem,callback_address,4);
    uint8_t *vtable=callback ? m8_fms_read(mem,m8_le_read(callback,4),4) : NULL;
    if(!left || !right || !ring || !callback || !vtable || m8_le_read(vtable,4)!=0x1fa5u) return 0;
    const uint8_t *spans[]={c.object,c.stack,c.ports,c.matrix,c.control,c.constants,
        literal,left,right,ring,callback,vtable};
    const unsigned sizes[]={0x254,20,224,16,128,28,64,count*4,count*4,640,4,4};
    for(unsigned i=0;i<12;i++)for(unsigned j=0;j<i;j++)
        if(m8_fm_overlap(spans[i],sizes[i],spans[j],sizes[j])) return 0;
    uint32_t cursor_bits=m8_le_read(c.object+0x44,4),step_bits=m8_le_read(c.object+0x48,4);
    if((cursor_bits>>31) || !m8_fm_number(cursor_bits,0x44000000u) ||
       (step_bits>>31) || !m8_fm_number(step_bits,0x3f800000u) ||
       m8_le_read(literal+24,4)!=0x43a00000u || m8_le_read(literal+28,4)!=0x42fe0000u) return 0;
    /* Modulation/PM may read writable spans, but they cannot write them.
     * Every load stays at its original operator boundary. Structural pointer
     * fields and callback/vtable words are outside all admitted write spans.
     * Therefore no callback/requalification occurs before143ea. */
    static const uint32_t weights[]={0,0x3e000000u,0x3e800000u,0x3ec00000u,
        0x3f000000u,0x3f200000u,0x3f400000u,0x3f600000u};
    static const uint32_t inverse[]={0x3f800000u,0x3f600000u,0x3f400000u,0x3f200000u,
        0x3f000000u,0x3ec00000u,0x3e800000u,0x3e000000u};
    for(unsigned iteration=0;iteration<count;iteration++) {
        m8_fms_quantum(cpu,&c);
        uint32_t scale=m8_fms_op(cpu,M8_FP_MUL,m8_le_read(c.object+0x19c,4),cpu->s[20],0);
        uint32_t sample=m8_fms_op(cpu,M8_FP_MUL,scale,cpu->s[4],0);
        cpu->s[15]=scale;cpu->s[1]=m8_le_read(c.control+0x4c,4);
        sample=m8_fms_op(cpu,M8_FP_MUL,sample,cpu->s[1],0);
        sample=m8_fms_op(cpu,M8_FP_MAXNM,sample,0xbf800000u,0);
        sample=m8_fms_op(cpu,M8_FP_MINNM,sample,0x3f800000u,0);cpu->s[0]=sample;
        if(c.object[0x40]==1) m8_fmb_filter_mode1_specialized(cpu,c.object,c.control,literal);
        else m8_fms_filter_mode6(cpu,c.object,c.control,literal);
        uint32_t cursor=m8_le_read(c.object+0x44,4),limit=m8_le_read(literal+24,4);
        m8_fp_cmp32(cpu,cursor,limit,1);
        uint32_t scaled=m8_fms_op(cpu,M8_FP_MUL,cpu->s[0],m8_le_read(literal+28,4),0);
        m8_fp_sync(cpu);cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
        uint32_t ring_value=m8_fp_to_int(cpu,scaled,0,0,3,32,0),ring_offset;
        if(m8_fp_float(cursor)<m8_fp_float(limit)) {
            uint32_t index=m8_fp_to_int(cpu,cursor,0,1,3,32,0);
            cpu->s[14]=index;cpu->r[3]=index;ring_offset=(index&65535u)*2u;
        } else {cursor=cpu->s[17];ring_offset=0;}
        uint32_t next_cursor=m8_fms_op(cpu,M8_FP_ADD,m8_le_read(c.object+0x48,4),cursor,0);
        cpu->r[0]=ring_address;cpu->r[1]=ring_offset;cpu->r[2]=0x1fa5u;cpu->r[4]=c.object[0x40]-1u;
        cpu->r[14]=0x14325u;m8_filter_write16(ring+ring_offset,ring_value);
        m8_fm_store_bits(c.object+0x44,next_cursor);
        uint32_t volume=m8_le_read(c.control+0x2c,4);
        sample=m8_fms_op(cpu,M8_FP_MUL,cpu->s[0],volume,0);
        uint32_t out_left=m8_fms_op(cpu,M8_FP_MUL,sample,cpu->s[23],0);
        uint32_t out_right=m8_fms_op(cpu,M8_FP_MUL,sample,cpu->s[16],0);
        m8_fm_store_bits(left+iteration*4u,out_left);m8_fm_store_bits(right+iteration*4u,out_right);
        cpu->r[6]+=4u;right_address+=4u;m8_fm_store_bits(c.stack+12,right_address);
        cpu->r[3]=end;cpu->s[0]=out_right;cpu->s[13]=cursor;cpu->s[14]=next_cursor;
        cpu->s[15]=volume;cpu->s[23]=out_left;(void)m8_sub_flags(cpu,cpu->r[6],end,0);
        if(cpu->r[6]==end) {cpu->r[15]=0x145bcu;*output=state;return 1;}
        cpu->r[2]=c.object[0x194];
        if(!cpu->r[2]) {cpu->r[15]=0x143eau;*output=state;return 1;}
        unsigned phase=cpu->r[2];cpu->s[15]=weights[phase];cpu->s[14]=inverse[phase];
        (void)m8_sub_flags(cpu,phase,6,0);
        if(phase<=6) {cpu->r[2]=phase+1;cpu->r[1]=cpu->r[2];(void)m8_add_flags(cpu,phase,1,0);}
        else {cpu->r[1]=0;cpu->xpsr=(cpu->xpsr&0x3fffffffu)|0x40000000u;}
        cpu->r[15]=0x1409cu;
    }
    *output=state;return 1;
}
#endif
#endif
#endif
