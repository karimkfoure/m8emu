/* Headless 6.5.2C mode-0 interval at 0x19b64. Render at most the
 * remaining phase quantum, then preserve the generator/caller boundary.
 * Qualify every span and alias before effects. The caller owns FP setup.
 * R14 is an input because a held sample does not invoke the clamp callback;
 * S4 is never read or written by this interval. */
#ifndef M8_AOT_BRAIDS_MODE0_H
#define M8_AOT_BRAIDS_MODE0_H
#include "braids_filter.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
M8_MEM_INLINE int32_t m8_braids_mode0_signed(uint32_t bits) {
    int32_t value;memcpy(&value,&bits,4);return value;
}
/* The preceding multiply produces a normal value, zero, infinity, or a
 * canonical quiet NaN under the qualified DN/FZ environment. Neither clamp
 * operand is zero. Preserve signed zero and the MAXNM quiet-NaN result while
 * avoiding repeated classification of an already repaired arithmetic value. */
M8_MEM_INLINE float m8_braids_mode0_clamp(float value) {
    if(M8_FILTER_UNLIKELY(m8_fp_bits32(value)==0x7fc00000u)) return -1.0f;
    if(value < -1.0f) return -1.0f;
    if(value > 1.0f) return 1.0f;
    return value;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_braids_mode0_kernel(m8_cpu *cpu,m8_mem *mem) {
    uint32_t object_address=cpu->r[5],phase=cpu->r[3],begin=cpu->r[6],end=cpu->r[9];
    if(cpu->r[15]!=0x19b64u || !m8_fp_native_supported(cpu) || phase>7u ||
       (cpu->xpsr&0x0600fc00u) || end<=begin || ((end-begin)&3u) ||
       object_address>UINT32_MAX-0x228u || cpu->r[10]!=object_address+0x20u ||
       cpu->r[7]!=object_address+0x1aeu+phase*2u || cpu->r[13]>UINT32_MAX-8u) return 0;
    uint32_t count=(end-begin)/4u;
    if(count>8u-phase) count=8u-phase;
    if(count<2u) return 0;
    /* Disjoint canonical guest windows make ordinary per-span ownership
     * sufficient. A custom overlapping descriptor keeps the scalar path. */
    if(mem->itcm.base || mem->itcm.size>0x80000u ||
       mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x80000u ||
       mem->ocram.base!=0x20200000u || mem->ocram.size>0x80000u ||
       mem->flash.base!=0x60000000u || mem->flash.size>0x800000u) return 0;
    uint32_t limit=cpu->s[18];
    if(limit<0x3f800000u || limit>0x47800000u) return 0;
    unsigned shift=23u-((limit>>23)-127u);
    uint32_t record_count=(((limit&0x7fffffu)|0x800000u)+((1u<<shift)-1u))>>shift;
    uint8_t *object=m8_pointer(mem,object_address,0x228u);
    if(!object || object[0x40]!=0u || object[0x41]>3u || object[0x1ac]!=phase) return 0;
    uint32_t parameter_address=m8_filter_read32(object+0x210u);
    uint32_t record_address=m8_filter_read32(object+0x224u);
    uint32_t callback_address=m8_filter_read32(object+0x50u);
    uint8_t *callback=m8_pointer(mem,callback_address,4u);
    if(!callback) return 0;
    uint32_t vtable_address=m8_filter_read32(callback);
    uint8_t *vtable=m8_pointer(mem,vtable_address,4u);
    if(!vtable || m8_filter_read32(vtable)!=0x1fa5u) return 0;
    uint8_t *parameters=m8_pointer(mem,parameter_address,0x7cu);
    uint8_t *sample=m8_pointer(mem,cpu->r[13]+4u,4u);
    uint8_t *left=m8_pointer(mem,begin,count*4u);
    uint8_t *right=m8_pointer(mem,cpu->r[8],count*4u);
    uint8_t *record=m8_pointer(mem,record_address,record_count*2u);
    if(!parameters || !sample || !left || !right || !record) return 0;
    /* Six mutable spans cannot overlap each other, the two callback words,
     * or the public state descriptors. No restrict pointer exists until this
     * proof succeeds. Read-only callback words may overlap each other. */
    /* In the ordinary two-owner layout, prove the complete host regions
     * disjoint once, then check only spans in the same owner. Other valid
     * canonical layouts retain the complete alias matrix below. */
    int grouped=(object_address&0xfff80000u)==0x20000000u &&
        ((cpu->r[13]+4u)&0xfff80000u)==0x20000000u &&
        (callback_address&0xfff80000u)==0x20000000u &&
        (vtable_address&0xfff80000u)==0x20000000u &&
        (parameter_address&0xfff80000u)==0x20200000u &&
        (begin&0xfff80000u)==0x20200000u &&
        (cpu->r[8]&0xfff80000u)==0x20200000u &&
        (record_address&0xfff80000u)==0x20200000u;
#define M8_MODE0_SEPARATE(a,an,b,bn) \
    if(!m8_filter_disjoint((uintptr_t)(a),(an),(uintptr_t)(b),(bn))) return 0
    if(grouped) {
        uint8_t *dtcm=mem->dtcm.host,*ocram=mem->ocram.host;
        M8_MODE0_SEPARATE(dtcm,mem->dtcm.size,ocram,mem->ocram.size);
        M8_MODE0_SEPARATE(dtcm,mem->dtcm.size,cpu,sizeof(*cpu));
        M8_MODE0_SEPARATE(dtcm,mem->dtcm.size,mem,sizeof(*mem));
        M8_MODE0_SEPARATE(ocram,mem->ocram.size,cpu,sizeof(*cpu));
        M8_MODE0_SEPARATE(ocram,mem->ocram.size,mem,sizeof(*mem));
        M8_MODE0_SEPARATE(object,0x228u,sample,4u);
        M8_MODE0_SEPARATE(object,0x228u,callback,4u);
        M8_MODE0_SEPARATE(object,0x228u,vtable,4u);
        M8_MODE0_SEPARATE(sample,4u,callback,4u);
        M8_MODE0_SEPARATE(sample,4u,vtable,4u);
        M8_MODE0_SEPARATE(parameters,0x7cu,left,count*4u);
        M8_MODE0_SEPARATE(parameters,0x7cu,right,count*4u);
        M8_MODE0_SEPARATE(parameters,0x7cu,record,record_count*2u);
        M8_MODE0_SEPARATE(left,count*4u,right,count*4u);
        M8_MODE0_SEPARATE(left,count*4u,record,record_count*2u);
        M8_MODE0_SEPARATE(right,count*4u,record,record_count*2u);
    } else {
        uintptr_t spans[10]={(uintptr_t)object,(uintptr_t)parameters,(uintptr_t)sample,
            (uintptr_t)left,(uintptr_t)right,(uintptr_t)record,(uintptr_t)callback,
            (uintptr_t)vtable,(uintptr_t)cpu,(uintptr_t)mem};
        uint32_t widths[10]={0x228u,0x7cu,4u,count*4u,count*4u,record_count*2u,
                            4u,4u,sizeof(*cpu),sizeof(*mem)};
        for(unsigned i=0;i<6;i++)
            for(unsigned j=i+1;j<10;j++)
                if(!m8_filter_disjoint(spans[i],widths[i],spans[j],widths[j])) return 0;
        for(unsigned i=6;i<8;i++)
            for(unsigned j=8;j<10;j++)
                if(!m8_filter_disjoint(spans[i],widths[i],spans[j],widths[j])) return 0;
    }
    M8_MODE0_SEPARATE(cpu,sizeof(*cpu),mem,sizeof(*mem));
#undef M8_MODE0_SEPARATE
    uint8_t *restrict voice=object,*restrict param=parameters,*restrict scratch=sample;
    uint8_t *restrict output_left=left,*restrict output_right=right,*restrict recorded=record;
    m8_cpu flags;flags.xpsr=cpu->xpsr;flags.fpscr=cpu->fpscr;
    /* Every modified lane is defined before its first read. Only s17..s21
     * carry input coefficients; S4 stays in the enclosing original state.
     * Integer casts below consume signed/unsigned 16-bit values. Every such
     * value is exact in binary32 and cannot produce an FP exception. */
    float s[32];for(unsigned i=17;i<=21;i++) s[i]=m8_fp_float(cpu->s[i]);
    uint32_t hold=m8_filter_read16(voice+0x1ecu),integer=0,index=0,link=cpu->r[14];
    for(uint32_t i=0;i<count;i++) {
        s[1]=(float)phase*0.125f;
        s[11]=m8_filter_op(&flags,M8_FP_SUB,s[17],s[1],0);
        uint32_t next=phase<7u ? phase+1u : 0u;
        voice[0x1ac]=(uint8_t)next;
        s[10]=0.5f;
        s[15]=m8_filter_op(&flags,M8_FP_MUL,s[1],m8_filter_read(param+0x28u),0);
        s[8]=m8_filter_read(param+0x78u);
        s[9]=m8_filter_read(param+0x24u);
        s[12]=m8_filter_op(&flags,M8_FP_MUL,s[1],m8_filter_read(param+4u),0);
        s[13]=m8_filter_op(&flags,M8_FP_MUL,s[1],m8_filter_read(param+0x1cu),0);
        s[14]=m8_filter_op(&flags,M8_FP_MUL,s[1],m8_filter_read(param+0x20u),0);
        s[5]=m8_filter_read(param+0x54u);s[6]=m8_filter_read(param+0x6cu);
        s[7]=m8_filter_read(param+0x70u);s[9]=m8_filter_read(param+0x74u);
        /* s1 consumes the earlier 0x24 load before s9 takes 0x74. */
        s[1]=m8_filter_op(&flags,M8_FP_MUL,(float)phase*0.125f,m8_filter_read(param+0x24u),0);
        s[15]=m8_filter_op(&flags,M8_FP_FMA,s[11],s[8],s[15]);
        s[12]=m8_filter_op(&flags,M8_FP_FMA,s[11],s[5],s[12]);
        s[13]=m8_filter_op(&flags,M8_FP_FMA,s[11],s[6],s[13]);
        m8_filter_write(param+0x50u,s[15]);
        s[14]=m8_filter_op(&flags,M8_FP_FMA,s[11],s[7],s[14]);
        s[1]=m8_filter_op(&flags,M8_FP_FMA,s[11],s[9],s[1]);
        uint32_t comparison=m8_braids_filter_compare(&flags,s[15],s[10],1);
        m8_filter_write(param+0x2cu,s[12]);m8_filter_write(param+0x44u,s[13]);
        m8_filter_write(param+0x48u,s[14]);m8_filter_write(param+0x4cu,s[1]);
        if(m8_condition(comparison,12)) {
            s[16]=m8_filter_op(&flags,M8_FP_ADD,s[15],s[15],0);s[22]=1.0f;
        } else {
            s[10]=m8_filter_op(&flags,M8_FP_SUB,s[10],s[15],0);
            s[22]=s[17];s[15]=2.0f;s[16]=1.0f;
            s[22]=m8_filter_op(&flags,M8_FP_FMA,s[10],s[15],s[22]);
        }
        if(!hold) {
            uint32_t signed_sample=m8_braids_filter_sx16(m8_filter_read16(voice+0x1aeu+phase*2u));
            s[0]=(float)m8_braids_mode0_signed(signed_sample);
            s[0]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[21],0);
            m8_filter_write(scratch,s[0]);
            s[14]=-1.0f;s[15]=1.0f;
            s[0]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[1],0);
            s[0]=m8_braids_mode0_clamp(s[0]);
            m8_filter_write(scratch,s[0]);m8_filter_write(voice+0x1f0u,s[0]);link=0x19a7bu;
        } else m8_filter_write32(scratch,m8_filter_read32(voice+0x1f0u));
        s[15]=m8_filter_read(param+0x14u);hold=(hold+1u)&0xffffu;
        s[14]=(float)hold;
        comparison=m8_braids_filter_compare(&flags,s[14],s[15],0);
        if(m8_condition(comparison,13)) hold=0;
        m8_filter_write16(voice+0x1ecu,hold);
        s[14]=m8_filter_read(param+0x18u);
        comparison=m8_braids_filter_compare(&flags,s[14],0.0f,0);
        if(!m8_condition(comparison,1)) {
            s[15]=m8_filter_op(&flags,M8_FP_MUL,s[14],m8_filter_read(scratch),0);
            uint32_t quantized=m8_filter_to_int(&flags,s[15],0);
            uint32_t signed_sample=m8_braids_filter_sx16(quantized);
            s[15]=(float)m8_braids_mode0_signed(signed_sample);
            s[13]=m8_filter_op(&flags,M8_FP_DIV,s[15],s[14],0);m8_filter_write(scratch,s[13]);
        }
        s[0]=m8_filter_read(scratch);
        s[13]=m8_filter_read(voice+0x44u);
        s[15]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[19],0);
        integer=m8_filter_to_int(&flags,s[15],0);
        comparison=m8_braids_filter_compare(&flags,s[13],s[18],1);
        flags.fpscr=(flags.fpscr&0x0fffffffu)|comparison;
        if(!(comparison&0x80000000u)) {s[13]=s[20];index=0;}
        else index=((uint16_t)m8_filter_to_int(&flags,s[13],1))*2u;
        s[14]=m8_filter_op(&flags,M8_FP_ADD,m8_filter_read(voice+0x48u),s[13],0);
        m8_filter_write16(recorded+index,integer);m8_filter_write(voice+0x44u,s[14]);
        s[15]=m8_filter_read(param+0x2cu);
        s[0]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[15],0);
        s[22]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[22],0);
        m8_filter_write(scratch,s[0]);s[15]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[16],0);
        m8_filter_write(output_left+i*4u,s[22]);m8_filter_write(output_right+i*4u,s[15]);
        phase=next;
    }
    uint32_t final=begin+count*4u;
    m8_sub_flags(&flags,final,end,0);
    cpu->r[0]=0;cpu->r[1]=integer;cpu->r[2]=record_address;
    cpu->r[3]=final==end ? index : 0u;cpu->r[4]=parameter_address;
    cpu->r[6]=final;cpu->r[7]+=count*2u;cpu->r[8]+=count*4u;cpu->r[14]=link;
    cpu->r[15]=final==end ? 0x19bd2u : 0x19b94u;
    cpu->xpsr=flags.xpsr;cpu->fpscr=flags.fpscr;
    cpu->s[0]=m8_fp_bits32(s[0]);cpu->s[1]=m8_fp_bits32(s[1]);
    for(unsigned i=5;i<=16;i++) cpu->s[i]=m8_fp_bits32(s[i]);
    cpu->s[22]=m8_fp_bits32(s[22]);
    return 1;
}
#endif
#endif
