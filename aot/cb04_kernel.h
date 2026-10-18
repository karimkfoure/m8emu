/* Headless 6.5.2C six-ring stereo delay loop, guest 0xcb04 range 0xd2f6..0xd7d2.
 * The translated caller retains allocation, filter calls and publication.
 * Failure leaves CPU and RAM unchanged. Success stops before 0xd7d2.
 * Guest values stay in host locals; FP operation order and FZ/DN semantics
 * use the native FP contract, with exact fallback for special results.
 */
#ifndef M8_AOT_CB04_KERNEL_H
#define M8_AOT_CB04_KERNEL_H
#include "filter_kernel.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_CB04_KERNEL) && !defined(__wasm__) && \
    !defined(__EMSCRIPTEN__) && !defined(M8_WEB_FP_TEST)
/* WebAssembly builds use the stub below and run the translated body: the
 * kernel's exact path needs m8_fp_native_minmax32, which the wasm
 * fp_wasm.h shadow headers do not declare. Native builds are unaffected. */

/* The firmware wraps once, then truncates each index to U16. Entry bounds
 * prove that this is a ring index; retain the same one-step arithmetic. */
M8_MEM_INLINE uint32_t m8_cb04_wrap(int32_t index,uint32_t length) {
    if(index<0) index+=(int32_t)length;
    else if(index>=(int32_t)length) index-=(int32_t)length;
    return (uint16_t)index;
}
M8_MEM_INLINE int m8_cb04_range(float value,float lo,float hi) {
    if(!m8_fp_common32(m8_fp_bits32(value))) return 0;
    return value>=lo && value<=hi;
}
M8_MEM_INLINE uint32_t m8_cb04_next(uint32_t index,uint32_t length) {
    return index+1==length?0:index+1;
}
M8_MEM_INLINE void m8_cb04_cmp(m8_cpu *cpu,float a,float b,int signaling) {
    m8_fp_cmp32(cpu,m8_fp_bits32(a),m8_fp_bits32(b),signaling);
}
M8_MEM_INLINE uint8_t *m8_cb04_ram(m8_mem *mem,uint32_t address,uint32_t size) {
    uint8_t *p=m8_region_pointer(&mem->dtcm,address,size);
    return p?p:m8_region_pointer(&mem->ocram,address,size);
}
static _Thread_local unsigned long m8_cb04_kernel_admissions_tls;
unsigned long m8_cb04_kernel_admissions(void) {
    return m8_cb04_kernel_admissions_tls;
}
/* Admission proves that every tap conversion is finite and fits int32_t.
 * Result-only builds must leave all cumulative guest flags unchanged. */
M8_MEM_INLINE uint32_t m8_cb04_to_int(m8_cpu *cpu,float value) {
#ifdef M8_NATIVE_FP_NO_STICKY
    (void)cpu;
    return (uint32_t)(int32_t)value;
#else
    return m8_filter_to_int(cpu,value,0);
#endif
}
static M8_FILTER_COLD float m8_cb04_special(m8_cpu *cpu,int op,float a,float b,float d) {
    return m8_fp_float(m8_fp_native32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),m8_fp_bits32(d)));
}
M8_MEM_INLINE float m8_cb04_op(m8_cpu *cpu,int op,float a,float b,float d) {
#if defined(M8_NATIVE_FP_NO_STICKY) && defined(__x86_64__)
    float result;
    switch(op) {
    case M8_FP_ADD:result=a+b;break;
    case M8_FP_SUB:result=a-b;break;
    case M8_FP_MUL:result=a*b;break;
    default:result=fmaf(a,b,d);break;
    }
    /* ADD/SUB cancellation is exact near zero. MUL/FMA can round a tiny
     * result to min-normal on x86, while ARM flushes before rounding.
     * Preserve NaN canonicalization and that boundary through native32. */
    if(op==M8_FP_ADD || op==M8_FP_SUB) {
        if(M8_FILTER_UNLIKELY(isnan(result))) return m8_cb04_special(cpu,op,a,b,d);
    } else if(M8_FILTER_UNLIKELY(!islessgreater(fabsf(result),0x1p-126f))) {
        return m8_cb04_special(cpu,op,a,b,d);
    }
    return result;
#else
    return m8_filter_op(cpu,op,a,b,d);
#endif
}
M8_MEM_INLINE float m8_cb04_minmax(m8_cpu *cpu,int op,float a,float b) {
#ifdef M8_NATIVE_FP_NO_STICKY
    return m8_fp_float(m8_fp_eval32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b),0));
#else
    return m8_fp_float(m8_fp_native_minmax32(cpu,op,m8_fp_bits32(a),m8_fp_bits32(b)));
#endif
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_cb04_kernel(m8_cpu *cpu,m8_mem *mem) {
    if(cpu->r[15]!=0xd2f6u || !m8_fp_native_supported(cpu) ||
       ((cpu->r[4]|cpu->r[13]|cpu->r[8]|cpu->r[9])&3u) ||
       cpu->r[4]>UINT32_MAX-0xccu || cpu->r[13]>UINT32_MAX-0x34u) return 0;
    /* Every admitted span is written. Code memory must retain the normal
     * write callback and its translation-invalidation boundary. */
    uint8_t *object=m8_cb04_ram(mem,cpu->r[4]+0x48u,0x84u);
    uint8_t *stack=m8_cb04_ram(mem,cpu->r[13],0x34u);
    if(!object || !stack || m8_filter_read32(stack+0x10)!=0 || object[0x95-0x48]==1 ||
       object[0x95-0x48]>2 || cpu->r[3]!=object[0x94-0x48]) return 0;
    const uint32_t bases[6]={m8_filter_read32(object),m8_filter_read32(object+0xc),
        m8_filter_read32(object+4),m8_filter_read32(object+0x10),
        m8_filter_read32(object+8),m8_filter_read32(object+0x14)};
    const uint32_t lengths[6]={cpu->r[7],cpu->r[7],cpu->r[6],cpu->r[6],cpu->r[5],cpu->r[5]};
    const uint32_t index_offsets[6]={0x18,0x1e,0x1a,0x20,0x1c,0x22};
    uint8_t *ring[6];
    for(unsigned k=0;k<6;k++) {
        if(lengths[k]<4 || lengths[k]>32767 || (bases[k]&3u) || bases[k]>UINT32_MAX-lengths[k]*4u) return 0;
        ring[k]=m8_cb04_ram(mem,bases[k],lengths[k]*4u);
        if(!ring[k] || m8_filter_read16(object+index_offsets[k])>=lengths[k]) return 0;
    }
    if(cpu->r[0]!=bases[5] || cpu->r[1]!=bases[4] ||
       cpu->r[5]!=m8_filter_read16(object+0x28) ||
       cpu->r[6]!=m8_filter_read16(object+0x26) ||
       cpu->r[7]!=m8_filter_read16(object+0x24)) return 0;
    const uint32_t end=m8_filter_read32(stack+0x14);
    if(end<=cpu->r[8] || (end-cpu->r[8])%4 || end-cpu->r[8]>256 ||
       cpu->r[8]!=m8_filter_read32(stack+0x2c) || cpu->r[9]!=m8_filter_read32(stack+0x28) ||
       cpu->r[9]>UINT32_MAX-(end-cpu->r[8])) return 0;
    uint32_t count=(end-cpu->r[8])/4;
    uint8_t *left=m8_cb04_ram(mem,cpu->r[9],count*4),*right=m8_cb04_ram(mem,cpu->r[8],count*4);
    if(!left || !right) return 0;
    uintptr_t owners[11]={(uintptr_t)cpu,(uintptr_t)object,(uintptr_t)stack,(uintptr_t)left,(uintptr_t)right};
    uint32_t sizes[11]={sizeof(*cpu),0x84,0x34,count*4,count*4};
    for(unsigned k=0;k<6;k++) {owners[k+5]=(uintptr_t)ring[k];sizes[k+5]=lengths[k]*4;}
    for(unsigned i=0;i<11;i++) for(unsigned j=0;j<i;j++)
        if(!m8_filter_disjoint(owners[i],sizes[i],owners[j],sizes[j])) return 0;
    float s[32];for(unsigned i=0;i<32;i++) s[i]=m8_fp_float(cpu->s[i]);
    /* Only delay positions depend on these controls. Their convex smoothing
     * and bounded parabolic modulation keep all four taps inside one wrap.
     * Other values, including ring samples, use the normal NaN repair path. */
    if(cpu->s[16]!=0x3f000000u || cpu->s[17]!=0x3f800000u ||
       !m8_cb04_range(s[18],0,1) ||
       !m8_cb04_range(m8_filter_read(object+0x50),0.125f,0.875f) ||
       !m8_cb04_range(m8_filter_read(object+0x6c),0.125f,0.875f) ||
       !m8_cb04_range(m8_filter_read(object+0x60),0,0.25f) ||
       !m8_cb04_range(m8_filter_read(object+0x78),0,0.25f) ||
       !m8_cb04_range(m8_filter_read(object+0x58),-0.5f,0.5f) ||
       !m8_cb04_range(m8_filter_read(object+0x5c),0,0.001f) ||
       !m8_cb04_range(m8_filter_read(object+0x74),0,0.001f) ||
       cpu->s[22]!=m8_fp_bits32((float)(cpu->r[6]-1)) ||
       cpu->s[23]!=m8_fp_bits32((float)(cpu->r[7]-1)) ||
       cpu->s[24]!=m8_fp_bits32((float)(cpu->r[5]-1)) ||
       cpu->s[25]!=(cpu->s[23]^0x80000000u) || cpu->s[26]!=(cpu->s[24]^0x80000000u) ||
       m8_filter_read32(stack+0x18)!=(cpu->s[22]^0x80000000u)) return 0;
    /* Convex smoothing keeps center and depth within these entry bounds.
     * Parabolic modulation has magnitude at most depth. This margin keeps
     * pos-1..pos+2 inside one ring wrap, including the shortest rings. */
    float largest_depth=fmaxf(m8_filter_read(object+0x60),m8_filter_read(object+0x78));
    float lowest_center=fminf(m8_filter_read(object+0x50),m8_filter_read(object+0x6c));
    float highest_center=fmaxf(m8_filter_read(object+0x50),m8_filter_read(object+0x6c));
    if(lowest_center-largest_depth<0.03125f || highest_center+largest_depth>0.96875f) return 0;
    uint8_t *restrict o=object;
    uint32_t r0=cpu->r[0],r1=cpu->r[1],r2=cpu->r[2],r3=cpu->r[3];
    uint32_t r10=cpu->r[10],r11=cpu->r[11],r12=cpu->r[12],r14=cpu->r[14];
#define M8_CB04_LD(offset) m8_filter_read(o+(offset)-0x48)
#define M8_CB04_ST(offset,value) m8_filter_write(o+(offset)-0x48,(value))
#define M8_CB04_ADD(a,b) m8_cb04_op(cpu,M8_FP_ADD,(a),(b),0)
#define M8_CB04_SUB(a,b) m8_cb04_op(cpu,M8_FP_SUB,(a),(b),0)
#define M8_CB04_MUL(a,b) m8_cb04_op(cpu,M8_FP_MUL,(a),(b),0)
#define M8_CB04_FMA(a,b,c) m8_cb04_op(cpu,M8_FP_FMA,(a),(b),(c))
#define M8_CB04_FMS(a,b,c) M8_CB04_FMA(-(a),(b),(c))
#define M8_CB04_NMS(a,b,c) M8_CB04_FMA((a),(b),-(c))
    /* Alias guards make the control inputs constant and give this loop sole
     * ownership of its object state until the block ends. Keep that state
     * local, then publish the same final words before translated execution. */
    const float target_center=M8_CB04_LD(0x98);
    const float target_step=M8_CB04_LD(0xa4);
    const float target_depth=M8_CB04_LD(0xa8);
    const float target_width=M8_CB04_LD(0xac);
    const float target_feedback=M8_CB04_LD(0x9c);
    float center=M8_CB04_LD(0xb4);
    float step=M8_CB04_LD(0xbc);
    float depth=M8_CB04_LD(0xc0);
    float width=M8_CB04_LD(0xc4);
    float freeze=M8_CB04_LD(0xc8);
    float feedback=M8_CB04_LD(0xb8);
    float feedback_left=M8_CB04_LD(0x90);
    float feedback_right=M8_CB04_LD(0x8c);
    float phase=M8_CB04_LD(0xa0);
    float delay0=M8_CB04_LD(0x74);
    float delay1=M8_CB04_LD(0x80);
    float delay2=M8_CB04_LD(0x78);
    float delay3=M8_CB04_LD(0x84);
    float delay4=M8_CB04_LD(0x7c);
    float delay5=M8_CB04_LD(0x88);
    const uint32_t freeze_target=r3;
    uint32_t index[6];
    for(unsigned k=0;k<6;k++) index[k]=m8_filter_read16(o+index_offsets[k]);
    for(uint32_t n=0;n<count;n++) {
        s[15]=freeze;s[11]=center;s[13]=freeze_target==0?s[31]:s[17];
        s[13]=M8_CB04_SUB(s[13],s[15]);s[8]=target_center;s[6]=feedback;
        s[15]=M8_CB04_FMA(s[13],s[18],s[15]);s[9]=step;
        s[8]=M8_CB04_SUB(s[8],s[11]);s[14]=width;s[10]=depth;s[7]=target_step;
        s[11]=M8_CB04_FMA(s[8],s[18],s[11]);s[2]=target_width;s[3]=target_depth;
        s[7]=M8_CB04_SUB(s[7],s[9]);s[4]=M8_CB04_SUB(s[17],s[15]);freeze=s[15];
        s[8]=M8_CB04_SUB(s[15],s[6]);s[2]=M8_CB04_SUB(s[2],s[14]);s[13]=feedback_left;
        s[3]=M8_CB04_SUB(s[3],s[10]);s[12]=feedback_right;s[8]=M8_CB04_FMA(s[4],s[5],s[8]);
        center=s[11];s[15]=s[6];s[9]=M8_CB04_FMA(s[7],s[18],s[9]);
        s[14]=M8_CB04_FMA(s[2],s[18],s[14]);s[10]=M8_CB04_FMA(s[3],s[18],s[10]);
        s[15]=M8_CB04_FMA(s[8],s[18],s[15]);step=s[9];width=s[14];depth=s[10];feedback=s[15];
        s[15]=(float)index[4];s[14]=(float)index[5];
        if(n+1==count) {m8_filter_write32(stack,index[4]);m8_filter_write32(stack+4,index[5]);}
        /* The guarded finite phase values cannot raise compare exceptions.
         * Clipping below replaces their FPSCR condition flags. Keep the four
         * branch decisions, without publishing flags that no code reads. */
        /* Wrap the base phase and its three offsets, in the original branch order. */
        s[6]=phase;s[8]=m8_filter_read(right+n*4);s[9]=M8_CB04_ADD(s[9],s[6]);
        s[7]=m8_filter_read(left+n*4);s[12]=M8_CB04_FMA(s[8],s[4],s[12]);s[8]=0.375f;
        s[13]=M8_CB04_FMA(s[7],s[4],s[13]);
        if(s[9]>s[16]) s[9]=M8_CB04_SUB(s[9],s[17]);
        s[7]=s[9]>s[8]?M8_CB04_SUB(s[9],0.875f):M8_CB04_ADD(s[9],0.125f);
        phase=s[9];s[4]=fabsf(s[7]);s[5]=16;s[6]=8;s[8]=0.25f;
        s[7]=M8_CB04_MUL(s[7],s[10]);s[6]=M8_CB04_FMS(s[4],s[5],s[6]);
        s[7]=M8_CB04_MUL(s[7],s[6]);s[8]=s[9]>s[8]?M8_CB04_SUB(s[9],0.75f):M8_CB04_ADD(s[9],s[8]);
        s[3]=fabsf(s[8]);s[4]=16;s[6]=8;s[5]=0.125f;s[8]=M8_CB04_MUL(s[8],s[10]);
        s[6]=M8_CB04_FMS(s[3],s[4],s[6]);s[8]=M8_CB04_MUL(s[8],s[6]);
        s[6]=0.375f;s[9]=s[9]>s[5]?M8_CB04_SUB(s[9],0.625f):M8_CB04_ADD(s[9],s[6]);s[6]=8;
        /* Ring one: left all-pass stage. */
        s[5]=M8_CB04_ADD(s[7],s[11]);s[4]=fabsf(s[9]);s[11]=16;s[9]=M8_CB04_MUL(s[9],s[10]);s[10]=s[6];
        s[10]=M8_CB04_FMS(s[4],s[11],s[10]);s[11]=(float)index[0];s[11]=M8_CB04_FMS(s[5],s[23],s[11]);s[9]=M8_CB04_MUL(s[9],s[10]);
        s[10]=M8_CB04_SUB(s[11],s[16]);int32_t pos=(int32_t)m8_cb04_to_int(cpu,s[10]);s[10]=(float)pos;s[11]=M8_CB04_SUB(s[11],s[10]);
        uint32_t a=m8_cb04_wrap(pos-1,lengths[0]),b=m8_cb04_wrap(pos,lengths[0]),c=m8_cb04_wrap(pos+1,lengths[0]),d=m8_cb04_wrap(pos+2,lengths[0]);
        r11=bases[0]+b*4;r10=bases[0]+c*4;r14=bases[0]+a*4;r3=bases[0]+d*4;r12=bases[0]+index[0]*4;
        s[6]=-0.5f;s[1]=M8_CB04_SUB(s[11],s[17]);s[4]=m8_filter_read(ring[0]+b*4);s[3]=m8_filter_read(ring[0]+c*4);
        s[10]=m8_filter_read(ring[0]+a*4);s[5]=M8_CB04_SUB(s[4],s[3]);s[2]=m8_filter_read(ring[0]+d*4);
        s[3]=M8_CB04_SUB(s[3],s[10]);s[10]=delay0;s[2]=M8_CB04_SUB(s[2],s[4]);
        s[10]=M8_CB04_FMA(s[13],s[6],s[10]);s[6]=s[5];s[6]=M8_CB04_FMA(s[3],s[16],s[6]);s[5]=M8_CB04_ADD(s[6],s[5]);
        s[5]=M8_CB04_FMA(s[2],s[16],s[5]);s[2]=M8_CB04_MUL(s[10],s[16]);s[13]=M8_CB04_ADD(s[2],s[13]);
        s[6]=M8_CB04_NMS(s[1],s[5],s[6]);s[6]=M8_CB04_MUL(s[11],s[6]);s[6]=M8_CB04_FMA(s[3],s[16],s[6]);s[4]=M8_CB04_FMA(s[11],s[6],s[4]);
        delay0=s[4];m8_filter_write(ring[0]+index[0]*4,s[13]);index[0]=m8_cb04_next(index[0],lengths[0]);r2=index[0];
        /* Ring two: right all-pass stage. */
        s[11]=center;s[13]=(float)index[1];s[11]=M8_CB04_SUB(s[11],s[7]);s[13]=M8_CB04_FMA(s[25],s[11],s[13]);
        s[11]=M8_CB04_SUB(s[13],s[16]);pos=(int32_t)m8_cb04_to_int(cpu,s[11]);s[11]=(float)pos;s[13]=M8_CB04_SUB(s[13],s[11]);
        a=m8_cb04_wrap(pos-1,lengths[1]);b=m8_cb04_wrap(pos,lengths[1]);c=m8_cb04_wrap(pos+1,lengths[1]);d=m8_cb04_wrap(pos+2,lengths[1]);
        r14=bases[1]+a*4;r11=bases[1]+b*4;r10=bases[1]+c*4;r3=bases[1]+d*4;r12=bases[1]+index[1]*4;
        s[6]=delay1;s[1]=M8_CB04_SUB(s[13],s[17]);s[11]=m8_filter_read(ring[1]+a*4);s[4]=m8_filter_read(ring[1]+b*4);
        s[3]=m8_filter_read(ring[1]+c*4);s[5]=m8_filter_read(ring[1]+d*4);s[7]=M8_CB04_SUB(s[4],s[3]);s[3]=M8_CB04_SUB(s[3],s[11]);
        s[11]=-0.5f;s[5]=M8_CB04_SUB(s[5],s[4]);s[6]=M8_CB04_FMA(s[12],s[11],s[6]);s[11]=s[7];s[11]=M8_CB04_FMA(s[3],s[16],s[11]);
        s[7]=M8_CB04_ADD(s[7],s[11]);s[7]=M8_CB04_FMA(s[5],s[16],s[7]);s[5]=M8_CB04_MUL(s[6],s[16]);s[12]=M8_CB04_ADD(s[5],s[12]);
        s[11]=M8_CB04_NMS(s[1],s[7],s[11]);s[11]=M8_CB04_MUL(s[13],s[11]);s[11]=M8_CB04_FMA(s[3],s[16],s[11]);s[4]=M8_CB04_FMA(s[13],s[11],s[4]);
        delay1=s[4];m8_filter_write(ring[1]+index[1]*4,s[12]);index[1]=m8_cb04_next(index[1],lengths[1]);r2=index[1];
        /* Ring three: left stage with subtraction. */
        s[12]=center;s[13]=(float)index[2];s[12]=M8_CB04_ADD(s[8],s[12]);s[13]=M8_CB04_FMS(s[12],s[22],s[13]);
        s[12]=M8_CB04_SUB(s[13],s[16]);pos=(int32_t)m8_cb04_to_int(cpu,s[12]);s[12]=(float)pos;s[13]=M8_CB04_SUB(s[13],s[12]);
        a=m8_cb04_wrap(pos-1,lengths[2]);b=m8_cb04_wrap(pos,lengths[2]);c=m8_cb04_wrap(pos+1,lengths[2]);d=m8_cb04_wrap(pos+2,lengths[2]);
        r11=bases[2]+b*4;r10=bases[2]+c*4;r14=bases[2]+a*4;r3=bases[2]+d*4;r12=bases[2]+index[2]*4;
        s[11]=delay2;s[1]=M8_CB04_SUB(s[13],s[17]);s[4]=m8_filter_read(ring[2]+b*4);s[3]=m8_filter_read(ring[2]+c*4);s[12]=m8_filter_read(ring[2]+a*4);
        s[11]=M8_CB04_SUB(s[11],s[2]);s[0]=M8_CB04_SUB(s[4],s[3]);s[7]=m8_filter_read(ring[2]+d*4);s[3]=M8_CB04_SUB(s[3],s[12]);s[7]=M8_CB04_SUB(s[7],s[4]);
        s[12]=s[0];s[12]=M8_CB04_FMA(s[3],s[16],s[12]);s[2]=M8_CB04_ADD(s[0],s[12]);s[2]=M8_CB04_FMA(s[7],s[16],s[2]);
        s[7]=M8_CB04_MUL(s[11],s[16]);s[10]=M8_CB04_ADD(s[7],s[10]);s[12]=M8_CB04_NMS(s[1],s[2],s[12]);s[12]=M8_CB04_MUL(s[13],s[12]);
        s[12]=M8_CB04_FMA(s[3],s[16],s[12]);s[4]=M8_CB04_FMA(s[13],s[12],s[4]);delay2=s[4];m8_filter_write(ring[2]+index[2]*4,s[10]);
        index[2]=m8_cb04_next(index[2],lengths[2]);r2=index[2];
        /* Ring four: right stage with subtraction. */
        s[12]=center;s[13]=(float)index[3];s[10]=m8_filter_read(stack+0x18);s[12]=M8_CB04_SUB(s[12],s[8]);s[13]=M8_CB04_FMA(s[10],s[12],s[13]);
        s[12]=M8_CB04_SUB(s[13],s[16]);pos=(int32_t)m8_cb04_to_int(cpu,s[12]);s[12]=(float)pos;s[13]=M8_CB04_SUB(s[13],s[12]);
        a=m8_cb04_wrap(pos-1,lengths[3]);b=m8_cb04_wrap(pos,lengths[3]);c=m8_cb04_wrap(pos+1,lengths[3]);d=m8_cb04_wrap(pos+2,lengths[3]);
        r11=bases[3]+b*4;r10=bases[3]+c*4;r14=bases[3]+a*4;r3=bases[3]+d*4;r12=bases[3]+index[3]*4;
        s[2]=M8_CB04_SUB(s[13],s[17]);s[10]=delay3;s[4]=m8_filter_read(ring[3]+b*4);s[3]=m8_filter_read(ring[3]+c*4);s[12]=m8_filter_read(ring[3]+a*4);
        s[10]=M8_CB04_SUB(s[10],s[5]);s[0]=M8_CB04_SUB(s[4],s[3]);s[1]=m8_filter_read(ring[3]+d*4);s[3]=M8_CB04_SUB(s[3],s[12]);s[1]=M8_CB04_SUB(s[1],s[4]);
        s[12]=s[0];s[8]=M8_CB04_MUL(s[10],s[16]);s[12]=M8_CB04_FMA(s[3],s[16],s[12]);s[6]=M8_CB04_ADD(s[8],s[6]);
        s[0]=M8_CB04_ADD(s[0],s[12]);s[0]=M8_CB04_FMA(s[1],s[16],s[0]);s[12]=M8_CB04_NMS(s[2],s[0],s[12]);s[12]=M8_CB04_MUL(s[13],s[12]);
        s[12]=M8_CB04_FMA(s[3],s[16],s[12]);s[4]=M8_CB04_FMA(s[13],s[12],s[4]);delay3=s[4];m8_filter_write(ring[3]+index[3]*4,s[6]);
        index[3]=m8_cb04_next(index[3],lengths[3]);r2=index[3];
        /* Ring five: left final stage. */
        s[13]=center;s[13]=M8_CB04_ADD(s[9],s[13]);s[15]=M8_CB04_FMS(s[13],s[24],s[15]);s[13]=M8_CB04_SUB(s[15],s[16]);
        pos=(int32_t)m8_cb04_to_int(cpu,s[13]);s[13]=(float)pos;s[15]=M8_CB04_SUB(s[15],s[13]);
        a=m8_cb04_wrap(pos-1,lengths[4]);b=m8_cb04_wrap(pos,lengths[4]);c=m8_cb04_wrap(pos+1,lengths[4]);d=m8_cb04_wrap(pos+2,lengths[4]);
        r14=bases[4]+b*4;r12=bases[4]+c*4;r2=bases[4]+a*4;r3=bases[4]+d*4;r1=bases[4]+index[4]*4;
        s[4]=M8_CB04_SUB(s[15],s[17]);s[20]=delay4;s[6]=m8_filter_read(ring[4]+b*4);s[5]=m8_filter_read(ring[4]+c*4);
        s[13]=m8_filter_read(ring[4]+a*4);s[20]=M8_CB04_SUB(s[20],s[7]);s[12]=M8_CB04_SUB(s[6],s[5]);s[3]=m8_filter_read(ring[4]+d*4);
        s[5]=M8_CB04_SUB(s[5],s[13]);s[3]=M8_CB04_SUB(s[3],s[6]);s[13]=s[12];s[11]=M8_CB04_FMA(s[20],s[16],s[11]);
        s[13]=M8_CB04_FMA(s[5],s[16],s[13]);s[12]=M8_CB04_ADD(s[12],s[13]);s[12]=M8_CB04_FMA(s[3],s[16],s[12]);
        s[13]=M8_CB04_NMS(s[4],s[12],s[13]);s[13]=M8_CB04_MUL(s[15],s[13]);s[13]=M8_CB04_FMA(s[5],s[16],s[13]);s[6]=M8_CB04_FMA(s[15],s[13],s[6]);
        delay4=s[6];m8_filter_write(ring[4]+index[4]*4,s[11]);index[4]=m8_cb04_next(index[4],lengths[4]);r3=index[4];
        /* Ring six: right final stage. */
        s[15]=center;s[15]=M8_CB04_SUB(s[15],s[9]);s[14]=M8_CB04_FMA(s[26],s[15],s[14]);s[15]=M8_CB04_SUB(s[14],s[16]);
        pos=(int32_t)m8_cb04_to_int(cpu,s[15]);s[15]=(float)pos;s[14]=M8_CB04_SUB(s[14],s[15]);
        a=m8_cb04_wrap(pos-1,lengths[5]);b=m8_cb04_wrap(pos,lengths[5]);c=m8_cb04_wrap(pos+1,lengths[5]);d=m8_cb04_wrap(pos+2,lengths[5]);
        r12=bases[5]+b*4;r1=bases[5]+c*4;r2=bases[5]+a*4;r3=bases[5]+d*4;r0=bases[5]+index[5]*4;
        s[6]=M8_CB04_SUB(s[14],s[17]);s[13]=delay5;s[11]=m8_filter_read(ring[5]+b*4);s[9]=m8_filter_read(ring[5]+c*4);s[15]=m8_filter_read(ring[5]+a*4);
        s[13]=M8_CB04_SUB(s[13],s[8]);s[12]=M8_CB04_SUB(s[11],s[9]);s[7]=m8_filter_read(ring[5]+d*4);s[9]=M8_CB04_SUB(s[9],s[15]);s[7]=M8_CB04_SUB(s[7],s[11]);
        s[15]=s[12];s[10]=M8_CB04_FMA(s[13],s[16],s[10]);s[15]=M8_CB04_FMA(s[9],s[16],s[15]);s[12]=M8_CB04_ADD(s[12],s[15]);
        s[12]=M8_CB04_FMA(s[7],s[16],s[12]);s[15]=M8_CB04_NMS(s[6],s[12],s[15]);s[12]=-1;s[15]=M8_CB04_MUL(s[14],s[15]);
        s[15]=M8_CB04_FMA(s[9],s[16],s[15]);s[11]=M8_CB04_FMA(s[14],s[15],s[11]);delay5=s[11];m8_filter_write(ring[5]+index[5]*4,s[10]);
        index[5]=m8_cb04_next(index[5],lengths[5]);r3=index[5];
        s[15]=feedback;s[14]=M8_CB04_MUL(s[15],s[20]);s[15]=M8_CB04_MUL(s[15],s[13]);
        s[14]=m8_cb04_minmax(cpu,M8_FP_MINNM,s[14],s[17]);feedback_right=s[14];
        m8_cb04_cmp(cpu,s[15],s[17],1);
        if((cpu->fpscr&0xf0000000u)==0x20000000u) {
            feedback_left=s[17];m8_cb04_cmp(cpu,s[14],s[12],1);
            if(cpu->fpscr&0x80000000u) feedback_right=s[12];
            s[15]=1;feedback_left=s[15];
        } else {
            m8_cb04_cmp(cpu,s[15],s[12],0);feedback_left=s[15];
            s[14]=m8_cb04_minmax(cpu,M8_FP_MAXNM,s[14],s[12]);feedback_right=s[14];
            if(cpu->fpscr&0x80000000u) s[15]=s[12];
            feedback_left=s[15];
        }
        s[15]=width;s[12]=M8_CB04_ADD(s[20],s[13]);s[14]=M8_CB04_SUB(s[17],s[15]);
        s[15]=M8_CB04_MUL(s[15],s[20]);s[14]=M8_CB04_MUL(s[14],s[16]);s[15]=M8_CB04_FMA(s[12],s[14],s[15]);m8_filter_write(left+n*4,s[15]);
        s[15]=width;s[15]=M8_CB04_MUL(s[13],s[15]);s[15]=M8_CB04_FMA(s[12],s[14],s[15]);m8_filter_write(right+n*4,s[15]);
        if(n+1<count) {s[5]=target_feedback;r3=freeze_target;r1=bases[4];r0=bases[5];}
    }
    M8_CB04_ST(0xb4,center);
    M8_CB04_ST(0xbc,step);
    M8_CB04_ST(0xc0,depth);
    M8_CB04_ST(0xc4,width);
    M8_CB04_ST(0xc8,freeze);
    M8_CB04_ST(0xb8,feedback);
    M8_CB04_ST(0x90,feedback_left);
    M8_CB04_ST(0x8c,feedback_right);
    M8_CB04_ST(0xa0,phase);
    M8_CB04_ST(0x74,delay0);
    M8_CB04_ST(0x80,delay1);
    M8_CB04_ST(0x78,delay2);
    M8_CB04_ST(0x84,delay3);
    M8_CB04_ST(0x7c,delay4);
    M8_CB04_ST(0x88,delay5);
    for(unsigned k=0;k<6;k++) m8_filter_write16(o+index_offsets[k],index[k]);
    cpu->r[0]=r0;cpu->r[1]=r1;cpu->r[2]=r2;cpu->r[3]=end;cpu->r[8]=end;cpu->r[9]+=count*4;
    cpu->r[10]=r10;cpu->r[11]=r11;cpu->r[12]=r12;cpu->r[14]=r14;
    for(unsigned i=0;i<16;i++) cpu->s[i]=m8_fp_bits32(s[i]);
    cpu->s[20]=m8_fp_bits32(s[20]);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;cpu->r[15]=0xd7d2u;
    ++m8_cb04_kernel_admissions_tls;
#undef M8_CB04_LD
#undef M8_CB04_ST
#undef M8_CB04_ADD
#undef M8_CB04_SUB
#undef M8_CB04_MUL
#undef M8_CB04_FMA
#undef M8_CB04_FMS
#undef M8_CB04_NMS
    return 1;
}
#else
static inline int m8_cb04_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu;(void)mem;return 0;
}
#if defined(__wasm__) || defined(__EMSCRIPTEN__) || defined(M8_WEB_FP_TEST)
/* The wasm AOT split compiles this header into many translation units, so
 * the counter must not have external linkage there. Native disabled builds
 * keep the exported symbol for the ctypes difftest. */
static inline unsigned long m8_cb04_kernel_admissions(void) {
    return 0;
}
#else
unsigned long m8_cb04_kernel_admissions(void) {
    return 0;
}
#endif
#endif
#endif
