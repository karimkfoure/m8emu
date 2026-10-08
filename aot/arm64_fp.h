/* Default-off ARM64 arithmetic. Volatile operations preserve guest exceptions
 * even when the result is constant or overwritten. The native FP boundary
 * sets FPCR and collects FPSR; these operations do not change that contract. */
#ifndef M8_AOT_ARM64_FP_H
#define M8_AOT_ARM64_FP_H
#if defined(M8_AOT_ARM64_FP_HARDWARE) && defined(__aarch64__) && defined(M8_AOT_NATIVE_FP) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#define M8_FP_ARM64_HARDWARE 1
#define M8_ARM64_FP_OP(TYPE, SUFFIX) \
static inline __attribute__((always_inline)) TYPE m8_fp_arm64_##SUFFIX( \
        int op, TYPE a, TYPE b, TYPE d) { \
    TYPE result; \
    switch(op) { \
    case M8_FP_ADD: __asm__ volatile("fadd %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(result) : "w"(a), "w"(b)); break; \
    case M8_FP_SUB: __asm__ volatile("fsub %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(result) : "w"(a), "w"(b)); break; \
    case M8_FP_MUL: __asm__ volatile("fmul %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(result) : "w"(a), "w"(b)); break; \
    case M8_FP_DIV: __asm__ volatile("fdiv %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(result) : "w"(a), "w"(b)); break; \
    case M8_FP_SQRT: __asm__ volatile("fsqrt %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(result) : "w"(a)); break; \
    default: __asm__ volatile("fmadd %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2, %" #SUFFIX "3" : "=w"(result) : "w"(a), "w"(b), "w"(d)); break; \
    } \
    return result; \
}
M8_ARM64_FP_OP(float, s)
M8_ARM64_FP_OP(double, d)
#undef M8_ARM64_FP_OP
#endif
#endif
