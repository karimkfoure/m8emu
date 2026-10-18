/* Private default-off FP alias-analysis candidate. Include after the
 * prepared renderer body. A real, fully initialized m8_cpu owns FP status;
 * it is physically separate from the architectural register object.
 * The existing exact helpers and their native host environment stay intact.
 */
#ifndef M8_AOT_FM_FP_SHADOW_H
#define M8_AOT_FM_FP_SHADOW_H
#if defined(M8_AOT_FM_STRUCTURAL) && defined(M8_AOT_FM_FP_SHADOW)
M8_FMS_INLINE void m8_fm_fp_shadow_render(m8_cpu *cpu,m8_fms_context *context) {
    m8_cpu fp={0};
    fp.fpscr=cpu->fpscr;
    m8_fm_fp_shadow_body(cpu,&fp,context);
    cpu->fpscr=fp.fpscr;
}
#define m8_fms_render m8_fm_fp_shadow_render
#endif
#endif
