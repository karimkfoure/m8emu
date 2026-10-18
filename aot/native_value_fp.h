/* Default-off scalar FPSCR trial for the ORIGINAL native FP implementation.
 * The caller must keep the matching m8_fp_enter/native_enter environment active.
 * A packet contains result bits below the raw, possibly deferred, FPSCR value.
 * Sync and leave at the same observation boundaries as the original caller.
 * Require strict FP compilation (-frounding-math, no fast-math or contraction).
 */
#ifndef M8_AOT_NATIVE_VALUE_FP_H
#define M8_AOT_NATIVE_VALUE_FP_H
#ifdef M8_WEB_FP_WASM_H
#error "native_value_fp.h requires original aot/fp.h, not the web override"
#endif
#include "fp.h"
#if defined(__GNUC__) || defined(__clang__)
#define M8_NATIVE_VALUE_INLINE static inline __attribute__((always_inline))
#define M8_NATIVE_VALUE_COLD static __attribute__((cold,noinline))
#else
#define M8_NATIVE_VALUE_INLINE static inline
#define M8_NATIVE_VALUE_COLD static
#endif

enum { M8_NATIVE_VALUE_EVAL, M8_NATIVE_VALUE_OP, M8_NATIVE_VALUE_NATIVE };
M8_NATIVE_VALUE_INLINE uint64_t m8_native_value_pack32(uint32_t bits,uint32_t fpscr) {
    return (uint64_t)bits | ((uint64_t)fpscr << 32);
}
M8_NATIVE_VALUE_INLINE uint32_t m8_native_value_fpscr32(uint64_t packet) {
    return (uint32_t)(packet >> 32);
}
M8_NATIVE_VALUE_INLINE float m8_native_value_float32(uint64_t packet) {
    return m8_fp_float((uint32_t)packet);
}

/* Do not sync here. Once IXC is sticky the original helper leaves other host
 * exceptions pending until a declared sync/leave boundary. Eager collection
 * would change raw snapshots and the ordering of subsequent host exceptions.
 * All ignored operands still reach the same original helper.
 */
M8_NATIVE_VALUE_COLD uint64_t m8_native_value_slow32(uint32_t fpscr,int kind,
        int op,uint32_t a,uint32_t b,uint32_t d) {
    m8_cpu scratch;
    scratch.fpscr=fpscr;
    uint32_t result;
#ifdef M8_AOT_NATIVE_FP
    if(kind==M8_NATIVE_VALUE_NATIVE)result=m8_fp_native32(&scratch,op,a,b,d);
    else
#endif
    if(kind==M8_NATIVE_VALUE_EVAL)result=m8_fp_eval32(&scratch,op,a,b,d);
    else result=m8_fp_op32(&scratch,op,a,b,d);
    return m8_native_value_pack32(result,scratch.fpscr);
}

/* RN and sticky IXC permit only an already-recorded inexact exception.
 * The ADD/SUB exponent bounds exclude overflow, tiny results and denormals.
 * The MUL/DIV bounds exclude overflow, tiny results, zero divisors and NaNs.
 * Volatile evaluation also preserves the original helper's host flag effect
 * when the generated caller overwrites a result before its next sync boundary.
 */
M8_NATIVE_VALUE_INLINE int m8_native_value_admit32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b) {
    if((fpscr & 0x00c00010u)!=16u || (unsigned)op>3u)return 0;
    uint32_t am=a & 0x7fffffffu,bm=b & 0x7fffffffu;
    if(op<=M8_FP_SUB)
        return am-0x10000000u<=0x5fffffffu && bm-0x10000000u<=0x5fffffffu;
    return am-0x20800000u<=0x3e7fffffu && bm-0x20800000u<=0x3e7fffffu;
}
M8_NATIVE_VALUE_INLINE uint64_t m8_native_value_arithmetic32(uint32_t fpscr,
        int kind,int op,uint32_t a,uint32_t b,uint32_t d) {
    if(!d && m8_native_value_admit32(fpscr,op,a,b)) {
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
    return m8_native_value_slow32(fpscr,kind,op,a,b,d);
}
M8_NATIVE_VALUE_INLINE uint64_t m8_native_value_eval32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_value_arithmetic32(fpscr,M8_NATIVE_VALUE_EVAL,op,a,b,d);
}
M8_NATIVE_VALUE_INLINE uint64_t m8_native_value_op32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_value_arithmetic32(fpscr,M8_NATIVE_VALUE_OP,op,a,b,d);
}
#ifdef M8_AOT_NATIVE_FP
M8_NATIVE_VALUE_INLINE uint64_t m8_native_value_native32(uint32_t fpscr,int op,
        uint32_t a,uint32_t b,uint32_t d) {
    return m8_native_value_arithmetic32(fpscr,M8_NATIVE_VALUE_NATIVE,op,a,b,d);
}
#endif

M8_NATIVE_VALUE_INLINE uint32_t m8_native_value_sync32(uint32_t fpscr) {
    m8_cpu scratch;
    scratch.fpscr=fpscr;
    m8_fp_sync(&scratch);
    return scratch.fpscr;
}
M8_NATIVE_VALUE_INLINE uint32_t m8_native_value_leave32(m8_fp_env env,uint32_t fpscr) {
    env.cpu->fpscr=fpscr;
    m8_fp_leave(env);
    return env.cpu->fpscr;
}
#ifdef M8_AOT_NATIVE_FP
M8_NATIVE_VALUE_INLINE uint32_t m8_native_value_native_sync32(uint32_t fpscr) {
    m8_cpu scratch;
    scratch.fpscr=fpscr;
    m8_fp_native_sync(&scratch);
    return scratch.fpscr;
}
M8_NATIVE_VALUE_INLINE uint32_t m8_native_value_native_leave32(m8_fp_env env,uint32_t fpscr) {
    env.cpu->fpscr=fpscr;
    m8_fp_native_leave(env);
    return env.cpu->fpscr;
}
#endif
#undef M8_NATIVE_VALUE_INLINE
#undef M8_NATIVE_VALUE_COLD
#endif
