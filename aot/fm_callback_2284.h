/* Exact checked Headless 6.5.2C callback 2284..2290 -> 36d04..36d8e.
 * The original path reads RAM and changes CPU state only. The table can
 * alias earlier FM writes: its two words are loaded at their original
 * position, after phase/index FP conversions. No guest pointer is restrict.
 */
#ifndef M8_AOT_FM_CALLBACK_2284_H
#define M8_AOT_FM_CALLBACK_2284_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_CALLBACK_2284)
#include "integer.h"
typedef struct m8_fmcb2284_context {
    const uint8_t *table;
} m8_fmcb2284_context;
/* A rejection occurs before FP, mutable guest state or a slow bus access.
 * Qualifying the whole table also bounds both loads when conversion
 * saturates or produces an invalid result. The last index is511 and the
 * second table word is512. Unmapped/overlapping profiles use original code.
 */
M8_FMS_INLINE int m8_fmcb2284_prepare(m8_cpu *cpu,m8_mem *mem,
                                    m8_fmcb2284_context *context) {
    if((cpu->xpsr&0x0700fc00u)!=0x01000000u ||
#if defined(M8_AOT_FM_FAST) && \
    (defined(M8_NATIVE_FP_NO_STICKY) || defined(M8_WEB_FP_NO_STICKY))
       (cpu->fpscr&0x03c00000u)!=0x03000000u ||
#else
       (cpu->fpscr&0x03c00010u)!=0x03000010u ||
#endif
       mem->itcm.base || mem->itcm.size>0x20000000u ||
       mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
       mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u)return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++) {
        const m8_region *r=regions[i];
        if(r->size && (!r->host || (uintptr_t)r->host>UINTPTR_MAX-r->size))return 0;
        if(m8_fm_overlap(r->host,r->size,(uint8_t *)cpu,sizeof(*cpu)) ||
           m8_fm_overlap(r->host,r->size,(uint8_t *)mem,sizeof(*mem)))return 0;
        for(unsigned j=0;j<i;j++)
            if(m8_fm_overlap(r->host,r->size,regions[j]->host,regions[j]->size))return 0;
    }
    const uint8_t *scale=m8_region_pointer(&mem->itcm,0x2294u,4);
    const uint8_t *literals=m8_region_pointer(&mem->itcm,0x36d90u,16);
    const uint8_t *table=m8_region_pointer(&mem->dtcm,0x20005b80u,2052);
    if(!scale || !literals || !table ||
       m8_le_read(scale,4)!=0x3fc70386u ||
       m8_le_read(literals,4)!=0x3e22f983u ||
       m8_le_read(literals+4,4)!=0x44000000u ||
       m8_le_read(literals+8,4)!=0x20005b80u ||
       m8_le_read(literals+12,4)!=0xb44c02cdu)return 0;
    context->table=table;return 1;
}
M8_FMS_INLINE void m8_fmcb2284_apply(m8_cpu *cpu,const m8_fmcb2284_context *context) {
    /* The caller has selected original target2285 and installed its odd LR.
     * Memory can be prepared before the batch changes those GPR fields. */
    uint32_t s0=cpu->s[0],s1=cpu->s[1],s13=cpu->s[13],s14=cpu->s[14],s15=0x3fc70386u;
    s1=m8_fms_op(cpu,M8_FP_MUL,s1,s15,0);
    s0=m8_fms_op(cpu,M8_FP_MUL,s1,s0,0);
    m8_fp_cmp32(cpu,s0,0,1);m8_fp_sync(cpu);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
    if(cpu->xpsr&0x80000000u) {
        s15=0xb44c02cdu;m8_fp_cmp32(cpu,s0,s15,1);m8_fp_sync(cpu);
        cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
        if(!(((cpu->xpsr>>31)^(cpu->xpsr>>28))&1u))goto finish;
        s15=0x3e22f983u;s0=m8_fms_op(cpu,M8_FP_MUL,s0,s15,0);
        s15=m8_fms_to_int(cpu,s0,0);
        cpu->r[3]=m8_sub_flags(cpu,s15,1,0);s15=cpu->r[3];
    } else {
        s15=0x3e22f983u;s0=m8_fms_op(cpu,M8_FP_MUL,s0,s15,0);
        s15=m8_fms_to_int(cpu,s0,0);
    }
    s15=m8_fms_from_int(cpu,s15,0);
    s13=0x44000000u;cpu->r[2]=0x20005b80u;s14=0x3f800000u;
    s0=m8_fms_op(cpu,M8_FP_SUB,s0,s15,0);
    s0=m8_fms_op(cpu,M8_FP_MUL,s0,s13,0);
    s15=m8_fms_to_int(cpu,s0,1);
    cpu->r[3]=s15&511u;s15=cpu->r[3];
    cpu->r[1]=cpu->r[2]+cpu->r[3]*4u;
    s15=m8_fms_from_int(cpu,s15,1);
    s13=m8_le_read(context->table+cpu->r[3]*4u,4);
    s15=m8_fms_op(cpu,M8_FP_SUB,s0,s15,0);
    s0=m8_le_read(context->table+cpu->r[3]*4u+4u,4);
    s14=m8_fms_op(cpu,M8_FP_SUB,s14,s15,0);
    s15=m8_fms_op(cpu,M8_FP_MUL,s15,s0,0);
    s0=m8_fms_op(cpu,M8_FP_MUL,s14,s13,0);
    s0=m8_fms_op(cpu,M8_FP_ADD,s0,s15,0);
finish:
    cpu->s[0]=s0;cpu->s[1]=s1;cpu->s[13]=s13;cpu->s[14]=s14;cpu->s[15]=s15;
    cpu->r[15]=cpu->r[14]&~1u;
}
#endif
#endif
