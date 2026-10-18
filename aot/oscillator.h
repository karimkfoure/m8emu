/* Bounded stationary oscillator loop, Headless 6.5.2C 0x20850..0x208c0.
 * Complete the remaining oversamples and resume at 0x208c2. The native FP
 * environment is already configured by the enclosing translated callback.
 */
#ifndef M8_AOT_OSCILLATOR_H
#define M8_AOT_OSCILLATOR_H
#include "runtime.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC)
static inline int m8_oscillator_disjoint(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn) {
    return a<=b ? b-a>=an : a-b>=bn;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_oscillator_kernel(m8_cpu *cpu,m8_mem *mem) {
    uint32_t total=cpu->r[2],done=cpu->r[8];
    if (!total || total>255u || done>=total) return 0;
    if (cpu->r[4]>UINT32_MAX-0xcau || cpu->r[11]>UINT32_MAX-1024u ||
        cpu->r[13]>UINT32_MAX-8u) return 0;
    uint8_t *object=m8_pointer(mem,cpu->r[4],0xcau);
    uint8_t *wave=m8_pointer(mem,cpu->r[11],1024u);
    uint8_t *temporary=m8_pointer(mem,cpu->r[13]+4u,4u);
    if (!object || !wave || !temporary || object[0xc9]) return 0;
    uintptr_t oh=(uintptr_t)object,wh=(uintptr_t)wave,th=(uintptr_t)temporary,ch=(uintptr_t)cpu;
    if (!m8_oscillator_disjoint(oh,0xcau,wh,1024u) ||
        !m8_oscillator_disjoint(oh,0xcau,th,4u) ||
        !m8_oscillator_disjoint(wh,1024u,th,4u) ||
        !m8_oscillator_disjoint(ch,sizeof(*cpu),oh,0xcau) ||
        !m8_oscillator_disjoint(ch,sizeof(*cpu),wh,1024u) ||
        !m8_oscillator_disjoint(ch,sizeof(*cpu),th,4u)) return 0;
    m8_cpu *restrict state=cpu;
    uint8_t *restrict fields=object;
    const uint8_t *restrict table=wave;
    uint8_t *restrict scratch=temporary;
    uint32_t phase=state->s[15],period=state->s[14],sum=state->s[16];
    uint32_t step=m8_le_read(fields+0xa4,4),value=0;
    uint32_t i=done;
    for (;i<total;i++) {
        uint32_t index=m8_fp_native_to_int(state,phase,0,1,3,32,0);
        m8_le_write(scratch,4,index);
        phase=m8_fp_native32(state,M8_FP_ADD,phase,step,0);
        m8_fp_cmp32(state,phase,period,1);
        value=m8_le_read(table+(index & 255u)*4u,4);
        m8_le_write(fields+0x70,4,phase);
        sum=m8_fp_native32(state,M8_FP_ADD,sum,value,0);
        /* Original BLT also takes the unordered comparison (N != V). */
        if (((state->fpscr >> 31) ^ (state->fpscr >> 28)) & 1u) continue;
        phase=m8_fp_native32(state,M8_FP_SUB,phase,period,0);
        m8_le_write(fields+0x70,4,phase);
    }
    state->s[13]=value;state->s[15]=phase;state->s[16]=sum;
    state->r[3]=total;state->r[8]=total;
    state->xpsr=(state->xpsr & 0x0fffffffu)|0x60000000u;
    return 1;
}
#endif
#endif
