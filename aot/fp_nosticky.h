/* Result-only FP path for M8_NATIVE_FP_NO_STICKY (native only, default off).
 *
 * Must be included from aot/fp.h after the exact helpers and (optionally)
 * fast_fp.h. Uses m8_fp_float/bits, m8_fp_nan/snan and the M8_FP_* op numbers.
 *
 * Contract (proven against the 6.5.2C scan):
 * - Guest FP mode after boot is fixed: RN, FZ=1, DN=1. RMode never changes.
 * - Cumulative flags (IOC,DZC,OFC,UFC,IXC,IDC) never influence firmware
 *   behavior: the only full FPSCR read is at boot; all others read APSR only.
 * - Therefore this path keeps result bits exact and NZCV of compares, but
 *   never computes cumulative flags.
 *
 * Host mode is set once per thread (RN + FTZ/DAZ on x86, RN + FZ/DN on ARM64).
 * x86 FTZ detects tininess after rounding while ARM detects before rounding. The
 * only divergence is a result that rounds to min-normal magnitude from below;
 * those cases fall back to the existing exact helper (rare).
 */
#ifndef M8_AOT_FP_NOSTICKY_H
#define M8_AOT_FP_NOSTICKY_H

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define M8_NOSTICKY_INLINE static inline __attribute__((always_inline))
#else
#define M8_NOSTICKY_INLINE static inline
#endif

static inline void m8_fp_nosticky_init_thread(void) {
    static _Thread_local int done = 0;
    if (done) return;
    done = 1;
#if defined(__x86_64__) || defined(__i386__)
    fesetround(FE_TONEAREST);
    {
        unsigned c = (unsigned)_mm_getcsr();
        c = (c & ~(0xe040u)) | 0x1f80u;
        c |= 0x8040u;
        _mm_setcsr(c);
    }
#elif defined(__aarch64__)
    {
        /* RN, FZ, DN; no traps. Clear AH/FIZ (FEAT_AFP), FZ16 and AHP.
         * AH=0 selects ARM's original tininess-before-rounding behavior.
         * This thread owns its FP mode for the lifetime of guest execution. */
        uint64_t c = UINT64_C(0x03000000);
        __asm__ volatile("msr fpcr, %0" :: "r"(c) : "memory");
    }
#else
    fesetround(FE_TONEAREST);
#endif
}

M8_NOSTICKY_INLINE int m8_fp_nosticky_supported(const m8_cpu *cpu) {
    return (cpu->fpscr & 0x03c00000u) == 0x03000000u;
}

#if defined(__aarch64__)
#include "fp_arm64_nosticky.h"
#else
M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky_input32(uint32_t fpscr, uint32_t a) {
    if ((fpscr & (1u << 24)) && !(a & 0x7f800000u) && (a & 0x007fffffu))
        return a & 0x80000000u;
    return a;
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_input64(uint32_t fpscr, uint64_t a) {
    if ((fpscr & (1u << 24)) && !(a & UINT64_C(0x7ff0000000000000)) &&
        (a & UINT64_C(0x000fffffffffffff)))
        return a & UINT64_C(0x8000000000000000);
    return a;
}

/* Exact fallback for the min-normal boundary. Disables FTZ/DAZ so the
 * existing exact helper (and its tininess probe) observes the documented
 * RN/no-flush host mode, then restores the thread mode. Rare path. */
static inline uint32_t m8_fp_nosticky_fallback32(m8_cpu *cpu, int op,
        uint32_t a, uint32_t b, uint32_t d) {
#if defined(__x86_64__) || defined(__i386__)
    unsigned saved = (unsigned)_mm_getcsr();
    _mm_setcsr((saved & ~(0x8040u)) | 0x1f80u);
    m8_cpu scratch = *cpu;
    uint32_t r = m8_fp_op32(&scratch, op, a, b, d);
    _mm_setcsr(saved);
    return r;
#elif defined(__aarch64__)
    uint64_t saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(saved));
    __asm__ volatile("msr fpcr, %0" :: "r"(saved & ~((UINT64_C(3) << 24))) : "memory");
    m8_cpu scratch = *cpu;
    uint32_t r = m8_fp_op32(&scratch, op, a, b, d);
    __asm__ volatile("msr fpcr, %0" :: "r"(saved) : "memory");
    return r;
#else
    m8_cpu scratch = *cpu;
    return m8_fp_op32(&scratch, op, a, b, d);
#endif
}

static inline uint64_t m8_fp_nosticky_fallback64(m8_cpu *cpu, int op,
        uint64_t a, uint64_t b, uint64_t d) {
#if defined(__x86_64__) || defined(__i386__)
    unsigned saved = (unsigned)_mm_getcsr();
    _mm_setcsr((saved & ~(0x8040u)) | 0x1f80u);
    m8_cpu scratch = *cpu;
    uint64_t r = m8_fp_op64_exact(&scratch, op, a, b, d);
    _mm_setcsr(saved);
    return r;
#elif defined(__aarch64__)
    uint64_t saved;
    __asm__ volatile("mrs %0, fpcr" : "=r"(saved));
    __asm__ volatile("msr fpcr, %0" :: "r"(saved & ~((UINT64_C(3) << 24))) : "memory");
    m8_cpu scratch = *cpu;
    uint64_t r = m8_fp_op64_exact(&scratch, op, a, b, d);
    __asm__ volatile("msr fpcr, %0" :: "r"(saved) : "memory");
    return r;
#else
    m8_cpu scratch = *cpu;
    return m8_fp_op64_exact(&scratch, op, a, b, d);
#endif
}

M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky_flush32(uint32_t a) {
    return (!(a & 0x7f800000u) && (a & 0x007fffffu)) ? a & 0x80000000u : a;
}
M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky_minmax32(m8_cpu *cpu, int op,
        uint32_t a, uint32_t b) {
    (void)cpu;
    int na = m8_fp_nan32(a), nb = m8_fp_nan32(b);
    if (na || nb) {
        if (!m8_fp_snan32(a) && !m8_fp_snan32(b)) {
            if (!na) return m8_fp_nosticky_flush32(a);
            if (!nb) return m8_fp_nosticky_flush32(b);
        }
        return 0x7fc00000u;
    }
    /* DAZ orders subnormal inputs as signed zero; only returned bits need an
     * explicit flush (entry-admitted FZ=1). NaN and zero paths need none. */
    float x = m8_fp_float(a), y = m8_fp_float(b);
    uint32_t r;
    if (op == M8_FP_MAXNM)
        r = x > y ? a : x < y ? b : (a & b);
    else
        r = x < y ? a : x > y ? b : (a | b);
    return m8_fp_nosticky_flush32(r);
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_flush64(uint64_t a) {
    return (!(a & UINT64_C(0x7ff0000000000000)) &&
            (a & UINT64_C(0x000fffffffffffff)))
               ? a & UINT64_C(0x8000000000000000)
               : a;
}
M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_minmax64(m8_cpu *cpu, int op,
        uint64_t a, uint64_t b) {
    (void)cpu;
    int na = m8_fp_nan64(a), nb = m8_fp_nan64(b);
    if (na || nb) {
        if (!m8_fp_snan64(a) && !m8_fp_snan64(b)) {
            if (!na) return m8_fp_nosticky_flush64(a);
            if (!nb) return m8_fp_nosticky_flush64(b);
        }
        return UINT64_C(0x7ff8000000000000);
    }
    double x = m8_fp_double(a), y = m8_fp_double(b);
    uint64_t r;
    if (op == M8_FP_MAXNM)
        r = x > y ? a : x < y ? b : (a & b);
    else
        r = x < y ? a : x > y ? b : (a | b);
    return m8_fp_nosticky_flush64(r);
}

M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky32(m8_cpu *cpu, int op,
        uint32_t aa, uint32_t bb, uint32_t dd) {
    if (op >= M8_FP_MAXNM)
        return m8_fp_nosticky_minmax32(cpu, op, aa, bb);
#if !defined(__x86_64__) && !defined(__aarch64__)
    return m8_fp_nosticky_fallback32(cpu, op, aa, bb, dd);
#endif
    /* DAZ flushes subnormal inputs to signed zero exactly like ARM FZ input
     * flush, so no input check is needed. FTZ flushes tiny results after
     * rounding; only the min-normal boundary differs from ARM.
     * Guest mode (RN+FZ+DN) is admitted once at region/function entry by the
     * caller, matching the native backend's assumption; the firmware mode is
     * fixed after boot, so no per-op mode test is done here. */
    float x = m8_fp_float(aa), y = m8_fp_float(bb), z = m8_fp_float(dd);
    /* SQRT ignores its second slot in production (literal 0), but the exact
     * helper reports NaN if any slot holds NaN. Preserve that rule. */
    if (op == M8_FP_SQRT && m8_fp_nan32(bb))
        return 0x7fc00000u;
    float v;
    switch (op) {
    case M8_FP_ADD: v = x + y; break;
    case M8_FP_SUB: v = x - y; break;
    case M8_FP_MUL: v = x * y; break;
    case M8_FP_DIV: v = x / y; break;
    case M8_FP_SQRT: v = sqrtf(x); break;
    default:
#if defined(__FMA__) || defined(__aarch64__)
        v = fmaf(x, y, z); break;
#else
        return m8_fp_nosticky_fallback32(cpu, op, aa, bb, dd);
#endif
    }
    if (v != v)
        return 0x7fc00000u;
    uint32_t r = m8_fp_bits32(v);
    if ((r & 0x7fffffffu) == 0x00800000u)
        return m8_fp_nosticky_fallback32(cpu, op, aa, bb, dd);
    return r;
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky64(m8_cpu *cpu, int op,
        uint64_t aa, uint64_t bb, uint64_t dd) {
    if (op >= M8_FP_MAXNM)
        return m8_fp_nosticky_minmax64(cpu, op, aa, bb);
#if !defined(__x86_64__) && !defined(__aarch64__)
    return m8_fp_nosticky_fallback64(cpu, op, aa, bb, dd);
#endif
    double x = m8_fp_double(aa), y = m8_fp_double(bb), z = m8_fp_double(dd);
    if (op == M8_FP_SQRT && m8_fp_nan64(bb))
        return UINT64_C(0x7ff8000000000000);
    double v;
    switch (op) {
    case M8_FP_ADD: v = x + y; break;
    case M8_FP_SUB: v = x - y; break;
    case M8_FP_MUL: v = x * y; break;
    case M8_FP_DIV: v = x / y; break;
    case M8_FP_SQRT: v = sqrt(x); break;
    default:
#if defined(__FMA__) || defined(__aarch64__)
        v = fma(x, y, z); break;
#else
        return m8_fp_nosticky_fallback64(cpu, op, aa, bb, dd);
#endif
    }
    if (v != v)
        return UINT64_C(0x7ff8000000000000);
    uint64_t r = m8_fp_bits64(v);
    if ((r & UINT64_C(0x7fffffffffffffff)) == UINT64_C(0x0010000000000000))
        return m8_fp_nosticky_fallback64(cpu, op, aa, bb, dd);
    return r;
}

/* DAZ flushes subnormal compare inputs, matching ARM VCMP with FZ, so the
 * NZCV mapping below is a fixed table over the raw inputs. */
M8_NOSTICKY_INLINE void m8_fp_nosticky_cmp32(m8_cpu *c, uint32_t aa, uint32_t bb,
        int signaling) {
    uint32_t flags;
    if (m8_fp_nan32(aa) || m8_fp_nan32(bb)) {
        flags = 0x30000000u;
    } else {
        float x = m8_fp_float(aa), y = m8_fp_float(bb);
        flags = x == y ? 0x60000000u : x < y ? 0x80000000u : 0x20000000u;
    }
    (void)signaling;
    c->fpscr = (c->fpscr & 0x0fffffffu) | flags;
}

M8_NOSTICKY_INLINE void m8_fp_nosticky_cmp64(m8_cpu *c, uint64_t aa, uint64_t bb,
        int signaling) {
    uint32_t flags;
    if (m8_fp_nan64(aa) || m8_fp_nan64(bb)) {
        flags = 0x30000000u;
    } else {
        double x = m8_fp_double(aa), y = m8_fp_double(bb);
        flags = x == y ? 0x60000000u : x < y ? 0x80000000u : 0x20000000u;
    }
    (void)signaling;
    c->fpscr = (c->fpscr & 0x0fffffffu) | flags;
}

/* Result-only float-to-int: saturating fixups without cumulative flags.
 * x86 returns the indefinite value on overflow/NaN; subnormal inputs flush
 * to signed zero exactly like ARM input flush. */
M8_NOSTICKY_INLINE uint32_t m8_fp_nosticky_to_int(m8_cpu *c, uint64_t a, int dp,
        int uns, unsigned mode, unsigned bits, unsigned frac) {
    double x;
    if (dp) {
        a = m8_fp_nosticky_input64(c->fpscr, a);
        if (m8_fp_nan64(a))
            return 0;
        x = m8_fp_double(a);
    } else {
        uint32_t b = m8_fp_nosticky_input32(c->fpscr, (uint32_t)a);
        if (m8_fp_nan32(b))
            return 0;
        x = (double)m8_fp_float(b);
    }
    x = ldexp(x, (int)frac);
    double y = m8_fp_round(x, mode), lo = uns ? 0.0 : -ldexp(1.0, (int)bits - 1),
           hi = uns ? ldexp(1.0, (int)bits) - 1 : ldexp(1.0, (int)bits - 1) - 1;
    if (y < lo)
        return uns ? 0 : (uint32_t)(int32_t)lo;
    if (y > hi)
        return (uint32_t)hi;
    return uns ? (uint32_t)y : (uint32_t)(int32_t)y;
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_from_int(m8_cpu *c, uint32_t a, int dp,
        int uns, unsigned bits, unsigned frac, int nearest) {
    (void)c; (void)nearest;
    double x;
    if (bits == 16)
        x = uns ? (double)(uint16_t)a : (double)(int16_t)a;
    else
        x = uns ? (double)a : (double)(int32_t)a;
    if (frac)
        x = ldexp(x, -(int)frac);
    if (dp)
        return m8_fp_bits64(x);
    {
        float y = (float)x;
        uint32_t r = m8_fp_bits32(y);
        if (m8_fp_nan32(r))
            return 0x7fc00000u;
        if (!(r & 0x7f800000u) && (r & 0x007fffffu))
            return r & 0x80000000u;
        return r;
    }
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_convert(m8_cpu *c, uint64_t a,
        int from_dp) {
    if (from_dp) {
        uint64_t v = m8_fp_nosticky_input64(c->fpscr, a);
        if (m8_fp_nan64(v))
            return 0x7fc00000u;
        float y = (float)m8_fp_double(v);
        uint32_t r = m8_fp_bits32(y);
        if (m8_fp_nan32(r))
            return 0x7fc00000u;
        if (!(r & 0x7f800000u) && (r & 0x007fffffu))
            return r & 0x80000000u;
        if ((r & 0x7fffffffu) == 0x00800000u) {
#if defined(__x86_64__) || defined(__i386__)
            unsigned saved = (unsigned)_mm_getcsr();
            _mm_setcsr((saved & ~(0x8040u)) | 0x1f80u);
            m8_cpu scratch = *c;
            uint64_t e = m8_fp_convert_exact(&scratch, a, 1);
            _mm_setcsr(saved);
            return (uint32_t)e;
#else
            m8_cpu scratch = *c;
            return (uint32_t)m8_fp_convert_exact(&scratch, a, 1);
#endif
        }
        return r;
    }
    uint32_t b = m8_fp_nosticky_input32(c->fpscr, (uint32_t)a);
    if (m8_fp_nan32(b))
        return UINT64_C(0x7ff8000000000000);
    return m8_fp_bits64((double)m8_fp_float(b));
}

M8_NOSTICKY_INLINE uint64_t m8_fp_nosticky_rint(m8_cpu *c, uint64_t a, int dp,
        unsigned mode, int exact) {
    (void)exact;
    double x;
    if (dp) {
        uint64_t v = m8_fp_nosticky_input64(c->fpscr, a);
        if (m8_fp_nan64(v))
            return (c->fpscr & (1u << 25)) ? UINT64_C(0x7ff8000000000000)
                                           : (v | UINT64_C(0x0008000000000000));
        x = m8_fp_double(v);
    } else {
        uint32_t v = m8_fp_nosticky_input32(c->fpscr, (uint32_t)a);
        if (m8_fp_nan32(v))
            return (c->fpscr & (1u << 25)) ? 0x7fc00000u : (uint64_t)(v | 0x00400000u);
        x = m8_fp_float(v);
    }
    double y = m8_fp_round(x, mode);
    return dp ? m8_fp_bits64(y) : m8_fp_bits32((float)y);
}
#endif

/* Environment entry points: init once per thread, never save/restore/test. */
M8_NOSTICKY_INLINE m8_fp_env m8_fp_nosticky_enter(m8_cpu *cpu) {
    m8_fp_nosticky_init_thread();
    m8_fp_env e;
    memset(&e, 0, sizeof(e));
    e.cpu = cpu;
    return e;
}

M8_NOSTICKY_INLINE void m8_fp_nosticky_leave(m8_fp_env e) {
    (void)e;
}

M8_NOSTICKY_INLINE void m8_fp_nosticky_sync(m8_cpu *cpu) {
    (void)cpu;
}

#endif
