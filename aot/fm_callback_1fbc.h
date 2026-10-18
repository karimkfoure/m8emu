/* Exact Headless 6.5.2C callback 1fbc..1fea (target1fbd).
 * The original reads no guest RAM and changes S0/S1/S15, FP status,
 * APSR NZCV and PC only. Keep the compare/VMRS before the conditional
 * half-step and integer roundtrip; saturation and NaNs use exact APIs.
 */
#ifndef M8_AOT_FM_CALLBACK_1FBC_H
#define M8_AOT_FM_CALLBACK_1FBC_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_CALLBACK_1FBC)
M8_FMS_INLINE int m8_fmcb1fbc_prepare(m8_cpu *cpu,m8_mem *mem) {
    (void)mem;
    /* Preflight can run before the caller installs callback PC and LR.
     * Use the same exact FP domain as the structural parent. The original
     * path remains available for every other mode, before any effect. */
    return (cpu->xpsr&0x0700fc00u)==0x01000000u &&
        (cpu->fpscr&0x03c00010u)==0x03000010u;
}
M8_FMS_INLINE void m8_fmcb1fbc_apply(m8_cpu *cpu) {
    /* The caller enters the guest FP environment and installs an odd LR. */
    uint32_t s0=cpu->s[0],s1=cpu->s[1],s15=0x3f000000u;
    s1=m8_fms_op(cpu,M8_FP_MUL,s1,s15,0);
    s0=m8_fms_op(cpu,M8_FP_MUL,s1,s0,0);
    m8_fp_cmp32(cpu,s0,0,1);m8_fp_sync(cpu);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
    s15=m8_fms_op(cpu,(cpu->xpsr&0x80000000u)?M8_FP_SUB:M8_FP_ADD,s0,s15,0);
    s15=m8_fp_to_int(cpu,s15,0,0,3,32,0);
    s15=(uint32_t)m8_fp_from_int(cpu,s15,0,0,32,0,0);
    s0=m8_fms_op(cpu,M8_FP_SUB,s0,s15,0);
    s0=m8_fms_op(cpu,M8_FP_ADD,s0,s0,0);
    cpu->s[0]=s0;cpu->s[1]=s1;cpu->s[15]=s15;
    cpu->r[15]=cpu->r[14]&~1u;
}
#endif
#endif
