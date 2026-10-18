/* Whole Headless6.5.2C HYPERSYNTH filter-1/filter-2 block,168e8..1773a.
 * The caller supplies the unchanged native15da4 and configures RN/FZ/DN
 * once. Scope: filter1..2,shape0..3,alternate mixer0,2..64 samples;
 * the scalar continuation owns every other entry or a changed control layout.
 * Qualify all effects before the first PUSH. Control updates retain their
 * original CPU/RAM/fault boundary; changed assumptions exit at175d4.
 * A negative result declines unchanged. Nonnegative results publish the
 * exact original return/child/safe continuation CPU and preserve all stores.
 * The shaper must be the clamp at0x1fa5, the fold limiter at0x2299, or the
 * folder at0x1fbd, with stable descriptors. Every entry CPU word is carried; every exit publishes
 * all50 CPU words. Twelve BLEP lanes retain original FP/lane/store order.
 * No guest data cache.
 */
#ifndef M8_AOT_HYPER_FAST_H
#define M8_AOT_HYPER_FAST_H
#include "filter_kernel.h"
#include "integer.h"
#include "canonical_memory.h"
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
#include "compact_voices.h"
#endif
#include "hyper_fast_fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && (defined(M8_AOT_FIXED_MEMORY) || (defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES))) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
int m8_hyper_fast_kernel(m8_cpu *output,m8_mem *mem,m8_aot_fn update);
#if defined(M8_AOT_HYPER_FAST_IMPLEMENTATION)
/* Inline host arithmetic with the fixed no-sticky FP mode. Bit-exact with
 * the translated body: NaN results become the ARM default NaN and results
 * at the minimum-normal boundary use the cold exact helper. */
static inline __attribute__((always_inline)) float m8_hvf_arithmetic(
        m8_cpu *cpu,int op,float a,float b,float d) {
    return m8_fp_float(m8_hvf_op(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
/* A fixed arena supplies non-overlapping guest regions. Reject all overlap
 * with CPU/private state and all overlap among render owners, including the
 * callback table and mutable literal words. This retains a scalar fallback
 * for safe unusual aliases, instead of changing their load/store ordering. */
static inline int m8_hvf_regions(const m8_cpu *cpu,const m8_mem *mem) {
    return m8_filter_disjoint((uintptr_t)mem->itcm.host,mem->itcm.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->dtcm.host,mem->dtcm.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->ocram.host,mem->ocram.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->flash.host,mem->flash.size,(uintptr_t)cpu,sizeof(*cpu));
}
/* Cached render spans. Pointer fields are re-read by the body only where
 * the firmware reads them; every cached span is disjoint from all write
 * spans, and the control update requalifies before further effects. */
typedef struct {
    uint8_t *object,*frame,*control,*steps,*scratchA,*scratchB,*phases,*phaseDst,
            *wave,*ring,*left,*right,*table,*vtable,*lit16998,*lit17470;
    uint32_t controlAddr,leftAddr,rightAddr;
} m8_hvf_ctx;
static inline int m8_hvf_qualify(const m8_cpu *cpu,m8_mem *mem,m8_hvf_ctx *c,
    uint32_t root,uint32_t left,uint32_t right,uint32_t count,int initial) {
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uintptr_t arena=m8_compact_voice_memory_offset(mem);
#else
    uintptr_t arena=m8_canonical_memory_offset(mem);
#endif
    if(!arena || !m8_hvf_regions(cpu,mem) || m8_current_it ||
       (cpu->fpscr&0x03c00000u)!=0x03000000u || (cpu->xpsr&0x0600fc00u) ||
       !count || count>64u || (initial && cpu->r[15]!=0x168e8u)) return 0;
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uint8_t *object=m8_compact_voice_data_pointer(arena,root,256u);
#else
    uint8_t *object=m8_canonical_offset_pointer(arena,root,256u);
#endif
    /* Real songs use filter1/filter2 with shape0..3;
     * fixtures use filter0. Other filters, shapes and the alternate mixer
     * retain the scalar or bounded-kernel path. */
    if(!object || (uint32_t)(root-0x20000000u)>0x80000u-256u ||
       object[0x40]<1 || object[0x40]>2 || object[0x41]>3 ||
       object[0xe0]!=0 || object[0x54]>7) return 0;
    uint32_t address[]={root,cpu->r[13]-(initial?80u:0u),
        m8_le_read(object+0x58,4),m8_le_read(object+0xcc,4),
        m8_le_read(object+0xd0,4),m8_le_read(object+0xd4,4),
        m8_le_read(object+0xec,4),m8_le_read(object+0xf0,4),
        m8_le_read(object+0xf4,4),m8_le_read(object+0xe8,4),
        left,right,m8_le_read(object+0x50,4),0,0x16998u,0x17470u};
    uint32_t size[]={256,80,124,48,48,48,48,52,32,640,count*4u,count*4u,4,4,16,24};
    uint8_t *pointer[16];
    for(unsigned i=0;i<16;i++) {
        if(i==13) address[i]=m8_le_read(pointer[12],4);
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
        pointer[i]=((i==14u && address[i]==0x16998u && size[i]==16u) ||
                    (i==15u && address[i]==0x17470u && size[i]==24u)) ?
            m8_compact_voice_pointer(arena,address[i],size[i]) :
            m8_compact_voice_data_pointer(arena,address[i],size[i]);
#else
        pointer[i]=m8_canonical_offset_pointer(arena,address[i],size[i]);
#endif
        if(!pointer[i] || (address[i]&3u)) return 0;
        if(i<12 && !m8_region_pointer(&mem->dtcm,address[i],size[i]) &&
                   !m8_region_pointer(&mem->ocram,address[i],size[i])) return 0;
        for(unsigned j=0;j<i;j++)
            if(!m8_filter_disjoint((uintptr_t)pointer[i],size[i],(uintptr_t)pointer[j],size[j])) return 0;
    }
    /* Shape0 voices use the inlined clamp at0x1fa5; shape2/shape3 voices use
     * the inlined fold limiter at0x2299 or the inlined folder at0x1fbd. */
    uint32_t target=m8_le_read(pointer[13],4);
    if(target!=0x1fa5u && target!=0x2299u && target!=0x1fbdu) return 0;
    if(m8_le_read(pointer[15],4)!=0x43a00000u) return 0;
    c->object=pointer[0];c->frame=pointer[1];c->control=pointer[2];
    c->steps=pointer[3];c->scratchA=pointer[4];c->scratchB=pointer[5];
    c->phases=pointer[6];c->phaseDst=pointer[7];c->wave=pointer[8];
    c->ring=pointer[9];c->left=pointer[10];c->right=pointer[11];
    c->table=pointer[12];c->vtable=pointer[13];
    c->lit16998=pointer[14];c->lit17470=pointer[15];
    c->controlAddr=address[2];
    c->leftAddr=left;c->rightAddr=right;
    return 1;
}
#if defined(__GNUC__) || defined(__clang__)
#define M8_HVF_NOINLINE __attribute__((noinline))
#else
#define M8_HVF_NOINLINE
#endif
#define M8_HVF_SAVE(dst) do {(dst)->r[0]=r_0; (dst)->r[1]=r_1; (dst)->r[2]=r_2; (dst)->r[3]=r_3; (dst)->r[4]=r_4; (dst)->r[5]=r_5; (dst)->r[6]=r_6; (dst)->r[7]=r_7; (dst)->r[8]=r_8; (dst)->r[9]=r_9; (dst)->r[10]=r_10; (dst)->r[11]=r_11; (dst)->r[12]=r_12; (dst)->r[13]=r_13; (dst)->r[14]=r_14; (dst)->r[15]=r_15; (dst)->s[0]=m8_fp_bits32(s_0); (dst)->s[1]=m8_fp_bits32(s_1); (dst)->s[2]=m8_fp_bits32(s_2); (dst)->s[3]=m8_fp_bits32(s_3); (dst)->s[4]=m8_fp_bits32(s_4); (dst)->s[5]=m8_fp_bits32(s_5); (dst)->s[6]=m8_fp_bits32(s_6); (dst)->s[7]=m8_fp_bits32(s_7); (dst)->s[8]=m8_fp_bits32(s_8); (dst)->s[9]=m8_fp_bits32(s_9); (dst)->s[10]=m8_fp_bits32(s_10); (dst)->s[11]=m8_fp_bits32(s_11); (dst)->s[12]=m8_fp_bits32(s_12); (dst)->s[13]=m8_fp_bits32(s_13); (dst)->s[14]=m8_fp_bits32(s_14); (dst)->s[15]=m8_fp_bits32(s_15); (dst)->s[16]=m8_fp_bits32(s_16); (dst)->s[17]=m8_fp_bits32(s_17); (dst)->s[18]=m8_fp_bits32(s_18); (dst)->s[19]=m8_fp_bits32(s_19); (dst)->s[20]=m8_fp_bits32(s_20); (dst)->s[21]=m8_fp_bits32(s_21); (dst)->s[22]=m8_fp_bits32(s_22); (dst)->s[23]=m8_fp_bits32(s_23); (dst)->s[24]=m8_fp_bits32(s_24); (dst)->s[25]=m8_fp_bits32(s_25); (dst)->s[26]=m8_fp_bits32(s_26); (dst)->s[27]=m8_fp_bits32(s_27); (dst)->s[28]=m8_fp_bits32(s_28); (dst)->s[29]=m8_fp_bits32(s_29); (dst)->s[30]=m8_fp_bits32(s_30); (dst)->s[31]=m8_fp_bits32(s_31); (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr;} while(0)
#define M8_HVF_LOAD(src) do {r_0=(src)->r[0]; r_1=(src)->r[1]; r_2=(src)->r[2]; r_3=(src)->r[3]; r_4=(src)->r[4]; r_5=(src)->r[5]; r_6=(src)->r[6]; r_7=(src)->r[7]; r_8=(src)->r[8]; r_9=(src)->r[9]; r_10=(src)->r[10]; r_11=(src)->r[11]; r_12=(src)->r[12]; r_13=(src)->r[13]; r_14=(src)->r[14]; r_15=(src)->r[15]; s_0=m8_fp_float((src)->s[0]); s_1=m8_fp_float((src)->s[1]); s_2=m8_fp_float((src)->s[2]); s_3=m8_fp_float((src)->s[3]); s_4=m8_fp_float((src)->s[4]); s_5=m8_fp_float((src)->s[5]); s_6=m8_fp_float((src)->s[6]); s_7=m8_fp_float((src)->s[7]); s_8=m8_fp_float((src)->s[8]); s_9=m8_fp_float((src)->s[9]); s_10=m8_fp_float((src)->s[10]); s_11=m8_fp_float((src)->s[11]); s_12=m8_fp_float((src)->s[12]); s_13=m8_fp_float((src)->s[13]); s_14=m8_fp_float((src)->s[14]); s_15=m8_fp_float((src)->s[15]); s_16=m8_fp_float((src)->s[16]); s_17=m8_fp_float((src)->s[17]); s_18=m8_fp_float((src)->s[18]); s_19=m8_fp_float((src)->s[19]); s_20=m8_fp_float((src)->s[20]); s_21=m8_fp_float((src)->s[21]); s_22=m8_fp_float((src)->s[22]); s_23=m8_fp_float((src)->s[23]); s_24=m8_fp_float((src)->s[24]); s_25=m8_fp_float((src)->s[25]); s_26=m8_fp_float((src)->s[26]); s_27=m8_fp_float((src)->s[27]); s_28=m8_fp_float((src)->s[28]); s_29=m8_fp_float((src)->s[29]); s_30=m8_fp_float((src)->s[30]); s_31=m8_fp_float((src)->s[31]); cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr;} while(0)
M8_HVF_NOINLINE int m8_hyper_fast_kernel(m8_cpu *output,m8_mem *mem,m8_aot_fn update) {
m8_cpu state,*cpu=&state;
m8_hvf_ctx ctx;
if(output->r[3]<=1u || !update ||
   !m8_hvf_qualify(output,mem,&ctx,output->r[0],output->r[1],output->r[2],output->r[3],1)) return -1;
uint32_t root=output->r[0],frame=output->r[13]-80u;
state=*output;
uint32_t r_0=output->r[0];
uint32_t r_1=output->r[1];
uint32_t r_2=output->r[2];
uint32_t r_3=output->r[3];
uint32_t r_4=output->r[4];
uint32_t r_5=output->r[5];
uint32_t r_6=output->r[6];
uint32_t r_7=output->r[7];
uint32_t r_8=output->r[8];
uint32_t r_9=output->r[9];
uint32_t r_10=output->r[10];
uint32_t r_11=output->r[11];
uint32_t r_12=output->r[12];
uint32_t r_13=output->r[13];
uint32_t r_14=output->r[14];
uint32_t r_15=output->r[15];
float s_0=m8_fp_float(output->s[0]);
float s_1=m8_fp_float(output->s[1]);
float s_2=m8_fp_float(output->s[2]);
float s_3=m8_fp_float(output->s[3]);
float s_4=m8_fp_float(output->s[4]);
float s_5=m8_fp_float(output->s[5]);
float s_6=m8_fp_float(output->s[6]);
float s_7=m8_fp_float(output->s[7]);
float s_8=m8_fp_float(output->s[8]);
float s_9=m8_fp_float(output->s[9]);
float s_10=m8_fp_float(output->s[10]);
float s_11=m8_fp_float(output->s[11]);
float s_12=m8_fp_float(output->s[12]);
float s_13=m8_fp_float(output->s[13]);
float s_14=m8_fp_float(output->s[14]);
float s_15=m8_fp_float(output->s[15]);
float s_16=m8_fp_float(output->s[16]);
float s_17=m8_fp_float(output->s[17]);
float s_18=m8_fp_float(output->s[18]);
float s_19=m8_fp_float(output->s[19]);
float s_20=m8_fp_float(output->s[20]);
float s_21=m8_fp_float(output->s[21]);
float s_22=m8_fp_float(output->s[22]);
float s_23=m8_fp_float(output->s[23]);
float s_24=m8_fp_float(output->s[24]);
float s_25=m8_fp_float(output->s[25]);
float s_26=m8_fp_float(output->s[26]);
float s_27=m8_fp_float(output->s[27]);
float s_28=m8_fp_float(output->s[28]);
float s_29=m8_fp_float(output->s[29]);
float s_30=m8_fp_float(output->s[30]);
float s_31=m8_fp_float(output->s[31]);
uint8_t *object=ctx.object,*control=ctx.control,*framePtr=ctx.frame;
uint8_t *lit16998=ctx.lit16998,*lit17470=ctx.lit17470;
/* 168e8: push.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, lr} */
{r_13-=40u;
 m8_le_write(framePtr+40u,4,r_3);m8_le_write(framePtr+44u,4,r_4);
 m8_le_write(framePtr+48u,4,r_5);m8_le_write(framePtr+52u,4,r_6);
 m8_le_write(framePtr+56u,4,r_7);m8_le_write(framePtr+60u,4,r_8);
 m8_le_write(framePtr+64u,4,r_9);m8_le_write(framePtr+68u,4,r_10);
 m8_le_write(framePtr+72u,4,r_11);m8_le_write(framePtr+76u,4,r_14);}
/* 168ec: mov r8, r0 */ {r_8=r_0;}
/* 168ee: ldr r0, [r0, #0x58] */ {r_0=m8_le_read(object+0x58,4);}
/* 168f0: vpush {d8, d9, d10, d11, d12} */
{m8_le_write(framePtr+0u,4,m8_fp_bits32(s_16));m8_le_write(framePtr+4u,4,m8_fp_bits32(s_17));
 m8_le_write(framePtr+8u,4,m8_fp_bits32(s_18));m8_le_write(framePtr+12u,4,m8_fp_bits32(s_19));
 m8_le_write(framePtr+16u,4,m8_fp_bits32(s_20));m8_le_write(framePtr+20u,4,m8_fp_bits32(s_21));
 m8_le_write(framePtr+24u,4,m8_fp_bits32(s_22));m8_le_write(framePtr+28u,4,m8_fp_bits32(s_23));
 m8_le_write(framePtr+32u,4,m8_fp_bits32(s_24));m8_le_write(framePtr+36u,4,m8_fp_bits32(s_25));
 r_13-=40u;}
/* 168f4: cmp r0, #0 */ {m8_sub_flags(cpu,r_0,0x0u,0u);}
/* 168f6: beq.w #0x17732 */ {if(m8_condition(cpu->xpsr,1)) {goto L_17732;}}
/* 168fa: mov sb, r1 */ {r_9=r_1;}
/* 168fc: mov fp, r2 */ {r_11=r_2;}
/* 168fe: ldr.w r0, [r8, #0xf4] */ {r_0=m8_le_read(object+0xf4,4);}
/* 16902: ldrd r1, r2, [r8, #0xec] */
{r_1=m8_le_read(object+0xec,4);r_2=m8_le_read(object+0xf0,4);}
/* 16906..16930: copy 11 words [r1]->[r2] */
{uint8_t *src=ctx.phases,*dst=ctx.phaseDst;
 for(unsigned i=0;i<11;i++) m8_le_write(dst+4*i,4,m8_le_read(src+4*i,4));}
/* 16932: ldr r1, [r1, #0x2c] */ {r_1=m8_le_read(ctx.phases+44u,4);}
/* 16934: str r1, [r2, #0x2c] */ {m8_le_write(ctx.phaseDst+44u,4,r_1);}
/* 16936: ldr.w r1, [r8, #0x9c] */ {r_1=m8_le_read(object+0x9c,4);}
/* 1693a: str r1, [r2, #0x30] */ {m8_le_write(ctx.phaseDst+48u,4,r_1);}
/* 1693c: ldr.w r2, [r8, #0x44] */ {r_2=m8_le_read(object+0x44,4);}
/* 16940: ldr.w r5, [r8, #0xa8] */ {r_5=m8_le_read(object+0xa8,4);}
/* 16944: str.w r2, [r8, #0x4c] */ {m8_le_write(object+0x4c,4,r_2);}
/* 16948: ldr.w r4, [r8, #0xac] */ {r_4=m8_le_read(object+0xac,4);}
/* 1694c: ldr.w r1, [r8, #0xb0] */ {r_1=m8_le_read(object+0xb0,4);}
/* 16950: ldr.w r2, [r8, #0xb4] */ {r_2=m8_le_read(object+0xb4,4);}
/* 16954: str r5, [r0] */ {m8_le_write(ctx.wave+0u,4,r_5);}
/* 16956: str r4, [r0, #4] */ {m8_le_write(ctx.wave+4u,4,r_4);}
/* 16958: str r1, [r0, #8] */ {m8_le_write(ctx.wave+8u,4,r_1);}
/* 1695a: str r2, [r0, #0xc] */ {m8_le_write(ctx.wave+12u,4,r_2);}
/* 1695c: ldr.w r5, [r8, #0xb8] */ {r_5=m8_le_read(object+0xb8,4);}
/* 16960: ldr.w r4, [r8, #0xbc] */ {r_4=m8_le_read(object+0xbc,4);}
/* 16964: ldr.w r1, [r8, #0xc0] */ {r_1=m8_le_read(object+0xc0,4);}
/* 16968: ldr.w r2, [r8, #0xc4] */ {r_2=m8_le_read(object+0xc4,4);}
/* 1696c: str r5, [r0, #0x10] */ {m8_le_write(ctx.wave+16u,4,r_5);}
/* 1696e: str r4, [r0, #0x14] */ {m8_le_write(ctx.wave+20u,4,r_4);}
/* 16970: str r1, [r0, #0x18] */ {m8_le_write(ctx.wave+24u,4,r_1);}
/* 16972: str r2, [r0, #0x1c] */ {m8_le_write(ctx.wave+28u,4,r_2);}
/* 16974: cmp r3, #0 */ {m8_sub_flags(cpu,r_3,0x0u,0u);}
/* 16976: beq.w #0x17730 */ {if(m8_condition(cpu->xpsr,1)) {goto L_17730;}}
/* 1697a: vmov.f32 s17, #1.000000e+00 */ {s_17=m8_fp_float(0x3f800000u);}
/* 1697e: add.w sl, fp, r3, lsl #2 */ {r_10=r_11+m8_shift(r_3,2,2,NULL);}
/* 16982: vmov.f32 s22, #5.000000e-01 */ {s_22=m8_fp_float(0x3f000000u);}
/* 16986: vldr s23, [pc, #0x18] */ {s_23=m8_fp_float(m8_le_read(lit16998+8u,4));}
/* 1698a: vldr s21, [pc, #0x18] */ {s_21=m8_fp_float(m8_le_read(lit16998+12u,4));}
/* 1698e: vldr d9, [pc, #8] */ {s_18=m8_fp_float(m8_le_read(lit16998+0u,4));s_19=m8_fp_float(m8_le_read(lit16998+4u,4));}
/* 16992: b.w #0x1732c */ {goto L_1732c;}
/* 1732c: per-sample phase dispatch. Phase 0 runs the control update;
 * phases 1..6 advance the counter; phase 7 wraps to 0. */
L_1732c:{uint32_t phase=object[0x54];
 r_3=phase;
 {m8_sub_flags(cpu,r_3,0x0u,0u);}
 if(m8_condition(cpu->xpsr,1)) {goto L_175ce;}}
L_17336:{s_15=m8_fp_float(r_3);
 {m8_sub_flags(cpu,r_3,0x6u,0u);}
 if(cpu->fpscr&(3u<<22)) m8_fp_sync(cpu);
 s_15=m8_fp_float(m8_fp_from_int(cpu,(uint32_t)m8_fp_bits32(s_15),0,1,32,3,1));
 s_13=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_17,s_15,0);
 if(m8_condition(cpu->xpsr,10)) {
/* 169a8: adds r3, #1 */ {r_3=m8_add_flags(cpu,r_3,0x1u,0u);}
/* 169aa: uxtb r3, r3 */ {r_3&=255u;}
  goto L_169ac;
 }
/* 17348: movs r3, #0 */ {m8_logic_flags(cpu,0u,(cpu->xpsr>>29)&1u);r_3=0u;}
 goto L_169ac;}
L_169ac: ;
/* 169ac: ldr.w r6, [r8, #0x58] */ {r_6=ctx.controlAddr;}
/* 169b0: strb.w r3, [r8, #0x54] */ {m8_le_write(object+0x54,1,r_3);}
/* 169b4: vldr s14, [r6, #0x28] */ {s_14=m8_fp_float(m8_le_read(control+40u,4));}
/* 169b8: vldr s0, [r6, #0x78] */ {s_0=m8_fp_float(m8_le_read(control+120u,4));}
/* 169bc: vmul.f32 s14, s15, s14 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_14,0);}
/* 169c0: vldr s5, [r6, #0x18] */ {s_5=m8_fp_float(m8_le_read(control+24u,4));}
/* 169c4: vldr s6, [r6, #4] */ {s_6=m8_fp_float(m8_le_read(control+4u,4));}
/* 169c8: vldr s7, [r6, #0x1c] */ {s_7=m8_fp_float(m8_le_read(control+28u,4));}
/* 169cc: vfma.f32 s14, s13, s0 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_0,s_14);}
/* 169d0: vldr s8, [r6, #0x20] */ {s_8=m8_fp_float(m8_le_read(control+32u,4));}
/* 169d4: vldr s9, [r6, #0x24] */ {s_9=m8_fp_float(m8_le_read(control+36u,4));}
/* 169d8: vmul.f32 s6, s15, s6 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_6,0);}
/* 169dc: vldr s10, [r6, #0xc] */ {s_10=m8_fp_float(m8_le_read(control+12u,4));}
/* 169e0: vmul.f32 s7, s15, s7 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_7,0);}
/* 169e4: vldr s11, [r6, #0x10] */ {s_11=m8_fp_float(m8_le_read(control+16u,4));}
/* 169e8: vmul.f32 s8, s15, s8 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_8,0);}
/* 169ec: vldr s12, [r6, #0x14] */ {s_12=m8_fp_float(m8_le_read(control+20u,4));}
/* 169f0: vmul.f32 s9, s15, s9 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_9,0);}
/* 169f4: vmul.f32 s10, s15, s10 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_10,0);}
/* 169f8: vldr s24, [r6, #0x54] */ {s_24=m8_fp_float(m8_le_read(control+84u,4));}
/* 169fc: vmul.f32 s11, s15, s11 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_11,0);}
/* 16a00: vldr s20, [r6, #0x6c] */ {s_20=m8_fp_float(m8_le_read(control+108u,4));}
/* 16a04: vmul.f32 s12, s15, s12 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_12,0);}
/* 16a08: vldr s16, [r6, #0x70] */ {s_16=m8_fp_float(m8_le_read(control+112u,4));}
/* 16a0c: vmul.f32 s15, s15, s5 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_5,0);}
/* 16a10: vldr s1, [r6, #0x74] */ {s_1=m8_fp_float(m8_le_read(control+116u,4));}
/* 16a14: vldr s2, [r6, #0x5c] */ {s_2=m8_fp_float(m8_le_read(control+92u,4));}
/* 16a18: vfma.f32 s6, s13, s24 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_24,s_6);}
/* 16a1c: vldr s3, [r6, #0x60] */ {s_3=m8_fp_float(m8_le_read(control+96u,4));}
/* 16a20: vfma.f32 s7, s13, s20 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_20,s_7);}
/* 16a24: vldr s4, [r6, #0x64] */ {s_4=m8_fp_float(m8_le_read(control+100u,4));}
/* 16a28: vfma.f32 s8, s13, s16 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_16,s_8);}
/* 16a2c: vldr s5, [r6, #0x68] */ {s_5=m8_fp_float(m8_le_read(control+104u,4));}
/* 16a30: vfma.f32 s9, s13, s1 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_1,s_9);}
/* 16a34: vfma.f32 s10, s13, s2 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_2,s_10);}
/* 16a38: vstr s14, [r6, #0x50] */ {m8_le_write(control+80u,4,m8_fp_bits32(s_14));}
/* 16a3c: vfma.f32 s11, s13, s3 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_3,s_11);}
/* 16a40: vfma.f32 s12, s13, s4 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_4,s_12);}
/* 16a44: vstr s6, [r6, #0x2c] */ {m8_le_write(control+44u,4,m8_fp_bits32(s_6));}
/* 16a48: vfma.f32 s15, s13, s5 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_5,s_15);}
/* 16a4c: vstr s7, [r6, #0x44] */ {m8_le_write(control+68u,4,m8_fp_bits32(s_7));}
/* 16a50: vcmpe.f32 s14, s22 */ {m8_fp_cmp32(cpu,m8_fp_bits32(s_14),m8_fp_bits32(s_22),1);}
/* 16a54: vstr s8, [r6, #0x48] */ {m8_le_write(control+72u,4,m8_fp_bits32(s_8));}
/* 16a58: vstr s9, [r6, #0x4c] */ {m8_le_write(control+76u,4,m8_fp_bits32(s_9));}
/* 16a5c: vstr s10, [r6, #0x34] */ {m8_le_write(control+52u,4,m8_fp_bits32(s_10));}
/* 16a60: vmrs apsr_nzcv, fpscr */ {cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);}
/* 16a64: vstr s11, [r6, #0x38] */ {m8_le_write(control+56u,4,m8_fp_bits32(s_11));}
/* 16a68: vstr s12, [r6, #0x3c] */ {m8_le_write(control+60u,4,m8_fp_bits32(s_12));}
/* 16a6c: vstr s15, [r6, #0x40] */ {m8_le_write(control+64u,4,m8_fp_bits32(s_15));}
/* 16a70: blt.w #0x17418 */ {if(m8_condition(cpu->xpsr,12)) {goto L_17418;}}
/* 16a74: vsub.f32 s14, s22, s14 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_22,s_14,0);}
/* 16a78: vmov.f32 s15, #2.000000e+00 */ {s_15=m8_fp_float(0x40000000u);}
/* 16a7c: vmov.f32 s25, s17 */ {s_25=s_17;}
/* 16a80: vmov.f32 s24, #1.000000e+00 */ {s_24=m8_fp_float(0x3f800000u);}
/* 16a84: vfma.f32 s25, s14, s15 */ {s_25=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_14,s_15,s_25);}
/* 16a88: load phase/step base pointers (live, as firmware). */
L_16a88:{r_5=m8_le_read(object+0xec,4);}
/* 16a8c: ldr.w r7, [r8, #0xcc] */ {r_7=m8_le_read(object+0xcc,4);}
/* 16a90: vldr s13, [r5] */ {s_13=m8_fp_float(m8_le_read(ctx.phases+0u,4));}
/* 16a94: vldr s12, [r7] */ {s_12=m8_fp_float(m8_le_read(ctx.steps+0u,4));}
/* 16a98: vldr s14, [r5, #4] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+4u,4));}
/* 16a9c: vadd.f32 s13, s13, s12 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_12,0);}
/* 16aa0: vldr s15, [r5, #8] */ {s_15=m8_fp_float(m8_le_read(ctx.phases+8u,4));}
/* 16aa4: vldr s7, [r5, #0xc] */ {s_7=m8_fp_float(m8_le_read(ctx.phases+12u,4));}
/* 16aa8: vldr s8, [r5, #0x10] */ {s_8=m8_fp_float(m8_le_read(ctx.phases+16u,4));}
/* 16aac: vstr s13, [r5] */ {m8_le_write(ctx.phases+0u,4,m8_fp_bits32(s_13));}
/* 16ab0: vcvt.s32.f32 s11, s13 */ {s_11=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),0));}
/* 16ab4: vldr s12, [r7, #4] */ {s_12=m8_fp_float(m8_le_read(ctx.steps+4u,4));}
/* 16ab8: vldr s16, [r5, #0x14] */ {s_16=m8_fp_float(m8_le_read(ctx.phases+20u,4));}
/* 16abc: vadd.f32 s14, s14, s12 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_12,0);}
/* 16ac0: vldr s0, [r5, #0x18] */ {s_0=m8_fp_float(m8_le_read(ctx.phases+24u,4));}
/* 16ac4: vcvt.f32.s32 s11, s11 */ {s_11=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_11),0));}
/* 16ac8: vldr s10, [r5, #0x1c] */ {s_10=m8_fp_float(m8_le_read(ctx.phases+28u,4));}
/* 16acc: vldr s2, [r5, #0x20] */ {s_2=m8_fp_float(m8_le_read(ctx.phases+32u,4));}
/* 16ad0: vstr s14, [r5, #4] */ {m8_le_write(ctx.phases+4u,4,m8_fp_bits32(s_14));}
/* 16ad4: vcvt.s32.f32 s12, s14 */ {s_12=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16ad8: vldr s9, [r7, #8] */ {s_9=m8_fp_float(m8_le_read(ctx.steps+8u,4));}
/* 16adc: vsub.f32 s4, s13, s11 */ {s_4=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_11,0);}
/* 16ae0: vldr s1, [r5, #0x24] */ {s_1=m8_fp_float(m8_le_read(ctx.phases+36u,4));}
/* 16ae4: vadd.f32 s15, s15, s9 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_9,0);}
/* 16ae8: vcvt.f32.s32 s12, s12 */ {s_12=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_12),0));}
/* 16aec: vadd.f32 s3, s4, s22 */ {s_3=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_4,s_22,0);}
/* 16af0: vstr s15, [r5, #8] */ {m8_le_write(ctx.phases+8u,4,m8_fp_bits32(s_15));}
/* 16af4: vcvt.s32.f32 s13, s15 */ {s_13=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_15),0));}
/* 16af8: vldr s11, [r7, #0xc] */ {s_11=m8_fp_float(m8_le_read(ctx.steps+12u,4));}
/* 16afc: vsub.f32 s5, s14, s12 */ {s_5=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_12,0);}
/* 16b00: vadd.f32 s7, s7, s11 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_7,s_11,0);}
/* 16b04: vcvt.f32.s32 s13, s13 */ {s_13=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_13),0));}
/* 16b08: vcvt.s32.f32 s11, s3 */ {s_11=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_3),0));}
/* 16b0c: vstr s7, [r5, #0xc] */ {m8_le_write(ctx.phases+12u,4,m8_fp_bits32(s_7));}
/* 16b10: vcvt.s32.f32 s14, s7 */ {s_14=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_7),0));}
/* 16b14: vldr s12, [r7, #0x10] */ {s_12=m8_fp_float(m8_le_read(ctx.steps+16u,4));}
/* 16b18: vsub.f32 s6, s15, s13 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_13,0);}
/* 16b1c: vcvt.f32.s32 s11, s11 */ {s_11=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_11),0));}
/* 16b20: vadd.f32 s8, s8, s12 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_8,s_12,0);}
/* 16b24: vcvt.f32.s32 s15, s14 */ {s_15=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_14),0));}
/* 16b28: vsub.f32 s11, s3, s11 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_3,s_11,0);}
/* 16b2c: vstr s8, [r5, #0x10] */ {m8_le_write(ctx.phases+16u,4,m8_fp_bits32(s_8));}
/* 16b30: vcvt.s32.f32 s14, s8 */ {s_14=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_8),0));}
/* 16b34: vldr s9, [r7, #0x14] */ {s_9=m8_fp_float(m8_le_read(ctx.steps+20u,4));}
/* 16b38: vsub.f32 s7, s7, s15 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_7,s_15,0);}
/* 16b3c: vadd.f32 s9, s16, s9 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_16,s_9,0);}
/* 16b40: vcvt.f32.s32 s14, s14 */ {s_14=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_14),0));}
/* 16b44: vstr s9, [r5, #0x14] */ {m8_le_write(ctx.phases+20u,4,m8_fp_bits32(s_9));}
/* 16b48: vcvt.s32.f32 s15, s9 */ {s_15=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_9),0));}
/* 16b4c: vldr s12, [r7, #0x18] */ {s_12=m8_fp_float(m8_le_read(ctx.steps+24u,4));}
/* 16b50: vsub.f32 s8, s8, s14 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_8,s_14,0);}
/* 16b54: vadd.f32 s12, s0, s12 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_0,s_12,0);}
/* 16b58: vcvt.f32.s32 s15, s15 */ {s_15=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_15),0));}
/* 16b5c: vstr s12, [r5, #0x18] */ {m8_le_write(ctx.phases+24u,4,m8_fp_bits32(s_12));}
/* 16b60: vcvt.s32.f32 s14, s12 */ {s_14=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_12),0));}
/* 16b64: vldr s16, [r7, #0x1c] */ {s_16=m8_fp_float(m8_le_read(ctx.steps+28u,4));}
/* 16b68: vsub.f32 s9, s9, s15 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_9,s_15,0);}
/* 16b6c: vadd.f32 s16, s10, s16 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_10,s_16,0);}
/* 16b70: vcvt.f32.s32 s14, s14 */ {s_14=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_14),0));}
/* 16b74: vadd.f32 s10, s11, s11 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_11,s_11,0);}
/* 16b78: vstr s16, [r5, #0x1c] */ {m8_le_write(ctx.phases+28u,4,m8_fp_bits32(s_16));}
/* 16b7c: vcvt.s32.f32 s15, s16 */ {s_15=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_16),0));}
/* 16b80: vldr s0, [r7, #0x20] */ {s_0=m8_fp_float(m8_le_read(ctx.steps+32u,4));}
/* 16b84: vsub.f32 s12, s12, s14 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_12,s_14,0);}
/* 16b88: vadd.f32 s0, s2, s0 */ {s_0=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_2,s_0,0);}
/* 16b8c: vcvt.f32.s32 s15, s15 */ {s_15=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_15),0));}
/* 16b90: vstr s0, [r5, #0x20] */ {m8_le_write(ctx.phases+32u,4,m8_fp_bits32(s_0));}
/* 16b94: vcvt.s32.f32 s2, s0 */ {s_2=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_0),0));}
/* 16b98: vldr s13, [r7, #0x24] */ {s_13=m8_fp_float(m8_le_read(ctx.steps+36u,4));}
/* 16b9c: vsub.f32 s16, s16, s15 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_16,s_15,0);}
/* 16ba0: vadd.f32 s13, s1, s13 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_1,s_13,0);}
/* 16ba4: vcvt.f32.s32 s2, s2 */ {s_2=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_2),0));}
/* 16ba8: vstr s13, [r5, #0x24] */ {m8_le_write(ctx.phases+36u,4,m8_fp_bits32(s_13));}
/* 16bac: vcvt.s32.f32 s1, s13 */ {s_1=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),0));}
/* 16bb0: vldr s15, [r5, #0x28] */ {s_15=m8_fp_float(m8_le_read(ctx.phases+40u,4));}
/* 16bb4: vsub.f32 s0, s0, s2 */ {s_0=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_0,s_2,0);}
/* 16bb8: vldr s20, [r7, #0x28] */ {s_20=m8_fp_float(m8_le_read(ctx.steps+40u,4));}
/* 16bbc: ldr.w r4, [r8, #0xd4] */ {r_4=m8_le_read(object+0xd4,4);}
/* 16bc0: vcvt.f32.s32 s1, s1 */ {s_1=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_1),0));}
/* 16bc4: vadd.f32 s15, s15, s20 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_20,0);}
/* 16bc8: vldr s14, [r5, #0x2c] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+44u,4));}
/* 16bcc: vsub.f32 s13, s13, s1 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_1,0);}
/* 16bd0: vstr s15, [r5, #0x28] */ {m8_le_write(ctx.phases+40u,4,m8_fp_bits32(s_15));}
/* 16bd4: vcvt.s32.f32 s2, s15 */ {s_2=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_15),0));}
/* 16bd8: vldr s20, [r7, #0x2c] */ {s_20=m8_fp_float(m8_le_read(ctx.steps+44u,4));}
/* 16bdc: vstr s8, [r5, #0x10] */ {m8_le_write(ctx.phases+16u,4,m8_fp_bits32(s_8));}
/* 16be0: vadd.f32 s14, s14, s20 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_20,0);}
/* 16be4: vstr s7, [r5, #0xc] */ {m8_le_write(ctx.phases+12u,4,m8_fp_bits32(s_7));}
/* 16be8: vstr s12, [r5, #0x18] */ {m8_le_write(ctx.phases+24u,4,m8_fp_bits32(s_12));}
/* 16bec: vcvt.f32.s32 s7, s2 */ {s_7=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_2),0));}
/* 16bf0: vstr s5, [r5, #4] */ {m8_le_write(ctx.phases+4u,4,m8_fp_bits32(s_5));}
/* 16bf4: vcvt.s32.f32 s8, s14 */ {s_8=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16bf8: vstr s6, [r5, #8] */ {m8_le_write(ctx.phases+8u,4,m8_fp_bits32(s_6));}
/* 16bfc: vsub.f32 s15, s15, s7 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_7,0);}
/* 16c00: vstr s9, [r5, #0x14] */ {m8_le_write(ctx.phases+20u,4,m8_fp_bits32(s_9));}
/* 16c04: vstr s16, [r5, #0x1c] */ {m8_le_write(ctx.phases+28u,4,m8_fp_bits32(s_16));}
/* 16c08: vmov.f32 s6, #2.000000e+00 */ {s_6=m8_fp_float(0x40000000u);}
/* 16c0c: vcvt.f32.s32 s12, s8 */ {s_12=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_8),0));}
/* 16c10: vstr s4, [r5] */ {m8_le_write(ctx.phases+0u,4,m8_fp_bits32(s_4));}
/* 16c14: vstr s0, [r5, #0x20] */ {m8_le_write(ctx.phases+32u,4,m8_fp_bits32(s_0));}
/* 16c18: vmov.f32 s8, #-1.000000e+00 */ {s_8=m8_fp_float(0xbf800000u);}
/* 16c1c: vstr s13, [r5, #0x24] */ {m8_le_write(ctx.phases+36u,4,m8_fp_bits32(s_13));}
/* 16c20: vsub.f32 s14, s14, s12 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_12,0);}
/* 16c24: vstr s15, [r5, #0x28] */ {m8_le_write(ctx.phases+40u,4,m8_fp_bits32(s_15));}
/* 16c28: vsub.f32 s15, s10, s17 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_10,s_17,0);}
/* 16c2c: vstr s14, [r5, #0x2c] */ {m8_le_write(ctx.phases+44u,4,m8_fp_bits32(s_14));}
/* 16c30: vstr s3, [r4] */ {m8_le_write(ctx.scratchB+0u,4,m8_fp_bits32(s_3));}
/* 16c34: vldr s14, [r5, #4] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+4u,4));}
/* 16c38: vadd.f32 s14, s14, s22 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_22,0);}
/* 16c3c: vstr s14, [r4, #4] */ {m8_le_write(ctx.scratchB+4u,4,m8_fp_bits32(s_14));}
/* 16c40: vcvt.s32.f32 s9, s14 */ {s_9=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16c44: vldr s12, [r5, #8] */ {s_12=m8_fp_float(m8_le_read(ctx.phases+8u,4));}
/* 16c48: vadd.f32 s12, s12, s22 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_22,0);}
/* 16c4c: vcvt.f32.s32 s9, s9 */ {s_9=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_9),0));}
/* 16c50: vstr s12, [r4, #8] */ {m8_le_write(ctx.scratchB+8u,4,m8_fp_bits32(s_12));}
/* 16c54: vcvt.s32.f32 s7, s12 */ {s_7=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_12),0));}
/* 16c58: vldr s13, [r5, #0xc] */ {s_13=m8_fp_float(m8_le_read(ctx.phases+12u,4));}
/* 16c5c: vsub.f32 s3, s14, s9 */ {s_3=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_9,0);}
/* 16c60: vadd.f32 s13, s13, s22 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_22,0);}
/* 16c64: vcvt.f32.s32 s7, s7 */ {s_7=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_7),0));}
/* 16c68: vstr s13, [r4, #0xc] */ {m8_le_write(ctx.scratchB+12u,4,m8_fp_bits32(s_13));}
/* 16c6c: vcvt.s32.f32 s9, s13 */ {s_9=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),0));}
/* 16c70: vldr s14, [r5, #0x10] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+16u,4));}
/* 16c74: vsub.f32 s4, s12, s7 */ {s_4=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_12,s_7,0);}
/* 16c78: vadd.f32 s14, s14, s22 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_22,0);}
/* 16c7c: vcvt.f32.s32 s9, s9 */ {s_9=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_9),0));}
/* 16c80: vstr s14, [r4, #0x10] */ {m8_le_write(ctx.scratchB+16u,4,m8_fp_bits32(s_14));}
/* 16c84: vcvt.s32.f32 s7, s14 */ {s_7=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16c88: vldr s12, [r5, #0x14] */ {s_12=m8_fp_float(m8_le_read(ctx.phases+20u,4));}
/* 16c8c: vsub.f32 s5, s13, s9 */ {s_5=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_9,0);}
/* 16c90: vadd.f32 s12, s12, s22 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_22,0);}
/* 16c94: vcvt.f32.s32 s7, s7 */ {s_7=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_7),0));}
/* 16c98: vstr s12, [r4, #0x14] */ {m8_le_write(ctx.scratchB+20u,4,m8_fp_bits32(s_12));}
/* 16c9c: vcvt.s32.f32 s9, s12 */ {s_9=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_12),0));}
/* 16ca0: vldr s13, [r5, #0x18] */ {s_13=m8_fp_float(m8_le_read(ctx.phases+24u,4));}
/* 16ca4: vsub.f32 s7, s14, s7 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_7,0);}
/* 16ca8: vadd.f32 s13, s13, s22 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_22,0);}
/* 16cac: vcvt.f32.s32 s9, s9 */ {s_9=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_9),0));}
/* 16cb0: vstr s13, [r4, #0x18] */ {m8_le_write(ctx.scratchB+24u,4,m8_fp_bits32(s_13));}
/* 16cb4: vcvt.s32.f32 s2, s13 */ {s_2=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),0));}
/* 16cb8: vldr s14, [r5, #0x1c] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+28u,4));}
/* 16cbc: vsub.f32 s9, s12, s9 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_12,s_9,0);}
/* 16cc0: vadd.f32 s14, s14, s22 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_22,0);}
/* 16cc4: vcvt.f32.s32 s12, s2 */ {s_12=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_2),0));}
/* 16cc8: vstr s14, [r4, #0x1c] */ {m8_le_write(ctx.scratchB+28u,4,m8_fp_bits32(s_14));}
/* 16ccc: vcvt.s32.f32 s1, s14 */ {s_1=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16cd0: vldr s20, [r5, #0x20] */ {s_20=m8_fp_float(m8_le_read(ctx.phases+32u,4));}
/* 16cd4: vsub.f32 s0, s13, s12 */ {s_0=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_12,0);}
/* 16cd8: ldr.w r3, [r8, #0xd0] */ {r_3=m8_le_read(object+0xd0,4);}
/* 16cdc: vadd.f32 s20, s20, s22 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_20,s_22,0);}
/* 16ce0: vcvt.f32.s32 s1, s1 */ {s_1=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_1),0));}
/* 16ce4: vstr s20, [r4, #0x20] */ {m8_le_write(ctx.scratchB+32u,4,m8_fp_bits32(s_20));}
/* 16ce8: vcvt.s32.f32 s2, s20 */ {s_2=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_20),0));}
/* 16cec: vldr s12, [r5, #0x24] */ {s_12=m8_fp_float(m8_le_read(ctx.phases+36u,4));}
/* 16cf0: vsub.f32 s16, s14, s1 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_1,0);}
/* 16cf4: vadd.f32 s12, s12, s22 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_22,0);}
/* 16cf8: vcvt.f32.s32 s2, s2 */ {s_2=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_2),0));}
/* 16cfc: vstr s12, [r4, #0x24] */ {m8_le_write(ctx.scratchB+36u,4,m8_fp_bits32(s_12));}
/* 16d00: vcvt.s32.f32 s1, s12 */ {s_1=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_12),0));}
/* 16d04: vldr s13, [r5, #0x28] */ {s_13=m8_fp_float(m8_le_read(ctx.phases+40u,4));}
/* 16d08: vsub.f32 s20, s20, s2 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_20,s_2,0);}
/* 16d0c: vadd.f32 s13, s13, s22 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_22,0);}
/* 16d10: vcvt.f32.s32 s1, s1 */ {s_1=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_1),0));}
/* 16d14: vstr s13, [r4, #0x28] */ {m8_le_write(ctx.scratchB+40u,4,m8_fp_bits32(s_13));}
/* 16d18: vcvt.s32.f32 s2, s13 */ {s_2=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),0));}
/* 16d1c: vldr s14, [r5, #0x2c] */ {s_14=m8_fp_float(m8_le_read(ctx.phases+44u,4));}
/* 16d20: vsub.f32 s12, s12, s1 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_12,s_1,0);}
/* 16d24: vstr s7, [r4, #0x10] */ {m8_le_write(ctx.scratchB+16u,4,m8_fp_bits32(s_7));}
/* 16d28: vadd.f32 s14, s14, s22 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_22,0);}
/* 16d2c: vstr s5, [r4, #0xc] */ {m8_le_write(ctx.scratchB+12u,4,m8_fp_bits32(s_5));}
/* 16d30: vstr s11, [r4] */ {m8_le_write(ctx.scratchB+0u,4,m8_fp_bits32(s_11));}
/* 16d34: vcvt.f32.s32 s5, s2 */ {s_5=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_2),0));}
/* 16d38: vstr s3, [r4, #4] */ {m8_le_write(ctx.scratchB+4u,4,m8_fp_bits32(s_3));}
/* 16d3c: vcvt.s32.f32 s7, s14 */ {s_7=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 16d40: vstr s4, [r4, #8] */ {m8_le_write(ctx.scratchB+8u,4,m8_fp_bits32(s_4));}
/* 16d44: vsub.f32 s13, s13, s5 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_5,0);}
/* 16d48: vstr s9, [r4, #0x14] */ {m8_le_write(ctx.scratchB+20u,4,m8_fp_bits32(s_9));}
/* 16d4c: vstr s0, [r4, #0x18] */ {m8_le_write(ctx.scratchB+24u,4,m8_fp_bits32(s_0));}
/* 16d50: vcvt.f32.s32 s11, s7 */ {s_11=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_7),0));}
/* 16d54: vstr s12, [r4, #0x24] */ {m8_le_write(ctx.scratchB+36u,4,m8_fp_bits32(s_12));}
/* 16d58: vstr s13, [r4, #0x28] */ {m8_le_write(ctx.scratchB+40u,4,m8_fp_bits32(s_13));}
/* 16d5c: vstr s16, [r4, #0x1c] */ {m8_le_write(ctx.scratchB+28u,4,m8_fp_bits32(s_16));}
/* 16d60: vsub.f32 s14, s14, s11 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_11,0);}
/* 16d64: vstr s20, [r4, #0x20] */ {m8_le_write(ctx.scratchB+32u,4,m8_fp_bits32(s_20));}
/* 16d68: vstr s14, [r4, #0x2c] */ {m8_le_write(ctx.scratchB+44u,4,m8_fp_bits32(s_14));}
/* 16d6c: vstr s10, [r3] */ {m8_le_write(ctx.scratchA+0u,4,m8_fp_bits32(s_10));}
/* 16d70: vldr s12, [r4, #4] */ {s_12=m8_fp_float(m8_le_read(ctx.scratchB+4u,4));}
/* 16d74: vadd.f32 s12, s12, s12 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_12,0);}
/* 16d78: vstr s12, [r3, #4] */ {m8_le_write(ctx.scratchA+4u,4,m8_fp_bits32(s_12));}
/* 16d7c: vsub.f32 s12, s12, s17 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_12,s_17,0);}
/* 16d80: vldr s9, [r4, #8] */ {s_9=m8_fp_float(m8_le_read(ctx.scratchB+8u,4));}
/* 16d84: vadd.f32 s9, s9, s9 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_9,s_9,0);}
/* 16d88: vstr s9, [r3, #8] */ {m8_le_write(ctx.scratchA+8u,4,m8_fp_bits32(s_9));}
/* 16d8c: vsub.f32 s9, s9, s17 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_9,s_17,0);}
/* 16d90: vldr s7, [r4, #0xc] */ {s_7=m8_fp_float(m8_le_read(ctx.scratchB+12u,4));}
/* 16d94: vadd.f32 s7, s7, s7 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_7,s_7,0);}
/* 16d98: vstr s7, [r3, #0xc] */ {m8_le_write(ctx.scratchA+12u,4,m8_fp_bits32(s_7));}
/* 16d9c: vsub.f32 s7, s7, s17 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_7,s_17,0);}
/* 16da0: vldr s10, [r4, #0x10] */ {s_10=m8_fp_float(m8_le_read(ctx.scratchB+16u,4));}
/* 16da4: vadd.f32 s10, s10, s10 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_10,s_10,0);}
/* 16da8: vstr s10, [r3, #0x10] */ {m8_le_write(ctx.scratchA+16u,4,m8_fp_bits32(s_10));}
/* 16dac: vsub.f32 s10, s10, s17 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_10,s_17,0);}
/* 16db0: vldr s11, [r4, #0x14] */ {s_11=m8_fp_float(m8_le_read(ctx.scratchB+20u,4));}
/* 16db4: vadd.f32 s11, s11, s11 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_11,s_11,0);}
/* 16db8: vstr s11, [r3, #0x14] */ {m8_le_write(ctx.scratchA+20u,4,m8_fp_bits32(s_11));}
/* 16dbc: vsub.f32 s11, s11, s17 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_11,s_17,0);}
/* 16dc0: vldr s13, [r4, #0x18] */ {s_13=m8_fp_float(m8_le_read(ctx.scratchB+24u,4));}
/* 16dc4: vadd.f32 s13, s13, s13 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_13,0);}
/* 16dc8: vstr s13, [r3, #0x18] */ {m8_le_write(ctx.scratchA+24u,4,m8_fp_bits32(s_13));}
/* 16dcc: vsub.f32 s2, s13, s17 */ {s_2=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_17,0);}
/* 16dd0: vldr s14, [r4, #0x1c] */ {s_14=m8_fp_float(m8_le_read(ctx.scratchB+28u,4));}
/* 16dd4: vadd.f32 s14, s14, s14 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_14,0);}
/* 16dd8: vstr s14, [r3, #0x1c] */ {m8_le_write(ctx.scratchA+28u,4,m8_fp_bits32(s_14));}
/* 16ddc: vsub.f32 s3, s14, s17 */ {s_3=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_17,0);}
/* 16de0: vldr s13, [r4, #0x20] */ {s_13=m8_fp_float(m8_le_read(ctx.scratchB+32u,4));}
/* 16de4: vadd.f32 s13, s13, s13 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_13,0);}
/* 16de8: vstr s13, [r3, #0x20] */ {m8_le_write(ctx.scratchA+32u,4,m8_fp_bits32(s_13));}
/* 16dec: vsub.f32 s4, s13, s17 */ {s_4=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_17,0);}
/* 16df0: vldr s14, [r4, #0x24] */ {s_14=m8_fp_float(m8_le_read(ctx.scratchB+36u,4));}
/* 16df4: vadd.f32 s14, s14, s14 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_14,0);}
/* 16df8: vstr s14, [r3, #0x24] */ {m8_le_write(ctx.scratchA+36u,4,m8_fp_bits32(s_14));}
/* 16dfc: vsub.f32 s14, s14, s17 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_14,s_17,0);}
/* 16e00: vldr s13, [r4, #0x28] */ {s_13=m8_fp_float(m8_le_read(ctx.scratchB+40u,4));}
/* 16e04: vadd.f32 s13, s13, s13 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_13,s_13,0);}
/* 16e08: vstr s13, [r3, #0x28] */ {m8_le_write(ctx.scratchA+40u,4,m8_fp_bits32(s_13));}
/* 16e0c: vldr s5, [r4, #0x2c] */ {s_5=m8_fp_float(m8_le_read(ctx.scratchB+44u,4));}
/* 16e10: vstr s15, [r3] */ {m8_le_write(ctx.scratchA+0u,4,m8_fp_bits32(s_15));}
/* 16e14: vfma.f32 s8, s5, s6 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_5,s_6,s_8);}
/* 16e18: vstr s12, [r3, #4] */ {m8_le_write(ctx.scratchA+4u,4,m8_fp_bits32(s_12));}
/* 16e1c: vsub.f32 s6, s13, s17 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_17,0);}
/* 16e20: vstr s9, [r3, #8] */ {m8_le_write(ctx.scratchA+8u,4,m8_fp_bits32(s_9));}
/* 16e24: vstr s7, [r3, #0xc] */ {m8_le_write(ctx.scratchA+12u,4,m8_fp_bits32(s_7));}
/* 16e28: vstr s10, [r3, #0x10] */ {m8_le_write(ctx.scratchA+16u,4,m8_fp_bits32(s_10));}
/* 16e2c: vstr s11, [r3, #0x14] */ {m8_le_write(ctx.scratchA+20u,4,m8_fp_bits32(s_11));}
/* 16e30: vstr s2, [r3, #0x18] */ {m8_le_write(ctx.scratchA+24u,4,m8_fp_bits32(s_2));}
/* 16e34: vstr s3, [r3, #0x1c] */ {m8_le_write(ctx.scratchA+28u,4,m8_fp_bits32(s_3));}
/* 16e38: vmov.f32 s1, s8 */ {s_1=s_8;}
/* 16e3c: vstr s4, [r3, #0x20] */ {m8_le_write(ctx.scratchA+32u,4,m8_fp_bits32(s_4));}
/* 16e40: vstr s14, [r3, #0x24] */ {m8_le_write(ctx.scratchA+36u,4,m8_fp_bits32(s_14));}
/* 16e44: vstr s6, [r3, #0x28] */ {m8_le_write(ctx.scratchA+40u,4,m8_fp_bits32(s_6));}
/* 16e48: vstr s8, [r3, #0x2c] */ {m8_le_write(ctx.scratchA+44u,4,m8_fp_bits32(s_8));}
/* 16e4c..170d2: twelve BLEP lanes. Each lane keeps its original FP order
 * and conditional store; lanes are independent. */
{float wave[12]={s_15,s_12,s_9,s_7,s_10,s_11,s_2,s_3,s_4,s_14,s_6,s_1};
for(unsigned lane=0;lane<12;lane++) {
s_8=m8_fp_float(m8_le_read(ctx.steps+lane*4u,4));
s_13=m8_fp_float(m8_le_read(ctx.scratchB+lane*4u,4));
m8_fp_cmp32(cpu,m8_fp_bits32(s_13),m8_fp_bits32(s_8),1);
cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
if(cpu->xpsr&0x80000000u) {
s_5=m8_hvf_arithmetic(cpu,M8_FP_DIV,s_13,s_8,0);
s_13=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_5,s_17,0);
wave[lane]=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_13,wave[lane]);
m8_le_write(ctx.scratchA+lane*4u,4,m8_fp_bits32(wave[lane]));
} else {
s_5=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_17,s_8,0);
m8_fp_cmp32(cpu,m8_fp_bits32(s_13),m8_fp_bits32(s_5),1);
cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
if(!(cpu->xpsr&0x40000000u) && (((cpu->xpsr>>31)^(cpu->xpsr>>28))&1u)==0) {
s_13=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_13,s_17,0);
s_5=m8_hvf_arithmetic(cpu,M8_FP_DIV,s_13,s_8,0);
s_13=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_5,s_17,0);
wave[lane]=m8_hvf_arithmetic(cpu,M8_FP_FMA,-s_13,s_13,wave[lane]);
m8_le_write(ctx.scratchA+lane*4u,4,m8_fp_bits32(wave[lane]));
}
}
} s_15=wave[0];s_12=wave[1];s_9=wave[2];s_7=wave[3];
s_10=wave[4];s_11=wave[5];s_2=wave[6];s_3=wave[7];
s_4=wave[8];s_14=wave[9];s_6=wave[10];s_1=wave[11];}
/* 170d4: vldr s5, [r6, #0x34] */ {s_5=m8_fp_float(m8_le_read(control+52u,4));}
/* 170d8: ldrb.w r2, [r8, #0xe0] */ {r_2=object[0xe0];}
/* 170dc: vsub.f32 s8, s17, s5 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_17,s_5,0);}
/* 170e0: vmul.f32 s2, s5, s2 */ {s_2=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_2,0);}
/* 170e4: vmul.f32 s3, s5, s3 */ {s_3=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_3,0);}
/* 170e8: vmul.f32 s4, s5, s4 */ {s_4=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_4,0);}
/* 170ec: vmul.f32 s14, s5, s14 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_14,0);}
/* 170f0: vstr s2, [r3, #0x18] */ {m8_le_write(ctx.scratchA+24u,4,m8_fp_bits32(s_2));}
/* 170f4: vmul.f32 s6, s5, s6 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_6,0);}
/* 170f8: vstr s3, [r3, #0x1c] */ {m8_le_write(ctx.scratchA+28u,4,m8_fp_bits32(s_3));}
/* 170fc: vmul.f32 s15, s8, s15 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_8,s_15,0);}
/* 17100: vstr s4, [r3, #0x20] */ {m8_le_write(ctx.scratchA+32u,4,m8_fp_bits32(s_4));}
/* 17104: vmul.f32 s12, s8, s12 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_8,s_12,0);}
/* 17108: vstr s14, [r3, #0x24] */ {m8_le_write(ctx.scratchA+36u,4,m8_fp_bits32(s_14));}
/* 1710c: vmul.f32 s9, s9, s8 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_9,s_8,0);}
/* 17110: vstr s6, [r3, #0x28] */ {m8_le_write(ctx.scratchA+40u,4,m8_fp_bits32(s_6));}
/* 17114: vmul.f32 s7, s8, s7 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_8,s_7,0);}
/* 17118: vstr s15, [r3] */ {m8_le_write(ctx.scratchA+0u,4,m8_fp_bits32(s_15));}
/* 1711c: vmul.f32 s10, s10, s8 */ {s_10=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_10,s_8,0);}
/* 17120: vstr s12, [r3, #4] */ {m8_le_write(ctx.scratchA+4u,4,m8_fp_bits32(s_12));}
/* 17124: vmul.f32 s11, s11, s8 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_11,s_8,0);}
/* 17128: vstr s9, [r3, #8] */ {m8_le_write(ctx.scratchA+8u,4,m8_fp_bits32(s_9));}
/* 1712c: vmul.f32 s13, s5, s1 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_5,s_1,0);}
/* 17130: vstr s7, [r3, #0xc] */ {m8_le_write(ctx.scratchA+12u,4,m8_fp_bits32(s_7));}
/* 17134: vstr s10, [r3, #0x10] */ {m8_le_write(ctx.scratchA+16u,4,m8_fp_bits32(s_10));}
/* 17138: vstr s11, [r3, #0x14] */ {m8_le_write(ctx.scratchA+20u,4,m8_fp_bits32(s_11));}
/* 1713c: vstr s13, [r3, #0x2c] */ {m8_le_write(ctx.scratchA+44u,4,m8_fp_bits32(s_13));}
/* 17140: cmp r2, #0 */ {m8_sub_flags(cpu,r_2,0x0u,0u);}
/* 17142: bne.w #0x175da: alternate mixer must stay 0; else resume translated. */
{if(m8_condition(cpu->xpsr,2)) {M8_HVF_SAVE(output);output->r[15]=0x17142u;return M8_AOT_EXIT;}}
/* 17146: ldrb.w r2, [r8, #0x55] */ {r_2=object[0x55];}
/* 1714a: vldr s10, [r6, #0x3c] */ {s_10=m8_fp_float(m8_le_read(control+60u,4));}
/* 1714e: lsls r7, r2, #0x1f */ {uint32_t carry=(cpu->xpsr>>29)&1u;r_7=m8_shift(r_2,2,0x1fu,&carry);m8_logic_flags(cpu,r_7,carry);}
/* 17150: vsub.f32 s9, s17, s10 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_17,s_10,0);}
/* 17154: bmi #0x1715e */ {if(m8_condition(cpu->xpsr,5)) {goto L_1715e;}}
/* 17156: vldr s15, [pc, #0x314] */ {s_15=m8_fp_float(m8_le_read(lit17470-4u,4));}
/* 1715a: vmov.f32 s12, s15 */ {s_12=s_15;}
/* 1715e: lsls r5, r2, #0x1e */ L_1715e:{uint32_t carry=(cpu->xpsr>>29)&1u;r_5=m8_shift(r_2,2,0x1eu,&carry);m8_logic_flags(cpu,r_5,carry);}
/* 17160: vstr s15, [r3] */ {m8_le_write(ctx.scratchA+0u,4,m8_fp_bits32(s_15));}
/* 17164: vstr s12, [r3, #4] */ {m8_le_write(ctx.scratchA+4u,4,m8_fp_bits32(s_12));}
/* 17168: bpl.w #0x1737e */ {if(m8_condition(cpu->xpsr,6)) {goto L_1737e;}}
/* 1716c: vldr s8, [r3, #8] */ {s_8=m8_fp_float(m8_le_read(ctx.scratchA+8u,4));}
/* 17170: vadd.f32 s12, s12, s7 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_7,0);}
/* 17174: vadd.f32 s15, s15, s8 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_8,0);}
/* 17178: lsls r4, r2, #0x1d */ L_17178:{uint32_t carry=(cpu->xpsr>>29)&1u;r_4=m8_shift(r_2,2,0x1du,&carry);m8_logic_flags(cpu,r_4,carry);}
/* 1717a: vstr s8, [r3, #8] */ {m8_le_write(ctx.scratchA+8u,4,m8_fp_bits32(s_8));}
/* 1717e: vstr s7, [r3, #0xc] */ {m8_le_write(ctx.scratchA+12u,4,m8_fp_bits32(s_7));}
/* 17182: bpl.w #0x17374 */ {if(m8_condition(cpu->xpsr,6)) {goto L_17374;}}
/* 17186: vldr s8, [r3, #0x10] */ {s_8=m8_fp_float(m8_le_read(ctx.scratchA+16u,4));}
/* 1718a: vadd.f32 s12, s12, s11 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_11,0);}
/* 1718e: vadd.f32 s15, s15, s8 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_8,0);}
/* 17192: lsls r0, r2, #0x1c */ L_17192:{uint32_t carry=(cpu->xpsr>>29)&1u;r_0=m8_shift(r_2,2,0x1cu,&carry);m8_logic_flags(cpu,r_0,carry);}
/* 17194: vstr s8, [r3, #0x10] */ {m8_le_write(ctx.scratchA+16u,4,m8_fp_bits32(s_8));}
/* 17198: vstr s11, [r3, #0x14] */ {m8_le_write(ctx.scratchA+20u,4,m8_fp_bits32(s_11));}
/* 1719c: bpl.w #0x1736a */ {if(m8_condition(cpu->xpsr,6)) {goto L_1736a;}}
/* 171a0: vldr s11, [r3, #0x18] */ {s_11=m8_fp_float(m8_le_read(ctx.scratchA+24u,4));}
/* 171a4: vadd.f32 s12, s12, s3 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_3,0);}
/* 171a8: vadd.f32 s15, s15, s11 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_11,0);}
/* 171ac: lsls r1, r2, #0x1b */ L_171ac:{uint32_t carry=(cpu->xpsr>>29)&1u;r_1=m8_shift(r_2,2,0x1bu,&carry);m8_logic_flags(cpu,r_1,carry);}
/* 171ae: vstr s11, [r3, #0x18] */ {m8_le_write(ctx.scratchA+24u,4,m8_fp_bits32(s_11));}
/* 171b2: vstr s3, [r3, #0x1c] */ {m8_le_write(ctx.scratchA+28u,4,m8_fp_bits32(s_3));}
/* 171b6: bpl.w #0x17360 */ {if(m8_condition(cpu->xpsr,6)) {goto L_17360;}}
/* 171ba: vldr s11, [r3, #0x20] */ {s_11=m8_fp_float(m8_le_read(ctx.scratchA+32u,4));}
/* 171be: vadd.f32 s12, s12, s14 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_14,0);}
/* 171c2: vadd.f32 s15, s15, s11 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_11,0);}
/* 171c6: lsls r2, r2, #0x1a */ L_171c6:{uint32_t carry=(cpu->xpsr>>29)&1u;r_2=m8_shift(r_2,2,0x1au,&carry);m8_logic_flags(cpu,r_2,carry);}
/* 171c8: vstr s11, [r3, #0x20] */ {m8_le_write(ctx.scratchA+32u,4,m8_fp_bits32(s_11));}
/* 171cc: vstr s14, [r3, #0x24] */ {m8_le_write(ctx.scratchA+36u,4,m8_fp_bits32(s_14));}
/* 171d0: bpl.w #0x17356 */ {if(m8_condition(cpu->xpsr,6)) {goto L_17356;}}
/* 171d4: vldr s14, [r3, #0x28] */ {s_14=m8_fp_float(m8_le_read(ctx.scratchA+40u,4));}
/* 171d8: vadd.f32 s12, s12, s13 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_12,s_13,0);}
/* 171dc: vadd.f32 s15, s15, s14 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_14,0);}
/* 171e0: vstr s14, [r3, #0x28] */ L_171e0:{m8_le_write(ctx.scratchA+40u,4,m8_fp_bits32(s_14));}
/* 171e4: vmul.f32 s11, s10, s15 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_10,s_15,0);}
/* 171e8: vstr s13, [r3, #0x2c] */ {m8_le_write(ctx.scratchA+44u,4,m8_fp_bits32(s_13));}
/* 171ec: vmul.f32 s16, s9, s15 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_9,s_15,0);}
/* 171f0: vldr s14, [r8, #0x9c] */ {s_14=m8_fp_float(m8_le_read(object+156u,4));}
/* 171f4: vldr s13, [r8, #0xa0] */ {s_13=m8_fp_float(m8_le_read(object+160u,4));}
/* 171f8: vfma.f32 s11, s9, s12 */ {s_11=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_9,s_12,s_11);}
/* 171fc: vfma.f32 s16, s10, s12 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_10,s_12,s_16);}
/* 17200: vldr s20, [r8, #0xa4] */ {s_20=m8_fp_float(m8_le_read(object+164u,4));}
/* 17204: vadd.f32 s15, s14, s13 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_13,0);}
/* 17208: ldrb.w r2, [r8, #0x41] */ {r_2=object[0x41];}
/* 1720c: vmov.f32 s13, #-1.000000e+00 */ {s_13=m8_fp_float(0xbf800000u);}
/* 17210: vadd.f32 s14, s15, s22 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_15,s_22,0);}
/* 17214: vmul.f32 s16, s20, s16 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_20,s_16,0);}
/* 17218: vcvt.s32.f32 s14, s14 */ {s_14=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_14),0));}
/* 1721c: vmul.f32 s20, s20, s11 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_20,s_11,0);}
/* 17220: vcvt.f32.s32 s14, s14 */ {s_14=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_14),0));}
/* 17224: vsub.f32 s15, s15, s14 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_14,0);}
/* 17228: vcmpe.f32 s15, #0 */ {m8_fp_cmp32(cpu,m8_fp_bits32(s_15),0,1);}
/* 1722c: vstr s15, [r8, #0x9c] */ {m8_le_write(object+156u,4,m8_fp_bits32(s_15));}
/* 17230: vldr s14, [r6, #0x40] */ {s_14=m8_fp_float(m8_le_read(control+64u,4));}
/* 17234: vmrs apsr_nzcv, fpscr */ {cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);}
/* 17238: vselgt.f32 s15, s17, s13 */ {s_15=((!(cpu->xpsr&0x40000000u))&&((((cpu->xpsr>>31)^(cpu->xpsr>>28))&1u)==0))?s_17:s_13;}
/* 1723c: cmp r2, #3 */ {m8_sub_flags(cpu,r_2,0x3u,0u);}
/* 1723e: vfma.f32 s16, s14, s15 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_14,s_15,s_16);}
/* 17242: vfma.f32 s20, s14, s15 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_14,s_15,s_20);}
/* 17246: bhi #0x17280: shape above 3 uses the indirect drive path. */
{if(m8_condition(cpu->xpsr,9)) {M8_HVF_SAVE(output);output->r[15]=0x17246u;return M8_AOT_EXIT;}}
/* 17248: shaper block. The firmware calls the vtable target per channel;
 * admitted targets are the inline clamp (0x1fa5), the inline fold limiter
 * (0x2299), or the inline folder (0x1fbd). Other targets resume translated. */
{uint32_t tabAddr=m8_le_read(object+0x50,4);r_0=tabAddr;}
/* 1724c: vmov.f32 s0, s16 */ {s_0=s_16;}
/* 17250: vldr s1, [r6, #0x4c] */ {s_1=m8_fp_float(m8_le_read(control+76u,4));}
/* 17254: ldr r3, [r0] */ {r_3=m8_le_read(ctx.table+0u,4);}
/* 17256: ldr r3, [r3] */ {r_3=m8_le_read(ctx.vtable+0u,4);
 if(r_3==0x1fa5u) {
  r_14=0x1725bu;s_14=m8_fp_float(0xbf800000u);s_15=1.0f;
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_0,s_1,0);
  s_0=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0));
  s_0=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));
 } else if(r_3==0x1fbdu) {
  /* 1fbc folder, inlined: s1*=0.5; s0*=s1; fold to [-0.5,0.5); s0*=2. */
  r_14=0x1725bu;s_15=m8_fp_float(0x3f000000u);
  s_1=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_1,s_15,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_1,s_0,0);
  m8_fp_cmp32(cpu,m8_fp_bits32(s_0),0,1);
  cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
  if(cpu->xpsr&0x80000000u) s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_0,s_15,0);
  else s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_0,s_15,0);
  s_15=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_15),0));
  s_15=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_15),0));
  s_0=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_0,s_15,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_0,s_0,0);
 } else if(r_3==0x2299u) {
  /* 2298 fold limiter, inlined. vrinta uses ties-away via the same helper
   * as the translated body. */
  r_14=0x1725bu;s_15=1.0f;s_14=m8_fp_float(0x3e800000u);s_13=m8_fp_float(0x40800000u);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_0,s_1,s_15);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_14,0);
  s_12=m8_fp_float(m8_fp_rint(cpu,m8_fp_bits32(s_15),0,4,0));
  s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_12,0);
  s_15=m8_fp_float(m8_fp_bits32(s_15)&0x7fffffffu);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_14,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_13,0);
 } else {M8_HVF_SAVE(output);output->r[15]=0x17248u;return M8_AOT_EXIT;}}
/* 1725a: ldr.w r0, [r8, #0x50] */ {r_0=m8_le_read(object+0x50,4);}
/* 1725e: ldr.w r2, [r8, #0x58] */ {r_2=m8_le_read(object+0x58,4);}
/* 17262: vmov.f32 s16, s0 */ {s_16=s_0;}
/* 17266: ldr r3, [r0] */ {r_3=m8_le_read(ctx.table+0u,4);}
/* 17268: vmov.f32 s0, s20 */ {s_0=s_20;}
/* 1726c: vldr s1, [r2, #0x4c] */ {s_1=m8_fp_float(m8_le_read(ctx.control+76u,4));}
/* 17270: ldr r3, [r3] */ {r_3=m8_le_read(ctx.vtable+0u,4);
 if(r_3==0x1fa5u) {
  r_14=0x17275u;s_14=m8_fp_float(0xbf800000u);s_15=1.0f;
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_0,s_1,0);
  s_0=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0));
  s_0=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));
 } else if(r_3==0x1fbdu) {
  r_14=0x17275u;s_15=m8_fp_float(0x3f000000u);
  s_1=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_1,s_15,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_1,s_0,0);
  m8_fp_cmp32(cpu,m8_fp_bits32(s_0),0,1);
  cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
  if(cpu->xpsr&0x80000000u) s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_0,s_15,0);
  else s_15=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_0,s_15,0);
  s_15=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_15),0));
  s_15=m8_fp_float(m8_hvf_from_int(cpu,m8_fp_bits32(s_15),0));
  s_0=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_0,s_15,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_0,s_0,0);
 } else if(r_3==0x2299u) {
  r_14=0x17275u;s_15=1.0f;s_14=m8_fp_float(0x3e800000u);s_13=m8_fp_float(0x40800000u);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_0,s_1,s_15);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_14,0);
  s_12=m8_fp_float(m8_fp_rint(cpu,m8_fp_bits32(s_15),0,4,0));
  s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_12,0);
  s_15=m8_fp_float(m8_fp_bits32(s_15)&0x7fffffffu);
  s_15=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_15,s_14,0);
  s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_13,0);
 } else {M8_HVF_SAVE(output);output->r[15]=0x1725au;return M8_AOT_EXIT;}}
/* 17274: vmov.f32 s20, s0 */ {s_20=s_0;}
/* 17278: ldr.w r6, [r8, #0x58] */ {r_6=ctx.controlAddr;}
/* 1727c: ldrb.w r2, [r8, #0x41] */ {r_2=object[0x41];}
/* 17280: filter dispatch. Admitted filters run the float core below;
 * anything else resumes translated at the dispatch. */
L_17280:{r_3=object[0x40];
 r_1=m8_sub_flags(cpu,r_3,0x1u,0u);
 m8_sub_flags(cpu,r_1,0x4u,0u);
 if(m8_condition(cpu->xpsr,10)) {
  if(r_3==1u||r_3==2u) goto L_1749c;
  M8_HVF_SAVE(output);output->r[15]=0x17288u;return M8_AOT_EXIT;
 }
 m8_sub_flags(cpu,r_3,0x0u,0u);
 if(m8_condition(cpu->xpsr,2)) {M8_HVF_SAVE(output);output->r[15]=0x1728cu;return M8_AOT_EXIT;}
 m8_sub_flags(cpu,r_2,0x3u,0u);
 if(m8_condition(cpu->xpsr,10)) goto L_172ca;
 M8_HVF_SAVE(output);output->r[15]=0x17292u;return M8_AOT_EXIT;}
/* 1749c: admitted-filter core. Inputs s16/s20 hold the pan mix; the core
 * leaves them intact and produces s12..s15 plus object state a8..b4. */
L_1749c:{s_11=m8_fp_float(m8_le_read(control+68u,4));}
/* 174a0: cmp r3, #5 */ {m8_sub_flags(cpu,r_3,0x5u,0u);}
/* 174a2: vldr s9, [r6, #0x48] */ {s_9=m8_fp_float(m8_le_read(control+72u,4));}
/* 174a6: vminnm.f32 s11, s11, s17 */ {s_11=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_11),m8_fp_bits32(s_17),0));}
/* 174aa: vmaxnm.f32 s11, s11, s23 */ {s_11=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_11),m8_fp_bits32(s_23),0));}
/* 174ae: beq.w #0x1773a: filter 5 second pass; admitted filters fall through. */
{if(m8_condition(cpu->xpsr,1)) {M8_HVF_SAVE(output);output->r[15]=0x174aeu;return M8_AOT_EXIT;}}
/* 174b2: vldr s10, [pc, #-0x3c] */ {s_10=m8_fp_float(m8_le_read(lit17470+8u,4));}
L_174b6:{s_14=m8_fp_float(m8_le_read(object+176u,4));}
/* 174ba: vmov.f32 s5, s16 */ {s_5=s_16;}
/* 174be: vldr s15, [r8, #0xb4] */ {s_15=m8_fp_float(m8_le_read(object+180u,4));}
/* 174c2: vmov.f32 s4, s17 */ {s_4=s_17;}
/* 174c6: vsub.f32 s7, s16, s14 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_16,s_14,0);}
/* 174ca: vldr s13, [r8, #0xa8] */ {s_13=m8_fp_float(m8_le_read(object+168u,4));}
/* 174ce: vsub.f32 s8, s20, s15 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_20,s_15,0);}
/* 174d2: vldr s12, [r8, #0xac] */ {s_12=m8_fp_float(m8_le_read(object+172u,4));}
/* 174d6: vldr s6, [pc, #-0x5c] */ {s_6=m8_fp_float(m8_le_read(lit17470+12u,4));}
/* 174da: cmp r3, #3 */ {m8_sub_flags(cpu,r_3,0x3u,0u);}
/* 174dc: vfms.f32 s7, s13, s9 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_13)^0x80000000u),s_9,s_7);}
/* 174e0: vfms.f32 s8, s9, s12 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_9)^0x80000000u),s_12,s_8);}
/* 174e4: vfma.f32 s13, s11, s7 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_7,s_13);}
/* 174e8: vfma.f32 s12, s11, s8 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_8,s_12);}
/* 174ec: vldr s8, [pc, #-0x70] */ {s_8=m8_fp_float(m8_le_read(lit17470+16u,4));}
/* 174f0: vmov.f32 s7, s20 */ {s_7=s_20;}
/* 174f4: vfma.f32 s14, s11, s13 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_13,s_14);}
/* 174f8: vfma.f32 s15, s11, s12 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_12,s_15);}
/* 174fc: vfms.f32 s5, s9, s13 */ {s_5=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_9)^0x80000000u),s_13,s_5);}
/* 17500: vfms.f32 s7, s9, s12 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_9)^0x80000000u),s_12,s_7);}
/* 17504: vsub.f32 s9, s5, s14 */ {s_9=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_5,s_14,0);}
/* 17508: vsub.f32 s7, s7, s15 */ {s_7=m8_hvf_arithmetic(cpu,M8_FP_SUB,s_7,s_15,0);}
/* 1750c: vfma.f32 s13, s11, s9 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_9,s_13);}
/* 17510: vfma.f32 s12, s11, s7 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_7,s_12);}
/* 17514: vfma.f32 s14, s11, s13 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_13,s_14);}
/* 17518: vmaxnm.f32 s13, s13, s6 */ {s_13=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_13),m8_fp_bits32(s_6),0));}
/* 1751c: vfma.f32 s15, s11, s12 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_11,s_12,s_15);}
/* 17520: vminnm.f32 s13, s13, s8 */ {s_13=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_13),m8_fp_bits32(s_8),0));}
/* 17524: vmaxnm.f32 s12, s12, s6 */ {s_12=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_12),m8_fp_bits32(s_6),0));}
/* 17528: vabs.f32 s5, s13 */ {s_5=m8_fp_float(m8_fp_bits32(s_13)&0x7fffffffu);}
/* 1752c: vminnm.f32 s12, s12, s8 */ {s_12=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_12),m8_fp_bits32(s_8),0));}
/* 17530: vldr s11, [pc, #-0xb0] */ {s_11=m8_fp_float(m8_le_read(lit17470+20u,4));}
/* 17534: vabs.f32 s2, s12 */ {s_2=m8_fp_float(m8_fp_bits32(s_12)&0x7fffffffu);}
/* 17538: vfms.f32 s4, s5, s11 */ {s_4=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_5)^0x80000000u),s_11,s_4);}
/* 1753c: vmaxnm.f32 s14, s14, s6 */ {s_14=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_14),m8_fp_bits32(s_6),0));}
/* 17540: vminnm.f32 s14, s14, s8 */ {s_14=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_14),m8_fp_bits32(s_8),0));}
/* 17544: vmaxnm.f32 s15, s15, s6 */ {s_15=m8_fp_float(m8_hvf_op(cpu,M8_FP_MAXNM,m8_fp_bits32(s_15),m8_fp_bits32(s_6),0));}
/* 17548: vmov.f32 s6, s17 */ {s_6=s_17;}
/* 1754c: vminnm.f32 s15, s15, s8 */ {s_15=m8_fp_float(m8_hvf_op(cpu,M8_FP_MINNM,m8_fp_bits32(s_15),m8_fp_bits32(s_8),0));}
/* 17550: vabs.f32 s8, s14 */ {s_8=m8_fp_float(m8_fp_bits32(s_14)&0x7fffffffu);}
/* 17554: vabs.f32 s3, s15 */ {s_3=m8_fp_float(m8_fp_bits32(s_15)&0x7fffffffu);}
/* 17558: vmov.f32 s5, s17 */ {s_5=s_17;}
/* 1755c: vfms.f32 s6, s8, s11 */ {s_6=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_8)^0x80000000u),s_11,s_6);}
/* 17560: vmov.f32 s8, s17 */ {s_8=s_17;}
/* 17564: vfms.f32 s5, s2, s11 */ {s_5=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_2)^0x80000000u),s_11,s_5);}
/* 17568: vmul.f32 s13, s13, s4 */ {s_13=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_13,s_4,0);}
/* 1756c: vfms.f32 s8, s3, s11 */ {s_8=m8_hvf_arithmetic(cpu,M8_FP_FMA,m8_fp_float(m8_fp_bits32(s_3)^0x80000000u),s_11,s_8);}
/* 17570: vmul.f32 s14, s14, s6 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_14,s_6,0);}
/* 17574: vstr s13, [r8, #0xa8] */ {m8_le_write(object+168u,4,m8_fp_bits32(s_13));}
/* 17578: vmul.f32 s12, s12, s5 */ {s_12=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_12,s_5,0);}
/* 1757c: vstr s14, [r8, #0xb0] */ {m8_le_write(object+176u,4,m8_fp_bits32(s_14));}
/* 17580: vmul.f32 s15, s15, s8 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_15,s_8,0);}
/* 17584: vstr s12, [r8, #0xac] */ {m8_le_write(object+172u,4,m8_fp_bits32(s_12));}
/* 17588: vstr s15, [r8, #0xb4] */ {m8_le_write(object+180u,4,m8_fp_bits32(s_15));}
/* 1758c/17590/17596: filter tail dispatch. Admitted filters select the
 * fall-through (filter 1) or the 1774e remap (filter 2); anything else
 * resumes translated at the dispatch. */
{m8_sub_flags(cpu,r_3,0x3u,0u);
 if(m8_condition(cpu->xpsr,1)) {M8_HVF_SAVE(output);output->r[15]=0x1758cu;return M8_AOT_EXIT;}
 {m8_sub_flags(cpu,r_3,0x4u,0u);}
 if(m8_condition(cpu->xpsr,1)) {M8_HVF_SAVE(output);output->r[15]=0x17590u;return M8_AOT_EXIT;}
 {m8_sub_flags(cpu,r_3,0x2u,0u);}
 if(m8_condition(cpu->xpsr,1)) {s_15=s_7;s_14=s_9;goto L_1759c;}
 if(r_3!=1u) {M8_HVF_SAVE(output);output->r[15]=0x17596u;return M8_AOT_EXIT;}}
/* 1759c: second biquad stage shared by filters 1..4. */
L_1759c:{s_16=m8_fp_float(m8_le_read(object+184u,4));}
/* 175a0: vldr s12, [r8, #0xbc] */ {s_12=m8_fp_float(m8_le_read(object+188u,4));}
/* 175a4: vldr s20, [r8, #0xc0] */ {s_20=m8_fp_float(m8_le_read(object+192u,4));}
/* 175a8: vldr s13, [r8, #0xc4] */ {s_13=m8_fp_float(m8_le_read(object+196u,4));}
/* 175ac: vfnms.f32 s16, s12, s10 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_12,s_10,m8_fp_float(m8_fp_bits32(s_16)^0x80000000u));}
/* 175b0: vstr s14, [r8, #0xb8] */ {m8_le_write(object+184u,4,m8_fp_bits32(s_14));}
/* 175b4: vfnms.f32 s20, s13, s10 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_FMA,s_13,s_10,m8_fp_float(m8_fp_bits32(s_20)^0x80000000u));}
/* 175b8: vstr s15, [r8, #0xc0] */ {m8_le_write(object+192u,4,m8_fp_bits32(s_15));}
/* 175bc: vadd.f32 s16, s14, s16 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_16,0);}
/* 175c0: vadd.f32 s20, s20, s15 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_20,s_15,0);}
/* 175c4: vstr s16, [r8, #0xbc] */ {m8_le_write(object+188u,4,m8_fp_bits32(s_16));}
/* 175c8: vstr s20, [r8, #0xc4] */ {m8_le_write(object+196u,4,m8_fp_bits32(s_20));}
/* 175cc: b #0x17292; shape dispatch reuses the admitted shape path. */
{m8_sub_flags(cpu,r_2,0x3u,0u);
 if(!m8_condition(cpu->xpsr,10)) {M8_HVF_SAVE(output);output->r[15]=0x17292u;return M8_AOT_EXIT;}
 goto L_172ca;}
/* 172ca: output tail. The ring index, cursor update and stereo stores. */
L_172ca:{s_13=m8_fp_float(m8_le_read(object+68u,4));}
/* 172ce: vldr s14, [pc, #0x1a0] */ {s_14=m8_fp_float(m8_le_read(lit17470+0u,4));}
/* 172d2: vldr s15, [pc, #0x1a0] */ {s_15=m8_fp_float(m8_le_read(lit17470+4u,4));}
/* 172d6: vcmpe.f32 s13, s14 */ {m8_fp_cmp32(cpu,m8_fp_bits32(s_13),m8_fp_bits32(s_14),1);}
/* 172da: vmul.f32 s15, s16, s15 */ {s_15=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_16,s_15,0);}
/* 172de: vmrs apsr_nzcv, fpscr */ {cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);}
/* 172e2: vcvt.s32.f32 s15, s15 */ {s_15=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_15),0));}
/* 172e6: bpl #0x1734e */ {if(m8_condition(cpu->xpsr,6)) {goto L_1734e;}}
/* 172e8: vcvt.u32.f32 s14, s13 */ {s_14=m8_fp_float(m8_hvf_to_int(cpu,m8_fp_bits32(s_13),1));}
/* 172ec: vmov r3, s14 */ {r_3=m8_fp_bits32(s_14);}
/* 172f0: uxth r3, r3 */ {r_3&=65535u;}
/* 172f2: lsls r3, r3, #1 */ {uint32_t carry=(cpu->xpsr>>29)&1u;r_3=m8_shift(r_3,2,0x1u,&carry);m8_logic_flags(cpu,r_3,carry);}
/* 172f4: vldr s14, [r8, #0x48] */ L_172f4:{s_14=m8_fp_float(m8_le_read(object+72u,4));}
/* 172f8: vmov r1, s15 */ {r_1=m8_fp_bits32(s_15);}
/* 172fc: ldr.w r2, [r8, #0xe8] */ {r_2=m8_le_read(object+232u,4);}
/* 17300: vmul.f32 s16, s16, s25 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_16,s_25,0);}
/* 17304: vadd.f32 s14, s14, s13 */ {s_14=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_13,0);}
/* 17308: vmul.f32 s20, s24, s20 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_24,s_20,0);}
/* 1730c: strh r1, [r2, r3] */ {m8_le_write(ctx.ring+r_3,2,r_1);}
/* 1730e: vstr s14, [r8, #0x44] */ {m8_le_write(object+68u,4,m8_fp_bits32(s_14));}
/* 17312: vldr s15, [r6, #0x2c] */ {s_15=m8_fp_float(m8_le_read(control+44u,4));}
/* 17316: vmul.f32 s16, s16, s15 */ {s_16=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_16,s_15,0);}
/* 1731a: vmul.f32 s20, s20, s15 */ {s_20=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_20,s_15,0);}
/* 1731e: vstmia sb!, {s16} */ {m8_le_write(ctx.left+(r_9-ctx.leftAddr),4,m8_fp_bits32(s_16));r_9+=4u;}
/* 17322: vstmia fp!, {s20} */ {m8_le_write(ctx.right+(r_11-ctx.rightAddr),4,m8_fp_bits32(s_20));r_11+=4u;}
/* 17326: cmp fp, sl */ {m8_sub_flags(cpu,r_11,r_10,0u);}
/* 17328: beq.w #0x17730 */ {if(m8_condition(cpu->xpsr,1)) {goto L_17730;}}
 goto L_1732c;
/* 1734e: cursor-wrap path. */
L_1734e:{s_13=s_23;}
/* 17352: movs r3, #0 */ {uint32_t carry=(cpu->xpsr>>29)&1u;m8_logic_flags(cpu,0u,carry);r_3=0u;}
/* 17354: b #0x172f4 */ {goto L_172f4;}
/* 17356: lane-mask stubs rejoin the accumulation. */
L_17356:{s_13=s_23;}
/* 1735a: vmov.f32 s14, s23 */ {s_14=s_23;}
/* 1735e: b #0x171e0 */ {goto L_171e0;}
L_17360:{s_14=s_23;}
/* 17364: vmov.f32 s11, s23 */ {s_11=s_23;}
/* 17368: b #0x171c6 */ {goto L_171c6;}
L_1736a:{s_3=s_23;}
/* 1736e: vmov.f32 s11, s23 */ {s_11=s_23;}
/* 17372: b #0x171ac */ {goto L_171ac;}
L_17374:{s_11=s_23;}
/* 17378: vmov.f32 s8, s23 */ {s_8=s_23;}
/* 1737c: b #0x17192 */ {goto L_17192;}
L_1737e:{s_7=s_23;}
/* 17382: vmov.f32 s8, s23 */ {s_8=s_23;}
/* 17386: b #0x17178 */ {goto L_17178;}
/* 17418: interpolation-underflow path. */
L_17418:{s_24=m8_hvf_arithmetic(cpu,M8_FP_ADD,s_14,s_14,0);}
/* 1741c: vmov.f32 s25, #1.000000e+00 */ {s_25=m8_fp_float(0x3f800000u);}
/* 17420: b.w #0x16a88 */ {goto L_16a88;}
/* 175ce: control update. Full publish, native call, requalify. */
L_175ce: r_0=r_8; r_14=0x175d5u; r_15=0x15da4u;
M8_HVF_SAVE(cpu);
{int status=0, trips=0, done_update=0;
 for(;;) {
  status=update(cpu,mem);
  if(status!=M8_AOT_EXIT) break;
  if(state.r[15]==0x175d4u) { done_update=1; M8_HVF_LOAD(cpu); break; }
  if(state.r[15]!=0x24a0u) break;
  /* The update tail-branches to the shared 0x24a0 store stub on some
   * voices. Run it natively; its return often resumes the update itself
   * (r14 points back into 15da4), which the loop follows. */
  M8_HVF_LOAD(cpu);
  {uint8_t *lit=m8_region_pointer(&mem->itcm,0x24b4u,4);
   uint8_t *r0host=m8_region_pointer(&mem->dtcm,r_0,14u);
   if(!r0host) r0host=m8_region_pointer(&mem->ocram,r_0,14u);
   if(!lit || !r0host) {
    M8_HVF_SAVE(output);
    output->r[15]=!lit?0x24a0u:0x24aau;
    return M8_AOT_FAULT;
   }
   s_15=m8_fp_float(m8_le_read(lit,4));
   {uint32_t carry=(cpu->xpsr>>29)&1u;m8_logic_flags(cpu,0x1u,carry);r_3=0x1u;}
   s_0=m8_hvf_arithmetic(cpu,M8_FP_MUL,s_0,s_15,0);
   m8_le_write(r0host+13u,1,r_3);
   m8_le_write(r0host+8u,4,m8_fp_bits32(s_0));}
  r_15=r_14&~1u;
  if(r_15==0x175d4u) { done_update=1; break; }
  if(r_15>=0x15da4u && r_15<0x168d2u && trips<8) { trips++; M8_HVF_SAVE(cpu); continue; }
  M8_HVF_SAVE(output); output->r[15]=r_15; return M8_AOT_EXIT;
 }
 if(!done_update) {*output=*cpu;return status;}
 if(r_8!=root || r_13!=frame || ((r_10-r_11)&3u) || !m8_hvf_qualify(cpu,mem,&ctx,r_8,r_9,r_11,(r_10-r_11)/4u,0)) {
  r_15=0x175d4u;goto done;
 }
 /* Requalification may have moved the render spans; refresh the cached
  * pointers before further effects. */
 object=ctx.object;control=ctx.control;framePtr=ctx.frame;
 lit16998=ctx.lit16998;lit17470=ctx.lit17470;
}
/* 175d4: ldrb.w r3, [r8, #0x54] */ {r_3=object[0x54];}
/* 175d8: b #0x17336 */ {goto L_17336;}
/* 17730: normal block end. */
L_17730: {uint32_t carry=(cpu->xpsr>>29)&1u;m8_logic_flags(cpu,0x1u,carry);r_0=0x1u;}
/* 17732: epilogue. */
L_17732: {s_16=m8_fp_float(m8_le_read(framePtr+0u,4));s_17=m8_fp_float(m8_le_read(framePtr+4u,4));
 s_18=m8_fp_float(m8_le_read(framePtr+8u,4));s_19=m8_fp_float(m8_le_read(framePtr+12u,4));
 s_20=m8_fp_float(m8_le_read(framePtr+16u,4));s_21=m8_fp_float(m8_le_read(framePtr+20u,4));
 s_22=m8_fp_float(m8_le_read(framePtr+24u,4));s_23=m8_fp_float(m8_le_read(framePtr+28u,4));
 s_24=m8_fp_float(m8_le_read(framePtr+32u,4));s_25=m8_fp_float(m8_le_read(framePtr+36u,4));
 r_13+=40u;}
{uint32_t v0=m8_le_read(framePtr+40u,4),v1=m8_le_read(framePtr+44u,4),
 v2=m8_le_read(framePtr+48u,4),v3=m8_le_read(framePtr+52u,4),
 v4=m8_le_read(framePtr+56u,4),v5=m8_le_read(framePtr+60u,4),
 v6=m8_le_read(framePtr+64u,4),v7=m8_le_read(framePtr+68u,4),
 v8=m8_le_read(framePtr+72u,4),v9=m8_le_read(framePtr+76u,4);
 r_13+=40u;r_3=v0;r_4=v1;r_5=v2;r_6=v3;r_7=v4;r_8=v5;r_9=v6;r_10=v7;r_11=v8;r_15=v9&~1u;goto done;}

done: M8_HVF_SAVE(output);return M8_AOT_EXIT;
}
#undef M8_HVF_SAVE
#undef M8_HVF_LOAD
#undef M8_HVF_NOINLINE
#endif
#endif
#endif
