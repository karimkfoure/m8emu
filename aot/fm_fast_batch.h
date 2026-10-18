/* Exact FM sample batches for fixed RN/FZ/DN execution. Structural spans
 * are checked once, before any write. Each batch stops at the control-update
 * boundary, where the firmware can change routing and coefficients. */
#ifndef M8_AOT_FM_FAST_BATCH_H
#define M8_AOT_FM_FAST_BATCH_H
#define M8_AOT_FM_CALLBACK_2284 1
#include "fm_callback_2284.h"
M8_FMS_INLINE void m8_fms_shape(m8_cpu *cpu,m8_fms_context *c,
        uint32_t callback_address,uint32_t target,const m8_fmcb2284_context *shape) {
    cpu->r[0]=callback_address;cpu->r[2]=target;
    cpu->r[14]=c->object[0x41]<=3 ? 0x14325u : 0x14351u;
    cpu->s[1]=m8_le_read(c->control+0x4c,4);
    if(target==0x2285u) m8_fmcb2284_apply(cpu,shape);
    else {
        uint32_t sample=m8_fms_op(cpu,M8_FP_MUL,cpu->s[0],cpu->s[1],0);
        sample=m8_fms_op(cpu,M8_FP_MAXNM,sample,0xbf800000u,0);
        cpu->s[0]=m8_fms_op(cpu,M8_FP_MINNM,sample,0x3f800000u,0);
        cpu->s[15]=0x3f800000u;
    }
}
M8_FMS_NOINLINE int m8_fms_batch_kernel(m8_cpu *output,m8_mem *mem) {
    if(mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
       !mem->dtcm.host || (uintptr_t)mem->dtcm.host>UINTPTR_MAX-mem->dtcm.size) return 0;
    uint8_t *quick=m8_region_pointer(&mem->dtcm,output->r[8],0x254);
    if(!quick || quick[0x40]>6 ||
#if defined(M8_AOT_FM_FAST_MODES)
       (quick[0x8e] && (quick[0x8e]<7 || quick[0x8e]>8)) ||
       (quick[0xde] && (quick[0xde]<7 || quick[0xde]>8)) ||
       (quick[0x12e] && (quick[0x12e]<7 || quick[0x12e]>8)) ||
       (quick[0x17e] && (quick[0x17e]<7 || quick[0x17e]>8))) return 0;
#else
       quick[0x8e] || quick[0xde] || quick[0x12e] || quick[0x17e]) return 0;
#endif
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
       c.object[0x40]>6 ||
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
    if(!left || !right || !ring || !callback || !vtable) return 0;
    uint32_t target=m8_le_read(vtable,4);
    m8_fmcb2284_context shape={0};
    if(target==0x2285u) {if(!m8_fmcb2284_prepare(output,mem,&shape))return 0;}
    else if(target!=0x1fa5u)return 0;
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
#if defined(M8_FM_FAST_CAPTURE)
#if defined(M8_AOT_FM_FAST_MODES)
    M8_FM_FAST_CAPTURE(output,mem,"mixed");
#else
    M8_FM_FAST_CAPTURE(output,mem,"mode0");
#endif
#endif
    for(unsigned iteration=0;iteration<count;iteration++) {
        m8_fms_quantum(cpu,&c);
        uint32_t scale=m8_fms_op(cpu,M8_FP_MUL,m8_le_read(c.object+0x19c,4),cpu->s[20],0);
        uint32_t sample=m8_fms_op(cpu,M8_FP_MUL,scale,cpu->s[4],0);
        cpu->s[15]=scale;cpu->s[0]=sample;
        if(c.object[0x41]<=3)m8_fms_shape(cpu,&c,callback_address,target,&shape);
        if(c.object[0x40]>=1 && c.object[0x40]<=5)
            m8_fmb_filter_svf(cpu,c.object,c.control,literal,c.object[0x40]);
        else if(c.object[0x40]==6) m8_fms_filter_mode6(cpu,c.object,c.control,literal);
        if(c.object[0x41]>3)m8_fms_shape(cpu,&c,callback_address,target,&shape);
        uint32_t cursor=m8_le_read(c.object+0x44,4),limit=m8_le_read(literal+24,4);
        m8_fp_cmp32(cpu,cursor,limit,1);
        uint32_t scaled=m8_fms_op(cpu,M8_FP_MUL,cpu->s[0],m8_le_read(literal+28,4),0);
        m8_fp_sync(cpu);cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
        uint32_t ring_value=m8_fms_to_int(cpu,scaled,0),ring_offset;
        if(m8_fp_float(cursor)<m8_fp_float(limit)) {
            uint32_t index=m8_fms_to_int(cpu,cursor,1);
            cpu->s[14]=index;cpu->r[3]=index;ring_offset=(index&65535u)*2u;
        } else {cursor=cpu->s[17];ring_offset=0;}
        uint32_t next_cursor=m8_fms_op(cpu,M8_FP_ADD,m8_le_read(c.object+0x48,4),cursor,0);
        cpu->r[0]=ring_address;cpu->r[1]=ring_offset;cpu->r[4]=c.object[0x40]-1u;
        m8_filter_write16(ring+ring_offset,ring_value);
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
