/* Exact fallback for the Headless 6.5.2C FM quantum, 1409c..14302.
 * The bounded native kernels remain first. This explicit candidate admits
 * distinct RAM modulation/PM pointers and arbitrary FP values. Every FP
 * operation uses exact helpers, including saturating conversions. The fast
 * engine selects inline native operations after the fixed-mode check.
 */
#ifndef M8_AOT_FM_STRUCTURAL_H
#define M8_AOT_FM_STRUCTURAL_H
#if defined(M8_AOT_FM_STRUCTURAL)
#include "fm_fast_fp.h"
#define M8_FMS_INLINE static inline __attribute__((always_inline))
#define M8_FMS_NOINLINE static __attribute__((noinline))
typedef struct m8_fms_context {
    uint8_t *object,*stack,*ports,*matrix,*control,*constants;
    uint8_t *mod[16],*pm[4];
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    const uint8_t *bias[4];uint32_t bias_address[4];
#endif
    uint32_t address,base,matrix_address,control_address;
} m8_fms_context;
M8_FMS_INLINE uint8_t *m8_fms_ram(m8_mem *mem,uint32_t address,unsigned size) {
    uint8_t *p=m8_region_pointer(&mem->dtcm,address,size);
    return p ? p : m8_region_pointer(&mem->ocram,address,size);
}
/* Only qualify addresses here. Values can depend on earlier operator stores.
 * No callback, FP operation or mutable RAM write occurs on rejection. */
M8_FMS_NOINLINE int m8_fms_prepare(m8_cpu *cpu,m8_mem *mem,
                                  uint32_t entry,m8_fms_context *c) {
    uint32_t a=cpu->r[8],sp=cpu->r[13];
    if(cpu->r[15]!=entry || cpu->s[17] ||
#if defined(M8_AOT_FM_FAST) && \
    (defined(M8_NATIVE_FP_NO_STICKY) || defined(M8_WEB_FP_NO_STICKY))
       (cpu->fpscr&0x03c00000u)!=0x03000000u ||
#else
       (cpu->fpscr&0x03c00010u)!=0x03000010u ||
#endif
       (cpu->xpsr&0x0700fc00u)!=0x01000000u ||
       a>UINT32_MAX-0x24bu || sp<24u || ((a|sp)&3u) ||
       cpu->r[9]!=a+0xf4u || cpu->r[10]!=a+0xa4u ||
       cpu->r[11]!=a+0x54u || cpu->s[25]!=a+0x144u ||
       mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
       mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u ||
       mem->itcm.base || mem->itcm.size>0x20000000u) return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++) {
        const m8_region *r=regions[i];
        if(r->size && (!r->host || (uintptr_t)r->host>UINTPTR_MAX-r->size)) return 0;
        if(m8_fm_overlap(r->host,r->size,(uint8_t *)cpu,sizeof(*cpu)) ||
           m8_fm_overlap(r->host,r->size,(uint8_t *)mem,sizeof(*mem))) return 0;
        for(unsigned j=0;j<i;j++)
            if(m8_fm_overlap(r->host,r->size,regions[j]->host,regions[j]->size)) return 0;
    }
    c->object=m8_region_pointer(&mem->dtcm,a,0x24c);
    c->stack=m8_region_pointer(&mem->dtcm,sp-12u,12);
    c->constants=m8_region_pointer(&mem->itcm,0x124a8u,28);
    if(!c->object || !c->stack || !c->constants ||
       m8_fm_overlap(c->object,0x24c,c->stack,12) ||
       !m8_fm_literals(c->constants)) return 0;
    c->address=a;c->base=m8_le_read(c->object+0x70,4);
    c->matrix_address=m8_le_read(c->object+0x1f0,4);
    c->control_address=m8_le_read(c->object+0x200,4);
    if((c->base|c->matrix_address|c->control_address)&3u) return 0;
    c->ports=m8_region_pointer(&mem->ocram,c->base,224);
    c->matrix=m8_region_pointer(&mem->ocram,c->matrix_address,16);
    c->control=m8_region_pointer(&mem->ocram,c->control_address,128);
    if(!c->ports || !c->matrix || !c->control) return 0;
    const uint8_t *spans[]={c->object,c->stack,c->ports,c->matrix,c->control,c->constants};
    const unsigned sizes[]={0x24c,12,224,16,128,28};
    for(unsigned i=0;i<6;i++) for(unsigned j=0;j<i;j++)
        if(m8_fm_overlap(spans[i],sizes[i],spans[j],sizes[j])) return 0;
    for(unsigned i=0;i<16;i++) {
        uint32_t p=m8_le_read(c->object+0x20c+4*i,4);
        if((p&3u) || !(c->mod[i]=m8_fms_ram(mem,p,4))) return 0;
    }
    for(unsigned i=0;i<4;i++) {
        unsigned off=0x54+0x50*i;const uint8_t *op=c->object+off;
        uint32_t p=c->base+64*i,pm=m8_le_read(op+0x44,4);
#if defined(M8_AOT_FM_FAST_MODES)
        if((op[0x3a] && (op[0x3a]<7 || op[0x3a]>8)) || m8_le_read(op,4)!=p+8u ||
#elif defined(M8_AOT_FM_MODE8)
        if((op[0x3a] && op[0x3a]!=8) || m8_le_read(op,4)!=p+8u ||
#else
        if(op[0x3a] || m8_le_read(op,4)!=p+8u ||
#endif
           m8_le_read(op+0x1c,4)!=p || m8_le_read(op+0x24,4)!=p+28u ||
           m8_le_read(op+0x28,4)!=p+12u || m8_le_read(op+0x2c,4)!=p+16u ||
           m8_le_read(op+0x30,4)!=p+20u ||
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
           ((c->bias_address[i]=m8_le_read(op+0x48,4)),
            ((c->bias_address[i]&3u) || !(c->bias[i]=m8_fms_ram(mem,c->bias_address[i],4)))) ||
#else
           m8_le_read(op+0x48,4)!=a+off+16u ||
#endif
           (pm&3u) || !(c->pm[i]=m8_fms_ram(mem,pm,4))) return 0;
    }
    /* Earlier writes reach only object phase/total/flag words, interpolation
     * state, stack and ports. They cannot reach the16pointer fields or op+44.
     * The checked source addresses therefore stay fixed until the callback.
     * Under dynamic bias, op+48 pointer fields are also outside every phase,
     * total, flag, interpolation and filter write. Disjoint stack, control,
     * ports, output and ring spans cannot change these fields. Bias values
     * are not hoisted: the operator loads them after the two gain stores.
     * Modulation and PM VALUES may alias those writes: load them in order.
     * There is deliberately no restrict on the object/ports/PM access paths. */
    return 1;
}
M8_FMS_INLINE uint32_t m8_fms_op(m8_cpu *cpu,int op,uint32_t a,uint32_t b,uint32_t d) {
#if defined(M8_AOT_FM_FAST)
    return m8_fms_fast_op(cpu,op,a,b,d);
#else
    return m8_fp_op32(cpu,op,a,b,d);
#endif
}
/* Keep literal opcodes visible to the existing exact float-register bridge.
 * This changes only FM call placement. The original integer helper already
 * has the same bounded arithmetic admission and full software fallback.
 * Hardware-native retains its complete input-denormal/status path above. */
#if defined(M8_WEB_FM_STRUCTURAL_FP_ABI) && \
    (defined(__wasm__) || defined(M8_WEB_FP_TEST))
#if !defined(M8_WEB_NATIVE_FP) || !defined(M8_WEB_EXACT_FP_FAST) || \
    !defined(M8_WEB_FP_REGISTER_ABI) || !defined(M8_WEB_FP_REGISTER_SPECIALIZE)
#error "FM opcode placement requires the exact specialized Web FP profile"
#endif
#include <fp_specialized_wasm.h>
#define m8_fms_op(C,OP,A,B,D) m8_fp_eval32((C),(OP),(A),(B),(D))
#endif
/* Exact operator12174..121ee,1232c..123f4, without pointer restrict.
 * The parent has already written its R5 byte before entering this interval. */
M8_FMS_INLINE void m8_fms_operator(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
#else
        uint8_t *stack,uint32_t matrix_address) {
#endif
    uint32_t s0,s7,s8,s9,s10,s11,s12,s13,s14=0x3f000000u;
    uint32_t step=m8_le_read(ports,4);
    uint32_t s15=m8_fms_op(cpu,M8_FP_ADD,m8_le_read(object+4,4),step,0);
    m8_fm_store_bits(stack,matrix_address);
    m8_fm_store_bits(stack+4,cpu->r[5]);m8_fm_store_bits(stack+8,cpu->r[6]);
    s13=m8_fms_op(cpu,M8_FP_ADD,s15,s14,0);
    m8_fm_store_bits(object+4,s15);
    s13=m8_fms_to_int(cpu,s13,0);
    s12=m8_fms_op(cpu,M8_FP_ADD,m8_le_read(object+8,4),step,0);
    s13=m8_fms_from_int(cpu,s13,0);
    m8_fm_store_bits(object+8,s12);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s13,0);
    m8_fm_store_bits(object+4,s15);
    s13=m8_le_read(pm,4);
    s15=m8_fms_op(cpu,M8_FP_ADD,s15,s13,0);
    m8_fp_cmp32(cpu,s15,0,1);
    s14=m8_fms_op(cpu,(cpu->fpscr&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    s14=m8_fms_to_int(cpu,s14,0);
    s14=m8_fms_from_int(cpu,s14,0);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s14,0);
    s14=m8_le_read(ports+8,4);m8_fm_store_bits(ports+8,0);
    s11=0x3f000000u;s7=m8_le_read(constants+8,4);
    s13=m8_le_read(ports+20,4);s12=m8_le_read(constants+12,4);
    s14=m8_fms_op(cpu,M8_FP_MUL,s14,s13,0);
    s9=m8_le_read(constants+4,4);s8=m8_le_read(constants+16,4);
    s10=m8_le_read(constants+24,4);
    s15=m8_fms_op(cpu,M8_FP_ADD,s14,s15,0);
    m8_fm_store_bits(ports+20,s14);s14=0x3f000000u;
    m8_fp_cmp32(cpu,s15,0,1);
    uint32_t compare=cpu->fpscr&0xf0000000u;
    s14=m8_fms_op(cpu,(compare&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    s14=m8_fms_to_int(cpu,s14,0);
    s14=m8_fms_from_int(cpu,s14,0);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s14,0);
    s14=m8_le_read(constants,4);s13=s15&0x7fffffffu;
    s15=m8_fms_op(cpu,M8_FP_MUL,s15,s14,0);s14=m8_le_read(constants+20,4);
    s11=m8_fms_op(cpu,M8_FP_SUB,s13,s11,0);
    s7=m8_fms_op(cpu,M8_FP_ADD,s13,s7,0);s12=m8_fms_op(cpu,M8_FP_SUB,s13,s12,0);
    s15=m8_fms_op(cpu,M8_FP_MUL,s11,s15,0);
    s9=m8_fms_op(cpu,M8_FP_FMA,s7,s13,s9);s8=m8_fms_op(cpu,M8_FP_SUB,s13,s8,0);
    s14=m8_fms_op(cpu,M8_FP_ADD,s13,s14,0);s11=s10;
    s15=m8_fms_op(cpu,M8_FP_MUL,s12,s15,0);
    s11=m8_fms_op(cpu,M8_FP_FMA,s8,s13,s11);
    s15=m8_fms_op(cpu,M8_FP_MUL,s14,s15,0);
    s15=m8_fms_op(cpu,M8_FP_MUL,s15,s9,0);
    s11=m8_fms_op(cpu,M8_FP_MUL,s15,s11,0);m8_fm_store_bits(ports+8,s11);
    s0=m8_fms_op(cpu,M8_FP_MUL,s11,m8_le_read(ports+12,4),0);
    m8_fm_store_bits(ports+8,s0);s15=m8_le_read(ports+16,4);
    s0=m8_fms_op(cpu,M8_FP_MUL,s0,s15,0);m8_fm_store_bits(ports+8,s0);
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    s15=m8_le_read(bias,4);s0=m8_fms_op(cpu,M8_FP_ADD,s0,s15,0);
#else
    s15=m8_le_read(object+16,4);s0=m8_fms_op(cpu,M8_FP_ADD,s0,s15,0);
#endif
    m8_fm_store_bits(ports+8,s0);object[0x38]=1;
    cpu->s[0]=s0;cpu->s[7]=s7;cpu->s[8]=s8;cpu->s[9]=s9;cpu->s[10]=s10;
    cpu->s[11]=s11;cpu->s[12]=s12;cpu->s[13]=s13;cpu->s[14]=s14;cpu->s[15]=s15;
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(compare&0x30000000u);
}
#if defined(M8_WEB_FM_OPERATOR_LOCAL)
#include "fm_operator_local.h"
#define M8_FMS_OPERATOR m8_fms_operator_local
#else
#define M8_FMS_OPERATOR m8_fms_operator
#endif
#include "fm_operator_mode8.h"
#if defined(M8_AOT_FM_FAST_MODES)
#include "fm_operator_mode7.h"
#endif
M8_FMS_INLINE void m8_fms_render(m8_cpu *cpu,m8_fms_context *c) {
    uint32_t sum=cpu->s[4];
    for(int i=3;i>=0;i--) {
        uint8_t *op=c->object+0x54+0x50*i,*port=c->ports+64*i;
        uint32_t a=m8_le_read(c->mod[i],4),b=m8_le_read(c->mod[i+4],4);
        uint32_t gain=m8_fms_op(cpu,M8_FP_MUL,a,b,0);
        a=m8_le_read(c->mod[i+8],4);b=m8_le_read(c->mod[i+12],4);
        uint32_t matrix=m8_le_read(c->matrix+4*i,4);
        op[0x38]=(uint8_t)cpu->r[5];
        uint32_t feedback_mod=m8_fms_op(cpu,M8_FP_MUL,a,b,0);
        m8_fm_store_bits(port+16,gain);
        uint32_t depth=m8_le_read(port+28,4);
        depth=m8_fms_op(cpu,M8_FP_ADD,depth,depth,0);
        uint32_t feedback=m8_fms_op(cpu,M8_FP_FMA,feedback_mod,matrix,depth);
        m8_fm_store_bits(port+20,feedback);
        /* The actual PM load is inside the operator, after its phase stores. */
#if defined(M8_AOT_FM_MODE8)
#if defined(M8_AOT_FM_FAST_MODES)
        if(op[0x3a]==7)
            m8_fms_operator_mode7(cpu,op,port,c->pm[i],c->stack,
                c->matrix_address,c->bias[i]);
        else
#endif
        if(op[0x3a]==8)
            m8_fms_operator_mode8(cpu,op,port,c->pm[i],c->constants,
                c->stack,c->matrix_address,
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
                c->bias[i]);
#else
                op+16);
#endif
        else
#endif
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
        M8_FMS_OPERATOR(cpu,op,port,c->pm[i],c->constants,c->stack,c->matrix_address,c->bias[i]);
#else
        M8_FMS_OPERATOR(cpu,op,port,c->pm[i],c->constants,c->stack,c->matrix_address);
#endif
        if(i==3) sum=op[0x39] ? cpu->s[17] : cpu->s[0];
        else if(!op[0x39]) sum=m8_fms_op(cpu,M8_FP_ADD,sum,cpu->s[0],0);
    }
    cpu->s[4]=sum;cpu->r[0]=c->address+0x54u;cpu->r[1]=c->object[0x8d];
#if defined(M8_AOT_FM_DYNAMIC_BIAS)
    cpu->r[2]=c->bias_address[0];cpu->r[3]=1;cpu->r[4]=c->matrix_address;
#else
    cpu->r[2]=c->address+0x64u;cpu->r[3]=1;cpu->r[4]=c->matrix_address;
#endif
    cpu->r[14]=0x142f9u;cpu->r[15]=0x14302u;
}
#undef M8_FMS_OPERATOR
M8_FMS_INLINE int m8_fms_phase(m8_cpu *cpu,const uint8_t *object) {
    static const uint32_t weights[]={0,0x3e000000u,0x3e800000u,0x3ec00000u,
        0x3f000000u,0x3f200000u,0x3f400000u,0x3f600000u};
    static const uint32_t inverse[]={0x3f800000u,0x3f600000u,0x3f400000u,0x3f200000u,
        0x3f000000u,0x3ec00000u,0x3e800000u,0x3e000000u};
    unsigned phase=object[0x194];
    return phase<=7 && cpu->s[15]==weights[phase] && cpu->s[14]==inverse[phase] &&
        cpu->r[2]==(phase<7?phase+1u:7u) && cpu->r[1]==(phase<7?phase+1u:0u);
}
M8_FMS_INLINE void m8_fms_quantum(m8_cpu *cpu,m8_fms_context *c) {
    uint8_t *p=c->control;uint32_t w=cpu->s[15],v=cpu->s[14];
    c->object[0x194]=(uint8_t)cpu->r[1];
    uint32_t pan=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x28,4),0);
    uint32_t volume=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+4,4),0);
    uint32_t cutoff=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x1c,4),0);
    pan=m8_fms_op(cpu,M8_FP_FMA,v,m8_le_read(p+0x78,4),pan);
    uint32_t resonance=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x20,4),0);
    uint32_t extra=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x24,4),0);
    uint32_t mixc=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0xc,4),0);
    uint32_t mix10=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x10,4),0);
    uint32_t mix14=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x14,4),0);
    uint32_t feedback=m8_fms_op(cpu,M8_FP_MUL,w,m8_le_read(p+0x18,4),0);
    volume=m8_fms_op(cpu,M8_FP_FMA,v,m8_le_read(p+0x54,4),volume);
    resonance=m8_fms_op(cpu,M8_FP_FMA,v,m8_le_read(p+0x70,4),resonance);
    uint32_t old_extra=m8_le_read(p+0x74,4),old_mixc=m8_le_read(p+0x5c,4);
    uint32_t old_mix10=m8_le_read(p+0x60,4),old_mix14=m8_le_read(p+0x64,4);
    extra=m8_fms_op(cpu,M8_FP_FMA,v,old_extra,extra);
    uint32_t old_feedback=m8_le_read(p+0x68,4),old_cutoff=m8_le_read(p+0x6c,4);
    mixc=m8_fms_op(cpu,M8_FP_FMA,v,old_mixc,mixc);
    mix10=m8_fms_op(cpu,M8_FP_FMA,v,old_mix10,mix10);
    mix14=m8_fms_op(cpu,M8_FP_FMA,v,old_mix14,mix14);
    m8_fm_store_bits(p+0x50,pan);cutoff=m8_fms_op(cpu,M8_FP_FMA,v,old_cutoff,cutoff);
    m8_fm_store_bits(p+0x2c,volume);feedback=m8_fms_op(cpu,M8_FP_FMA,v,old_feedback,feedback);
    m8_fm_store_bits(p+0x48,resonance);m8_fp_cmp32(cpu,pan,0x3f000000u,1);
    m8_fm_store_bits(p+0x4c,extra);m8_fm_store_bits(p+0x34,mixc);m8_fm_store_bits(p+0x38,mix10);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
    m8_fm_store_bits(p+0x3c,mix14);m8_fm_store_bits(p+0x44,cutoff);m8_fm_store_bits(p+0x40,feedback);
    uint32_t left,right;
    if(((cpu->fpscr>>31)^(cpu->fpscr>>28))&1u) {
        left=0x3f800000u;right=m8_fms_op(cpu,M8_FP_ADD,pan,pan,0);}
    else {right=0x3f800000u;left=m8_fms_op(cpu,M8_FP_SUB,0x3f000000u,pan,0);
        left=m8_fms_op(cpu,M8_FP_FMA,left,0x40000000u,0x3f800000u);}
    cpu->r[7]=c->control_address;cpu->s[1]=old_extra;cpu->s[2]=old_mixc;
    cpu->s[3]=old_mix10;cpu->s[4]=mix14;cpu->s[5]=old_feedback;cpu->s[6]=0x3f000000u;
    cpu->s[16]=right;cpu->s[23]=left;cpu->s[24]=old_cutoff;
#if defined(M8_AOT_FM_MODE8)
    /* Mode 8 preserves these registers; mode 0 overwrites them. */
    cpu->s[7]=volume;cpu->s[8]=cutoff;
#if defined(M8_AOT_FM_FAST_MODES)
    cpu->s[9]=resonance;cpu->s[10]=extra;
#endif
#endif
    m8_fms_render(cpu,c);
}
#include "fm_quantum_local.h"
#if defined(M8_WEB_FM_QUANTUM_LOCAL)
#define m8_fms_quantum m8_fms_quantum_local
#endif
M8_FMS_NOINLINE int m8_fms_renderer_kernel(m8_cpu *cpu,m8_mem *mem) {
    m8_fms_context c;
    if(!m8_fms_prepare(cpu,mem,0x1417cu,&c)) return 0;
    m8_fms_render(cpu,&c);m8_fp_sync(cpu);return 1;
}
M8_FMS_NOINLINE int m8_fms_quantum_kernel(m8_cpu *cpu,m8_mem *mem) {
    m8_fms_context c;
    if(!m8_fms_prepare(cpu,mem,0x1409cu,&c) || !m8_fms_phase(cpu,c.object)) return 0;
    m8_fms_quantum(cpu,&c);m8_fp_sync(cpu);return 1;
}
#endif
#endif
