/* Headless 6.5.2C mode-0 FM operator: 0x12174..0x121ee, then
 * 0x1232c..0x123f4. The caller configures native FP before this kernel.
 * Success performs the complete return through LR. Rejection has no effects.
 */
#ifndef M8_AOT_FM_OPERATOR_H
#define M8_AOT_FM_OPERATOR_H
#include "runtime.h"
#include "fp.h"
#if defined(M8_AOT_NATIVE_FP) && defined(M8_AOT_FAST_MEMORY) && \
    !defined(M8_AOT_DIAGNOSTIC) && !defined(M8_AOT_GUEST_COUNT) && \
    !defined(M8_AOT_DISABLE_FM_OPERATOR)
#if defined(__GNUC__) || defined(__clang__)
#define M8_FM_INLINE static inline __attribute__((always_inline))
#define M8_FM_NOINLINE __attribute__((noinline))
#else
#define M8_FM_INLINE static inline
#define M8_FM_NOINLINE
#endif
M8_FM_INLINE int m8_fm_overlap(const uint8_t *a,unsigned na,
                             const uint8_t *b,unsigned nb) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<na : x-y<nb;
}
M8_FM_INLINE int m8_fm_number(uint32_t bits,uint32_t maximum) {
    uint32_t magnitude=bits & 0x7fffffffu;
    return !magnitude || (magnitude>=0x37800000u && magnitude<=maximum);
}
M8_FM_INLINE float m8_fm_load(const uint8_t *p) {
    return m8_fp_float(m8_le_read(p,4));
}
M8_FM_INLINE void m8_fm_store_bits(uint8_t *p,uint32_t bits) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(p,&bits,4);
#else
    m8_le_write(p,4,bits);
#endif
}
M8_FM_INLINE void m8_fm_store(uint8_t *p,float value) {
    m8_fm_store_bits(p,m8_fp_bits32(value));
}
/* Integer vector guards use the same C source on x86-64 and AArch64.
 * Compilers without this extension use the scalar form below. No floating
 * operation occurs before the complete preflight succeeds.
 */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(M8_AOT_FM_SCALAR_GUARDS) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
typedef uint32_t m8_fm_u4 __attribute__((vector_size(16)));
M8_FM_INLINE m8_fm_u4 m8_fm_load4(const uint8_t *p) {
    m8_fm_u4 value; memcpy(&value,p,16); return value;
}
M8_FM_INLINE m8_fm_u4 m8_fm_bad_numbers(m8_fm_u4 value,m8_fm_u4 maximum) {
    const m8_fm_u4 mask={0x7fffffffu,0x7fffffffu,0x7fffffffu,0x7fffffffu};
    const m8_fm_u4 minimum={0x37800000u,0x37800000u,0x37800000u,0x37800000u};
    const m8_fm_u4 zero={0,0,0,0};
    m8_fm_u4 magnitude=value & mask;
    return (m8_fm_u4)((magnitude!=zero) & ((magnitude-minimum)>(maximum-minimum)));
}
M8_FM_INLINE int m8_fm_literals(const uint8_t *p) {
    const m8_fm_u4 first={0x42710241u,0x3edf7d0fu,0x3f876d33u,0x3f6cc869u};
    const m8_fm_u4 last={0x3f6cc869u,0x4003b69au,0x3ed990d1u,0x3f9b95d5u};
    m8_fm_u4 bad=(m8_fm_load4(p)^first)|(m8_fm_load4(p+12)^last);
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
}
M8_FM_INLINE int m8_fm_numbers(const uint8_t *object,const uint8_t *ports,
                             const uint8_t *pm) {
    const m8_fm_u4 maximum={0x44800000u,0x44800000u,0x44800000u,0x44800000u};
    const m8_fm_u4 phase_max={0x44800000u,0x4b800000u,0x44800000u,0x44800000u};
    m8_fm_u4 extra={m8_le_read(ports,4),m8_le_read(pm,4),0,0};
    /* Also qualify the unused default PM field when PM points elsewhere. */
    m8_fm_u4 bad=m8_fm_bad_numbers(m8_fm_load4(object+4),phase_max)|
                 m8_fm_bad_numbers(m8_fm_load4(ports+8),maximum)|
                 m8_fm_bad_numbers(extra,maximum);
    return !(bad[0]|bad[1]|bad[2]|bad[3]);
}
#else
M8_FM_INLINE int m8_fm_literals(const uint8_t *p) {
    return m8_le_read(p,4)==0x42710241u && m8_le_read(p+4,4)==0x3edf7d0fu &&
        m8_le_read(p+8,4)==0x3f876d33u && m8_le_read(p+12,4)==0x3f6cc869u &&
        m8_le_read(p+16,4)==0x4003b69au && m8_le_read(p+20,4)==0x3ed990d1u &&
        m8_le_read(p+24,4)==0x3f9b95d5u;
}
M8_FM_INLINE int m8_fm_numbers(const uint8_t *object,const uint8_t *ports,
                             const uint8_t *pm) {
    return m8_fm_number(m8_le_read(object+4,4),0x44800000u) &&
        m8_fm_number(m8_le_read(object+8,4),0x4b800000u) &&
        m8_fm_number(m8_le_read(object+12,4),0x44800000u) &&
        m8_fm_number(m8_le_read(object+16,4),0x44800000u) &&
        m8_fm_number(m8_le_read(ports,4),0x44800000u) &&
        m8_fm_number(m8_le_read(ports+8,4),0x44800000u) &&
        m8_fm_number(m8_le_read(ports+12,4),0x44800000u) &&
        m8_fm_number(m8_le_read(ports+16,4),0x44800000u) &&
        m8_fm_number(m8_le_read(ports+20,4),0x44800000u) &&
        m8_fm_number(m8_le_read(pm,4),0x44800000u);
}
#endif
M8_FM_NOINLINE static int m8_fm_operator_kernel(m8_cpu *restrict cpu,m8_mem *restrict mem) {
    uint32_t object_address=cpu->r[0],sp=cpu->r[13];
    if ((cpu->fpscr & 0x03c00010u)!=0x03000010u ||
        (cpu->xpsr & 0x0700fc00u)!=0x01000000u ||
        !(cpu->r[14] & 1u) || cpu->r[14]>=0xfffffff0u ||
        sp<24u || object_address>UINT32_MAX-75u ||
        mem->dtcm.base!=0x20000000u || mem->dtcm.size>0x200000u ||
        mem->ocram.base!=0x20200000u || mem->ocram.size>0x200000u ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(mem->ocram.host,mem->ocram.size,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,(const uint8_t *)mem,sizeof(*mem)) ||
        m8_fm_overlap(mem->ocram.host,mem->ocram.size,(const uint8_t *)mem,sizeof(*mem))) return 0;
    uint8_t *restrict object=m8_region_pointer(&mem->dtcm,object_address,76);
    uint8_t *restrict stack=m8_region_pointer(&mem->dtcm,sp-12u,12);
    if (!object || !stack || object[0x3a]) return 0;
    uint32_t base=m8_le_read(object+0x1c,4);
    uint32_t pm_address=m8_le_read(object+0x44,4);
    if (base>UINT32_MAX-23u || pm_address>UINT32_MAX-3u ||
        ((object_address|base|pm_address|sp) & 3u) ||
        m8_le_read(object,4)!=base+8u ||
        m8_le_read(object+0x28,4)!=base+12u ||
        m8_le_read(object+0x2c,4)!=base+16u ||
        m8_le_read(object+0x30,4)!=base+20u ||
        m8_le_read(object+0x48,4)!=object_address+16u) return 0;
    uint8_t *restrict ports=m8_region_pointer(&mem->ocram,base,24);
    uint8_t *pm=pm_address==object_address+12u ? object+12 :
               m8_region_pointer(&mem->ocram,pm_address,4);
    uint8_t *constants=m8_region_pointer(&mem->itcm,0x124a8u,28);
    if (!ports || !pm || !constants ||
        m8_fm_overlap(mem->dtcm.host,mem->dtcm.size,
                      mem->ocram.host,mem->ocram.size) ||
        m8_fm_overlap(constants,28,mem->dtcm.host,mem->dtcm.size) ||
        m8_fm_overlap(constants,28,mem->ocram.host,mem->ocram.size) ||
        m8_fm_overlap(constants,28,(const uint8_t *)cpu,sizeof(*cpu)) ||
        m8_fm_overlap(constants,28,(const uint8_t *)mem,sizeof(*mem)) ||
        (uint32_t)(sp-object_address-1u)<=86u ||
        (pm_address!=object_address+12u &&
         (uint32_t)(pm_address-base+3u)<=26u)) return 0;
    /* Guest ITCM can be writable. Do not assume these literals are immutable. */
    if (!m8_fm_literals(constants)) return 0;
    /* Nonzero inputs lie in [2^-16,2^10]. The unwrapped phase may reach
     * 2^24. Thus all three round-zero conversions remain within int32,
     * and polynomial/multiply results cannot approach the FZ boundary.
     * Sticky IXC permits deferred hardware flag collection at the caller.
     */
    if (!m8_fm_numbers(object,ports,pm)) return 0;
    float step=m8_fm_load(ports),phase=m8_fm_load(object+4);
    float total=m8_fm_load(object+8),s14=0.5f;
    float s15=phase+step;                       /* 0x12188 */
    m8_fm_store_bits(stack,cpu->r[4]);            /* original PUSH residue */
    m8_fm_store_bits(stack+4,cpu->r[5]);
    m8_fm_store_bits(stack+8,cpu->r[6]);
    float s13=s15+s14;
    m8_fm_store(object+4,s15);
    int32_t rounded=(int32_t)s13;               /* 0x1219e */
    float s12=total+step;
    s13=(float)rounded;
    m8_fm_store(object+8,s12);
    s15=s15-s13;
    m8_fm_store(object+4,s15);
    s13=m8_fm_load(pm);
    s15=s15+s13;
    s14=s15<0.0f ? s15-s14 : s15+s14;
    s14=(float)(int32_t)s14;
    s15=s15-s14;
    s14=m8_fm_load(ports+8);
    m8_fm_store_bits(ports+8,0);                 /* 0x121e2 */
    float s11=0.5f,s7=m8_fm_load(constants+8);
    s13=m8_fm_load(ports+20);
    s12=m8_fm_load(constants+12);
    s14=s14*s13;
    float s9=m8_fm_load(constants+4);
    float s8=m8_fm_load(constants+16);
    float s10=m8_fm_load(constants+24);
    s15=s14+s15;
    m8_fm_store(ports+20,s14);
    s14=0.5f;
    uint32_t compare=s15==0.0f ? 0x60000000u :
                     s15<0.0f ? 0x80000000u : 0x20000000u;
    s14=s15<0.0f ? s15-s14 : s15+s14;
    s14=(float)(int32_t)s14;
    s15=s15-s14;
    s14=m8_fm_load(constants);
    s13=fabsf(s15);
    s15=s15*s14;
    s14=m8_fm_load(constants+20);
    s11=s13-s11;
    s7=s13+s7;
    s12=s13-s12;
    s15=s11*s15;
    s9=fmaf(s7,s13,s9);
    s8=s13-s8;
    s14=s13+s14;
    s11=s10;
    s15=s12*s15;
    s11=fmaf(s8,s13,s11);
    s15=s14*s15;
    s15=s15*s9;
    s11=s15*s11;
    m8_fm_store(ports+8,s11);
    float s0=s11*m8_fm_load(ports+12);
    m8_fm_store(ports+8,s0);
    s15=m8_fm_load(ports+16);
    s0=s0*s15;
    m8_fm_store(ports+8,s0);
    s15=m8_fm_load(object+16);
    s0=s0+s15;
    m8_fm_store(ports+8,s0);
    object[0x38]=1;
    cpu->r[1]=base+12u; cpu->r[2]=object_address+16u; cpu->r[3]=1;
    cpu->s[0]=m8_fp_bits32(s0);
    cpu->s[7]=m8_fp_bits32(s7); cpu->s[8]=m8_fp_bits32(s8);
    cpu->s[9]=m8_fp_bits32(s9); cpu->s[10]=m8_fp_bits32(s10);
    cpu->s[11]=m8_fp_bits32(s11); cpu->s[12]=m8_fp_bits32(s12);
    cpu->s[13]=m8_fp_bits32(s13); cpu->s[14]=m8_fp_bits32(s14);
    cpu->s[15]=m8_fp_bits32(s15);
    cpu->fpscr=(cpu->fpscr & 0x0fffffffu)|compare;
    /* MOVS r3,#1 clears N/Z and preserves the comparison's C/V. */
    cpu->xpsr=(cpu->xpsr & 0x0fffffffu)|(compare & 0x30000000u);
    cpu->r[15]=cpu->r[14] & ~1u;
    return 1;
}
#undef M8_FM_INLINE
#undef M8_FM_NOINLINE
#else
static inline int m8_fm_operator_kernel(m8_cpu *cpu,m8_mem *mem) {
    (void)cpu; (void)mem; return 0;
}
#endif
#endif
