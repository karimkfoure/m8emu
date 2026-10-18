/* Headless 6.5.2C FM parameter tail: 0x13a46..0x13b54. Select the
 * sixteen modulation pointers, then restore the original stack frame.
 * Read the live unit-pointer literal. Rejection has no CPU/RAM effects.
 */
#ifndef M8_AOT_FM_MODULATION_TAIL_H
#define M8_AOT_FM_MODULATION_TAIL_H
#include "runtime.h"
#if defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && \
    !defined(M8_AOT_GUEST_COUNT) && !defined(M8_AOT_DISABLE_FM_MODULATION_TAIL)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FMT_INLINE static inline __attribute__((always_inline))
#define M8_FMT_NOINLINE __attribute__((noinline))
#else
#define M8_FMT_INLINE static inline
#define M8_FMT_NOINLINE
#endif
M8_FMT_INLINE int m8_fmt_overlap(const uint8_t *a,unsigned na,
                                const uint8_t *b,unsigned nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_FMT_INLINE void m8_fmt_store(uint8_t *p,uint32_t value) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(p,&value,4);
#else
    m8_le_write(p,4,value);
#endif
}
M8_FMT_NOINLINE static int m8_fm_modulation_tail_kernel(m8_cpu *restrict cpu,
                                                       m8_mem *restrict mem) {
    uint32_t address=cpu->r[4],sp=cpu->r[13];
    if (cpu->r[15]!=0x13a46u ||
        (cpu->xpsr & 0x0700fc00u)!=0x01000000u ||
        address>UINT32_MAX-0x24bu || sp>UINT32_MAX-32u || ((address|sp)&3u) ||
        mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
        mem->itcm.base!=0u || mem->itcm.size>0x20000000u ||
        (uint32_t)(sp-address+31u)<=618u) return 0;
    /* Buffers that overlap CPU or descriptor storage would make register
     * writeback alter guest RAM. Distinct mapped regions must also be
     * independent, including regions this block does not access directly.
     */
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned i=0;i<4;i++) {
        const m8_region *r=regions[i];
        if (m8_fmt_overlap(r->host,r->size,(const uint8_t *)cpu,sizeof(*cpu)) ||
            m8_fmt_overlap(r->host,r->size,(const uint8_t *)mem,sizeof(*mem))) return 0;
        for (unsigned j=0;j<i;j++) {
            if (m8_fmt_overlap(r->host,r->size,regions[j]->host,regions[j]->size)) return 0;
        }
    }
    uint8_t *restrict object=m8_region_pointer(&mem->dtcm,address,0x24c);
    uint8_t *restrict stack=m8_region_pointer(&mem->dtcm,sp,32);
    const uint8_t *literal=m8_region_pointer(&mem->itcm,0x13d9cu,4);
    if (!object || !stack || !literal) return 0;
    uint32_t target=m8_le_read(stack+28,4);
    if (!(target&1u) || target>=0xfffffff0u) return 0;
    uint32_t unit=m8_le_read(literal,4);
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_FM_TAIL_SCALAR_SELECT) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* All spans are qualified and source fields are separate from the
     * destination fields. Integer vector selects preserve all pointer bits.
     * The same C extension lowers to SSE or NEON; no ISA intrinsic is used.
     */
    typedef uint32_t u4 __attribute__((vector_size(16)));
    const u4 zero={0,0,0,0},one={1,1,1,1},four={4,4,4,4};
    u4 units={unit,unit,unit,unit};
    for (unsigned group=0;group<2;group++) {
        unsigned i=4*group;
        u4 source;memcpy(&source,object+0x1c8+4*i,16);
        u4 codes={object[0x1f4+i],object[0x1f5+i],object[0x1f6+i],object[0x1f7+i]};
        u4 equal=(u4)(codes==one),bit=(u4)((codes&four)!=zero);
        u4 first=(source&equal)|(units&~equal);
        u4 second=(source&bit)|(units&~bit);
        memcpy(object+0x20c+4*i,&first,16);
        memcpy(object+0x22c+4*i,&second,16);
    }
#else
    for (unsigned i=0;i<8;i++) {
        uint32_t source=m8_le_read(object+0x1c8+4*i,4);
        uint8_t code=object[0x1f4+i];
        m8_fmt_store(object+0x20c+4*i,code==1 ? source : unit);
        m8_fmt_store(object+0x22c+4*i,(code&4u) ? source : unit);
    }
#endif
    cpu->r[0]=m8_le_read(object+0x23c,4);
    cpu->r[1]=m8_le_read(object+0x240,4);
    cpu->r[2]=m8_le_read(object+0x244,4);
    cpu->r[12]=object[0x1f6]; cpu->r[14]=object[0x1f5];
    cpu->s[16]=m8_le_read(stack,4); cpu->s[17]=m8_le_read(stack+4,4);
    for (unsigned i=0;i<5;i++) cpu->r[3+i]=m8_le_read(stack+8+4*i,4);
    cpu->r[13]=sp+32u; cpu->r[15]=target&~1u;
    /* CMP byte,#1 cannot set V. The last LSLS byte,#29 supplies N/Z/C.
     * The POP restores R3 but leaves these condition flags unchanged.
     */
    uint32_t code=object[0x1fb];
    uint32_t flags=((code&4u) ? 0x80000000u : 0u) |
                   ((code&7u) ? 0u : 0x40000000u) |
                   ((code&8u) ? 0x20000000u : 0u);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|flags;
    return 1;
}
#undef M8_FMT_INLINE
#undef M8_FMT_NOINLINE
#else
static inline int m8_fm_modulation_tail_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu; (void)mem; return 0;
}
#endif
#endif
