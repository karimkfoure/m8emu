/* Private, default-off FM float/int32/float conversion candidate.
 * The bounded input truncates without saturation; the integer is exactly
 * representable in binary32. Clear fractional mantissa bits without an FP
 * instruction, conversion cast or change to the host exception status.
 * The caller owns guest cumulative flags and private FP publication. */
#ifndef M8_AOT_FM_INTEGER_QUIET_H
#define M8_AOT_FM_INTEGER_QUIET_H
#include <float.h>
#include <stdint.h>
#include <string.h>
#ifdef __FAST_MATH__
#error "Quiet FM conversion requires strict IEEE arithmetic"
#endif
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error "Quiet FM conversion requires IEEE binary32"
#endif
static inline __attribute__((always_inline)) int
m8_fmiq_integer(float value,float *result) {
#if defined(M8_AOT_FM_INTEGER_QUIET)
    uint32_t bits;memcpy(&bits,&value,4);
    uint32_t magnitude=bits&0x7fffffffu;
    if(magnitude>0x4b000000u || (magnitude && magnitude<0x00800000u))return 0;
    /* Values below one truncate to positive integer zero. Otherwise the
     * exponent is 127..150, so the fractional-bit count is safely 23..0. */
    if(magnitude<0x3f800000u)bits=0;
    else bits&=~((UINT32_C(1)<<(150u-(magnitude>>23)))-1u);
    memcpy(result,&bits,4);return 1;
#else
    (void)value;(void)result;return 0;
#endif
}
#endif
