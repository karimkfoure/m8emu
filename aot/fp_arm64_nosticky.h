/* AArch64 result-only FP for the fixed Headless 6.5.2C RN/FZ/DN mode.
 * Included by fp_nosticky.h. FPCR.AH must be zero (original ARM behavior).
 * No helper below reads or writes FPCR/FPSR. NZCV remains guest-visible.
 * Explicit instructions prevent constant folding from bypassing FZ/DN and
 * prevent contraction of the separately rounded VMLA/VMLS operations.
 * Non-volatile assembly permits dead-result removal: flags are unobservable.
 */
#ifndef M8_AOT_FP_ARM64_NOSTICKY_H
#define M8_AOT_FP_ARM64_NOSTICKY_H

#define M8_ARM64_RESULT_OP(TYPE, SUFFIX) \
M8_NOSTICKY_INLINE TYPE m8_fp_arm64_result_##SUFFIX(int op, TYPE a, TYPE b, TYPE d) { \
    TYPE r; \
    switch (op) { \
    case M8_FP_ADD: __asm__("fadd %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    case M8_FP_SUB: __asm__("fsub %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    case M8_FP_MUL: __asm__("fmul %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    case M8_FP_DIV: __asm__("fdiv %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    case M8_FP_SQRT: __asm__("fsqrt %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(a)); break; \
    case M8_FP_MAXNM: __asm__("fmaxnm %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    case M8_FP_MINNM: __asm__("fminnm %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2" : "=w"(r) : "w"(a), "w"(b)); break; \
    default: __asm__("fmadd %" #SUFFIX "0, %" #SUFFIX "1, %" #SUFFIX "2, %" #SUFFIX "3" : "=w"(r) : "w"(a), "w"(b), "w"(d)); break; \
    } \
    return r; \
}
M8_ARM64_RESULT_OP(float, s)
M8_ARM64_RESULT_OP(double, d)
#undef M8_ARM64_RESULT_OP

M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky32(m8_cpu *c, int op,
        uint32_t a, uint32_t b, uint32_t d) {
    (void)c;
    /* The exact helper treats a NaN in SQRT's unused slot as an input NaN.
     * The emitter passes literal zero, so this compatibility check folds out. */
    if (op == M8_FP_SQRT && m8_fp_nan32(b)) return 0x7fc00000u;
    return m8_fp_bits32(m8_fp_arm64_result_s(op, m8_fp_float(a), m8_fp_float(b), m8_fp_float(d)));
}
M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky64(m8_cpu *c, int op,
        uint64_t a, uint64_t b, uint64_t d) {
    (void)c;
    if (op == M8_FP_SQRT && m8_fp_nan64(b)) return UINT64_C(0x7ff8000000000000);
    return m8_fp_bits64(m8_fp_arm64_result_d(op, m8_fp_double(a), m8_fp_double(b), m8_fp_double(d)));
}

#define M8_ARM64_RESULT_CMP(BITS, UINT, TYPE, SUFFIX) \
M8_NOSTICKY_INLINE void m8_fp_nosticky_cmp##BITS(m8_cpu *c, UINT a, UINT b, int signaling) { \
    uint64_t nzcv; \
    (void)signaling; \
    __asm__("fcmp %" #SUFFIX "1, %" #SUFFIX "2\n\tmrs %0, nzcv" \
            : "=r"(nzcv) : "w"(m8_fp_##TYPE(a)), "w"(m8_fp_##TYPE(b)) : "cc"); \
    c->fpscr = (c->fpscr & 0x0fffffffu) | (uint32_t)nzcv; \
}
M8_ARM64_RESULT_CMP(32, uint32_t, float, s)
M8_ARM64_RESULT_CMP(64, uint64_t, double, d)
#undef M8_ARM64_RESULT_CMP

/* FCVT implements the guest's NaN-to-zero and signed/unsigned saturation.
 * Select the opcode's rounding mode without changing FPCR. */
#define M8_ARM64_INT_CASE(MODE, ROUND, SUFFIX) \
    case MODE: \
        if (uns) __asm__("fcvt" ROUND "u %w0, %" #SUFFIX "1" : "=r"(r) : "w"(x)); \
        else __asm__("fcvt" ROUND "s %w0, %" #SUFFIX "1" : "=r"(r) : "w"(x)); \
        break;
#define M8_ARM64_TO_INT(TYPE, SUFFIX) \
M8_NOSTICKY_INLINE uint32_t m8_fp_arm64_to_int_##SUFFIX(TYPE x, int uns, unsigned mode) { \
    uint32_t r; \
    switch (mode) { \
    M8_ARM64_INT_CASE(1, "p", SUFFIX) \
    M8_ARM64_INT_CASE(2, "m", SUFFIX) \
    M8_ARM64_INT_CASE(3, "z", SUFFIX) \
    M8_ARM64_INT_CASE(4, "a", SUFFIX) \
    default: \
        if (uns) __asm__("fcvtnu %w0, %" #SUFFIX "1" : "=r"(r) : "w"(x)); \
        else __asm__("fcvtns %w0, %" #SUFFIX "1" : "=r"(r) : "w"(x)); \
    } \
    return r; \
}
M8_ARM64_TO_INT(float, s)
M8_ARM64_TO_INT(double, d)
#undef M8_ARM64_TO_INT
#undef M8_ARM64_INT_CASE

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_convert(m8_cpu *c, uint64_t a, int from_dp) {
    (void)c;
    if (from_dp) {
        float r;
        __asm__("fcvt %s0, %d1" : "=w"(r) : "w"(m8_fp_double(a)));
        return m8_fp_bits32(r);
    }
    double r;
    __asm__("fcvt %d0, %s1" : "=w"(r) : "w"(m8_fp_float((uint32_t)a)));
    return m8_fp_bits64(r);
}

M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky_to_int(m8_cpu *c, uint64_t a, int dp,
        int uns, unsigned mode, unsigned bits, unsigned frac) {
    uint32_t r;
    if (!frac) {
        r = dp ? m8_fp_arm64_to_int_d(m8_fp_double(a), uns, mode)
               : m8_fp_arm64_to_int_s(m8_fp_float((uint32_t)a), uns, mode);
    } else {
        double x = m8_fp_double(dp ? a : m8_fp_nosticky_convert(c, a, 0));
        /* VCVT fixed-point has 0..32 fraction bits. Scaling in binary64
         * preserves binary32 input precision and avoids premature overflow. */
        x = m8_fp_arm64_result_d(M8_FP_MUL, x,
                m8_fp_double((UINT64_C(1023) + frac) << 52), 0);
        r = m8_fp_arm64_to_int_d(x, uns, mode);
    }
    if (bits == 16) {
        if (uns) { if (r > UINT16_MAX) r = UINT16_MAX; }
        else if ((int32_t)r > INT16_MAX) r = INT16_MAX;
        else if ((int32_t)r < INT16_MIN) r = (uint32_t)INT16_MIN;
    }
    return r;
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_from_int(m8_cpu *c, uint32_t a, int dp,
        int uns, unsigned bits, unsigned frac, int nearest) {
    (void)nearest; /* The admitted guest mode is already RN. */
    if (bits == 16) a = uns ? (uint16_t)a : (uint32_t)(int32_t)(int16_t)a;
    if (!dp && !frac) {
        float r;
        if (uns) __asm__("ucvtf %s0, %w1" : "=w"(r) : "r"(a));
        else __asm__("scvtf %s0, %w1" : "=w"(r) : "r"(a));
        return m8_fp_bits32(r);
    }
    double r;
    if (uns) __asm__("ucvtf %d0, %w1" : "=w"(r) : "r"(a));
    else __asm__("scvtf %d0, %w1" : "=w"(r) : "r"(a));
    if (frac) r = m8_fp_arm64_result_d(M8_FP_MUL, r,
            m8_fp_double((UINT64_C(1023) - frac) << 52), 0);
    return dp ? m8_fp_bits64(r) : m8_fp_nosticky_convert(c, m8_fp_bits64(r), 1);
}

#define M8_ARM64_ROUND(TYPE, SUFFIX) \
M8_NOSTICKY_INLINE TYPE m8_fp_arm64_round_##SUFFIX(TYPE x, unsigned mode) { \
    TYPE r; \
    switch (mode) { \
    case 1: __asm__("frintp %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(x)); break; \
    case 2: __asm__("frintm %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(x)); break; \
    case 3: __asm__("frintz %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(x)); break; \
    case 4: __asm__("frinta %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(x)); break; \
    default: __asm__("frintn %" #SUFFIX "0, %" #SUFFIX "1" : "=w"(r) : "w"(x)); break; \
    } \
    return r; \
}
M8_ARM64_ROUND(float, s)
M8_ARM64_ROUND(double, d)
#undef M8_ARM64_ROUND

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_rint(m8_cpu *c, uint64_t a, int dp,
        unsigned mode, int exact) {
    (void)c; (void)exact;
    return dp ? m8_fp_bits64(m8_fp_arm64_round_d(m8_fp_double(a), mode))
              : m8_fp_bits32(m8_fp_arm64_round_s(m8_fp_float((uint32_t)a), mode));
}
#endif
