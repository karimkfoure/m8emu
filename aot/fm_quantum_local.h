/* Default-off local interpolation for the admitted 6.5.2C FM quantum.
 * Include after fm_structural.h defines m8_fms_quantum and its context.
 * The original renderer and arbitrary-input interpolation remain intact. */
#ifndef M8_AOT_FM_QUANTUM_LOCAL_H
#define M8_AOT_FM_QUANTUM_LOCAL_H
#include "fm_structural.h"
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_WEB_FM_QUANTUM_LOCAL)
#if defined(__wasm__) || defined(M8_WEB_FP_TEST)
#if !defined(M8_WEB_NATIVE_FP) || !defined(M8_WEB_EXACT_FP_FAST)
#error "Local quantum interpolation requires the exact Web FP profile"
#endif
#elif defined(__x86_64__) || defined(__aarch64__)
#define M8_FMQL_HARDWARE 1
#else
#error "Local quantum interpolation needs a qualified FP environment"
#endif
#ifdef __FAST_MATH__
#error "Local quantum interpolation requires strict IEEE arithmetic"
#endif
#define M8_FMQL_INLINE static inline __attribute__((always_inline))
#ifdef M8_FMQL_PROBE
typedef struct m8_fmql_failure {
    unsigned predicate,index;uint32_t a,b,d;
} m8_fmql_failure;
static m8_fmql_failure m8_fmql_first_false;
#define M8_FMQL_DECLINE(P,I,A,B,D) do { \
    m8_fmql_first_false=(m8_fmql_failure){P,I,A,B,D};return 0; \
} while(0)
#else
#define M8_FMQL_DECLINE(P,I,A,B,D) return 0
#endif

/* Each nonzero source lies in [2^-16,2^16]. Phase weights are multiples of
 * 1/8 in [0,1]. Products, cancellation and pan arithmetic then remain
 * finite and either zero or normal. IXC is already sticky. The binary32
 * product in each FMA is exact in binary64. Reject a binary32 midpoint
 * after the binary64 addition, as the existing exact Web helper does.
 * No FMA library call or FP status helper is needed in this domain. */
M8_FMQL_INLINE int m8_fmql_fma(float a,float b,float d,float *result) {
    double sum=(double)a*(double)b+(double)d;
    uint64_t bits;memcpy(&bits,&sum,8);
    if((bits&UINT64_C(0x1fffffff))==UINT64_C(0x10000000))return 0;
    *result=(float)sum;return 1;
}
#if defined(M8_FMQL_HARDWARE)
#define M8_FMQL_BODY static __attribute__((noinline))
#define M8_FMQL_NAME m8_fmql_native_prefix
#else
#define M8_FMQL_BODY M8_FMQL_INLINE
#define M8_FMQL_NAME m8_fmql_try_prefix
#endif

/* The caller has qualified the structural context and disjoint writable
 * spans. All control inputs precede interpolation stores in the firmware.
 * The early phase-byte store cannot alias control. PM and modulation can
 * alias these destinations: keep their reads in the unchanged renderer,
 * after the original ordered stores below. Rejection has no guest effect. */
M8_FMQL_BODY int M8_FMQL_NAME(m8_cpu *cpu,m8_fms_context *c) {
    if((cpu->fpscr&0x03c00010u)!=0x03000010u)
        M8_FMQL_DECLINE(1,0,cpu->fpscr,0,0);
    if(!m8_fms_phase(cpu,c->object))
        M8_FMQL_DECLINE(2,c->object[0x194],cpu->s[15],cpu->s[14],cpu->r[1]);
    static const unsigned offsets[]={0x28,4,0x1c,0x20,0x24,0xc,0x10,0x14,0x18,
        0x78,0x54,0x6c,0x70,0x74,0x5c,0x60,0x64,0x68};
    uint32_t raw[18];float values[18];
    unsigned reset_private=!(cpu->s[15]&0x7fffffffu);
    for(unsigned i=0;i<18;i++) {
        uint32_t bits=m8_le_read(c->control+offsets[i],4),magnitude=bits&0x7fffffffu;
        if(magnitude && magnitude-0x37800000u>0x10000000u)
            M8_FMQL_DECLINE(3,i,bits,offsets[i],0);
        raw[i]=bits;values[i]=m8_fp_float(bits);
        if(i<9 && !magnitude)reset_private=1;
    }
    float w=m8_fp_float(cpu->s[15]),v=m8_fp_float(cpu->s[14]);
    float pan=w*values[0],volume=w*values[1],cutoff=w*values[2];
    float resonance=w*values[3],extra=w*values[4],mixc=w*values[5];
    float mix10=w*values[6],mix14=w*values[7],feedback=w*values[8];
#define M8_FMQL_FMA(I,DEST,A,B,D) do { \
    float qa=(A),qb=(B),qd=(D); \
    if(!m8_fmql_fma(qa,qb,qd,&(DEST))) \
        M8_FMQL_DECLINE(4,I,m8_fp_bits32(qa),m8_fp_bits32(qb),m8_fp_bits32(qd)); \
} while(0)
    M8_FMQL_FMA(0,pan,v,values[9],pan);
    M8_FMQL_FMA(1,volume,v,values[10],volume);
    M8_FMQL_FMA(2,resonance,v,values[12],resonance);
    M8_FMQL_FMA(3,extra,v,values[13],extra);
    M8_FMQL_FMA(4,mixc,v,values[14],mixc);
    M8_FMQL_FMA(5,mix10,v,values[15],mix10);
    M8_FMQL_FMA(6,mix14,v,values[16],mix14);
    M8_FMQL_FMA(7,cutoff,v,values[11],cutoff);
    M8_FMQL_FMA(8,feedback,v,values[17],feedback);
    uint32_t compare=pan<0.5f?0x80000000u:pan==0.5f?0x60000000u:0x20000000u;
    float left,right;
    if(compare&0x80000000u) {
        left=1.0f;right=pan+pan;
        if(!(m8_fp_bits32(pan)&0x7fffffffu))reset_private=1;
    } else {
        right=1.0f;left=0.5f-pan;
        M8_FMQL_FMA(9,left,left,2.0f,1.0f);
    }
#undef M8_FMQL_FMA
    c->object[0x194]=(uint8_t)cpu->r[1];
    m8_fm_store_bits(c->control+0x50,m8_fp_bits32(pan));
    m8_fm_store_bits(c->control+0x2c,m8_fp_bits32(volume));
    m8_fm_store_bits(c->control+0x48,m8_fp_bits32(resonance));
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    m8_fm_store_bits(c->control+0x4c,m8_fp_bits32(extra));
    m8_fm_store_bits(c->control+0x34,m8_fp_bits32(mixc));
    m8_fm_store_bits(c->control+0x38,m8_fp_bits32(mix10));
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|compare;
    m8_fm_store_bits(c->control+0x3c,m8_fp_bits32(mix14));
    m8_fm_store_bits(c->control+0x44,m8_fp_bits32(cutoff));
    m8_fm_store_bits(c->control+0x40,m8_fp_bits32(feedback));
    cpu->r[7]=c->control_address;cpu->s[1]=raw[13];cpu->s[2]=raw[14];
    cpu->s[3]=raw[15];cpu->s[4]=m8_fp_bits32(mix14);cpu->s[5]=raw[17];cpu->s[6]=0x3f000000u;
    cpu->s[16]=m8_fp_bits32(right);cpu->s[23]=m8_fp_bits32(left);cpu->s[24]=raw[11];
#if defined(M8_AOT_FM_MODE8)
    /* Mode 8 preserves the original interpolation S7/S8 values. Mode 0
     * overwrites them in its renderer. Publish before entering either path. */
    cpu->s[7]=m8_fp_bits32(volume);cpu->s[8]=m8_fp_bits32(cutoff);
#endif
#if !defined(M8_FMQL_HARDWARE)
    /* Only zero-input MUL/ADD declines the original bounded Web helpers
     * within this domain. Such exact software operations reset the private
     * context. All admitted FMAs and nonzero MUL/ADD/SUB preserve it. */
    if(reset_private) {
        softfloat_roundingMode=softfloat_round_near_even;
        softfloat_detectTininess=softfloat_tininess_beforeRounding;
        softfloat_exceptionFlags=0;
    }
#else
    (void)reset_private;
#endif
    return 1;
}
#undef M8_FMQL_BODY
#undef M8_FMQL_NAME
#if defined(M8_FMQL_HARDWARE)
M8_FMQL_INLINE int m8_fmql_try_prefix(m8_cpu *cpu,m8_fms_context *c) {
#if defined(M8_NATIVE_FP_NO_STICKY)
    /* Host mode is fixed per thread; no host test needed. */
#elif defined(__x86_64__)
    unsigned saved=_mm_getcsr();
    if((saved&0x0000ffc0u)!=0x00009fc0u || !(saved&0x20u))
        M8_FMQL_DECLINE(5,0,saved,0,0);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
    uint64_t mode,saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(mode));
    __asm__ volatile("mrs %0, fpsr" : "=r"(saved));
    if((mode&UINT64_C(0x03c09f00))!=UINT64_C(0x03000000) || !(saved&16u))
        M8_FMQL_DECLINE(5,0,(uint32_t)mode,(uint32_t)saved,0);
#endif
    int result=m8_fmql_native_prefix(cpu,c);
    if(!result) {
#if defined(__x86_64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        _mm_setcsr(saved);
#elif defined(__aarch64__) && !defined(M8_NATIVE_FP_NO_STICKY)
        __asm__ volatile("msr fpsr, %0" :: "r"(saved) : "memory");
#endif
    }
    return result;
}
#endif
M8_FMQL_INLINE void m8_fms_quantum_local(m8_cpu *cpu,m8_fms_context *c) {
    if(!m8_fmql_try_prefix(cpu,c)) {m8_fms_quantum(cpu,c);return;}
    m8_fms_render(cpu,c);
}
#undef M8_FMQL_DECLINE
#undef M8_FMQL_INLINE
#undef M8_FMQL_HARDWARE
#endif
#endif
