#ifndef M8_AOT_COMPACT_MEMORY_H
#define M8_AOT_COMPACT_MEMORY_H
#include "canonical_memory.h"

/* One allocation; the guest's sparse addresses never index it directly.
 * Keep these offsets equal to core's compact allocator. ROM/DWT remain slow
 * services because they are not members of the four-region AOT ABI. */
#define M8_COMPACT_DTCM_OFFSET UINT32_C(0x80000)
#define M8_COMPACT_OCRAM_OFFSET UINT32_C(0x100000)
#define M8_COMPACT_FLASH_OFFSET UINT32_C(0x180000)
#define M8_COMPACT_ROM_OFFSET UINT32_C(0x980000)
#define M8_COMPACT_DWT_OFFSET UINT32_C(0x9a0000)
#define M8_COMPACT_SIZE UINT32_C(0x9a1000)
M8_MEM_INLINE int m8_compact_memory_supported(const m8_mem *mem) {
    if(!m8_canonical_memory_supported(mem)) return 0;
    uintptr_t base=(uintptr_t)mem->itcm.host;
    return base<=UINTPTR_MAX-M8_COMPACT_SIZE &&
        (uintptr_t)mem->dtcm.host==base+M8_COMPACT_DTCM_OFFSET &&
        (uintptr_t)mem->ocram.host==base+M8_COMPACT_OCRAM_OFFSET &&
        (uintptr_t)mem->flash.host==base+M8_COMPACT_FLASH_OFFSET;
}
/* Entry qualification proves backing and bounds. Constant guest addresses
 * fold to a single compact offset; variable accesses retain the same guards
 * and region priority as m8_pointer. MMIO and holes return NULL. */
#if defined(M8_AOT_COMPACT_TABLE) && M8_AOT_COMPACT_TABLE && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
/* The high nibble and bit 21 distinguish DTCM from OCRAM. Each active slot's
 * minimum address is at or above its guest start, so one widened end check
 * also proves the lower bound. Flash spans both bit-21 values. The low word
 * is a modulo-2^32 compact offset minus guest start, not a host pointer. */
#define M8_COMPACT_ROW(end,delta) (((uint64_t)(end)<<32)|(uint32_t)(delta))
static const uint64_t m8_compact_rows[32]={
    [0]=M8_COMPACT_ROW(0x80000u,0u),
    [4]=M8_COMPACT_ROW(0x20080000u,M8_COMPACT_DTCM_OFFSET-0x20000000u),
    [5]=M8_COMPACT_ROW(0x20280000u,M8_COMPACT_OCRAM_OFFSET-0x20200000u),
    [12]=M8_COMPACT_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u),
    [13]=M8_COMPACT_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u),
};
/* Native writes must reject executable backing before any effect. */
static const uint64_t m8_compact_data_rows[32]={
    [4]=M8_COMPACT_ROW(0x20080000u,M8_COMPACT_DTCM_OFFSET-0x20000000u),
    [5]=M8_COMPACT_ROW(0x20280000u,M8_COMPACT_OCRAM_OFFSET-0x20200000u),
};
/* Fixed scalar widths use a pre-biased exclusive start limit. Empty rows
 * have limit zero, so even address zero rejects. This needs neither an
 * address+size operation nor a widened comparison on the hot path. A full
 * 2 MiB page index costs one shift; only the real pages contain active rows. */
#define M8_COMPACT_SMALL_ROW(end,delta,n) M8_COMPACT_ROW((end)-(n)+1u,delta)
#define M8_COMPACT_SMALL_READ_ROWS(n) { \
    [0]=M8_COMPACT_SMALL_ROW(0x80000u,0u,n), \
    [256]=M8_COMPACT_SMALL_ROW(0x20080000u,M8_COMPACT_DTCM_OFFSET-0x20000000u,n), \
    [257]=M8_COMPACT_SMALL_ROW(0x20280000u,M8_COMPACT_OCRAM_OFFSET-0x20200000u,n), \
    [768]=M8_COMPACT_SMALL_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u,n), \
    [769]=M8_COMPACT_SMALL_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u,n), \
    [770]=M8_COMPACT_SMALL_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u,n), \
    [771]=M8_COMPACT_SMALL_ROW(0x60800000u,M8_COMPACT_FLASH_OFFSET-0x60000000u,n) }
#define M8_COMPACT_SMALL_DATA_ROWS(n) { \
    [256]=M8_COMPACT_SMALL_ROW(0x20080000u,M8_COMPACT_DTCM_OFFSET-0x20000000u,n), \
    [257]=M8_COMPACT_SMALL_ROW(0x20280000u,M8_COMPACT_OCRAM_OFFSET-0x20200000u,n) }
static const uint64_t m8_compact_read1[2048]=M8_COMPACT_SMALL_READ_ROWS(1u);
static const uint64_t m8_compact_read2[2048]=M8_COMPACT_SMALL_READ_ROWS(2u);
static const uint64_t m8_compact_read4[2048]=M8_COMPACT_SMALL_READ_ROWS(4u);
static const uint64_t m8_compact_data1[2048]=M8_COMPACT_SMALL_DATA_ROWS(1u);
static const uint64_t m8_compact_data2[2048]=M8_COMPACT_SMALL_DATA_ROWS(2u);
static const uint64_t m8_compact_data4[2048]=M8_COMPACT_SMALL_DATA_ROWS(4u);
#undef M8_COMPACT_SMALL_DATA_ROWS
#undef M8_COMPACT_SMALL_READ_ROWS
#undef M8_COMPACT_SMALL_ROW
#undef M8_COMPACT_ROW
M8_MEM_INLINE uint8_t *m8_compact_lookup(const m8_mem *mem,uint32_t addr,uint32_t size,
                                      const uint64_t *rows) {
    uint32_t index=((addr>>28)<<1)|((addr>>21)&1u);
    uint64_t row=rows[index];
    if((uint64_t)addr+size>(row>>32)) return NULL;
    uint32_t offset=addr+(uint32_t)row;
    return (uint8_t *)((uintptr_t)mem->itcm.host+offset);
}
M8_MEM_INLINE uint8_t *m8_compact_small_lookup(const m8_mem *mem,uint32_t addr,
                                            const uint64_t *rows) {
    uint64_t row=rows[addr>>21];
    if(addr>=(uint32_t)(row>>32)) return NULL;
    uint32_t offset=addr+(uint32_t)row;
    return (uint8_t *)((uintptr_t)mem->itcm.host+offset);
}
M8_MEM_INLINE uint8_t *m8_compact_pointer(const m8_mem *mem,uint32_t addr,uint32_t size) {
    if(size==4u) return m8_compact_small_lookup(mem,addr,m8_compact_read4);
    if(size==2u) return m8_compact_small_lookup(mem,addr,m8_compact_read2);
    if(size==1u) return m8_compact_small_lookup(mem,addr,m8_compact_read1);
    return m8_compact_lookup(mem,addr,size,m8_compact_rows);
}
M8_MEM_INLINE uint8_t *m8_compact_data_pointer(const m8_mem *mem,uint32_t addr,uint32_t size) {
    if(size==4u) return m8_compact_small_lookup(mem,addr,m8_compact_data4);
    if(size==2u) return m8_compact_small_lookup(mem,addr,m8_compact_data2);
    if(size==1u) return m8_compact_small_lookup(mem,addr,m8_compact_data1);
    return size?m8_compact_lookup(mem,addr,size,m8_compact_data_rows):NULL;
}
#else
M8_MEM_INLINE uint8_t *m8_compact_pointer(const m8_mem *mem,uint32_t addr,uint32_t size) {
    uint32_t offset;
    uintptr_t base=(uintptr_t)mem->itcm.host;
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
#endif
M8_MEM_INLINE uint8_t *m8_compact_span_pointer(const m8_mem *mem,uint32_t addr,uint32_t size) {
    return size?m8_compact_pointer(mem,addr,size):NULL;
}
#endif
