/* Default-off checked whole-batch transaction. The private probe supplies
 * fm_batch_local_admission.h from a pinned structural qualification prefix.
 * No production call site includes this discovery header. */
#ifndef M8_AOT_FM_BATCH_LOCAL_H
#define M8_AOT_FM_BATCH_LOCAL_H
#if defined(M8_AOT_FM_BATCH_LOCAL)
#if !defined(M8_AOT_FM_STRUCTURAL) || !defined(M8_AOT_FM_DYNAMIC_BIAS) || \
    !defined(M8_AOT_FM_SVF_LOCAL) || !defined(M8_AOT_FM_CALLBACK_LOCAL)
#error "Whole local batch requires qualified local FM math and dynamic bias"
#endif
#if defined(__FAST_MATH__)
#error "Whole local batch requires strict FP arithmetic"
#endif
#include <softfloat.h>
#define M8_FMBL_INLINE static inline __attribute__((always_inline))
typedef struct m8_fmbl_prepared {
    m8_fms_context c;
    m8_fmcb2284_context callback_context;
    uint8_t *literal,*left,*right,*ring;
    uint32_t left_address,right_address,end,ring_address;
    uint32_t callback_address,callback_target,count;
} m8_fmbl_prepared;
#include "fm_batch_local_admission.h"

typedef struct m8_fmbl_span {
    uint8_t *original,*shadow;
    unsigned size;
} m8_fmbl_span;
typedef struct m8_fmbl_transaction {
    m8_cpu cpu;
    m8_fms_context c;
    uint8_t object[0x254],ports[224],control[128],stack[20];
    uint8_t ring[640],left[32],right[32];
    m8_fmbl_span spans[7];
    struct {unsigned offset;uint32_t value;} ring_writes[8];
    unsigned samples;
} m8_fmbl_transaction;
#if defined(M8_FMBL_TEST)
typedef struct m8_fmbl_failure {unsigned predicate,index;uint32_t a,b;} m8_fmbl_failure;
static m8_fmbl_failure m8_fmbl_first_false;
static unsigned m8_fmbl_admitted,m8_fmbl_declined,m8_fmbl_shadow_samples;
#define M8_FMBL_DECLINE(P,I,A,B) do { \
    m8_fmbl_first_false=(m8_fmbl_failure){P,I,A,B};return 0; \
} while(0)
#else
#define M8_FMBL_DECLINE(P,I,A,B) return 0
#endif

/* A complete word sees the corresponding private bytes when it aliases a
 * mutable span. Crossing a span edge declines before guest effects. Words
 * outside every mutable span remain original plain RAM reads. */
M8_FMBL_INLINE uint8_t *m8_fmbl_resolve(m8_fmbl_transaction *t,
                                      const uint8_t *word) {
    uintptr_t address=(uintptr_t)word;
    for(unsigned i=0;i<7;i++) {
        uintptr_t base=(uintptr_t)t->spans[i].original;
        unsigned size=t->spans[i].size;
        if(address>=base && address-base<=size && size-(address-base)>=4)
            return t->spans[i].shadow+(address-base);
        if(m8_fm_overlap(word,4,t->spans[i].original,size))return NULL;
    }
    return (uint8_t *)word;
}
M8_FMBL_INLINE int m8_fmbl_snapshot(m8_fmbl_transaction *t,
                                   const m8_fmbl_prepared *p,
                                   const m8_cpu *cpu,m8_mem *mem) {
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++)
        if(m8_fm_overlap(regions[i]->host,regions[i]->size,
                         (const uint8_t *)t,sizeof(*t)))
            M8_FMBL_DECLINE(2,i,regions[i]->base,regions[i]->size);
    t->cpu=*cpu;t->c=p->c;t->samples=0;
    t->spans[0]=(m8_fmbl_span){p->c.object,t->object,sizeof t->object};
    t->spans[1]=(m8_fmbl_span){p->c.ports,t->ports,sizeof t->ports};
    t->spans[2]=(m8_fmbl_span){p->c.control,t->control,sizeof t->control};
    t->spans[3]=(m8_fmbl_span){p->c.stack,t->stack,sizeof t->stack};
    t->spans[4]=(m8_fmbl_span){p->ring,t->ring,sizeof t->ring};
    t->spans[5]=(m8_fmbl_span){p->left,t->left,p->count*4};
    t->spans[6]=(m8_fmbl_span){p->right,t->right,p->count*4};
    if(p->callback_target==0x2285u)
        for(unsigned i=0;i<7;i++)
            if(m8_fm_overlap(p->callback_context.table,2052,
                             t->spans[i].original,t->spans[i].size))
                M8_FMBL_DECLINE(3,i,p->callback_target,t->spans[i].size);
    /* Resolve every address before copying or doing arithmetic. The guest
     * pointer fields are stable under the existing batch qualification. */
    for(unsigned i=0;i<16;i++) {
        t->c.mod[i]=m8_fmbl_resolve(t,p->c.mod[i]);
        if(!t->c.mod[i])M8_FMBL_DECLINE(4,i,0,0);
    }
    for(unsigned i=0;i<4;i++) {
        t->c.pm[i]=m8_fmbl_resolve(t,p->c.pm[i]);
        t->c.bias[i]=m8_fmbl_resolve(t,p->c.bias[i]);
        if(!t->c.pm[i] || !t->c.bias[i])M8_FMBL_DECLINE(5,i,0,0);
    }
    for(unsigned i=0;i<7;i++)
        memcpy(t->spans[i].shadow,t->spans[i].original,t->spans[i].size);
    t->c.object=t->object;t->c.ports=t->ports;
    t->c.control=t->control;t->c.stack=t->stack;
    return 1;
}

#define M8_FMBL_OP(DEST,OP,A,B,D) do { \
    float qa=(A),qb=(B),qd=(D); \
    if(!m8_fmol_op(OP,qa,qb,qd,&(DEST))) \
        M8_FMBL_DECLINE(10,OP,m8_fp_bits32(qa),m8_fp_bits32(qb)); \
} while(0)
M8_FMBL_INLINE int m8_fmbl_render(m8_cpu *cpu,m8_fms_context *c) {
    float sum=m8_fp_float(cpu->s[4]);
    for(int i=3;i>=0;i--) {
        uint8_t *op=c->object+0x54+0x50*i,*port=c->ports+64*i;
        float gain,feedback_mod,depth,feedback;
        M8_FMBL_OP(gain,M8_FP_MUL,m8_fm_load(c->mod[i]),m8_fm_load(c->mod[i+4]),0);
        op[0x38]=(uint8_t)cpu->r[5];
        M8_FMBL_OP(feedback_mod,M8_FP_MUL,m8_fm_load(c->mod[i+8]),m8_fm_load(c->mod[i+12]),0);
        m8_fm_store(port+16,gain);
        M8_FMBL_OP(depth,M8_FP_ADD,m8_fm_load(port+28),m8_fm_load(port+28),0);
        M8_FMBL_OP(feedback,M8_FP_FMA,feedback_mod,m8_fm_load(c->matrix+4*i),depth);
        m8_fm_store(port+20,feedback);
        int admitted;
        if(op[0x3a]==8)
            admitted=m8_fm8l_math(cpu,op,port,c->pm[i],c->constants,
                                  c->stack,c->matrix_address,c->bias[i]);
        else {
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
            admitted=m8_fms_operator_try_local(cpu,op,port,c->pm[i],c->constants,
                                               c->stack,c->matrix_address,c->bias[i]);
#else
            admitted=m8_fmol_native_math(cpu,op,port,c->pm[i],c->constants,
                                        c->stack,c->matrix_address,c->bias[i]);
#endif
        }
        if(!admitted)M8_FMBL_DECLINE(11,(unsigned)i,cpu->fpscr,op[0x3a]);
        if(i==3)sum=op[0x39]?m8_fp_float(cpu->s[17]):m8_fp_float(cpu->s[0]);
        else if(!op[0x39])M8_FMBL_OP(sum,M8_FP_ADD,sum,m8_fp_float(cpu->s[0]),0);
    }
    cpu->s[4]=m8_fp_bits32(sum);cpu->r[0]=c->address+0x54u;
    cpu->r[1]=c->object[0x8d];cpu->r[2]=c->bias_address[0];
    cpu->r[3]=1;cpu->r[4]=c->matrix_address;cpu->r[14]=0x142f9u;cpu->r[15]=0x14302u;
    return 1;
}
M8_FMBL_INLINE int m8_fmbl_callback(m8_cpu *cpu,const m8_fmbl_prepared *p,
                                   const uint8_t *control,uint32_t link) {
    cpu->r[0]=p->callback_address;cpu->r[2]=p->callback_target;cpu->r[14]=link;
    cpu->s[1]=m8_le_read(control+0x4c,4);
    if(p->callback_target==0x2285u)return m8_fmcl_math2284(cpu,&p->callback_context);
    if(p->callback_target==0x1fbdu)return m8_fmcl_math1fbc(cpu);
    float sample;
    cpu->s[14]=0xbf800000u;cpu->s[15]=0x3f800000u;
    M8_FMBL_OP(sample,M8_FP_MUL,m8_fp_float(cpu->s[0]),m8_fp_float(cpu->s[1]),0);
    M8_FMBL_OP(sample,M8_FP_MAXNM,sample,-1.0f,0);
    M8_FMBL_OP(sample,M8_FP_MINNM,sample,1.0f,0);
    cpu->s[0]=m8_fp_bits32(sample);cpu->r[15]=link&~1u;return 1;
}

/* This is the native speculation boundary. The outer wrapper checks host
 * traps/mode/status before this call. Private inline math APIs stay inside
 * it, so CPU S-register publications can disappear during optimization. */
static __attribute__((noinline)) int m8_fmbl_compute(m8_fmbl_transaction *t,
                                                   const m8_fmbl_prepared *p) {
    m8_cpu state=t->cpu,*cpu=&state;m8_fms_context *c=&t->c;
    uint32_t right_address=p->right_address;
    static const uint32_t weights[]={0,0x3e000000u,0x3e800000u,0x3ec00000u,
        0x3f000000u,0x3f200000u,0x3f400000u,0x3f600000u};
    static const uint32_t inverse[]={0x3f800000u,0x3f600000u,0x3f400000u,0x3f200000u,
        0x3f000000u,0x3ec00000u,0x3e800000u,0x3e000000u};
    for(unsigned iteration=0;iteration<p->count;iteration++) {
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
        int prefix=m8_fmql_try_prefix(cpu,c);
#else
        int prefix=m8_fmql_native_prefix(cpu,c);
#endif
        if(!prefix)M8_FMBL_DECLINE(12,iteration,cpu->fpscr,c->object[0x194]);
        if(!m8_fmbl_render(cpu,c))return 0;
        float scale,sample;
        M8_FMBL_OP(scale,M8_FP_MUL,m8_fm_load(c->object+0x19c),m8_fp_float(cpu->s[20]),0);
        M8_FMBL_OP(sample,M8_FP_MUL,scale,m8_fp_float(cpu->s[4]),0);
        cpu->s[15]=m8_fp_bits32(scale);cpu->s[0]=m8_fp_bits32(sample);
        unsigned family=c->object[0x40],placement=c->object[0x41];
        if(placement<=3 && !m8_fmbl_callback(cpu,p,c->control,0x14325u))
            M8_FMBL_DECLINE(13,iteration,p->callback_target,placement);
        cpu->r[7]=c->control_address;cpu->r[0]=placement;cpu->r[1]=family;cpu->r[4]=family-1u;
        if(!m8_fmsvfl_math(cpu,c->object,c->control,p->literal,family))
            M8_FMBL_DECLINE(14,iteration,family,placement);
        if(placement>3) {
            if(!m8_fmbl_callback(cpu,p,c->control,0x14351u))
                M8_FMBL_DECLINE(15,iteration,p->callback_target,placement);
            cpu->r[7]=c->control_address;
        }
        float cursor=m8_fm_load(c->object+0x44),limit=m8_fm_load(p->literal+24);
        uint32_t compare=m8_fm8l_compare(cursor,limit);
        cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
        cpu->xpsr=(cpu->xpsr&0x0fffffffu)|compare;
        float scaled,integer;
        M8_FMBL_OP(scaled,M8_FP_MUL,m8_fp_float(cpu->s[0]),m8_fm_load(p->literal+28),0);
        if(!m8_fmol_integer(scaled,&integer))
            M8_FMBL_DECLINE(16,iteration,m8_fp_bits32(scaled),0);
        uint32_t ring_value=(uint32_t)(int32_t)integer,ring_offset;
        if(cursor<limit) {
            uint32_t index=(uint32_t)cursor;
            cpu->s[14]=index;cpu->r[3]=index;ring_offset=(index&65535u)*2u;
        } else {cursor=m8_fp_float(cpu->s[17]);ring_offset=0;}
        float next_cursor;
        M8_FMBL_OP(next_cursor,M8_FP_ADD,m8_fm_load(c->object+0x48),cursor,0);
        cpu->r[0]=p->ring_address;cpu->r[1]=ring_offset;
        if(p->callback_target==0x1fa5u)cpu->r[2]=0x1fa5u;
        cpu->r[4]=family-1u;
        m8_filter_write16(t->ring+ring_offset,ring_value);
        t->ring_writes[iteration].offset=ring_offset;t->ring_writes[iteration].value=ring_value;
        m8_fm_store(c->object+0x44,next_cursor);
        float volume=m8_fm_load(c->control+0x2c),out_left,out_right;
        M8_FMBL_OP(sample,M8_FP_MUL,m8_fp_float(cpu->s[0]),volume,0);
        M8_FMBL_OP(out_left,M8_FP_MUL,sample,m8_fp_float(cpu->s[23]),0);
        M8_FMBL_OP(out_right,M8_FP_MUL,sample,m8_fp_float(cpu->s[16]),0);
        m8_fm_store(t->left+iteration*4,out_left);m8_fm_store(t->right+iteration*4,out_right);
        cpu->r[6]+=4;right_address+=4;m8_fm_store_bits(c->stack+12,right_address);
        cpu->r[3]=p->end;cpu->s[0]=m8_fp_bits32(out_right);cpu->s[13]=m8_fp_bits32(cursor);
        cpu->s[14]=m8_fp_bits32(next_cursor);cpu->s[15]=m8_fp_bits32(volume);
        cpu->s[23]=m8_fp_bits32(out_left);(void)m8_sub_flags(cpu,cpu->r[6],p->end,0);
        t->samples=iteration+1;
#if defined(M8_FMBL_TEST)
        m8_fmbl_shadow_samples++;
#endif
        if(cpu->r[6]==p->end){cpu->r[15]=0x145bcu;t->cpu=state;return 1;}
        cpu->r[2]=c->object[0x194];
        if(!cpu->r[2]){cpu->r[15]=0x143eau;t->cpu=state;return 1;}
        unsigned phase=cpu->r[2];cpu->s[15]=weights[phase];cpu->s[14]=inverse[phase];
        (void)m8_sub_flags(cpu,phase,6,0);
        if(phase<=6){cpu->r[2]=phase+1;cpu->r[1]=cpu->r[2];(void)m8_add_flags(cpu,phase,1,0);}
        else{cpu->r[1]=0;cpu->xpsr=(cpu->xpsr&0x3fffffffu)|0x40000000u;}
        cpu->r[15]=0x1409cu;
    }
    t->cpu=state;return 1;
}
#undef M8_FMBL_OP

/* All writable spans are plain disjoint RAM. No IRQ, service, slow memory
 * access or callback boundary occurs inside this admitted interval. Aliases
 * observe original ordering in scratch. Publish only original write fields;
 * metadata and untouched bytes are not copied back. Ring/output writes keep
 * iteration order. An active IT rollback journal uses the original path. */
M8_FMBL_INLINE void m8_fmbl_commit(const m8_fmbl_transaction *t,
                                  const m8_fmbl_prepared *p,m8_cpu *output) {
    static const unsigned controls[]={0x50,0x2c,0x48,0x4c,0x34,0x38,0x3c,0x44,0x40};
    p->c.object[0x194]=t->object[0x194];
    for(unsigned i=0;i<9;i++)memcpy(p->c.control+controls[i],t->control+controls[i],4);
    for(int i=3;i>=0;i--) {
        unsigned off=0x54+0x50*i,port=64*i;
        memcpy(p->c.ports+port+16,t->ports+port+16,4);
        memcpy(p->c.ports+port+20,t->ports+port+20,4);
        memcpy(p->c.object+off+4,t->object+off+4,8);
        memcpy(p->c.ports+port+8,t->ports+port+8,4);
        p->c.object[off+0x38]=t->object[off+0x38];
    }
    memcpy(p->c.stack,t->stack,12);
    memcpy(p->c.object+0x20,t->object+0x20,16);
    for(unsigned i=0;i<t->samples;i++) {
        m8_filter_write16(p->ring+t->ring_writes[i].offset,t->ring_writes[i].value);
        memcpy(p->left+4*i,t->left+4*i,4);memcpy(p->right+4*i,t->right+4*i,4);
    }
    memcpy(p->c.object+0x44,t->object+0x44,4);memcpy(p->c.stack+12,t->stack+12,4);
    *output=t->cpu;
}
static int m8_fm_batch_try_local(m8_cpu *output,m8_mem *mem) {
    m8_fmbl_prepared p;m8_fmbl_transaction t;
    if(m8_current_it || !m8_fmbl_prepare(output,mem,&p) ||
       (p.c.object[0x40]!=1 && p.c.object[0x40]!=3))
        M8_FMBL_DECLINE(1,0,output->r[15],output->fpscr);
    if(!m8_fmbl_snapshot(&t,&p,output,mem))return 0;
    uint_fast8_t rounding=softfloat_roundingMode,tininess=softfloat_detectTininess;
    uint_fast8_t flags=softfloat_exceptionFlags;
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
#if defined(M8_NATIVE_FP_NO_STICKY)
    /* Host mode is fixed per thread; no host test needed. */
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))
        M8_FMBL_DECLINE(6,0,saved,0);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    uint64_t mode,saved;__asm__ volatile("mrs %0, fpcr":"=r"(mode));
    __asm__ volatile("mrs %0, fpsr":"=r"(saved));
    if((mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))
        M8_FMBL_DECLINE(6,0,(uint32_t)mode,(uint32_t)saved);
#else
    M8_FMBL_DECLINE(6,0,0,0);
#endif
#endif
    int admitted=m8_fmbl_compute(&t,&p);
    if(!admitted) {
        softfloat_roundingMode=rounding;softfloat_detectTininess=tininess;
        softfloat_exceptionFlags=flags;
#if !defined(__wasm__) && !defined(M8_WEB_FP_TEST)
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        _mm_setcsr(saved);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        __asm__ volatile("msr fpsr, %0"::"r"(saved):"memory");
#endif
#endif
#if defined(M8_FMBL_TEST)
        m8_fmbl_declined++;
#endif
        return 0;
    }
    m8_fmbl_commit(&t,&p,output);
#if defined(M8_FMBL_TEST)
    m8_fmbl_admitted++;
#endif
    return 1;
}
#undef M8_FMBL_DECLINE
#undef M8_FMBL_INLINE
#endif
#endif
