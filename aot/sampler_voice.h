/* Headless 6.5.2C unfiltered mono16 SAMPLER render blocks.
 * Exact entry 1caa8, 2..64 samples. Filter/shape/slice/fade modes stay
 * scalar. Forward loop mode is admitted while the
 * original 1cc2c wrap/reset boundary remains scalar. Keep the operation, lane
 * and store order and stack bytes.
 * The caller supplies unchanged native fn1c414 and fn1b938, an RN/FZ/DN
 * environment, and stable fixed-map descriptors through each native callback.
 * Qualify object, frame, cache, ring, outputs and literals before effects.
 * Requalify after expected control/refill returns; changed assumptions exit
 * at 1d442/1cd08. Other child results preserve their actual PC and stores.
 * The stationary 1b938 path retains its original PUSH/POP and DP temporaries;
 * its other paths call the unchanged method at continuation 1b990.
 * Negative means unchanged decline; every other exit publishes all 50 CPU
 * words. One-sample calls must skip this helper before the caller bridge.
 * No guest data is cached and no arithmetic is approximated.
 * M8_AOT_SAMPLER_EXTENDED is a default-off candidate. It adds
 * original PCM24, stereo, interpolation/slice and filter 0..5 instructions.
 * Packed sample reads and dynamic slice-table reads have separate bounds.
 * M8_WEB_SAMPLER_EXTENDED retains the compact-web spelling of that option.
 * M8_AOT_SAMPLER_POSITION_FP adds bounded native position arithmetic within
 * the extension. Other modes and values retain the original FP helpers.
 * M8_AOT_SAMPLER_SHAPE admits shape 5 and the pinned 1fa4/1fec methods.
 */
#ifndef M8_AOT_SAMPLER_VOICE_H
#define M8_AOT_SAMPLER_VOICE_H
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && (defined(M8_AOT_FIXED_MEMORY) || (defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES))) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#if defined(M8_AOT_SAMPLER_EXTENDED) || defined(M8_WEB_SAMPLER_EXTENDED)
#define M8_SV_EXTENDED
#endif
#if defined(M8_SV_EXTENDED) && (defined(M8_AOT_SAMPLER_SHAPE) || defined(M8_WEB_SAMPLER_SHAPE))
#define M8_SV_SHAPE
#endif
#include "filter_kernel.h"
#include "integer.h"
#include "canonical_memory.h"
#if defined(M8_SV_EXTENDED) && (defined(M8_AOT_SAMPLER_POSITION_FP) || defined(M8_WEB_SAMPLER_POSITION_FP))
#include "sampler_position_fp.h"
#define M8_SV_POSITION_FP
#endif
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
#include "compact_voices.h"
#endif
#if defined(__GNUC__) || defined(__clang__)
#define M8_SV_NOINLINE __attribute__((noinline))
#else
#define M8_SV_NOINLINE
#endif
static inline int m8_sampler_voice_regions(const m8_cpu *cpu,const m8_mem *mem) {
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++)
        if(!m8_filter_disjoint((uintptr_t)regions[i]->host,regions[i]->size,(uintptr_t)cpu,sizeof(*cpu)))return 0;
    return 1;
}
static M8_SV_NOINLINE int m8_sampler_voice_qualify(const m8_cpu *cpu,m8_mem *mem,
    uint32_t root,uint32_t left,uint32_t right,uint32_t count,int initial) {
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uintptr_t arena=m8_compact_voice_memory_offset(mem);
#else
    uintptr_t arena=m8_canonical_memory_offset(mem);
#endif
    if(!arena || !m8_sampler_voice_regions(cpu,mem) || m8_current_it ||
       !m8_fp_native_supported(cpu) || (cpu->xpsr&0x0600fc00u) || !count || count>64u ||
       (initial && cpu->r[15]!=0x1caa8u))return 0;
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
    uint8_t *object=m8_compact_voice_data_pointer(arena,root,480);
#else
    uint8_t *object=m8_canonical_offset_pointer(arena,root,480);
#endif
    if(!object || (uint32_t)(root-0x20000000u)>0x80000u-480u ||
#ifdef M8_SV_EXTENDED
       object[0x40]>5 ||
#ifdef M8_SV_SHAPE
       (object[0x41] && object[0x41]!=5) ||
#else
       object[0x41] ||
#endif
       (object[0x54]!=16 && object[0x54]!=24) ||
#else
       object[0x40] || object[0x41] || object[0x54]!=16 || object[0x57] ||
#endif
       (object[0x59] && (object[0x59]!=1 || object[0x5a] || object[0x5c])) ||
#ifdef M8_SV_EXTENDED
       object[0x1ac] || !object[0x1de] || object[0xcc]>7)return 0;
#else
       object[0x63] || object[0x1ac] || !object[0x1de] || object[0xcc]>7)return 0;
#endif
#ifdef M8_SV_SHAPE
    uint32_t address[]={root,cpu->r[13]-(initial?152u:16u),m8_le_read(object+0xbc,4),
        m8_le_read(object+0xc8,4),left,right,m8_le_read(object+0xb4,4),
        m8_le_read(object+0x50,4),0,0x1cdc8u,0x2070u};
    uint32_t size[]={480,152,16384,640,count*4u,count*4u,124,4,4,5196,12};
    enum {spans=sizeof address/sizeof *address};
    uint8_t *pointer[spans];
    for(unsigned i=0;i<spans;i++) {
#else
    uint32_t address[]={root,cpu->r[13]-(initial?152u:16u),m8_le_read(object+0xbc,4),
        m8_le_read(object+0xc8,4),left,right,m8_le_read(object+0xb4,4),
        m8_le_read(object+0x50,4),0,0x1cdc8u};
    uint32_t size[]={480,152,16384,640,count*4u,count*4u,124,4,4,5196};
    uint8_t *pointer[10];
    for(unsigned i=0;i<10;i++) {
#endif
        if(i==8)address[i]=m8_le_read(pointer[7],4);
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
        pointer[i]=((i==9u && address[i]==0x1cdc8u && size[i]==5196u)
#ifdef M8_SV_SHAPE
            || (i==10u && address[i]==0x2070u && size[i]==12u)
#endif
            ) ?
            m8_compact_voice_pointer(arena,address[i],size[i]) :
            m8_compact_voice_data_pointer(arena,address[i],size[i]);
#else
        pointer[i]=m8_canonical_offset_pointer(arena,address[i],size[i]);
#endif
        if(!pointer[i] || (address[i]&3u))return 0;
        if(i<6 && !m8_region_pointer(&mem->dtcm,address[i],size[i]) &&
                  !m8_region_pointer(&mem->ocram,address[i],size[i]))return 0;
        for(unsigned j=0;j<i;j++)
            if(!m8_filter_disjoint((uintptr_t)pointer[i],size[i],(uintptr_t)pointer[j],size[j]))return 0;
    }
#ifdef M8_SV_EXTENDED
    /* PCM24 uses a separate packed-byte view. Both cache views are read-only
     * between callbacks, so they may overlap each other. Keep them disjoint
     * from every other qualified span, including all written state. */
    if(object[0x54]==24) {
        uint32_t packed=m8_le_read(object+0xc4,4);
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
        uint8_t *data=m8_compact_voice_data_pointer(arena,packed,16384);
#else
        uint8_t *data=m8_canonical_offset_pointer(arena,packed,16384);
#endif
        if(!data || (!m8_region_pointer(&mem->dtcm,packed,16384) &&
                     !m8_region_pointer(&mem->ocram,packed,16384)))return 0;
#ifdef M8_SV_SHAPE
        for(unsigned i=0;i<spans;i++)
#else
        for(unsigned i=0;i<10;i++)
#endif
            if(i!=2 && !m8_filter_disjoint((uintptr_t)data,16384,(uintptr_t)pointer[i],size[i]))return 0;
    }
#endif
#ifdef M8_SV_SHAPE
    return !m8_le_read(pointer[6]+0x78,4) && (m8_le_read(pointer[8],4)==0x1fa5u ||
           m8_le_read(pointer[8],4)==0x1fedu) &&
#else
    return !m8_le_read(pointer[6]+0x78,4) && m8_le_read(pointer[8],4)==0x1fa5u &&
#endif
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
           m8_le_read(m8_compact_voice_pointer(arena,0x1d484,4),4)==0x43a00000u;
#else
           m8_le_read(m8_canonical_offset_pointer(arena,0x1d484,4),4)==0x43a00000u;
#endif
}

static M8_SV_NOINLINE int m8_sampler_voice_kernel(m8_cpu *output,m8_mem *mem,m8_aot_fn update,m8_aot_fn receive) {
m8_cpu state,*cpu=&state;
if(output->r[3]<=1u || !update || !receive || !m8_sampler_voice_regions(cpu,mem) || !m8_sampler_voice_qualify(output,mem,output->r[0],output->r[1],output->r[2],output->r[3],1))return -1;
uintptr_t arena=(uintptr_t)mem->itcm.host;uint32_t root=output->r[0],frame=output->r[13]-136u;state=*output;
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
#if defined(M8_AOT_COMPACT_MEMORY) && defined(M8_AOT_COMPACT_VOICES) && !defined(M8_AOT_FIXED_MEMORY)
#define M8_SV_HOST(addr) m8_compact_voice_host(arena,(uint32_t)(addr))
#else
#define M8_SV_HOST(addr) ((uint8_t *)(arena+(uintptr_t)(uint32_t)(addr)))
#endif
#define M8_SV_SAVE(dst) do {(dst)->r[0]=r_0; (dst)->r[1]=r_1; (dst)->r[2]=r_2; (dst)->r[3]=r_3; (dst)->r[4]=r_4; (dst)->r[5]=r_5; (dst)->r[6]=r_6; (dst)->r[7]=r_7; (dst)->r[8]=r_8; (dst)->r[9]=r_9; (dst)->r[10]=r_10; (dst)->r[11]=r_11; (dst)->r[12]=r_12; (dst)->r[13]=r_13; (dst)->r[14]=r_14; (dst)->r[15]=r_15; (dst)->s[0]=m8_fp_bits32(s_0); (dst)->s[1]=m8_fp_bits32(s_1); (dst)->s[2]=m8_fp_bits32(s_2); (dst)->s[3]=m8_fp_bits32(s_3); (dst)->s[4]=m8_fp_bits32(s_4); (dst)->s[5]=m8_fp_bits32(s_5); (dst)->s[6]=m8_fp_bits32(s_6); (dst)->s[7]=m8_fp_bits32(s_7); (dst)->s[8]=m8_fp_bits32(s_8); (dst)->s[9]=m8_fp_bits32(s_9); (dst)->s[10]=m8_fp_bits32(s_10); (dst)->s[11]=m8_fp_bits32(s_11); (dst)->s[12]=m8_fp_bits32(s_12); (dst)->s[13]=m8_fp_bits32(s_13); (dst)->s[14]=m8_fp_bits32(s_14); (dst)->s[15]=m8_fp_bits32(s_15); (dst)->s[16]=m8_fp_bits32(s_16); (dst)->s[17]=m8_fp_bits32(s_17); (dst)->s[18]=m8_fp_bits32(s_18); (dst)->s[19]=m8_fp_bits32(s_19); (dst)->s[20]=m8_fp_bits32(s_20); (dst)->s[21]=m8_fp_bits32(s_21); (dst)->s[22]=m8_fp_bits32(s_22); (dst)->s[23]=m8_fp_bits32(s_23); (dst)->s[24]=m8_fp_bits32(s_24); (dst)->s[25]=m8_fp_bits32(s_25); (dst)->s[26]=m8_fp_bits32(s_26); (dst)->s[27]=m8_fp_bits32(s_27); (dst)->s[28]=m8_fp_bits32(s_28); (dst)->s[29]=m8_fp_bits32(s_29); (dst)->s[30]=m8_fp_bits32(s_30); (dst)->s[31]=m8_fp_bits32(s_31); (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr;} while(0)
#define M8_SV_LOAD(src) do {r_0=(src)->r[0]; r_1=(src)->r[1]; r_2=(src)->r[2]; r_3=(src)->r[3]; r_4=(src)->r[4]; r_5=(src)->r[5]; r_6=(src)->r[6]; r_7=(src)->r[7]; r_8=(src)->r[8]; r_9=(src)->r[9]; r_10=(src)->r[10]; r_11=(src)->r[11]; r_12=(src)->r[12]; r_13=(src)->r[13]; r_14=(src)->r[14]; r_15=(src)->r[15]; s_0=m8_fp_float((src)->s[0]); s_1=m8_fp_float((src)->s[1]); s_2=m8_fp_float((src)->s[2]); s_3=m8_fp_float((src)->s[3]); s_4=m8_fp_float((src)->s[4]); s_5=m8_fp_float((src)->s[5]); s_6=m8_fp_float((src)->s[6]); s_7=m8_fp_float((src)->s[7]); s_8=m8_fp_float((src)->s[8]); s_9=m8_fp_float((src)->s[9]); s_10=m8_fp_float((src)->s[10]); s_11=m8_fp_float((src)->s[11]); s_12=m8_fp_float((src)->s[12]); s_13=m8_fp_float((src)->s[13]); s_14=m8_fp_float((src)->s[14]); s_15=m8_fp_float((src)->s[15]); s_16=m8_fp_float((src)->s[16]); s_17=m8_fp_float((src)->s[17]); s_18=m8_fp_float((src)->s[18]); s_19=m8_fp_float((src)->s[19]); s_20=m8_fp_float((src)->s[20]); s_21=m8_fp_float((src)->s[21]); s_22=m8_fp_float((src)->s[22]); s_23=m8_fp_float((src)->s[23]); s_24=m8_fp_float((src)->s[24]); s_25=m8_fp_float((src)->s[25]); s_26=m8_fp_float((src)->s[26]); s_27=m8_fp_float((src)->s[27]); s_28=m8_fp_float((src)->s[28]); s_29=m8_fp_float((src)->s[29]); s_30=m8_fp_float((src)->s[30]); s_31=m8_fp_float((src)->s[31]); cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr;} while(0)
#define M8_SV_SET_D4(value) do {uint64_t v=(value);s_8=m8_fp_float((uint32_t)v);s_9=m8_fp_float((uint32_t)(v>>32));} while(0)
#define M8_SV_SET_D5(value) do {uint64_t v=(value);s_10=m8_fp_float((uint32_t)v);s_11=m8_fp_float((uint32_t)(v>>32));} while(0)
#define M8_SV_SET_D6(value) do {uint64_t v=(value);s_12=m8_fp_float((uint32_t)v);s_13=m8_fp_float((uint32_t)(v>>32));} while(0)
#define M8_SV_SET_D7(value) do {uint64_t v=(value);s_14=m8_fp_float((uint32_t)v);s_15=m8_fp_float((uint32_t)(v>>32));} while(0)
#ifdef M8_SV_SHAPE
#define M8_SV_METHOD(resume,pc) do { \
    if(r_3!=0x1fa5u && r_3!=0x1fedu) {r_15=(pc);goto done;} \
    r_14=(resume)|1u;r_15=r_3&~1u; \
    if(r_3==0x1fa5u)goto L_1fa4;goto L_1fec; \
} while(0)
#endif
#ifdef M8_SV_POSITION_FP
/* Preserve the caller's native or software FP bridge after this kernel. */
#pragma push_macro("m8_fp_op64")
#pragma push_macro("m8_fp_from_int")
#pragma push_macro("m8_fp_convert")
#pragma push_macro("m8_fp_to_int")
#pragma push_macro("m8_fp_cmp64")
#undef m8_fp_op64
#undef m8_fp_from_int
#undef m8_fp_convert
#undef m8_fp_to_int
#undef m8_fp_cmp64
#define m8_fp_op64 m8_sampler_fp_op64
#define m8_fp_from_int m8_sampler_fp_from_int
#define m8_fp_convert m8_sampler_fp_convert
#define m8_fp_to_int m8_sampler_fp_to_int
#define m8_fp_cmp64 m8_sampler_fp_cmp64
#endif
goto L_1caa8;
L_1caa8:
/* 1caa8: push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr} */ { { uint32_t base = r_13; uint32_t addr = base + 4294967260u; uint32_t value0 = r_4; uint32_t value1 = r_5; uint32_t value2 = r_6; uint32_t value3 = r_7; uint32_t value4 = r_8; uint32_t value5 = r_9; uint32_t value6 = r_10; uint32_t value7 = r_11; uint32_t value8 = r_14; m8_le_write(M8_SV_HOST(addr + 0u),4,value0); m8_le_write(M8_SV_HOST(addr + 4u),4,value1); m8_le_write(M8_SV_HOST(addr + 8u),4,value2); m8_le_write(M8_SV_HOST(addr + 12u),4,value3); m8_le_write(M8_SV_HOST(addr + 16u),4,value4); m8_le_write(M8_SV_HOST(addr + 20u),4,value5); m8_le_write(M8_SV_HOST(addr + 24u),4,value6); m8_le_write(M8_SV_HOST(addr + 28u),4,value7); m8_le_write(M8_SV_HOST(addr + 32u),4,value8); r_13 = base - 36u; } }
/* 1caac: mov r8, r3 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_3; uint32_t result = b; r_8 = (uint32_t)(result); } }
/* 1caae: ldrb.w r3, [r0, #0x1de] */ { { uint32_t base = r_0; uint32_t addr = base + (478u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1cab2: mov r4, r0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_0; uint32_t result = b; r_4 = (uint32_t)(result); } }
/* 1cab4: mov fp, r2 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_2; uint32_t result = b; r_11 = (uint32_t)(result); } }
/* 1cab6: vpush {d8, d9, d10, d11, d12, d13, d14, d15} */ { { uint32_t fp_addr = (r_13 - 64u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); m8_le_write(M8_SV_HOST(fp_addr + 4u),4,m8_fp_bits32(s_17)); m8_le_write(M8_SV_HOST(fp_addr + 8u),4,m8_fp_bits32(s_18)); m8_le_write(M8_SV_HOST(fp_addr + 12u),4,m8_fp_bits32(s_19)); m8_le_write(M8_SV_HOST(fp_addr + 16u),4,m8_fp_bits32(s_20)); m8_le_write(M8_SV_HOST(fp_addr + 20u),4,m8_fp_bits32(s_21)); m8_le_write(M8_SV_HOST(fp_addr + 24u),4,m8_fp_bits32(s_22)); m8_le_write(M8_SV_HOST(fp_addr + 28u),4,m8_fp_bits32(s_23)); m8_le_write(M8_SV_HOST(fp_addr + 32u),4,m8_fp_bits32(s_24)); m8_le_write(M8_SV_HOST(fp_addr + 36u),4,m8_fp_bits32(s_25)); m8_le_write(M8_SV_HOST(fp_addr + 40u),4,m8_fp_bits32(s_26)); m8_le_write(M8_SV_HOST(fp_addr + 44u),4,m8_fp_bits32(s_27)); m8_le_write(M8_SV_HOST(fp_addr + 48u),4,m8_fp_bits32(s_28)); m8_le_write(M8_SV_HOST(fp_addr + 52u),4,m8_fp_bits32(s_29)); m8_le_write(M8_SV_HOST(fp_addr + 56u),4,m8_fp_bits32(s_30)); m8_le_write(M8_SV_HOST(fp_addr + 60u),4,m8_fp_bits32(s_31)); r_13 -= 64u; } }
/* 1caba: sub sp, #0x24 */ { { uint32_t a = r_13; uint32_t b = 0x24u; uint32_t result = a - b - 0u; r_13 = (uint32_t)(result); } }
/* 1cabc: str r1, [sp, #0x14] */ { { uint32_t base = r_13; uint32_t addr = base + (20u); uint32_t value = r_1; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1cabe: str r3, [sp, #8] */ { { uint32_t base = r_13; uint32_t addr = base + (8u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1cac0: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cac2: beq.w #0x1df60 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1df60; } }
/* 1cac6: vldr s14, [r0, #0x188] */ { { uint32_t fp_addr = (r_0 + 392u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1caca: vcvt.u32.f32 s15, s14 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 1, 3, 32, 0)); }
/* 1cace: vmov r3, s15 */ { r_3 = m8_fp_bits32(s_15); }
/* 1cad2: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
/* 1cad4: vmov s15, r3 */ { s_15=m8_fp_float(r_3); }
/* 1cad8: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
/* 1cadc: vsub.f32 s14, s14, s15 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_15,0); }
/* 1cae0: vcmp.f32 s14, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), 0, 0); }
/* 1cae4: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cae8: bne.w #0x1d2c4 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1d2c4; } }
/* 1caec: movs r3, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 1caee: str r3, [sp, #4] */ { { uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
L_1caf0:
/* 1caf0: ldr.w r3, [r4, #0xb4] */ { { uint32_t base = r_4; uint32_t addr = base + (180u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1caf4: ldr r3, [r3, #0x78] */ { { uint32_t base = r_3; uint32_t addr = base + (120u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1caf6: cbz r3, #0x1cb08 */ { if (r_3 == 0) { goto L_1cb08; } }
/* 1caf8: vldr s15, [r4, #0xf4] */ { { uint32_t fp_addr = (r_4 + 244u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1cafc: vcmp.f32 s15, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), 0, 0); }
/* 1cb00: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cb04: beq.w #0x1e192 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1e192; } }
L_1cb08:
/* 1cb08: ldr r3, [sp, #8] */ { { uint32_t base = r_13; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1cb0a: str r3, [sp, #0x1c] */ { { uint32_t base = r_13; uint32_t addr = base + (28u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1cb0c: ldrb.w r3, [r4, #0x54] */ { { uint32_t base = r_4; uint32_t addr = base + (84u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1cb10: vmov.f32 s28, #1.000000e+00 */ { s_28=m8_fp_float(0x3f800000u); }
/* 1cb14: vldr s15, [pc, #0x2d8] */ { { uint32_t fp_addr = (0x1cb18u + 728u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1cb18: cmp r3, #0x20 */ { { uint32_t a = r_3; uint32_t b = 0x20u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cb1a: vseleq.f32 s28, s28, s15 */ { s_28=m8_fp_float(((cpu->xpsr & 0x40000000u)) ? m8_fp_bits32(s_28) : m8_fp_bits32(s_15)); }
/* 1cb1e: cmp.w r8, #0 */ { { uint32_t a = r_8; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cb22: beq.w #0x1d2b8 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d2b8; } }
/* 1cb26: vldr s17, [pc, #0x2d4] */ { { uint32_t fp_addr = (0x1cb28u + 724u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_17=m8_fp_float(fp_value); } }
/* 1cb2a: movs r5, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_5 = (uint32_t)(result); } }
/* 1cb2c: vldr s29, [pc, #0x2c4] */ { { uint32_t fp_addr = (0x1cb30u + 708u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_29=m8_fp_float(fp_value); } }
/* 1cb30: ldr.w sl, [sp, #0x14] */ { { uint32_t base = r_13; uint32_t addr = base + (20u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_10 = (uint32_t)(value); } }
/* 1cb34: str.w fp, [sp] */ { { uint32_t base = r_13; uint32_t addr = base + (0u); uint32_t value = r_11; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1cb38: vldr d13, [pc, #0x28c] */ { { uint32_t fp_addr = (0x1cb3cu + 652u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_26=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_27=m8_fp_float(fp_value); } }
/* 1cb3c: vldr d9, [pc, #0x290] */ { { uint32_t fp_addr = (0x1cb40u + 656u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_18=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_19=m8_fp_float(fp_value); } }
/* 1cb40: vldr d10, [pc, #0x294] */ { { uint32_t fp_addr = (0x1cb44u + 660u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_21=m8_fp_float(fp_value); } }
/* 1cb44: vldr d12, [pc, #0x298] */ { { uint32_t fp_addr = (0x1cb48u + 664u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_24=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_25=m8_fp_float(fp_value); } }
/* 1cb48: vldr d11, [pc, #0x29c] */ { { uint32_t fp_addr = (0x1cb4cu + 668u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_22=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_23=m8_fp_float(fp_value); } }
L_1cb4c:
/* 1cb4c: ldrb.w r3, [r4, #0xcc] */ { { uint32_t base = r_4; uint32_t addr = base + (204u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1cb50: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cb52: beq.w #0x1d43c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d43c; } }
L_1cb56:
/* 1cb56: vmov s15, r3 */ { s_15=m8_fp_float(r_3); }
/* 1cb5a: vmov.f32 s14, #1.000000e+00 */ { s_14=m8_fp_float(0x3f800000u); }
/* 1cb5e: cmp r3, #6 */ { { uint32_t a = r_3; uint32_t b = 0x6u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cb60: vcvt.f32.u32 s15, s15, #3 */ { if (cpu->fpscr & (3u << 22)) m8_fp_sync(cpu); s_15=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_15), 0, 1, 32, 3, 1)); }
/* 1cb64: vsub.f32 s14, s14, s15 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_15,0); }
/* 1cb68: bhi.w #0x1d2ec */ if(m8_condition(cpu->xpsr,9)) { { goto L_1d2ec; } }
/* 1cb6c: adds r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); } }
/* 1cb6e: uxtb r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 255u); } }
L_1cb70:
/* 1cb70: vldr s11, [r4, #0x10c] */ { { uint32_t fp_addr = (r_4 + 268u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 1cb74: vldr s12, [r4, #0x108] */ { { uint32_t fp_addr = (r_4 + 264u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 1cb78: vldr s8, [r4, #0xec] */ { { uint32_t fp_addr = (r_4 + 236u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 1cb7c: vmul.f32 s11, s15, s11 */ { s_11=m8_filter_op(cpu,M8_FP_MUL,s_15,s_11,0); }
/* 1cb80: vldr s9, [r4, #0x100] */ { { uint32_t fp_addr = (r_4 + 256u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 1cb84: vmul.f32 s12, s15, s12 */ { s_12=m8_filter_op(cpu,M8_FP_MUL,s_15,s_12,0); }
/* 1cb88: vldr s10, [r4, #0x104] */ { { uint32_t fp_addr = (r_4 + 260u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); } }
/* 1cb8c: vmul.f32 s8, s15, s8 */ { s_8=m8_filter_op(cpu,M8_FP_MUL,s_15,s_8,0); }
/* 1cb90: vldr s13, [r4, #0xfc] */ { { uint32_t fp_addr = (r_4 + 252u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1cb94: vmul.f32 s9, s15, s9 */ { s_9=m8_filter_op(cpu,M8_FP_MUL,s_15,s_9,0); }
/* 1cb98: vmul.f32 s10, s15, s10 */ { s_10=m8_filter_op(cpu,M8_FP_MUL,s_15,s_10,0); }
/* 1cb9c: vldr s3, [r4, #0x134] */ { { uint32_t fp_addr = (r_4 + 308u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_3=m8_fp_float(fp_value); } }
/* 1cba0: vmul.f32 s13, s15, s13 */ { s_13=m8_filter_op(cpu,M8_FP_MUL,s_15,s_13,0); }
/* 1cba4: vldr s15, [r4, #0x154] */ { { uint32_t fp_addr = (r_4 + 340u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1cba8: vldr s4, [r4, #0x148] */ { { uint32_t fp_addr = (r_4 + 328u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_4=m8_fp_float(fp_value); } }
/* 1cbac: vfma.f32 s8, s3, s14 */ { s_8=m8_filter_op(cpu,M8_FP_FMA,s_3,s_14,s_8); }
/* 1cbb0: vfma.f32 s11, s15, s14 */ { s_11=m8_filter_op(cpu,M8_FP_FMA,s_15,s_14,s_11); }
/* 1cbb4: vldr s5, [r4, #0x14c] */ { { uint32_t fp_addr = (r_4 + 332u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_5=m8_fp_float(fp_value); } }
/* 1cbb8: vmov.f32 s15, s12 */ { s_15=m8_fp_float(m8_fp_bits32(s_12)); }
/* 1cbbc: vldr s6, [r4, #0x150] */ { { uint32_t fp_addr = (r_4 + 336u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); } }
/* 1cbc0: vldr s7, [r4, #0x144] */ { { uint32_t fp_addr = (r_4 + 324u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); } }
/* 1cbc4: vfma.f32 s9, s4, s14 */ { s_9=m8_filter_op(cpu,M8_FP_FMA,s_4,s_14,s_9); }
/* 1cbc8: vfma.f32 s10, s5, s14 */ { s_10=m8_filter_op(cpu,M8_FP_FMA,s_5,s_14,s_10); }
/* 1cbcc: strb.w r3, [r4, #0xcc] */ { { uint32_t base = r_4; uint32_t addr = base + (204u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),1,value); } }
/* 1cbd0: vfma.f32 s15, s6, s14 */ { s_15=m8_filter_op(cpu,M8_FP_FMA,s_6,s_14,s_15); }
/* 1cbd4: ldrb.w r3, [r4, #0x1de] */ { { uint32_t base = r_4; uint32_t addr = base + (478u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1cbd8: vfma.f32 s13, s7, s14 */ { s_13=m8_filter_op(cpu,M8_FP_FMA,s_7,s_14,s_13); }
/* 1cbdc: vstr s8, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 1cbe0: vstr s11, [r4, #0x130] */ { { uint32_t fp_addr = (r_4 + 304u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 1cbe4: vstr s9, [r4, #0x124] */ { { uint32_t fp_addr = (r_4 + 292u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 1cbe8: vstr s10, [r4, #0x128] */ { { uint32_t fp_addr = (r_4 + 296u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 1cbec: vstr s15, [r4, #0x12c] */ { { uint32_t fp_addr = (r_4 + 300u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 1cbf0: vstr s13, [r4, #0x120] */ { { uint32_t fp_addr = (r_4 + 288u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 1cbf4: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cbf6: beq.w #0x1d838 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d838; } }
/* 1cbfa: ldrh.w r3, [r4, #0x90] */ { { uint32_t base = r_4; uint32_t addr = base + (144u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_3 = (uint32_t)(value); } }
/* 1cbfe: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cc00: bne.w #0x1d2f0 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1d2f0; } }
/* 1cc04: vldr s13, [r4, #0x1b4] */ { { uint32_t fp_addr = (r_4 + 436u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1cc08: vldr d7, [r4, #0x180] */ { { uint32_t fp_addr = (r_4 + 384u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_15=m8_fp_float(fp_value); } }
/* 1cc0c: vcvt.f64.u32 d6, s13 */ { M8_SV_SET_D6(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 1, 1, 32, 0, 0)); }
/* 1cc10: vcmpe.f64 d7, d6 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 1); }
/* 1cc14: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cc18: bge #0x1cc2c */ if(m8_condition(cpu->xpsr,11)) { { goto L_1cc2c; } }
/* 1cc1a: vldr s11, [r4, #0x1b0] */ { { uint32_t fp_addr = (r_4 + 432u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 1cc1e: vcvt.f64.u32 d5, s11 */ { M8_SV_SET_D5(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 1, 1, 32, 0, 0)); }
/* 1cc22: vcmpe.f64 d7, d5 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 1); }
/* 1cc26: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cc2a: bpl #0x1cd02 */ if(m8_condition(cpu->xpsr,6)) { { goto L_1cd02; } } goto L_1cc2c;
L_1cd02:
/* 1cd02: mov r0, r4 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_4; uint32_t result = b; r_0 = (uint32_t)(result); } }
/* 1cd04: bl #0x1b938 */ { r_14=0x1cd09u; r_15=0x1b938u; goto L_1b938; }
L_1cd08:
/* 1cd08: cmp r0, #0 */ { { uint32_t a = r_0; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cd0a: beq.w #0x1e04c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1e04c; } }
/* 1cd0e: ldrb.w r0, [r4, #0x57] */ { { uint32_t base = r_4; uint32_t addr = base + (87u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_0 = (uint32_t)(value); } }
/* 1cd12: vldr d5, [r4, #0x190] */ { { uint32_t fp_addr = (r_4 + 400u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_11=m8_fp_float(fp_value); } }
/* 1cd16: cmp r0, #0 */ { { uint32_t a = r_0; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cd18: beq.w #0x1d5a2 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d5a2; } } goto L_1cd1c;
L_1cf76:
/* 1cf76: cbnz r2, #0x1cfa0 */ { if (r_2 != 0) { goto L_1cfa0; } }
/* 1cf78: ldrb.w r1, [r4, #0x59] */ { { uint32_t base = r_4; uint32_t addr = base + (89u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_1 = (uint32_t)(value); } }
/* 1cf7c: cbz r1, #0x1cfa0 */ { if (r_1 == 0) { goto L_1cfa0; } }
/* 1cf7e: vcmpe.f32 s16, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_16), 0, 1); }
/* 1cf82: ldr.w r7, [r4, #0x1a8] */ { { uint32_t base = r_4; uint32_t addr = base + (424u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_7 = (uint32_t)(value); } }
/* 1cf86: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cf8c: movge r1, #1 */ if(m8_condition(cpu->xpsr,11)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; r_1 = (uint32_t)(result); } } }
/* 1cf8e: movlt r1, #0 */ if(m8_condition(cpu->xpsr,12)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_1 = (uint32_t)(result); } } }
/* 1cf90: cmp r7, #0x3f */ { { uint32_t a = r_7; uint32_t b = 0x3fu; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cf94: movhi r1, #0 */ if(m8_condition(cpu->xpsr,9)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_1 = (uint32_t)(result); } } }
/* 1cf96: andls r1, r1, #1 */ if(m8_condition(cpu->xpsr,10)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_1; uint32_t b = 0x1u; uint32_t result = a & b; r_1 = (uint32_t)(result); } } }
/* 1cf9a: cmp r1, #0 */ { { uint32_t a = r_1; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1cf9c: bne.w #0x1e06a */ if(m8_condition(cpu->xpsr,2)) { { goto L_1e06a; } }
L_1cfa0:
/* 1cfa0: vcvt.s32.f32 s15, s16 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_16), 0, 0, 3, 32, 0)); }
/* 1cfa4: strh.w r3, [r4, #0x94] */ { { uint32_t base = r_4; uint32_t addr = base + (148u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),2,value); } }
/* 1cfa8: vldr s13, [r4, #0x120] */ { { uint32_t fp_addr = (r_4 + 288u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1cfac: vmov r3, s15 */ { r_3 = m8_fp_bits32(s_15); }
/* 1cfb0: strh.w r3, [r4, #0x92] */ { { uint32_t base = r_4; uint32_t addr = base + (146u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),2,value); } }
/* 1cfb4: ldrh.w r3, [r4, #0x90] */ { { uint32_t base = r_4; uint32_t addr = base + (144u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_3 = (uint32_t)(value); } }
L_1cfb8:
/* 1cfb8: adds r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); } }
/* 1cfba: vldr s12, [pc, #-0x1c4] */ { { uint32_t fp_addr = (0x1cfbcu - 452u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 1cfbe: vldr s8, [r4, #0x188] */ { { uint32_t fp_addr = (r_4 + 392u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 1cfc2: vmul.f32 s16, s16, s28 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_28,0); }
/* 1cfc6: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
/* 1cfc8: vmul.f32 s13, s13, s12 */ { s_13=m8_filter_op(cpu,M8_FP_MUL,s_13,s_12,0); }
/* 1cfcc: vcvt.f64.f32 d4, s8 */ { M8_SV_SET_D4(m8_fp_convert(cpu, m8_fp_bits32(s_8), 0)); }
/* 1cfd0: vmov s12, r3 */ { s_12=m8_fp_float(r_3); }
/* 1cfd4: vcvt.f32.s32 s12, s12 */ { s_12=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_12), 0, 0, 32, 0, 0)); }
/* 1cfd8: vldr d7, [r4, #0x180] */ { { uint32_t fp_addr = (r_4 + 384u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_15=m8_fp_float(fp_value); } }
/* 1cfdc: vcmp.f32 s12, s13 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_12), m8_fp_bits32(s_13), 0); }
/* 1cfe0: vadd.f64 d7, d4, d7 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_ADD, ((uint64_t)m8_fp_bits32(s_8)|((uint64_t)m8_fp_bits32(s_9)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
/* 1cfe4: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1cfe8: vadd.f64 d4, d4, d5 */ { M8_SV_SET_D4(m8_fp_op64(cpu, M8_FP_ADD, ((uint64_t)m8_fp_bits32(s_8)|((uint64_t)m8_fp_bits32(s_9)<<32)), ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 0)); }
/* 1cfee: movgt r3, #0 */ if(m8_condition(cpu->xpsr,13)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1cff0: vstr d7, [r4, #0x180] */ { { uint32_t fp_addr = (r_4 + 384u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); m8_le_write(M8_SV_HOST(fp_addr + 4u),4,m8_fp_bits32(s_15)); } }
/* 1cff4: vstr d4, [r4, #0x190] */ { { uint32_t fp_addr = (r_4 + 400u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); m8_le_write(M8_SV_HOST(fp_addr + 4u),4,m8_fp_bits32(s_9)); } }
/* 1cff8: strh.w r3, [r4, #0x90] */ { { uint32_t base = r_4; uint32_t addr = base + (144u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),2,value); } }
/* 1cffc: cbz r0, #0x1d002 */ { if (r_0 == 0) { goto L_1d002; } }
/* 1cffe: vmul.f32 s30, s30, s28 */ { s_30=m8_filter_op(cpu,M8_FP_MUL,s_30,s_28,0); }
L_1d002:
/* 1d002: cmp r2, #0 */ { { uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d004: beq #0x1d09c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d09c; } }
/* 1d006: ldrb.w r3, [r4, #0x59] */ { { uint32_t base = r_4; uint32_t addr = base + (89u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d00a: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d00c: beq #0x1d09c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d09c; } }
/* 1d00e: ldr.w r3, [r4, #0xb4] */ { { uint32_t base = r_4; uint32_t addr = base + (180u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d012: ldr r3, [r3, #0x78] */ { { uint32_t base = r_3; uint32_t addr = base + (120u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d014: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d016: beq.w #0x1d780 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d780; } }
/* 1d01a: vldr s13, [r4, #0xf4] */ { { uint32_t fp_addr = (r_4 + 244u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1d01e: vcmp.f32 s13, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_13), 0, 0); }
/* 1d022: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d026: bne.w #0x1d780 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1d780; } }
/* 1d02a: vmov.f32 s13, #1.000000e+00 */ { s_13=m8_fp_float(0x3f800000u); }
/* 1d02e: vldr s12, [r4, #0xf8] */ { { uint32_t fp_addr = (r_4 + 248u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 1d032: vcmp.f32 s12, s13 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_12), m8_fp_bits32(s_13), 0); }
/* 1d036: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d03a: bne.w #0x1d780 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1d780; } }
/* 1d03e: vmov.f64 d6, d13 */ { M8_SV_SET_D6(((uint64_t)m8_fp_bits32(s_26)|((uint64_t)m8_fp_bits32(s_27)<<32))); }
/* 1d042: ldr.w r2, [r4, #0x1b4] */ { { uint32_t base = r_4; uint32_t addr = base + (436u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); } }
L_1d046:
/* 1d046: vmov s11, r2 */ { s_11=m8_fp_float(r_2); }
/* 1d04a: vcvt.f64.u32 d5, s11 */ { M8_SV_SET_D5(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 1, 1, 32, 0, 0)); }
/* 1d04e: vsub.f64 d5, d5, d7 */ { M8_SV_SET_D5(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
/* 1d052: vcmpe.f64 d5, d6 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 1); }
/* 1d056: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d05a: bpl.w #0x1d798 */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d798; } }
/* 1d05e: vmov s11, r2 */ { s_11=m8_fp_float(r_2); }
/* 1d062: vmov.f32 s9, #1.000000e+00 */ { s_9=m8_fp_float(0x3f800000u); }
/* 1d066: vldr s8, [pc, #-0x26c] */ { { uint32_t fp_addr = (0x1d068u - 620u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 1d06a: vcvt.f32.u32 s11, s11 */ { s_11=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 0, 1, 32, 0, 0)); }
/* 1d06e: vcvt.f64.f32 d5, s11 */ { M8_SV_SET_D5(m8_fp_convert(cpu, m8_fp_bits32(s_11), 0)); }
/* 1d072: vsub.f64 d7, d5, d7 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
/* 1d076: vdiv.f64 d5, d7, d6 */ { M8_SV_SET_D5(m8_fp_op64(cpu, M8_FP_DIV, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0)); }
/* 1d07a: vcvt.f32.f64 s10, d5 */ { s_10=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 1)); }
/* 1d07e: vmaxnm.f32 s10, s10, s8 */ { s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_10), m8_fp_bits32(s_8), 0)); }
/* 1d082: vminnm.f32 s10, s10, s9 */ { s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_10), m8_fp_bits32(s_9), 0)); }
L_1d086:
/* 1d086: vmov.f32 s15, #1.000000e+00 */ { s_15=m8_fp_float(0x3f800000u); }
/* 1d08a: vcmp.f32 s10, s15 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_10), m8_fp_bits32(s_15), 0); }
/* 1d08e: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d092: beq #0x1d09c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d09c; } }
/* 1d094: vmul.f32 s16, s16, s10 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_10,0); }
/* 1d098: vmul.f32 s30, s30, s10 */ { s_30=m8_filter_op(cpu,M8_FP_MUL,s_30,s_10,0); }
L_1d09c:
/* 1d09c: ldrb.w r3, [r4, #0x1ac] */ { { uint32_t base = r_4; uint32_t addr = base + (428u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d0a0: cbz r3, #0x1d0ea */ { if (r_3 == 0) { goto L_1d0ea; } }
/* 1d0a2: subs r2, r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_2 = (uint32_t)(result); } }
/* 1d0a4: vmov.f32 s11, #1.600000e+01 */ { s_11=m8_fp_float(0x41800000u); }
/* 1d0a8: vldr s12, [pc, #-0x2ac] */ { { uint32_t fp_addr = (0x1d0acu - 684u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 1d0ac: vmov.f32 s14, #1.000000e+00 */ { s_14=m8_fp_float(0x3f800000u); }
/* 1d0b0: vmov s15, r2 */ { s_15=m8_fp_float(r_2); }
/* 1d0b4: adds r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); } }
/* 1d0b6: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
/* 1d0ba: uxtb r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 255u); } }
/* 1d0bc: cmp r3, #0x10 */ { { uint32_t a = r_3; uint32_t b = 0x10u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d0be: vdiv.f32 s13, s11, s15 */ { s_13=m8_filter_op(cpu,M8_FP_DIV,s_11,s_15,0); }
/* 1d0c4: movhi r3, #0 */ if(m8_condition(cpu->xpsr,9)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d0c6: strb.w r3, [r4, #0x1ac] */ { { uint32_t base = r_4; uint32_t addr = base + (428u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),1,value); } }
/* 1d0ca: vmul.f32 s15, s15, s12 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_15,s_12,0); }
/* 1d0ce: vsub.f32 s12, s15, s14 */ { s_12=m8_filter_op(cpu,M8_FP_SUB,s_15,s_14,0); }
/* 1d0d2: vsub.f32 s14, s14, s13 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_13,0); }
/* 1d0d6: vmov.f32 s13, s15 */ { s_13=m8_fp_float(m8_fp_bits32(s_15)); }
/* 1d0da: vfma.f32 s13, s14, s12 */ { s_13=m8_filter_op(cpu,M8_FP_FMA,s_14,s_12,s_13); }
/* 1d0de: vdiv.f32 s14, s15, s13 */ { s_14=m8_filter_op(cpu,M8_FP_DIV,s_15,s_13,0); }
/* 1d0e2: vmul.f32 s16, s16, s14 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_14,0); }
/* 1d0e6: vmul.f32 s30, s30, s14 */ { s_30=m8_filter_op(cpu,M8_FP_MUL,s_30,s_14,0); }
L_1d0ea:
/* 1d0ea: ldrb.w r2, [r4, #0x41] */ { { uint32_t base = r_4; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 1d0ee: cmp r2, #3 */ { { uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d0f0: bhi #0x1d10a */ if(m8_condition(cpu->xpsr,9)) { { goto L_1d10a; } }
/* 1d0f2: ldr r0, [r4, #0x50] */ { { uint32_t base = r_4; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 1d0f4: vmov.f32 s0, s16 */ { s_0=m8_fp_float(m8_fp_bits32(s_16)); }
/* 1d0f8: vldr s1, [r4, #0x12c] */ { { uint32_t fp_addr = (r_4 + 300u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); } }
/* 1d0fc: ldr r3, [r0] */ { { uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
#ifdef M8_SV_SHAPE
/* 1d0fe: ldr r3, [r3] */ { { uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d100: blx r3 */ {M8_SV_METHOD(0x1d102u,0x1d100u);}
L_1d102:
#else
/* 1d0fe: ldr r3, [r3] */ { { uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } } r_14=0x1d103u;s_14=m8_fp_float(0xbf800000u);s_15=1.0f; s_0=m8_filter_op(cpu,M8_FP_MUL,s_0,s_1,0); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0)); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));
#endif
/* 1d102: vmov.f32 s16, s0 */ { s_16=m8_fp_float(m8_fp_bits32(s_0)); }
/* 1d106: ldrb.w r2, [r4, #0x41] */ { { uint32_t base = r_4; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); } }
L_1d10a:
/* 1d10a: ldrb.w r3, [r4, #0x40] */ { { uint32_t base = r_4; uint32_t addr = base + (64u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d10e: subs r1, r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_1 = (uint32_t)(result); } }
/* 1d110: cmp r1, #4 */ { { uint32_t a = r_1; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d112: bls.w #0x1d4b8 */ if(m8_condition(cpu->xpsr,10)) { { goto L_1d4b8; } }
/* 1d116: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d118: bne.w #0x1d6b6 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1d6b6; } }
#ifdef M8_SV_EXTENDED
L_1d11c:
#endif
/* 1d11c: cmp r2, #3 */ { { uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d11e: bls.w #0x1d34c */ if(m8_condition(cpu->xpsr,10)) { { goto L_1d34c; } } goto L_1d122;
L_1d174:
/* 1d174: ldrb.w r3, [r4, #0x60] */ { { uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d178: cbz r3, #0x1d1d6 */ { if (r_3 == 0) { goto L_1d1d6; } }
/* 1d17a: ldrb.w r3, [r4, #0x59] */ { { uint32_t base = r_4; uint32_t addr = base + (89u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d17e: cbnz r3, #0x1d1d6 */ { if (r_3 != 0) { goto L_1d1d6; } }
/* 1d180: vldr s14, [r4, #0x1b4] */ { { uint32_t fp_addr = (r_4 + 436u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d184: vldr d6, [r4, #0x180] */ { { uint32_t fp_addr = (r_4 + 384u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_13=m8_fp_float(fp_value); } }
/* 1d188: vcvt.f64.u32 d5, s14 */ { M8_SV_SET_D5(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 1, 1, 32, 0, 0)); }
/* 1d18c: vsub.f64 d5, d5, d6 */ { M8_SV_SET_D5(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0)); }
/* 1d190: vcmpe.f64 d5, d10 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_20)|((uint64_t)m8_fp_bits32(s_21)<<32)), 1); }
/* 1d194: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d198: bpl.w #0x1d7d4 */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d7d4; } }
/* 1d19c: vcvt.f32.u32 s14, s14 */ { s_14=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 1, 32, 0, 0)); }
/* 1d1a0: vcvt.f64.f32 d7, s14 */ { M8_SV_SET_D7(m8_fp_convert(cpu, m8_fp_bits32(s_14), 0)); }
/* 1d1a4: vsub.f64 d7, d7, d6 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0)); }
/* 1d1a8: vldr s12, [pc, #0x308] */ { { uint32_t fp_addr = (0x1d1acu + 776u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 1d1ac: vmov.f32 s13, #1.000000e+00 */ { s_13=m8_fp_float(0x3f800000u); }
/* 1d1b0: vmul.f64 d7, d7, d12 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_MUL, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_24)|((uint64_t)m8_fp_bits32(s_25)<<32)), 0)); }
/* 1d1b4: vcvt.f32.f64 s14, d7 */ { s_14=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1)); }
/* 1d1b8: vmaxnm.f32 s14, s14, s12 */ { s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_14), m8_fp_bits32(s_12), 0)); }
/* 1d1bc: vminnm.f32 s14, s14, s13 */ { s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0)); }
/* 1d1c0: vmov.f32 s15, #1.000000e+00 */ { s_15=m8_fp_float(0x3f800000u); }
/* 1d1c4: vcmp.f32 s14, s15 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0); }
/* 1d1c8: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d1cc: beq #0x1d1d6 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d1d6; } }
/* 1d1ce: vmul.f32 s16, s16, s14 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_14,0); }
/* 1d1d2: vmul.f32 s30, s30, s14 */ { s_30=m8_filter_op(cpu,M8_FP_MUL,s_30,s_14,0); }
L_1d1d6:
/* 1d1d6: vldr s15, [r4, #0x44] */ { { uint32_t fp_addr = (r_4 + 68u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d1da: vadd.f32 s14, s16, s30 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_16,s_30,0); }
/* 1d1de: vldr s13, [pc, #0x2a4] */ { { uint32_t fp_addr = (0x1d1e0u + 676u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1d1e2: vcmpe.f32 s15, s13 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 1); }
/* 1d1e6: vcvt.s32.f32 s14, s14, #6 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 6)); }
/* 1d1ea: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d1ee: bpl.w #0x1d33e */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d33e; } }
/* 1d1f2: vcvt.u32.f32 s13, s15 */ { s_13=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_15), 0, 1, 3, 32, 0)); }
/* 1d1f6: vmov r3, s13 */ { r_3 = m8_fp_bits32(s_13); }
/* 1d1fa: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
/* 1d1fc: lsls r3, r3, #1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x1u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
L_1d1fe:
/* 1d1fe: vmov r1, s14 */ { r_1 = m8_fp_bits32(s_14); }
/* 1d202: ldr.w r2, [r4, #0xc8] */ { { uint32_t base = r_4; uint32_t addr = base + (200u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 1d206: vldr s13, [r4, #0x48] */ { { uint32_t fp_addr = (r_4 + 72u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1d20a: strh r1, [r2, r3] */ { { uint32_t base = r_2; uint32_t addr = base + ((0u + r_3)); uint32_t value = r_1; m8_le_write(M8_SV_HOST(addr),2,value); } }
/* 1d20c: ldrh.w r3, [r4, #0x1dc] */ { { uint32_t base = r_4; uint32_t addr = base + (476u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_3 = (uint32_t)(value); } }
/* 1d210: vadd.f32 s15, s15, s13 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_13,0); }
/* 1d214: vmov s14, r3 */ { s_14=m8_fp_float(r_3); }
/* 1d218: vstr s15, [r4, #0x44] */ { { uint32_t fp_addr = (r_4 + 68u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 1d21c: vcvt.f32.s32 s14, s14 */ { s_14=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0)); }
/* 1d220: vcmpe.f32 s15, s14 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 1); }
/* 1d224: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d228: ble #0x1d234 */ if(m8_condition(cpu->xpsr,14)) { { goto L_1d234; } }
/* 1d22a: vcvt.u32.f32 s15, s15 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_15), 0, 1, 3, 32, 0)); }
/* 1d22e: vmov r3, s15 */ { r_3 = m8_fp_bits32(s_15); }
/* 1d232: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
L_1d234:
/* 1d234: vmov.f32 s15, #5.000000e-01 */ { s_15=m8_fp_float(0x3f000000u); }
/* 1d238: vldr s14, [r4, #0x130] */ { { uint32_t fp_addr = (r_4 + 304u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d23c: strh.w r3, [r4, #0x1dc] */ { { uint32_t base = r_4; uint32_t addr = base + (476u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),2,value); } }
/* 1d240: vcmpe.f32 s14, s15 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 1); }
/* 1d244: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d248: blt #0x1d338 */ if(m8_condition(cpu->xpsr,12)) { { goto L_1d338; } }
/* 1d24a: vsub.f32 s15, s15, s14 */ { s_15=m8_filter_op(cpu,M8_FP_SUB,s_15,s_14,0); }
/* 1d24e: vmov.f32 s13, #2.000000e+00 */ { s_13=m8_fp_float(0x40000000u); }
/* 1d252: vmov.f32 s14, #1.000000e+00 */ { s_14=m8_fp_float(0x3f800000u); }
/* 1d256: vfma.f32 s14, s15, s13 */ { s_14=m8_filter_op(cpu,M8_FP_FMA,s_15,s_13,s_14); }
L_1d25a:
/* 1d25a: vldr s15, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d25e: vmov.f32 s13, #5.000000e-01 */ { s_13=m8_fp_float(0x3f000000u); }
/* 1d262: vmul.f32 s15, s16, s15 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_16,s_15,0); }
/* 1d266: vmul.f32 s15, s15, s14 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_15,s_14,0); }
/* 1d26a: vstmia sl!, {s15} */ { { uint32_t fp_addr = r_10; uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); r_10 += 4u; } }
/* 1d26e: vldr s15, [r4, #0x130] */ { { uint32_t fp_addr = (r_4 + 304u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d272: vcmpe.f32 s15, s13 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 1); }
/* 1d276: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d27a: bpl #0x1d316 */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d316; } }
/* 1d27c: vldr s14, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d280: vadd.f32 s15, s15, s15 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_15,0); }
/* 1d284: ldr r3, [sp] */ { { uint32_t base = r_13; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d286: adds r5, #1 */ { { uint32_t a = r_5; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_5 = (uint32_t)(result); } }
/* 1d288: vmul.f32 s14, s30, s14 */ { s_14=m8_filter_op(cpu,M8_FP_MUL,s_30,s_14,0); }
/* 1d28c: vldr s13, [pc, #0x1f8] */ { { uint32_t fp_addr = (0x1d290u + 504u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1d290: uxth r5, r5 */ { { uint32_t value = r_5; r_5 = (uint32_t)(value & 65535u); } }
/* 1d292: vmul.f32 s15, s14, s15 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_14,s_15,0); }
/* 1d296: vstmia r3!, {s15} */ { { uint32_t fp_addr = r_3; uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); r_3 += 4u; } }
/* 1d29a: vldr s15, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d29e: str r3, [sp] */ { { uint32_t base = r_13; uint32_t addr = base + (0u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1d2a0: vcmpe.f32 s15, s13 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 1); }
L_1d2a4:
/* 1d2a4: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d2aa: movmi r3, #1 */ if(m8_condition(cpu->xpsr,5)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d2ac: movpl r3, #0 */ if(m8_condition(cpu->xpsr,6)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d2ae: cmp r8, r5 */ { { uint32_t a = r_8; uint32_t b = r_5; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d2b0: strb.w r3, [r4, #0x61] */ { { uint32_t base = r_4; uint32_t addr = base + (97u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),1,value); } }
/* 1d2b4: bne.w #0x1cb4c */ if(m8_condition(cpu->xpsr,2)) { { goto L_1cb4c; } }
L_1d2b8:
/* 1d2b8: ldr r0, [sp, #8] */ { { uint32_t base = r_13; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 1d2ba: add sp, #0x24 */ { { uint32_t a = r_13; uint32_t b = 0x24u; uint32_t result = a + b + 0u; r_13 = (uint32_t)(result); } }
/* 1d2bc: vpop {d8, d9, d10, d11, d12, d13, d14, d15} */ { { uint32_t fp_addr = r_13; uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_17=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 8u),4); s_18=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 12u),4); s_19=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 16u),4); s_20=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 20u),4); s_21=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 24u),4); s_22=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 28u),4); s_23=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 32u),4); s_24=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 36u),4); s_25=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 40u),4); s_26=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 44u),4); s_27=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 48u),4); s_28=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 52u),4); s_29=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 56u),4); s_30=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 60u),4); s_31=m8_fp_float(fp_value); r_13 += 64u; } }
/* 1d2c0: pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc} */ { { uint32_t base = r_13; uint32_t addr = base + 0u; uint32_t value0; value0=m8_le_read(M8_SV_HOST(addr + 0u),4); uint32_t value1; value1=m8_le_read(M8_SV_HOST(addr + 4u),4); uint32_t value2; value2=m8_le_read(M8_SV_HOST(addr + 8u),4); uint32_t value3; value3=m8_le_read(M8_SV_HOST(addr + 12u),4); uint32_t value4; value4=m8_le_read(M8_SV_HOST(addr + 16u),4); uint32_t value5; value5=m8_le_read(M8_SV_HOST(addr + 20u),4); uint32_t value6; value6=m8_le_read(M8_SV_HOST(addr + 24u),4); uint32_t value7; value7=m8_le_read(M8_SV_HOST(addr + 28u),4); uint32_t value8; value8=m8_le_read(M8_SV_HOST(addr + 32u),4); r_13 = base + 36u; r_4 = (uint32_t)(value0); r_5 = (uint32_t)(value1); r_6 = (uint32_t)(value2); r_7 = (uint32_t)(value3); r_8 = (uint32_t)(value4); r_9 = (uint32_t)(value5); r_10 = (uint32_t)(value6); r_11 = (uint32_t)(value7); r_15 = (value8) & ~1u; goto done; } }
L_1d2c4:
/* 1d2c4: vldr s14, [r0, #0x120] */ { { uint32_t fp_addr = (r_0 + 288u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d2c8: vldr s15, [pc, #0x1c0] */ { { uint32_t fp_addr = (0x1d2ccu + 448u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d2cc: vcmpe.f32 s14, s15 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 1); }
/* 1d2d0: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d2d6: movmi r3, #1 */ if(m8_condition(cpu->xpsr,5)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d2d8: movpl r3, #0 */ if(m8_condition(cpu->xpsr,6)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d2da: str r3, [sp, #4] */ { { uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1d2dc: b #0x1caf0 */ { goto L_1caf0; }
/* 1d2de: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d2e2: ble.w #0x1cca6 */ if(m8_condition(cpu->xpsr,14)) { { goto L_1cca6; } }
/* 1d2e6: vneg.f32 s15, s15 */ { s_15=m8_fp_float((m8_fp_bits32(s_15) ^ 0x80000000u)); }
/* 1d2ea: b #0x1cca6 */ { goto L_1cca6; }
L_1d2ec:
/* 1d2ec: movs r3, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 1d2ee: b #0x1cb70 */ { goto L_1cb70; }
L_1d2f0:
/* 1d2f0: ldrsh.w r2, [r4, #0x92] */ { { uint32_t base = r_4; uint32_t addr = base + (146u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_2 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1d2f4: ldrb.w r0, [r4, #0x57] */ { { uint32_t base = r_4; uint32_t addr = base + (87u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_0 = (uint32_t)(value); } }
/* 1d2f8: vmov s16, r2 */ { s_16=m8_fp_float(r_2); }
/* 1d2fc: ldrsh.w r2, [r4, #0x94] */ { { uint32_t base = r_4; uint32_t addr = base + (148u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_2 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1d300: vmov s30, r2 */ { s_30=m8_fp_float(r_2); }
/* 1d304: vcvt.f32.s32 s16, s16 */ { s_16=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_16), 0, 0, 32, 0, 0)); }
/* 1d308: ldrb.w r2, [r4, #0x60] */ { { uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 1d30c: vcvt.f32.s32 s30, s30 */ { s_30=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_30), 0, 0, 32, 0, 0)); }
/* 1d310: vldr d5, [r4, #0x190] */ { { uint32_t fp_addr = (r_4 + 400u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_11=m8_fp_float(fp_value); } }
/* 1d314: b #0x1cfb8 */ { goto L_1cfb8; }
L_1d316:
/* 1d316: vldr s15, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d31a: adds r5, #1 */ { { uint32_t a = r_5; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_5 = (uint32_t)(result); } }
/* 1d31c: ldr r3, [sp] */ { { uint32_t base = r_13; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d31e: vmul.f32 s15, s15, s30 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_15,s_30,0); }
/* 1d322: vldr s14, [pc, #0x164] */ { { uint32_t fp_addr = (0x1d324u + 356u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d326: uxth r5, r5 */ { { uint32_t value = r_5; r_5 = (uint32_t)(value & 65535u); } }
/* 1d328: vstmia r3!, {s15} */ { { uint32_t fp_addr = r_3; uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); r_3 += 4u; } }
/* 1d32c: vldr s15, [r4, #0x110] */ { { uint32_t fp_addr = (r_4 + 272u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d330: str r3, [sp] */ { { uint32_t base = r_13; uint32_t addr = base + (0u); uint32_t value = r_3; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1d332: vcmpe.f32 s15, s14 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 1); }
/* 1d336: b #0x1d2a4 */ { goto L_1d2a4; }
L_1d338:
/* 1d338: vmov.f32 s14, #1.000000e+00 */ { s_14=m8_fp_float(0x3f800000u); }
/* 1d33c: b #0x1d25a */ { goto L_1d25a; }
L_1d33e:
/* 1d33e: vldr s15, [pc, #0x174] */ { { uint32_t fp_addr = (0x1d340u + 372u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d342: movs r3, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 1d344: b #0x1d1fe */ { goto L_1d1fe; }
L_1d346:
/* 1d346: vmov.f32 s30, s16 */ { s_30=m8_fp_float(m8_fp_bits32(s_16)); }
/* 1d34a: b #0x1d174 */ { goto L_1d174; }
L_1d34c:
/* 1d34c: ldrb.w r3, [r4, #0x57] */ { { uint32_t base = r_4; uint32_t addr = base + (87u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d350: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d352: beq #0x1d346 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d346; } } goto L_1d354;
L_1d43c:
/* 1d43c: mov r0, r4 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_4; uint32_t result = b; r_0 = (uint32_t)(result); } }
/* 1d43e: bl #0x1c414 */ { r_14=0x1d443u;r_15=0x1c414u;M8_SV_SAVE(cpu); {int status=update(cpu,mem); if(status!=M8_AOT_EXIT || state.r[15]!=0x1d442u) {*output=*cpu;return status;} M8_SV_LOAD(cpu); if(r_4!=root || r_13!=frame || r_5>=r_8 || !m8_sampler_voice_qualify(cpu,mem,r_4,r_10,m8_le_read(M8_SV_HOST(r_13),4),r_8-r_5,0)) {r_15=0x1d442u;goto done;} } }
/* 1d442: vldr s14, [r4, #0x188] */ { { uint32_t fp_addr = (r_4 + 392u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d446: vcvt.u32.f32 s15, s14 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 1, 3, 32, 0)); }
/* 1d44a: vmov r3, s15 */ { r_3 = m8_fp_bits32(s_15); }
/* 1d44e: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
/* 1d450: vmov s15, r3 */ { s_15=m8_fp_float(r_3); }
/* 1d454: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
/* 1d458: vsub.f32 s14, s14, s15 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_15,0); }
/* 1d45c: vcmp.f32 s14, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), 0, 0); }
/* 1d460: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d464: beq.w #0x1d580 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d580; } }
/* 1d468: vldr s15, [r4, #0x120] */ { { uint32_t fp_addr = (r_4 + 288u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1d46c: vcmpe.f32 s15, s29 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_29), 1); }
/* 1d470: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d474: bpl.w #0x1d580 */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d580; } }
/* 1d478: ldr r2, [sp, #8] */ { { uint32_t base = r_13; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 1d47a: ldrb.w r3, [r4, #0xcc] */ { { uint32_t base = r_4; uint32_t addr = base + (204u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d47e: str r2, [sp, #4] */ { { uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value = r_2; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1d480: b.w #0x1cb56 */ { goto L_1cb56; }
L_1d580:
/* 1d580: movs r2, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_2 = (uint32_t)(result); } }
/* 1d582: ldrb.w r3, [r4, #0xcc] */ { { uint32_t base = r_4; uint32_t addr = base + (204u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d586: str r2, [sp, #4] */ { { uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value = r_2; m8_le_write(M8_SV_HOST(addr),4,value); } }
/* 1d588: b.w #0x1cb56 */ { goto L_1cb56; }
L_1d58c:
/* 1d58c: vcvt.s32.f32 s15, s17 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_17), 0, 0, 3, 32, 0)); }
/* 1d590: ldrb.w r2, [r4, #0x60] */ { { uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 1d594: vmov.f32 s30, s17 */ { s_30=m8_fp_float(m8_fp_bits32(s_17)); }
/* 1d598: vstr s15, [sp, #0xc] */ { { uint32_t fp_addr = (r_13 + 12u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 1d59c: ldrsh.w r3, [sp, #0xc] */ { { uint32_t base = r_13; uint32_t addr = base + (12u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_3 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1d5a0: b #0x1cf76 */ { goto L_1cf76; }
L_1d5a2:
/* 1d5a2: vcvt.u32.f64 s15, d5 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 1, 1, 3, 32, 0)); }
/* 1d5a6: ldrb.w r3, [r4, #0x54] */ { { uint32_t base = r_4; uint32_t addr = base + (84u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d5aa: cmp r3, #0x18 */ { { uint32_t a = r_3; uint32_t b = 0x18u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d5ac: vmov r1, s15 */ { r_1 = m8_fp_bits32(s_15); }
/* 1d5b0: vcvt.f64.u32 d7, s15 */ { M8_SV_SET_D7(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 1, 1, 32, 0, 0)); }
/* 1d5b4: vsub.f64 d7, d5, d7 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
/* 1d5b8: vcvt.f32.f64 s14, d7 */ { s_14=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1)); }
/* 1d5bc: beq.w #0x1dcf0 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1dcf0; } }
/* 1d5c0: cmp r3, #0x20 */ { { uint32_t a = r_3; uint32_t b = 0x20u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d5c2: beq.w #0x1dcb4 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1dcb4; } }
/* 1d5c6: cmp r3, #0x10 */ { { uint32_t a = r_3; uint32_t b = 0x10u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d5c8: beq.w #0x1dc2e */ if(m8_condition(cpu->xpsr,1)) { { goto L_1dc2e; } } goto L_1d5cc;
L_1d61e:
/* 1d61e: ldr r3, [sp, #4] */ { { uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1d620: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d622: beq #0x1d58c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d58c; } } goto L_1d624;
L_1d7d4:
/* 1d7d4: ldrb.w r3, [r4, #0x5c] */ { { uint32_t base = r_4; uint32_t addr = base + (92u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d7d8: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d7da: beq.w #0x1d1d6 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d1d6; } } goto L_1d7de;
L_1d810:
/* 1d810: vldr s13, [pc, #-0x360] */ { { uint32_t fp_addr = (0x1d814u - 864u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1d814: b #0x1d61e */ { goto L_1d61e; }
L_1dc2e:
/* One current-index proof covers all four mono16 cache reads. An unusual
 * position resumes before this original CMP, with previous effects intact. */
if(r_1>8189u) {r_15=0x1dc2eu;goto done;}
/* 1dc2e: cmp r1, #0 */ { { uint32_t a = r_1; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1dc30: beq.w #0x1e0e4 */ if(m8_condition(cpu->xpsr,1)) { { goto L_1e0e4; } }
/* 1dc34: mvn r2, #0x80000000 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; carry = (0x80000000u) >> 31; uint32_t b = 0x80000000u; uint32_t result = ~b; r_2 = (uint32_t)(result); } }
/* 1dc38: ldr.w r3, [r4, #0xbc] */ { { uint32_t base = r_4; uint32_t addr = base + (188u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1dc3c: add r2, r1 */ { { uint32_t a = r_2; uint32_t b = r_1; uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); } }
/* 1dc3e: ldrsh.w r2, [r3, r2, lsl #1] */ { { uint32_t base = r_3; uint32_t addr = base + ((0u + m8_shift(r_2, 2, 1, NULL))); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_2 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1dc42: vmov s7, r2 */ { s_7=m8_fp_float(r_2); }
/* 1dc46: vcvt.f32.s32 s7, s7 */ { s_7=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_7), 0, 0, 32, 0, 0)); }
L_1dc4a:
/* 1dc4a: ldrsh.w r2, [r3, r1, lsl #1] */ { { uint32_t base = r_3; uint32_t addr = base + ((0u + m8_shift(r_1, 2, 1, NULL))); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_2 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1dc4e: adds r7, r1, #1 */ { { uint32_t a = r_1; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_7 = (uint32_t)(result); } }
/* 1dc50: ldrh.w lr, [r4, #0x1b8] */ { { uint32_t base = r_4; uint32_t addr = base + (440u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_14 = (uint32_t)(value); } }
/* 1dc54: vmov s16, r2 */ { s_16=m8_fp_float(r_2); }
/* 1dc58: lsls r2, r1, #1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_1, 2, 0x1u, &carry); m8_logic_flags(cpu, result, carry); r_2 = (uint32_t)(result); } }
/* 1dc5a: cmp r7, lr */ { { uint32_t a = r_7; uint32_t b = r_14; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1dc5c: vcvt.f32.s32 s16, s16 */ { s_16=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_16), 0, 0, 32, 0, 0)); }
/* 1dc60: bhs.w #0x1e0de */ if(m8_condition(cpu->xpsr,3)) { { goto L_1e0de; } }
/* 1dc64: add.w ip, r3, r2 */ { { uint32_t a = r_3; uint32_t b = r_2; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); } }
/* 1dc68: ldrsh.w r6, [ip, #2] */ { { uint32_t base = r_12; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1dc6c: vmov s15, r6 */ { s_15=m8_fp_float(r_6); }
/* 1dc70: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
L_1dc74:
/* 1dc74: add.w ip, r1, #2 */ { { uint32_t a = r_1; uint32_t b = 0x2u; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); } }
/* 1dc78: cmp lr, ip */ { { uint32_t a = r_14; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1dc7a: bls.w #0x1d810 */ if(m8_condition(cpu->xpsr,10)) { { goto L_1d810; } }
/* 1dc7e: add r3, r2 */ { { uint32_t a = r_3; uint32_t b = r_2; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); } }
/* 1dc80: ldrsh.w r3, [r3, #4] */ { { uint32_t base = r_3; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_3 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); } }
/* 1dc84: vmov s13, r3 */ { s_13=m8_fp_float(r_3); }
/* 1dc88: vcvt.f32.s32 s13, s13 */ { s_13=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 0, 0, 32, 0, 0)); }
/* 1dc8c: b #0x1d61e */ { goto L_1d61e; }
L_1e0de:
/* 1e0de: vldr s15, [pc, #0x130] */ { { uint32_t fp_addr = (0x1e0e0u + 304u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1e0e2: b #0x1dc74 */ { goto L_1dc74; }
L_1e0e4:
/* 1e0e4: vldr s7, [pc, #0x128] */ { { uint32_t fp_addr = (0x1e0e8u + 296u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); } }
/* 1e0e8: ldr.w r3, [r4, #0xbc] */ { { uint32_t base = r_4; uint32_t addr = base + (188u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1e0ec: b #0x1dc4a */ { goto L_1dc4a; }
L_1b938:
/* 1b938: vmov.f64 d7, #2.000000e+00 */ { M8_SV_SET_D7(UINT64_C(0x4000000000000000)); }
/* 1b93c: vldr d3, [r0, #0x180] */ { { uint32_t fp_addr = (r_0 + 384u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_7=m8_fp_float(fp_value); } }
/* 1b940: push {r3, r4, r5, lr} */ { { uint32_t base = r_13; uint32_t addr = base + 4294967280u; uint32_t value0 = r_3; uint32_t value1 = r_4; uint32_t value2 = r_5; uint32_t value3 = r_14; m8_le_write(M8_SV_HOST(addr + 0u),4,value0); m8_le_write(M8_SV_HOST(addr + 4u),4,value1); m8_le_write(M8_SV_HOST(addr + 8u),4,value2); m8_le_write(M8_SV_HOST(addr + 12u),4,value3); r_13 = base - 16u; } }
/* 1b942: vcmpe.f64 d3, d7 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1); }
/* 1b946: mov r4, r0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_0; uint32_t result = b; r_4 = (uint32_t)(result); } }
/* 1b948: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1b94c: ble #0x1ba36 */ if(m8_condition(cpu->xpsr,14)) { { goto L_1ba36; } }
/* 1b94e: vsub.f64 d7, d3, d7 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
L_1b952:
/* 1b952: vldr s13, [r4, #0x1cc] */ { { uint32_t fp_addr = (r_4 + 460u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 1b956: ldr.w r3, [r4, #0x1b4] */ { { uint32_t base = r_4; uint32_t addr = base + (436u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 1b95a: vcvt.f64.u32 d6, s13 */ { M8_SV_SET_D6(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 1, 1, 32, 0, 0)); }
/* 1b95e: vcmpe.f64 d6, d7 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1); }
/* 1b962: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1b966: bhi #0x1b990 */ if(m8_condition(cpu->xpsr,9)) { { goto L_1b990; } }
/* 1b968: vmov.f64 d7, #6.000000e+00 */ { M8_SV_SET_D7(UINT64_C(0x4018000000000000)); }
/* 1b96c: vmov s11, r3 */ { s_11=m8_fp_float(r_3); }
/* 1b970: vldr s9, [r4, #0x1d0] */ { { uint32_t fp_addr = (r_4 + 464u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 1b974: vcvt.f64.u32 d5, s11 */ { M8_SV_SET_D5(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 1, 1, 32, 0, 0)); }
/* 1b978: vcvt.f64.u32 d4, s9 */ { M8_SV_SET_D4(m8_fp_from_int(cpu, m8_fp_bits32(s_9), 1, 1, 32, 0, 0)); }
/* 1b97c: vadd.f64 d7, d3, d7 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_ADD, ((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0)); }
/* 1b980: vminnm.f64 d7, d7, d5 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_MINNM, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 0)); }
/* 1b984: vcmpe.f64 d4, d7 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_8)|((uint64_t)m8_fp_bits32(s_9)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1); }
/* 1b988: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1b98c: bge.w #0x1bade */ if(m8_condition(cpu->xpsr,11)) { { goto L_1bade; } } goto L_1b990;
L_1ba36:
/* 1ba36: vmov.f64 d7, d3 */ { M8_SV_SET_D7(((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32))); }
/* 1ba3a: b #0x1b952 */ { goto L_1b952; }
L_1bade:
/* 1bade: vldr d7, [r4, #0x190] */ { { uint32_t fp_addr = (r_4 + 400u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); fp_value=m8_le_read(M8_SV_HOST(fp_addr + 4u),4); s_15=m8_fp_float(fp_value); } }
/* 1bae2: vcmp.f64 d7, #0 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0, 0); }
/* 1bae6: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1baea: vcmp.f64 d3, #0 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32)), 0, 0); }
/* 1baf0: moveq r0, #1 */ if(m8_condition(cpu->xpsr,1)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; r_0 = (uint32_t)(result); } } }
/* 1baf2: movne r0, #0 */ if(m8_condition(cpu->xpsr,2)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_0 = (uint32_t)(result); } } }
/* 1baf4: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1baf8: and r0, r0, #1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_0; uint32_t b = 0x1u; uint32_t result = a & b; r_0 = (uint32_t)(result); } }
/* 1bafe: moveq r0, #0 */ if(m8_condition(cpu->xpsr,1)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_0 = (uint32_t)(result); } } }
/* 1bb00: cmp r0, #0 */ { { uint32_t a = r_0; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1bb02: bne.w #0x1bc24 */ if(m8_condition(cpu->xpsr,2)) { { goto L_1bc24; } }
/* 1bb06: movs r0, #1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_0 = (uint32_t)(result); } }
/* 1bb08: pop {r3, r4, r5, pc} */ { { uint32_t base = r_13; uint32_t addr = base + 0u; uint32_t value0; value0=m8_le_read(M8_SV_HOST(addr + 0u),4); uint32_t value1; value1=m8_le_read(M8_SV_HOST(addr + 4u),4); uint32_t value2; value2=m8_le_read(M8_SV_HOST(addr + 8u),4); uint32_t value3; value3=m8_le_read(M8_SV_HOST(addr + 12u),4); r_13 = base + 16u; r_3 = (uint32_t)(value0); r_4 = (uint32_t)(value1); r_5 = (uint32_t)(value2); r_15 = (value3) & ~1u; goto L_1cd08; } }
L_1bc24:
/* 1bc24: vsub.f64 d6, d3, d6 */ { M8_SV_SET_D6(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_6)|((uint64_t)m8_fp_bits32(s_7)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0)); }
/* 1bc28: vstr d6, [r4, #0x190] */ { { uint32_t fp_addr = (r_4 + 400u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); m8_le_write(M8_SV_HOST(fp_addr + 4u),4,m8_fp_bits32(s_13)); } }
/* 1bc2c: pop {r3, r4, r5, pc} */ { { uint32_t base = r_13; uint32_t addr = base + 0u; uint32_t value0; value0=m8_le_read(M8_SV_HOST(addr + 0u),4); uint32_t value1; value1=m8_le_read(M8_SV_HOST(addr + 4u),4); uint32_t value2; value2=m8_le_read(M8_SV_HOST(addr + 8u),4); uint32_t value3; value3=m8_le_read(M8_SV_HOST(addr + 12u),4); r_13 = base + 16u; r_3 = (uint32_t)(value0); r_4 = (uint32_t)(value1); r_5 = (uint32_t)(value2); r_15 = (value3) & ~1u; goto L_1cd08; } }
L_1b990: r_15=0x1b990u;M8_SV_SAVE(cpu);
{int status=receive(cpu,mem);
 if(status!=M8_AOT_EXIT || state.r[15]!=0x1cd08u) {*output=*cpu;return status;}
 M8_SV_LOAD(cpu);
 if(r_4!=root || r_13!=frame || r_5>=r_8 || !m8_sampler_voice_qualify(cpu,mem,r_4,r_10,m8_le_read(M8_SV_HOST(r_13),4),r_8-r_5,0)) {r_15=0x1cd08u;goto done;}
 goto L_1cd08;
}
L_1cc2c: r_15=0x1cc2cu;goto done;
L_1cca6: r_15=0x1cca6u;goto done;
#ifdef M8_SV_EXTENDED
#include "sampler_voice_extended.h"
#else
L_1cd1c: r_15=0x1cd1cu;goto done;
#endif
#ifdef M8_SV_SHAPE
#include "sampler_voice_shape.h"
#else
L_1d122: r_15=0x1d122u;goto done;
#endif
#ifndef M8_SV_EXTENDED
L_1d354: r_15=0x1d354u;goto done;
L_1d4b8: r_15=0x1d4b8u;goto done;
#endif
L_1d5cc: r_15=0x1d5ccu;goto done;
#ifndef M8_SV_EXTENDED
L_1d624: r_15=0x1d624u;goto done;
#endif
L_1d6b6: r_15=0x1d6b6u;goto done;
L_1d780:
/* 1d780: ldrd r3, r2, [r4, #0x1b0] */ { { uint32_t base = r_4; uint32_t addr = base + (432u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); uint32_t value2; value2=m8_le_read(M8_SV_HOST(addr + 4u),4); r_3 = (uint32_t)(value); r_2 = (uint32_t)(value2); } }
/* 1d784: subs r3, r2, r3 */ { { uint32_t a = r_2; uint32_t b = r_3; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); } }
/* 1d786: lsrs r3, r3, #3 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 3, 0x3u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 1d788: cmp r3, #0x80 */ { { uint32_t a = r_3; uint32_t b = 0x80u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d78c: movhs r3, #0x80 */ if(m8_condition(cpu->xpsr,3)) { { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x80u; uint32_t result = b; r_3 = (uint32_t)(result); } } }
/* 1d78e: vmov s13, r3 */ { s_13=m8_fp_float(r_3); }
/* 1d792: vcvt.f64.s32 d6, s13 */ { M8_SV_SET_D6(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 1, 0, 32, 0, 0)); }
/* 1d796: b #0x1d046 */ { goto L_1d046; }
L_1d798:
/* 1d798: ldrb.w r3, [r4, #0x5e] */ { { uint32_t base = r_4; uint32_t addr = base + (94u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 1d79c: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1d79e: beq.w #0x1d09c */ if(m8_condition(cpu->xpsr,1)) { { goto L_1d09c; } }
/* 1d7a2: vldr s11, [r4, #0x1b0] */ { { uint32_t fp_addr = (r_4 + 432u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 1d7a6: vcvt.f64.u32 d5, s11 */ { M8_SV_SET_D5(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 1, 1, 32, 0, 0)); }
/* 1d7aa: vsub.f64 d7, d7, d5 */ { M8_SV_SET_D7(m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 0)); }
/* 1d7ae: vcmpe.f64 d7, d6 */ { m8_fp_cmp64(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 1); }
/* 1d7b2: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 1d7b6: bpl.w #0x1d09c */ if(m8_condition(cpu->xpsr,6)) { { goto L_1d09c; } }
/* 1d7ba: vdiv.f64 d5, d7, d6 */ { M8_SV_SET_D5(m8_fp_op64(cpu, M8_FP_DIV, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0)); }
/* 1d7be: vldr s14, [pc, #-0x30c] */ { { uint32_t fp_addr = (0x1d7c0u - 780u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 1d7c2: vmov.f32 s15, #1.000000e+00 */ { s_15=m8_fp_float(0x3f800000u); }
/* 1d7c6: vcvt.f32.f64 s10, d5 */ { s_10=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 1)); }
/* 1d7ca: vmaxnm.f32 s10, s10, s14 */ { s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_10), m8_fp_bits32(s_14), 0)); }
/* 1d7ce: vminnm.f32 s10, s10, s15 */ { s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_10), m8_fp_bits32(s_15), 0)); }
/* 1d7d2: b #0x1d086 */ { goto L_1d086; }
L_1d7de: r_15=0x1d7deu;goto done;
L_1d838: r_15=0x1d838u;goto done;
L_1dcb4: r_15=0x1dcb4u;goto done;
#ifndef M8_SV_EXTENDED
L_1dcf0: r_15=0x1dcf0u;goto done;
#endif
L_1df60: r_15=0x1df60u;goto done;
L_1e04c: r_15=0x1e04cu;goto done;
#ifndef M8_SV_EXTENDED
L_1e06a: r_15=0x1e06au;goto done;
#endif
L_1e192: r_15=0x1e192u;goto done;

done: M8_SV_SAVE(output);return M8_AOT_EXIT;
#undef M8_SV_HOST
#undef M8_SV_SAVE
#undef M8_SV_LOAD
#undef M8_SV_SET_D4
#undef M8_SV_SET_D5
#undef M8_SV_SET_D6
#undef M8_SV_SET_D7
#undef M8_SV_NOINLINE
#undef M8_SV_EXTENDED
#ifdef M8_SV_SHAPE
#undef M8_SV_METHOD
#undef M8_SV_SHAPE
#endif
#ifdef M8_SV_POSITION_FP
#undef m8_fp_op64
#undef m8_fp_from_int
#undef m8_fp_convert
#undef m8_fp_to_int
#undef m8_fp_cmp64
#pragma pop_macro("m8_fp_cmp64")
#pragma pop_macro("m8_fp_to_int")
#pragma pop_macro("m8_fp_convert")
#pragma pop_macro("m8_fp_from_int")
#pragma pop_macro("m8_fp_op64")
#undef M8_SV_POSITION_FP
#endif
}
#endif
#endif
