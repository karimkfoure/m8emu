/* Whole Headless 6.5.2C effect block 0xbd28..0xc4e8, bypass mode.
 *
 * The caller (0xc4f0, effect type 0) invokes this block once per 64-sample
 * quantum. Every observed call on the 19 curated projects and the 5 kit
 * fixtures (97k+ block entries) takes the bypass path with count 64,
 * delay length 1000 and the per-sample biquad disabled: the firmware
 * still runs the full per-sample loop (input smoothing, phase update and
 * fold cascade, four delay-line interpolation sections, output mix) to
 * advance rings and smoothing state. That loop is ~4% of pooled release
 * playback and almost entirely translated; the existing kernel window
 * [0xbea8,0xbfb6) covers only the per-sample index computation.
 *
 * The engine runs the whole block natively with state in host locals,
 * exact float operation order and the same FP semantics as the FM/HYPER
 * fast engines (inline host arithmetic under the fixed RN/FZ/DN mode,
 * ARM default-NaN, cold exact helper at the minimum-normal boundary,
 * saturation-preserving conversions). Memory effects are identical and
 * in the same order. Admission guards decline to the translated path
 * for any other mode or layout:
 *   entry 0xbd28, count exactly 64, mix word [obj+0x1f90] exactly +0.0
 *   (bypass), per-sample-biquad byte [obj+0x2014] zero, delay length
 *   exactly 1000 with write index below 1000, RN/FZ/DN FP mode, no IT
 *   state, 4-byte-aligned object and sample buffers, all touched spans
 *   mapped with sample buffers disjoint from the object.
 * A negative result declines with CPU and RAM unchanged. M8_AOT_EXIT
 * publishes the exact original return continuation (or a safe resume PC
 * for the statically unreachable surprises; the guest stack frame is
 * replicated byte-for-byte, so translated execution resumed there
 * continues bit-exactly).
 */
#ifndef M8_AOT_EFFECT_FAST_H
#define M8_AOT_EFFECT_FAST_H
#include "fp.h"
#include "integer.h"
#include "local_flags.h"
#include "memory_spans.h"
#include "filter_kernel.h"
#include "canonical_memory.h"
#include "effect_fast_fp.h"
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
#include "compact_voices.h"
#endif
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && (defined(M8_AOT_FIXED_MEMORY) || (defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES))) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
int m8_effect_fast_kernel(m8_cpu *output,m8_mem *mem);
#if defined(M8_AOT_EFFECT_FAST_IMPLEMENTATION)
#if defined(__GNUC__) || defined(__clang__)
#define M8_EF_NOINLINE __attribute__((noinline))
#else
#define M8_EF_NOINLINE
#endif
/* Bypass smoothing literal at guest 0xbe04 (vldr s20,[pc,#0x2c] at
 * 0xbdd4), read dynamically each block. Expected value 0x3bbe37c6,
 * pinned against the firmware image by the differential test. */
#define M8_EF_S20_BITS 0x3bbe37c6u
#define M8_EF_COUNT 64u
#define M8_EF_RING_LEN 1000u
/* Fixed object layout used by the admitted path. ring0 at obj+0x34 and
 * ring1 at obj+0xfd4 hold 1000 words each; fields live at 0x1f74..0x2018. */
#define M8_EF_OBJ_SPAN 0x2020u
#define M8_EF_RING0_OFF 0x34u
#define M8_EF_RING1_OFF 0x0fd4u
static inline int m8_ef_disjoint(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn) {
    return m8_filter_disjoint(a,an,b,bn);
}
typedef struct {
    uint8_t *obj,*sb,*sq,*lit,*stk;
    uint32_t objAddr,sbAddr,sqAddr;
} m8_ef_ctx;
/* Qualify the whole block before any effect. Pure: no CPU/RAM change. */
static inline int m8_ef_qualify(const m8_cpu *cpu,m8_mem *mem,m8_ef_ctx *c) {
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uintptr_t arena=m8_compact_voice_memory_offset(mem);
#else
    uintptr_t arena=m8_canonical_memory_offset(mem);
#endif
    if(!arena) return 0;
    if(m8_current_it) return 0;
    if((cpu->fpscr&0x03c00000u)!=0x03000000u) return 0;
    if(cpu->xpsr&0x0600fc00u) return 0;
    if(cpu->r[15]!=0xbd28u) return 0;
    if(cpu->r[3]!=M8_EF_COUNT) return 0;
    uint32_t root=cpu->r[0],sb0=cpu->r[1],sq0=cpu->r[2];
    if((root&3u)||(sb0&3u)||(sq0&3u)) return 0;
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uint8_t *obj=m8_compact_voice_data_pointer(arena,root,M8_EF_OBJ_SPAN);
    uint8_t *sb=m8_compact_voice_data_pointer(arena,sb0,M8_EF_COUNT*4u);
    uint8_t *sq=m8_compact_voice_data_pointer(arena,sq0,M8_EF_COUNT*4u);
#else
    uint8_t *obj=m8_canonical_offset_pointer(arena,root,M8_EF_OBJ_SPAN);
    uint8_t *sb=m8_canonical_offset_pointer(arena,sb0,M8_EF_COUNT*4u);
    uint8_t *sq=m8_canonical_offset_pointer(arena,sq0,M8_EF_COUNT*4u);
#endif
    if(!obj||!sb||!sq) return 0;
    if(!m8_region_pointer(&mem->dtcm,root,M8_EF_OBJ_SPAN)&&
       !m8_region_pointer(&mem->ocram,root,M8_EF_OBJ_SPAN)) return 0;
    if(!m8_ef_disjoint((uintptr_t)obj,M8_EF_OBJ_SPAN,(uintptr_t)sb,M8_EF_COUNT*4u)) return 0;
    if(!m8_ef_disjoint((uintptr_t)obj,M8_EF_OBJ_SPAN,(uintptr_t)sq,M8_EF_COUNT*4u)) return 0;
    if(!m8_ef_disjoint((uintptr_t)sb,M8_EF_COUNT*4u,(uintptr_t)sq,M8_EF_COUNT*4u)) return 0;
    if(!m8_ef_disjoint((uintptr_t)mem->itcm.host,mem->itcm.size,(uintptr_t)cpu,sizeof(*cpu))||
       !m8_ef_disjoint((uintptr_t)mem->dtcm.host,mem->dtcm.size,(uintptr_t)cpu,sizeof(*cpu))||
       !m8_ef_disjoint((uintptr_t)mem->ocram.host,mem->ocram.size,(uintptr_t)cpu,sizeof(*cpu))||
       !m8_ef_disjoint((uintptr_t)mem->flash.host,mem->flash.size,(uintptr_t)cpu,sizeof(*cpu))) return 0;
    /* Bypass mode: mix word exactly +0.0. Any other value (including -0.0,
     * whose output signs differ) retains the translated path. */
    if(m8_le_read(obj+0x1f90u,4)!=0x00000000u) return 0;
    /* Per-sample biquad disabled; delay length exactly 1000. */
    if(obj[0x2014]!=0) return 0;
    uint32_t idx=m8_le_read(obj+0x1f74u,2);
    uint32_t ln=m8_le_read(obj+0x1f76u,2);
    if(ln!=M8_EF_RING_LEN||idx>=M8_EF_RING_LEN) return 0;
    /* Guest stack frame (push 36 + vpush 32 + sub 0x24) stays mapped so a
     * byte-for-byte, so it stays mapped and disjoint from all other spans. */
    uint32_t fsp=cpu->r[13]-104u;
    uint8_t *stk=m8_pointer(mem,fsp,104u);
    if(!stk) return 0;
    if(!m8_ef_disjoint((uintptr_t)stk,104u,(uintptr_t)obj,M8_EF_OBJ_SPAN)) return 0;
    if(!m8_ef_disjoint((uintptr_t)stk,104u,(uintptr_t)sb,M8_EF_COUNT*4u)) return 0;
    if(!m8_ef_disjoint((uintptr_t)stk,104u,(uintptr_t)sq,M8_EF_COUNT*4u)) return 0;
    /* Smoothing literal pool word. */
    uint8_t *lit=m8_pointer(mem,0xbe04u,4);
    if(!lit) return 0;
    if(!m8_ef_disjoint((uintptr_t)lit,4u,(uintptr_t)obj,M8_EF_OBJ_SPAN)) return 0;
    if(!m8_ef_disjoint((uintptr_t)lit,4u,(uintptr_t)sb,M8_EF_COUNT*4u)) return 0;
    if(!m8_ef_disjoint((uintptr_t)lit,4u,(uintptr_t)sq,M8_EF_COUNT*4u)) return 0;
    c->obj=obj;c->sb=sb;c->sq=sq;c->lit=lit;c->stk=stk;
    c->objAddr=root;c->sbAddr=sb0;c->sqAddr=sq0;
    return 1;
}
M8_EF_NOINLINE int m8_effect_fast_kernel(m8_cpu *output,m8_mem *mem) {
m8_cpu state,*cpu=&state;
m8_ef_ctx ctx;
if(!m8_ef_qualify(output,mem,&ctx)) return -1;
state=*output;
uint32_t er4=output->r[4],er5=output->r[5],er6=output->r[6],er7=output->r[7];
uint32_t er8=output->r[8],er9=output->r[9],er10=output->r[10],er11=output->r[11];
uint32_t erlr=output->r[14];
uint32_t es16=output->s[16],es17=output->s[17],es18=output->s[18],es19=output->s[19];
uint32_t es20=output->s[20],es21=output->s[21],es22=output->s[22],es23=output->s[23];
uint32_t r_0=output->r[0],r_1=output->r[1],r_2=output->r[2],r_3=output->r[3];
uint32_t r_4,r_5,r_6,r_7,r_8,r_9,r_10,r_11,r_12,r_13,r_14,r_15;
/* s0-s4 are never touched by the admitted path; they keep entry values. */
float s_5=m8_fp_float(output->s[5]);
float s_6=m8_fp_float(output->s[6]),s_7=m8_fp_float(output->s[7]);
float s_8=m8_fp_float(output->s[8]),s_9=m8_fp_float(output->s[9]);
float s_10=m8_fp_float(output->s[10]),s_11=m8_fp_float(output->s[11]);
float s_12=m8_fp_float(output->s[12]),s_13=m8_fp_float(output->s[13]);
float s_14=m8_fp_float(output->s[14]),s_15=m8_fp_float(output->s[15]);
float s_16=m8_fp_float(output->s[16]),s_17=m8_fp_float(output->s[17]);
float s_18=m8_fp_float(output->s[18]),s_19=m8_fp_float(output->s[19]);
float s_20=m8_fp_float(output->s[20]),s_21=m8_fp_float(output->s[21]);
float s_22=m8_fp_float(output->s[22]),s_23=m8_fp_float(output->s[23]);
float s_24=m8_fp_float(output->s[24]),s_25=m8_fp_float(output->s[25]);
float s_26=m8_fp_float(output->s[26]),s_27=m8_fp_float(output->s[27]);
float s_28=m8_fp_float(output->s[28]),s_29=m8_fp_float(output->s[29]);
float s_30=m8_fp_float(output->s[30]),s_31=m8_fp_float(output->s[31]);
uint32_t nz_n=output->xpsr>>31,nz_z=(output->xpsr>>30)&1u;
uint32_t nz_c=(output->xpsr>>29)&1u,nz_v=(output->xpsr>>28)&1u;
uint32_t xpsr_low=output->xpsr&0x0fffffffu;
uint32_t cmpw=0x60000000u;
uint8_t *obj=ctx.obj;
uint8_t *sbp=ctx.sb,*slp=ctx.sq;
uint8_t *stk=ctx.stk;
r_13=output->r[13];
uint32_t slot_c0,slot_sbend,slot_d0,slot_e0,slot_obj;
/* bd28: push {r4,r5,r6,r7,r8,sb,sl,fp,lr} */
m8_le_write(stk+68,4,er4);m8_le_write(stk+72,4,er5);m8_le_write(stk+76,4,er6);
m8_le_write(stk+80,4,er7);m8_le_write(stk+84,4,er8);m8_le_write(stk+88,4,er9);
m8_le_write(stk+92,4,er10);m8_le_write(stk+96,4,er11);m8_le_write(stk+100,4,erlr);
r_13-=36u;
/* bd4a: vpush {d8-d11} (s16-s23, entry values) */
m8_le_write(stk+36,4,es16);m8_le_write(stk+40,4,es17);m8_le_write(stk+44,4,es18);
m8_le_write(stk+48,4,es19);m8_le_write(stk+52,4,es20);m8_le_write(stk+56,4,es21);
m8_le_write(stk+60,4,es22);m8_le_write(stk+64,4,es23);
r_13-=32u;
/* bd4e: sub sp,#0x24 */
r_13-=36u;
/* bd2c: r5 = r0+0x1000 */
r_5=r_0+0x1000u;
/* bd30: r6 = r0+0x34 */
r_6=r_0+0x34u;
/* bd34: r7 = r0+0xfd4 */
r_7=r_0+0xfd4u;
/* bd38: s15 = 0.5 */
s_15=m8_fp_float(0x3f000000u);
/* bd3c: r4 = ldrh [obj+0x1f76] (1000 by admission) */
r_4=m8_le_read(obj+0x1f76u,2);
/* bd40: r11 = ldrh [obj+0x1f74] */
r_11=m8_le_read(obj+0x1f74u,2);
/* bd44: r12 = r4-2 */
r_12=r_4-2u;
/* bd48: r8 = r11 */
r_8=r_11;
/* bd50/bd5a: s18 = (float)(int32)r12 */
s_18=m8_fp_float(m8_ef_from_int(cpu,r_12,0));
/* bd54: [sp,0x14] = r0 */
slot_obj=r_0;
m8_le_write(stk+20,4,slot_obj);
slot_obj=r_0;
/* bd56/bd5e: s21 = [obj+0x1f88] */
s_21=m8_fp_float(m8_le_read(obj+0x1f88u,4));
/* bd62/bd66: s14 = [obj+0x1f9c] (depth) */
s_14=m8_fp_float(m8_le_read(obj+0x1f9cu,4));
/* bd6a: s21 = s21*0.5 */
s_21=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_21),0x3f000000u,0));
/* bd6e/bd72: vcmpe depth,0.5; bd76 it gt / bd78 vstrgt s21,[f9c] */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.5f?0x60000000u:(_x<0.5f?0x80000000u:0x20000000u));}
if(s_14>0.5f) m8_le_write(obj+0x1f9cu,4,m8_fp_bits32(s_21));
/* bd7c/bd80: s15 = [obj+0x1f90] (+0.0 by admission) */
s_15=m8_fp_float(m8_le_read(obj+0x1f90u,4));
/* bd84/bd88: vcmpe s15,#0; bd8c vcmp s15,#0; bd90 ite ge / bd92 movge r0,#1 / bd94 movlt r0,#0.
 * GE after VCMP is ordered >= (false for NaN); +0.0 admitted so r0=1. */
r_0=(s_15>=0.0f)?1u:0u;
/* bd96 vmrs; bd9a/bd9e: s19 = (float)(int32)r0 (=1.0) */
s_19=m8_fp_float(m8_ef_from_int(cpu,r_0,0));
/* bda2 beq 0xc420: taken for the admitted +0.0 (verified at qualify). */
/* c420 bypass: subs r0,r3,#1 (r3==64 admitted); c424 cmp r3,#0 (ne); c42a s22=s15 */
r_0=r_3-1u;
s_22=s_15;
/* bdce: sl=r10 = r2 */
r_10=r_2;
/* bdd0: r2 = slot_obj */
r_2=slot_obj;
/* bdd2: sb=r9 = r1 */
r_9=r_1;
/* bdd4: s20 = [pool 0xbe04] (read dynamically; pinned by the test) */
s_20=m8_fp_float(m8_le_read(ctx.lit,4));
/* bdd8/bddc: slot_c0 = obj+0x1fa0; s13 = [slot_c0] */
slot_c0=slot_obj+0x1fa0u;
s_13=m8_fp_float(m8_le_read(obj+0x1fa0u,4));
/* bde4: [sp,4] = slot_c0 */
m8_le_write(stk+4,4,slot_c0);
/* bde0: s18 = -s18 */
s_18=m8_fp_float(m8_fp_bits32(s_18)^0x80000000u);
/* bde6/bdec: slot_sbend = r1+4+r0*4 */
slot_sbend=r_1+4u+(r_0<<2);
/* bdf4: [sp,8] = slot_sbend */
m8_le_write(stk+8,4,slot_sbend);
/* bde8/bdf0: s16 = 0.5; s17 = 1.0 */
s_16=m8_fp_float(0x3f000000u);
s_17=m8_fp_float(0x3f800000u);
/* bdf6/bdfa: slot_d0 = obj+0x1f80 */
slot_d0=slot_obj+0x1f80u;
/* bdfa: [sp,0xc] = slot_d0 */
m8_le_write(stk+12,4,slot_d0);
/* bdfc/be00: slot_e0 = obj+0x2000 */
slot_e0=slot_obj+0x2000u;
/* be00: [sp,0x10] = slot_e0 */
m8_le_write(stk+16,4,slot_e0);
/* bdfc/be00: slot_e0 = obj+0x2000 */
slot_e0=slot_obj+0x2000u;
/* be00: [sp,0x10] = slot_e0 */
m8_le_write(stk+16,4,slot_e0);
for(unsigned ef_i=0;ef_i<M8_EF_COUNT;ef_i++) {
/* c224: r3 = r5+0xf94 */
r_3=r_5+0xf94u;
/* c228: r2 = slot_d0 */
r_2=slot_d0;
/* c22a: r1 = r5+0xf84 */
r_1=r_5+0xf84u;
/* c22e: s11 = [sb]; sb += 4 */
s_11=m8_fp_float(m8_le_read(sbp,4));sbp+=4;r_9+=4u;
/* c232: s15 = [f94] */
s_15=m8_fp_float(m8_le_read(obj+0x1f94u,4));
/* c236: s10 = [slot_d0] */
s_10=m8_fp_float(m8_le_read(obj+(slot_d0-slot_obj),4));
/* c23a: r2 = r5+0xf98 */
r_2=r_5+0xf98u;
/* c23e: s11 = s11*0.5 */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_11),0x3f000000u,0));
/* c242: s12 = [sl]; sl += 4 */
s_12=m8_fp_float(m8_le_read(slp,4));slp+=4;r_10+=4u;
/* c246: s10 = s10-s15 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_10),m8_fp_bits32(s_15),0));
/* c24a: s12 = s12*0.5 */
s_12=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_12),0x3f000000u,0));
/* c24e: s15 = fma(s10,s20,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),m8_fp_bits32(s_20),m8_fp_bits32(s_15)));
/* c252: s10 = [f84] */
s_10=m8_fp_float(m8_le_read(obj+0x1f84u,4));
/* c256: r1 = r5+0xf8c */
r_1=r_5+0xf8cu;
/* c25a: s14 = s15 */
s_14=s_15;
/* c25e: s15 = [f98] */
s_15=m8_fp_float(m8_le_read(obj+0x1f98u,4));
/* c262: s10 = s10-s15 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_10),m8_fp_bits32(s_15),0));
/* c266: [f94] = s14 */
m8_le_write(obj+0x1f94u,4,m8_fp_bits32(s_14));
/* c26a: s15 = fma(s10,s20,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),m8_fp_bits32(s_20),m8_fp_bits32(s_15)));
/* c26e: [f98] = s15 */
m8_le_write(obj+0x1f98u,4,m8_fp_bits32(s_15));
/* c272: r2 = r5+0xf9c */
r_2=r_5+0xf9cu;
/* c276: s15 = [f9c] */
s_15=m8_fp_float(m8_le_read(obj+0x1f9cu,4));
/* c27a: s14 = s21-s15 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_21),m8_fp_bits32(s_15),0));
/* c27e: s15 = fma(s14,s20,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_14),m8_fp_bits32(s_20),m8_fp_bits32(s_15)));
/* c282: s14 = [f8c] */
s_14=m8_fp_float(m8_le_read(obj+0x1f8cu,4));
/* c286: [f9c] = s15 */
m8_le_write(obj+0x1f9cu,4,m8_fp_bits32(s_15));
/* c28a: s15 = s14-s13 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),m8_fp_bits32(s_13),0));
/* c28e: r2 = slot_c0 */
r_2=slot_c0;
/* c290: s13 = fma(s15,s20,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_15),m8_fp_bits32(s_20),m8_fp_bits32(s_13)));
/* c294: [slot_c0] = s13 */
m8_le_write(obj+0x1fa0u,4,m8_fp_bits32(s_13));
/* c298/c29c: ring0[r8] = s11 */
m8_le_write(obj+M8_EF_RING0_OFF+(r_8<<2),4,m8_fp_bits32(s_11));
/* c2a0/c2a4: ring1[r8] = s12 */
m8_le_write(obj+M8_EF_RING1_OFF+(r_8<<2),4,m8_fp_bits32(s_12));
/* c2a8: s14 = [r3] (r3 still f94: reload of the stored s14) */
s_14=m8_fp_float(m8_le_read(obj+0x1f94u,4));
/* c2ac: r3 = r5+0xf78 */
r_3=r_5+0xf78u;
/* c2b0: s15 = [f78] (phase) */
s_15=m8_fp_float(m8_le_read(obj+0x1f78u,4));
/* c2b4: s15 = s15+s14 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_15),m8_fp_bits32(s_14),0));
/* c2b8/c2bc: vcmpe s15,s17(1.0); c2c0 bgt be08 */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.0f?0x60000000u:(_x<1.0f?0x80000000u:0x20000000u));}
if(s_15>1.0f) goto ef_be08;
/* c2c4/c2c8: vcmpe s15,#0 */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.0f?0x60000000u:(_x<0.0f?0x80000000u:0x20000000u));}
/* c2cc/c2ce: it mi; vaddmi s15,s15,s17 */
if(s_15<0.0f) s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_15),0x3f800000u,0));
goto ef_be0c;
goto ef_be0c;
ef_be08:
/* be08: s15 = s15-1.0 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),0x3f800000u,0));
ef_be0c:
/* be10: [f78] = s15 */
m8_le_write(obj+0x1f78u,4,m8_fp_bits32(s_15));
/* be18: s14 = [f7c] */
s_14=m8_fp_float(m8_le_read(obj+0x1f7cu,4));
/* be20: s13 = [f98] */
s_13=m8_fp_float(m8_le_read(obj+0x1f98u,4));
/* be24: s14 = s14+s13 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_14),m8_fp_bits32(s_13),0));
/* be28: vcmpe s14,1.0; be30 ble c3e0 */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.0f?0x60000000u:(_x<1.0f?0x80000000u:0x20000000u));}
if(!(s_14>1.0f)) goto ef_c3e0;
/* be34: s14 = s14-1.0 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),0x3f800000u,0));
ef_be38fallen: ;
/* be38: s13 = 1.5 */
s_13=m8_fp_float(0x3fc00000u);
/* be40: s12 = (float)(int32)r8 */
s_12=m8_fp_float(m8_ef_from_int(cpu,r_8,0));
/* be44: [f7c] = s14 */
m8_le_write(obj+0x1f7cu,4,m8_fp_bits32(s_14));
/* be48: vcmpe s15,1.5; be54 ble c3cc */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.5f?0x60000000u:(_x<1.5f?0x80000000u:0x20000000u));}
if(!(s_15>1.5f)) goto ef_c3cc;
/* be58: s10 = 2.0 */
s_10=m8_fp_float(0x40000000u);
/* be5c: s10 = s15-s10 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),m8_fp_bits32(s_10),0));
ef_be60fallen: ;
/* be60: s13 = 1.25 */
s_13=m8_fp_float(0x3fa00000u);
/* be64: vcmpe s14,1.25; be6c ble c3b2 */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.25f?0x60000000u:(_x<1.25f?0x80000000u:0x20000000u));}
if(!(s_14>1.25f)) goto ef_c3b2;
/* be70: s13 = s14-s13 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),m8_fp_bits32(s_13),0));
ef_be74fallen: ;
/* be74: s11 = 1.25 */
s_11=m8_fp_float(0x3fa00000u);
/* be78: s13 = s13-0.5 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_13),0x3f000000u,0));
/* be7c: vcmpe s15,1.25; be84 ble c398 */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.25f?0x60000000u:(_x<1.25f?0x80000000u:0x20000000u));}
if(!(s_15>1.25f)) goto ef_c398;
/* be88: s15 = s15-s11 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),m8_fp_bits32(s_11),0));
ef_be8cfallen: ;
/* be8c: s11 = 1.5 */
s_11=m8_fp_float(0x3fc00000u);
/* be90: s15 = s15-0.5 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),0x3f000000u,0));
/* be94: vcmpe s14,1.5; be9c ble c388 */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==1.5f?0x60000000u:(_x<1.5f?0x80000000u:0x20000000u));}
if(!(s_14>1.5f)) goto ef_c388;
/* bea0: s11 = 2.0 */
s_11=m8_fp_float(0x40000000u);
/* bea4: s14 = s14-s11 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),m8_fp_bits32(s_11),0));
goto ef_bea8;
ef_c3e0:
/* c3e0: vcmpe s14,#0; c3e8 it mi; c3ea vaddmi s14,s14,1.0; c3ee b be38 */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.0f?0x60000000u:(_x<0.0f?0x80000000u:0x20000000u));}
if(s_14<0.0f) s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_14),0x3f800000u,0));
goto ef_be38fallen;
ef_c3cc:
/* c3cc: vcmpe s15,0.5; c3d4 ite pl; c3d6 vsubpl s10,s15,1.0 / c3da vmov s10,s15 */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.5f?0x60000000u:(_x<0.5f?0x80000000u:0x20000000u));}
if(!(s_15<0.5f)) s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),0x3f800000u,0));
else s_10=s_15;
goto ef_be60fallen;
ef_c3b2:
/* c3b2: s13 = 0.25; c3b6 vcmpe s14,s13; c3be bpl be70 */
s_13=m8_fp_float(0x3e800000u);
{float _x=s_14,_y=m8_fp_float(0x3e800000u); uint32_t _un=(_x!=_x)|(_y!=_y);
 cmpw=_un?0x30000000u:(_x==_y?0x60000000u:(_x<_y?0x80000000u:0x20000000u));}
if(!(s_14<0.25f)) goto ef_be70;
/* c3c2: s13 = 0.75; c3c6: s13 = s14+0.75 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_14),0x3f400000u,0));
goto ef_be74fallen;
ef_c398:
/* c398: s11 = 0.25; c39c vcmpe s15,s11; c3a4 bpl be88 */
s_11=m8_fp_float(0x3e800000u);
{float _x=s_15,_y=m8_fp_float(0x3e800000u); uint32_t _un=(_x!=_x)|(_y!=_y);
 cmpw=_un?0x30000000u:(_x==_y?0x60000000u:(_x<_y?0x80000000u:0x20000000u));}
if(!(s_15<0.25f)) goto ef_be88;
/* c3a8: s11 = 0.75; c3ac: s15 = s15+0.75 */
s_11=m8_fp_float(0x3f400000u);
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_15),m8_fp_bits32(s_11),0));
goto ef_be8cfallen;
ef_c388:
/* c388: vcmpe s14,0.5; c390 it pl; c392 vsubpl s14,s14,1.0 */
{float _x=s_14; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.5f?0x60000000u:(_x<0.5f?0x80000000u:0x20000000u));}
if(!(s_14<0.5f)) s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),0x3f800000u,0));
goto ef_bea8;
ef_be88:
/* be88 (via bpl): s15 = s15-s11 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_15),m8_fp_bits32(s_11),0));
goto ef_be8cfallen;
ef_be70:
/* be70 (via bpl): s13 = s14-s13 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),m8_fp_bits32(s_13),0));
goto ef_be74fallen;
ef_bea8:
/* bea8: s11 = -1.0 */
s_11=m8_fp_float(0xbf800000u);
/* beb0: s9 = 4.0 */
s_9=m8_fp_float(0x40800000u);
/* beb4: s10 = |s10| */
s_10=m8_fp_float(m8_fp_bits32(s_10)&0x7fffffffu);
/* beb8: s6 = s11 */
s_6=s_11;
/* bebc: s13 = |s13| */
s_13=m8_fp_float(m8_fp_bits32(s_13)&0x7fffffffu);
/* bec0: s15 = |s15| */
s_15=m8_fp_float(m8_fp_bits32(s_15)&0x7fffffffu);
/* bec4: s6 = fma(s10,4.0,s6) */
s_6=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),0x40800000u,m8_fp_bits32(s_6)));
/* bec8: s7 = s11; becc: s10 = s11 */
s_7=s_11;s_10=s_11;
/* bed0: s14 = |s14| */
s_14=m8_fp_float(m8_fp_bits32(s_14)&0x7fffffffu);
/* bed4: s7 = fma(s13,4.0,s7) */
s_7=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_13),0x40800000u,m8_fp_bits32(s_7)));
/* bed8: s10 = fma(s15,4.0,s10) */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_15),0x40800000u,m8_fp_bits32(s_10)));
/* bedc: s15 = [f9c] (depth) */
s_15=m8_fp_float(m8_le_read(obj+0x1f9cu,4));
/* bee0: s11 = fma(s14,4.0,s11) */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_14),0x40800000u,m8_fp_bits32(s_11)));
/* bee4: s13 = s16; bee8: s9 = s16; beec: s8 = s16 */
s_13=s_16;s_9=s_16;s_8=s_16;
/* bef0: s13 = fma(s15,s7,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_15),m8_fp_bits32(s_7),m8_fp_bits32(s_13)));
/* bef4: s9 = fma(s15,s10,s9) */
s_9=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_15),m8_fp_bits32(s_10),m8_fp_bits32(s_9)));
/* bef8: s10 = s16 */
/* befc: s8 = fma(s6,s15,s8) */
s_10=s_16;
s_8=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_6),m8_fp_bits32(s_15),m8_fp_bits32(s_8)));
/* bf00: s14 = s12 */
s_14=s_12;
/* bf04: s10 = fma(s15,s11,s10) */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_15),m8_fp_bits32(s_11),m8_fp_bits32(s_10)));
/* bf08: s11 = s12 */
s_11=s_12;
/* bf0c: s11 = fma(s18,s13,s11) */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_18),m8_fp_bits32(s_13),m8_fp_bits32(s_11)));
/* bf10: s13 = s12 */
/* bf14: s14 = fma(s18,s8,s14) */
s_13=s_12;
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_18),m8_fp_bits32(s_8),m8_fp_bits32(s_14)));
/* bf18: s12 = fma(s18,s10,s12) */
s_12=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_18),m8_fp_bits32(s_10),m8_fp_bits32(s_12)));
/* bf1c: s13 = fma(s18,s9,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_18),m8_fp_bits32(s_9),m8_fp_bits32(s_13)));
/* bf20: s9 = s11-0.5; bf24: s8 = s14-0.5 */
s_9=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_11),0x3f000000u,0));
s_8=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),0x3f000000u,0));
/* bf28: s15 = s12-0.5; bf2c: s10 = s13-0.5 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_12),0x3f000000u,0));
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_13),0x3f000000u,0));
/* bf30/bf40/bf48: r3 = (int32)s8; s8 = (float)r3 */
{uint32_t _i=m8_ef_to_int(cpu,m8_fp_bits32(s_8),0);r_3=_i;s_8=m8_fp_float(m8_ef_from_int(cpu,_i,0));}
/* bf34/bf44/bf4c: r2 = (int32)s9; s9 = (float)r2 */
{uint32_t _i=m8_ef_to_int(cpu,m8_fp_bits32(s_9),0);r_2=_i;s_9=m8_fp_float(m8_ef_from_int(cpu,_i,0));}
/* bf38/bf50/bf58: r1 = (int32)s10; s10 = (float)r1 */
{uint32_t _i=m8_ef_to_int(cpu,m8_fp_bits32(s_10),0);r_1=_i;s_10=m8_fp_float(m8_ef_from_int(cpu,_i,0));}
/* bf3c/bf54/bf5c: r0 = (int32)s15; s15 = (float)r0 */
{uint32_t _i=m8_ef_to_int(cpu,m8_fp_bits32(s_15),0);r_0=_i;s_15=m8_fp_float(m8_ef_from_int(cpu,_i,0));}
/* bf60: subs ip,r3,#1 */
r_12=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,1u,0);
/* bf64: s14 = s14-s8 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),m8_fp_bits32(s_8),0));
/* bf68: s11 = s11-s9 */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_11),m8_fp_bits32(s_9),0));
/* bf6c: s13 = s13-s10 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_13),m8_fp_bits32(s_10),0));
/* bf70: s12 = s12-s15 */
s_12=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_12),m8_fp_bits32(s_15),0));
/* bf74: bmi c380 */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c380;
/* bf78: cmp ip,r4; bf7a it ge; bf7c subge ip,ip,r4 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_12,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_12=r_12-r_4;
/* bf80: uxth ip */
r_12&=0xffffu;
/* bf84: cmp r3,#0; bf86 blt c368 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c368;
/* bf8a: cmp r4,r3; bf8c bgt c3f0 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_3,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3f0;
/* bf90: fp = r3-r4 */
r_11=r_3-r_4;
/* bf94: uxth fp */
r_11&=0xffffu;
/* bf98: lr = r3+1 */
ef_bf98:
r_14=r_3+1u;
ef_bf9c_cmp:
/* bf9c: cmp r4,lr; bf9e it le; bfa0 suble lr,lr,r4 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_14,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,14)) r_14=r_14-r_4;
/* bfa4: uxth lr */
r_14&=0xffffu;
/* bfa8: adds r3,#2 */
ef_bfa8:
r_3=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_3,2u,0);
/* bfaa: bmi c362 */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c362;
/* bfae: cmp r3,r4; bfb0 it ge; bfb2 subsge r3,r3,r4 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_3=r_3-r_4;
/* bfb4: uxth r3 */
r_3&=0xffffu;
goto ef_bfb6;
ef_c380:
/* c380: ip = r4+ip; c382 uxth ip; c386 b bf84 */
r_12=r_4+r_12;
r_12&=0xffffu;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c368;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_3,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3f0;
r_11=r_3-r_4;
r_11&=0xffffu;
goto ef_bf98;
ef_c368:
/* c368: fp = r4+r3; c36c adds lr,r3,#1; c370 uxth fp; c374 beq bf9c */
r_11=r_4+r_3;
r_14=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_3,1u,0);
r_11&=0xffffu;
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_bf9c_cmp;
/* c378: lr = lr+r4; c37a uxth lr; c37e b bfa8 */
r_14=r_14+r_4;
r_14&=0xffffu;
goto ef_bfa8;
ef_c3f0:
/* c3f0: uxth fp,r3; c3f4 b bf98 */
r_11=r_3&0xffffu;
goto ef_bf98;
ef_c362:
/* c362: r3 = r4+r3; c364 uxth r3; c366 b bfb6 */
r_3=r_4+r_3;
r_3&=0xffffu;
goto ef_bfb6;
ef_bfb6:
/* indices for section 1 must stay in the admitted ring span */
if(r_11>=M8_EF_RING_LEN||r_14>=M8_EF_RING_LEN||r_12>=M8_EF_RING_LEN||r_3>=M8_EF_RING_LEN)
    goto ef_exit_bfb6;
/* bfb6: fp = r6+fp*4 (address) */
r_11=r_6+(r_11<<2);
/* bfba: s6 = s14-1.0 */
s_6=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_14),0x3f800000u,0));
/* bfbe: lr = r6+lr*4 */
r_14=r_6+(r_14<<2);
/* bfc2: s7 = [fp] */
s_7=m8_fp_float(m8_le_read(obj+(r_11-slot_obj),4));
/* bfc6: ip = r6+ip*4 */
r_12=r_6+(r_12<<2);
/* bfca: s9 = [lr] */
s_9=m8_fp_float(m8_le_read(obj+(r_14-slot_obj),4));
/* bfce: r3 = r6+r3*4 */
r_3=r_6+(r_3<<2);
/* bfd2: s15 = [ip] */
s_15=m8_fp_float(m8_le_read(obj+(r_12-slot_obj),4));
/* bfd6: s10 = s7-s9 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_7),m8_fp_bits32(s_9),0));
/* bfda: s8 = [r3] */
s_8=m8_fp_float(m8_le_read(obj+(r_3-slot_obj),4));
/* bfde: s9 = s9-s15 */
s_9=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_9),m8_fp_bits32(s_15),0));
/* bfe2: subs r3,r1,#1 */
r_3=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_1,1u,0);
/* bfe4: s8 = s8-s7 */
s_8=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_8),m8_fp_bits32(s_7),0));
/* bfe8: s15 = s10 */
s_15=s_10;
/* bfec: s15 = fma(s9,0.5,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_9),0x3f000000u,m8_fp_bits32(s_15)));
/* bff0: s10 = s10+s15 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_10),m8_fp_bits32(s_15),0));
/* bff4: s10 = fma(s8,0.5,s10) */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_8),0x3f000000u,m8_fp_bits32(s_10)));
/* bff8: s15 = fma(s6,s10,-s15) [vfnms] */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_6),m8_fp_bits32(s_10),m8_fp_bits32(s_15)^0x80000000u));
/* bffc: s15 = s14*s15 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_14),m8_fp_bits32(s_15),0));
/* c000: s15 = fma(s9,0.5,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_9),0x3f000000u,m8_fp_bits32(s_15)));
/* c004: s14 = s14*s15 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_14),m8_fp_bits32(s_15),0));
/* c008: bmi c35c (flags from bfe2 subs) */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c35c;
/* c00c: cmp r3,r4; c00e it ge; c010 subsge r3,r3,r4 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_3=r_3-r_4;
/* c012: uxth r3 */
r_3&=0xffffu;
/* c014: cmp r1,#0; c016 blt c344 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_1,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c344;
/* c01a: cmp r4,r1; c01c bgt c3fc */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_1,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3fc;
/* c020: lr = r1-r4 */
r_14=r_1-r_4;
/* c024: uxth lr */
r_14&=0xffffu;
/* c028: ip = r1+1 */
ef_c028:
r_12=r_1+1u;
ef_c02c_cmp:
/* c02c: cmp ip,r4; c02e it ge; c030 subge ip,ip,r4 (no flags) */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_12,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_12=r_12-r_4;
/* c034: uxth ip */
r_12&=0xffffu;
/* c038: adds r1,#2 */
ef_c038:
r_1=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_1,2u,0);
/* c03a: bmi c33e */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c33e;
/* c03e: cmp r1,r4; c040 it ge; c042 subsge r1,r1,r4 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_1,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_1=r_1-r_4;
/* c044: uxth r1 */
r_1&=0xffffu;
goto ef_c046;
ef_c35c:
/* c35c: r3 = r4+r3; c35e uxth r3; c360 b c014 */
r_3=r_4+r_3;
r_3&=0xffffu;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_1,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c344;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_1,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3fc;
r_14=r_1-r_4;
r_14&=0xffffu;
goto ef_c028;
ef_c344:
/* c344: lr = r4+r1; c348 adds ip,r1,#1; c34c uxth lr; c350 beq c02c */
r_14=r_4+r_1;
r_12=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_1,1u,0);
r_14&=0xffffu;
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_c02c_cmp;
/* c354: ip = ip+r4; c356 uxth ip; c35a b c038 */
r_12=r_12+r_4;
r_12&=0xffffu;
goto ef_c038;
ef_c3fc:
/* c3fc: uxth lr,r1; c400 b c028 */
r_14=r_1&0xffffu;
goto ef_c028;
ef_c33e:
/* c33e: r1 = r4+r1; c340 uxth r1; c342 b c046 */
r_1=r_4+r_1;
r_1&=0xffffu;
goto ef_c046;
ef_c046:
if(r_14>=M8_EF_RING_LEN||r_12>=M8_EF_RING_LEN||r_3>=M8_EF_RING_LEN||r_1>=M8_EF_RING_LEN)
    goto ef_exit_c046;
/* c046: lr = r7+lr*4 */
r_14=r_7+(r_14<<2);
/* c04a: s5 = s13-s17 */
s_5=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_13),m8_fp_bits32(s_17),0));
/* c04e: ip = r7+ip*4 */
r_12=r_7+(r_12<<2);
/* c052: s9 = [lr] */
s_9=m8_fp_float(m8_le_read(obj+(r_14-slot_obj),4));
/* c056: r3 = r7+r3*4 */
r_3=r_7+(r_3<<2);
/* c05a: s8 = [ip] */
s_8=m8_fp_float(m8_le_read(obj+(r_12-slot_obj),4));
/* c05e: r1 = r7+r1*4 */
r_1=r_7+(r_1<<2);
/* c062: s15 = [r3] */
s_15=m8_fp_float(m8_le_read(obj+(r_3-slot_obj),4));
/* c066: subs r3,r2,#1 */
r_3=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_2,1u,0);
/* c068: s10 = s9-s8 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_9),m8_fp_bits32(s_8),0));
/* c06c: s6 = [r1] */
s_6=m8_fp_float(m8_le_read(obj+(r_1-slot_obj),4));
/* c070: s8 = s8-s15 */
s_8=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_8),m8_fp_bits32(s_15),0));
/* c074: s6 = s6-s9 */
s_6=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_6),m8_fp_bits32(s_9),0));
/* c078: s15 = s10 */
s_15=s_10;
/* c07c: s15 = fma(s8,0.5,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_8),0x3f000000u,m8_fp_bits32(s_15)));
/* c080: s10 = s10+s15 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_10),m8_fp_bits32(s_15),0));
/* c084: s10 = fma(s6,0.5,s10) */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_6),0x3f000000u,m8_fp_bits32(s_10)));
/* c088: s15 = fma(s5,s10,-s15) [vfnms] */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_5),m8_fp_bits32(s_10),m8_fp_bits32(s_15)^0x80000000u));
/* c08c: s15 = s13*s15 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_13),m8_fp_bits32(s_15),0));
/* c090: s15 = fma(s8,0.5,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_8),0x3f000000u,m8_fp_bits32(s_15)));
/* c094: s15 = s13*s15 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_13),m8_fp_bits32(s_15),0));
/* c098: bmi c338 (flags from c066) */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c338;
/* c09c: cmp r3,r4; ge: r3-=r4 (plain); c0a2 uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_3=r_3-r_4;
r_3&=0xffffu;
/* c0a4: cmp r2,#0; c0a6 blt c324 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_2,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c324;
/* c0aa: cmp r4,r2; c0ac bgt c3f6 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_2,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3f6;
/* c0b0: ip = r2-r4; c0b4 uxth ip */
r_12=r_2-r_4;
r_12&=0xffffu;
/* c0b8: adds r1,r2,#1 */
ef_c0b8:
r_1=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_2,1u,0);
ef_c0ba_cmp:
/* c0ba: cmp r4,r1; c0bc it le; c0be suble (plain); c0c0 uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_1,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,14)) r_1=r_1-r_4;
r_1&=0xffffu;
/* c0c2: adds r2,#2 */
ef_c0c2:
r_2=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_2,2u,0);
/* c0c4: bmi c31e */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c31e;
/* c0c8: cmp r2,r4; c0ca it ge; c0cc subge (plain); c0ce uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_2,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_2=r_2-r_4;
r_2&=0xffffu;
goto ef_c0d0;
ef_c338:
/* c338: r3 = r4+r3; c33a uxth; c33c b c0a4 */
r_3=r_4+r_3;
r_3&=0xffffu;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_2,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c324;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_2,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c3f6;
r_12=r_2-r_4;
r_12&=0xffffu;
goto ef_c0b8;
ef_c324:
/* c324: ip = r4+r2; c328 adds r1,r2,#1; c32a uxth ip; c32e beq c0ba */
r_12=r_4+r_2;
r_1=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_2,1u,0);
r_12&=0xffffu;
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_c0ba_cmp;
/* c332: r1 = r1+r4; c334 uxth; c336 b c0c2 */
r_1=r_1+r_4;
r_1&=0xffffu;
goto ef_c0c2;
ef_c3f6:
/* c3f6: uxth ip,r2; c3fa b c0b8 */
r_12=r_2&0xffffu;
goto ef_c0b8;
ef_c31e:
/* c31e: r2 = r4+r2; c320 uxth; c322 b c0d0 */
r_2=r_4+r_2;
r_2&=0xffffu;
goto ef_c0d0;
ef_c0d0:
if(r_12>=M8_EF_RING_LEN||r_1>=M8_EF_RING_LEN||r_3>=M8_EF_RING_LEN||r_2>=M8_EF_RING_LEN)
    goto ef_exit_c0d0;
/* c0d0: ip = r6+ip*4 */
r_12=r_6+(r_12<<2);
/* c0d4: s14 = s7+s14 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_7),m8_fp_bits32(s_14),0));
/* c0d8: r1 = r6+r1*4 */
r_1=r_6+(r_1<<2);
/* c0dc: s6 = [ip] */
s_6=m8_fp_float(m8_le_read(obj+(r_12-slot_obj),4));
/* c0e0: r3 = r6+r3*4 */
r_3=r_6+(r_3<<2);
/* c0e4: s8 = [r1] */
s_8=m8_fp_float(m8_le_read(obj+(r_1-slot_obj),4));
/* c0e8: r2 = r6+r2*4 */
r_2=r_6+(r_2<<2);
/* c0ec: s5 = [r3] */
s_5=m8_fp_float(m8_le_read(obj+(r_3-slot_obj),4));
/* c0f0: s14 = s14+s6 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_14),m8_fp_bits32(s_6),0));
/* c0f4: s10 = s6-s8 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_6),m8_fp_bits32(s_8),0));
/* c0f8: s13 = [r2] */
s_13=m8_fp_float(m8_le_read(obj+(r_2-slot_obj),4));
/* c0fc: s8 = s8-s5 */
s_8=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_8),m8_fp_bits32(s_5),0));
/* c100: subs r3,r0,#1 */
r_3=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,1u,0);
/* c102: s7 = s13-s6 */
s_7=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_13),m8_fp_bits32(s_6),0));
/* c106: s13 = s10 */
s_13=s_10;
/* c10a: s5 = s11-s17 */
s_5=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_11),m8_fp_bits32(s_17),0));
/* c10e: s13 = fma(s8,0.5,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_8),0x3f000000u,m8_fp_bits32(s_13)));
/* c112: s10 = s10+s13 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_10),m8_fp_bits32(s_13),0));
/* c116: s10 = fma(s7,0.5,s10) */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_7),0x3f000000u,m8_fp_bits32(s_10)));
/* c11a: s13 = fma(s10,s5,-s13) [vfnms] */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),m8_fp_bits32(s_5),m8_fp_bits32(s_13)^0x80000000u));
/* c11e: s13 = s11*s13 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_11),m8_fp_bits32(s_13),0));
/* c122: s13 = fma(s8,0.5,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_8),0x3f000000u,m8_fp_bits32(s_13)));
/* c126: s14 = fma(s11,s13,s14) */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_11),m8_fp_bits32(s_13),m8_fp_bits32(s_14)));
/* c12a: bmi c318 (flags from c100) */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c318;
/* c12e: cmp r3,r4; ge: sub plain; uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_3=r_3-r_4;
r_3&=0xffffu;
/* c136: cmp r0,#0; c138 blt c308 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c308;
/* c13c: cmp r4,r0; c13e bgt c402 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_0,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c402;
/* c142: subs r1,r0,r4 (flags) */
r_1=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,r_4,0);
/* c144: uxth r1 */
r_1&=0xffffu;
/* c146: adds r2,r0,#1 (flags) */
ef_c146:
r_2=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_0,1u,0);
ef_c148_cmp:
/* c148: cmp r4,r2; c14a it le; c14c suble (plain); c14e uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_2,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,14)) r_2=r_2-r_4;
r_2&=0xffffu;
/* c150: adds r0,#2 (flags) */
ef_c150:
r_0=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_0,2u,0);
/* c152: bmi c302 */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,5)) goto ef_c302;
/* c156: cmp r0,r4; c158 it ge; c15a subsge (plain); c15c uxth */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,r_4,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,11)) r_0=r_0-r_4;
r_0&=0xffffu;
goto ef_c15e;
ef_c318:
/* c318: r3 = r4+r3; c31a uxth; c31c b c136 */
r_3=r_4+r_3;
r_3&=0xffffu;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,0u,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,12)) goto ef_c308;
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_0,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,13)) goto ef_c402;
r_1=m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_0,r_4,0);
r_1&=0xffffu;
goto ef_c146;
ef_c308:
/* c308: adds r1,r4,r0; c30a adds r2,r0,#1; c30c uxth r1; c30e beq c148 */
r_1=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_0,0);
r_2=m8_flags_add(&nz_n,&nz_z,&nz_c,&nz_v,r_0,1u,0);
r_1&=0xffffu;
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_c148_cmp;
/* c312: r2 = r2+r4; c314 uxth; c316 b c150 */
r_2=r_2+r_4;
r_2&=0xffffu;
goto ef_c150;
ef_c402:
/* c402: uxth r1,r0; c404 b c146 */
r_1=r_0&0xffffu;
goto ef_c146;
ef_c302:
/* c302: r0 = r4+r0; c304 uxth; c306 b c15e */
r_0=r_4+r_0;
r_0&=0xffffu;
goto ef_c15e;
ef_c15e:
if(r_1>=M8_EF_RING_LEN||r_2>=M8_EF_RING_LEN||r_3>=M8_EF_RING_LEN||r_0>=M8_EF_RING_LEN)
    goto ef_exit_c15e;
/* c15e: r1 = r7+r1*4 */
r_1=r_7+(r_1<<2);
/* c162: s15 = s9+s15 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_9),m8_fp_bits32(s_15),0));
/* c166: r2 = r7+r2*4 */
r_2=r_7+(r_2<<2);
/* c16a: s6 = s12-s17 */
s_6=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_12),m8_fp_bits32(s_17),0));
/* c16e: s8 = [r1] */
s_8=m8_fp_float(m8_le_read(obj+(r_1-slot_obj),4));
/* c172: r3 = r7+r3*4 */
r_3=r_7+(r_3<<2);
/* c176: s10 = [r2] */
s_10=m8_fp_float(m8_le_read(obj+(r_2-slot_obj),4));
/* c17a: r0 = r7+r0*4 */
r_0=r_7+(r_0<<2);
/* c17e: s13 = [r3] */
s_13=m8_fp_float(m8_le_read(obj+(r_3-slot_obj),4));
/* c182: s15 = s15+s8 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_15),m8_fp_bits32(s_8),0));
/* c186: s11 = s8-s10 */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_8),m8_fp_bits32(s_10),0));
/* c18a: s9 = [r0] */
s_9=m8_fp_float(m8_le_read(obj+(r_0-slot_obj),4));
/* c18e: s10 = s10-s13 */
s_10=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_10),m8_fp_bits32(s_13),0));
/* c192: r3 = slot_e0 */
r_3=slot_e0;
/* c194: s9 = s9-s8 */
s_9=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,m8_fp_bits32(s_9),m8_fp_bits32(s_8),0));
/* c198: r8 = r8+1 (no flags) */
r_8=r_8+1u;
/* c19c: s13 = s11 */
s_13=s_11;
/* c1a0: r3 = ldrb [slot_e0+0x14] (byte2014, zero by admission) */
r_3=obj[0x2014];
/* c1a2: s19 = s19+s22 */
s_19=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_19),m8_fp_bits32(s_22),0));
/* c1a6: cmp r4,r8; c1a8 it le; c1aa movle r8,#0 (plain) */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_4,r_8,0);
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,14)) r_8=0u;
/* c1ae: s7 = -1.0 */
s_7=m8_fp_float(0xbf800000u);
/* c1b2: cmp r3,#1 */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_3,1u,0);
/* c1b4: s13 = fma(s10,0.5,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),0x3f000000u,m8_fp_bits32(s_13)));
/* c1b8: s14 = maxnm(s14,s7) */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_14),m8_fp_bits32(s_7),0));
/* c1bc: s14 = minnm(s14,s17) */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_14),m8_fp_bits32(s_17),0));
/* c1c0: s14 = s14*s19 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_14),m8_fp_bits32(s_19),0));
/* c1c4: s11 = s13+s11 */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_13),m8_fp_bits32(s_11),0));
/* c1c8: [sp,0x18] = s14 */
m8_le_write(stk+24,4,m8_fp_bits32(s_14));
/* c1cc: s11 = fma(s9,0.5,s11) */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_9),0x3f000000u,m8_fp_bits32(s_11)));
/* c1d0: s13 = fma(s11,s6,-s13) [vfnms] */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_11),m8_fp_bits32(s_6),m8_fp_bits32(s_13)^0x80000000u));
/* c1d4: s13 = s12*s13 */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_12),m8_fp_bits32(s_13),0));
/* c1d8: s13 = fma(s10,0.5,s13) */
s_13=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_10),0x3f000000u,m8_fp_bits32(s_13)));
/* c1dc: s15 = fma(s12,s13,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_12),m8_fp_bits32(s_13),m8_fp_bits32(s_15)));
/* c1e0: s15 = maxnm(s15,s7) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_15),m8_fp_bits32(s_7),0));
/* c1e4: s15 = minnm(s15,s17) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_15),m8_fp_bits32(s_17),0));
/* c1e8: s15 = s15*s19 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_15),m8_fp_bits32(s_19),0));
/* c1ec: [sp,0x1c] = s15 */
m8_le_write(stk+28,4,m8_fp_bits32(s_15));
/* c1f0: beq c406 (byte2014==1: defensive safe-exit; admitted byte is 0) */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_exit_c1f0;
/* c1f4: r3 = slot_c0 */
r_3=slot_c0;
/* c1f6: s11 = s14+s15 */
s_11=m8_fp_float(m8_ef_op(cpu,M8_FP_ADD,m8_fp_bits32(s_14),m8_fp_bits32(s_15),0));
/* c1fa: s13 = [slot_c0] */
s_13=m8_fp_float(m8_le_read(obj+0x1fa0u,4));
/* c1fe: r3 = slot_sbend */
r_3=slot_sbend;
/* c200: s12 = 1.0-s13 */
s_12=m8_fp_float(m8_ef_op(cpu,M8_FP_SUB,0x3f800000u,m8_fp_bits32(s_13),0));
/* c204: s14 = s13*s14 */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_13),m8_fp_bits32(s_14),0));
/* c208: cmp sb,r3(sbend) */
m8_flags_sub(&nz_n,&nz_z,&nz_c,&nz_v,r_9,r_3,0);
/* c20a: s15 = s13*s15 */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_13),m8_fp_bits32(s_15),0));
/* c20e: s12 = s12*0.5 */
s_12=m8_fp_float(m8_ef_op(cpu,M8_FP_MUL,m8_fp_bits32(s_12),0x3f000000u,0));
/* c212: s14 = fma(s11,s12,s14) */
s_14=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_11),m8_fp_bits32(s_12),m8_fp_bits32(s_14)));
/* c216: s15 = fma(s11,s12,s15) */
s_15=m8_fp_float(m8_ef_op(cpu,M8_FP_FMA,m8_fp_bits32(s_11),m8_fp_bits32(s_12),m8_fp_bits32(s_15)));
/* c21a: [sb-4] = s14 */
m8_le_write(sbp-4,4,m8_fp_bits32(s_14));
/* c21e: [sl-4] = s15 */
m8_le_write(slp-4,4,m8_fp_bits32(s_15));
/* c222: beq c2d4 (sb==sbend: block done) */
if(m8_flags_condition(nz_n,nz_z,nz_c,nz_v,1)) goto ef_c2d4;
}
/* 64 iterations done but sb missed the end: resume translated at 0xc222. */
goto ef_exit_c222;
ef_c2d4:
/* c2d4: r3 = r5+0xf90 */
r_3=r_5+0xf90u;
/* c2d8: fp = r8 (uxth no-op) */
r_11=r_8&0xffffu;
/* c2dc: s15 = [f90] (+0.0: never written by the loop) */
s_15=m8_fp_float(m8_le_read(obj+0x1f90u,4));
/* c2e0/c2e4: vcmpe s15,#0 */
{float _x=s_15; uint32_t _un=(_x!=_x);
 cmpw=_un?0x30000000u:(_x==0.0f?0x60000000u:(_x<0.0f?0x80000000u:0x20000000u));}
/* c2e8: bgt c430 (s15>0: defensive safe-exit; statically unreachable) */
if(s_15>0.0f) goto ef_exit_c2e0;
/* c2ec: r3 = f90addr */
r_3=r_5+0xf90u;
/* c2f0: movs r2,#0 (N=0,Z=1,C/V kept) */
m8_flags_logic(&nz_n,&nz_z,&nz_c,r_2=0u,nz_c);
/* c2f2: [f90] = 0 */
m8_le_write(obj+0x1f90u,4,0u);
/* c2f4: strh [obj+0x1f74] = r8 */
m8_le_write(obj+0x1f74u,2,r_11);
/* c2f8: add sp,#0x24 */
r_13+=36u;
/* c2fa: vpop {d8-d11} (restores entry s16-s23) */
s_16=m8_fp_float(m8_le_read(stk+36,4));s_17=m8_fp_float(m8_le_read(stk+40,4));
s_18=m8_fp_float(m8_le_read(stk+44,4));s_19=m8_fp_float(m8_le_read(stk+48,4));
s_20=m8_fp_float(m8_le_read(stk+52,4));s_21=m8_fp_float(m8_le_read(stk+56,4));
s_22=m8_fp_float(m8_le_read(stk+60,4));s_23=m8_fp_float(m8_le_read(stk+64,4));
r_13+=32u;
/* c2fe: pop {r4,r5,r6,r7,r8,sb,sl,fp,pc} */
r_4=m8_le_read(stk+68,4);r_5=m8_le_read(stk+72,4);r_6=m8_le_read(stk+76,4);
r_7=m8_le_read(stk+80,4);r_8=m8_le_read(stk+84,4);r_9=m8_le_read(stk+88,4);
r_10=m8_le_read(stk+92,4);r_11=m8_le_read(stk+96,4);
{uint32_t ef_lr=m8_le_read(stk+100,4);r_15=ef_lr&~1u;}
r_13+=36u;
output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
output->r[4]=er4;output->r[5]=er5;output->r[6]=er6;output->r[7]=er7;
output->r[8]=er8;output->r[9]=er9;output->r[10]=er10;output->r[11]=er11;
output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=r_15;
output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
output->s[16]=es16;output->s[17]=es17;output->s[18]=es18;output->s[19]=es19;
output->s[20]=es20;output->s[21]=es21;output->s[22]=es22;output->s[23]=es23;
output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
return M8_AOT_EXIT;
ef_exit_bfb6:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xbfb6u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c046:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc046u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c0d0:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc0d0u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c15e:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc15eu;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c1f0:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc1f0u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c222:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc222u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}
ef_exit_c2e0:
{ output->r[0]=r_0;output->r[1]=r_1;output->r[2]=r_2;output->r[3]=r_3;
 output->r[4]=r_4;output->r[5]=r_5;output->r[6]=r_6;output->r[7]=r_7;
 output->r[8]=r_8;output->r[9]=r_9;output->r[10]=r_10;output->r[11]=r_11;
 output->r[12]=r_12;output->r[13]=r_13;output->r[14]=r_14;output->r[15]=0xc2e0u;
 output->s[5]=m8_fp_bits32(s_5);output->s[6]=m8_fp_bits32(s_6);output->s[7]=m8_fp_bits32(s_7);
 output->s[8]=m8_fp_bits32(s_8);output->s[9]=m8_fp_bits32(s_9);output->s[10]=m8_fp_bits32(s_10);
 output->s[11]=m8_fp_bits32(s_11);output->s[12]=m8_fp_bits32(s_12);output->s[13]=m8_fp_bits32(s_13);
 output->s[14]=m8_fp_bits32(s_14);output->s[15]=m8_fp_bits32(s_15);
 output->s[16]=m8_fp_bits32(s_16);output->s[17]=m8_fp_bits32(s_17);
 output->s[18]=m8_fp_bits32(s_18);output->s[19]=m8_fp_bits32(s_19);
 output->s[20]=m8_fp_bits32(s_20);output->s[21]=m8_fp_bits32(s_21);
 output->s[22]=m8_fp_bits32(s_22);output->s[23]=m8_fp_bits32(s_23);
 output->s[24]=m8_fp_bits32(s_24);output->s[25]=m8_fp_bits32(s_25);
 output->s[26]=m8_fp_bits32(s_26);output->s[27]=m8_fp_bits32(s_27);
 output->s[28]=m8_fp_bits32(s_28);output->s[29]=m8_fp_bits32(s_29);
 output->s[30]=m8_fp_bits32(s_30);output->s[31]=m8_fp_bits32(s_31);
 output->xpsr=xpsr_low|(nz_n<<31)|(nz_z<<30)|(nz_c<<29)|(nz_v<<28);
 output->fpscr=(state.fpscr&0x0fffffffu)|cmpw;
 return M8_AOT_EXIT;}

}

#endif
#else
static inline int m8_effect_fast_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return -1;
}
#endif
#endif
