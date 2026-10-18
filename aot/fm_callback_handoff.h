/* Exact FM prefix through14320. The original BLX at14322 remains in firmware.
 * Include after fm_structural_batch.h. No callback or output sample runs here. */
#ifndef M8_AOT_FM_CALLBACK_HANDOFF_H
#define M8_AOT_FM_CALLBACK_HANDOFF_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_CALLBACK_HANDOFF)
M8_FMS_NOINLINE int m8_fms_callback_handoff(m8_cpu *output,m8_mem *mem) {
    m8_cpu state=*output,*cpu=&state;m8_fms_context c;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++)
        if(m8_fm_overlap(regions[i]->host,regions[i]->size,(uint8_t *)cpu,sizeof(*cpu))) return 0;
    if(!m8_fms_prepare(output,mem,0x1409cu,&c) ||
       !m8_fms_phase(output,c.object) || c.object[0x41]>3u) return 0;
    /* Original14316..14320 follows the quantum's writes. These checked words
     * must stay unchanged across those writes before their values can be
     * qualified early. Other read-only spans may alias the metadata. */
    uint32_t callback_address=m8_le_read(c.object+0x50,4);
    uint8_t *callback=m8_fms_read(mem,callback_address,4);
    if(!callback || (callback_address&3u)) return 0;
    uint32_t table_address=m8_le_read(callback,4);
    uint8_t *table=m8_fms_read(mem,table_address,4);
    if(!table || (table_address&3u)) return 0;
    const uint8_t *written[]={c.object,c.stack,c.ports,c.control};
    const unsigned sizes[]={0x24c,12,224,128};
    for(unsigned i=0;i<4;i++)
        if(m8_fm_overlap(callback,4,written[i],sizes[i]) ||
           m8_fm_overlap(table,4,written[i],sizes[i])) return 0;
    uint32_t target=m8_le_read(table,4);
    /* Qualify only the Thumb code-bank address. Do not fetch instructions or
     * run the callback. Unsupported targets decline before any guest effect;
     * the original path retains its own fault and partial-effect order. */
    uint32_t code=target&~1u;
    if(!(target&1u) || (!m8_region_pointer(&mem->itcm,code,2) &&
                       !m8_region_pointer(&mem->flash,code,2))) return 0;
    m8_fms_quantum(cpu,&c);
    uint32_t scale=m8_fms_op(cpu,M8_FP_MUL,m8_le_read(c.object+0x19c,4),cpu->s[20],0);
    cpu->r[0]=c.object[0x41];
    cpu->s[15]=scale;
    cpu->s[0]=m8_fms_op(cpu,M8_FP_MUL,scale,cpu->s[4],0);
    (void)m8_sub_flags(cpu,cpu->r[0],3u,0);
    /*14316 LDR R0,1431a VLDR S1,1431e/14320 LDR R2. Publish the
     * pre-BLX state; neither LR nor the callback's effects change here. */
    cpu->r[0]=callback_address;
    cpu->s[1]=m8_le_read(c.control+0x4c,4);
    cpu->r[2]=target;cpu->r[15]=0x14322u;
    m8_fp_sync(cpu);*output=state;return 1;
}
#endif
#endif
