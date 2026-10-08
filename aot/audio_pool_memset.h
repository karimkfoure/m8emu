/* Exact aligned RAM subset of original fn_3ad10, [0x3ad10,0x3adb4).
 * Headless6.5.2C body SHA256:
 * 6bfae40cb2fc2425026f7b2f00ee9dcafc8f093db6ef386e513e0e66f52b064c.
 * The caller must verify this body at generation/admission and prevent entry
 * after code invalidation. No per-call code scan or device service is used. */
#ifndef M8_AUDIO_POOL_MEMSET_H
#define M8_AUDIO_POOL_MEMSET_H
#include "runtime.h"

static inline int m8_audio_pool_memset_overlap(const void *first,uint32_t first_size,
                                              const void *second,uint32_t second_size) {
    if(!first || !second || !first_size || !second_size) return 0;
    uintptr_t a=(uintptr_t)first,b=(uintptr_t)second;
    return a<=b ? b-a<first_size : a-b<second_size;
}

static inline uint8_t *m8_audio_pool_memset_ram(m8_mem *mem,uint32_t addr,uint32_t size) {
    m8_region *region;
    if((uint32_t)(addr-0x20000000u)<0x80000u) region=&mem->dtcm;
    else if((uint32_t)(addr-0x20200000u)<0x80000u) region=&mem->ocram;
    else return NULL;
    if(region->base!=(region==&mem->dtcm?0x20000000u:0x20200000u) ||
       region->size>0x80000u) return NULL;
    return m8_region_pointer(region,addr,size);
}

/* Return 1 only after completing the qualified original function. Return 0
 * before effects so the unchanged translation can handle every other case.
 * The common candidate flag is absent in ordinary builds. */
static inline int m8_audio_pool_memset(m8_cpu *cpu,m8_mem *mem) {
#if !defined(M8_AOT_AUDIO_POOL) || defined(M8_AOT_DIAGNOSTIC) || defined(M8_AOT_GUEST_COUNT)
    (void)cpu;(void)mem;return 0;
#else
    uint32_t dst=cpu->r[0],count=cpu->r[2],sp=cpu->r[13],link=cpu->r[14];
    if(cpu->r[15]!=0x3ad10u || (cpu->xpsr&0x0600fc00u) || m8_current_it ||
       !(link&1u) || link>=0xfffffff0u || (dst&3u) || (sp&3u) || count<16u || (count&3u)) return 0;
    uint8_t *target=m8_audio_pool_memset_ram(mem,dst,count);
    uint8_t *frame=m8_audio_pool_memset_ram(mem,sp-12u,12u);
    if(!target || !frame ||
       m8_audio_pool_memset_overlap(target,count,frame,12u) ||
       m8_audio_pool_memset_overlap(target,count,cpu,sizeof *cpu) ||
       m8_audio_pool_memset_overlap(frame,12u,cpu,sizeof *cpu) ||
       m8_audio_pool_memset_overlap(target,count,mem,sizeof *mem) ||
       m8_audio_pool_memset_overlap(frame,12u,mem,sizeof *mem) ||
       m8_audio_pool_memset_overlap(target,count,mem->itcm.host,mem->itcm.size) ||
       m8_audio_pool_memset_overlap(frame,12u,mem->itcm.host,mem->itcm.size) ||
       m8_audio_pool_memset_overlap(target,count,mem->flash.host,mem->flash.size) ||
       m8_audio_pool_memset_overlap(frame,12u,mem->flash.host,mem->flash.size)) return 0;
    /* PUSH is observable in RAM after POP. Preserve its exact three words. */
    m8_le_write(frame,4,cpu->r[4]);
    m8_le_write(frame+4,4,cpu->r[5]);
    m8_le_write(frame+8,4,link);
    memset(target,cpu->r[1]&255u,count);
    cpu->r[2]=cpu->r[3]=dst+count;
    cpu->r[12]=count&15u;
    cpu->r[14]=dst+(count&~15u)+16u;
    cpu->r[15]=link&~1u;
    cpu->xpsr=(cpu->xpsr&~0xf0000000u)|0x60000000u;
    return 1;
#endif
}
#endif
