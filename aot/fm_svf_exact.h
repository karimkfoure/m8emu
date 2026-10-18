/* Exact FM SVF families1..4, original14438..144f4 and145d6..145e8.
 * Include after the structural exact FP API. The caller qualifies disjoint
 * object/control/literal spans, R1==mode in1..4 and S17==+0 before effects.
 * Preserve state stores before each family selection and ordered output-state
 * reads. PC and callback publication belong to the batch caller. */
#ifndef M8_AOT_FM_SVF_EXACT_H
#define M8_AOT_FM_SVF_EXACT_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_SVF_EXACT)
M8_FMS_INLINE float m8_fmsvf_op(m8_cpu *cpu,int op,float a,float b,float d) {
    return m8_fp_float(m8_fms_op(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
M8_FMS_INLINE void m8_fmb_filter_svf_exact(m8_cpu *cpu,uint8_t *object,
        uint8_t *control,const uint8_t *literal,unsigned mode) {
    float s0=m8_fp_float(cpu->s[0]),s15=1.0f;
    float s13=m8_filter_read(control+0x44);
    float s10=m8_filter_read(control+0x48);
    s13=m8_fp_float(m8_fms_op(cpu,M8_FP_MINNM,m8_fp_bits32(s13),m8_fp_bits32(s15),0));
    s13=m8_fp_float(m8_fms_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s13),cpu->s[17],0));
    float s7=m8_filter_read(literal+0x20);
    s15=m8_filter_read(object+0x24);
    float s12=1.0f,s14=m8_filter_read(object+0x20);
    /* Each admitted family ends on its equal CMP, leaving NZCV60000000. */
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    float s11=m8_fmsvf_op(cpu,M8_FP_SUB,s0,s15,0);
    float s6=m8_filter_read(literal+0x24);
    float s8=m8_filter_read(literal+0x28);
    float s9=m8_filter_read(literal+0x2c);
    s11=m8_fmsvf_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s14)^0x80000000u),s10,s11);
    s14=m8_fmsvf_op(cpu,M8_FP_FMA,s13,s11,s14);
    s11=s0;
    s15=m8_fmsvf_op(cpu,M8_FP_FMA,s13,s14,s15);
    s11=m8_fmsvf_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s14,s11);
    s11=m8_fmsvf_op(cpu,M8_FP_SUB,s11,s15,0);
    s14=m8_fmsvf_op(cpu,M8_FP_FMA,s13,s11,s14);
    s15=m8_fmsvf_op(cpu,M8_FP_FMA,s13,s14,s15);
    s14=m8_fp_float(m8_fms_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s14),m8_fp_bits32(s6),0));
    s13=s12;
    s14=m8_fp_float(m8_fms_op(cpu,M8_FP_MINNM,m8_fp_bits32(s14),m8_fp_bits32(s8),0));
    s10=m8_fp_float(m8_fp_bits32(s14)&0x7fffffffu);
    s13=m8_fmsvf_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s13);
    s15=m8_fp_float(m8_fms_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s15),m8_fp_bits32(s6),0));
    s15=m8_fp_float(m8_fms_op(cpu,M8_FP_MINNM,m8_fp_bits32(s15),m8_fp_bits32(s8),0));
    s10=m8_fp_float(m8_fp_bits32(s15)&0x7fffffffu);
    s12=m8_fmsvf_op(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s10)^0x80000000u),s9,s12);
    s14=m8_fmsvf_op(cpu,M8_FP_MUL,s14,s13,0);
    m8_filter_write(object+0x20,s14);
    s15=m8_fmsvf_op(cpu,M8_FP_MUL,s15,s12,0);
    m8_filter_write(object+0x24,s15);
    /* Original145d6/145dc/145e2, after both state stores. */
    if(mode==2) s15=s11;
    else if(mode==3) s15=s14;
    else if(mode==4) s15=m8_fmsvf_op(cpu,M8_FP_SUB,s0,s14,0);
    s0=m8_filter_read(object+0x28);
    s14=m8_filter_read(object+0x2c);
    m8_filter_write(object+0x28,s15);
    s0=m8_fmsvf_op(cpu,M8_FP_FMA,s14,s7,m8_fp_float(m8_fp_bits32(s0)^0x80000000u));
    s0=m8_fmsvf_op(cpu,M8_FP_ADD,s15,s0,0);
    m8_filter_write(object+0x2c,s0);
    cpu->s[0]=m8_fp_bits32(s0);cpu->s[6]=m8_fp_bits32(s6);
    cpu->s[7]=m8_fp_bits32(s7);cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9);cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11);cpu->s[12]=m8_fp_bits32(s12);
    cpu->s[13]=m8_fp_bits32(s13);cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);
}
#endif
#endif
