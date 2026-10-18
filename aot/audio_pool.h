/* Exact integer audio-pool shortcuts for the original Headless 6.5.2C. */
#ifndef M8_AOT_AUDIO_POOL_H
#define M8_AOT_AUDIO_POOL_H
#include "runtime.h"
#include "integer.h"

/* These helpers only write data RAM. The fixed map is qualified by m8rt at
 * library admission; other hosts retain the region/host-alias checks. */
static inline int m8_pool_overlap(const uint8_t *a,unsigned an,const uint8_t *b,unsigned bn) {
    return (uintptr_t)a<(uintptr_t)b+bn && (uintptr_t)b<(uintptr_t)a+an;
}
static inline uint8_t *m8_pool_ram(m8_mem *mem,uint32_t addr,uint32_t n) {
#if defined(M8_AOT_FIXED_MEMORY) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
    if(n<=0x80000u && ((uint32_t)(addr-0x20000000u)<=0x80000u-n ||
                       (uint32_t)(addr-0x20200000u)<=0x80000u-n))
        return (uint8_t *)((uintptr_t)mem->itcm.host+addr);
    return NULL;
#else
    uint8_t *p=m8_region_pointer(&mem->dtcm,addr,n);
    if(!p) p=m8_region_pointer(&mem->ocram,addr,n);
    if(!p || p!=m8_pointer(mem,addr,n) ||
       (mem->itcm.host && m8_pool_overlap(p,n,mem->itcm.host,mem->itcm.size)) ||
       (mem->flash.host && m8_pool_overlap(p,n,mem->flash.host,mem->flash.size))) return NULL;
    return p;
#endif
}
/* 1: complete function, 2: completed access requiring admission, 3: fault.
 * Callback result0 completes the instruction and stops at its successor;
 * result2 permits continued execution. A failed access retains its own PC. */
static inline int m8_pool_read(m8_cpu *c,m8_mem *m,unsigned reg,uint32_t pc) {
    uint32_t value;
    c->r[15]=pc;
    int result=m->read(m->ctx,0xe000e108u,4,&value);
    c->r[15]=pc;
    if(result!=0 && result!=M8_AOT_MEM_CONTINUE) return 3;
    c->r[reg]=value; c->r[15]=pc+4;
    return result==0 ? 2 : 0;
}
static inline int m8_pool_write(m8_cpu *c,m8_mem *m,uint32_t addr,uint32_t pc) {
    c->r[15]=pc;
    int result=m->write(m->ctx,addr,4,0x40u);
    c->r[15]=pc;
    if(result!=0 && result!=M8_AOT_MEM_CONTINUE) return 3;
    c->r[15]=pc+4;
    return result==0 ? 2 : 0;
}
static inline void m8_pool_push(m8_cpu *c,uint8_t *stack,unsigned words) {
    for(unsigned i=0;i<words;i++) m8_le_write(stack+4*i,4,c->r[4+i]);
    c->r[13]-=words*4;
}
static inline void m8_pool_pop(m8_cpu *c,uint8_t *stack,unsigned words) {
    for(unsigned i=0;i<words;i++) c->r[4+i]=m8_le_read(stack+4*i,4);
    c->r[13]+=words*4;
}
static inline int m8_pool_return(m8_cpu *c) { c->r[15]=c->r[14]&~1u; return 1; }

/* Register-threaded NZCV helpers: bit-exact copies of the m8_nz /
 * m8_logic_flags / m8_add_flags / m8_sub_flags formulas from integer.h,
 * applied to a caller-owned word instead of cpu->xpsr. The kernels flush the
 * word to c->xpsr before every mem callback (the CPU words are part of the
 * recorded access state) and before every return that can follow a flag
 * update, so guest-visible state is unchanged while intermediate flag
 * traffic stays in a register. Decline paths return before any flag update
 * and leave xpsr untouched. */
static inline void m8_pool_nz(uint32_t *xpsr,uint32_t x) {
    *xpsr=(*xpsr & ~UINT32_C(0xc0000000)) | (x & UINT32_C(0x80000000)) | (x ? 0 : UINT32_C(0x40000000));
}
static inline void m8_pool_logic(uint32_t *xpsr,uint32_t x,uint32_t carry) {
    m8_pool_nz(xpsr,x); *xpsr=(*xpsr & ~UINT32_C(0x20000000)) | (carry << 29);
}
static inline uint32_t m8_pool_add(uint32_t *xpsr,uint32_t a,uint32_t b,uint32_t carry) {
    uint64_t sum=(uint64_t)a+b+carry;
    uint32_t r=(uint32_t)sum;
    m8_pool_nz(xpsr,r);
    *xpsr=(*xpsr & ~UINT32_C(0x30000000)) | ((uint32_t)(sum >> 32) << 29) | ((~(a ^ b) & (a ^ r) & UINT32_C(0x80000000)) >> 3);
    return r;
}
static inline uint32_t m8_pool_sub(uint32_t *xpsr,uint32_t a,uint32_t b,uint32_t borrow) {
    uint64_t sub=(uint64_t)b+borrow;
    uint32_t r=a-b-borrow;
    m8_pool_nz(xpsr,r);
    *xpsr=(*xpsr & ~UINT32_C(0x30000000)) | ((uint32_t)((uint64_t)a >= sub) << 29) | (((a ^ b) & (a ^ r) & UINT32_C(0x80000000)) >> 3);
    return r;
}

/* Net-zero NVIC mask pair. Allocate/release surround their critical sections
 * with RD ISER -> WR ICER -> WR ISER of bit 0x40; the two writes only clear
 * and restore that bit, so the net register change is zero (proven in
 * docs/native-pool-nvic-hle.md section 3). Between the opening read and the
 * writes the kernels run only register arithmetic and plain data-RAM
 * stores, which cannot change runtime service state, so writes that would
 * run then return whatever the opening read returned. Elision additionally
 * requires a running handler (IPSR!=0, hence irq_nest>0 with no mid-sequence
 * delivery) and the mask bit set. Every other state -- thread-mode callers,
 * declined or faulted reads, a cleared mask bit, instrumented buses -- runs
 * the original calls verbatim, preserving exact ordered callbacks. */
static inline int m8_pool_nvic_skip(const m8_cpu *c) { return (c->xpsr & 0x1ffu) != 0; }
static inline int m8_pool_write_masked(m8_cpu *c,m8_mem *m,uint32_t addr,uint32_t pc,int skip) {
    if(skip) { c->r[15]=pc; c->r[15]=pc+4; return 0; }
    return m8_pool_write(c,m,addr,pc);
}

static int m8_audio_pool_allocate(m8_cpu *c,m8_mem *m) {
    uint32_t xpsr=c->xpsr;
    int skip_nvic=0;
    uint8_t *globals=m8_pool_ram(m,0x20019228u,44),*counts=m8_pool_ram(m,0x2003b472u,4);
    uint8_t *stack=m8_pool_ram(m,c->r[13]-8,8);
    if(!globals || !counts || !stack || !m->read || !m->write) return 0;
    uint32_t base=m8_le_read(globals,4);
    uint8_t *blocks=m8_pool_ram(m,base,320u*276u);
    if((uint64_t)base+320u*276u>UINT32_MAX || !blocks ||
       m8_pool_overlap(stack,8,globals,44) || m8_pool_overlap(stack,8,counts,4) ||
       m8_pool_overlap(stack,8,blocks,320u*276u) ||
       m8_pool_overlap(blocks,320u*276u,globals,44) ||
       m8_pool_overlap(blocks,320u*276u,counts,4)) return 0;
    c->r[2]=0xe000e000u;
    int result=m8_pool_read(c,m,1,0x2ac14u); if(result) return result;
    c->r[1]&=0x40u; m8_pool_nz(&xpsr,c->r[1]);
    unsigned enabled=c->r[1]!=0;
    skip_nvic=enabled && m8_pool_nvic_skip(c);
    m8_pool_push(c,stack,2);
    if(enabled) { c->r[3]=0x40u; m8_pool_nz(&xpsr,c->r[3]); }
    c->r[4]=0x2001922cu;
    if(enabled) { c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e188u,0x2ac24u,skip_nvic); xpsr=c->xpsr; if(result) return result; }
    c->r[3]=m8_le_read(globals+4,4);
    if(enabled) (void)m8_pool_sub(&xpsr,c->r[3],0,0);
    unsigned group=0;
    uint32_t mask=c->r[3];
    while(!mask && group<9) {
        ++group; mask=m8_le_read(globals+4+group*4,4);
        if(group==1) c->r[3]=mask; else c->r[0]=mask;
    }
    if(!mask) {
        if(enabled) {
            c->r[3]=0xe000e000u; c->r[2]=0x40u; m8_pool_nz(&xpsr,c->r[2]);
            c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e108u,0x2ad3eu,skip_nvic); xpsr=c->xpsr; if(result) return result;
        } else c->r[0]=c->r[1];
        m8_pool_pop(c,stack,2); c->xpsr=xpsr; return m8_pool_return(c);
    }
    c->r[12]=m8_clz(mask);
    uint32_t bit=0x80000000u>>c->r[12];
    uint32_t used=m8_le_read(counts,2);
    if(group>=2) {
        c->r[5]=group*0x2280u; c->r[4]=0x2001922cu+group*4;
        c->r[2]=0x2003b472u; c->r[0]=mask&~bit;
        c->r[3]=m8_pool_add(&xpsr,used,1,0);
        m8_le_write(globals+4+group*4,4,c->r[0]);
    } else if(group==0 && !enabled) {
        c->r[2]=0x2003b472u; c->r[3]=mask&~bit;
        m8_le_write(globals+4,4,c->r[3]);
        c->r[3]=m8_pool_add(&xpsr,used,1,0);
    } else {
        c->r[0]=0x2003b472u;
        if(group) c->r[1]=0x2280u;
        else { c->r[1]=0; m8_pool_nz(&xpsr,0); }
        c->r[2]=mask&~bit; c->r[3]=m8_pool_add(&xpsr,used,1,0);
        m8_le_write(globals+4+group*4,4,c->r[2]);
    }
    c->r[3]&=0xffffu; m8_le_write(counts,2,c->r[3]);
    if(group>=2) c->r[1]=c->r[5];
    if(enabled) {
        c->r[2]=0xe000e000u; c->r[0]=0x40u; m8_pool_nz(&xpsr,c->r[0]);
        c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e108u,0x2ac52u,skip_nvic); xpsr=c->xpsr; if(result) return result;
    }
    c->r[2]=0x216cu+c->r[1]-276u*c->r[12];
    c->r[4]=0xfffffeecu; c->r[0]=base; c->r[1]=0x2003b474u; c->r[12]=1;
    blocks[group*0x2280u+276u*(31u-m8_clz(mask))]=1;
    c->r[0]+=c->r[2]; c->r[2]=m8_le_read(counts+2,2);
    (void)m8_pool_sub(&xpsr,c->r[2],c->r[3],0);
    if(c->r[2]<c->r[3]) m8_le_write(counts+2,2,c->r[3]);
    m8_pool_pop(c,stack,2); c->xpsr=xpsr; return m8_pool_return(c);
}

static int m8_audio_pool_release(m8_cpu *c,m8_mem *m) {
    uint32_t xpsr=c->xpsr;
    int skip_nvic=0;
    uint8_t *block=m8_pool_ram(m,c->r[0],2),*masks=m8_pool_ram(m,0x2001922cu,40);
    uint8_t *counts=m8_pool_ram(m,0x2003b472u,4),*stack=m8_pool_ram(m,c->r[13]-8,8);
    if(!block || !masks || !counts || !stack || !m->read || !m->write ||
       m8_pool_overlap(stack,8,block,2) || m8_pool_overlap(stack,8,masks,40) ||
       m8_pool_overlap(stack,8,counts,4) || m8_pool_overlap(block,2,masks,40) ||
       m8_pool_overlap(block,2,counts,4)) return 0;
    c->r[12]=0xe000e000u; c->r[2]=block[1];
    int result=m8_pool_read(c,m,1,0x2adc2u); if(result) return result;
    c->r[1]&=0x40u; m8_pool_nz(&xpsr,c->r[1]); unsigned enabled=c->r[1]!=0;
    skip_nvic=enabled && m8_pool_nvic_skip(c);
    if(enabled) {
        c->r[3]=0x40u; m8_pool_nz(&xpsr,c->r[3]);
        c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e188u,0x2adceu,skip_nvic); xpsr=c->xpsr; if(result) return result;
    }
    c->r[3]=block[0]; (void)m8_pool_sub(&xpsr,c->r[3],1,0);
    if(c->r[3]>1) {
        c->r[3]=m8_pool_sub(&xpsr,c->r[3],1,0); block[0]=(uint8_t)c->r[3];
        if(enabled) {
            c->r[3]=0xe000e000u; c->r[2]=0x40u; m8_pool_nz(&xpsr,c->r[2]);
            c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e108u,0x2ade4u,skip_nvic); xpsr=c->xpsr; if(result) return result;
        }
        c->xpsr=xpsr; return m8_pool_return(c);
    }
    c->r[0]=~c->r[2]; m8_pool_nz(&xpsr,c->r[0]); c->r[3]=0x80000000u;
    uint32_t carry=(c->r[2]>>4)&1u;
    c->r[2]>>=5; m8_pool_logic(&xpsr,c->r[2],carry); c->r[0]&=31u;
    m8_pool_push(c,stack,2); c->r[5]=0x2001922cu;
    c->r[3]=m8_shift(c->r[3],3,c->r[0],&carry); m8_pool_logic(&xpsr,c->r[3],carry);
    c->r[4]=0x2003b472u; c->r[0]=m8_le_read(masks+c->r[2]*4,4);
    c->r[3]|=c->r[0]; m8_pool_nz(&xpsr,c->r[3]); m8_le_write(masks+c->r[2]*4,4,c->r[3]);
    c->r[3]=m8_pool_sub(&xpsr,m8_le_read(counts,2),1,0); m8_le_write(counts,2,c->r[3]);
    if(enabled) {
        c->r[3]=0xe000e000u; c->r[2]=0x40u; m8_pool_nz(&xpsr,c->r[2]);
        c->xpsr=xpsr; result=m8_pool_write_masked(c,m,0xe000e108u,0x2ae18u,skip_nvic); xpsr=c->xpsr; if(result) return result;
    }
    m8_pool_pop(c,stack,2); c->xpsr=xpsr; return m8_pool_return(c);
}

/* Publication qualification rejects a long/cyclic graph and all aliases of
 * saved registers, link metadata and refcount with writable input slots.
 * Rejected inputs retain the original instruction-by-instruction path.
 * Alias checks are deferred past the route walk: per-node range checks stay
 * inline (they guard the walk's own dereferences), while all
 * stack/block/source/slot overlap pairs move into one post-walk pass over
 * the recorded addresses. A conservative address box over every route node
 * and owner filters the quadratic slot-vs-node/owner pass; a box miss
 * proves disjointness, a box hit runs the original per-pair checks. The
 * decline set is identical -- declines have no effects, so check order is
 * unobservable -- and accepted inputs take the same action loop. */
static int m8_audio_pool_publish(m8_cpu *c,m8_mem *m) {
    struct route { uint32_t pc; uint8_t *node,*owner,*inputs,*slot; } routes[32];
    uint32_t xpsr=c->xpsr;
    uintptr_t box_lo=(uintptr_t)-1,box_hi=0;
    uint8_t *source=m8_pool_ram(m,c->r[0]+0x1c,4),*block=m8_pool_ram(m,c->r[1],1);
    uint8_t *stack=m8_pool_ram(m,c->r[13]-4,4);
    if(!source || !block || !stack) return 0;
    uint32_t next=m8_le_read(source,4); unsigned count=0;
    while(next) {
        if(count==32) return 0;
        struct route *r=&routes[count++]; r->pc=next; r->node=m8_pool_ram(m,next,16);
        if(!r->node) return 0;
        { uintptr_t lo=(uintptr_t)r->node; if(lo<box_lo) box_lo=lo; if(lo+16>box_hi) box_hi=lo+16; }
        r->owner=NULL; r->inputs=NULL; r->slot=NULL;
        if(r->node[8]==c->r[2]) {
            uint32_t owner=m8_le_read(r->node+4,4);
            r->owner=m8_pool_ram(m,owner+0x20,4); if(!r->owner) return 0;
            { uintptr_t lo=(uintptr_t)r->owner; if(lo<box_lo) box_lo=lo; if(lo+4>box_hi) box_hi=lo+4; }
            uint32_t inputs=m8_le_read(r->owner,4);
            r->inputs=m8_pool_ram(m,inputs+(uint32_t)r->node[9]*4,4); r->slot=r->inputs;
            if(!r->slot) return 0;
        }
        next=m8_le_read(r->node+12,4);
    }
    if(count && (m8_pool_overlap((const uint8_t *)box_lo,(unsigned)(box_hi-box_lo),stack,4) ||
                 m8_pool_overlap((const uint8_t *)box_lo,(unsigned)(box_hi-box_lo),block,1))) {
        for(unsigned j=0;j<count;j++) {
            if(m8_pool_overlap(routes[j].node,16,stack,4) || m8_pool_overlap(routes[j].node,16,block,1)) return 0;
            if(routes[j].owner && (m8_pool_overlap(routes[j].owner,4,stack,4) ||
                                   m8_pool_overlap(routes[j].owner,4,block,1))) return 0;
        }
    }
    for(unsigned i=0;i<count;i++) if(routes[i].slot) {
        if(m8_pool_overlap(routes[i].slot,4,stack,4) || m8_pool_overlap(routes[i].slot,4,block,1) ||
           m8_pool_overlap(routes[i].slot,4,source,4)) return 0;
        { uintptr_t s=(uintptr_t)routes[i].slot;
          if(s+4>box_lo && s<box_hi)
              for(unsigned j=0;j<count;j++)
                  if(m8_pool_overlap(routes[i].slot,4,routes[j].node,16) ||
                     (routes[j].owner && m8_pool_overlap(routes[i].slot,4,routes[j].owner,4))) return 0; }
    }
    c->r[3]=m8_le_read(source,4); unsigned i=0;
    while(i<count) {
        c->r[0]=routes[i].node[8]; (void)m8_pool_sub(&xpsr,c->r[0],c->r[2],0);
        if(c->r[0]==c->r[2]) break;
        c->r[3]=m8_le_read(routes[i++].node+12,4);
    }
    if(i==count) { c->xpsr=xpsr; return m8_pool_return(c); }
    m8_pool_push(c,stack,1);
    for(;;) {
        c->r[0]=m8_le_read(routes[i].owner,4); c->r[12]=routes[i].node[9];
        c->r[4]=m8_le_read(routes[i].slot,4);
        if(!c->r[4]) {
            m8_le_write(routes[i].slot,4,c->r[1]);
            c->r[0]=m8_pool_add(&xpsr,block[0],1,0); block[0]=(uint8_t)c->r[0];
        }
        c->r[3]=m8_le_read(routes[i++].node+12,4);
        if(i==count) break;
        for(;;) {
            c->r[0]=routes[i].node[8]; (void)m8_pool_sub(&xpsr,c->r[0],c->r[2],0);
            if(c->r[0]==c->r[2]) break;
            c->r[3]=m8_le_read(routes[i++].node+12,4); (void)m8_pool_sub(&xpsr,c->r[3],0,0);
            if(i==count) goto finished;
        }
    }
finished:
    m8_pool_pop(c,stack,1); c->xpsr=xpsr; return m8_pool_return(c);
}

static inline int m8_audio_pool_control(m8_cpu *cpu,m8_mem *mem) {
#if defined(M8_AOT_DIAGNOSTIC) || defined(M8_AOT_GUEST_COUNT)
    (void)cpu;(void)mem; return 0;
#else
    if(m8_current_it || m8_slow_fallback || (cpu->r[13]&3u) || (cpu->xpsr&0x0600fc00u) || !(cpu->r[14]&1u) || cpu->r[14]>=0xfffffff0u) return 0;
    switch(cpu->r[15]) {
    case 0x2ac10u: return m8_audio_pool_allocate(cpu,mem);
    case 0x2adbcu: return m8_audio_pool_release(cpu,mem);
    case 0x2ae28u: return m8_audio_pool_publish(cpu,mem);
    default: return 0;
    }
#endif
}
#endif
