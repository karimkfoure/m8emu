/* Private NZCV values let a C compiler use native comparisons and defer
 * packed APSR construction to publication, calls and transactional exits. */
#ifndef M8_AOT_LOCAL_FLAGS_H
#define M8_AOT_LOCAL_FLAGS_H
#include <stdint.h>
#if defined(__GNUC__) || defined(__clang__)
#define M8_FLAGS_INLINE static inline __attribute__((always_inline))
#else
#define M8_FLAGS_INLINE static inline
#endif
M8_FLAGS_INLINE void m8_flags_nz(uint32_t *n,uint32_t *z,uint32_t r) {
    *n=r>>31;*z=!r;
}
M8_FLAGS_INLINE void m8_flags_logic(uint32_t *n,uint32_t *z,uint32_t *c,uint32_t r,uint32_t carry) {
    m8_flags_nz(n,z,r);*c=carry;
}
M8_FLAGS_INLINE uint32_t m8_flags_add(uint32_t *n,uint32_t *z,uint32_t *c,uint32_t *v,
                                   uint32_t a,uint32_t b,uint32_t carry) {
    uint64_t sum=(uint64_t)a+b+carry;
    uint32_t r=(uint32_t)sum;
    m8_flags_nz(n,z,r);*c=(uint32_t)(sum>>32);*v=(~(a^b)&(a^r))>>31;
    return r;
}
M8_FLAGS_INLINE uint32_t m8_flags_sub(uint32_t *n,uint32_t *z,uint32_t *c,uint32_t *v,
                                   uint32_t a,uint32_t b,uint32_t borrow) {
    uint64_t sub=(uint64_t)b+borrow;
    uint32_t r=a-b-borrow;
    m8_flags_nz(n,z,r);*c=(uint64_t)a>=sub;*v=((a^b)&(a^r))>>31;
    return r;
}
M8_FLAGS_INLINE int m8_flags_condition(uint32_t n,uint32_t z,uint32_t c,uint32_t v,unsigned cc) {
    switch(cc) {
    case 1:return z;case 2:return !z;case 3:return c;case 4:return !c;
    case 5:return n;case 6:return !n;case 7:return v;case 8:return !v;
    case 9:return c && !z;case 10:return !c || z;case 11:return n==v;
    case 12:return n!=v;case 13:return !z && n==v;case 14:return z || n!=v;
    default:return 1;
    }
}
#undef M8_FLAGS_INLINE
#endif
