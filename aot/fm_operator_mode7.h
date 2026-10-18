/* Exact mode-7 saw operator for Headless 6.5.2C: common entry
 * 12174..121ee, waveform 12826..128b2, 12c1e..12c26, 12cc6..12cda,
 * and output 123c0..123f4. The caller qualifies readable sources and
 * writable spans. Keep every guest store in order. */
#ifndef M8_AOT_FM_OPERATOR_MODE7_H
#define M8_AOT_FM_OPERATOR_MODE7_H
M8_FMS_INLINE void m8_fms_operator_mode7(m8_cpu *cpu,uint8_t *object,
        uint8_t *ports,const uint8_t *pm,uint8_t *stack,
        uint32_t matrix_address,const uint8_t *bias) {
    uint32_t s0,s11,s12,s13,s14=0x3f000000u,s15;
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
    s13=m8_le_read(pm,4);s15=m8_fms_op(cpu,M8_FP_ADD,s15,s13,0);
    m8_fp_cmp32(cpu,s15,0,1);
    s14=m8_fms_op(cpu,(cpu->fpscr&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    s14=m8_fms_to_int(cpu,s14,0);s14=m8_fms_from_int(cpu,s14,0);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s14,0);
    s14=m8_le_read(ports+8,4);m8_fm_store_bits(ports+8,0);
    s13=m8_le_read(ports+20,4);s14=m8_fms_op(cpu,M8_FP_MUL,s14,s13,0);
    s15=m8_fms_op(cpu,M8_FP_ADD,s14,s15,0);m8_fm_store_bits(ports+20,s14);
    s14=0x3f000000u;m8_fp_cmp32(cpu,s15,0,1);
    s14=m8_fms_op(cpu,(cpu->fpscr&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s15,s14,0);
    s14=m8_fms_to_int(cpu,s14,0);s14=m8_fms_from_int(cpu,s14,0);
    s15=m8_fms_op(cpu,M8_FP_SUB,s15,s14,0);m8_fp_cmp32(cpu,s15,0,1);
    if(cpu->fpscr&0x80000000u) {
        s14=0x3f800000u;s15=m8_fms_op(cpu,M8_FP_ADD,s15,s14,0);
    }
    s12=m8_fms_op(cpu,M8_FP_ADD,s15,s15,0);s13=0x3f800000u;
    s11=m8_fms_op(cpu,M8_FP_SUB,s12,s13,0);m8_fm_store_bits(ports+8,s11);
    s14=m8_le_read(ports,4);m8_fp_cmp32(cpu,s14,s15,1);
    if(m8_fm8_gt(cpu->fpscr)) {
        s13=m8_fms_op(cpu,M8_FP_DIV,s15,s14,0);s15=0x40000000u;
        s15=m8_fms_op(cpu,M8_FP_SUB,s13,s15,0);
        s12=m8_fms_op(cpu,M8_FP_FMA,s15,s13,s12);s11=s12;
    } else {
        s12=m8_fms_op(cpu,M8_FP_SUB,s13,s14,0);m8_fp_cmp32(cpu,s15,s12,1);
        if(m8_fm8_gt(cpu->fpscr)) {
            s15=m8_fms_op(cpu,M8_FP_SUB,s15,s13,0);s13=0x40000000u;
            s12=m8_fms_op(cpu,M8_FP_DIV,s15,s14,0);
            s15=m8_fms_op(cpu,M8_FP_ADD,s15,s15,0);
            s14=m8_fms_op(cpu,M8_FP_ADD,s12,s13,0);
            s15=m8_fms_op(cpu,M8_FP_FMA,s12^0x80000000u,s14,s15);s11=s15;
        }
    }
    uint32_t compare=cpu->fpscr&0xf0000000u;
    m8_fm_store_bits(ports+8,s11);
    s0=m8_fms_op(cpu,M8_FP_MUL,s11,m8_le_read(ports+12,4),0);m8_fm_store_bits(ports+8,s0);
    s15=m8_le_read(ports+16,4);s0=m8_fms_op(cpu,M8_FP_MUL,s0,s15,0);m8_fm_store_bits(ports+8,s0);
    s15=m8_le_read(bias,4);s0=m8_fms_op(cpu,M8_FP_ADD,s0,s15,0);m8_fm_store_bits(ports+8,s0);
    object[0x38]=1;cpu->s[0]=s0;cpu->s[11]=s11;cpu->s[12]=s12;
    cpu->s[13]=s13;cpu->s[14]=s14;cpu->s[15]=s15;
    cpu->fpscr=(cpu->fpscr&0x0fffffffu)|compare;
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(compare&0x30000000u);
}
#endif
