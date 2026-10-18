/* Exact position/conversion arithmetic for the opt-in SAMPLER kernel.
 * The parent admits RN/FZ/DN. Each helper checks that mode again, so a
 * changed mode or an exceptional value retains the original FP operation.
 * No shared FP API is replaced outside this one kernel. */
#ifndef M8_AOT_SAMPLER_POSITION_FP_H
#define M8_AOT_SAMPLER_POSITION_FP_H
#include <float.h>
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || DBL_MANT_DIG != 53
#error "SAMPLER position arithmetic requires IEEE binary32 and binary64"
#endif
#ifdef __FAST_MATH__
#error "SAMPLER position arithmetic requires strict IEEE arithmetic"
#endif

M8_MEM_INLINE int m8_sampler_fp_mode(const m8_cpu *cpu) {
    return (cpu->fpscr&0x03c00000u)==0x03000000u;
}
M8_MEM_INLINE int m8_sampler_fp_normal64(uint64_t bits) {
    uint64_t magnitude=bits&UINT64_C(0x7fffffffffffffff);
    return !magnitude || (magnitude>=UINT64_C(0x0010000000000000) &&
                         magnitude<UINT64_C(0x7ff0000000000000));
}
M8_MEM_INLINE int m8_sampler_fp_position64(uint64_t bits) {
    uint64_t magnitude=bits&UINT64_C(0x7fffffffffffffff);
    return !magnitude || (magnitude>=UINT64_C(0x2ff0000000000000) &&
                         magnitude<=UINT64_C(0x4ff0000000000000));
}
M8_MEM_INLINE uint64_t m8_sampler_fp_op64(m8_cpu *cpu,int op,
        uint64_t a,uint64_t b,uint64_t d) {
    if(m8_sampler_fp_mode(cpu) && !d) {
        if((op==M8_FP_MAXNM || op==M8_FP_MINNM) &&
           m8_sampler_fp_normal64(a) && m8_sampler_fp_normal64(b)) {
            if(!((a|b)&UINT64_C(0x7fffffffffffffff)))
                return op==M8_FP_MAXNM?a&b:a|b;
            double x=m8_fp_double(a),y=m8_fp_double(b);
            return (op==M8_FP_MAXNM?x<y:y<x)?b:a;
        }
        /* Nonzero inputs lie in [2^-256,2^256]. Cancellation is at least
         * 2^-308; products and quotients lie in [2^-512,2^512]. Thus only
         * IXC can change, and IXC is already sticky. Signed zero stays exact. */
        if((cpu->fpscr&16u) && m8_sampler_fp_position64(a) &&
           m8_sampler_fp_position64(b) && (unsigned)op<=M8_FP_DIV &&
           (op!=M8_FP_DIV || (b&UINT64_C(0x7fffffffffffffff)))) {
            double x=m8_fp_double(a),y=m8_fp_double(b),result;
            switch(op) {
            case M8_FP_ADD:result=x+y;break;
            case M8_FP_SUB:result=x-y;break;
            case M8_FP_MUL:result=x*y;break;
            default:result=x/y;break;
            }
            return m8_fp_bits64(result);
        }
    }
    return m8_fp_op64(cpu,op,a,b,d);
}
M8_MEM_INLINE uint64_t m8_sampler_fp_from_int(m8_cpu *cpu,uint32_t a,
        int dp,int uns,unsigned bits,unsigned frac,int nearest) {
    if(m8_sampler_fp_mode(cpu) && (bits==16u || bits==32u) && frac<=16u) {
        if(bits==16u)a=uns?(uint16_t)a:(uint32_t)(int32_t)(int16_t)a;
        /* Every 32-bit integer and its power-of-two scale is exact in
         * binary64. A binary32 result can add only IXC in this range. */
        double wide=uns?(double)a:(double)(int32_t)a;
        if(frac)wide*=m8_fp_double((uint64_t)(1023u-frac)<<52);
        if(dp)return m8_fp_bits64(wide);
        float result=(float)wide;
        if(!(cpu->fpscr&16u) && (double)result!=wide)cpu->fpscr|=16u;
        return m8_fp_bits32(result);
    }
    return m8_fp_from_int(cpu,a,dp,uns,bits,frac,nearest);
}
M8_MEM_INLINE uint64_t m8_sampler_fp_convert(m8_cpu *cpu,uint64_t a,int from_dp) {
    if(m8_sampler_fp_mode(cpu)) {
        if(!from_dp && m8_fp_common32((uint32_t)a))
            return m8_fp_bits64((double)m8_fp_float((uint32_t)a));
        if(from_dp) {
            uint64_t magnitude=a&UINT64_C(0x7fffffffffffffff);
            /* This interior binary32 range excludes overflow and both
             * normal/subnormal rounding boundaries. Only IXC can change. */
            if(!magnitude || (magnitude>=UINT64_C(0x3870000000000000) &&
                              magnitude<=UINT64_C(0x4770000000000000))) {
                double wide=m8_fp_double(a);float result=(float)wide;
                if(!(cpu->fpscr&16u) && (double)result!=wide)cpu->fpscr|=16u;
                return m8_fp_bits32(result);
            }
        }
    }
    return m8_fp_convert(cpu,a,from_dp);
}
M8_MEM_INLINE uint32_t m8_sampler_fp_to_int(m8_cpu *cpu,uint64_t a,
        int dp,int uns,unsigned mode,unsigned bits,unsigned frac) {
    if(m8_sampler_fp_mode(cpu) && mode==3u &&
       (bits==16u || bits==32u) && frac<=16u &&
       (dp?m8_sampler_fp_normal64(a):m8_fp_common32((uint32_t)a))) {
        double value=dp?m8_fp_double(a):(double)m8_fp_float((uint32_t)a);
        /* Check before scaling and before the C cast. Scaling a value in
         * this range by at most 2^16 is exact and cannot overflow. */
        if(value>=-4294967296.0 && value<=4294967296.0) {
            if(frac)value*=m8_fp_double((uint64_t)(1023u+frac)<<52);
            double lower=uns?0.0:(bits==16u?-32768.0:-2147483648.0);
            double upper=uns?(bits==16u?65536.0:4294967296.0):
                             (bits==16u?32768.0:2147483648.0);
            if(value>=lower && value<upper) {
                uint32_t result=uns?(uint32_t)value:(uint32_t)(int32_t)value;
                double restored=uns?(double)result:(double)(int32_t)result;
                if(!(cpu->fpscr&16u) && restored!=value)cpu->fpscr|=16u;
                return result;
            }
        }
    }
    return m8_fp_to_int(cpu,a,dp,uns,mode,bits,frac);
}
M8_MEM_INLINE void m8_sampler_fp_cmp64(m8_cpu *cpu,uint64_t a,uint64_t b,int signaling) {
    if(m8_sampler_fp_mode(cpu) && m8_sampler_fp_normal64(a) && m8_sampler_fp_normal64(b)) {
        double x=m8_fp_double(a),y=m8_fp_double(b);
        uint32_t flags=x==y?0x60000000u:x<y?0x80000000u:0x20000000u;
        cpu->fpscr=(cpu->fpscr&0x0fffffffu)|flags;return;
    }
    m8_fp_cmp64(cpu,a,b,signaling);
}
#endif
