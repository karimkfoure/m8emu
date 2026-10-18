/* Default-off native helper trial. The original native helper defers host
 * exception collection even before guest IXC becomes sticky. The generic
 * software-corrected helper has a different contract and keeps its old path.
 * Include at the same location as native_value_fp.h, after helper aliases
 * have been selected (or explicitly removed by a diagnostic).
 */
#ifndef M8_AOT_NATIVE_VALUE_FP_DEFERRED_H
#define M8_AOT_NATIVE_VALUE_FP_DEFERRED_H
#ifndef M8_AOT_NATIVE_FP
#error "native_value_fp_deferred.h requires the original native helper profile"
#endif
#include "native_value_fp.h"
#if defined(m8_fp_eval32)
#define M8_NATIVE_VALUE_EVAL_IS_DEFERRED 1
#else
#define M8_NATIVE_VALUE_EVAL_IS_DEFERRED 0
#endif
#define M8_NATIVE_DEFERRED_INLINE static inline __attribute__((always_inline))
M8_NATIVE_DEFERRED_INLINE uint64_t m8_native_deferred_arithmetic32(uint32_t fpscr,
        int kind,int op,uint32_t a,uint32_t b,uint32_t d) {
    int deferred=kind==M8_NATIVE_VALUE_NATIVE ||
        (kind==M8_NATIVE_VALUE_EVAL && M8_NATIVE_VALUE_EVAL_IS_DEFERRED);
    /* Adding IXC only tests the established finite operand bounds. The
     * returned guest status is unchanged; the active host FP environment
     * retains inexact until the original sync/leave observation boundary.
     */
    if(deferred && !d && m8_native_value_admit32(fpscr|16u,op,a,b)) {
        float x=m8_fp_float(a),y=m8_fp_float(b);
        volatile float result;
        switch(op) {
        case M8_FP_ADD:result=x+y;break;
        case M8_FP_SUB:result=x-y;break;
        case M8_FP_MUL:result=x*y;break;
        default:result=x/y;break;
        }
        return m8_native_value_pack32(m8_fp_bits32(result),fpscr);
    }
    return m8_native_value_arithmetic32(fpscr,kind,op,a,b,d);
}
M8_NATIVE_DEFERRED_INLINE uint64_t m8_native_deferred_eval32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_deferred_arithmetic32(fpscr,M8_NATIVE_VALUE_EVAL,op,a,b,d);
}
M8_NATIVE_DEFERRED_INLINE uint64_t m8_native_deferred_op32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_deferred_arithmetic32(fpscr,M8_NATIVE_VALUE_OP,op,a,b,d);
}
M8_NATIVE_DEFERRED_INLINE uint64_t m8_native_deferred_native32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_deferred_arithmetic32(fpscr,M8_NATIVE_VALUE_NATIVE,op,a,b,d);
}
#define m8_native_value_eval32 m8_native_deferred_eval32
#define m8_native_value_op32 m8_native_deferred_op32
#define m8_native_value_native32 m8_native_deferred_native32
#undef M8_NATIVE_DEFERRED_INLINE
#undef M8_NATIVE_VALUE_EVAL_IS_DEFERRED
#endif
