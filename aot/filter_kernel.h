/* Arithmetic for the Headless 6.5.2C filter at 0x19830..0x198fe. The caller
 * configures native RN/FZ/DN once and qualifies all buffers before effects.
 * Preserve the original operation order and both halves of VFP D registers. */
#ifndef M8_AOT_FILTER_KERNEL_H
#define M8_AOT_FILTER_KERNEL_H
#include "memory_spans.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FILTER_UNLIKELY(x) __builtin_expect(!!(x),0)
#define M8_FILTER_COLD __attribute__((cold,noinline))
#else
#define M8_FILTER_UNLIKELY(x) (x)
#define M8_FILTER_COLD
#endif
static inline int m8_filter_disjoint(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn) {
    return b-a>=an && a-b>=bn;
}
/* Pass floating operands as floating values. Raw bit arguments are needed
 * only for rare NaN repair, not for every normal arithmetic operation. */
M8_MEM_INLINE float m8_filter_op(m8_cpu *cpu,int op,float a,float b,float d) {
#if !defined(__x86_64__) && !defined(__aarch64__)
    return m8_fp_float(m8_fp_eval32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
#else
#ifdef M8_FP_ARM64_HARDWARE
    (void)cpu;
    return m8_fp_arm64_s(op,a,b,d);
#else
    float result;
    switch(op) {
    case M8_FP_ADD: result=a+b;break;
    case M8_FP_SUB: result=a-b;break;
    case M8_FP_MUL: result=a*b;break;
    case M8_FP_DIV: result=a/b;break;
    default: result=fmaf(a,b,d);break;
    }
    if(M8_FILTER_UNLIKELY(result!=result))
        return m8_fp_float(m8_fp_native_nan32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
    return result;
#endif
#endif
}
M8_MEM_INLINE uint32_t m8_filter_read32(const uint8_t *p) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t value;memcpy(&value,p,4);return value;
#else
    return m8_le_read(p,4);
#endif
}
M8_MEM_INLINE uint32_t m8_filter_read16(const uint8_t *p) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t value;memcpy(&value,p,2);return value;
#else
    return m8_le_read(p,2);
#endif
}
M8_MEM_INLINE float m8_filter_read(const uint8_t *p) {return m8_fp_float(m8_filter_read32(p));}
M8_MEM_INLINE void m8_filter_write32(uint8_t *p,uint32_t bits) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(p,&bits,4);
#else
    m8_le_write(p,4,bits);
#endif
}
M8_MEM_INLINE void m8_filter_write16(uint8_t *p,uint32_t bits) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t value=(uint16_t)bits;memcpy(p,&value,2);
#else
    m8_le_write(p,2,bits);
#endif
}
M8_MEM_INLINE void m8_filter_write(uint8_t *p,float value) {m8_filter_write32(p,m8_fp_bits32(value));}
M8_MEM_INLINE uint32_t m8_filter_input32(m8_cpu *cpu,uint32_t bits) {
    if((bits&0x7fffffffu) && !(bits&0x7f800000u)) {cpu->fpscr|=128u;return bits&0x80000000u;}
    return bits;
}
M8_MEM_INLINE uint64_t m8_filter_widen(m8_cpu *cpu,float value) {
    uint32_t bits=m8_filter_input32(cpu,m8_fp_bits32(value));
    if((bits&0x7f800000u)==0x7f800000u && (bits&0x7fffffu)) {
        if(!(bits&0x400000u)) cpu->fpscr|=1u;
        return UINT64_C(0x7ff8000000000000);
    }
    return m8_fp_bits64((double)m8_fp_float(bits));
}
M8_MEM_INLINE float m8_filter_narrow(m8_cpu *cpu,uint64_t bits) {
    uint64_t exp=bits&UINT64_C(0x7ff0000000000000),frac=bits&UINT64_C(0xfffffffffffff);
    if(!exp && frac) {cpu->fpscr|=128u;bits&=UINT64_C(0x8000000000000000);}
    if(exp==UINT64_C(0x7ff0000000000000) && frac) {
        if(!(bits&UINT64_C(0x8000000000000))) cpu->fpscr|=1u;
        return m8_fp_float(0x7fc00000u);
    }
    return (float)m8_fp_double(bits);
}
M8_MEM_INLINE uint32_t m8_filter_to_int(m8_cpu *cpu,float value,int uns) {
    uint32_t bits=m8_filter_input32(cpu,m8_fp_bits32(value));
    if(m8_fp_nan32(bits)) {cpu->fpscr|=1u;return 0;}
    float x=m8_fp_float(bits);
    if(uns) {
        if(x<=-1.0f) {cpu->fpscr|=1u;return 0;}
        if(x>=4294967296.0f) {cpu->fpscr|=1u;return UINT32_MAX;}
        uint32_t result=(uint32_t)x;
        if((float)result!=x) cpu->fpscr|=16u;
        return result;
    }
    if(x<-2147483648.0f) {cpu->fpscr|=1u;return 0x80000000u;}
    if(x>=2147483648.0f) {cpu->fpscr|=1u;return 0x7fffffffu;}
    int32_t result=(int32_t)x;
    if((float)result!=x) cpu->fpscr|=16u;
    return (uint32_t)result;
}
M8_MEM_INLINE void m8_filter_apply(m8_cpu *cpu,float *registers,uint32_t kind,
        uint8_t *restrict input,uint8_t *restrict words,const uint8_t *restrict constants) {
    float s0=m8_fp_float(m8_fp_bits32(registers[0])),s1=m8_fp_float(m8_fp_bits32(registers[1]));
    uint32_t s1_bits;
    float s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15;
    s14=m8_filter_read(constants+8u);
    s12=1.0f;
    s15=m8_filter_read(constants+12u);
    s0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s0),m8_fp_bits32(s14),0));
    s8=m8_filter_read(constants+16u);
    s0=m8_filter_op(cpu,M8_FP_MUL,s0,s15,0);
    s15=m8_filter_read(constants+20u);
    s14=m8_filter_op(cpu,M8_FP_SUB,s12,s1,0);
    s7=m8_filter_read(input);
    s13=m8_filter_read(words+4u);
    uint64_t d2=m8_filter_widen(cpu,s0);
    s4=m8_fp_float((uint32_t)d2);s5=m8_fp_float((uint32_t)(d2>>32));
    s6=m8_filter_read(constants+24u);
    uint64_t d5=(uint64_t)m8_filter_read32(constants)|((uint64_t)m8_filter_read32(constants+4u)<<32);
    s10=m8_fp_float((uint32_t)d5);s11=m8_fp_float((uint32_t)(d5>>32));
    s14=m8_filter_op(cpu,M8_FP_MUL,s14,s6,0);
    uint64_t d0=m8_fp_op64(cpu,M8_FP_MUL,d2,d5,0);
    s0=m8_fp_float((uint32_t)d0);s1_bits=(uint32_t)(d0>>32);
    s10=m8_filter_read(words+8u);
    s11=m8_filter_read(words);
    s9=m8_filter_op(cpu,M8_FP_SUB,s7,s10,0);
    s0=m8_filter_narrow(cpu,d0);
    s15=m8_filter_op(cpu,M8_FP_FMA,s0,s8,s15);
    s8=m8_filter_op(cpu,M8_FP_MUL,s0,s0,0);
    s8=m8_filter_op(cpu,M8_FP_MUL,s8,s0,0);
    s0=m8_filter_op(cpu,M8_FP_FMA,s8,s15,s0);
    s8=m8_filter_op(cpu,M8_FP_ADD,s0,s12,0);
    s15=m8_filter_op(cpu,M8_FP_DIV,s0,s8,0);
    s8=s11;
    s8=m8_filter_op(cpu,M8_FP_FMA,s15,s9,s8);
    s5=m8_filter_op(cpu,M8_FP_ADD,s15,s15,0);
    s6=m8_filter_op(cpu,M8_FP_MUL,s15,s15,0);
    s4=m8_filter_op(cpu,M8_FP_SUB,s6,s15,0);
    s10=m8_filter_op(cpu,M8_FP_ADD,s10,s8,0);
    s9=m8_filter_op(cpu,M8_FP_SUB,s9,s8,0);
    s12=m8_filter_op(cpu,M8_FP_FMA,s4,s14,s12);
    s8=m8_filter_op(cpu,M8_FP_SUB,s10,s13,0);
    s11=m8_filter_op(cpu,M8_FP_FMA,s9,s5,s11);
    s9=s8;
    s9=m8_filter_op(cpu,M8_FP_FMA,-s15,s10,s9);
    s15=m8_filter_op(cpu,M8_FP_MUL,s11,s15,0);
    m8_filter_write(words,s11);
    s13=m8_filter_op(cpu,M8_FP_FMA,s9,s5,s13);
    m8_filter_write(words+4u,s13);
    s11=m8_filter_read(input);
    s15=m8_filter_op(cpu,M8_FP_FMA,s11,s6,s15);
    s13=m8_filter_op(cpu,M8_FP_ADD,s13,s15,0);
    s15=m8_filter_op(cpu,M8_FP_DIV,s13,s12,0);
    s14=m8_filter_op(cpu,M8_FP_MUL,s15,s14,0);
    m8_filter_write(input,s15);
    m8_filter_write(words+8u,s14);
    s15=m8_filter_read(input);
    if(kind) s15=m8_filter_op(cpu,M8_FP_SUB,s15,s7,0);
    m8_filter_write(input,s15);
    registers[0]=s0;registers[1]=m8_fp_float(s1_bits);
    registers[4]=s4;registers[5]=s5;registers[6]=s6;registers[7]=s7;
    registers[8]=s8;registers[9]=s9;registers[10]=s10;registers[11]=s11;
    registers[12]=s12;registers[13]=s13;registers[14]=s14;registers[15]=s15;
}
#endif
#endif
