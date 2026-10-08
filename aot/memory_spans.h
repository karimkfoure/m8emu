#ifndef M8_AOT_MEMORY_SPANS_H
#define M8_AOT_MEMORY_SPANS_H
#include "runtime.h"

/* Preserve m8_pointer's per-access region priority. A smaller earlier region
 * must not redirect part of a span to a different backing buffer. Host buffers
 * may alias: these pointers deliberately carry no restrict qualification. */
M8_MEM_INLINE uint8_t *m8_span_pointer(m8_mem *mem,uint32_t addr,uint32_t size) {
    m8_region *regions[]={&mem->dtcm,&mem->ocram,&mem->itcm,&mem->flash};
    uint64_t end=(uint64_t)addr+size;
    if(!size || end>UINT64_C(0x100000000)) return NULL;
    for(unsigned i=0;i<4;i++) {
        m8_region *r=regions[i];
        if(!r->host || !r->size) continue;
        uint64_t region_end=(uint64_t)r->base+r->size;
        if(region_end>UINT64_C(0x100000000)) return NULL;
        uint8_t *p=m8_region_pointer(r,addr,size);
        if(p) return p;
        if((uint64_t)addr<region_end && (uint64_t)r->base<end) return NULL;
    }
    return NULL;
}
#endif
