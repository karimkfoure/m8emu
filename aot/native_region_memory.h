#ifndef M8_NATIVE_REGION_MEMORY_H
#define M8_NATIVE_REGION_MEMORY_H
#include "canonical_memory.h"
#ifdef M8_AOT_COMPACT_MEMORY
#include "compact_memory.h"
#define M8_NATIVE_REGION_POINTER(mem,addr,size) m8_compact_pointer(mem,addr,size)
#define M8_NATIVE_REGION_RAM_POINTER(mem,addr,size) m8_compact_pointer(mem,addr,size)
#else
#define M8_NATIVE_REGION_POINTER(mem,addr,size) m8_canonical_offset_pointer((uintptr_t)(mem)->itcm.host,addr,size)
/* The caller has already checked the complete DTCM/OCRAM span. */
#define M8_NATIVE_REGION_RAM_POINTER(mem,addr,size) ((uint8_t *)((uintptr_t)(mem)->itcm.host+(addr)))
#endif

#if M8_NATIVE_REGION_INLINE_MEMORY
#define M8_NATIVE_REGION_MEM_INLINE M8_MEM_INLINE
#else
#define M8_NATIVE_REGION_MEM_INLINE static inline
#endif

/* Admission proves the fixed backing map. Reads retain the AOT region bounds.
 * Code writes use the bus callback so the owner can invalidate translations. */
M8_NATIVE_REGION_MEM_INLINE int m8_native_region_read(m8_mem *mem,uint32_t addr,unsigned size,
                                        uint32_t *value,int *slow) {
    uint8_t *p=M8_NATIVE_REGION_POINTER(mem,addr,size);
    if(p) { *value=m8_le_read(p,size);return 0; }
    int result=mem->read?mem->read(mem->ctx,addr,size,value):1;
#if M8_NATIVE_REGION_SERVICE_CONTINUE
    /* The runtime supplies result=2 only when this device effect needs no
     * boundary. DWT is a separate RAM page, advanced outside native calls. */
    if(result==M8_AOT_MEM_CONTINUE ||
       (result==0 && size && size<=4 && (uint32_t)(addr-0xe0001000u)<=4096u-size))
        return 0;
#endif
    *slow=1;
    return result!=0 && result!=M8_AOT_MEM_CONTINUE;
}
M8_NATIVE_REGION_MEM_INLINE int m8_native_region_write(m8_mem *mem,uint32_t addr,unsigned size,
                                         uint32_t value,int *slow) {
    if(size<=0x80000u && ((uint32_t)(addr-0x20000000u)<=0x80000u-size ||
                        (uint32_t)(addr-0x20200000u)<=0x80000u-size)) {
        m8_le_write(M8_NATIVE_REGION_RAM_POINTER(mem,addr,size),size,value);
        return 0;
    }
    int result=mem->write?mem->write(mem->ctx,addr,size,value):1;
#if M8_NATIVE_REGION_SERVICE_CONTINUE
    if(result==M8_AOT_MEM_CONTINUE) return 0;
#endif
    *slow=1;
    return result!=0 && result!=M8_AOT_MEM_CONTINUE;
}
#undef M8_NATIVE_REGION_MEM_INLINE
#endif
