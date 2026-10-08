/* Headless 6.5.2C mode-6 filter interval at 0x19b64. Process 2..8 samples
 * without a generator call, then resume at 0x19b94 or 0x19bd2. Qualify every
 * guest span before effects. The caller owns the native FP environment. */
#ifndef M8_AOT_BRAIDS_FILTER_H
#define M8_AOT_BRAIDS_FILTER_H
#include "filter_kernel.h"
#include "integer.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
static inline uint32_t m8_braids_filter_sx16(uint32_t bits) {
    uint16_t low=(uint16_t)bits;int16_t value;
    memcpy(&value,&low,sizeof(value));return (uint32_t)(int32_t)value;
}
M8_FILTER_COLD static uint32_t m8_braids_filter_compare_special(m8_cpu *flags,float a,float b,int signaling) {
    uint32_t aa=m8_filter_input32(flags,m8_fp_bits32(a)),bb=m8_filter_input32(flags,m8_fp_bits32(b));
    int an=(aa&0x7f800000u)==0x7f800000u && (aa&0x7fffffu);
    int bn=(bb&0x7f800000u)==0x7f800000u && (bb&0x7fffffu);
    uint32_t nzcv;
    if(an || bn) {
        nzcv=0x30000000u;
        if(signaling || (an && !(aa&0x400000u)) || (bn && !(bb&0x400000u))) flags->fpscr|=1u;
    } else {
        float x=m8_fp_float(aa),y=m8_fp_float(bb);
        nzcv=x==y ? 0x60000000u : x<y ? 0x80000000u : 0x20000000u;
    }
    return nzcv;
}
/* Earlier VMRS results feed only the adjacent branch. Their NZCV writes are
 * overwritten before either continuation. Keep those flags in a local word;
 * publish the final VFP comparison below. Sticky flags remain accumulated in
 * hardware until the caller leaves the native FP environment. */
M8_MEM_INLINE uint32_t m8_braids_filter_compare(m8_cpu *flags,float a,float b,int signaling) {
    uint32_t aa=m8_fp_bits32(a)&0x7fffffffu,bb=m8_fp_bits32(b)&0x7fffffffu;
    if(M8_FILTER_UNLIKELY((aa && aa<0x00800000u) || aa>0x7f800000u ||
                            (bb && bb<0x00800000u) || bb>0x7f800000u))
        return m8_braids_filter_compare_special(flags,a,b,signaling);
    return a==b ? 0x60000000u : a<b ? 0x80000000u : 0x20000000u;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_braids_filter_kernel(m8_cpu *cpu,m8_mem *mem) {
    uint32_t object_address=cpu->r[5],phase=cpu->r[3],begin=cpu->r[6],end=cpu->r[9];
    if(cpu->r[15]!=0x19b64u || !m8_fp_native_supported(cpu) || phase>7u ||
       end<=begin || ((end-begin)&3u) ||
       object_address>UINT32_MAX-0x228u || cpu->r[10]!=object_address+0x20u ||
       cpu->r[7]!=object_address+0x1aeu+phase*2u || cpu->r[13]>UINT32_MAX-8u) return 0;
    uint32_t count=(end-begin)/4u;
    if(count>8u-phase) count=8u-phase;
    if(count<2u) return 0;
    /* Fixed disjoint guest windows prove region priority once. Other region
     * layouts keep the original path; host aliases are checked below. */
    if(mem->itcm.base || mem->itcm.size>0x80000u ||
       mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x80000u ||
       mem->ocram.base!=0x20200000u || mem->ocram.size>0x80000u ||
       mem->flash.base!=0x60000000u || mem->flash.size>0x800000u) return 0;
    /* Positive finite cursor limit in [1,65536]. Its ceiling bounds every
     * masked 16-bit record index, including negative or unordered cursors. */
    uint32_t limit=cpu->s[18];
    if(limit<0x3f800000u || limit>0x47800000u) return 0;
    unsigned shift=23u-((limit>>23)-127u);
    uint32_t record_count=(((limit&0x7fffffu)|0x800000u)+((1u<<shift)-1u))>>shift;
    uint8_t *object=m8_pointer(mem,object_address,0x228u);
    if(!object || object[0x40]!=6u || object[0x41]>3u || object[0x1ac]!=phase) return 0;
    uint32_t parameter_address=m8_filter_read32(object+0x210u);
    uint32_t record_address=m8_filter_read32(object+0x224u);
    uint32_t callback_address=m8_filter_read32(object+0x50u);
    uint8_t *callback=m8_pointer(mem,callback_address,4u);
    if(!callback) return 0;
    uint8_t *vtable=m8_pointer(mem,m8_filter_read32(callback),4u);
    /* The original six-instruction callback at 0x1fa4 multiplies s0 by s1
     * and clamps to [-1,1]. It has no guest memory or core-register effects. */
    if(!vtable || m8_filter_read32(vtable)!=0x1fa5u) return 0;
    uint8_t *parameters=m8_pointer(mem,parameter_address,0x7cu);
    uint8_t *sample=m8_pointer(mem,cpu->r[13]+4u,4u);
    uint8_t *left=m8_pointer(mem,begin,count*4u);
    uint8_t *right=m8_pointer(mem,cpu->r[8],count*4u);
    uint8_t *record=m8_pointer(mem,record_address,record_count*2u);
    uint8_t *literal=m8_pointer(mem,0x19900u,28u);
    if(!parameters || !sample || !left || !right || !record || !literal) return 0;
#define M8_BRAIDS_FILTER_SEPARATE(a,an,b,bn) \
    if(!m8_filter_disjoint((uintptr_t)(a),(an),(uintptr_t)(b),(bn))) return 0
    M8_BRAIDS_FILTER_SEPARATE(parameters,0x7cu,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(sample,4u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(sample,4u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(left,count*4u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(left,count*4u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(left,count*4u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(right,count*4u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(right,count*4u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(right,count*4u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(right,count*4u,left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(record,record_count*2u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(record,record_count*2u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(record,record_count*2u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(record,record_count*2u,left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(record,record_count*2u,right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(literal,28u,record,record_count*2u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),record,record_count*2u);
    M8_BRAIDS_FILTER_SEPARATE(cpu,sizeof(*cpu),literal,28u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),record,record_count*2u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),literal,28u);
    M8_BRAIDS_FILTER_SEPARATE(mem,sizeof(*mem),cpu,sizeof(*cpu));
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,record,record_count*2u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,literal,28u);
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,cpu,sizeof(*cpu));
    M8_BRAIDS_FILTER_SEPARATE(callback,4u,mem,sizeof(*mem));
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,object,0x228u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,parameters,0x7cu);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,sample,4u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,left,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,right,count*4u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,record,record_count*2u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,literal,28u);
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,cpu,sizeof(*cpu));
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,mem,sizeof(*mem));
    M8_BRAIDS_FILTER_SEPARATE(vtable,4u,callback,4u);
#undef M8_BRAIDS_FILTER_SEPARATE
#if defined(__x86_64__)
    /* x86 DAZ cannot expose ARM IDC. Reject external subnormal operands
     * before effects; retain the native profile's documented FP limits. */
    uint32_t input_offsets[]={0x20u,0x24u,0x28u,0x44u,0x48u,0x1f0u};
    uint32_t parameter_offsets[]={4u,0x14u,0x18u,0x1cu,0x20u,0x24u,0x28u,
                                 0x54u,0x6cu,0x70u,0x74u,0x78u};
    for(unsigned i=17;i<=21;i++)
        if((cpu->s[i]&0x7fffffffu) && !(cpu->s[i]&0x7f800000u)) return 0;
    for(unsigned i=0;i<sizeof(input_offsets)/sizeof(input_offsets[0]);i++) {
        uint32_t bits=m8_filter_read32(object+input_offsets[i]);
        if((bits&0x7fffffffu) && !(bits&0x7f800000u)) return 0;
    }
    for(unsigned i=0;i<sizeof(parameter_offsets)/sizeof(parameter_offsets[0]);i++) {
        uint32_t bits=m8_filter_read32(parameters+parameter_offsets[i]);
        if((bits&0x7fffffffu) && !(bits&0x7f800000u)) return 0;
    }
    for(unsigned i=8;i<28;i+=4) {
        uint32_t bits=m8_filter_read32(literal+i);
        if((bits&0x7fffffffu) && !(bits&0x7f800000u)) return 0;
    }
    uint64_t wide=(uint64_t)m8_filter_read32(literal)|((uint64_t)m8_filter_read32(literal+4u)<<32);
    if((wide&UINT64_C(0x7fffffffffffffff)) && !(wide&UINT64_C(0x7ff0000000000000))) return 0;
#endif
    uint8_t *restrict voice=object,*restrict param=parameters,*restrict scratch=sample;
    uint8_t *restrict output_left=left,*restrict output_right=right,*restrict recorded=record;
    const uint8_t *restrict constants=literal;
    m8_cpu flags;flags.xpsr=cpu->xpsr;flags.fpscr=cpu->fpscr;
    /* Every modified lane is defined by the interval before its first read.
     * Only s17..s21 carry input coefficients across this boundary. */
    float s[32];for(unsigned i=17;i<=21;i++) s[i]=m8_fp_float(cpu->s[i]);
    uint32_t hold=m8_filter_read16(voice+0x1ecu),integer=0,index=0;
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
            s[0]=m8_fp_float((uint32_t)m8_fp_from_int(&flags,signed_sample,0,0,32,0,0));
            s[0]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[21],0);
            m8_filter_write(scratch,s[0]);
            s[14]=-1.0f;s[15]=1.0f;
            s[0]=m8_filter_op(&flags,M8_FP_MUL,s[0],s[1],0);
            s[0]=m8_fp_float(m8_fp_eval32(&flags,M8_FP_MAXNM,m8_fp_bits32(s[0]),m8_fp_bits32(s[14]),0));
            s[0]=m8_fp_float(m8_fp_eval32(&flags,M8_FP_MINNM,m8_fp_bits32(s[0]),m8_fp_bits32(s[15]),0));
            m8_filter_write(scratch,s[0]);m8_filter_write(voice+0x1f0u,s[0]);
        } else m8_filter_write32(scratch,m8_filter_read32(voice+0x1f0u));
        s[15]=m8_filter_read(param+0x14u);hold=(hold+1u)&0xffffu;
        s[14]=m8_fp_float((uint32_t)m8_fp_from_int(&flags,hold,0,0,32,0,0));
        comparison=m8_braids_filter_compare(&flags,s[14],s[15],0);
        if(m8_condition(comparison,13)) hold=0;
        m8_filter_write16(voice+0x1ecu,hold);
        s[14]=m8_filter_read(param+0x18u);
        comparison=m8_braids_filter_compare(&flags,s[14],0.0f,0);
        if(!m8_condition(comparison,1)) {
            s[15]=m8_filter_op(&flags,M8_FP_MUL,s[14],m8_filter_read(scratch),0);
            uint32_t quantized=m8_filter_to_int(&flags,s[15],0);
            uint32_t signed_sample=m8_braids_filter_sx16(quantized);
            s[15]=m8_fp_float((uint32_t)m8_fp_from_int(&flags,signed_sample,0,0,32,0,0));
            s[13]=m8_filter_op(&flags,M8_FP_DIV,s[15],s[14],0);m8_filter_write(scratch,s[13]);
        }
        s[1]=m8_filter_read(param+0x48u);s[0]=m8_filter_read(param+0x44u);
        m8_filter_apply(&flags,s,0,scratch,voice+0x20u,constants);
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
    cpu->r[6]=final;cpu->r[7]+=count*2u;cpu->r[8]+=count*4u;cpu->r[14]=0x19bd1u;
    cpu->r[15]=final==end ? 0x19bd2u : 0x19b94u;
    cpu->xpsr=flags.xpsr;cpu->fpscr=flags.fpscr;
    cpu->s[0]=m8_fp_bits32(s[0]);cpu->s[1]=m8_fp_bits32(s[1]);
    for(unsigned i=4;i<=16;i++) cpu->s[i]=m8_fp_bits32(s[i]);
    cpu->s[22]=m8_fp_bits32(s[22]);
    return 1;
}
#endif
#endif
