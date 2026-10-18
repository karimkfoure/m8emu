/* Exact callback publication for the checked structural SVF batch.
 * The caller qualifies callback metadata and the selected callback before
 * guest effects. Read S1 at the actual call position: the table callback
 * must observe the original phase/filter/stack/ring stores. */
#ifndef M8_AOT_FM_SVF_BATCH_H
#define M8_AOT_FM_SVF_BATCH_H
#if defined(M8_AOT_FM_SVF_BATCH)
#if !defined(M8_AOT_FM_SVF_EXACT) || !defined(M8_AOT_FM_CALLBACK_2284)
#error "SVF batch requires the exact SVF and checked table callback"
#endif
#include "fm_svf_exact.h"
#if defined(M8_AOT_FM_CALLBACK_1FBC)
#include "fm_callback_1fbc.h"
#endif
M8_FMS_INLINE void m8_fms_svf_callback(m8_cpu *cpu,const uint8_t *control,
        uint32_t callback_address,uint32_t target,uint32_t link,
        const m8_fmcb2284_context *context) {
    cpu->r[0]=callback_address;cpu->r[2]=target;cpu->r[14]=link;
    cpu->s[1]=m8_le_read(control+0x4c,4);
    if(target==0x2285u) m8_fmcb2284_apply(cpu,context);
#if defined(M8_AOT_FM_CALLBACK_1FBC)
    else if(target==0x1fbdu) m8_fmcb1fbc_apply(cpu);
#endif
    else {
        cpu->s[14]=0xbf800000u;cpu->s[15]=0x3f800000u;
        cpu->s[0]=m8_fms_op(cpu,M8_FP_MUL,cpu->s[0],cpu->s[1],0);
        cpu->s[0]=m8_fms_op(cpu,M8_FP_MAXNM,cpu->s[0],cpu->s[14],0);
        cpu->s[0]=m8_fms_op(cpu,M8_FP_MINNM,cpu->s[0],cpu->s[15],0);
        cpu->r[15]=link&~1u;
    }
}
#endif
#endif
