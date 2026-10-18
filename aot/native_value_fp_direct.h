/* Default-off direct native helper over private scalar FPSCR storage.
 * The original helper owns arithmetic, NaN rules and pending host flags.
 * Only the cold helper can observe this scratch CPU; the active CPU stays
 * in the caller. Keep the original enter/sync/leave environment boundaries.
 */
#ifndef M8_AOT_NATIVE_VALUE_FP_DIRECT_H
#define M8_AOT_NATIVE_VALUE_FP_DIRECT_H
#ifndef M8_AOT_NATIVE_FP
#error "native_value_fp_direct.h requires the original native helper profile"
#endif
#include "native_value_fp.h"
#define M8_NATIVE_DIRECT_INLINE static inline __attribute__((always_inline))
M8_NATIVE_DIRECT_INLINE uint64_t m8_native_direct_native32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    m8_cpu scratch;
    scratch.fpscr=fpscr;
    uint32_t result=m8_fp_native32(&scratch,op,a,b,d);
    return m8_native_value_pack32(result,scratch.fpscr);
}
M8_NATIVE_DIRECT_INLINE uint64_t m8_native_direct_eval32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
#if defined(m8_fp_eval32)
    return m8_native_direct_native32(fpscr,op,a,b,d);
#else
    return m8_native_value_eval32(fpscr,op,a,b,d);
#endif
}
#define m8_native_value_eval32 m8_native_direct_eval32
#define m8_native_value_native32 m8_native_direct_native32
#undef M8_NATIVE_DIRECT_INLINE
#endif
