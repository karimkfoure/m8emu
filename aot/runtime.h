#ifndef M8_AOT_RUNTIME_H
#define M8_AOT_RUNTIME_H
#include "m8_aot.h"
#include <stddef.h>
#include <string.h>
#include <fenv.h>
#if defined(__GNUC__) || defined(__clang__)
#define M8_MEM_INLINE static inline __attribute__((always_inline))
#define M8_MEM_COLD static __attribute__((cold,noinline))
#else
#define M8_MEM_INLINE static inline
#define M8_MEM_COLD static
#endif

/* IT blocks are atomic native units. A non-RAM access rolls back this bounded
 * journal before returning to Unicorn at the IT instruction. No MMIO callback
 * has run at that point. A valid ARM IT block has at most four instructions. */
typedef struct {
    m8_cpu before;
    fenv_t fp_before;
    struct { uint8_t *host; uint32_t old; uint8_t size; } writes[128];
    unsigned count;
    int forbid_slow, saw_slow;
#if !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    uint32_t r_mask, s_mask; /* r_mask bit 16 also records an FP snapshot. */
#endif
} m8_it_tx;
static _Thread_local m8_it_tx *m8_current_it;
static _Thread_local int m8_slow_fallback;
M8_MEM_INLINE uint8_t *m8_region_pointer(m8_region *r,uint32_t addr,uint32_t size) {
    uint32_t off=addr-r->base;
    return r->host && size<=r->size && off<=r->size-size ? r->host+off : NULL;
}
M8_MEM_INLINE uint8_t *m8_pointer(m8_mem *mem,uint32_t addr,uint32_t size) {
    uint8_t *p;
    /* DSP buffers and stack normally use DTCM; output buffers use OCRAM. */
    if((p=m8_region_pointer(&mem->dtcm,addr,size))) return p;
    if((p=m8_region_pointer(&mem->ocram,addr,size))) return p;
    if((p=m8_region_pointer(&mem->itcm,addr,size))) return p;
    return m8_region_pointer(&mem->flash,addr,size);
}
M8_MEM_INLINE uint32_t m8_le_read(const uint8_t *p,unsigned size) {
    uint32_t v=0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(&v,p,size);
#else
    for(unsigned i=0;i<size;i++) v|=(uint32_t)p[i]<<(8*i);
#endif
    return v;
}
M8_MEM_INLINE void m8_le_write(uint8_t *p,unsigned size,uint32_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(p,&v,size);
#else
    for(unsigned i=0;i<size;i++) p[i]=(uint8_t)(v>>(8*i));
#endif
}
M8_MEM_INLINE int m8_write_it(uint8_t *p,unsigned size,uint32_t v) {
    m8_it_tx *tx=m8_current_it;
    if(tx->count==128) return 2;
    tx->writes[tx->count].host=p; tx->writes[tx->count].size=(uint8_t)size;
    tx->writes[tx->count++].old=m8_le_read(p,size);
    m8_le_write(p,size,v); return 0;
}
/* m8rt maps DWT as RAM. Its clock advances only at explicit runtime time
 * boundaries, outside a native DSP call. This opt-in read does not access
 * MMIO, change time or deliver an IRQ. Other hosts must supply this contract. */
M8_MEM_COLD int m8_read_dwt_ram(m8_mem *mem,uint32_t addr,unsigned size,uint32_t *value) {
#ifdef M8_AOT_DWT_RAM
    if(size && size<=4 && (uint32_t)(addr-0xe0001000u)<=4096u-size && mem->read)
        return mem->read(mem->ctx,addr,size,value)==0;
#else
    (void)mem;(void)addr;(void)size;(void)value;
#endif
    return 0;
}
M8_MEM_INLINE int m8_nvic_word(uint32_t addr,unsigned size) {
    return size==4 && !(addr & 3u) &&
        ((uint32_t)(addr-0xe000e100u)<=28u || (uint32_t)(addr-0xe000e180u)<=28u);
}
M8_MEM_COLD int m8_read_cooperative(m8_mem *mem,uint32_t addr,unsigned size,uint32_t *value,int *slow) {
    int result=mem->read(mem->ctx,addr,size,value);
    if(result==M8_AOT_MEM_CONTINUE) return 0;
    if(result==0) { *slow=1; return 0; }
    return 1;
}
M8_MEM_COLD int m8_write_cooperative(m8_mem *mem,uint32_t addr,unsigned size,uint32_t value,int *slow) {
    int result=mem->write(mem->ctx,addr,size,value);
    if(result==M8_AOT_MEM_CONTINUE) return 0;
    if(result==0) { *slow=1; return 0; }
    return 1;
}
M8_MEM_COLD int m8_read_slow(m8_mem *mem,uint32_t addr,uint32_t size,uint32_t *v,int *slow) {
    if((m8_current_it && m8_current_it->forbid_slow) || m8_slow_fallback) return 2;
    if(m8_current_it) m8_current_it->saw_slow=1;
    *slow=1; return mem->read ? mem->read(mem->ctx,addr,size,v) : 1;
}
M8_MEM_INLINE int m8_read(m8_mem *mem,uint32_t addr,uint32_t size,uint32_t *v,int *slow) {
    uint8_t *p=m8_pointer(mem,addr,size);
    if(p) { *v=m8_le_read(p,size); return 0; }
    return m8_read_slow(mem,addr,size,v,slow);
}
M8_MEM_COLD int m8_write_slow(m8_mem *mem,uint32_t addr,uint32_t size,uint32_t v,int *slow) {
    if((m8_current_it && m8_current_it->forbid_slow) || m8_slow_fallback) return 2;
    if(m8_current_it) m8_current_it->saw_slow=1;
    *slow=1; return mem->write ? mem->write(mem->ctx,addr,size,v) : 1;
}
M8_MEM_INLINE int m8_write(m8_mem *mem,uint32_t addr,uint32_t size,uint32_t v,int *slow) {
    uint8_t *p=m8_pointer(mem,addr,size);
    if(p) {
        if(m8_current_it) {
            m8_it_tx *tx=m8_current_it;
            if(tx->count==128) return 2;
            tx->writes[tx->count].host=p; tx->writes[tx->count].size=(uint8_t)size;
            tx->writes[tx->count++].old=m8_le_read(p,size);
        }
        m8_le_write(p,size,v); return 0;
    }
    return m8_write_slow(mem,addr,size,v,slow);
}
/* A single transfer cannot need rollback after a successful RAM store. */
M8_MEM_INLINE int m8_write_plain(m8_mem *mem,uint32_t addr,uint32_t size,uint32_t v,int *slow) {
    uint8_t *p=m8_pointer(mem,addr,size);
    if(p) { m8_le_write(p,size,v); return 0; }
    return m8_write_slow(mem,addr,size,v,slow);
}
static inline void m8_it_begin(m8_it_tx *tx,m8_cpu *cpu) {
    tx->before=*cpu;
#ifndef M8_NATIVE_FP_NO_STICKY
    fegetenv(&tx->fp_before);
#endif
    tx->count=0; tx->forbid_slow=1; tx->saw_slow=0; m8_current_it=tx;
#if !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    tx->r_mask=0x1ffffu; tx->s_mask=UINT32_MAX;
#endif
}
/* The emitter proves the complete write set before it uses this entry. PC and
 * both flag words always survive rollback. Unknown units use m8_it_begin.
 * Integer and raw register-copy units cannot change the host FP environment.
 * Guest RAM cannot alias cpu under the existing restrict-qualified ABI. */
M8_MEM_INLINE void m8_it_begin_partial(m8_it_tx *tx,m8_cpu *cpu,
                                      uint32_t r_mask,uint32_t s_mask,int save_fp) {
#if !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    r_mask|=0x8000u;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC unroll 16
#endif
    for(unsigned i=0;i<16;i++) if(r_mask & (UINT32_C(1)<<i)) tx->before.r[i]=cpu->r[i];
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC unroll 32
#endif
    for(unsigned i=0;i<32;i++) if(s_mask & (UINT32_C(1)<<i)) tx->before.s[i]=cpu->s[i];
    tx->before.xpsr=cpu->xpsr; tx->before.fpscr=cpu->fpscr;
#ifndef M8_NATIVE_FP_NO_STICKY
    if(save_fp) fegetenv(&tx->fp_before);
#else
    (void)save_fp;
#endif
    tx->r_mask=(r_mask & 0xffffu) | (save_fp ? 0x10000u : 0); tx->s_mask=s_mask;
    tx->count=0; tx->forbid_slow=1; tx->saw_slow=0; m8_current_it=tx;
#else
    (void)r_mask; (void)s_mask; (void)save_fp; m8_it_begin(tx,cpu);
#endif
}
static inline void m8_it_commit(void) { m8_current_it=NULL; }
static inline void m8_it_abort(m8_cpu *cpu) {
    m8_it_tx *tx=m8_current_it;
    while(tx->count && !tx->saw_slow) {
        unsigned i=--tx->count;
        m8_le_write(tx->writes[i].host,tx->writes[i].size,tx->writes[i].old);
    }
#if !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    for(unsigned i=0;i<16;i++) if(tx->r_mask & (UINT32_C(1)<<i)) cpu->r[i]=tx->before.r[i];
    for(unsigned i=0;i<32;i++) if(tx->s_mask & (UINT32_C(1)<<i)) cpu->s[i]=tx->before.s[i];
    cpu->xpsr=tx->before.xpsr; cpu->fpscr=tx->before.fpscr;
#ifndef M8_NATIVE_FP_NO_STICKY
    if(tx->r_mask & 0x10000u) fesetenv(&tx->fp_before);
#else
    (void)0;
#endif
#else
    *cpu=tx->before;
#ifndef M8_NATIVE_FP_NO_STICKY
    fesetenv(&tx->fp_before);
#endif
#endif
    m8_current_it=NULL;
}
#endif
