/* Default-off Headless 6.5.2C WAVSYNTH voice prototype.
 * Original render instructions retain their order and exact fallback. Qualify
 * disjoint RAM once per block and again after each original control callback.
 * Unknown methods or changed assumptions decline before effects or publish
 * the actual continuation after completed effects. No guest work is replayed. */
#ifndef M8_AOT_OSCILLATOR_VOICE_H
#define M8_AOT_OSCILLATOR_VOICE_H
#if (defined(M8_AOT_OSCILLATOR_VOICE) || defined(M8_AOT_WASM_OSCILLATOR_VOICE)) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && (defined(M8_AOT_FIXED_MEMORY) || defined(M8_AOT_COMPACT_MEMORY)) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#include "fp.h"
#if defined(M8_AOT_FIXED_MEMORY)
#include "canonical_memory.h"
#else
#include "compact_voices.h"
#endif
#include "integer.h"
#if defined(M8_WEB_NATIVE_FP)
#include <fp_fast_wasm.h>
#endif
#if defined(__GNUC__) || defined(__clang__)
#define M8_OV_NOINLINE __attribute__((noinline))
#else
#define M8_OV_NOINLINE
#endif
static inline int m8_oscillator_voice_disjoint(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn) {
    return a<=b ? b-a>=an : a-b>=bn;
}
static inline int m8_oscillator_voice_regions(const void *cpu,uint32_t size,const m8_mem *mem) {
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for (unsigned n=0;n<4;n++)
        if (!m8_oscillator_voice_disjoint((uintptr_t)cpu,size,
                (uintptr_t)regions[n]->host,regions[n]->size)) return 0;
    return 1;
}
M8_MEM_INLINE uintptr_t m8_oscillator_voice_memory_offset(const m8_mem *mem) {
#if defined(M8_AOT_FIXED_MEMORY)
    return m8_canonical_memory_offset(mem);
#else
    return m8_compact_voice_memory_offset(mem);
#endif
}
M8_MEM_INLINE uint8_t *m8_oscillator_voice_pointer(uintptr_t arena,uint32_t address,uint32_t size,int data) {
#if defined(M8_AOT_FIXED_MEMORY)
    if (data && (size>0x80000u ||
        ((uint32_t)(address-0x20000000u)>0x80000u-size &&
         (uint32_t)(address-0x20200000u)>0x80000u-size))) return NULL;
    return m8_canonical_offset_pointer(arena,address,size);
#else
    return data ? m8_compact_voice_data_pointer(arena,address,size) :
        m8_compact_voice_pointer(arena,address,size);
#endif
}
static M8_OV_NOINLINE int m8_oscillator_voice_qualify(const m8_cpu *cpu,m8_mem *mem,
        uint32_t root,uint32_t left,uint32_t right,uint32_t count,uint32_t stack,uint32_t wave) {
    uintptr_t arena=m8_oscillator_voice_memory_offset(mem);
    if (!arena || !m8_oscillator_voice_regions(cpu,sizeof(*cpu),mem) ||
        !m8_oscillator_voice_regions(mem,sizeof(*mem),mem) || m8_current_it ||
        !m8_fp_native_supported(cpu) || (cpu->xpsr & 0x0600fc00u) ||
        !count || count>64u) return 0;
    uint8_t *object=m8_oscillator_voice_pointer(arena,root,0xcau,1);
    if (!object || !m8_le_read(object+0x64,4)) return 0;
    uint32_t address[]={root,stack,m8_le_read(object+0x64,4),wave,
        m8_le_read(object+0x68,4),left,right,0x20990u,0x2070u,
        m8_le_read(object+0x58,4),m8_le_read(object+0x50,4),0};
    uint32_t size[]={0xcau,96,124,1024,16,count*4u,count*4u,68,12,640,4,4};
    uint8_t *pointer[12];
    for (unsigned n=0;n<12;n++) {
        /* Filtered paths can reach the ring store at 0x2092a even for
         * methods <=3 (0x20c02 branches directly to 0x208fa). Only the
         * indirect method slots are conditional on the method byte. */
        if (n>=10u && object[0x41]<=3u) break;
        if (n==11u) address[n]=m8_le_read(pointer[10],4);
        pointer[n]=m8_oscillator_voice_pointer(arena,address[n],size[n],
            n!=7u && n!=8u && n!=11u);
        if (!pointer[n] || (address[n] & 3u)) return 0;
        for (unsigned prior=0;prior<n;prior++)
            if (!m8_oscillator_voice_disjoint((uintptr_t)pointer[n],size[n],
                    (uintptr_t)pointer[prior],size[prior])) return 0;
    }
    /* These two literals prove a ring index in 0..319 for every F32 input:
     * finite negative values saturate to zero; unordered and >=320 reset. */
    return m8_le_read(pointer[7]+16,4)==0x43a00000u &&
        m8_le_read(pointer[7]+20,4)==0 &&
        (object[0x41]<=3u || m8_le_read(pointer[11],4)==0x1fa5u ||
            m8_le_read(pointer[11],4)==0x1fedu);
}
M8_MEM_INLINE uint8_t *m8_oscillator_voice_host(uintptr_t arena,uint32_t address) {
#if defined(M8_AOT_FIXED_MEMORY)
    return (uint8_t *)(arena+(uintptr_t)address);
#else
    return address>=0x60000000u ?
        (uint8_t *)(arena+M8_COMPACT_FLASH_OFFSET+(address-0x60000000u)) :
        m8_compact_voice_host(arena,address);
#endif
}
#if defined(M8_WEB_NATIVE_FP)
/* The enclosing qualifier proves RN/FZ/DN. Sticky IXC and the bounded
 * exponent grids prove that direct operations add no other status flag.
 * Complete software helpers retain rare values and every new exception. */
M8_MEM_INLINE uint32_t m8_oscillator_voice_op32(m8_cpu *cpu,int op,uint32_t a,uint32_t b,uint32_t d) {
    uint32_t am=a & 0x7fffffffu,bm=b & 0x7fffffffu,result;
    if ((cpu->fpscr & 16u) && (op==M8_FP_ADD || op==M8_FP_SUB) &&
        (!am || am-0x10000000u<=0x5fffffffu) &&
        (!bm || bm-0x10000000u<=0x5fffffffu)) {
        float x=m8_fp_float(a),y=m8_fp_float(b);
        return m8_fp_bits32(op==M8_FP_ADD ? x+y : x-y);
    }
    if (m8_web_fp_sticky32(cpu,op,a,b,d,&result)) return result;
    return m8_fp_eval32(cpu,op,a,b,d);
}
M8_MEM_INLINE void m8_oscillator_voice_cmp32(m8_cpu *cpu,uint32_t a,uint32_t b,int signaling) {
    if (m8_fp_common32(a) && m8_fp_common32(b)) {
        float x=m8_fp_float(a),y=m8_fp_float(b);
        cpu->fpscr=(cpu->fpscr & 0x0fffffffu)|
            (x==y ? 0x60000000u : x<y ? 0x80000000u : 0x20000000u);
    } else m8_fp_cmp32(cpu,a,b,signaling);
}
M8_MEM_INLINE uint32_t m8_oscillator_voice_to_int(m8_cpu *cpu,uint64_t a,int dp,int uns,
        unsigned mode,unsigned bits,unsigned frac) {
    if (!dp && mode==3u && bits==32u && !frac && m8_fp_common32((uint32_t)a)) {
        float x=m8_fp_float((uint32_t)a);
        if (uns && x>=0.0f && x<4294967296.0f) {
            uint32_t result=(uint32_t)x;
            if ((float)result!=x) cpu->fpscr|=16u;
            return result;
        }
        if (!uns && x>=-2147483648.0f && x<2147483648.0f) {
            int32_t result=(int32_t)x;
            if ((float)result!=x) cpu->fpscr|=16u;
            return (uint32_t)result;
        }
    }
    return m8_fp_to_int(cpu,a,dp,uns,mode,bits,frac);
}
M8_MEM_INLINE uint64_t m8_oscillator_voice_from_int(m8_cpu *cpu,uint32_t a,int dp,int uns,
        unsigned bits,unsigned frac,int nearest) {
    if (!dp && bits==32u && frac<=32u) {
        float result=uns ? (float)a : (float)(int32_t)a;
        double exact=uns ? (double)a : (double)(int32_t)a;
        if ((double)result!=exact) cpu->fpscr|=16u;
        return m8_fp_bits32(result*m8_fp_float((127u-frac)<<23));
    }
    return m8_fp_from_int(cpu,a,dp,uns,bits,frac,nearest);
}
#else
/* Native qualification changes only the voice/memory boundary. Retain the
 * existing host arithmetic API, including its collected hardware flags. */
#define m8_oscillator_voice_op32 m8_fp_eval32
#define m8_oscillator_voice_cmp32 m8_fp_cmp32
#define m8_oscillator_voice_to_int m8_fp_to_int
#define m8_oscillator_voice_from_int m8_fp_from_int
#endif
#include "oscillator_voice_body.h"
#if !defined(M8_WEB_NATIVE_FP)
#undef m8_oscillator_voice_op32
#undef m8_oscillator_voice_cmp32
#undef m8_oscillator_voice_to_int
#undef m8_oscillator_voice_from_int
#endif
#undef M8_OV_NOINLINE
#endif
#endif
