/* Keep the pinned SAMPLER memory proof unchanged. Select exact result-only
 * arithmetic for its existing raw FP sites only in the native no-sticky
 * profile. The alias applies to this header's body and cannot escape it. */
#ifndef M8_AOT_SAMPLER_VOICE_EXACT_H
#define M8_AOT_SAMPLER_VOICE_EXACT_H
#include "filter_kernel.h"
#ifdef M8_NATIVE_FP_NO_STICKY
M8_MEM_INLINE float m8_sampler_op32(m8_cpu *cpu,int op,float a,float b,float d) {
    return m8_fp_float(m8_fp_nosticky32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
#define m8_filter_op m8_sampler_op32
#endif
#include "sampler_voice.h"
#ifdef M8_NATIVE_FP_NO_STICKY
#undef m8_filter_op
#endif
#endif
