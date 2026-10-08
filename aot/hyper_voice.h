/* Whole Headless6.5.2C HYPERSYNTH mode0 block,168e8..1773a.
 * The caller supplies unchanged native15da4 and configures RN/FZ/DN once.
 * Scope: filter0,shape0,alternate mixer0,2..64 samples; the scalar
 * continuation owns every other entry or a changed control layout.
 * Qualify all effects before the first PUSH. Control updates retain their
 * original CPU/RAM/fault boundary; changed assumptions exit at175d4.
 * A negative result declines unchanged. Nonnegative results publish the
 * exact original return/child/safe continuation CPU and preserve all stores.
 * The callback must be the unchanged privatefn15da4 with stable descriptors.
 * Every entry CPU word is carried; every exit publishes all50 CPU words.
 * Twelve BLEP lanes retain original FP/lane/store order. No guest data cache.
 */
#ifndef M8_AOT_HYPER_VOICE_H
#define M8_AOT_HYPER_VOICE_H
#if defined(M8_AOT_HYPER_VOICE) && defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
#include "filter_kernel.h"
#include "integer.h"
#include "canonical_memory.h"
/* A fixed arena supplies non-overlapping guest regions. Reject all overlap
 * with CPU/private state and all overlap among render owners, including the
 * callback table and mutable literal words. This retains a scalar fallback
 * for safe unusual aliases, instead of changing their load/store ordering. */
static inline int m8_hyper_voice_regions(const m8_cpu *cpu,const m8_mem *mem) {
    return m8_filter_disjoint((uintptr_t)mem->itcm.host,mem->itcm.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->dtcm.host,mem->dtcm.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->ocram.host,mem->ocram.size,(uintptr_t)cpu,sizeof(*cpu)) &&
           m8_filter_disjoint((uintptr_t)mem->flash.host,mem->flash.size,(uintptr_t)cpu,sizeof(*cpu));
}
static inline int m8_hyper_voice_qualify(const m8_cpu *cpu,m8_mem *mem,
    uint32_t root,uint32_t left,uint32_t right,uint32_t count,int initial) {
    uintptr_t arena=m8_canonical_memory_offset(mem);
    if(!arena || !m8_hyper_voice_regions(cpu,mem) || m8_current_it ||
       !m8_fp_native_supported(cpu) || (cpu->xpsr&0x0600fc00u) ||
       !count || count>64u || (initial && cpu->r[15]!=0x168e8u)) return 0;
    uint8_t *object=m8_canonical_offset_pointer(arena,root,256u);
    if(!object || (uint32_t)(root-0x20000000u)>0x80000u-256u ||
       object[0x40]!=0 || object[0x41]!=0 || object[0xe0]!=0 || object[0x54]>7) return 0;
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
        pointer[i]=m8_canonical_offset_pointer(arena,address[i],size[i]);
        if(!pointer[i] || (address[i]&3u)) return 0;
        if(i<12 && !m8_region_pointer(&mem->dtcm,address[i],size[i]) &&
                   !m8_region_pointer(&mem->ocram,address[i],size[i])) return 0;
        for(unsigned j=0;j<i;j++)
            if(!m8_filter_disjoint((uintptr_t)pointer[i],size[i],(uintptr_t)pointer[j],size[j])) return 0;
    }
    return m8_le_read(pointer[13],4)==0x1fa5u && m8_le_read(pointer[15],4)==0x43a00000u;
}

#if defined(__GNUC__) || defined(__clang__)
#define M8_HV_NOINLINE __attribute__((noinline))
#else
#define M8_HV_NOINLINE
#endif
static M8_HV_NOINLINE int m8_hyper_voice_kernel(m8_cpu *output,m8_mem *mem,m8_aot_fn update) {
m8_cpu state,*cpu=&state;
if(output->r[3]<=1u || !update || !m8_canonical_memory_offset(mem) ||
   !m8_hyper_voice_regions(cpu,mem) ||
   !m8_hyper_voice_qualify(output,mem,output->r[0],output->r[1],output->r[2],output->r[3],1)) return -1;
uintptr_t arena=(uintptr_t)mem->itcm.host; uint32_t root=output->r[0],frame=output->r[13]-80u;
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
#define M8_HV_HOST(addr) ((uint8_t *)(arena+(uintptr_t)(uint32_t)(addr)))
#define M8_HV_SAVE(dst) do {(dst)->r[0]=r_0; (dst)->r[1]=r_1; (dst)->r[2]=r_2; (dst)->r[3]=r_3; (dst)->r[4]=r_4; (dst)->r[5]=r_5; (dst)->r[6]=r_6; (dst)->r[7]=r_7; (dst)->r[8]=r_8; (dst)->r[9]=r_9; (dst)->r[10]=r_10; (dst)->r[11]=r_11; (dst)->r[12]=r_12; (dst)->r[13]=r_13; (dst)->r[14]=r_14; (dst)->r[15]=r_15; (dst)->s[0]=m8_fp_bits32(s_0); (dst)->s[1]=m8_fp_bits32(s_1); (dst)->s[2]=m8_fp_bits32(s_2); (dst)->s[3]=m8_fp_bits32(s_3); (dst)->s[4]=m8_fp_bits32(s_4); (dst)->s[5]=m8_fp_bits32(s_5); (dst)->s[6]=m8_fp_bits32(s_6); (dst)->s[7]=m8_fp_bits32(s_7); (dst)->s[8]=m8_fp_bits32(s_8); (dst)->s[9]=m8_fp_bits32(s_9); (dst)->s[10]=m8_fp_bits32(s_10); (dst)->s[11]=m8_fp_bits32(s_11); (dst)->s[12]=m8_fp_bits32(s_12); (dst)->s[13]=m8_fp_bits32(s_13); (dst)->s[14]=m8_fp_bits32(s_14); (dst)->s[15]=m8_fp_bits32(s_15); (dst)->s[16]=m8_fp_bits32(s_16); (dst)->s[17]=m8_fp_bits32(s_17); (dst)->s[18]=m8_fp_bits32(s_18); (dst)->s[19]=m8_fp_bits32(s_19); (dst)->s[20]=m8_fp_bits32(s_20); (dst)->s[21]=m8_fp_bits32(s_21); (dst)->s[22]=m8_fp_bits32(s_22); (dst)->s[23]=m8_fp_bits32(s_23); (dst)->s[24]=m8_fp_bits32(s_24); (dst)->s[25]=m8_fp_bits32(s_25); (dst)->s[26]=m8_fp_bits32(s_26); (dst)->s[27]=m8_fp_bits32(s_27); (dst)->s[28]=m8_fp_bits32(s_28); (dst)->s[29]=m8_fp_bits32(s_29); (dst)->s[30]=m8_fp_bits32(s_30); (dst)->s[31]=m8_fp_bits32(s_31); (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr;} while(0)
#define M8_HV_LOAD(src) do {r_0=src->r[0]; r_1=src->r[1]; r_2=src->r[2]; r_3=src->r[3]; r_4=src->r[4]; r_5=src->r[5]; r_6=src->r[6]; r_7=src->r[7]; r_8=src->r[8]; r_9=src->r[9]; r_10=src->r[10]; r_11=src->r[11]; r_12=src->r[12]; r_13=src->r[13]; r_14=src->r[14]; r_15=src->r[15]; s_0=m8_fp_float(src->s[0]); s_1=m8_fp_float(src->s[1]); s_2=m8_fp_float(src->s[2]); s_3=m8_fp_float(src->s[3]); s_4=m8_fp_float(src->s[4]); s_5=m8_fp_float(src->s[5]); s_6=m8_fp_float(src->s[6]); s_7=m8_fp_float(src->s[7]); s_8=m8_fp_float(src->s[8]); s_9=m8_fp_float(src->s[9]); s_10=m8_fp_float(src->s[10]); s_11=m8_fp_float(src->s[11]); s_12=m8_fp_float(src->s[12]); s_13=m8_fp_float(src->s[13]); s_14=m8_fp_float(src->s[14]); s_15=m8_fp_float(src->s[15]); s_16=m8_fp_float(src->s[16]); s_17=m8_fp_float(src->s[17]); s_18=m8_fp_float(src->s[18]); s_19=m8_fp_float(src->s[19]); s_20=m8_fp_float(src->s[20]); s_21=m8_fp_float(src->s[21]); s_22=m8_fp_float(src->s[22]); s_23=m8_fp_float(src->s[23]); s_24=m8_fp_float(src->s[24]); s_25=m8_fp_float(src->s[25]); s_26=m8_fp_float(src->s[26]); s_27=m8_fp_float(src->s[27]); s_28=m8_fp_float(src->s[28]); s_29=m8_fp_float(src->s[29]); s_30=m8_fp_float(src->s[30]); s_31=m8_fp_float(src->s[31]); cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr;} while(0)
#define M8_HV_SET_D9(value) do {uint64_t v=(value);s_18=m8_fp_float((uint32_t)v);s_19=m8_fp_float((uint32_t)(v>>32));} while(0)
/* 168e8: push.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, lr} */ { { uint32_t base = r_13; uint32_t addr = base + 4294967256u; uint32_t value0 = r_3; uint32_t value1 = r_4; uint32_t value2 = r_5; uint32_t value3 = r_6; uint32_t value4 = r_7; uint32_t value5 = r_8; uint32_t value6 = r_9; uint32_t value7 = r_10; uint32_t value8 = r_11; uint32_t value9 = r_14; m8_le_write(M8_HV_HOST(addr + 0u),4,value0); m8_le_write(M8_HV_HOST(addr + 4u),4,value1); m8_le_write(M8_HV_HOST(addr + 8u),4,value2); m8_le_write(M8_HV_HOST(addr + 12u),4,value3); m8_le_write(M8_HV_HOST(addr + 16u),4,value4); m8_le_write(M8_HV_HOST(addr + 20u),4,value5); m8_le_write(M8_HV_HOST(addr + 24u),4,value6); m8_le_write(M8_HV_HOST(addr + 28u),4,value7); m8_le_write(M8_HV_HOST(addr + 32u),4,value8); m8_le_write(M8_HV_HOST(addr + 36u),4,value9); r_13 = base - 40u; } }
/* 168ec: mov r8, r0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_0; uint32_t result = b; r_8 = (uint32_t)(result); } }
/* 168ee: ldr r0, [r0, #0x58] */ { { uint32_t base = r_0; uint32_t addr = base + (88u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 168f0: vpush {d8, d9, d10, d11, d12} */ { { uint32_t fp_addr = (r_13 - 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); m8_le_write(M8_HV_HOST(fp_addr + 4u),4,m8_fp_bits32(s_17)); m8_le_write(M8_HV_HOST(fp_addr + 8u),4,m8_fp_bits32(s_18)); m8_le_write(M8_HV_HOST(fp_addr + 12u),4,m8_fp_bits32(s_19)); m8_le_write(M8_HV_HOST(fp_addr + 16u),4,m8_fp_bits32(s_20)); m8_le_write(M8_HV_HOST(fp_addr + 20u),4,m8_fp_bits32(s_21)); m8_le_write(M8_HV_HOST(fp_addr + 24u),4,m8_fp_bits32(s_22)); m8_le_write(M8_HV_HOST(fp_addr + 28u),4,m8_fp_bits32(s_23)); m8_le_write(M8_HV_HOST(fp_addr + 32u),4,m8_fp_bits32(s_24)); m8_le_write(M8_HV_HOST(fp_addr + 36u),4,m8_fp_bits32(s_25)); r_13 -= 40u; } }
/* 168f4: cmp r0, #0 */ { { uint32_t a = r_0; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 168f6: beq.w #0x17732 */ { if (m8_condition(cpu->xpsr,1)) { goto L_17732; } }
/* 168fa: mov sb, r1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_1; uint32_t result = b; r_9 = (uint32_t)(result); } }
/* 168fc: mov fp, r2 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_2; uint32_t result = b; r_11 = (uint32_t)(result); } }
/* 168fe: ldr.w r0, [r8, #0xf4] */ { { uint32_t base = r_8; uint32_t addr = base + (244u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 16902: ldrd r1, r2, [r8, #0xec] */ { { uint32_t base = r_8; uint32_t addr = base + (236u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); uint32_t value2; value2=m8_le_read(M8_HV_HOST(addr + 4u),4); r_1 = (uint32_t)(value); r_2 = (uint32_t)(value2); } }
/* 16906: ldr r4, [r1] */ { { uint32_t base = r_1; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16908: str r4, [r2] */ { { uint32_t base = r_2; uint32_t addr = base + (0u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1690a: ldr r4, [r1, #4] */ { { uint32_t base = r_1; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 1690c: str r4, [r2, #4] */ { { uint32_t base = r_2; uint32_t addr = base + (4u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1690e: ldr r4, [r1, #8] */ { { uint32_t base = r_1; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16910: str r4, [r2, #8] */ { { uint32_t base = r_2; uint32_t addr = base + (8u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16912: ldr r4, [r1, #0xc] */ { { uint32_t base = r_1; uint32_t addr = base + (12u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16914: str r4, [r2, #0xc] */ { { uint32_t base = r_2; uint32_t addr = base + (12u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16916: ldr r4, [r1, #0x10] */ { { uint32_t base = r_1; uint32_t addr = base + (16u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16918: str r4, [r2, #0x10] */ { { uint32_t base = r_2; uint32_t addr = base + (16u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1691a: ldr r4, [r1, #0x14] */ { { uint32_t base = r_1; uint32_t addr = base + (20u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 1691c: str r4, [r2, #0x14] */ { { uint32_t base = r_2; uint32_t addr = base + (20u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1691e: ldr r4, [r1, #0x18] */ { { uint32_t base = r_1; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16920: str r4, [r2, #0x18] */ { { uint32_t base = r_2; uint32_t addr = base + (24u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16922: ldr r4, [r1, #0x1c] */ { { uint32_t base = r_1; uint32_t addr = base + (28u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16924: str r4, [r2, #0x1c] */ { { uint32_t base = r_2; uint32_t addr = base + (28u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16926: ldr r4, [r1, #0x20] */ { { uint32_t base = r_1; uint32_t addr = base + (32u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16928: str r4, [r2, #0x20] */ { { uint32_t base = r_2; uint32_t addr = base + (32u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1692a: ldr r4, [r1, #0x24] */ { { uint32_t base = r_1; uint32_t addr = base + (36u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 1692c: str r4, [r2, #0x24] */ { { uint32_t base = r_2; uint32_t addr = base + (36u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1692e: ldr r4, [r1, #0x28] */ { { uint32_t base = r_1; uint32_t addr = base + (40u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16930: str r4, [r2, #0x28] */ { { uint32_t base = r_2; uint32_t addr = base + (40u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16932: ldr r1, [r1, #0x2c] */ { { uint32_t base = r_1; uint32_t addr = base + (44u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_1 = (uint32_t)(value); } }
/* 16934: str r1, [r2, #0x2c] */ { { uint32_t base = r_2; uint32_t addr = base + (44u); uint32_t value = r_1; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16936: ldr.w r1, [r8, #0x9c] */ { { uint32_t base = r_8; uint32_t addr = base + (156u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_1 = (uint32_t)(value); } }
/* 1693a: str r1, [r2, #0x30] */ { { uint32_t base = r_2; uint32_t addr = base + (48u); uint32_t value = r_1; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1693c: ldr.w r2, [r8, #0x44] */ { { uint32_t base = r_8; uint32_t addr = base + (68u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 16940: ldr.w r5, [r8, #0xa8] */ { { uint32_t base = r_8; uint32_t addr = base + (168u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_5 = (uint32_t)(value); } }
/* 16944: str.w r2, [r8, #0x4c] */ { { uint32_t base = r_8; uint32_t addr = base + (76u); uint32_t value = r_2; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16948: ldr.w r4, [r8, #0xac] */ { { uint32_t base = r_8; uint32_t addr = base + (172u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 1694c: ldr.w r1, [r8, #0xb0] */ { { uint32_t base = r_8; uint32_t addr = base + (176u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_1 = (uint32_t)(value); } }
/* 16950: ldr.w r2, [r8, #0xb4] */ { { uint32_t base = r_8; uint32_t addr = base + (180u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 16954: str r5, [r0] */ { { uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value = r_5; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16956: str r4, [r0, #4] */ { { uint32_t base = r_0; uint32_t addr = base + (4u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16958: str r1, [r0, #8] */ { { uint32_t base = r_0; uint32_t addr = base + (8u); uint32_t value = r_1; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1695a: str r2, [r0, #0xc] */ { { uint32_t base = r_0; uint32_t addr = base + (12u); uint32_t value = r_2; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1695c: ldr.w r5, [r8, #0xb8] */ { { uint32_t base = r_8; uint32_t addr = base + (184u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_5 = (uint32_t)(value); } }
/* 16960: ldr.w r4, [r8, #0xbc] */ { { uint32_t base = r_8; uint32_t addr = base + (188u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16964: ldr.w r1, [r8, #0xc0] */ { { uint32_t base = r_8; uint32_t addr = base + (192u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_1 = (uint32_t)(value); } }
/* 16968: ldr.w r2, [r8, #0xc4] */ { { uint32_t base = r_8; uint32_t addr = base + (196u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 1696c: str r5, [r0, #0x10] */ { { uint32_t base = r_0; uint32_t addr = base + (16u); uint32_t value = r_5; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 1696e: str r4, [r0, #0x14] */ { { uint32_t base = r_0; uint32_t addr = base + (20u); uint32_t value = r_4; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16970: str r1, [r0, #0x18] */ { { uint32_t base = r_0; uint32_t addr = base + (24u); uint32_t value = r_1; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16972: str r2, [r0, #0x1c] */ { { uint32_t base = r_0; uint32_t addr = base + (28u); uint32_t value = r_2; m8_le_write(M8_HV_HOST(addr),4,value); } }
/* 16974: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 16976: beq.w #0x17730 */ { if (m8_condition(cpu->xpsr,1)) { goto L_17730; } }
/* 1697a: vmov.f32 s17, #1.000000e+00 */ { s_17=m8_fp_float(0x3f800000u); }
/* 1697e: add.w sl, fp, r3, lsl #2 */ { { uint32_t a = r_11; uint32_t b = m8_shift(r_3, 2, 2, NULL); uint32_t result = a + b + 0u; r_10 = (uint32_t)(result); } }
/* 16982: vmov.f32 s22, #5.000000e-01 */ { s_22=m8_fp_float(0x3f000000u); }
/* 16986: vldr s23, [pc, #0x18] */ { { uint32_t fp_addr = (0x16988u + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_23=m8_fp_float(fp_value); } }
/* 1698a: vldr s21, [pc, #0x18] */ { { uint32_t fp_addr = (0x1698cu + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_21=m8_fp_float(fp_value); } }
/* 1698e: vldr d9, [pc, #8] */ { { uint32_t fp_addr = (0x16990u + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_18=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 4u),4); s_19=m8_fp_float(fp_value); } }
/* 16992: b.w #0x1732c */ { goto L_1732c; } goto L_1732c;
/* 169a8: adds r3, #1 */ L_169a8: { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); } }
/* 169aa: uxtb r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 255u); } }
/* 169ac: ldr.w r6, [r8, #0x58] */ L_169ac: { { uint32_t base = r_8; uint32_t addr = base + (88u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_6 = (uint32_t)(value); } }
/* 169b0: strb.w r3, [r8, #0x54] */ { { uint32_t base = r_8; uint32_t addr = base + (84u); uint32_t value = r_3; m8_le_write(M8_HV_HOST(addr),1,value); } }
/* 169b4: vldr s14, [r6, #0x28] */ { { uint32_t fp_addr = (r_6 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 169b8: vldr s0, [r6, #0x78] */ { { uint32_t fp_addr = (r_6 + 120u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_0=m8_fp_float(fp_value); } }
/* 169bc: vmul.f32 s14, s15, s14 */ { s_14=m8_filter_op(cpu,M8_FP_MUL,s_15,s_14,0); }
/* 169c0: vldr s5, [r6, #0x18] */ { { uint32_t fp_addr = (r_6 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_5=m8_fp_float(fp_value); } }
/* 169c4: vldr s6, [r6, #4] */ { { uint32_t fp_addr = (r_6 + 4u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); } }
/* 169c8: vldr s7, [r6, #0x1c] */ { { uint32_t fp_addr = (r_6 + 28u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); } }
/* 169cc: vfma.f32 s14, s13, s0 */ { s_14=m8_filter_op(cpu,M8_FP_FMA,s_13,s_0,s_14); }
/* 169d0: vldr s8, [r6, #0x20] */ { { uint32_t fp_addr = (r_6 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 169d4: vldr s9, [r6, #0x24] */ { { uint32_t fp_addr = (r_6 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 169d8: vmul.f32 s6, s15, s6 */ { s_6=m8_filter_op(cpu,M8_FP_MUL,s_15,s_6,0); }
/* 169dc: vldr s10, [r6, #0xc] */ { { uint32_t fp_addr = (r_6 + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); } }
/* 169e0: vmul.f32 s7, s15, s7 */ { s_7=m8_filter_op(cpu,M8_FP_MUL,s_15,s_7,0); }
/* 169e4: vldr s11, [r6, #0x10] */ { { uint32_t fp_addr = (r_6 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 169e8: vmul.f32 s8, s15, s8 */ { s_8=m8_filter_op(cpu,M8_FP_MUL,s_15,s_8,0); }
/* 169ec: vldr s12, [r6, #0x14] */ { { uint32_t fp_addr = (r_6 + 20u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 169f0: vmul.f32 s9, s15, s9 */ { s_9=m8_filter_op(cpu,M8_FP_MUL,s_15,s_9,0); }
/* 169f4: vmul.f32 s10, s15, s10 */ { s_10=m8_filter_op(cpu,M8_FP_MUL,s_15,s_10,0); }
/* 169f8: vldr s24, [r6, #0x54] */ { { uint32_t fp_addr = (r_6 + 84u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_24=m8_fp_float(fp_value); } }
/* 169fc: vmul.f32 s11, s15, s11 */ { s_11=m8_filter_op(cpu,M8_FP_MUL,s_15,s_11,0); }
/* 16a00: vldr s20, [r6, #0x6c] */ { { uint32_t fp_addr = (r_6 + 108u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); } }
/* 16a04: vmul.f32 s12, s15, s12 */ { s_12=m8_filter_op(cpu,M8_FP_MUL,s_15,s_12,0); }
/* 16a08: vldr s16, [r6, #0x70] */ { { uint32_t fp_addr = (r_6 + 112u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); } }
/* 16a0c: vmul.f32 s15, s15, s5 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_15,s_5,0); }
/* 16a10: vldr s1, [r6, #0x74] */ { { uint32_t fp_addr = (r_6 + 116u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); } }
/* 16a14: vldr s2, [r6, #0x5c] */ { { uint32_t fp_addr = (r_6 + 92u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_2=m8_fp_float(fp_value); } }
/* 16a18: vfma.f32 s6, s13, s24 */ { s_6=m8_filter_op(cpu,M8_FP_FMA,s_13,s_24,s_6); }
/* 16a1c: vldr s3, [r6, #0x60] */ { { uint32_t fp_addr = (r_6 + 96u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_3=m8_fp_float(fp_value); } }
/* 16a20: vfma.f32 s7, s13, s20 */ { s_7=m8_filter_op(cpu,M8_FP_FMA,s_13,s_20,s_7); }
/* 16a24: vldr s4, [r6, #0x64] */ { { uint32_t fp_addr = (r_6 + 100u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_4=m8_fp_float(fp_value); } }
/* 16a28: vfma.f32 s8, s13, s16 */ { s_8=m8_filter_op(cpu,M8_FP_FMA,s_13,s_16,s_8); }
/* 16a2c: vldr s5, [r6, #0x68] */ { { uint32_t fp_addr = (r_6 + 104u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_5=m8_fp_float(fp_value); } }
/* 16a30: vfma.f32 s9, s13, s1 */ { s_9=m8_filter_op(cpu,M8_FP_FMA,s_13,s_1,s_9); }
/* 16a34: vfma.f32 s10, s13, s2 */ { s_10=m8_filter_op(cpu,M8_FP_FMA,s_13,s_2,s_10); }
/* 16a38: vstr s14, [r6, #0x50] */ { { uint32_t fp_addr = (r_6 + 80u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16a3c: vfma.f32 s11, s13, s3 */ { s_11=m8_filter_op(cpu,M8_FP_FMA,s_13,s_3,s_11); }
/* 16a40: vfma.f32 s12, s13, s4 */ { s_12=m8_filter_op(cpu,M8_FP_FMA,s_13,s_4,s_12); }
/* 16a44: vstr s6, [r6, #0x2c] */ { { uint32_t fp_addr = (r_6 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_6)); } }
/* 16a48: vfma.f32 s15, s13, s5 */ { s_15=m8_filter_op(cpu,M8_FP_FMA,s_13,s_5,s_15); }
/* 16a4c: vstr s7, [r6, #0x44] */ { { uint32_t fp_addr = (r_6 + 68u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16a50: vcmpe.f32 s14, s22 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_14), m8_fp_bits32(s_22), 1); }
/* 16a54: vstr s8, [r6, #0x48] */ { { uint32_t fp_addr = (r_6 + 72u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 16a58: vstr s9, [r6, #0x4c] */ { { uint32_t fp_addr = (r_6 + 76u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16a5c: vstr s10, [r6, #0x34] */ { { uint32_t fp_addr = (r_6 + 52u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 16a60: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 16a64: vstr s11, [r6, #0x38] */ { { uint32_t fp_addr = (r_6 + 56u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 16a68: vstr s12, [r6, #0x3c] */ { { uint32_t fp_addr = (r_6 + 60u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16a6c: vstr s15, [r6, #0x40] */ { { uint32_t fp_addr = (r_6 + 64u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 16a70: blt.w #0x17418 */ { if (m8_condition(cpu->xpsr,12)) { goto L_17418; } }
/* 16a74: vsub.f32 s14, s22, s14 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_22,s_14,0); }
/* 16a78: vmov.f32 s15, #2.000000e+00 */ { s_15=m8_fp_float(0x40000000u); }
/* 16a7c: vmov.f32 s25, s17 */ { s_25=m8_fp_float(m8_fp_bits32(s_17)); }
/* 16a80: vmov.f32 s24, #1.000000e+00 */ { s_24=m8_fp_float(0x3f800000u); }
/* 16a84: vfma.f32 s25, s14, s15 */ { s_25=m8_filter_op(cpu,M8_FP_FMA,s_14,s_15,s_25); }
/* 16a88: ldr.w r5, [r8, #0xec] */ L_16a88: { { uint32_t base = r_8; uint32_t addr = base + (236u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_5 = (uint32_t)(value); } }
/* 16a8c: ldr.w r7, [r8, #0xcc] */ { { uint32_t base = r_8; uint32_t addr = base + (204u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_7 = (uint32_t)(value); } }
/* 16a90: vldr s13, [r5] */ { { uint32_t fp_addr = (r_5 + 0u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16a94: vldr s12, [r7] */ { { uint32_t fp_addr = (r_7 + 0u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16a98: vldr s14, [r5, #4] */ { { uint32_t fp_addr = (r_5 + 4u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16a9c: vadd.f32 s13, s13, s12 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_12,0); }
/* 16aa0: vldr s15, [r5, #8] */ { { uint32_t fp_addr = (r_5 + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 16aa4: vldr s7, [r5, #0xc] */ { { uint32_t fp_addr = (r_5 + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); } }
/* 16aa8: vldr s8, [r5, #0x10] */ { { uint32_t fp_addr = (r_5 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 16aac: vstr s13, [r5] */ { { uint32_t fp_addr = (r_5 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16ab0: vcvt.s32.f32 s11, s13 */ { s_11=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 0, 3, 32, 0)); }
/* 16ab4: vldr s12, [r7, #4] */ { { uint32_t fp_addr = (r_7 + 4u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16ab8: vldr s16, [r5, #0x14] */ { { uint32_t fp_addr = (r_5 + 20u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); } }
/* 16abc: vadd.f32 s14, s14, s12 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_12,0); }
/* 16ac0: vldr s0, [r5, #0x18] */ { { uint32_t fp_addr = (r_5 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_0=m8_fp_float(fp_value); } }
/* 16ac4: vcvt.f32.s32 s11, s11 */ { s_11=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 0, 0, 32, 0, 0)); }
/* 16ac8: vldr s10, [r5, #0x1c] */ { { uint32_t fp_addr = (r_5 + 28u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); } }
/* 16acc: vldr s2, [r5, #0x20] */ { { uint32_t fp_addr = (r_5 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_2=m8_fp_float(fp_value); } }
/* 16ad0: vstr s14, [r5, #4] */ { { uint32_t fp_addr = (r_5 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16ad4: vcvt.s32.f32 s12, s14 */ { s_12=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16ad8: vldr s9, [r7, #8] */ { { uint32_t fp_addr = (r_7 + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 16adc: vsub.f32 s4, s13, s11 */ { s_4=m8_filter_op(cpu,M8_FP_SUB,s_13,s_11,0); }
/* 16ae0: vldr s1, [r5, #0x24] */ { { uint32_t fp_addr = (r_5 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); } }
/* 16ae4: vadd.f32 s15, s15, s9 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_9,0); }
/* 16ae8: vcvt.f32.s32 s12, s12 */ { s_12=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_12), 0, 0, 32, 0, 0)); }
/* 16aec: vadd.f32 s3, s4, s22 */ { s_3=m8_filter_op(cpu,M8_FP_ADD,s_4,s_22,0); }
/* 16af0: vstr s15, [r5, #8] */ { { uint32_t fp_addr = (r_5 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 16af4: vcvt.s32.f32 s13, s15 */ { s_13=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_15), 0, 0, 3, 32, 0)); }
/* 16af8: vldr s11, [r7, #0xc] */ { { uint32_t fp_addr = (r_7 + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 16afc: vsub.f32 s5, s14, s12 */ { s_5=m8_filter_op(cpu,M8_FP_SUB,s_14,s_12,0); }
/* 16b00: vadd.f32 s7, s7, s11 */ { s_7=m8_filter_op(cpu,M8_FP_ADD,s_7,s_11,0); }
/* 16b04: vcvt.f32.s32 s13, s13 */ { s_13=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 0, 0, 32, 0, 0)); }
/* 16b08: vcvt.s32.f32 s11, s3 */ { s_11=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_3), 0, 0, 3, 32, 0)); }
/* 16b0c: vstr s7, [r5, #0xc] */ { { uint32_t fp_addr = (r_5 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16b10: vcvt.s32.f32 s14, s7 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_7), 0, 0, 3, 32, 0)); }
/* 16b14: vldr s12, [r7, #0x10] */ { { uint32_t fp_addr = (r_7 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16b18: vsub.f32 s6, s15, s13 */ { s_6=m8_filter_op(cpu,M8_FP_SUB,s_15,s_13,0); }
/* 16b1c: vcvt.f32.s32 s11, s11 */ { s_11=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_11), 0, 0, 32, 0, 0)); }
/* 16b20: vadd.f32 s8, s8, s12 */ { s_8=m8_filter_op(cpu,M8_FP_ADD,s_8,s_12,0); }
/* 16b24: vcvt.f32.s32 s15, s14 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0)); }
/* 16b28: vsub.f32 s11, s3, s11 */ { s_11=m8_filter_op(cpu,M8_FP_SUB,s_3,s_11,0); }
/* 16b2c: vstr s8, [r5, #0x10] */ { { uint32_t fp_addr = (r_5 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 16b30: vcvt.s32.f32 s14, s8 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_8), 0, 0, 3, 32, 0)); }
/* 16b34: vldr s9, [r7, #0x14] */ { { uint32_t fp_addr = (r_7 + 20u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 16b38: vsub.f32 s7, s7, s15 */ { s_7=m8_filter_op(cpu,M8_FP_SUB,s_7,s_15,0); }
/* 16b3c: vadd.f32 s9, s16, s9 */ { s_9=m8_filter_op(cpu,M8_FP_ADD,s_16,s_9,0); }
/* 16b40: vcvt.f32.s32 s14, s14 */ { s_14=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0)); }
/* 16b44: vstr s9, [r5, #0x14] */ { { uint32_t fp_addr = (r_5 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16b48: vcvt.s32.f32 s15, s9 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_9), 0, 0, 3, 32, 0)); }
/* 16b4c: vldr s12, [r7, #0x18] */ { { uint32_t fp_addr = (r_7 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16b50: vsub.f32 s8, s8, s14 */ { s_8=m8_filter_op(cpu,M8_FP_SUB,s_8,s_14,0); }
/* 16b54: vadd.f32 s12, s0, s12 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_0,s_12,0); }
/* 16b58: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
/* 16b5c: vstr s12, [r5, #0x18] */ { { uint32_t fp_addr = (r_5 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16b60: vcvt.s32.f32 s14, s12 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_12), 0, 0, 3, 32, 0)); }
/* 16b64: vldr s16, [r7, #0x1c] */ { { uint32_t fp_addr = (r_7 + 28u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); } }
/* 16b68: vsub.f32 s9, s9, s15 */ { s_9=m8_filter_op(cpu,M8_FP_SUB,s_9,s_15,0); }
/* 16b6c: vadd.f32 s16, s10, s16 */ { s_16=m8_filter_op(cpu,M8_FP_ADD,s_10,s_16,0); }
/* 16b70: vcvt.f32.s32 s14, s14 */ { s_14=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0)); }
/* 16b74: vadd.f32 s10, s11, s11 */ { s_10=m8_filter_op(cpu,M8_FP_ADD,s_11,s_11,0); }
/* 16b78: vstr s16, [r5, #0x1c] */ { { uint32_t fp_addr = (r_5 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); } }
/* 16b7c: vcvt.s32.f32 s15, s16 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_16), 0, 0, 3, 32, 0)); }
/* 16b80: vldr s0, [r7, #0x20] */ { { uint32_t fp_addr = (r_7 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_0=m8_fp_float(fp_value); } }
/* 16b84: vsub.f32 s12, s12, s14 */ { s_12=m8_filter_op(cpu,M8_FP_SUB,s_12,s_14,0); }
/* 16b88: vadd.f32 s0, s2, s0 */ { s_0=m8_filter_op(cpu,M8_FP_ADD,s_2,s_0,0); }
/* 16b8c: vcvt.f32.s32 s15, s15 */ { s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0)); }
/* 16b90: vstr s0, [r5, #0x20] */ { { uint32_t fp_addr = (r_5 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_0)); } }
/* 16b94: vcvt.s32.f32 s2, s0 */ { s_2=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_0), 0, 0, 3, 32, 0)); }
/* 16b98: vldr s13, [r7, #0x24] */ { { uint32_t fp_addr = (r_7 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16b9c: vsub.f32 s16, s16, s15 */ { s_16=m8_filter_op(cpu,M8_FP_SUB,s_16,s_15,0); }
/* 16ba0: vadd.f32 s13, s1, s13 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_1,s_13,0); }
/* 16ba4: vcvt.f32.s32 s2, s2 */ { s_2=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_2), 0, 0, 32, 0, 0)); }
/* 16ba8: vstr s13, [r5, #0x24] */ { { uint32_t fp_addr = (r_5 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16bac: vcvt.s32.f32 s1, s13 */ { s_1=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 0, 3, 32, 0)); }
/* 16bb0: vldr s15, [r5, #0x28] */ { { uint32_t fp_addr = (r_5 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 16bb4: vsub.f32 s0, s0, s2 */ { s_0=m8_filter_op(cpu,M8_FP_SUB,s_0,s_2,0); }
/* 16bb8: vldr s20, [r7, #0x28] */ { { uint32_t fp_addr = (r_7 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); } }
/* 16bbc: ldr.w r4, [r8, #0xd4] */ { { uint32_t base = r_8; uint32_t addr = base + (212u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_4 = (uint32_t)(value); } }
/* 16bc0: vcvt.f32.s32 s1, s1 */ { s_1=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_1), 0, 0, 32, 0, 0)); }
/* 16bc4: vadd.f32 s15, s15, s20 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_20,0); }
/* 16bc8: vldr s14, [r5, #0x2c] */ { { uint32_t fp_addr = (r_5 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16bcc: vsub.f32 s13, s13, s1 */ { s_13=m8_filter_op(cpu,M8_FP_SUB,s_13,s_1,0); }
/* 16bd0: vstr s15, [r5, #0x28] */ { { uint32_t fp_addr = (r_5 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 16bd4: vcvt.s32.f32 s2, s15 */ { s_2=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_15), 0, 0, 3, 32, 0)); }
/* 16bd8: vldr s20, [r7, #0x2c] */ { { uint32_t fp_addr = (r_7 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); } }
/* 16bdc: vstr s8, [r5, #0x10] */ { { uint32_t fp_addr = (r_5 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 16be0: vadd.f32 s14, s14, s20 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_20,0); }
/* 16be4: vstr s7, [r5, #0xc] */ { { uint32_t fp_addr = (r_5 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16be8: vstr s12, [r5, #0x18] */ { { uint32_t fp_addr = (r_5 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16bec: vcvt.f32.s32 s7, s2 */ { s_7=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_2), 0, 0, 32, 0, 0)); }
/* 16bf0: vstr s5, [r5, #4] */ { { uint32_t fp_addr = (r_5 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_5)); } }
/* 16bf4: vcvt.s32.f32 s8, s14 */ { s_8=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16bf8: vstr s6, [r5, #8] */ { { uint32_t fp_addr = (r_5 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_6)); } }
/* 16bfc: vsub.f32 s15, s15, s7 */ { s_15=m8_filter_op(cpu,M8_FP_SUB,s_15,s_7,0); }
/* 16c00: vstr s9, [r5, #0x14] */ { { uint32_t fp_addr = (r_5 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16c04: vstr s16, [r5, #0x1c] */ { { uint32_t fp_addr = (r_5 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); } }
/* 16c08: vmov.f32 s6, #2.000000e+00 */ { s_6=m8_fp_float(0x40000000u); }
/* 16c0c: vcvt.f32.s32 s12, s8 */ { s_12=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_8), 0, 0, 32, 0, 0)); }
/* 16c10: vstr s4, [r5] */ { { uint32_t fp_addr = (r_5 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_4)); } }
/* 16c14: vstr s0, [r5, #0x20] */ { { uint32_t fp_addr = (r_5 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_0)); } }
/* 16c18: vmov.f32 s8, #-1.000000e+00 */ { s_8=m8_fp_float(0xbf800000u); }
/* 16c1c: vstr s13, [r5, #0x24] */ { { uint32_t fp_addr = (r_5 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16c20: vsub.f32 s14, s14, s12 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_12,0); }
/* 16c24: vstr s15, [r5, #0x28] */ { { uint32_t fp_addr = (r_5 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 16c28: vsub.f32 s15, s10, s17 */ { s_15=m8_filter_op(cpu,M8_FP_SUB,s_10,s_17,0); }
/* 16c2c: vstr s14, [r5, #0x2c] */ { { uint32_t fp_addr = (r_5 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16c30: vstr s3, [r4] */ { { uint32_t fp_addr = (r_4 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_3)); } }
/* 16c34: vldr s14, [r5, #4] */ { { uint32_t fp_addr = (r_5 + 4u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16c38: vadd.f32 s14, s14, s22 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_22,0); }
/* 16c3c: vstr s14, [r4, #4] */ { { uint32_t fp_addr = (r_4 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16c40: vcvt.s32.f32 s9, s14 */ { s_9=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16c44: vldr s12, [r5, #8] */ { { uint32_t fp_addr = (r_5 + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16c48: vadd.f32 s12, s12, s22 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_22,0); }
/* 16c4c: vcvt.f32.s32 s9, s9 */ { s_9=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_9), 0, 0, 32, 0, 0)); }
/* 16c50: vstr s12, [r4, #8] */ { { uint32_t fp_addr = (r_4 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16c54: vcvt.s32.f32 s7, s12 */ { s_7=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_12), 0, 0, 3, 32, 0)); }
/* 16c58: vldr s13, [r5, #0xc] */ { { uint32_t fp_addr = (r_5 + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16c5c: vsub.f32 s3, s14, s9 */ { s_3=m8_filter_op(cpu,M8_FP_SUB,s_14,s_9,0); }
/* 16c60: vadd.f32 s13, s13, s22 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_22,0); }
/* 16c64: vcvt.f32.s32 s7, s7 */ { s_7=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_7), 0, 0, 32, 0, 0)); }
/* 16c68: vstr s13, [r4, #0xc] */ { { uint32_t fp_addr = (r_4 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16c6c: vcvt.s32.f32 s9, s13 */ { s_9=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 0, 3, 32, 0)); }
/* 16c70: vldr s14, [r5, #0x10] */ { { uint32_t fp_addr = (r_5 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16c74: vsub.f32 s4, s12, s7 */ { s_4=m8_filter_op(cpu,M8_FP_SUB,s_12,s_7,0); }
/* 16c78: vadd.f32 s14, s14, s22 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_22,0); }
/* 16c7c: vcvt.f32.s32 s9, s9 */ { s_9=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_9), 0, 0, 32, 0, 0)); }
/* 16c80: vstr s14, [r4, #0x10] */ { { uint32_t fp_addr = (r_4 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16c84: vcvt.s32.f32 s7, s14 */ { s_7=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16c88: vldr s12, [r5, #0x14] */ { { uint32_t fp_addr = (r_5 + 20u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16c8c: vsub.f32 s5, s13, s9 */ { s_5=m8_filter_op(cpu,M8_FP_SUB,s_13,s_9,0); }
/* 16c90: vadd.f32 s12, s12, s22 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_22,0); }
/* 16c94: vcvt.f32.s32 s7, s7 */ { s_7=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_7), 0, 0, 32, 0, 0)); }
/* 16c98: vstr s12, [r4, #0x14] */ { { uint32_t fp_addr = (r_4 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16c9c: vcvt.s32.f32 s9, s12 */ { s_9=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_12), 0, 0, 3, 32, 0)); }
/* 16ca0: vldr s13, [r5, #0x18] */ { { uint32_t fp_addr = (r_5 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16ca4: vsub.f32 s7, s14, s7 */ { s_7=m8_filter_op(cpu,M8_FP_SUB,s_14,s_7,0); }
/* 16ca8: vadd.f32 s13, s13, s22 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_22,0); }
/* 16cac: vcvt.f32.s32 s9, s9 */ { s_9=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_9), 0, 0, 32, 0, 0)); }
/* 16cb0: vstr s13, [r4, #0x18] */ { { uint32_t fp_addr = (r_4 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16cb4: vcvt.s32.f32 s2, s13 */ { s_2=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 0, 3, 32, 0)); }
/* 16cb8: vldr s14, [r5, #0x1c] */ { { uint32_t fp_addr = (r_5 + 28u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16cbc: vsub.f32 s9, s12, s9 */ { s_9=m8_filter_op(cpu,M8_FP_SUB,s_12,s_9,0); }
/* 16cc0: vadd.f32 s14, s14, s22 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_22,0); }
/* 16cc4: vcvt.f32.s32 s12, s2 */ { s_12=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_2), 0, 0, 32, 0, 0)); }
/* 16cc8: vstr s14, [r4, #0x1c] */ { { uint32_t fp_addr = (r_4 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16ccc: vcvt.s32.f32 s1, s14 */ { s_1=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16cd0: vldr s20, [r5, #0x20] */ { { uint32_t fp_addr = (r_5 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); } }
/* 16cd4: vsub.f32 s0, s13, s12 */ { s_0=m8_filter_op(cpu,M8_FP_SUB,s_13,s_12,0); }
/* 16cd8: ldr.w r3, [r8, #0xd0] */ { { uint32_t base = r_8; uint32_t addr = base + (208u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 16cdc: vadd.f32 s20, s20, s22 */ { s_20=m8_filter_op(cpu,M8_FP_ADD,s_20,s_22,0); }
/* 16ce0: vcvt.f32.s32 s1, s1 */ { s_1=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_1), 0, 0, 32, 0, 0)); }
/* 16ce4: vstr s20, [r4, #0x20] */ { { uint32_t fp_addr = (r_4 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_20)); } }
/* 16ce8: vcvt.s32.f32 s2, s20 */ { s_2=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_20), 0, 0, 3, 32, 0)); }
/* 16cec: vldr s12, [r5, #0x24] */ { { uint32_t fp_addr = (r_5 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16cf0: vsub.f32 s16, s14, s1 */ { s_16=m8_filter_op(cpu,M8_FP_SUB,s_14,s_1,0); }
/* 16cf4: vadd.f32 s12, s12, s22 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_22,0); }
/* 16cf8: vcvt.f32.s32 s2, s2 */ { s_2=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_2), 0, 0, 32, 0, 0)); }
/* 16cfc: vstr s12, [r4, #0x24] */ { { uint32_t fp_addr = (r_4 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16d00: vcvt.s32.f32 s1, s12 */ { s_1=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_12), 0, 0, 3, 32, 0)); }
/* 16d04: vldr s13, [r5, #0x28] */ { { uint32_t fp_addr = (r_5 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16d08: vsub.f32 s20, s20, s2 */ { s_20=m8_filter_op(cpu,M8_FP_SUB,s_20,s_2,0); }
/* 16d0c: vadd.f32 s13, s13, s22 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_22,0); }
/* 16d10: vcvt.f32.s32 s1, s1 */ { s_1=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_1), 0, 0, 32, 0, 0)); }
/* 16d14: vstr s13, [r4, #0x28] */ { { uint32_t fp_addr = (r_4 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16d18: vcvt.s32.f32 s2, s13 */ { s_2=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 0, 3, 32, 0)); }
/* 16d1c: vldr s14, [r5, #0x2c] */ { { uint32_t fp_addr = (r_5 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16d20: vsub.f32 s12, s12, s1 */ { s_12=m8_filter_op(cpu,M8_FP_SUB,s_12,s_1,0); }
/* 16d24: vstr s7, [r4, #0x10] */ { { uint32_t fp_addr = (r_4 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16d28: vadd.f32 s14, s14, s22 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_22,0); }
/* 16d2c: vstr s5, [r4, #0xc] */ { { uint32_t fp_addr = (r_4 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_5)); } }
/* 16d30: vstr s11, [r4] */ { { uint32_t fp_addr = (r_4 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 16d34: vcvt.f32.s32 s5, s2 */ { s_5=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_2), 0, 0, 32, 0, 0)); }
/* 16d38: vstr s3, [r4, #4] */ { { uint32_t fp_addr = (r_4 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_3)); } }
/* 16d3c: vcvt.s32.f32 s7, s14 */ { s_7=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 16d40: vstr s4, [r4, #8] */ { { uint32_t fp_addr = (r_4 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_4)); } }
/* 16d44: vsub.f32 s13, s13, s5 */ { s_13=m8_filter_op(cpu,M8_FP_SUB,s_13,s_5,0); }
/* 16d48: vstr s9, [r4, #0x14] */ { { uint32_t fp_addr = (r_4 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16d4c: vstr s0, [r4, #0x18] */ { { uint32_t fp_addr = (r_4 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_0)); } }
/* 16d50: vcvt.f32.s32 s11, s7 */ { s_11=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_7), 0, 0, 32, 0, 0)); }
/* 16d54: vstr s12, [r4, #0x24] */ { { uint32_t fp_addr = (r_4 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16d58: vstr s13, [r4, #0x28] */ { { uint32_t fp_addr = (r_4 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16d5c: vstr s16, [r4, #0x1c] */ { { uint32_t fp_addr = (r_4 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); } }
/* 16d60: vsub.f32 s14, s14, s11 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_11,0); }
/* 16d64: vstr s20, [r4, #0x20] */ { { uint32_t fp_addr = (r_4 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_20)); } }
/* 16d68: vstr s14, [r4, #0x2c] */ { { uint32_t fp_addr = (r_4 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16d6c: vstr s10, [r3] */ { { uint32_t fp_addr = (r_3 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 16d70: vldr s12, [r4, #4] */ { { uint32_t fp_addr = (r_4 + 4u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); } }
/* 16d74: vadd.f32 s12, s12, s12 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_12,0); }
/* 16d78: vstr s12, [r3, #4] */ { { uint32_t fp_addr = (r_3 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16d7c: vsub.f32 s12, s12, s17 */ { s_12=m8_filter_op(cpu,M8_FP_SUB,s_12,s_17,0); }
/* 16d80: vldr s9, [r4, #8] */ { { uint32_t fp_addr = (r_4 + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); } }
/* 16d84: vadd.f32 s9, s9, s9 */ { s_9=m8_filter_op(cpu,M8_FP_ADD,s_9,s_9,0); }
/* 16d88: vstr s9, [r3, #8] */ { { uint32_t fp_addr = (r_3 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16d8c: vsub.f32 s9, s9, s17 */ { s_9=m8_filter_op(cpu,M8_FP_SUB,s_9,s_17,0); }
/* 16d90: vldr s7, [r4, #0xc] */ { { uint32_t fp_addr = (r_4 + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); } }
/* 16d94: vadd.f32 s7, s7, s7 */ { s_7=m8_filter_op(cpu,M8_FP_ADD,s_7,s_7,0); }
/* 16d98: vstr s7, [r3, #0xc] */ { { uint32_t fp_addr = (r_3 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16d9c: vsub.f32 s7, s7, s17 */ { s_7=m8_filter_op(cpu,M8_FP_SUB,s_7,s_17,0); }
/* 16da0: vldr s10, [r4, #0x10] */ { { uint32_t fp_addr = (r_4 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); } }
/* 16da4: vadd.f32 s10, s10, s10 */ { s_10=m8_filter_op(cpu,M8_FP_ADD,s_10,s_10,0); }
/* 16da8: vstr s10, [r3, #0x10] */ { { uint32_t fp_addr = (r_3 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 16dac: vsub.f32 s10, s10, s17 */ { s_10=m8_filter_op(cpu,M8_FP_SUB,s_10,s_17,0); }
/* 16db0: vldr s11, [r4, #0x14] */ { { uint32_t fp_addr = (r_4 + 20u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 16db4: vadd.f32 s11, s11, s11 */ { s_11=m8_filter_op(cpu,M8_FP_ADD,s_11,s_11,0); }
/* 16db8: vstr s11, [r3, #0x14] */ { { uint32_t fp_addr = (r_3 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 16dbc: vsub.f32 s11, s11, s17 */ { s_11=m8_filter_op(cpu,M8_FP_SUB,s_11,s_17,0); }
/* 16dc0: vldr s13, [r4, #0x18] */ { { uint32_t fp_addr = (r_4 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16dc4: vadd.f32 s13, s13, s13 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_13,0); }
/* 16dc8: vstr s13, [r3, #0x18] */ { { uint32_t fp_addr = (r_3 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16dcc: vsub.f32 s2, s13, s17 */ { s_2=m8_filter_op(cpu,M8_FP_SUB,s_13,s_17,0); }
/* 16dd0: vldr s14, [r4, #0x1c] */ { { uint32_t fp_addr = (r_4 + 28u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16dd4: vadd.f32 s14, s14, s14 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_14,0); }
/* 16dd8: vstr s14, [r3, #0x1c] */ { { uint32_t fp_addr = (r_3 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16ddc: vsub.f32 s3, s14, s17 */ { s_3=m8_filter_op(cpu,M8_FP_SUB,s_14,s_17,0); }
/* 16de0: vldr s13, [r4, #0x20] */ { { uint32_t fp_addr = (r_4 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16de4: vadd.f32 s13, s13, s13 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_13,0); }
/* 16de8: vstr s13, [r3, #0x20] */ { { uint32_t fp_addr = (r_3 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16dec: vsub.f32 s4, s13, s17 */ { s_4=m8_filter_op(cpu,M8_FP_SUB,s_13,s_17,0); }
/* 16df0: vldr s14, [r4, #0x24] */ { { uint32_t fp_addr = (r_4 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 16df4: vadd.f32 s14, s14, s14 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_14,0); }
/* 16df8: vstr s14, [r3, #0x24] */ { { uint32_t fp_addr = (r_3 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16dfc: vsub.f32 s14, s14, s17 */ { s_14=m8_filter_op(cpu,M8_FP_SUB,s_14,s_17,0); }
/* 16e00: vldr s13, [r4, #0x28] */ { { uint32_t fp_addr = (r_4 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 16e04: vadd.f32 s13, s13, s13 */ { s_13=m8_filter_op(cpu,M8_FP_ADD,s_13,s_13,0); }
/* 16e08: vstr s13, [r3, #0x28] */ { { uint32_t fp_addr = (r_3 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 16e0c: vldr s5, [r4, #0x2c] */ { { uint32_t fp_addr = (r_4 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_5=m8_fp_float(fp_value); } }
/* 16e10: vstr s15, [r3] */ { { uint32_t fp_addr = (r_3 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 16e14: vfma.f32 s8, s5, s6 */ { s_8=m8_filter_op(cpu,M8_FP_FMA,s_5,s_6,s_8); }
/* 16e18: vstr s12, [r3, #4] */ { { uint32_t fp_addr = (r_3 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 16e1c: vsub.f32 s6, s13, s17 */ { s_6=m8_filter_op(cpu,M8_FP_SUB,s_13,s_17,0); }
/* 16e20: vstr s9, [r3, #8] */ { { uint32_t fp_addr = (r_3 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 16e24: vstr s7, [r3, #0xc] */ { { uint32_t fp_addr = (r_3 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 16e28: vstr s10, [r3, #0x10] */ { { uint32_t fp_addr = (r_3 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 16e2c: vstr s11, [r3, #0x14] */ { { uint32_t fp_addr = (r_3 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 16e30: vstr s2, [r3, #0x18] */ { { uint32_t fp_addr = (r_3 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_2)); } }
/* 16e34: vstr s3, [r3, #0x1c] */ { { uint32_t fp_addr = (r_3 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_3)); } }
/* 16e38: vmov.f32 s1, s8 */ { s_1=m8_fp_float(m8_fp_bits32(s_8)); }
/* 16e3c: vstr s4, [r3, #0x20] */ { { uint32_t fp_addr = (r_3 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_4)); } }
/* 16e40: vstr s14, [r3, #0x24] */ { { uint32_t fp_addr = (r_3 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 16e44: vstr s6, [r3, #0x28] */ { { uint32_t fp_addr = (r_3 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_6)); } }
/* 16e48: vstr s8, [r3, #0x2c] */ { { uint32_t fp_addr = (r_3 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
{
float wave[12]={s_15,s_12,s_9,s_7,s_10,s_11,s_2,s_3,s_4,s_14,s_6,s_1};
for(unsigned lane=0;lane<12;lane++) {
s_8=m8_filter_read(M8_HV_HOST(r_7+lane*4u));
s_13=m8_filter_read(M8_HV_HOST(r_4+lane*4u));
m8_fp_cmp32(cpu,m8_fp_bits32(s_13),m8_fp_bits32(s_8),1);
cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
if(cpu->xpsr&0x80000000u) {
s_5=m8_filter_op(cpu,M8_FP_DIV,s_13,s_8,0);
s_13=m8_filter_op(cpu,M8_FP_SUB,s_5,s_17,0);
wave[lane]=m8_filter_op(cpu,M8_FP_FMA,s_13,s_13,wave[lane]);
m8_filter_write(M8_HV_HOST(r_3+lane*4u),wave[lane]);
} else {
s_5=m8_filter_op(cpu,M8_FP_SUB,s_17,s_8,0);
m8_fp_cmp32(cpu,m8_fp_bits32(s_13),m8_fp_bits32(s_5),1);
cpu->xpsr=(cpu->xpsr&0x0fffffffu)|(cpu->fpscr&0xf0000000u);
if(!(cpu->xpsr&0x40000000u) && (((cpu->xpsr>>31)^(cpu->xpsr>>28))&1u)==0) {
s_13=m8_filter_op(cpu,M8_FP_SUB,s_13,s_17,0);
s_5=m8_filter_op(cpu,M8_FP_DIV,s_13,s_8,0);
s_13=m8_filter_op(cpu,M8_FP_ADD,s_5,s_17,0);
wave[lane]=m8_filter_op(cpu,M8_FP_FMA,-s_13,s_13,wave[lane]);
m8_filter_write(M8_HV_HOST(r_3+lane*4u),wave[lane]);
}
}
} s_15=wave[0];s_12=wave[1];s_9=wave[2];s_7=wave[3];
s_10=wave[4];s_11=wave[5];s_2=wave[6];s_3=wave[7];
s_4=wave[8];s_14=wave[9];s_6=wave[10];s_1=wave[11];
}
/* 170d4: vldr s5, [r6, #0x34] */ { { uint32_t fp_addr = (r_6 + 52u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_5=m8_fp_float(fp_value); } }
/* 170d8: ldrb.w r2, [r8, #0xe0] */ { { uint32_t base = r_8; uint32_t addr = base + (224u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 170dc: vsub.f32 s8, s17, s5 */ { s_8=m8_filter_op(cpu,M8_FP_SUB,s_17,s_5,0); }
/* 170e0: vmul.f32 s2, s5, s2 */ { s_2=m8_filter_op(cpu,M8_FP_MUL,s_5,s_2,0); }
/* 170e4: vmul.f32 s3, s5, s3 */ { s_3=m8_filter_op(cpu,M8_FP_MUL,s_5,s_3,0); }
/* 170e8: vmul.f32 s4, s5, s4 */ { s_4=m8_filter_op(cpu,M8_FP_MUL,s_5,s_4,0); }
/* 170ec: vmul.f32 s14, s5, s14 */ { s_14=m8_filter_op(cpu,M8_FP_MUL,s_5,s_14,0); }
/* 170f0: vstr s2, [r3, #0x18] */ { { uint32_t fp_addr = (r_3 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_2)); } }
/* 170f4: vmul.f32 s6, s5, s6 */ { s_6=m8_filter_op(cpu,M8_FP_MUL,s_5,s_6,0); }
/* 170f8: vstr s3, [r3, #0x1c] */ { { uint32_t fp_addr = (r_3 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_3)); } }
/* 170fc: vmul.f32 s15, s8, s15 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_8,s_15,0); }
/* 17100: vstr s4, [r3, #0x20] */ { { uint32_t fp_addr = (r_3 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_4)); } }
/* 17104: vmul.f32 s12, s8, s12 */ { s_12=m8_filter_op(cpu,M8_FP_MUL,s_8,s_12,0); }
/* 17108: vstr s14, [r3, #0x24] */ { { uint32_t fp_addr = (r_3 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 1710c: vmul.f32 s9, s9, s8 */ { s_9=m8_filter_op(cpu,M8_FP_MUL,s_9,s_8,0); }
/* 17110: vstr s6, [r3, #0x28] */ { { uint32_t fp_addr = (r_3 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_6)); } }
/* 17114: vmul.f32 s7, s8, s7 */ { s_7=m8_filter_op(cpu,M8_FP_MUL,s_8,s_7,0); }
/* 17118: vstr s15, [r3] */ { { uint32_t fp_addr = (r_3 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 1711c: vmul.f32 s10, s10, s8 */ { s_10=m8_filter_op(cpu,M8_FP_MUL,s_10,s_8,0); }
/* 17120: vstr s12, [r3, #4] */ { { uint32_t fp_addr = (r_3 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 17124: vmul.f32 s11, s11, s8 */ { s_11=m8_filter_op(cpu,M8_FP_MUL,s_11,s_8,0); }
/* 17128: vstr s9, [r3, #8] */ { { uint32_t fp_addr = (r_3 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_9)); } }
/* 1712c: vmul.f32 s13, s5, s1 */ { s_13=m8_filter_op(cpu,M8_FP_MUL,s_5,s_1,0); }
/* 17130: vstr s7, [r3, #0xc] */ { { uint32_t fp_addr = (r_3 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 17134: vstr s10, [r3, #0x10] */ { { uint32_t fp_addr = (r_3 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_10)); } }
/* 17138: vstr s11, [r3, #0x14] */ { { uint32_t fp_addr = (r_3 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 1713c: vstr s13, [r3, #0x2c] */ { { uint32_t fp_addr = (r_3 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 17140: cmp r2, #0 */ { { uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 17142: bne.w #0x175da */ { if (m8_condition(cpu->xpsr,2)) { goto rejected_unreachable; } }
/* 17146: ldrb.w r2, [r8, #0x55] */ { { uint32_t base = r_8; uint32_t addr = base + (85u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 1714a: vldr s10, [r6, #0x3c] */ { { uint32_t fp_addr = (r_6 + 60u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); } }
/* 1714e: lsls r7, r2, #0x1f */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1fu, &carry); m8_logic_flags(cpu, result, carry); r_7 = (uint32_t)(result); } }
/* 17150: vsub.f32 s9, s17, s10 */ { s_9=m8_filter_op(cpu,M8_FP_SUB,s_17,s_10,0); }
/* 17154: bmi #0x1715e */ { if (m8_condition(cpu->xpsr,5)) { goto L_1715e; } }
/* 17156: vldr s15, [pc, #0x314] */ { { uint32_t fp_addr = (0x17158u + 788u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 1715a: vmov.f32 s12, s15 */ { s_12=m8_fp_float(m8_fp_bits32(s_15)); }
/* 1715e: lsls r5, r2, #0x1e */ L_1715e: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1eu, &carry); m8_logic_flags(cpu, result, carry); r_5 = (uint32_t)(result); } }
/* 17160: vstr s15, [r3] */ { { uint32_t fp_addr = (r_3 + 0u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 17164: vstr s12, [r3, #4] */ { { uint32_t fp_addr = (r_3 + 4u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_12)); } }
/* 17168: bpl.w #0x1737e */ { if (m8_condition(cpu->xpsr,6)) { goto L_1737e; } }
/* 1716c: vldr s8, [r3, #8] */ { { uint32_t fp_addr = (r_3 + 8u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 17170: vadd.f32 s12, s12, s7 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_7,0); }
/* 17174: vadd.f32 s15, s15, s8 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_8,0); }
/* 17178: lsls r4, r2, #0x1d */ L_17178: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1du, &carry); m8_logic_flags(cpu, result, carry); r_4 = (uint32_t)(result); } }
/* 1717a: vstr s8, [r3, #8] */ { { uint32_t fp_addr = (r_3 + 8u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 1717e: vstr s7, [r3, #0xc] */ { { uint32_t fp_addr = (r_3 + 12u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_7)); } }
/* 17182: bpl.w #0x17374 */ { if (m8_condition(cpu->xpsr,6)) { goto L_17374; } }
/* 17186: vldr s8, [r3, #0x10] */ { { uint32_t fp_addr = (r_3 + 16u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); } }
/* 1718a: vadd.f32 s12, s12, s11 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_11,0); }
/* 1718e: vadd.f32 s15, s15, s8 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_8,0); }
/* 17192: lsls r0, r2, #0x1c */ L_17192: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1cu, &carry); m8_logic_flags(cpu, result, carry); r_0 = (uint32_t)(result); } }
/* 17194: vstr s8, [r3, #0x10] */ { { uint32_t fp_addr = (r_3 + 16u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_8)); } }
/* 17198: vstr s11, [r3, #0x14] */ { { uint32_t fp_addr = (r_3 + 20u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 1719c: bpl.w #0x1736a */ { if (m8_condition(cpu->xpsr,6)) { goto L_1736a; } }
/* 171a0: vldr s11, [r3, #0x18] */ { { uint32_t fp_addr = (r_3 + 24u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 171a4: vadd.f32 s12, s12, s3 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_3,0); }
/* 171a8: vadd.f32 s15, s15, s11 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_11,0); }
/* 171ac: lsls r1, r2, #0x1b */ L_171ac: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1bu, &carry); m8_logic_flags(cpu, result, carry); r_1 = (uint32_t)(result); } }
/* 171ae: vstr s11, [r3, #0x18] */ { { uint32_t fp_addr = (r_3 + 24u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 171b2: vstr s3, [r3, #0x1c] */ { { uint32_t fp_addr = (r_3 + 28u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_3)); } }
/* 171b6: bpl.w #0x17360 */ { if (m8_condition(cpu->xpsr,6)) { goto L_17360; } }
/* 171ba: vldr s11, [r3, #0x20] */ { { uint32_t fp_addr = (r_3 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); } }
/* 171be: vadd.f32 s12, s12, s14 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_14,0); }
/* 171c2: vadd.f32 s15, s15, s11 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_11,0); }
/* 171c6: lsls r2, r2, #0x1a */ L_171c6: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x1au, &carry); m8_logic_flags(cpu, result, carry); r_2 = (uint32_t)(result); } }
/* 171c8: vstr s11, [r3, #0x20] */ { { uint32_t fp_addr = (r_3 + 32u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_11)); } }
/* 171cc: vstr s14, [r3, #0x24] */ { { uint32_t fp_addr = (r_3 + 36u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 171d0: bpl.w #0x17356 */ { if (m8_condition(cpu->xpsr,6)) { goto L_17356; } }
/* 171d4: vldr s14, [r3, #0x28] */ { { uint32_t fp_addr = (r_3 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 171d8: vadd.f32 s12, s12, s13 */ { s_12=m8_filter_op(cpu,M8_FP_ADD,s_12,s_13,0); }
/* 171dc: vadd.f32 s15, s15, s14 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_15,s_14,0); }
/* 171e0: vstr s14, [r3, #0x28] */ L_171e0: { { uint32_t fp_addr = (r_3 + 40u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 171e4: vmul.f32 s11, s10, s15 */ { s_11=m8_filter_op(cpu,M8_FP_MUL,s_10,s_15,0); }
/* 171e8: vstr s13, [r3, #0x2c] */ { { uint32_t fp_addr = (r_3 + 44u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_13)); } }
/* 171ec: vmul.f32 s16, s9, s15 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_9,s_15,0); }
/* 171f0: vldr s14, [r8, #0x9c] */ { { uint32_t fp_addr = (r_8 + 156u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 171f4: vldr s13, [r8, #0xa0] */ { { uint32_t fp_addr = (r_8 + 160u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 171f8: vfma.f32 s11, s9, s12 */ { s_11=m8_filter_op(cpu,M8_FP_FMA,s_9,s_12,s_11); }
/* 171fc: vfma.f32 s16, s10, s12 */ { s_16=m8_filter_op(cpu,M8_FP_FMA,s_10,s_12,s_16); }
/* 17200: vldr s20, [r8, #0xa4] */ { { uint32_t fp_addr = (r_8 + 164u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_20=m8_fp_float(fp_value); } }
/* 17204: vadd.f32 s15, s14, s13 */ { s_15=m8_filter_op(cpu,M8_FP_ADD,s_14,s_13,0); }
/* 17208: ldrb.w r2, [r8, #0x41] */ { { uint32_t base = r_8; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 1720c: vmov.f32 s13, #-1.000000e+00 */ { s_13=m8_fp_float(0xbf800000u); }
/* 17210: vadd.f32 s14, s15, s22 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_15,s_22,0); }
/* 17214: vmul.f32 s16, s20, s16 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_20,s_16,0); }
/* 17218: vcvt.s32.f32 s14, s14 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_14), 0, 0, 3, 32, 0)); }
/* 1721c: vmul.f32 s20, s20, s11 */ { s_20=m8_filter_op(cpu,M8_FP_MUL,s_20,s_11,0); }
/* 17220: vcvt.f32.s32 s14, s14 */ { s_14=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0)); }
/* 17224: vsub.f32 s15, s15, s14 */ { s_15=m8_filter_op(cpu,M8_FP_SUB,s_15,s_14,0); }
/* 17228: vcmpe.f32 s15, #0 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_15), 0, 1); }
/* 1722c: vstr s15, [r8, #0x9c] */ { { uint32_t fp_addr = (r_8 + 156u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); } }
/* 17230: vldr s14, [r6, #0x40] */ { { uint32_t fp_addr = (r_6 + 64u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 17234: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 17238: vselgt.f32 s15, s17, s13 */ { s_15=m8_fp_float((!(cpu->xpsr & 0x40000000u) && ((((cpu->xpsr >> 31) ^ (cpu->xpsr >> 28)) & 1u) == 0)) ? m8_fp_bits32(s_17) : m8_fp_bits32(s_13)); }
/* 1723c: cmp r2, #3 */ { { uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1723e: vfma.f32 s16, s14, s15 */ { s_16=m8_filter_op(cpu,M8_FP_FMA,s_14,s_15,s_16); }
/* 17242: vfma.f32 s20, s14, s15 */ { s_20=m8_filter_op(cpu,M8_FP_FMA,s_14,s_15,s_20); }
/* 17246: bhi #0x17280 */ { if (m8_condition(cpu->xpsr,9)) { goto L_17280; } }
/* 17248: ldr.w r0, [r8, #0x50] */ { { uint32_t base = r_8; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 1724c: vmov.f32 s0, s16 */ { s_0=m8_fp_float(m8_fp_bits32(s_16)); }
/* 17250: vldr s1, [r6, #0x4c] */ { { uint32_t fp_addr = (r_6 + 76u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); } }
/* 17254: ldr r3, [r0] */ { { uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 17256: ldr r3, [r3] */ { { uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_3 = (uint32_t)(value); } } r_14=0x1725bu; s_14=m8_fp_float(0xbf800000u);s_15=1.0f; s_0=m8_filter_op(cpu,M8_FP_MUL,s_0,s_1,0); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0)); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));
/* 1725a: ldr.w r0, [r8, #0x50] */ { { uint32_t base = r_8; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_0 = (uint32_t)(value); } }
/* 1725e: ldr.w r2, [r8, #0x58] */ { { uint32_t base = r_8; uint32_t addr = base + (88u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 17262: vmov.f32 s16, s0 */ { s_16=m8_fp_float(m8_fp_bits32(s_0)); }
/* 17266: ldr r3, [r0] */ { { uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_3 = (uint32_t)(value); } }
/* 17268: vmov.f32 s0, s20 */ { s_0=m8_fp_float(m8_fp_bits32(s_20)); }
/* 1726c: vldr s1, [r2, #0x4c] */ { { uint32_t fp_addr = (r_2 + 76u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); } }
/* 17270: ldr r3, [r3] */ { { uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_3 = (uint32_t)(value); } } r_14=0x17275u; s_14=m8_fp_float(0xbf800000u);s_15=1.0f; s_0=m8_filter_op(cpu,M8_FP_MUL,s_0,s_1,0); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0)); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));
/* 17274: vmov.f32 s20, s0 */ { s_20=m8_fp_float(m8_fp_bits32(s_0)); }
/* 17278: ldr.w r6, [r8, #0x58] */ { { uint32_t base = r_8; uint32_t addr = base + (88u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_6 = (uint32_t)(value); } }
/* 1727c: ldrb.w r2, [r8, #0x41] */ { { uint32_t base = r_8; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_2 = (uint32_t)(value); } }
/* 17280: ldrb.w r3, [r8, #0x40] */ L_17280: { { uint32_t base = r_8; uint32_t addr = base + (64u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 17284: subs r1, r3, #1 */ { { uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_1 = (uint32_t)(result); } }
/* 17286: cmp r1, #4 */ { { uint32_t a = r_1; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 17288: bls.w #0x1749c */ { if (m8_condition(cpu->xpsr,10)) { goto rejected_unreachable; } }
/* 1728c: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1728e: bne.w #0x1760c */ { if (m8_condition(cpu->xpsr,2)) { goto rejected_unreachable; } }
/* 17292: cmp r2, #3 */ { { uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 17294: bls #0x172ca */ { if (m8_condition(cpu->xpsr,10)) { goto L_172ca; } }
/* 172ca: vldr s13, [r8, #0x44] */ L_172ca: { { uint32_t fp_addr = (r_8 + 68u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); } }
/* 172ce: vldr s14, [pc, #0x1a0] */ { { uint32_t fp_addr = (0x172d0u + 416u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 172d2: vldr s15, [pc, #0x1a0] */ { { uint32_t fp_addr = (0x172d4u + 416u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 172d6: vcmpe.f32 s13, s14 */ { m8_fp_cmp32(cpu, m8_fp_bits32(s_13), m8_fp_bits32(s_14), 1); }
/* 172da: vmul.f32 s15, s16, s15 */ { s_15=m8_filter_op(cpu,M8_FP_MUL,s_16,s_15,0); }
/* 172de: vmrs apsr_nzcv, fpscr */ { cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u); }
/* 172e2: vcvt.s32.f32 s15, s15 */ { s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_15), 0, 0, 3, 32, 0)); }
/* 172e6: bpl #0x1734e */ { if (m8_condition(cpu->xpsr,6)) { goto L_1734e; } }
/* 172e8: vcvt.u32.f32 s14, s13 */ { s_14=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_13), 0, 1, 3, 32, 0)); }
/* 172ec: vmov r3, s14 */ { r_3 = m8_fp_bits32(s_14); }
/* 172f0: uxth r3, r3 */ { { uint32_t value = r_3; r_3 = (uint32_t)(value & 65535u); } }
/* 172f2: lsls r3, r3, #1 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x1u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 172f4: vldr s14, [r8, #0x48] */ L_172f4: { { uint32_t fp_addr = (r_8 + 72u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); } }
/* 172f8: vmov r1, s15 */ { r_1 = m8_fp_bits32(s_15); }
/* 172fc: ldr.w r2, [r8, #0xe8] */ { { uint32_t base = r_8; uint32_t addr = base + (232u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),4); r_2 = (uint32_t)(value); } }
/* 17300: vmul.f32 s16, s16, s25 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_25,0); }
/* 17304: vadd.f32 s14, s14, s13 */ { s_14=m8_filter_op(cpu,M8_FP_ADD,s_14,s_13,0); }
/* 17308: vmul.f32 s20, s24, s20 */ { s_20=m8_filter_op(cpu,M8_FP_MUL,s_24,s_20,0); }
/* 1730c: strh r1, [r2, r3] */ { { uint32_t base = r_2; uint32_t addr = base + ((0u + r_3)); uint32_t value = r_1; m8_le_write(M8_HV_HOST(addr),2,value); } }
/* 1730e: vstr s14, [r8, #0x44] */ { { uint32_t fp_addr = (r_8 + 68u); uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); } }
/* 17312: vldr s15, [r6, #0x2c] */ { { uint32_t fp_addr = (r_6 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); } }
/* 17316: vmul.f32 s16, s16, s15 */ { s_16=m8_filter_op(cpu,M8_FP_MUL,s_16,s_15,0); }
/* 1731a: vmul.f32 s20, s20, s15 */ { s_20=m8_filter_op(cpu,M8_FP_MUL,s_20,s_15,0); }
/* 1731e: vstmia sb!, {s16} */ { { uint32_t fp_addr = r_9; uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); r_9 += 4u; } }
/* 17322: vstmia fp!, {s20} */ { { uint32_t fp_addr = r_11; uint32_t fp_value; m8_le_write(M8_HV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_20)); r_11 += 4u; } }
/* 17326: cmp fp, sl */ { { uint32_t a = r_11; uint32_t b = r_10; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 17328: beq.w #0x17730 */ { if (m8_condition(cpu->xpsr,1)) { goto L_17730; } }
/* 1732c: ldrb.w r3, [r8, #0x54] */ L_1732c: { { uint32_t base = r_8; uint32_t addr = base + (84u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 17330: cmp r3, #0 */ { { uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 17332: beq.w #0x175ce */ { if (m8_condition(cpu->xpsr,1)) { goto L_175ce; } }
/* 17336: vmov s15, r3 */ L_17336: { s_15=m8_fp_float(r_3); }
/* 1733a: cmp r3, #6 */ { { uint32_t a = r_3; uint32_t b = 0x6u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); } }
/* 1733c: vcvt.f32.u32 s15, s15, #3 */ { if (cpu->fpscr & (3u << 22)) m8_fp_sync(cpu); s_15=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_15), 0, 1, 32, 3, 1)); }
/* 17340: vsub.f32 s13, s17, s15 */ { s_13=m8_filter_op(cpu,M8_FP_SUB,s_17,s_15,0); }
/* 17344: bls.w #0x169a8 */ { if (m8_condition(cpu->xpsr,10)) { goto L_169a8; } }
/* 17348: movs r3, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 1734a: b.w #0x169ac */ { goto L_169ac; }
/* 1734e: vmov.f32 s13, s23 */ L_1734e: { s_13=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17352: movs r3, #0 */ { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); } }
/* 17354: b #0x172f4 */ { goto L_172f4; }
/* 17356: vmov.f32 s13, s23 */ L_17356: { s_13=m8_fp_float(m8_fp_bits32(s_23)); }
/* 1735a: vmov.f32 s14, s23 */ { s_14=m8_fp_float(m8_fp_bits32(s_23)); }
/* 1735e: b #0x171e0 */ { goto L_171e0; }
/* 17360: vmov.f32 s14, s23 */ L_17360: { s_14=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17364: vmov.f32 s11, s23 */ { s_11=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17368: b #0x171c6 */ { goto L_171c6; }
/* 1736a: vmov.f32 s3, s23 */ L_1736a: { s_3=m8_fp_float(m8_fp_bits32(s_23)); }
/* 1736e: vmov.f32 s11, s23 */ { s_11=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17372: b #0x171ac */ { goto L_171ac; }
/* 17374: vmov.f32 s11, s23 */ L_17374: { s_11=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17378: vmov.f32 s8, s23 */ { s_8=m8_fp_float(m8_fp_bits32(s_23)); }
/* 1737c: b #0x17192 */ { goto L_17192; }
/* 1737e: vmov.f32 s7, s23 */ L_1737e: { s_7=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17382: vmov.f32 s8, s23 */ { s_8=m8_fp_float(m8_fp_bits32(s_23)); }
/* 17386: b #0x17178 */ { goto L_17178; } goto rejected_unreachable;
/* 17418: vadd.f32 s24, s14, s14 */ L_17418: { s_24=m8_filter_op(cpu,M8_FP_ADD,s_14,s_14,0); }
/* 1741c: vmov.f32 s25, #1.000000e+00 */ { s_25=m8_fp_float(0x3f800000u); }
/* 17420: b.w #0x16a88 */ { goto L_16a88; }

L_175ce: r_0=r_8; r_14=0x175d5u; r_15=0x15da4u;
M8_HV_SAVE(cpu);
{int status=update(cpu,mem);
 if(status!=M8_AOT_EXIT || state.r[15]!=0x175d4u) {*output=*cpu;return status;}
 M8_HV_LOAD(cpu);
 if(r_8!=root || r_13!=frame || ((r_10-r_11)&3u) || !m8_hyper_voice_qualify(cpu,mem,r_8,r_9,r_11,(r_10-r_11)/4u,0)) {
  r_15=0x175d4u;goto done;
 }
}
/* 175d4: ldrb.w r3, [r8, #0x54] */ { { uint32_t base = r_8; uint32_t addr = base + (84u); uint32_t value; value=m8_le_read(M8_HV_HOST(addr),1); r_3 = (uint32_t)(value); } }
/* 175d8: b #0x17336 */ { goto L_17336; }
/* 17730: movs r0, #1 */ L_17730: { { uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_0 = (uint32_t)(result); } }
/* 17732: vpop {d8, d9, d10, d11, d12} */ L_17732: { { uint32_t fp_addr = r_13; uint32_t fp_value; fp_value=m8_le_read(M8_HV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 4u),4); s_17=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 8u),4); s_18=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 12u),4); s_19=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 16u),4); s_20=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 20u),4); s_21=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 24u),4); s_22=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 28u),4); s_23=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 32u),4); s_24=m8_fp_float(fp_value); fp_value=m8_le_read(M8_HV_HOST(fp_addr + 36u),4); s_25=m8_fp_float(fp_value); r_13 += 40u; } }
/* 17736: pop.w {r3, r4, r5, r6, r7, r8, sb, sl, fp, pc} */ { { uint32_t base = r_13; uint32_t addr = base + 0u; uint32_t value0; value0=m8_le_read(M8_HV_HOST(addr + 0u),4); uint32_t value1; value1=m8_le_read(M8_HV_HOST(addr + 4u),4); uint32_t value2; value2=m8_le_read(M8_HV_HOST(addr + 8u),4); uint32_t value3; value3=m8_le_read(M8_HV_HOST(addr + 12u),4); uint32_t value4; value4=m8_le_read(M8_HV_HOST(addr + 16u),4); uint32_t value5; value5=m8_le_read(M8_HV_HOST(addr + 20u),4); uint32_t value6; value6=m8_le_read(M8_HV_HOST(addr + 24u),4); uint32_t value7; value7=m8_le_read(M8_HV_HOST(addr + 28u),4); uint32_t value8; value8=m8_le_read(M8_HV_HOST(addr + 32u),4); uint32_t value9; value9=m8_le_read(M8_HV_HOST(addr + 36u),4); r_13 = base + 40u; r_3 = (uint32_t)(value0); r_4 = (uint32_t)(value1); r_5 = (uint32_t)(value2); r_6 = (uint32_t)(value3); r_7 = (uint32_t)(value4); r_8 = (uint32_t)(value5); r_9 = (uint32_t)(value6); r_10 = (uint32_t)(value7); r_11 = (uint32_t)(value8); r_15 = (value9) & ~1u; goto done; } }

done: M8_HV_SAVE(output);return M8_AOT_EXIT;
rejected_unreachable: M8_HV_SAVE(output);return M8_AOT_FAULT;

#undef M8_HV_HOST
#undef M8_HV_SAVE
#undef M8_HV_LOAD
#undef M8_HV_SET_D9
#undef M8_HV_NOINLINE
}
#endif
#endif
