#ifndef M8_AOT_CANONICAL_MEMORY_H
#define M8_AOT_CANONICAL_MEMORY_H
#include "runtime.h"

/* Qualify once at a public native entry, before effects. The caller must keep
 * region metadata stable until that native call returns, including through
 * cooperative callbacks. Direct callees inherit this contract. Host regions
 * may alias each other; accesses retain their original order without restrict.
 * The generated caller selects these helpers only in its release fast profile.
 * Generic verification and m8_pointer remain available and unchanged. */
M8_MEM_INLINE int m8_canonical_host_span(const uint8_t *host,uint32_t size,const m8_mem *mem) {
    uintptr_t pointer=(uintptr_t)host,descriptor=(uintptr_t)mem;
    if(!pointer || pointer>UINTPTR_MAX-size) return 0;
    return pointer<=descriptor ? descriptor-pointer>=size : pointer-descriptor>=sizeof(*mem);
}
M8_MEM_INLINE int m8_canonical_memory_supported(const m8_mem *mem) {
    return mem &&
        mem->itcm.base==0u && mem->itcm.size==0x80000u &&
        mem->dtcm.base==0x20000000u && mem->dtcm.size==0x80000u &&
        mem->ocram.base==0x20200000u && mem->ocram.size==0x80000u &&
        mem->flash.base==0x60000000u && mem->flash.size==0x800000u &&
        m8_canonical_host_span(mem->itcm.host,0x80000u,mem) &&
        m8_canonical_host_span(mem->dtcm.host,0x80000u,mem) &&
        m8_canonical_host_span(mem->ocram.host,0x80000u,mem) &&
        m8_canonical_host_span(mem->flash.host,0x800000u,mem);
}
/* Use only after m8_canonical_memory_supported. Exact constant bounds remove
 * repeated base, size and null-host checks. Ordered selection matches
 * m8_pointer, including zero-sized spans and unaligned accesses. */
M8_MEM_INLINE uint8_t *m8_canonical_pointer(const m8_mem *mem,uint32_t addr,uint32_t size) {
    if(size<=0x80000u) {
        uint32_t maximum=0x80000u-size;
        /* The bound proves the aligned address prefix. XOR is subtraction
         * inside the accepted DTCM/flash window and needs no base register. */
        uint32_t offset=addr^0x20000000u;
        if(offset<=maximum) return mem->dtcm.host+offset;
        offset-=0x200000u;
        if(offset<=maximum) return mem->ocram.host+offset;
        if(addr<=maximum) return mem->itcm.host+addr;
    }
    uint32_t offset=addr^0x60000000u;
    if(size<=0x800000u && offset<=0x800000u-size) return mem->flash.host+offset;
    return NULL;
}
/* Optional fixed-offset backing. Numeric correspondence alone is insufficient
 * without the same map qualification. Return zero if any host offset differs.
 * No allocation, ABI field, cache or mutable state is added. */
M8_MEM_INLINE uintptr_t m8_canonical_memory_offset(const m8_mem *mem) {
    if(!m8_canonical_memory_supported(mem)) return 0;
    uintptr_t base=(uintptr_t)mem->itcm.host;
    if(base>UINTPTR_MAX-0x60800000u ||
       (uintptr_t)mem->dtcm.host!=base+0x20000000u ||
       (uintptr_t)mem->ocram.host!=base+0x20200000u ||
       (uintptr_t)mem->flash.host!=base+0x60000000u) return 0;
    return base;
}
/* Pass a nonzero base returned by m8_canonical_memory_offset. Guard the whole
 * access before constructing a host address; holes and wrapped spans reject.
 * Integer address formation also avoids pointer subtraction between mappings. */
M8_MEM_INLINE uint8_t *m8_canonical_offset_pointer(uintptr_t base,uint32_t addr,uint32_t size) {
    if(size<=0x80000u) {
        uint32_t maximum=0x80000u-size;
        if((uint32_t)(addr-0x20000000u)<=maximum ||
           (uint32_t)(addr-0x20200000u)<=maximum || addr<=maximum)
            return (uint8_t *)(base+(uintptr_t)addr);
    }
    if(size<=0x800000u && (addr^0x60000000u)<=0x800000u-size)
        return (uint8_t *)(base+(uintptr_t)addr);
    return NULL;
}
#if defined(M8_AOT_VALIDITY_FIRST) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
/* The public fixed-map qualifier proves that base is nonzero and that every
 * accepted address can be added without host overflow. Check the guest span
 * first, then form its host pointer. Keep this predicate equal to the pointer
 * helper above, including zero sizes, holes and wrapped spans. */
M8_MEM_INLINE int m8_canonical_offset_valid(uint32_t addr,uint32_t size) {
    if(size<=0x80000u) {
        uint32_t maximum=0x80000u-size;
        if((uint32_t)(addr-0x20000000u)<=maximum ||
           (uint32_t)(addr-0x20200000u)<=maximum || addr<=maximum)
            return 1;
    }
    return size<=0x800000u && (addr^0x60000000u)<=0x800000u-size;
}
#endif
#endif
