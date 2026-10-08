/* Exact entry shortcut for the firmware memcpy at 0x326d4. */
#ifndef M8_AOT_MEMCPY_KERNEL_H
#define M8_AOT_MEMCPY_KERNEL_H
#include "runtime.h"

M8_MEM_INLINE uint8_t *m8_copy_ram(m8_mem *mem,uint32_t addr,uint32_t size) {
    uint8_t *p;
    if((p=m8_region_pointer(&mem->dtcm,addr,size))) return p;
    if((p=m8_region_pointer(&mem->ocram,addr,size))) return p;
    return m8_region_pointer(&mem->itcm,addr,size);
}

/* Return 1 after the complete architectural operation, or 0 before effects.
 * Both guest and host spans must be disjoint, aligned, and wholly in RAM.
 * The original routine copies one word at a time. Overlap and partial faults
 * therefore stay in translated/original code. Active IT transactions and
 * diagnostic instruction stepping must also keep the original instruction
 * boundaries. No callback or FP operation occurs in this shortcut. */
M8_MEM_INLINE int m8_memcpy_words(m8_cpu *restrict cpu,m8_mem *restrict mem) {
#ifdef M8_AOT_DIAGNOSTIC
    (void)cpu; (void)mem;
    return 0;
#else
    uint32_t dst=cpu->r[0],src=cpu->r[1],size=cpu->r[2];
    if(cpu->r[15]!=UINT32_C(0x326d4) || m8_current_it ||
       size<4 || ((dst|src|size)&3u)) return 0;
    uint32_t distance=dst<src ? src-dst : dst-src;
    if(size>distance) return 0;
    uint8_t *to=m8_copy_ram(mem,dst,size),*from=m8_copy_ram(mem,src,size);
    if(!to || !from) return 0;
    uintptr_t a=(uintptr_t)to,b=(uintptr_t)from;
    uintptr_t host_distance=a<b ? b-a : a-b;
    if(size>host_distance) return 0;
    uint32_t last=m8_le_read(from+size-4,4);
    memcpy(to,from,size);
    /* For every nonzero aligned word count, the final ADDS at 0x327a0 adds
     * 4 to 0xfffffffc: R2=0, N=0, Z=1, C=1, V=0. R3 retains the last load;
     * R1 advances to the end, while MOV R0,IP restores the destination. */
    cpu->r[1]=src+size;
    cpu->r[2]=0;
    cpu->r[3]=last;
    cpu->r[12]=dst;
    cpu->r[15]=cpu->r[14]&~1u;
    cpu->xpsr=(cpu->xpsr&~UINT32_C(0xf0000000))|UINT32_C(0x60000000);
    return 1;
#endif
}
#endif
