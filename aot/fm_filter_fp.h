/* Optional Wasm arithmetic for the admitted mode-6 FM batch filter only. */
#ifndef M8_AOT_FM_FILTER_FP_H
#define M8_AOT_FM_FILTER_FP_H
#if defined(M8_WEB_FM_FILTER_FP) && (defined(__wasm__) || defined(M8_WEB_FP_TEST))
#if !defined(M8_WEB_NATIVE_FP) || !defined(M8_WEB_EXACT_FP_FAST)
#error "FM filter arithmetic requires the exact Web FP profile"
#endif
/* The only production caller is m8_fmb_filter, after m8_fm_batch_kernel
 * proves RN/FZ/DN and sticky IXC. Its callbacks are outside this interval.
 * FP helpers only add status bits; compare results replace only FPSCR NZCV.
 * Therefore this mode remains valid through every operation below.
 *
 * Keep the existing bounded ADD/SUB and MUL ranges. The smallest nonzero
 * cancellation is normal and neither sum nor product can overflow. Zeros
 * extend these ranges without adding a guest exception. Strict Wasm f32
 * arithmetic supplies the same signed zero and round-to-nearest result.
 *
 * Existing bounded nonzero calls leave private SoftFloat state unchanged.
 * An admitted zero call previously used the software path: reproduce its
 * RNE/tininess-before/zero-flags reset even though no arithmetic is needed
 * there. All rejected values and ignored operands use the original helper. */
static inline __attribute__((always_inline)) float m8_fmb_filter_fp(
        m8_cpu *cpu,int op,float a,float b,float d) {
    uint32_t am=m8_fp_bits32(a)&0x7fffffffu;
    uint32_t bm=m8_fp_bits32(b)&0x7fffffffu;
    if(!m8_fp_bits32(d) &&
       (((op==M8_FP_ADD || op==M8_FP_SUB) &&
         (!am || am-0x10000000u<=0x5fffffffu) &&
         (!bm || bm-0x10000000u<=0x5fffffffu)) ||
        (op==M8_FP_MUL &&
         (!am || am-0x20800000u<=0x3e7fffffu) &&
         (!bm || bm-0x20800000u<=0x3e7fffffu)))) {
        if(!am || !bm) {
            softfloat_roundingMode=softfloat_round_near_even;
            softfloat_detectTininess=softfloat_tininess_beforeRounding;
            softfloat_exceptionFlags=0;
        }
        if(op==M8_FP_ADD)return a+b;
        if(op==M8_FP_SUB)return a-b;
        return a*b;
    }
    /* A rejected local binary case also fails the unchanged bounded shared
     * predicate. Enter its exact software API directly, with all operands. */
    if(op==M8_FP_ADD || op==M8_FP_SUB || op==M8_FP_MUL)
        return m8_fp_float(m8_fp_op32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
    return m8_filter_op(cpu,op,a,b,d);
}
#define m8_fmb_filter_op m8_fmb_filter_fp
#else
#define m8_fmb_filter_op m8_filter_op
#endif
#endif
