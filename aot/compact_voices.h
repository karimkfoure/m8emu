#ifndef M8_AOT_COMPACT_VOICES_H
#define M8_AOT_COMPACT_VOICES_H
#include "compact_memory.h"

/* Candidate voice kernels qualify the same complete spans and alias matrix
 * as the fixed-map kernels. Keep descriptors stable through callbacks. The
 * compact allocation replaces only address formation, never guest effects. */
M8_MEM_INLINE uintptr_t m8_compact_voice_memory_offset(const m8_mem *mem) {
    return m8_compact_memory_supported(mem) ? (uintptr_t)mem->itcm.host : 0;
}
/* A nonzero base must come from m8_compact_voice_memory_offset. Match the
 * checked compact helper, including region priority and zero-sized spans. */
M8_MEM_INLINE uint8_t *m8_compact_voice_pointer(uintptr_t base,uint32_t addr,uint32_t size) {
    uint32_t offset;
    if(size<=0x80000u) {
        uint32_t maximum=0x80000u-size;
        offset=addr-0x20000000u;
        if(offset<=maximum) return (uint8_t *)(base+M8_COMPACT_DTCM_OFFSET+offset);
        offset=addr-0x20200000u;
        if(offset<=maximum) return (uint8_t *)(base+M8_COMPACT_OCRAM_OFFSET+offset);
        if(addr<=maximum) return (uint8_t *)(base+addr);
    }
    offset=addr-0x60000000u;
    if(size<=0x800000u && offset<=0x800000u-size)
        return (uint8_t *)(base+M8_COMPACT_FLASH_OFFSET+offset);
    return NULL;
}
/* Match production's data-only native guard. The voice qualifiers admit only
 * their separately proved ITCM literal spans through the general helper. */
M8_MEM_INLINE uint8_t *m8_compact_voice_data_pointer(uintptr_t base,uint32_t addr,uint32_t size) {
    if(size<=0x80000u) {
        uint32_t offset=addr-0x20000000u;
        if(offset<=0x80000u-size)
            return (uint8_t *)(base+M8_COMPACT_DTCM_OFFSET+offset);
        offset=addr-0x20200000u;
        if(offset<=0x80000u-size)
            return (uint8_t *)(base+M8_COMPACT_OCRAM_OFFSET+offset);
    }
    return NULL;
}
/* Use only within a nonempty ITCM/DTCM/OCRAM span proved by voice qualification.
 * Flash is never an admitted voice owner. Region bit 21 selects the two RAM
 * banks. Constants fold to one offset; dynamic addresses need one select,
 * with no descriptor read or repeated bound check in the sample loop. */
M8_MEM_INLINE uint8_t *m8_compact_voice_host(uintptr_t base,uint32_t addr) {
    uint32_t offset=addr<0x20000000u ? addr :
        M8_COMPACT_DTCM_OFFSET+(addr&0x1fffffu)+((addr&0x200000u)>>2);
    return (uint8_t *)(base+offset);
}
#endif
