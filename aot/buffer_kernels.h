/* Complete Headless 6.5.2C buffer functions 0x36da0 and 0x36e0c.
 * Qualify the complete RAM footprint before the original PUSH effects.
 * Exact in-place buffers are permitted; partial write overlap falls back. */
#ifndef M8_AOT_BUFFER_KERNELS_H
#define M8_AOT_BUFFER_KERNELS_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
static inline int m8_buffer_disjoint(const void *a,uint32_t an,const void *b,uint32_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x>=an : x-y>=bn;
}
static inline float m8_buffer_load(const uint8_t *p) {return m8_fp_float(m8_le_read(p,4));}
static inline void m8_buffer_store(uint8_t *p,float v) {m8_le_write(p,4,m8_fp_bits32(v));}
static inline float m8_buffer_op(m8_cpu *cpu,int add,float a,float b) {
    return m8_fp_float(m8_fp_native32(cpu,add?M8_FP_ADD:M8_FP_MUL,m8_fp_bits32(a),m8_fp_bits32(b),0));
}
static inline int m8_buffer_safe_span(const void *host,uint32_t size,const m8_cpu *cpu,const m8_mem *mem) {
    return m8_buffer_disjoint(host,size,cpu,sizeof(*cpu)) &&
           m8_buffer_disjoint(host,size,mem,sizeof(*mem));
}
#if defined(__GNUC__) || defined(__clang__)
#define M8_BUFFER_INLINE static inline __attribute__((always_inline))
#define M8_BUFFER_NOINLINE __attribute__((noinline))
#else
#define M8_BUFFER_INLINE static inline
#define M8_BUFFER_NOINLINE
#endif
M8_BUFFER_INLINE int m8_buffer_kernel_inner(m8_cpu *cpu,m8_mem *mem,int add) {
    if(!m8_fp_native_supported(cpu) || (cpu->xpsr & 0x0700fc00u)!=0x01000000u ||
       !(cpu->r[14] & 1u)) return 0;
    uint32_t count=cpu->r[add?3:2],stack_size=add?20u:12u;
    if(count>UINT32_MAX/4u || cpu->r[13]<stack_size) return 0;
    uint32_t size=count*4u;
    uint8_t *stack=m8_span_pointer(mem,cpu->r[13]-stack_size,stack_size);
    if(!stack || !m8_buffer_safe_span(stack,stack_size,cpu,mem)) return 0;
    uint8_t *a=NULL,*b=NULL,*out=NULL;
    if(count) {
        a=m8_span_pointer(mem,cpu->r[0],size);
        out=m8_span_pointer(mem,cpu->r[add?2:1],size);
        b=add?m8_span_pointer(mem,cpu->r[1],size):NULL;
        if(!a || !out || (add && !b)) return 0;
        if((out!=a && !m8_buffer_disjoint(out,size,a,size)) ||
           (add && out!=b && !m8_buffer_disjoint(out,size,b,size))) return 0;
        if(!m8_buffer_safe_span(a,size,cpu,mem) ||
           !m8_buffer_safe_span(out,size,cpu,mem) ||
           !m8_buffer_disjoint(stack,stack_size,a,size) ||
           !m8_buffer_disjoint(stack,stack_size,out,size)) return 0;
        if(add && (!m8_buffer_safe_span(b,size,cpu,mem) ||
                   !m8_buffer_disjoint(stack,stack_size,b,size))) return 0;
    }
    /* The saved stack words remain visible after POP. */
    for(unsigned i=0;i<(add?4u:3u);i++) m8_le_write(stack+i*4u,4,cpu->r[4+i]);
    if(add) m8_le_write(stack+16u,4,cpu->r[14]);
    float s10=m8_fp_float(cpu->s[10]),s11=m8_fp_float(cpu->s[11]);
    float s12=m8_fp_float(cpu->s[12]),s13=m8_fp_float(cpu->s[13]);
    float s14=m8_fp_float(cpu->s[14]),s15=m8_fp_float(cpu->s[15]);
    float gain=m8_fp_float(cpu->s[0]);
    uint32_t groups=count/4u,tail=count & 3u;
#if defined(M8_NATIVE_FP_NO_STICKY) && defined(__aarch64__) && !defined(M8_AOT_DISABLE_BUFFER_NEON) && \
        defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    /* All lanes are independent. The span checks permit only disjoint or
     * exact in-place outputs, so both inputs can be read before the store. */
    typedef float m8_buffer_vec4 __attribute__((vector_size(16)));
    m8_buffer_vec4 va,vb,vr,vg={gain,gain,gain,gain};
    for(uint32_t i=0;i<groups;i++,a+=16,out+=16) {
        memcpy(&va,a,16);
        if(add) {
            memcpy(&vb,b,16);b+=16;
            __asm__("fadd %0.4s, %1.4s, %2.4s" : "=w"(vr) : "w"(va), "w"(vb));
        } else {
            __asm__("fmul %0.4s, %1.4s, %2.4s" : "=w"(vr) : "w"(va), "w"(vg));
        }
        memcpy(out,&vr,16);
    }
    if(groups) {
        if(add) {
            __asm__("mov %s0, %1.s[3]" : "=w"(s10) : "w"(vb));
            __asm__("mov %s0, %1.s[0]" : "=w"(s11) : "w"(vb));
            s12=vr[1];s13=vr[2];s14=vr[3];s15=vr[0];
        } else {
            s12=vr[0];s13=vr[1];s14=vr[2];s15=vr[3];
        }
    }
#else
    for(uint32_t i=0;i<groups;i++,a+=16,out+=16) {
        if(add) {
            float a1=m8_buffer_load(a+4),a2=m8_buffer_load(a+8);
            s12=m8_buffer_load(b+4);s13=m8_buffer_load(b+8);s10=m8_buffer_load(b+12);
            s12=m8_buffer_op(cpu,1,a1,s12);s11=m8_buffer_load(b);
            s13=m8_buffer_op(cpu,1,a2,s13);
            s14=m8_buffer_load(a+12);s15=m8_buffer_load(a);
            s14=m8_buffer_op(cpu,1,s14,s10);m8_buffer_store(out+4,s12);
            s15=m8_buffer_op(cpu,1,s15,s11);m8_buffer_store(out+8,s13);
            m8_buffer_store(out+12,s14);m8_buffer_store(out,s15);b+=16;
        } else {
            s12=m8_buffer_load(a);s13=m8_buffer_load(a+4);
            s14=m8_buffer_load(a+8);s15=m8_buffer_load(a+12);
            s12=m8_buffer_op(cpu,0,s12,gain);s13=m8_buffer_op(cpu,0,s13,gain);
            s14=m8_buffer_op(cpu,0,gain,s14);s15=m8_buffer_op(cpu,0,gain,s15);
            m8_buffer_store(out,s12);m8_buffer_store(out+4,s13);
            m8_buffer_store(out+8,s14);m8_buffer_store(out+12,s15);
        }
    }
#endif
    for(uint32_t i=0;i<tail;i++,a+=4,out+=4) {
        s15=m8_buffer_load(a);
        if(add) {s14=m8_buffer_load(b);s15=m8_buffer_op(cpu,1,s15,s14);b+=4;}
        else s15=m8_buffer_op(cpu,0,s15,gain);
        m8_buffer_store(out,s15);
    }
    if(groups) {
        cpu->s[12]=m8_fp_bits32(s12);cpu->s[13]=m8_fp_bits32(s13);
        if(add) {cpu->s[10]=m8_fp_bits32(s10);cpu->s[11]=m8_fp_bits32(s11);}
    }
    if(groups || (add && tail)) cpu->s[14]=m8_fp_bits32(s14);
    if(count) cpu->s[15]=m8_fp_bits32(s15);
    if(!add && groups) cpu->r[3]=cpu->r[1]+16u+groups*16u;
    cpu->r[0]+=size;cpu->r[1]+=size;
    if(add) {cpu->r[2]+=size;cpu->r[3]=0;}
    else cpu->r[2]=0;
    uint32_t carry=add?!!count:!!tail;
    uint32_t overflow=count?0:cpu->xpsr & 0x10000000u;
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|0x40000000u|(carry<<29)|overflow;
    cpu->r[15]=cpu->r[14] & ~1u;
    if(add) cpu->r[14]=groups;
    return 1;
}
M8_BUFFER_NOINLINE static int m8_buffer_gain_kernel(m8_cpu *cpu,m8_mem *mem) {
    return m8_buffer_kernel_inner(cpu,mem,0);
}
M8_BUFFER_NOINLINE static int m8_buffer_add_kernel(m8_cpu *cpu,m8_mem *mem) {
    return m8_buffer_kernel_inner(cpu,mem,1);
}
#undef M8_BUFFER_INLINE
#undef M8_BUFFER_NOINLINE
#endif
#endif
