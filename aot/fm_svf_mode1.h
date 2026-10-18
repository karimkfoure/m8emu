/* Headless 6.5.2C FM filter families 1..5, 0x14438..0x144f4 with
 * family selections at 0x145c8..0x145e6. The caller admits S17==+0 and
 * disjoint object/control/literal spans. Retain the original FP order.
 * This helper leaves PC publication and the FP environment to its caller. */
#ifndef M8_AOT_FM_SVF_MODE1_H
#define M8_AOT_FM_SVF_MODE1_H
#include "filter_kernel.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
static inline void m8_fmb_filter_svf(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal,unsigned family) {
    float s0=m8_fp_float(cpu->s[0]),s15=1.0f;
    float s13=m8_filter_read(control+0x44);
    float s10=m8_filter_read(control+0x48);
    s13=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s13),m8_fp_bits32(s15),0));
    s13=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s13),cpu->s[17],0));
    /* PC-relative addresses are 0x14418, 0x1441c, 0x14420, 0x14424.
     * The literal pointer starts at the existing 0x143f8 FM pool. */
    float s7=m8_filter_read(literal+0x20);
    if(family==5) {
        s7=m8_filter_op(cpu,M8_FP_SUB,s10,m8_filter_read(literal+0x3c),0);
        s10=s15;
    }
    s15=m8_filter_read(object+0x24);
    float s12=1.0f,s14=m8_filter_read(object+0x20);
    /* Families 1..4 leave equal flags; family 5 ends with CMP R1,#4. */
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    float s11=m8_filter_op(cpu,M8_FP_SUB,s0,s15,0);
    float s6=m8_filter_read(literal+0x24);
    float s8=m8_filter_read(literal+0x28);
    float s9=m8_filter_read(literal+0x2c);
    s11=m8_filter_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s14)^0x80000000u),s10,s11);
    s14=m8_filter_op(cpu,M8_FP_FMA,s13,s11,s14);
    s11=s0;
    s15=m8_filter_op(cpu,M8_FP_FMA,s13,s14,s15);
    s11=m8_filter_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s14,s11);
    s11=m8_filter_op(cpu,M8_FP_SUB,s11,s15,0);
    s14=m8_filter_op(cpu,M8_FP_FMA,s13,s11,s14);
    s15=m8_filter_op(cpu,M8_FP_FMA,s13,s14,s15);
    s14=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s14),m8_fp_bits32(s6),0));
    s13=s12;
    s14=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s14),m8_fp_bits32(s8),0));
    s10=m8_fp_float(m8_fp_bits32(s14)&0x7fffffffu);
    s13=m8_filter_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s13);
    s15=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s15),m8_fp_bits32(s6),0));
    s15=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s15),m8_fp_bits32(s8),0));
    s10=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    s12=m8_filter_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s12);
    s14=m8_filter_op(cpu,M8_FP_MUL,s14,s13,0);
    m8_filter_write(object+0x20,s14);
    s15=m8_filter_op(cpu,M8_FP_MUL,s15,s12,0);
    m8_filter_write(object+0x24,s15);
    if(family==2)s15=s11;
    else if(family==3)s15=s14;
    else if(family==4)s15=m8_filter_op(cpu,M8_FP_SUB,s0,s14,0);
    if(family==5)cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x20000000u;
    s0=m8_filter_read(object+0x28);
    s14=m8_filter_read(object+0x2c);
    m8_filter_write(object+0x28,s15);
    s0=m8_filter_op(cpu,M8_FP_FMA,s14,s7,m8_fp_float(m8_fp_bits32(s0)^0x80000000u));
    s0=m8_filter_op(cpu,M8_FP_ADD,s15,s0,0);
    m8_filter_write(object+0x2c,s0);
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[6]=m8_fp_bits32(s6);
    cpu->s[7]=m8_fp_bits32(s7);cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11);cpu->s[12]=m8_fp_bits32(s12);
    cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);
}
static inline void m8_fmb_filter_mode1(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal) {
    m8_fmb_filter_svf(cpu,object,control,literal,1);
}
#endif
#endif
