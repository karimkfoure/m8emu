/* Headless 6.5.2C integer render loop 0x60015b5c..0x60015c3a.
 * Complete the loop-exit compare and resume at 0x60015c3c. All memory checks
 * precede effects; disjoint buffers permit state stores once at the exit. */
#ifndef M8_AOT_BRAIDS_LOOP_H
#define M8_AOT_BRAIDS_LOOP_H
#include "memory_spans.h"
#include "integer.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
static inline int32_t m8_braids_s32(uint32_t bits) {
    int32_t value;memcpy(&value,&bits,sizeof(value));return value;
}
static inline uint32_t m8_braids_sxth(uint32_t bits) {
    uint16_t low=(uint16_t)bits;int16_t value;
    memcpy(&value,&low,sizeof(value));return (uint32_t)(int32_t)value;
}
static inline uint32_t m8_braids_read16(const uint8_t *host) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    int16_t value;memcpy(&value,host,sizeof(value));return (uint32_t)(int32_t)value;
#else
    return m8_braids_sxth(m8_le_read(host,2));
#endif
}
/* The only callers use 8 or 15. Unsigned subtraction supplies sign extension
 * without depending on the C implementation's signed-right-shift behavior. */
static inline uint32_t m8_braids_asr(uint32_t value,unsigned n) {
    return (value>>n)-((value>>31)<<(32u-n));
}
static inline int m8_braids_disjoint(uintptr_t a,uint32_t an,uintptr_t b,uint32_t bn) {
    return a<=b ? b-a>=an : a-b>=bn;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static int m8_braids_loop_kernel(m8_cpu *cpu,m8_mem *mem) {
    if(cpu->r[13]>UINT32_MAX-56u) return 0;
    uint8_t *stack=m8_span_pointer(mem,cpu->r[13]+4u,52u);
    uint8_t *literal=m8_span_pointer(mem,0x60015c50u,8u);
    if(!stack || !literal) return 0;
    uint32_t end=m8_le_read(stack+48u,4),begin=cpu->r[10];
    if(end<=begin || end-begin>UINT32_MAX/2u) return 0;
    uint32_t count=end-begin,out=m8_le_read(stack+8u,4);
    uint32_t phase_address=m8_le_read(stack+12u,4);
    uint32_t table0_address=m8_le_read(literal,4),table1_address=m8_le_read(literal+4u,4);
    /* These two original literal words select consecutive table samples. */
    if(table1_address>UINT32_MAX-514u || table0_address!=table1_address+2u) return 0;
    uint8_t *state=m8_span_pointer(mem,cpu->r[2],24u);
    uint8_t *phase=m8_span_pointer(mem,phase_address,4u);
    uint8_t *input=m8_span_pointer(mem,begin,count);
    uint8_t *output=m8_span_pointer(mem,out,count*2u);
    uint8_t *table=m8_span_pointer(mem,table1_address,514u);
    if(!state || !phase || !input || !output || !table) return 0;
    struct {uint8_t *host;uint32_t size;} spans[]={
        {stack,52u},{literal,8u},{state,24u},{phase,4u},{input,count},
        {output,count*2u},{table,514u},{(uint8_t*)cpu,sizeof(*cpu)}};
    for(unsigned i=0;i<sizeof(spans)/sizeof(spans[0]);i++)
        for(unsigned j=0;j<i;j++)
            if(!m8_braids_disjoint((uintptr_t)spans[i].host,spans[i].size,
                                   (uintptr_t)spans[j].host,spans[j].size)) return 0;
    uint8_t *restrict words=state,*restrict scratch=stack,*restrict phase_word=phase;
    uint8_t *restrict samples=output;
    const uint8_t *restrict triggers=input,*restrict wave=table;
    uint32_t a0=m8_le_read(words,4),a1=m8_le_read(words+4u,4),a2=m8_le_read(words+8u,4);
    uint32_t a3=m8_le_read(words+12u,4),a4=m8_le_read(words+16u,4),a5=m8_le_read(words+20u,4);
    uint32_t p=m8_le_read(phase_word,4);
    uint32_t c0=m8_le_read(scratch+16u,4),c1=m8_le_read(scratch+20u,4);
    uint32_t c2=m8_le_read(scratch+24u,4),c3=m8_le_read(scratch+28u,4);
    uint32_t c4=m8_le_read(scratch+32u,4),c5=m8_le_read(scratch+36u,4);
    uint32_t c6=m8_le_read(scratch+40u,4),gain=m8_le_read(scratch+44u,4);
    uint32_t reset=cpu->r[7],scale=cpu->r[8],maximum=cpu->r[6],minimum=cpu->r[11];
    uint32_t filtered=cpu->r[0],feedback=cpu->r[14],product=0,difference=0;
    for(uint32_t i=0;i<count;i++) {
        if(triggers[i]) a0=a1=a2=a3=a4=a5=reset;
        p+=c0; a0+=c1; a1+=c2; a2+=c3; a3+=c4; a4+=c5; a5+=c6;
        uint32_t sum=(p>>19)+(a0>>19)+(a1>>19)+(a2>>19)+(a3>>19)+(a4>>19)+(a5>>19)-0xf000u;
        product=scale*filtered;
        feedback+=m8_braids_asr(product,15);
        if(m8_braids_s32(feedback)>=m8_braids_s32(maximum)) feedback=maximum;
        if(m8_braids_s32(feedback)<m8_braids_s32(minimum)) feedback=minimum;
        uint32_t index=(sum>>8)&255u,fraction=sum&255u;
        uint32_t low=m8_braids_read16(wave+index*2u);
        uint32_t high=m8_braids_read16(wave+index*2u+2u);
        difference=high-low;
        uint32_t value=m8_braids_sxth(low+m8_braids_asr(difference*fraction,8));
        value-=m8_braids_asr(filtered*gain,15);
        value-=feedback;
        uint32_t update=value*scale;
        if(m8_braids_s32(value)>=m8_braids_s32(maximum)) value=maximum;
        filtered+=m8_braids_asr(update,15);
        if(m8_braids_s32(value)<m8_braids_s32(minimum)) value=minimum;
        m8_le_write(samples+i*2u,2,value);
    }
    m8_le_write(words,4,a0);m8_le_write(words+4u,4,a1);m8_le_write(words+8u,4,a2);
    m8_le_write(words+12u,4,a3);m8_le_write(words+16u,4,a4);m8_le_write(words+20u,4,a5);
    m8_le_write(phase_word,4,p);
    m8_le_write(scratch,4,a2);m8_le_write(scratch+4u,4,a3);m8_le_write(scratch+8u,4,out+count*2u);
    cpu->r[0]=filtered;cpu->r[1]=out+count*2u;cpu->r[3]=end;cpu->r[4]=table0_address;
    cpu->r[5]=difference;cpu->r[9]=a4;cpu->r[10]=end;cpu->r[12]=product;cpu->r[14]=feedback;
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|0x60000000u;
    return 1;
}
#endif
#endif
