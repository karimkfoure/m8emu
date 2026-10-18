/* Whole 0x73bc stereo mixer block. Constant envelope steps only.
 * The native pool callbacks retain all allocation, publication, and service
 * boundaries. Four complete 64-sample loops keep gains and peaks in locals.
 * A negative result leaves CPU and RAM unchanged. A nonnegative result
 * publishes the exact return or safe translated continuation, including the
 * original stack bytes and all 50 CPU words. The caller owns the FP mode.
 */
#ifndef M8_AOT_MIXER_FAST_H
#define M8_AOT_MIXER_FAST_H
#include "fp.h"
#include "integer.h"
#include "filter_kernel.h"
#include "canonical_memory.h"
#include "mixer_fast_fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && defined(M8_AOT_FIXED_MEMORY) && !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT)
int m8_mixer_fast_kernel(m8_cpu*,m8_mem*,m8_aot_fn,m8_aot_fn,m8_aot_fn,m8_aot_fn);
#if defined(M8_AOT_MIXER_FAST_IMPLEMENTATION)
static inline uint8_t *m8_mx_ram(m8_mem *mem,uint32_t address,uint32_t size) {
    if(address&3u) return NULL;
    uint8_t *p=m8_region_pointer(&mem->dtcm,address,size);
    if(!p) p=m8_region_pointer(&mem->ocram,address,size);
    return p;
}
static int m8_mx_qualify(const m8_cpu *cpu,m8_mem *mem) {
    if(!cpu || !mem || !m8_canonical_memory_offset(mem) || m8_current_it ||
       cpu->r[15]!=0x73bcu || (cpu->fpscr&0x03c00000u)!=0x03000000u ||
       (cpu->xpsr&0x0600fc00u) || !(cpu->r[14]&1u)) return 0;
    uint8_t *obj=m8_mx_ram(mem,cpu->r[0],0x108u);
    uint8_t *stack=m8_mx_ram(mem,cpu->r[13]-160u,160u);
    if(!obj || !stack || !m8_filter_disjoint((uintptr_t)obj,0x108u,(uintptr_t)stack,160u)) return 0;
    const m8_region *regions[]={&mem->itcm,&mem->dtcm,&mem->ocram,&mem->flash};
    for(unsigned i=0;i<4;i++) if(!m8_filter_disjoint((uintptr_t)regions[i]->host,regions[i]->size,(uintptr_t)cpu,sizeof(*cpu))) return 0;
    uint8_t *slots=m8_mx_ram(mem,m8_le_read(obj+0x20u,4),64u);
    uint8_t *globals=m8_mx_ram(mem,0x20019228u,44u);
    if(!slots || !globals || m8_le_read(obj+0x20u,4)!=cpu->r[0]+0x88u) return 0;
    /* Empty channels do not evaluate their stored envelope step. */
    for(unsigned i=0;i<8;i++)
        if(m8_le_read(slots+8u*i,4) && m8_le_read(slots+8u*i+4u,4) &&
           (m8_le_read(obj+0x68u+4u*i,4)&0x7fffffffu)) return 0;
    uint8_t *pool=m8_mx_ram(mem,m8_le_read(globals,4),320u*276u);
    if(!pool) return 0;
    uint8_t *counts=m8_pointer(mem,0x2003b472u,4u);
    if(!counts) return 0;
    uint8_t *p[]={obj,stack,pool,globals,counts};
    uint32_t sizes[]={0x108u,160u,320u*276u,44u,4u};
    for(unsigned i=0;i<5;i++) for(unsigned j=0;j<i;j++)
        if(!m8_filter_disjoint((uintptr_t)p[i],sizes[i],(uintptr_t)p[j],sizes[j])) return 0;
    /* Pool callbacks can copy a shared input. Qualify their whole arena and
     * require every initial input to refer to one complete pool block. */
    uint32_t pool_base=m8_le_read(globals,4);
    for(unsigned i=0;i<16;i++) {
        uint32_t a=m8_le_read(slots+4u*i,4);
        if(a && ((uint32_t)(a-pool_base)>=320u*276u || (a-pool_base)%276u)) return 0;
    }
    uint8_t *literal=m8_pointer(mem,0x7c88u,4u);
    return literal && m8_le_read(literal,4)==0;
}
/* A pool callback can stop at its own original PC. Recheck every sample
 * owner after callbacks; a changed layout resumes before the next loop. */
static inline int m8_mx_loop_spans(m8_cpu *cpu,m8_mem *mem,uint32_t source,
        uint32_t destination,uint32_t gain,uint32_t peak,
        uint8_t **s,uint8_t **d,uint8_t **g,uint8_t **p) {
    *s=m8_mx_ram(mem,source,260u);*d=m8_mx_ram(mem,destination,260u);
    *g=m8_mx_ram(mem,gain,4u);*p=m8_mx_ram(mem,peak,4u);
    if(!*s || !*d || !*g || !*p) return 0;
    uint8_t *obj=m8_mx_ram(mem,cpu->r[8],0x108u);
    uint8_t *stack=m8_mx_ram(mem,cpu->r[13]-96u,160u);
    if(!obj || !stack || (uint32_t)(gain-cpu->r[8])>0x104u ||
       (uint32_t)(peak-cpu->r[8])>0x104u) return 0;
    for(unsigned i=0;i<2;i++) {
        uint8_t *b=i?*d:*s;
        if(!m8_filter_disjoint((uintptr_t)b,260u,(uintptr_t)obj,0x108u) ||
           !m8_filter_disjoint((uintptr_t)b,260u,(uintptr_t)stack,160u) ||
           !m8_filter_disjoint((uintptr_t)b,260u,(uintptr_t)cpu,sizeof(*cpu))) return 0;
    }
    return m8_filter_disjoint((uintptr_t)*g,4u,(uintptr_t)*p,4u);
}
/* The ordinary audio domain has no subnormal or non-finite result.
 * MUL is bounded by 2^-64..2^24. ADD cancellation stays above 2^-88;
 * its magnitude stays below 2^25. Partial buffer overlaps use exact helpers.
 * The integer vector scan does not execute floating-point operations. */
static inline int m8_mx_audio_words(const uint8_t *p) {
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    typedef uint32_t lanes __attribute__((vector_size(16)));
    const lanes sign={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const lanes minimum={0x2f800000u,0x2f800000u,0x2f800000u,0x2f800000u};
    const lanes range={0x18000000u,0x18000000u,0x18000000u,0x18000000u};
    lanes bad={0,0,0,0};
    for(unsigned i=4;i<260;i+=16) {
        lanes value;memcpy(&value,p+i,16);value&=sign;
        bad|=(value!=0)&((value-minimum)>range);
    }
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
#else
    for(unsigned i=4;i<260;i+=4) {
        uint32_t value=m8_le_read(p+i,4)&0x7fffffffu;
        if(value && (value<0x2f800000u || value>0x47800000u)) return 0;
    }
    return 1;
#endif
}
static inline int m8_mx_audio_domain(uint32_t gain,uint32_t peak) {
    gain&=0x7fffffffu;
    return (!gain || (gain>=0x2f800000u && gain<=0x43800000u)) &&
           peak<=0x4b800000u && (!peak || peak>=0x00800000u);
}
/* The first pair is scaled in place. The last unrolled group's temporary
 * registers differ from the preceding seven samples; retain those values. */
static inline int m8_mx_scale(m8_cpu *cpu,m8_mem *mem,unsigned peak_offset) {
    if(cpu->r[1] || (cpu->s[13]&0x7fffffffu)) return 0;
    uint8_t *s,*d,*g,*p;
    if(!m8_mx_loop_spans(cpu,mem,cpu->r[2],cpu->r[2],cpu->r[3]+0x48u,cpu->r[3]+peak_offset,&s,&d,&g,&p)) return 0;
    uint32_t gain=m8_le_read(g,4),peak=m8_le_read(p,4),abs7=0,abs8=0;
#if defined(M8_NATIVE_FP_NO_STICKY)
    if(m8_mx_audio_domain(gain,peak) && m8_mx_audio_words(s)) {
        float factor=m8_fp_float(gain),maximum=m8_fp_float(peak);
        for(unsigned i=1;i<=64;i++) {
            float value=m8_fp_float(m8_le_read(s+4u*i,4))*factor;
            m8_le_write(s+4u*i,4,m8_fp_bits32(value));
            float magnitude=fabsf(value);
            if(magnitude>maximum) maximum=magnitude;
            if(i==63) abs7=m8_fp_bits32(magnitude);
            abs8=m8_fp_bits32(magnitude);
        }
        peak=m8_fp_bits32(maximum);
    } else
#endif
    {
    for(unsigned i=1;i<=64;i++) {
        uint32_t value=m8_mxf_op(cpu,M8_FP_MUL,m8_le_read(s+4u*i,4),gain,0);
        m8_le_write(s+4u*i,4,value);
        uint32_t magnitude=value&0x7fffffffu;
        peak=m8_mxf_op(cpu,M8_FP_MAXNM,peak,magnitude,0);
        if(i==63) abs7=magnitude;
        abs8=magnitude;
    }
    }
    m8_le_write(p,4,peak);
    cpu->s[11]=abs7;cpu->s[12]=abs8;cpu->s[15]=peak;
    cpu->r[1]=64u;cpu->r[2]+=256u;
    m8_fp_cmp32(cpu,cpu->s[13],0,0);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    return 1;
}
/* Later pairs accumulate into the first pair. Multiply and add stay
 * separate; the peak records the source contribution, not the sum. */
static inline int m8_mx_add(m8_cpu *cpu,m8_mem *mem,unsigned peak_reg) {
    if(cpu->r[12] || (cpu->s[12]&0x7fffffffu)) return 0;
    uint8_t *s,*d,*g,*p;
    if(!m8_mx_loop_spans(cpu,mem,cpu->r[0],cpu->r[1],cpu->r[4],cpu->r[peak_reg],&s,&d,&g,&p)) return 0;
    uint32_t gain=m8_le_read(g,4),peak=m8_le_read(p,4),before=0,last=0;
#if defined(M8_NATIVE_FP_NO_STICKY)
    if(m8_mx_audio_domain(gain,peak) && (s==d ||
       m8_filter_disjoint((uintptr_t)s,260u,(uintptr_t)d,260u)) &&
       m8_mx_audio_words(s) && m8_mx_audio_words(d)) {
        float factor=m8_fp_float(gain),maximum=m8_fp_float(peak);
        for(unsigned i=1;i<=64;i++) {
            last=m8_le_read(s+4u*i,4);
            float original=m8_fp_float(m8_le_read(d+4u*i,4));
            float product=m8_fp_float(last)*factor;
            float value=original+product;
            m8_le_write(d+4u*i,4,m8_fp_bits32(value));
            before=m8_fp_bits32(maximum);
            float magnitude=fabsf(product);
            if(magnitude>maximum) maximum=magnitude;
        }
        peak=m8_fp_bits32(maximum);
    } else
#endif
    {
    for(unsigned i=1;i<=64;i++) {
        last=m8_le_read(s+4u*i,4);
        uint32_t original=m8_le_read(d+4u*i,4);
        uint32_t product=m8_mxf_op(cpu,M8_FP_MUL,last,gain,0);
        uint32_t value=m8_mxf_op(cpu,M8_FP_ADD,original,product,0);
        m8_le_write(d+4u*i,4,value);
        before=peak;
        peak=m8_mxf_op(cpu,M8_FP_MAXNM,peak,product&0x7fffffffu,0);
    }
    }
    m8_le_write(p,4,peak);
    cpu->s[11]=last;cpu->s[13]=before;cpu->s[15]=peak;
    cpu->r[0]+=256u;cpu->r[1]+=256u;cpu->r[12]=64u;
    m8_fp_cmp32(cpu,cpu->s[12],0,0);
    cpu->xpsr=(cpu->xpsr&0x0fffffffu)|0x60000000u;
    return 1;
}
__attribute__((noinline)) int m8_mixer_fast_kernel(m8_cpu *output,m8_mem *mem,
        m8_aot_fn pool_2adbc,m8_aot_fn pool_2ae28,
        m8_aot_fn pool_2ae70,m8_aot_fn pool_2ae88) {
    if(!pool_2adbc || !pool_2ae28 || !pool_2ae70 || !pool_2ae88 || !m8_mx_qualify(output,mem)) return -1;
    m8_cpu state=*output,*cpu=&state;
    uintptr_t arena=(uintptr_t)mem->itcm.host;
#define M8_MX_HOST(addr) ((uint8_t *)(arena+(uintptr_t)(uint32_t)(addr)))
    goto L_73bc;
L_76bc:
    if(!m8_mx_scale(cpu,mem,0xc8u)) {cpu->r[15]=0x76bcu;goto done;}
    goto L_77fe;
L_7aa8:
    if(!m8_mx_scale(cpu,mem,0xe8u)) {cpu->r[15]=0x7aa8u;goto done;}
    goto L_7bea;
L_7f7c:
    if(!m8_mx_add(cpu,mem,7)) {cpu->r[15]=0x7f7cu;goto done;}
    goto L_80a6;
L_80b6:
    if(!m8_mx_add(cpu,mem,5)) {cpu->r[15]=0x80b6u;goto done;}
    goto L_83b4;
L_73bc: { /* 73bc: push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr} */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + 4294967260u;
uint32_t value0 = cpu->r[4];
uint32_t value1 = cpu->r[5];
uint32_t value2 = cpu->r[6];
uint32_t value3 = cpu->r[7];
uint32_t value4 = cpu->r[8];
uint32_t value5 = cpu->r[9];
uint32_t value6 = cpu->r[10];
uint32_t value7 = cpu->r[11];
uint32_t value8 = cpu->r[14];
m8_le_write(M8_MX_HOST(addr + 0u),4,value0);
m8_le_write(M8_MX_HOST(addr + 4u),4,value1);
m8_le_write(M8_MX_HOST(addr + 8u),4,value2);
m8_le_write(M8_MX_HOST(addr + 12u),4,value3);
m8_le_write(M8_MX_HOST(addr + 16u),4,value4);
m8_le_write(M8_MX_HOST(addr + 20u),4,value5);
m8_le_write(M8_MX_HOST(addr + 24u),4,value6);
m8_le_write(M8_MX_HOST(addr + 28u),4,value7);
m8_le_write(M8_MX_HOST(addr + 32u),4,value8);
cpu->r[13] = base - 36u;
}
}
goto L_73c0;
L_73c0: { /* 73c0: movs r4, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[4] = (uint32_t)(result);
}
}
goto L_73c2;
L_73c2: { /* 73c2: mov r8, r0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[0];
uint32_t result = b;
cpu->r[8] = (uint32_t)(result);
}
}
goto L_73c4;
L_73c4: { /* 73c4: vpush {d8} */
{
uint32_t fp_addr = (cpu->r[13] - 8u);
uint32_t fp_value;
m8_le_write(M8_MX_HOST(fp_addr + 0u),4,cpu->s[16]);
m8_le_write(M8_MX_HOST(fp_addr + 4u),4,cpu->s[17]);
cpu->r[13] -= 8u;
}
}
goto L_73c8;
L_73c8: { /* 73c8: mov sl, r4 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[4];
uint32_t result = b;
cpu->r[10] = (uint32_t)(result);
}
}
goto L_73ca;
L_73ca: { /* 73ca: sub sp, #0x14 */
{
uint32_t a = cpu->r[13];
uint32_t b = 0x14u;
uint32_t result = a - b - 0u;
cpu->r[13] = (uint32_t)(result);
}
}
goto L_73cc;
L_73cc: { /* 73cc: b #0x73e6 */
goto L_73e6;
}
L_73ce: { /* 73ce: mov r0, r5 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[5];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_73d0;
L_73d0: { /* 73d0: cbnz r6, #0x7406 */
if (cpu->r[6] != 0) {
goto L_7406;
}
}
goto L_73d2;
L_73d2: { /* 73d2: cbz r5, #0x73fe */
if (cpu->r[5] == 0) {
goto L_73fe;
}
}
goto L_73d4;
L_73d4: { /* 73d4: bl #0x2adbc */
cpu->r[14]=0x73d9u;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x73d8u) {*output=*cpu;return call_result;}
}
goto L_73d8;
L_73d8: { /* 73d8: add.w sl, sl, #1 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x1u;
uint32_t result = a + b + 0u;
cpu->r[10] = (uint32_t)(result);
}
}
goto L_73dc;
L_73dc: { /* 73dc: adds r4, #2 */
{
uint32_t a = cpu->r[4];
uint32_t b = 0x2u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
cpu->r[4] = (uint32_t)(result);
}
}
goto L_73de;
L_73de: { /* 73de: cmp.w sl, #8 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_73e2;
L_73e2: { /* 73e2: beq.w #0x85b4 */
if(m8_condition(cpu->xpsr,1)) {
goto L_85b4;
}
}
goto L_73e6;
L_73e6: { /* 73e6: mov r1, r4 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[4];
uint32_t result = b;
cpu->r[1] = (uint32_t)(result);
}
}
goto L_73e8;
L_73e8: { /* 73e8: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_73ea;
L_73ea: { /* 73ea: bl #0x2ae88 */
cpu->r[14]=0x73efu;
cpu->r[15]=0x2ae88u;
int call_result=pool_2ae88(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x73eeu) {*output=*cpu;return call_result;}
}
goto L_73ee;
L_73ee: { /* 73ee: mov r5, r0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[0];
uint32_t result = b;
cpu->r[5] = (uint32_t)(result);
}
}
goto L_73f0;
L_73f0: { /* 73f0: adds r1, r4, #1 */
{
uint32_t a = cpu->r[4];
uint32_t b = 0x1u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
cpu->r[1] = (uint32_t)(result);
}
}
goto L_73f2;
L_73f2: { /* 73f2: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_73f4;
L_73f4: { /* 73f4: bl #0x2ae88 */
cpu->r[14]=0x73f9u;
cpu->r[15]=0x2ae88u;
int call_result=pool_2ae88(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x73f8u) {*output=*cpu;return call_result;}
}
goto L_73f8;
L_73f8: { /* 73f8: mov r6, r0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[0];
uint32_t result = b;
cpu->r[6] = (uint32_t)(result);
}
}
goto L_73fa;
L_73fa: { /* 73fa: cmp r5, #0 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_73fc;
L_73fc: { /* 73fc: bne #0x73ce */
if(m8_condition(cpu->xpsr,2)) {
goto L_73ce;
}
}
goto L_73fe;
L_73fe: { /* 73fe: mov r0, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7400;
L_7400: { /* 7400: cmp r6, #0 */
{
uint32_t a = cpu->r[6];
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_7402;
L_7402: { /* 7402: bne #0x73d4 */
if(m8_condition(cpu->xpsr,2)) {
goto L_73d4;
}
}
goto L_7404;
L_7404: { /* 7404: b #0x73d8 */
goto L_73d8;
}
L_7406: { /* 7406: add.w r3, r8, sl, lsl #2 */
{
uint32_t a = cpu->r[8];
uint32_t b = m8_shift(cpu->r[10], 2, 2, NULL);
uint32_t result = a + b + 0u;
cpu->r[3] = (uint32_t)(result);
}
}
goto L_740a;
L_740a: { /* 740a: mov r2, r5 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[5];
uint32_t result = b;
cpu->r[2] = (uint32_t)(result);
}
}
goto L_740c;
L_740c: { /* 740c: str r5, [sp, #0xc] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (12u);
uint32_t value = cpu->r[5];
m8_le_write(M8_MX_HOST(addr),4,value);
}
}
goto L_740e;
L_740e: { /* 740e: add.w r5, sl, #0xa */
{
uint32_t a = cpu->r[10];
uint32_t b = 0xau;
uint32_t result = a + b + 0u;
cpu->r[5] = (uint32_t)(result);
}
}
goto L_7412;
L_7412: { /* 7412: vldr s14, [r3, #0x28] */
{
uint32_t fp_addr = (cpu->r[3] + 40u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[14] = fp_value;
}
}
goto L_7416;
L_7416: { /* 7416: movs r1, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[1] = (uint32_t)(result);
}
}
goto L_7418;
L_7418: { /* 7418: lsls r5, r5, #2 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t result = m8_shift(cpu->r[5], 2, 0x2u, &carry);
m8_logic_flags(cpu, result, carry);
cpu->r[5] = (uint32_t)(result);
}
}
goto L_741a;
L_741a: { /* 741a: vldr s13, [r3, #0x68] */
{
uint32_t fp_addr = (cpu->r[3] + 104u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[13] = fp_value;
}
}
goto L_741e;
L_741e: { /* 741e: movs r0, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7420;
L_7420: { /* 7420: b #0x76bc */
goto L_76bc;
}
L_77fe: { /* 77fe: vldr s14, [r3, #0x28] */
{
uint32_t fp_addr = (cpu->r[3] + 40u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[14] = fp_value;
}
}
goto L_7802;
L_7802: { /* 7802: mov r2, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[2] = (uint32_t)(result);
}
}
goto L_7804;
L_7804: { /* 7804: vldr s13, [r3, #0x68] */
{
uint32_t fp_addr = (cpu->r[3] + 104u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[13] = fp_value;
}
}
goto L_7808;
L_7808: { /* 7808: movs r1, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[1] = (uint32_t)(result);
}
}
goto L_780a;
L_780a: { /* 780a: movs r0, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[0] = (uint32_t)(result);
}
}
goto L_780c;
L_780c: { /* 780c: b #0x7aa8 */
goto L_7aa8;
}
L_7bea: { /* 7bea: add.w r3, r5, #0xa4 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0xa4u;
uint32_t result = a + b + 0u;
cpu->r[3] = (uint32_t)(result);
}
}
goto L_7bee;
L_7bee: { /* 7bee: add.w sb, r5, #4 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0x4u;
uint32_t result = a + b + 0u;
cpu->r[9] = (uint32_t)(result);
}
}
goto L_7bf2;
L_7bf2: { /* 7bf2: add.w r4, r5, #0x24 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0x24u;
uint32_t result = a + b + 0u;
cpu->r[4] = (uint32_t)(result);
}
}
goto L_7bf6;
L_7bf6: { /* 7bf6: add.w r2, r5, #0x44 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0x44u;
uint32_t result = a + b + 0u;
cpu->r[2] = (uint32_t)(result);
}
}
goto L_7bfa;
L_7bfa: { /* 7bfa: cmp.w sl, #7 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x7u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_7bfe;
L_7bfe: { /* 7bfe: add.w r5, r5, #0xc4 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0xc4u;
uint32_t result = a + b + 0u;
cpu->r[5] = (uint32_t)(result);
}
}
goto L_7c02;
L_7c02: { /* 7c02: add.w r7, r8, r3 */
{
uint32_t a = cpu->r[8];
uint32_t b = cpu->r[3];
uint32_t result = a + b + 0u;
cpu->r[7] = (uint32_t)(result);
}
}
goto L_7c06;
L_7c06: { /* 7c06: add sb, r8 */
{
uint32_t a = cpu->r[9];
uint32_t b = cpu->r[8];
uint32_t result = a + b + 0u;
cpu->r[9] = (uint32_t)(result);
}
}
goto L_7c08;
L_7c08: { /* 7c08: add r4, r8 */
{
uint32_t a = cpu->r[4];
uint32_t b = cpu->r[8];
uint32_t result = a + b + 0u;
cpu->r[4] = (uint32_t)(result);
}
}
goto L_7c0a;
L_7c0a: { /* 7c0a: add.w fp, r8, r2 */
{
uint32_t a = cpu->r[8];
uint32_t b = cpu->r[2];
uint32_t result = a + b + 0u;
cpu->r[11] = (uint32_t)(result);
}
}
goto L_7c0e;
L_7c0e: { /* 7c0e: add r5, r8 */
{
uint32_t a = cpu->r[5];
uint32_t b = cpu->r[8];
uint32_t result = a + b + 0u;
cpu->r[5] = (uint32_t)(result);
}
}
goto L_7c10;
L_7c10: { /* 7c10: add.w r3, sl, #1 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x1u;
uint32_t result = a + b + 0u;
cpu->r[3] = (uint32_t)(result);
}
}
goto L_7c14;
L_7c14: { /* 7c14: beq.w #0x8588 */
if(m8_condition(cpu->xpsr,1)) {
goto L_8588;
}
}
goto L_7c18;
L_7c18: { /* 7c18: vldr s16, [pc, #0x6c] */
{
uint32_t fp_addr = (0x7c1cu + 108u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[16] = fp_value;
}
}
goto L_7c1c;
L_7c1c: { /* 7c1c: mov sl, r3 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[3];
uint32_t result = b;
cpu->r[10] = (uint32_t)(result);
}
}
goto L_7c1e;
L_7c1e: { /* 7c1e: str r6, [sp, #4] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (4u);
uint32_t value = cpu->r[6];
m8_le_write(M8_MX_HOST(addr),4,value);
}
}
goto L_7c20;
L_7c20: { /* 7c20: b #0x7c4a */
goto L_7c4a;
}
L_7c22: { /* 7c22: cbnz r0, #0x7c74 */
if (cpu->r[0] != 0) {
goto L_7c74;
}
}
goto L_7c24;
L_7c24: { /* 7c24: cbz r3, #0x7c68 */
if (cpu->r[3] == 0) {
goto L_7c68;
}
}
goto L_7c26;
L_7c26: { /* 7c26: mov r0, r3 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[3];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7c28;
L_7c28: { /* 7c28: bl #0x2adbc */
cpu->r[14]=0x7c2du;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x7c2cu) {*output=*cpu;return call_result;}
}
goto L_7c2c;
L_7c2c: { /* 7c2c: add.w sl, sl, #1 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x1u;
uint32_t result = a + b + 0u;
cpu->r[10] = (uint32_t)(result);
}
}
goto L_7c30;
L_7c30: { /* 7c30: add.w sb, sb, #4 */
{
uint32_t a = cpu->r[9];
uint32_t b = 0x4u;
uint32_t result = a + b + 0u;
cpu->r[9] = (uint32_t)(result);
}
}
goto L_7c34;
L_7c34: { /* 7c34: adds r4, #4 */
{
uint32_t a = cpu->r[4];
uint32_t b = 0x4u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
cpu->r[4] = (uint32_t)(result);
}
}
goto L_7c36;
L_7c36: { /* 7c36: add.w fp, fp, #4 */
{
uint32_t a = cpu->r[11];
uint32_t b = 0x4u;
uint32_t result = a + b + 0u;
cpu->r[11] = (uint32_t)(result);
}
}
goto L_7c3a;
L_7c3a: { /* 7c3a: cmp.w sl, #8 */
{
uint32_t a = cpu->r[10];
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_7c3e;
L_7c3e: { /* 7c3e: add.w r7, r7, #4 */
{
uint32_t a = cpu->r[7];
uint32_t b = 0x4u;
uint32_t result = a + b + 0u;
cpu->r[7] = (uint32_t)(result);
}
}
goto L_7c42;
L_7c42: { /* 7c42: add.w r5, r5, #4 */
{
uint32_t a = cpu->r[5];
uint32_t b = 0x4u;
uint32_t result = a + b + 0u;
cpu->r[5] = (uint32_t)(result);
}
}
goto L_7c46;
L_7c46: { /* 7c46: beq.w #0x8586 */
if(m8_condition(cpu->xpsr,1)) {
goto L_8586;
}
}
goto L_7c4a;
L_7c4a: { /* 7c4a: lsl.w r6, sl, #1 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t result = m8_shift(cpu->r[10], 2, 0x1u, &carry);
cpu->r[6] = (uint32_t)(result);
}
}
goto L_7c4e;
L_7c4e: { /* 7c4e: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7c50;
L_7c50: { /* 7c50: mov r1, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[1] = (uint32_t)(result);
}
}
goto L_7c52;
L_7c52: { /* 7c52: bl #0x2ae70 */
cpu->r[14]=0x7c57u;
cpu->r[15]=0x2ae70u;
int call_result=pool_2ae70(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x7c56u) {*output=*cpu;return call_result;}
}
goto L_7c56;
L_7c56: { /* 7c56: adds r1, r6, #1 */
{
uint32_t a = cpu->r[6];
uint32_t b = 0x1u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
cpu->r[1] = (uint32_t)(result);
}
}
goto L_7c58;
L_7c58: { /* 7c58: str r0, [sp, #8] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (8u);
uint32_t value = cpu->r[0];
m8_le_write(M8_MX_HOST(addr),4,value);
}
}
goto L_7c5a;
L_7c5a: { /* 7c5a: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7c5c;
L_7c5c: { /* 7c5c: bl #0x2ae70 */
cpu->r[14]=0x7c61u;
cpu->r[15]=0x2ae70u;
int call_result=pool_2ae70(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x7c60u) {*output=*cpu;return call_result;}
}
goto L_7c60;
L_7c60: { /* 7c60: ldr r3, [sp, #8] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (8u);
uint32_t value;
value=m8_le_read(M8_MX_HOST(addr),4);
cpu->r[3] = (uint32_t)(value);
}
}
goto L_7c62;
L_7c62: { /* 7c62: mov r6, r0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[0];
uint32_t result = b;
cpu->r[6] = (uint32_t)(result);
}
}
goto L_7c64;
L_7c64: { /* 7c64: cmp r3, #0 */
{
uint32_t a = cpu->r[3];
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_7c66;
L_7c66: { /* 7c66: bne #0x7c22 */
if(m8_condition(cpu->xpsr,2)) {
goto L_7c22;
}
}
goto L_7c68;
L_7c68: { /* 7c68: cmp r6, #0 */
{
uint32_t a = cpu->r[6];
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_7c6a;
L_7c6a: { /* 7c6a: beq #0x7c2c */
if(m8_condition(cpu->xpsr,1)) {
goto L_7c2c;
}
}
goto L_7c6c;
L_7c6c: { /* 7c6c: mov r0, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7c6e;
L_7c6e: { /* 7c6e: bl #0x2adbc */
cpu->r[14]=0x7c73u;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x7c72u) {*output=*cpu;return call_result;}
}
goto L_7c72;
L_7c72: { /* 7c72: b #0x7c2c */
goto L_7c2c;
}
L_7c74: { /* 7c74: vldr s14, [sb] */
{
uint32_t fp_addr = (cpu->r[9] + 0u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[14] = fp_value;
}
}
goto L_7c78;
L_7c78: { /* 7c78: mov r0, r3 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[3];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_7c7a;
L_7c7a: { /* 7c7a: vldr s12, [fp] */
{
uint32_t fp_addr = (cpu->r[11] + 0u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[12] = fp_value;
}
}
goto L_7c7e;
L_7c7e: { /* 7c7e: mov.w ip, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
cpu->r[12] = (uint32_t)(result);
}
}
goto L_7c82;
L_7c82: { /* 7c82: ldr r1, [sp, #0xc] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (12u);
uint32_t value;
value=m8_le_read(M8_MX_HOST(addr),4);
cpu->r[1] = (uint32_t)(value);
}
}
goto L_7c84;
L_7c84: { /* 7c84: b #0x7f7c */
goto L_7f7c;
}
L_80a6: { /* 80a6: vldr s14, [sb] */
{
uint32_t fp_addr = (cpu->r[9] + 0u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[14] = fp_value;
}
}
goto L_80aa;
L_80aa: { /* 80aa: mov r0, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_80ac;
L_80ac: { /* 80ac: vldr s12, [fp] */
{
uint32_t fp_addr = (cpu->r[11] + 0u);
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[12] = fp_value;
}
}
goto L_80b0;
L_80b0: { /* 80b0: mov.w ip, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
cpu->r[12] = (uint32_t)(result);
}
}
goto L_80b4;
L_80b4: { /* 80b4: ldr r1, [sp, #4] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (4u);
uint32_t value;
value=m8_le_read(M8_MX_HOST(addr),4);
cpu->r[1] = (uint32_t)(value);
}
}
goto L_80b6;
L_83b4: { /* 83b4: mov r0, r3 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[3];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_83b6;
L_83b6: { /* 83b6: bl #0x2adbc */
cpu->r[14]=0x83bbu;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x83bau) {*output=*cpu;return call_result;}
}
goto L_83ba;
L_83ba: { /* 83ba: mov r0, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_83bc;
L_83bc: { /* 83bc: bl #0x2adbc */
cpu->r[14]=0x83c1u;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x83c0u) {*output=*cpu;return call_result;}
}
goto L_83c0;
L_83c0: { /* 83c0: b #0x7c2c */
goto L_7c2c;
}
L_8586: { /* 8586: ldr r6, [sp, #4] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (4u);
uint32_t value;
value=m8_le_read(M8_MX_HOST(addr),4);
cpu->r[6] = (uint32_t)(value);
}
}
goto L_8588;
L_8588: { /* 8588: ldr r4, [sp, #0xc] */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + (12u);
uint32_t value;
value=m8_le_read(M8_MX_HOST(addr),4);
cpu->r[4] = (uint32_t)(value);
}
}
goto L_858a;
L_858a: { /* 858a: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_858c;
L_858c: { /* 858c: movs r2, #0 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[2] = (uint32_t)(result);
}
}
goto L_858e;
L_858e: { /* 858e: mov r1, r4 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[4];
uint32_t result = b;
cpu->r[1] = (uint32_t)(result);
}
}
goto L_8590;
L_8590: { /* 8590: bl #0x2ae28 */
cpu->r[14]=0x8595u;
cpu->r[15]=0x2ae28u;
int call_result=pool_2ae28(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x8594u) {*output=*cpu;return call_result;}
}
goto L_8594;
L_8594: { /* 8594: movs r2, #1 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x1u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
cpu->r[2] = (uint32_t)(result);
}
}
goto L_8596;
L_8596: { /* 8596: mov r1, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[1] = (uint32_t)(result);
}
}
goto L_8598;
L_8598: { /* 8598: mov r0, r8 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[8];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_859a;
L_859a: { /* 859a: bl #0x2ae28 */
cpu->r[14]=0x859fu;
cpu->r[15]=0x2ae28u;
int call_result=pool_2ae28(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x859eu) {*output=*cpu;return call_result;}
}
goto L_859e;
L_859e: { /* 859e: mov r0, r4 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[4];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_85a0;
L_85a0: { /* 85a0: bl #0x2adbc */
cpu->r[14]=0x85a5u;
cpu->r[15]=0x2adbcu;
int call_result=pool_2adbc(cpu,mem);
if (call_result!=M8_AOT_EXIT || cpu->r[15]!=0x85a4u) {*output=*cpu;return call_result;}
}
goto L_85a4;
L_85a4: { /* 85a4: mov r0, r6 */
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = cpu->r[6];
uint32_t result = b;
cpu->r[0] = (uint32_t)(result);
}
}
goto L_85a6;
L_85a6: { /* 85a6: add sp, #0x14 */
{
uint32_t a = cpu->r[13];
uint32_t b = 0x14u;
uint32_t result = a + b + 0u;
cpu->r[13] = (uint32_t)(result);
}
}
goto L_85a8;
L_85a8: { /* 85a8: vpop {d8} */
{
uint32_t fp_addr = cpu->r[13];
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[16] = fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 4u),4);
cpu->s[17] = fp_value;
cpu->r[13] += 8u;
}
}
goto L_85ac;
L_85ac: { /* 85ac: pop.w {r4, r5, r6, r7, r8, sb, sl, fp, lr} */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + 0u;
uint32_t value0;
value0=m8_le_read(M8_MX_HOST(addr + 0u),4);
uint32_t value1;
value1=m8_le_read(M8_MX_HOST(addr + 4u),4);
uint32_t value2;
value2=m8_le_read(M8_MX_HOST(addr + 8u),4);
uint32_t value3;
value3=m8_le_read(M8_MX_HOST(addr + 12u),4);
uint32_t value4;
value4=m8_le_read(M8_MX_HOST(addr + 16u),4);
uint32_t value5;
value5=m8_le_read(M8_MX_HOST(addr + 20u),4);
uint32_t value6;
value6=m8_le_read(M8_MX_HOST(addr + 24u),4);
uint32_t value7;
value7=m8_le_read(M8_MX_HOST(addr + 28u),4);
uint32_t value8;
value8=m8_le_read(M8_MX_HOST(addr + 32u),4);
cpu->r[13] = base + 36u;
cpu->r[4] = (uint32_t)(value0);
cpu->r[5] = (uint32_t)(value1);
cpu->r[6] = (uint32_t)(value2);
cpu->r[7] = (uint32_t)(value3);
cpu->r[8] = (uint32_t)(value4);
cpu->r[9] = (uint32_t)(value5);
cpu->r[10] = (uint32_t)(value6);
cpu->r[11] = (uint32_t)(value7);
cpu->r[14] = (uint32_t)(value8);
}
}
goto L_85b0;
L_85b0: { /* 85b0: b.w #0x2adbc */
cpu->r[15]=0x2adbcu;
{int status=pool_2adbc(cpu,mem);*output=*cpu;return status;}
}
L_85b4: { /* 85b4: add sp, #0x14 */
{
uint32_t a = cpu->r[13];
uint32_t b = 0x14u;
uint32_t result = a + b + 0u;
cpu->r[13] = (uint32_t)(result);
}
}
goto L_85b6;
L_85b6: { /* 85b6: vpop {d8} */
{
uint32_t fp_addr = cpu->r[13];
uint32_t fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 0u),4);
cpu->s[16] = fp_value;
fp_value=m8_le_read(M8_MX_HOST(fp_addr + 4u),4);
cpu->s[17] = fp_value;
cpu->r[13] += 8u;
}
}
goto L_85ba;
L_85ba: { /* 85ba: pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc} */
{
uint32_t base = cpu->r[13];
uint32_t addr = base + 0u;
uint32_t value0;
value0=m8_le_read(M8_MX_HOST(addr + 0u),4);
uint32_t value1;
value1=m8_le_read(M8_MX_HOST(addr + 4u),4);
uint32_t value2;
value2=m8_le_read(M8_MX_HOST(addr + 8u),4);
uint32_t value3;
value3=m8_le_read(M8_MX_HOST(addr + 12u),4);
uint32_t value4;
value4=m8_le_read(M8_MX_HOST(addr + 16u),4);
uint32_t value5;
value5=m8_le_read(M8_MX_HOST(addr + 20u),4);
uint32_t value6;
value6=m8_le_read(M8_MX_HOST(addr + 24u),4);
uint32_t value7;
value7=m8_le_read(M8_MX_HOST(addr + 28u),4);
uint32_t value8;
value8=m8_le_read(M8_MX_HOST(addr + 32u),4);
cpu->r[13] = base + 36u;
cpu->r[4] = (uint32_t)(value0);
cpu->r[5] = (uint32_t)(value1);
cpu->r[6] = (uint32_t)(value2);
cpu->r[7] = (uint32_t)(value3);
cpu->r[8] = (uint32_t)(value4);
cpu->r[9] = (uint32_t)(value5);
cpu->r[10] = (uint32_t)(value6);
cpu->r[11] = (uint32_t)(value7);
cpu->r[15] = (value8) & ~1u;
goto done;
}
}done:
    *output=*cpu;
    return M8_AOT_EXIT;
#undef M8_MX_HOST
}
#endif
#endif
#endif
