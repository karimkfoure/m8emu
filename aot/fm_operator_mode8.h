/* Exact scalar mode-8 operator for the structural FM candidate.
 * Headless 6.5.2C: 12174..121ee, 128b4..12994, 12cb0..12cc4,
 * 12d78..12d94, 1267c..12680 and 123c0..123f4.
 * The parent qualifies object/ports/stack spans and the readable PM/bias
 * words. It publishes the GPR return state. No pointer is restrict:
 * PM and bias values must observe the ordered guest stores below.
 */
#ifndef M8_AOT_FM_OPERATOR_MODE8_H
#define M8_AOT_FM_OPERATOR_MODE8_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_MODE8)
M8_FMS_INLINE int m8_fm8_gt(uint32_t compare) {
    return !(compare&0x40000000u) &&
        !(((compare>>31)^(compare>>28))&1u);
}
M8_FMS_INLINE void m8_fms_operator_mode8(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,const uint8_t *constants,
        uint8_t *stack,uint32_t matrix_address,const uint8_t *bias) {
    (void)constants;
    uint32_t s0,s9,s10,s11,s12,s13,s14=0x3f000000u,s15;
    uint32_t step=m8_le_read(ports,4);
    s15=m8_fms_op(cpu,M8_FP_ADD,m8_le_read(object+4,4),step,0);
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

    s9=0x3f000000u;s10=0xbf800000u;
    s13=m8_le_read(ports+20,4);
    s14=m8_fms_op(cpu,M8_FP_MUL,s14,s13,0);
    s15=m8_fms_op(cpu,M8_FP_ADD,s14,s15,0);
    m8_fm_store_bits(ports+20,s14);s14=0x3f000000u;
    m8_fp_cmp32(cpu,s15,0,1);
    s14=m8_fms_op(cpu,(cpu->fpscr&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    s14=m8_fms_to_int(cpu,s14,0);
    s14=m8_fms_from_int(cpu,s14,0);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s14,0);s14=0x3f800000u;
    m8_fp_cmp32(cpu,s15,0,1);
    s12=m8_fms_op(cpu,M8_FP_ADD,s15,s9,0);
    s11=m8_fm8_gt(cpu->fpscr)?s14:s10;
    m8_fm_store_bits(ports+8,s11);
    /* R2 still names op+1c here. It does not name the depth at op+24. */
    s13=m8_le_read(ports,4);
    m8_fp_cmp32(cpu,s13,s12,1);
    if(m8_fm8_gt(cpu->fpscr)) {
        s10=m8_fms_op(cpu,M8_FP_DIV,s12,s13,0);s13=0x40000000u;
        s13=m8_fms_op(cpu,M8_FP_SUB,s13,s10,0);
        s14=m8_fms_op(cpu,M8_FP_FMA,s10^0x80000000u,s13,s14);
        s11=m8_fms_op(cpu,M8_FP_ADD,s11,s14,0);
    } else {
        s14=m8_fms_op(cpu,M8_FP_SUB,s14,s13,0);
        m8_fp_cmp32(cpu,s12,s14,1);
        if(m8_fm8_gt(cpu->fpscr)) {
            s9=m8_fms_op(cpu,M8_FP_SUB,s15,s9,0);s14=0x40000000u;
            s12=m8_fms_op(cpu,M8_FP_DIV,s9,s13,0);
            s14=m8_fms_op(cpu,M8_FP_ADD,s12,s14,0);
            s10=m8_fms_op(cpu,M8_FP_FMA,s12^0x80000000u,s14,s10);
            s11=m8_fms_op(cpu,M8_FP_ADD,s11,s10,0);
        }
    }
    m8_fp_cmp32(cpu,s15,0,1);m8_fm_store_bits(ports+8,s11);
    if(!m8_fm8_gt(cpu->fpscr)) {
        s14=0x3f800000u;s15=m8_fms_op(cpu,M8_FP_ADD,s15,s14,0);
    }
    s14=m8_le_read(ports,4);
    m8_fp_cmp32(cpu,s14,s15,1);
    if(m8_fm8_gt(cpu->fpscr)) {
        s13=m8_fms_op(cpu,M8_FP_DIV,s15,s14,0);
        s14=0x40000000u;s15=0x3f800000u;
        s15=m8_fms_op(cpu,M8_FP_SUB,s11,s15,0);
        s14=m8_fms_op(cpu,M8_FP_SUB,s14,s13,0);
        s15=m8_fms_op(cpu,M8_FP_FMA,s13,s14,s15);s11=s15;
    } else {
        s13=0x3f800000u;s12=m8_fms_op(cpu,M8_FP_SUB,s13,s14,0);
        m8_fp_cmp32(cpu,s15,s12,1);
        if(m8_fm8_gt(cpu->fpscr)) {
            s15=m8_fms_op(cpu,M8_FP_SUB,s15,s13,0);s12=0x40000000u;
            s13=m8_fms_op(cpu,M8_FP_ADD,s11,s13,0);
            s10=m8_fms_op(cpu,M8_FP_DIV,s15,s14,0);
            s15=m8_fms_op(cpu,M8_FP_ADD,s10,s12,0);
            s13=m8_fms_op(cpu,M8_FP_FMA,s10,s15,s13);s11=s13;
        }
    }
    uint32_t compare=cpu->fpscr&0xf0000000u;
    m8_fm_store_bits(ports+8,s11);
    s0=m8_fms_op(cpu,M8_FP_MUL,s11,m8_le_read(ports+12,4),0);
    m8_fm_store_bits(ports+8,s0);s15=m8_le_read(ports+16,4);
    s0=m8_fms_op(cpu,M8_FP_MUL,s0,s15,0);m8_fm_store_bits(ports+8,s0);
    s15=m8_le_read(bias,4);s0=m8_fms_op(cpu,M8_FP_ADD,s0,s15,0);
    m8_fm_store_bits(ports+8,s0);object[0x38]=1;
    cpu->s[0]=s0;cpu->s[9]=s9;cpu->s[10]=s10;cpu->s[11]=s11;
    cpu->s[12]=s12;cpu->s[13]=s13;cpu->s[14]=s14;cpu->s[15]=s15;
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    /* The tail MOVS r3,1 clears N/Z and preserves compare C/V. */
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(compare&0x30000000u);
}
#endif
#endif
