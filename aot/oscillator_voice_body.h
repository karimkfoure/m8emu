/* Original Thumb body SHA-256 4a113ff57403f2c465629bf95c49a2c3826256ec6dc70b5f284c4ffeea989124. Regenerate with generate_oscillator_voice.py. */
#define M8_OV_CHILD_RETURN(status) do {*output=*cpu;return (status);} while(0)
#define M8_OV_CHILD_PC() (cpu->r[15])
#if defined(M8_AOT_OSCILLATOR_SPANS)
#if defined(M8_OSCILLATOR_SPAN_VERIFY)
#define M8_OV_CACHED(span,address) M8_OSCILLATOR_SPAN_VERIFY(((uint8_t *)((span)+(uint32_t)(address))),M8_OV_HOST(address),__LINE__)
#else
#define M8_OV_CACHED(span,address) ((uint8_t *)((span)+(uint32_t)(address)))
#endif
#define M8_OV_OBJECT(address) M8_OV_CACHED(span_object,address)
#define M8_OV_STACK(address) M8_OV_CACHED(span_stack,address)
#define M8_OV_LEFT(address) M8_OV_CACHED(span_left,address)
#define M8_OV_RIGHT(address) M8_OV_CACHED(span_right,address)
#define M8_OV_CONTROL(address) M8_OV_CACHED(span_control,address)
#define M8_OV_WAVE(address) M8_OV_CACHED(span_wave,address)
#define M8_OV_AUXILIARY(address) M8_OV_CACHED(span_auxiliary,address)
#define M8_OV_RING(address) M8_OV_CACHED(span_ring,address)
#define M8_OV_METHOD_SLOT(address) M8_OV_CACHED(span_method_slot,address)
#define M8_OV_METHOD_TARGET(address) M8_OV_CACHED(span_method_target,address)
#else
#define M8_OV_OBJECT(address) M8_OV_HOST(address)
#define M8_OV_STACK(address) M8_OV_HOST(address)
#define M8_OV_LEFT(address) M8_OV_HOST(address)
#define M8_OV_RIGHT(address) M8_OV_HOST(address)
#define M8_OV_CONTROL(address) M8_OV_HOST(address)
#define M8_OV_WAVE(address) M8_OV_HOST(address)
#define M8_OV_AUXILIARY(address) M8_OV_HOST(address)
#define M8_OV_RING(address) M8_OV_HOST(address)
#define M8_OV_METHOD_SLOT(address) M8_OV_HOST(address)
#define M8_OV_METHOD_TARGET(address) M8_OV_HOST(address)
#endif
#define M8_OV_HOST(address) m8_oscillator_voice_host(arena,(uint32_t)(address))
#define M8_OV_SAVE(dst) do { (dst)->r[0]=r_0; (dst)->r[1]=r_1; (dst)->r[2]=r_2; (dst)->r[3]=r_3; (dst)->r[4]=r_4; (dst)->r[5]=r_5; (dst)->r[6]=r_6; (dst)->r[7]=r_7; (dst)->r[8]=r_8; (dst)->r[9]=r_9; (dst)->r[10]=r_10; (dst)->r[11]=r_11; (dst)->r[12]=r_12; (dst)->r[13]=r_13; (dst)->r[14]=r_14; (dst)->r[15]=r_15; (dst)->s[0]=m8_fp_bits32(s_0); (dst)->s[1]=m8_fp_bits32(s_1); (dst)->s[2]=m8_fp_bits32(s_2); (dst)->s[3]=m8_fp_bits32(s_3); (dst)->s[4]=m8_fp_bits32(s_4); (dst)->s[5]=m8_fp_bits32(s_5); (dst)->s[6]=m8_fp_bits32(s_6); (dst)->s[7]=m8_fp_bits32(s_7); (dst)->s[8]=m8_fp_bits32(s_8); (dst)->s[9]=m8_fp_bits32(s_9); (dst)->s[10]=m8_fp_bits32(s_10); (dst)->s[11]=m8_fp_bits32(s_11); (dst)->s[12]=m8_fp_bits32(s_12); (dst)->s[13]=m8_fp_bits32(s_13); (dst)->s[14]=m8_fp_bits32(s_14); (dst)->s[15]=m8_fp_bits32(s_15); (dst)->s[16]=m8_fp_bits32(s_16); (dst)->s[17]=m8_fp_bits32(s_17); (dst)->s[18]=m8_fp_bits32(s_18); (dst)->s[19]=m8_fp_bits32(s_19); (dst)->s[20]=m8_fp_bits32(s_20); (dst)->s[21]=m8_fp_bits32(s_21); (dst)->s[22]=m8_fp_bits32(s_22); (dst)->s[23]=m8_fp_bits32(s_23); (dst)->s[24]=m8_fp_bits32(s_24); (dst)->s[25]=m8_fp_bits32(s_25); (dst)->s[26]=m8_fp_bits32(s_26); (dst)->s[27]=m8_fp_bits32(s_27); (dst)->s[28]=m8_fp_bits32(s_28); (dst)->s[29]=m8_fp_bits32(s_29); (dst)->s[30]=m8_fp_bits32(s_30); (dst)->s[31]=m8_fp_bits32(s_31); (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr; } while(0)
#define M8_OV_LOAD(src) do { r_0=(src)->r[0]; r_1=(src)->r[1]; r_2=(src)->r[2]; r_3=(src)->r[3]; r_4=(src)->r[4]; r_5=(src)->r[5]; r_6=(src)->r[6]; r_7=(src)->r[7]; r_8=(src)->r[8]; r_9=(src)->r[9]; r_10=(src)->r[10]; r_11=(src)->r[11]; r_12=(src)->r[12]; r_13=(src)->r[13]; r_14=(src)->r[14]; r_15=(src)->r[15]; s_0=m8_fp_float((src)->s[0]); s_1=m8_fp_float((src)->s[1]); s_2=m8_fp_float((src)->s[2]); s_3=m8_fp_float((src)->s[3]); s_4=m8_fp_float((src)->s[4]); s_5=m8_fp_float((src)->s[5]); s_6=m8_fp_float((src)->s[6]); s_7=m8_fp_float((src)->s[7]); s_8=m8_fp_float((src)->s[8]); s_9=m8_fp_float((src)->s[9]); s_10=m8_fp_float((src)->s[10]); s_11=m8_fp_float((src)->s[11]); s_12=m8_fp_float((src)->s[12]); s_13=m8_fp_float((src)->s[13]); s_14=m8_fp_float((src)->s[14]); s_15=m8_fp_float((src)->s[15]); s_16=m8_fp_float((src)->s[16]); s_17=m8_fp_float((src)->s[17]); s_18=m8_fp_float((src)->s[18]); s_19=m8_fp_float((src)->s[19]); s_20=m8_fp_float((src)->s[20]); s_21=m8_fp_float((src)->s[21]); s_22=m8_fp_float((src)->s[22]); s_23=m8_fp_float((src)->s[23]); s_24=m8_fp_float((src)->s[24]); s_25=m8_fp_float((src)->s[25]); s_26=m8_fp_float((src)->s[26]); s_27=m8_fp_float((src)->s[27]); s_28=m8_fp_float((src)->s[28]); s_29=m8_fp_float((src)->s[29]); s_30=m8_fp_float((src)->s[30]); s_31=m8_fp_float((src)->s[31]); cpu->xpsr=(src)->xpsr; cpu->fpscr=(src)->fpscr; } while(0)
#ifdef M8_AOT_NARROW_CALLS
#define M8_OV_PUBLISH(dst) do { (dst)->r[0]=r_0; (dst)->r[1]=r_1; (dst)->r[2]=r_2; (dst)->r[3]=r_3; (dst)->r[4]=r_4; (dst)->r[5]=r_5; (dst)->r[6]=r_6; (dst)->r[7]=r_7; (dst)->r[8]=r_8; (dst)->r[9]=r_9; (dst)->r[10]=r_10; (dst)->r[11]=r_11; (dst)->r[12]=r_12; (dst)->r[13]=r_13; (dst)->r[14]=r_14; (dst)->r[15]=r_15; (dst)->s[0]=m8_fp_bits32(s_0); (dst)->s[1]=m8_fp_bits32(s_1); (dst)->s[2]=m8_fp_bits32(s_2); (dst)->s[3]=m8_fp_bits32(s_3); (dst)->s[4]=m8_fp_bits32(s_4); (dst)->s[5]=m8_fp_bits32(s_5); (dst)->s[6]=m8_fp_bits32(s_6); (dst)->s[7]=m8_fp_bits32(s_7); (dst)->s[8]=m8_fp_bits32(s_8); (dst)->s[9]=m8_fp_bits32(s_9); (dst)->s[10]=m8_fp_bits32(s_10); (dst)->s[11]=m8_fp_bits32(s_11); (dst)->s[12]=m8_fp_bits32(s_12); (dst)->s[13]=m8_fp_bits32(s_13); (dst)->s[14]=m8_fp_bits32(s_14); (dst)->s[15]=m8_fp_bits32(s_15); (dst)->s[16]=m8_fp_bits32(s_16); (dst)->s[17]=m8_fp_bits32(s_17); (dst)->s[18]=m8_fp_bits32(s_18); (dst)->s[19]=m8_fp_bits32(s_19); (dst)->s[20]=m8_fp_bits32(s_20); (dst)->s[21]=m8_fp_bits32(s_21); (dst)->s[22]=m8_fp_bits32(s_22); (dst)->s[23]=m8_fp_bits32(s_23); (dst)->s[24]=m8_fp_bits32(s_24); (dst)->s[25]=m8_fp_bits32(s_25); (dst)->s[26]=m8_fp_bits32(s_26); (dst)->s[27]=m8_fp_bits32(s_27); (dst)->s[28]=m8_fp_bits32(s_28); (dst)->s[29]=m8_fp_bits32(s_29); (dst)->s[30]=m8_fp_bits32(s_30); (dst)->s[31]=m8_fp_bits32(s_31); (dst)->xpsr=cpu->xpsr; (dst)->fpscr=cpu->fpscr; } while(0)
#else
#define M8_OV_PUBLISH(dst) M8_OV_SAVE(dst)
#endif
static M8_OV_NOINLINE int m8_oscillator_voice_kernel(m8_cpu *restrict output,m8_mem *restrict mem,m8_aot_fn update,m8_aot_fn configure) {
uint8_t *entry_object=m8_pointer(mem,output->r[0],0xcau);
/* Admit only method-5 voices. Direct-method voices (<=3) keep the translated
 * path: its inner render kernel already covers the loop, while the
 * whole-voice block costs more per call on those shapes (measured
 * regressions of 1.0-1.5% when admitted). Method 4
 * admits also measure slower (+0.4%); only method 5 shows wins, so
 * unmeasured shapes stay on the translated path. The byte is already fetched
 * above, so declined voices skip the span checks below. */
if(!entry_object || entry_object[0x41]!=5u || output->r[15]!=0x20628u || output->r[3]<=1u || !update || !configure ||
   output->r[13]<96u || !m8_oscillator_voice_regions(output,sizeof(*output),mem) ||
   !m8_oscillator_voice_qualify(output,mem,output->r[0],output->r[1],output->r[2],
       output->r[3],output->r[13]-96u,m8_le_read(entry_object+0x54,4)))return -1;
uintptr_t arena=m8_oscillator_voice_memory_offset(mem);
uint32_t root=output->r[0],frame=output->r[13]-96u,left=output->r[1],right=output->r[2];
uint32_t left_end=left+output->r[3]*4u;

#if defined(M8_AOT_OSCILLATOR_SPANS)
uintptr_t span_object=(uintptr_t)entry_object-root;
uintptr_t span_stack=(uintptr_t)M8_OV_HOST(frame)-frame;
uintptr_t span_left=(uintptr_t)M8_OV_HOST(left)-left;
uintptr_t span_right=(uintptr_t)M8_OV_HOST(right)-right;
uintptr_t span_control,span_wave,span_auxiliary,span_ring=0;
uintptr_t span_method_slot=0,span_method_target=0;
#define M8_OV_REFRESH_SPANS(wave_address) do { \
    uint32_t address=m8_le_read(entry_object+0x64,4); \
    span_control=(uintptr_t)M8_OV_HOST(address)-address; \
    address=(uint32_t)(wave_address); \
    span_wave=(uintptr_t)M8_OV_HOST(address)-address; \
    address=m8_le_read(entry_object+0x68,4); \
    span_auxiliary=(uintptr_t)M8_OV_HOST(address)-address; \
    address=m8_le_read(entry_object+0x58,4); \
    span_ring=(uintptr_t)M8_OV_HOST(address)-address; \
    if(entry_object[0x41]>3u) { \
        address=m8_le_read(entry_object+0x50,4); \
        span_method_slot=(uintptr_t)M8_OV_HOST(address)-address; \
        address=m8_le_read(M8_OV_HOST(address),4); \
        span_method_target=(uintptr_t)M8_OV_HOST(address)-address; \
    } \
} while(0)
M8_OV_REFRESH_SPANS(m8_le_read(entry_object+0x54,4));
#endif
#ifdef M8_AOT_NARROW_CALLS
m8_cpu flags=*output;
#else
m8_cpu flags;
#endif
m8_cpu *cpu=&flags;
if(!m8_oscillator_voice_regions(cpu,sizeof(*cpu),mem))return -1;
#ifdef M8_AOT_FAST_MEMORY
m8_mem ram=*mem;
#endif
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
cpu->xpsr=output->xpsr; cpu->fpscr=output->fpscr;

goto L_20628;
L_1fa4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fa4u)) {r_15=0x1fa4u;goto done;}
#endif
/* 1fa4: vmov.f32 s14, #-1.000000e+00 */
{
s_14=m8_fp_float(0xbf800000u);
}
goto L_1fa8;
L_1fa8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fa8u)) {r_15=0x1fa8u;goto done;}
#endif
/* 1fa8: vmov.f32 s15, #1.000000e+00 */
{
s_15=m8_fp_float(0x3f800000u);
}
goto L_1fac;
L_1fac:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1facu)) {r_15=0x1facu;goto done;}
#endif
/* 1fac: vmul.f32 s0, s0, s1 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_1), 0));
}
goto L_1fb0;
L_1fb0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fb0u)) {r_15=0x1fb0u;goto done;}
#endif
/* 1fb0: vmaxnm.f32 s0, s0, s14 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_0), m8_fp_bits32(s_14), 0));
}
goto L_1fb4;
L_1fb4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fb4u)) {r_15=0x1fb4u;goto done;}
#endif
/* 1fb4: vminnm.f32 s0, s0, s15 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MINNM, m8_fp_bits32(s_0), m8_fp_bits32(s_15), 0));
}
goto L_1fb8;
L_1fb8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fb8u)) {r_15=0x1fb8u;goto done;}
#endif
/* 1fb8: bx lr */
{
r_15=r_14&~1u;
goto method_return;
}
r_15=0x1fbau;goto done;
L_1fec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1fecu)) {r_15=0x1fecu;goto done;}
#endif
/* 1fec: vmov.f32 s15, #-1.500000e+01 */
{
s_15=m8_fp_float(0xc1700000u);
}
goto L_1ff0;
L_1ff0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1ff0u)) {r_15=0x1ff0u;goto done;}
#endif
/* 1ff0: vcmpe.f32 s1, s15 */
{
m8_oscillator_voice_cmp32(cpu, m8_fp_bits32(s_1), m8_fp_bits32(s_15), 1);
}
goto L_1ff4;
L_1ff4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1ff4u)) {r_15=0x1ff4u;goto done;}
#endif
/* 1ff4: vmrs apsr_nzcv, fpscr */
{
#ifndef M8_AOT_NATIVE_FP
m8_fp_sync(cpu);
#endif
cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);
}
goto L_1ff8;
L_1ff8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1ff8u)) {r_15=0x1ff8u;goto done;}
#endif
if(m8_condition(cpu->xpsr,5)) {
/* 1ff8: bmi #0x206a */
{
goto L_206a;
}
}
goto L_1ffa;
L_1ffa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1ffau)) {r_15=0x1ffau;goto done;}
#endif
/* 1ffa: vmov.f32 s13, #1.000000e+00 */
{
s_13=m8_fp_float(0x3f800000u);
}
goto L_1ffe;
L_1ffe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x1ffeu)) {r_15=0x1ffeu;goto done;}
#endif
/* 1ffe: vldr s12, [pc, #0x70] */
{
{
uint32_t fp_addr = (0x2000u + 112u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_12=m8_fp_float(fp_value);
}
}
goto L_2002;
L_2002:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2002u)) {r_15=0x2002u;goto done;}
#endif
/* 2002: vldr s14, [pc, #0x70] */
{
{
uint32_t fp_addr = (0x2004u + 112u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_2006;
L_2006:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2006u)) {r_15=0x2006u;goto done;}
#endif
/* 2006: vmov.f32 s15, s13 */
{
s_15=m8_fp_float(m8_fp_bits32(s_13));
}
goto L_200a;
L_200a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x200au)) {r_15=0x200au;goto done;}
#endif
/* 200a: vfma.f32 s15, s1, s12 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_1), m8_fp_bits32(s_12), m8_fp_bits32(s_15)));
}
goto L_200e;
L_200e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x200eu)) {r_15=0x200eu;goto done;}
#endif
/* 200e: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2012;
L_2012:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2012u)) {r_15=0x2012u;goto done;}
#endif
/* 2012: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2016;
L_2016:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2016u)) {r_15=0x2016u;goto done;}
#endif
/* 2016: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_201a;
L_201a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x201au)) {r_15=0x201au;goto done;}
#endif
/* 201a: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_201e;
L_201e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x201eu)) {r_15=0x201eu;goto done;}
#endif
/* 201e: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2022;
L_2022:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2022u)) {r_15=0x2022u;goto done;}
#endif
/* 2022: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2026;
L_2026:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2026u)) {r_15=0x2026u;goto done;}
#endif
/* 2026: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_202a;
L_202a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x202au)) {r_15=0x202au;goto done;}
#endif
/* 202a: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_202e;
L_202e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x202eu)) {r_15=0x202eu;goto done;}
#endif
/* 202e: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2032;
L_2032:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2032u)) {r_15=0x2032u;goto done;}
#endif
/* 2032: vmul.f32 s15, s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_2036;
L_2036:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2036u)) {r_15=0x2036u;goto done;}
#endif
/* 2036: vmul.f32 s0, s15, s0 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_0), 0));
}
goto L_203a;
L_203a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x203au)) {r_15=0x203au;goto done;}
#endif
/* 203a: vmul.f32 s15, s0, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_14), 0));
}
goto L_203e;
L_203e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x203eu)) {r_15=0x203eu;goto done;}
#endif
/* 203e: vmov r3, s0 */
{
r_3 = m8_fp_bits32(s_0);
}
goto L_2042;
L_2042:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2042u)) {r_15=0x2042u;goto done;}
#endif
/* 2042: and r3, r3, #0x80000000 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
carry = (0x80000000u) >> 31;
uint32_t a = r_3;
uint32_t b = 0x80000000u;
uint32_t result = a & b;
r_3 = (uint32_t)(result);
}
}
goto L_2046;
L_2046:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2046u)) {r_15=0x2046u;goto done;}
#endif
/* 2046: vabs.f32 s15, s15 */
{
s_15=m8_fp_float((m8_fp_bits32(s_15) & ~0x80000000u));
}
goto L_204a;
L_204a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x204au)) {r_15=0x204au;goto done;}
#endif
/* 204a: vmov.f32 s14, s15 */
{
s_14=m8_fp_float(m8_fp_bits32(s_15));
}
goto L_204e;
L_204e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x204eu)) {r_15=0x204eu;goto done;}
#endif
/* 204e: vadd.f32 s15, s15, s13 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));
}
goto L_2052;
L_2052:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2052u)) {r_15=0x2052u;goto done;}
#endif
/* 2052: vfma.f32 s14, s0, s0 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_0), m8_fp_bits32(s_0), m8_fp_bits32(s_14)));
}
goto L_2056;
L_2056:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2056u)) {r_15=0x2056u;goto done;}
#endif
/* 2056: vadd.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_205a;
L_205a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x205au)) {r_15=0x205au;goto done;}
#endif
/* 205a: vdiv.f32 s15, s14, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_DIV, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));
}
goto L_205e;
L_205e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x205eu)) {r_15=0x205eu;goto done;}
#endif
/* 205e: vmov r2, s15 */
{
r_2 = m8_fp_bits32(s_15);
}
goto L_2062;
L_2062:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2062u)) {r_15=0x2062u;goto done;}
#endif
/* 2062: orrs r3, r2 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t a = r_3;
uint32_t b = r_2;
uint32_t result = a | b;
m8_logic_flags(cpu, result, carry);
r_3 = (uint32_t)(result);
}
}
goto L_2064;
L_2064:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2064u)) {r_15=0x2064u;goto done;}
#endif
/* 2064: vmov s0, r3 */
{
s_0=m8_fp_float(r_3);
}
goto L_2068;
L_2068:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2068u)) {r_15=0x2068u;goto done;}
#endif
/* 2068: bx lr */
{
r_15=r_14&~1u;
goto method_return;
}
goto L_206a;
L_206a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206au)) {r_15=0x206au;goto done;}
#endif
/* 206a: vldr s0, [pc, #0xc] */
{
{
uint32_t fp_addr = (0x206cu + 12u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_0=m8_fp_float(fp_value);
}
}
goto L_206e;
L_206e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206eu)) {r_15=0x206eu;goto done;}
#endif
/* 206e: bx lr */
{
r_15=r_14&~1u;
goto method_return;
}
r_15=0x2070u;goto done;
L_20628:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20628u)) {r_15=0x20628u;goto done;}
#endif
/* 20628: push.w {r4, r5, r6, r7, r8, sb, sl, fp, lr} */
{
{
uint32_t base = r_13;
uint32_t addr = base + 4294967260u;
uint32_t value0 = r_4;
uint32_t value1 = r_5;
uint32_t value2 = r_6;
uint32_t value3 = r_7;
uint32_t value4 = r_8;
uint32_t value5 = r_9;
uint32_t value6 = r_10;
uint32_t value7 = r_11;
uint32_t value8 = r_14;
m8_le_write(M8_OV_STACK(addr + 0u),4,value0);
m8_le_write(M8_OV_STACK(addr + 4u),4,value1);
m8_le_write(M8_OV_STACK(addr + 8u),4,value2);
m8_le_write(M8_OV_STACK(addr + 12u),4,value3);
m8_le_write(M8_OV_STACK(addr + 16u),4,value4);
m8_le_write(M8_OV_STACK(addr + 20u),4,value5);
m8_le_write(M8_OV_STACK(addr + 24u),4,value6);
m8_le_write(M8_OV_STACK(addr + 28u),4,value7);
m8_le_write(M8_OV_STACK(addr + 32u),4,value8);
r_13 = base - 36u;
}
}
goto L_2062c;
L_2062c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2062cu)) {r_15=0x2062cu;goto done;}
#endif
/* 2062c: mov r4, r0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_0;
uint32_t result = b;
r_4 = (uint32_t)(result);
}
}
goto L_2062e;
L_2062e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2062eu)) {r_15=0x2062eu;goto done;}
#endif
/* 2062e: ldr r0, [r0, #0x64] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_20630;
L_20630:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20630u)) {r_15=0x20630u;goto done;}
#endif
/* 20630: vpush {d8, d9, d10, d11, d12} */
{
{
uint32_t fp_addr = (r_13 - 40u);
uint32_t fp_value;
m8_le_write(M8_OV_STACK(fp_addr + 0u),4,m8_fp_bits32(s_16));
m8_le_write(M8_OV_STACK(fp_addr + 4u),4,m8_fp_bits32(s_17));
m8_le_write(M8_OV_STACK(fp_addr + 8u),4,m8_fp_bits32(s_18));
m8_le_write(M8_OV_STACK(fp_addr + 12u),4,m8_fp_bits32(s_19));
m8_le_write(M8_OV_STACK(fp_addr + 16u),4,m8_fp_bits32(s_20));
m8_le_write(M8_OV_STACK(fp_addr + 20u),4,m8_fp_bits32(s_21));
m8_le_write(M8_OV_STACK(fp_addr + 24u),4,m8_fp_bits32(s_22));
m8_le_write(M8_OV_STACK(fp_addr + 28u),4,m8_fp_bits32(s_23));
m8_le_write(M8_OV_STACK(fp_addr + 32u),4,m8_fp_bits32(s_24));
m8_le_write(M8_OV_STACK(fp_addr + 36u),4,m8_fp_bits32(s_25));
r_13 -= 40u;
}
}
goto L_20634;
L_20634:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20634u)) {r_15=0x20634u;goto done;}
#endif
/* 20634: sub sp, #0x14 */
{
{
uint32_t a = r_13;
uint32_t b = 0x14u;
uint32_t result = a - b - 0u;
r_13 = (uint32_t)(result);
}
}
goto L_20636;
L_20636:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20636u)) {r_15=0x20636u;goto done;}
#endif
/* 20636: cmp r0, #0 */
{
{
uint32_t a = r_0;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20638;
L_20638:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20638u)) {r_15=0x20638u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20638: beq.w #0x2095a */
{
goto L_2095a;
}
}
goto L_2063c;
L_2063c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2063cu)) {r_15=0x2063cu;goto done;}
#endif
/* 2063c: ldrb.w r5, [r4, #0x40] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (64u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_5 = (uint32_t)(value);
}
}
goto L_20640;
L_20640:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20640u)) {r_15=0x20640u;goto done;}
#endif
/* 20640: mov sb, r2 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_2;
uint32_t result = b;
r_9 = (uint32_t)(result);
}
}
goto L_20642;
L_20642:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20642u)) {r_15=0x20642u;goto done;}
#endif
/* 20642: ldrb.w r2, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_2 = (uint32_t)(value);
}
}
goto L_20646;
L_20646:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20646u)) {r_15=0x20646u;goto done;}
#endif
/* 20646: cmp r5, #8 */
{
{
uint32_t a = r_5;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20648;
L_20648:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20648u)) {r_15=0x20648u;goto done;}
#endif
if(m8_condition(cpu->xpsr,3)) {
/* 2064a: movhs r5, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
r_5 = (uint32_t)(result);
}
}
}
goto L_2064c;
L_2064c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2064cu)) {r_15=0x2064cu;goto done;}
#endif
/* 2064c: cmp r2, #8 */
{
{
uint32_t a = r_2;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_2064e;
L_2064e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2064eu)) {r_15=0x2064eu;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 2064e: beq.w #0x20c6c */
{
goto L_20c6c;
}
}
goto L_20652;
L_20652:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20652u)) {r_15=0x20652u;goto done;}
#endif
/* 20652: ldrh.w r6, [r4, #0x98] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (152u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_6 = (uint32_t)(value);
}
}
goto L_20656;
L_20656:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20656u)) {r_15=0x20656u;goto done;}
#endif
/* 20656: cmp r6, #1 */
{
{
uint32_t a = r_6;
uint32_t b = 0x1u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20658;
L_20658:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20658u)) {r_15=0x20658u;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 20658: bls.w #0x20964 */
{
goto L_20964;
}
}
goto L_2065c;
L_2065c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2065cu)) {r_15=0x2065cu;goto done;}
#endif
/* 2065c: cmp r6, #0xff */
{
{
uint32_t a = r_6;
uint32_t b = 0xffu;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_2065e;
L_2065e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2065eu)) {r_15=0x2065eu;goto done;}
#endif
if(m8_condition(cpu->xpsr,3)) {
/* 20660: movhs r6, #0xff */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0xffu;
uint32_t result = b;
r_6 = (uint32_t)(result);
}
}
}
goto L_20662;
L_20662:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20662u)) {r_15=0x20662u;goto done;}
#endif
/* 20662: uxtb r6, r6 */
{
{
uint32_t value = r_6;
r_6 = (uint32_t)(value & 255u);
}
}
goto L_20664;
L_20664:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20664u)) {r_15=0x20664u;goto done;}
#endif
/* 20664: ldr r2, [r4, #0x70] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (112u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_2 = (uint32_t)(value);
}
}
goto L_20666;
L_20666:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20666u)) {r_15=0x20666u;goto done;}
#endif
/* 20666: ldr r0, [r4, #0x68] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (104u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_20668;
L_20668:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20668u)) {r_15=0x20668u;goto done;}
#endif
/* 20668: str r2, [r4, #0x74] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (116u);
uint32_t value = r_2;
m8_le_write(M8_OV_OBJECT(addr),4,value);
}
}
goto L_2066a;
L_2066a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2066au)) {r_15=0x2066au;goto done;}
#endif
/* 2066a: ldr.w r8, [r4, #0x20] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (32u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_8 = (uint32_t)(value);
}
}
goto L_2066e;
L_2066e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2066eu)) {r_15=0x2066eu;goto done;}
#endif
/* 2066e: ldr r2, [r4, #0x44] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (68u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_2 = (uint32_t)(value);
}
}
goto L_20670;
L_20670:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20670u)) {r_15=0x20670u;goto done;}
#endif
/* 20670: ldr.w lr, [r4, #0x24] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (36u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_14 = (uint32_t)(value);
}
}
goto L_20674;
L_20674:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20674u)) {r_15=0x20674u;goto done;}
#endif
/* 20674: ldr.w ip, [r4, #0x28] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (40u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_12 = (uint32_t)(value);
}
}
goto L_20678;
L_20678:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20678u)) {r_15=0x20678u;goto done;}
#endif
/* 20678: ldr r7, [r4, #0x2c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (44u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_7 = (uint32_t)(value);
}
}
goto L_2067a;
L_2067a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2067au)) {r_15=0x2067au;goto done;}
#endif
/* 2067a: ldr.w fp, [r4, #0x54] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (84u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_11 = (uint32_t)(value);
}
}
goto L_2067e;
L_2067e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2067eu)) {r_15=0x2067eu;goto done;}
#endif
/* 2067e: str r2, [r4, #0x4c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (76u);
uint32_t value = r_2;
m8_le_write(M8_OV_OBJECT(addr),4,value);
}
}
goto L_20680;
L_20680:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20680u)) {r_15=0x20680u;goto done;}
#endif
/* 20680: str.w r8, [r0] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (0u);
uint32_t value = r_8;
m8_le_write(M8_OV_AUXILIARY(addr),4,value);
}
}
goto L_20684;
L_20684:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20684u)) {r_15=0x20684u;goto done;}
#endif
/* 20684: str.w lr, [r0, #4] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (4u);
uint32_t value = r_14;
m8_le_write(M8_OV_AUXILIARY(addr),4,value);
}
}
goto L_20688;
L_20688:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20688u)) {r_15=0x20688u;goto done;}
#endif
/* 20688: str.w ip, [r0, #8] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (8u);
uint32_t value = r_12;
m8_le_write(M8_OV_AUXILIARY(addr),4,value);
}
}
goto L_2068c;
L_2068c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2068cu)) {r_15=0x2068cu;goto done;}
#endif
/* 2068c: str r7, [r0, #0xc] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (12u);
uint32_t value = r_7;
m8_le_write(M8_OV_AUXILIARY(addr),4,value);
}
}
goto L_2068e;
L_2068e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2068eu)) {r_15=0x2068eu;goto done;}
#endif
/* 2068e: cmp r3, #0 */
{
{
uint32_t a = r_3;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20690;
L_20690:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20690u)) {r_15=0x20690u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20690: beq.w #0x20958 */
{
goto L_20958;
}
}
goto L_20694;
L_20694:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20694u)) {r_15=0x20694u;goto done;}
#endif
/* 20694: subs r3, #1 */
{
{
uint32_t a = r_3;
uint32_t b = 0x1u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
r_3 = (uint32_t)(result);
}
}
goto L_20696;
L_20696:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20696u)) {r_15=0x20696u;goto done;}
#endif
/* 20696: adds r2, r1, #4 */
{
{
uint32_t a = r_1;
uint32_t b = 0x4u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
r_2 = (uint32_t)(result);
}
}
goto L_20698;
L_20698:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20698u)) {r_15=0x20698u;goto done;}
#endif
/* 20698: mov r7, r1 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_1;
uint32_t result = b;
r_7 = (uint32_t)(result);
}
}
goto L_2069a;
L_2069a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2069au)) {r_15=0x2069au;goto done;}
#endif
/* 2069a: vldr s17, [pc, #0x2fc] */
{
{
uint32_t fp_addr = (0x2069cu + 764u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_17=m8_fp_float(fp_value);
}
}
goto L_2069e;
L_2069e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2069eu)) {r_15=0x2069eu;goto done;}
#endif
/* 2069e: uxth r3, r3 */
{
{
uint32_t value = r_3;
r_3 = (uint32_t)(value & 65535u);
}
}
goto L_206a0;
L_206a0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206a0u)) {r_15=0x206a0u;goto done;}
#endif
/* 206a0: vldr s21, [pc, #0x2f8] */
{
{
uint32_t fp_addr = (0x206a4u + 760u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_21=m8_fp_float(fp_value);
}
}
goto L_206a4;
L_206a4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206a4u)) {r_15=0x206a4u;goto done;}
#endif
/* 206a4: vldr s20, [pc, #0x2f8] */
{
{
uint32_t fp_addr = (0x206a8u + 760u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_20=m8_fp_float(fp_value);
}
}
goto L_206a8;
L_206a8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206a8u)) {r_15=0x206a8u;goto done;}
#endif
/* 206a8: add.w r3, r2, r3, lsl #2 */
{
{
uint32_t a = r_2;
uint32_t b = m8_shift(r_3, 2, 2, NULL);
uint32_t result = a + b + 0u;
r_3 = (uint32_t)(result);
}
}
goto L_206ac;
L_206ac:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206acu)) {r_15=0x206acu;goto done;}
#endif
/* 206ac: vldr s22, [pc, #0x2f4] */
{
{
uint32_t fp_addr = (0x206b0u + 756u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_22=m8_fp_float(fp_value);
}
}
goto L_206b0;
L_206b0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206b0u)) {r_15=0x206b0u;goto done;}
#endif
/* 206b0: vldr d9, [pc, #0x2dc] */
{
{
uint32_t fp_addr = (0x206b4u + 732u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_18=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 4u),4);
s_19=m8_fp_float(fp_value);
}
}
goto L_206b4;
L_206b4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206b4u)) {r_15=0x206b4u;goto done;}
#endif
/* 206b4: str r3, [sp, #0xc] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (12u);
uint32_t value = r_3;
m8_le_write(M8_OV_STACK(addr),4,value);
}
}
goto L_206b6;
L_206b6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206b6u)) {r_15=0x206b6u;goto done;}
#endif
/* 206b6: ldrb.w sl, [r4, #0x6c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (108u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_10 = (uint32_t)(value);
}
}
goto L_206ba;
L_206ba:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206bau)) {r_15=0x206bau;goto done;}
#endif
/* 206ba: cmp.w sl, #0 */
{
{
uint32_t a = r_10;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_206be;
L_206be:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206beu)) {r_15=0x206beu;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 206be: beq.w #0x20ab0 */
{
goto L_20ab0;
}
}
goto L_206c2;
L_206c2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206c2u)) {r_15=0x206c2u;goto done;}
#endif
/* 206c2: add.w r8, r5, #-1 */
{
{
uint32_t a = r_5;
uint32_t b = 0xffffffffu;
uint32_t result = a + b + 0u;
r_8 = (uint32_t)(result);
}
}
goto L_206c6;
L_206c6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206c6u)) {r_15=0x206c6u;goto done;}
#endif
/* 206c6: uxtb.w r3, r8 */
{
{
uint32_t value = r_8;
r_3 = (uint32_t)(value & 255u);
}
}
goto L_206ca;
L_206ca:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206cau)) {r_15=0x206cau;goto done;}
#endif
/* 206ca: str r3, [sp, #8] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (8u);
uint32_t value = r_3;
m8_le_write(M8_OV_STACK(addr),4,value);
}
}
goto L_206cc;
L_206cc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206ccu)) {r_15=0x206ccu;goto done;}
#endif
/* 206cc: vmov s15, sl */
{
s_15=m8_fp_float(r_10);
}
goto L_206d0;
L_206d0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206d0u)) {r_15=0x206d0u;goto done;}
#endif
/* 206d0: vmov.f32 s14, #1.000000e+00 */
{
s_14=m8_fp_float(0x3f800000u);
}
goto L_206d4;
L_206d4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206d4u)) {r_15=0x206d4u;goto done;}
#endif
/* 206d4: cmp.w sl, #6 */
{
{
uint32_t a = r_10;
uint32_t b = 0x6u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_206d8;
L_206d8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206d8u)) {r_15=0x206d8u;goto done;}
#endif
/* 206d8: vcvt.f32.u32 s15, s15, #3 */
{
#ifdef M8_AOT_NATIVE_FP
if (cpu->fpscr & (3u << 22)) m8_fp_sync(cpu);
#endif
s_15=m8_fp_float(m8_oscillator_voice_from_int(cpu, (uint32_t)m8_fp_bits32(s_15), 0, 1, 32, 3, 1));
}
goto L_206dc;
L_206dc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206dcu)) {r_15=0x206dcu;goto done;}
#endif
/* 206dc: vsub.f32 s14, s14, s15 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));
}
goto L_206e0;
L_206e0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206e0u)) {r_15=0x206e0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,9)) {
/* 206e0: bhi.w #0x20972 */
{
goto L_20972;
}
}
goto L_206e4;
L_206e4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206e4u)) {r_15=0x206e4u;goto done;}
#endif
/* 206e4: add.w sl, sl, #1 */
{
{
uint32_t a = r_10;
uint32_t b = 0x1u;
uint32_t result = a + b + 0u;
r_10 = (uint32_t)(result);
}
}
goto L_206e8;
L_206e8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206e8u)) {r_15=0x206e8u;goto done;}
#endif
/* 206e8: uxtb.w sl, sl */
{
{
uint32_t value = r_10;
r_10 = (uint32_t)(value & 255u);
}
}
goto L_206ec;
L_206ec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206ecu)) {r_15=0x206ecu;goto done;}
#endif
/* 206ec: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_206ee;
L_206ee:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206eeu)) {r_15=0x206eeu;goto done;}
#endif
/* 206ee: vmov.f32 s7, #5.000000e-01 */
{
s_7=m8_fp_float(0x3f000000u);
}
goto L_206f2;
L_206f2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206f2u)) {r_15=0x206f2u;goto done;}
#endif
/* 206f2: strb.w sl, [r4, #0x6c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (108u);
uint32_t value = r_10;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_206f6;
L_206f6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206f6u)) {r_15=0x206f6u;goto done;}
#endif
/* 206f6: vldr s23, [r3, #0x28] */
{
{
uint32_t fp_addr = (r_3 + 40u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_23=m8_fp_float(fp_value);
}
}
goto L_206fa;
L_206fa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206fau)) {r_15=0x206fau;goto done;}
#endif
/* 206fa: vldr s2, [r3, #0x78] */
{
{
uint32_t fp_addr = (r_3 + 120u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_2=m8_fp_float(fp_value);
}
}
goto L_206fe;
L_206fe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x206feu)) {r_15=0x206feu;goto done;}
#endif
/* 206fe: vmul.f32 s23, s15, s23 */
{
s_23=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_23), 0));
}
goto L_20702;
L_20702:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20702u)) {r_15=0x20702u;goto done;}
#endif
/* 20702: vldr s6, [r3, #0x18] */
{
{
uint32_t fp_addr = (r_3 + 24u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_6=m8_fp_float(fp_value);
}
}
goto L_20706;
L_20706:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20706u)) {r_15=0x20706u;goto done;}
#endif
/* 20706: vldr s8, [r3, #4] */
{
{
uint32_t fp_addr = (r_3 + 4u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_8=m8_fp_float(fp_value);
}
}
goto L_2070a;
L_2070a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2070au)) {r_15=0x2070au;goto done;}
#endif
/* 2070a: vldr s9, [r3, #0x1c] */
{
{
uint32_t fp_addr = (r_3 + 28u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_9=m8_fp_float(fp_value);
}
}
goto L_2070e;
L_2070e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2070eu)) {r_15=0x2070eu;goto done;}
#endif
/* 2070e: vfma.f32 s23, s2, s14 */
{
s_23=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_2), m8_fp_bits32(s_14), m8_fp_bits32(s_23)));
}
goto L_20712;
L_20712:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20712u)) {r_15=0x20712u;goto done;}
#endif
/* 20712: vldr s10, [r3, #0x20] */
{
{
uint32_t fp_addr = (r_3 + 32u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_10=m8_fp_float(fp_value);
}
}
goto L_20716;
L_20716:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20716u)) {r_15=0x20716u;goto done;}
#endif
/* 20716: vldr s11, [r3, #0x24] */
{
{
uint32_t fp_addr = (r_3 + 36u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_11=m8_fp_float(fp_value);
}
}
goto L_2071a;
L_2071a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2071au)) {r_15=0x2071au;goto done;}
#endif
/* 2071a: vmul.f32 s8, s15, s8 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_8), 0));
}
goto L_2071e;
L_2071e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2071eu)) {r_15=0x2071eu;goto done;}
#endif
/* 2071e: vldr s12, [r3, #0x10] */
{
{
uint32_t fp_addr = (r_3 + 16u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_12=m8_fp_float(fp_value);
}
}
goto L_20722;
L_20722:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20722u)) {r_15=0x20722u;goto done;}
#endif
/* 20722: vmul.f32 s9, s15, s9 */
{
s_9=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_9), 0));
}
goto L_20726;
L_20726:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20726u)) {r_15=0x20726u;goto done;}
#endif
/* 20726: vldr s13, [r3, #0x14] */
{
{
uint32_t fp_addr = (r_3 + 20u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_2072a;
L_2072a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2072au)) {r_15=0x2072au;goto done;}
#endif
/* 2072a: vmul.f32 s10, s15, s10 */
{
s_10=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_10), 0));
}
goto L_2072e;
L_2072e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2072eu)) {r_15=0x2072eu;goto done;}
#endif
/* 2072e: vmul.f32 s11, s15, s11 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_11), 0));
}
goto L_20732;
L_20732:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20732u)) {r_15=0x20732u;goto done;}
#endif
/* 20732: vldr s16, [r3, #0x54] */
{
{
uint32_t fp_addr = (r_3 + 84u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_16=m8_fp_float(fp_value);
}
}
goto L_20736;
L_20736:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20736u)) {r_15=0x20736u;goto done;}
#endif
/* 20736: vmul.f32 s12, s15, s12 */
{
s_12=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_12), 0));
}
goto L_2073a;
L_2073a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2073au)) {r_15=0x2073au;goto done;}
#endif
/* 2073a: vldr s0, [r3, #0x6c] */
{
{
uint32_t fp_addr = (r_3 + 108u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_0=m8_fp_float(fp_value);
}
}
goto L_2073e;
L_2073e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2073eu)) {r_15=0x2073eu;goto done;}
#endif
/* 2073e: vmul.f32 s13, s15, s13 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));
}
goto L_20742;
L_20742:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20742u)) {r_15=0x20742u;goto done;}
#endif
/* 20742: vldr s1, [r3, #0x70] */
{
{
uint32_t fp_addr = (r_3 + 112u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_1=m8_fp_float(fp_value);
}
}
goto L_20746;
L_20746:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20746u)) {r_15=0x20746u;goto done;}
#endif
/* 20746: vmul.f32 s15, s15, s6 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_6), 0));
}
goto L_2074a;
L_2074a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2074au)) {r_15=0x2074au;goto done;}
#endif
/* 2074a: vldr s3, [r3, #0x74] */
{
{
uint32_t fp_addr = (r_3 + 116u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_3=m8_fp_float(fp_value);
}
}
goto L_2074e;
L_2074e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2074eu)) {r_15=0x2074eu;goto done;}
#endif
/* 2074e: vldr s4, [r3, #0x60] */
{
{
uint32_t fp_addr = (r_3 + 96u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_4=m8_fp_float(fp_value);
}
}
goto L_20752;
L_20752:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20752u)) {r_15=0x20752u;goto done;}
#endif
/* 20752: vfma.f32 s8, s16, s14 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_16), m8_fp_bits32(s_14), m8_fp_bits32(s_8)));
}
goto L_20756;
L_20756:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20756u)) {r_15=0x20756u;goto done;}
#endif
/* 20756: vldr s5, [r3, #0x64] */
{
{
uint32_t fp_addr = (r_3 + 100u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_5=m8_fp_float(fp_value);
}
}
goto L_2075a;
L_2075a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2075au)) {r_15=0x2075au;goto done;}
#endif
/* 2075a: vfma.f32 s9, s0, s14 */
{
s_9=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_0), m8_fp_bits32(s_14), m8_fp_bits32(s_9)));
}
goto L_2075e;
L_2075e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2075eu)) {r_15=0x2075eu;goto done;}
#endif
/* 2075e: vldr s6, [r3, #0x68] */
{
{
uint32_t fp_addr = (r_3 + 104u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_6=m8_fp_float(fp_value);
}
}
goto L_20762;
L_20762:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20762u)) {r_15=0x20762u;goto done;}
#endif
/* 20762: vfma.f32 s10, s1, s14 */
{
s_10=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_1), m8_fp_bits32(s_14), m8_fp_bits32(s_10)));
}
goto L_20766;
L_20766:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20766u)) {r_15=0x20766u;goto done;}
#endif
/* 20766: vfma.f32 s11, s3, s14 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_3), m8_fp_bits32(s_14), m8_fp_bits32(s_11)));
}
goto L_2076a;
L_2076a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2076au)) {r_15=0x2076au;goto done;}
#endif
/* 2076a: vstr s23, [r3, #0x50] */
{
{
uint32_t fp_addr = (r_3 + 80u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_23));
}
}
goto L_2076e;
L_2076e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2076eu)) {r_15=0x2076eu;goto done;}
#endif
/* 2076e: vfma.f32 s12, s4, s14 */
{
s_12=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_4), m8_fp_bits32(s_14), m8_fp_bits32(s_12)));
}
goto L_20772;
L_20772:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20772u)) {r_15=0x20772u;goto done;}
#endif
/* 20772: vfma.f32 s13, s5, s14 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_5), m8_fp_bits32(s_14), m8_fp_bits32(s_13)));
}
goto L_20776;
L_20776:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20776u)) {r_15=0x20776u;goto done;}
#endif
/* 20776: vfma.f32 s15, s6, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_6), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));
}
goto L_2077a;
L_2077a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2077au)) {r_15=0x2077au;goto done;}
#endif
/* 2077a: vstr s8, [r3, #0x2c] */
{
{
uint32_t fp_addr = (r_3 + 44u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_8));
}
}
goto L_2077e;
L_2077e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2077eu)) {r_15=0x2077eu;goto done;}
#endif
/* 2077e: vcmpe.f32 s23, s7 */
{
m8_oscillator_voice_cmp32(cpu, m8_fp_bits32(s_23), m8_fp_bits32(s_7), 1);
}
goto L_20782;
L_20782:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20782u)) {r_15=0x20782u;goto done;}
#endif
/* 20782: vstr s9, [r3, #0x44] */
{
{
uint32_t fp_addr = (r_3 + 68u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_9));
}
}
goto L_20786;
L_20786:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20786u)) {r_15=0x20786u;goto done;}
#endif
/* 20786: vstr s10, [r3, #0x48] */
{
{
uint32_t fp_addr = (r_3 + 72u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_10));
}
}
goto L_2078a;
L_2078a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2078au)) {r_15=0x2078au;goto done;}
#endif
/* 2078a: vstr s11, [r3, #0x4c] */
{
{
uint32_t fp_addr = (r_3 + 76u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_11));
}
}
goto L_2078e;
L_2078e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2078eu)) {r_15=0x2078eu;goto done;}
#endif
/* 2078e: vmrs apsr_nzcv, fpscr */
{
#ifndef M8_AOT_NATIVE_FP
m8_fp_sync(cpu);
#endif
cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);
}
goto L_20792;
L_20792:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20792u)) {r_15=0x20792u;goto done;}
#endif
/* 20792: vstr s12, [r3, #0x38] */
{
{
uint32_t fp_addr = (r_3 + 56u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_12));
}
}
goto L_20796;
L_20796:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20796u)) {r_15=0x20796u;goto done;}
#endif
/* 20796: vstr s13, [r3, #0x3c] */
{
{
uint32_t fp_addr = (r_3 + 60u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_13));
}
}
goto L_2079a;
L_2079a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2079au)) {r_15=0x2079au;goto done;}
#endif
/* 2079a: vstr s15, [r3, #0x40] */
{
{
uint32_t fp_addr = (r_3 + 64u);
uint32_t fp_value;
m8_le_write(M8_OV_CONTROL(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_2079e;
L_2079e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2079eu)) {r_15=0x2079eu;goto done;}
#endif
if(m8_condition(cpu->xpsr,12)) {
/* 2079e: blt.w #0x20968 */
{
goto L_20968;
}
}
goto L_207a2;
L_207a2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207a2u)) {r_15=0x207a2u;goto done;}
#endif
/* 207a2: vmov.f32 s24, #1.000000e+00 */
{
s_24=m8_fp_float(0x3f800000u);
}
goto L_207a6;
L_207a6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207a6u)) {r_15=0x207a6u;goto done;}
#endif
/* 207a6: vsub.f32 s15, s7, s23 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_7), m8_fp_bits32(s_23), 0));
}
goto L_207aa;
L_207aa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207aau)) {r_15=0x207aau;goto done;}
#endif
/* 207aa: vmov.f32 s14, #2.000000e+00 */
{
s_14=m8_fp_float(0x40000000u);
}
goto L_207ae;
L_207ae:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207aeu)) {r_15=0x207aeu;goto done;}
#endif
/* 207ae: vmov.f32 s23, s24 */
{
s_23=m8_fp_float(m8_fp_bits32(s_24));
}
goto L_207b2;
L_207b2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207b2u)) {r_15=0x207b2u;goto done;}
#endif
/* 207b2: vfma.f32 s24, s15, s14 */
{
s_24=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_14), m8_fp_bits32(s_24)));
}
goto L_207b6;
L_207b6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207b6u)) {r_15=0x207b6u;goto done;}
#endif
/* 207b6: ldrb.w r1, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_207ba;
L_207ba:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207bau)) {r_15=0x207bau;goto done;}
#endif
/* 207ba: ldrb.w r3, [r4, #0x95] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (149u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_207be;
L_207be:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207beu)) {r_15=0x207beu;goto done;}
#endif
/* 207be: cmp r1, r3 */
{
{
uint32_t a = r_1;
uint32_t b = r_3;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_207c0;
L_207c0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207c0u)) {r_15=0x207c0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 207c0: bne #0x207cc */
{
goto L_207cc;
}
}
goto L_207c2;
L_207c2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207c2u)) {r_15=0x207c2u;goto done;}
#endif
/* 207c2: ldrh.w r3, [r4, #0x9a] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (154u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_3 = (uint32_t)(value);
}
}
goto L_207c6;
L_207c6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207c6u)) {r_15=0x207c6u;goto done;}
#endif
/* 207c6: cmp r6, r3 */
{
{
uint32_t a = r_6;
uint32_t b = r_3;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_207c8;
L_207c8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207c8u)) {r_15=0x207c8u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 207c8: beq.w #0x20be0 */
{
goto L_20be0;
}
}
goto L_207cc;
L_207cc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207ccu)) {r_15=0x207ccu;goto done;}
#endif
/* 207cc: ldrb.w r3, [r4, #0x9c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (156u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_207d0;
L_207d0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207d0u)) {r_15=0x207d0u;goto done;}
#endif
/* 207d0: mov r1, r6 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_6;
uint32_t result = b;
r_1 = (uint32_t)(result);
}
}
goto L_207d2;
L_207d2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207d2u)) {r_15=0x207d2u;goto done;}
#endif
/* 207d2: strb.w r3, [r4, #0x9d] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (157u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_207d6;
L_207d6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207d6u)) {r_15=0x207d6u;goto done;}
#endif
/* 207d6: mov r0, r4 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_4;
uint32_t result = b;
r_0 = (uint32_t)(result);
}
}
goto L_207d8;
L_207d8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207d8u)) {r_15=0x207d8u;goto done;}
#endif
/* 207d8: bl #0x1f820 */
{
r_14=0x207ddu;r_15=0x1f820u;
M8_OV_SAVE(cpu);int status=configure(cpu,mem);
if(status!=M8_AOT_EXIT || M8_OV_CHILD_PC()!=0x207dcu) {M8_OV_CHILD_RETURN(status);}
M8_OV_LOAD(cpu);
if(r_4!=root || r_13!=frame || r_7<left || r_7>=left_end ||
   ((r_7-left)&3u) || r_9!=right+(r_7-left) ||
   m8_oscillator_voice_memory_offset(mem)!=arena ||
   m8_le_read(M8_OV_HOST(frame+12u),4)!=left_end ||
   !m8_oscillator_voice_qualify(cpu,mem,root,r_7,r_9,
       (left_end-r_7)/4u,frame,r_11)) {
    r_15=0x207dcu;goto done;}
#if defined(M8_AOT_OSCILLATOR_SPANS)
M8_OV_REFRESH_SPANS(r_11);
#endif
}
goto L_207dc;
L_207dc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207dcu)) {r_15=0x207dcu;goto done;}
#endif
/* 207dc: ldrb.w r3, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_207e0;
L_207e0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207e0u)) {r_15=0x207e0u;goto done;}
#endif
/* 207e0: movs r1, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_1 = (uint32_t)(result);
}
}
goto L_207e2;
L_207e2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207e2u)) {r_15=0x207e2u;goto done;}
#endif
/* 207e2: vldr s15, [r4, #0xa0] */
{
{
uint32_t fp_addr = (r_4 + 160u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_207e6;
L_207e6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207e6u)) {r_15=0x207e6u;goto done;}
#endif
/* 207e6: cmp r3, #8 */
{
{
uint32_t a = r_3;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_207e8;
L_207e8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207e8u)) {r_15=0x207e8u;goto done;}
#endif
/* 207e8: strb.w r3, [r4, #0x95] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (149u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_207ec;
L_207ec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207ecu)) {r_15=0x207ecu;goto done;}
#endif
/* 207ec: strb.w r1, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value = r_1;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_207f0;
L_207f0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207f0u)) {r_15=0x207f0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 207f0: beq.w #0x20b00 */
{
goto L_20b00;
}
}
goto L_207f4;
L_207f4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207f4u)) {r_15=0x207f4u;goto done;}
#endif
/* 207f4: ldrh.w r3, [r4, #0x9a] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (154u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_3 = (uint32_t)(value);
}
}
goto L_207f8;
L_207f8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207f8u)) {r_15=0x207f8u;goto done;}
#endif
/* 207f8: vmov s14, r3 */
{
s_14=m8_fp_float(r_3);
}
goto L_207fc;
L_207fc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x207fcu)) {r_15=0x207fcu;goto done;}
#endif
/* 207fc: vcvt.f32.s32 s14, s14 */
{
s_14=m8_fp_float(m8_oscillator_voice_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0));
}
goto L_20800;
L_20800:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20800u)) {r_15=0x20800u;goto done;}
#endif
/* 20800: vmul.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_20804;
L_20804:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20804u)) {r_15=0x20804u;goto done;}
#endif
/* 20804: ldrb.w r3, [r4, #0x9c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (156u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20808;
L_20808:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20808u)) {r_15=0x20808u;goto done;}
#endif
/* 20808: vstr s15, [r4, #0xa4] */
{
{
uint32_t fp_addr = (r_4 + 164u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_2080c;
L_2080c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2080cu)) {r_15=0x2080cu;goto done;}
#endif
/* 2080c: cbz r3, #0x20814 */
{
if (r_3 == 0) {
goto L_20814;
}
}
goto L_2080e;
L_2080e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2080eu)) {r_15=0x2080eu;goto done;}
#endif
/* 2080e: movs r3, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_3 = (uint32_t)(result);
}
}
goto L_20810;
L_20810:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20810u)) {r_15=0x20810u;goto done;}
#endif
/* 20810: strb.w r3, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_20814;
L_20814:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20814u)) {r_15=0x20814u;goto done;}
#endif
/* 20814: ldrb.w r2, [r4, #0x9e] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (158u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_2 = (uint32_t)(value);
}
}
goto L_20818;
L_20818:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20818u)) {r_15=0x20818u;goto done;}
#endif
/* 20818: cmp r2, #0 */
{
{
uint32_t a = r_2;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_2081a;
L_2081a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2081au)) {r_15=0x2081au;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 2081a: beq.w #0x20c38 */
{
goto L_20c38;
}
}
goto L_2081e;
L_2081e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2081eu)) {r_15=0x2081eu;goto done;}
#endif
/* 2081e: ldrh.w r3, [r4, #0x9a] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (154u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_3 = (uint32_t)(value);
}
}
goto L_20822;
L_20822:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20822u)) {r_15=0x20822u;goto done;}
#endif
/* 20822: mov.w r8, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
r_8 = (uint32_t)(result);
}
}
goto L_20826;
L_20826:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20826u)) {r_15=0x20826u;goto done;}
#endif
/* 20826: vldr s15, [r4, #0x70] */
{
{
uint32_t fp_addr = (r_4 + 112u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_2082a;
L_2082a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2082au)) {r_15=0x2082au;goto done;}
#endif
/* 2082a: uxth r1, r6 */
{
{
uint32_t value = r_6;
r_1 = (uint32_t)(value & 65535u);
}
}
goto L_2082c;
L_2082c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2082cu)) {r_15=0x2082cu;goto done;}
#endif
/* 2082c: vmov s14, r3 */
{
s_14=m8_fp_float(r_3);
}
goto L_20830;
L_20830:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20830u)) {r_15=0x20830u;goto done;}
#endif
/* 20830: vldr s16, [pc, #0x170] */
{
{
uint32_t fp_addr = (0x20834u + 368u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_16=m8_fp_float(fp_value);
}
}
goto L_20834;
L_20834:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20834u)) {r_15=0x20834u;goto done;}
#endif
/* 20834: mov sl, r8 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_8;
uint32_t result = b;
r_10 = (uint32_t)(result);
}
}
goto L_20836;
L_20836:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20836u)) {r_15=0x20836u;goto done;}
#endif
/* 20836: vcvt.f32.s32 s14, s14 */
{
s_14=m8_fp_float(m8_oscillator_voice_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0));
}
goto L_2083a;
L_2083a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2083au)) {r_15=0x2083au;goto done;}
#endif
/* 2083a: b #0x20850 */
{
goto L_20850;
}
goto L_2083c;
L_2083c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2083cu)) {r_15=0x2083cu;goto done;}
#endif
/* 2083c: vsub.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_20840;
L_20840:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20840u)) {r_15=0x20840u;goto done;}
#endif
/* 20840: vstr s15, [r4, #0x70] */
{
{
uint32_t fp_addr = (r_4 + 112u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_20844;
L_20844:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20844u)) {r_15=0x20844u;goto done;}
#endif
/* 20844: add.w r8, r8, #1 */
{
{
uint32_t a = r_8;
uint32_t b = 0x1u;
uint32_t result = a + b + 0u;
r_8 = (uint32_t)(result);
}
}
goto L_20848;
L_20848:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20848u)) {r_15=0x20848u;goto done;}
#endif
/* 20848: uxtb.w r3, r8 */
{
{
uint32_t value = r_8;
r_3 = (uint32_t)(value & 255u);
}
}
goto L_2084c;
L_2084c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2084cu)) {r_15=0x2084cu;goto done;}
#endif
/* 2084c: cmp r2, r3 */
{
{
uint32_t a = r_2;
uint32_t b = r_3;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_2084e;
L_2084e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2084eu)) {r_15=0x2084eu;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 2084e: bls #0x208c2 */
{
goto L_208c2;
}
}
goto L_20850;
L_20850:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20850u)) {r_15=0x20850u;goto done;}
#endif
/* 20850: vcvt.u32.f32 s13, s15 */
{
s_13=m8_fp_float(m8_oscillator_voice_to_int(cpu, m8_fp_bits32(s_15), 0, 1, 3, 32, 0));
}
goto L_20854;
L_20854:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20854u)) {r_15=0x20854u;goto done;}
#endif
/* 20854: vstr s13, [sp, #4] */
{
{
uint32_t fp_addr = (r_13 + 4u);
uint32_t fp_value;
m8_le_write(M8_OV_STACK(fp_addr + 0u),4,m8_fp_bits32(s_13));
}
}
goto L_20858;
L_20858:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20858u)) {r_15=0x20858u;goto done;}
#endif
/* 20858: vldr s13, [r4, #0xa4] */
{
{
uint32_t fp_addr = (r_4 + 164u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_2085c;
L_2085c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2085cu)) {r_15=0x2085cu;goto done;}
#endif
/* 2085c: ldrb.w r3, [sp, #4] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (4u);
uint32_t value;
value=m8_le_read(M8_OV_STACK(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20860;
L_20860:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20860u)) {r_15=0x20860u;goto done;}
#endif
/* 20860: vadd.f32 s15, s15, s13 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));
}
goto L_20864;
L_20864:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20864u)) {r_15=0x20864u;goto done;}
#endif
/* 20864: add.w r3, fp, r3, lsl #2 */
{
{
uint32_t a = r_11;
uint32_t b = m8_shift(r_3, 2, 2, NULL);
uint32_t result = a + b + 0u;
r_3 = (uint32_t)(result);
}
}
goto L_20868;
L_20868:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20868u)) {r_15=0x20868u;goto done;}
#endif
/* 20868: vcmpe.f32 s15, s14 */
{
m8_oscillator_voice_cmp32(cpu, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 1);
}
goto L_2086c;
L_2086c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2086cu)) {r_15=0x2086cu;goto done;}
#endif
/* 2086c: vldr s13, [r3] */
{
{
uint32_t fp_addr = (r_3 + 0u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_WAVE(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_20870;
L_20870:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20870u)) {r_15=0x20870u;goto done;}
#endif
/* 20870: vstr s15, [r4, #0x70] */
{
{
uint32_t fp_addr = (r_4 + 112u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_20874;
L_20874:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20874u)) {r_15=0x20874u;goto done;}
#endif
/* 20874: vadd.f32 s16, s16, s13 */
{
s_16=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_16), m8_fp_bits32(s_13), 0));
}
goto L_20878;
L_20878:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20878u)) {r_15=0x20878u;goto done;}
#endif
/* 20878: vmrs apsr_nzcv, fpscr */
{
#ifndef M8_AOT_NATIVE_FP
m8_fp_sync(cpu);
#endif
cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);
}
goto L_2087c;
L_2087c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2087cu)) {r_15=0x2087cu;goto done;}
#endif
if(m8_condition(cpu->xpsr,12)) {
/* 2087c: blt #0x20844 */
{
goto L_20844;
}
}
goto L_2087e;
L_2087e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2087eu)) {r_15=0x2087eu;goto done;}
#endif
/* 2087e: ldrb.w r3, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20882;
L_20882:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20882u)) {r_15=0x20882u;goto done;}
#endif
/* 20882: cmp r3, #0 */
{
{
uint32_t a = r_3;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20884;
L_20884:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20884u)) {r_15=0x20884u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20884: beq #0x2083c */
{
goto L_2083c;
}
}
goto L_20886;
L_20886:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20886u)) {r_15=0x20886u;goto done;}
#endif
/* 20886: mov r0, r4 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_4;
uint32_t result = b;
r_0 = (uint32_t)(result);
}
}
goto L_20888;
L_20888:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20888u)) {r_15=0x20888u;goto done;}
#endif
/* 20888: str r1, [sp, #4] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (4u);
uint32_t value = r_1;
m8_le_write(M8_OV_STACK(addr),4,value);
}
}
goto L_2088a;
L_2088a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2088au)) {r_15=0x2088au;goto done;}
#endif
/* 2088a: bl #0x1f820 */
{
r_14=0x2088fu;r_15=0x1f820u;
M8_OV_SAVE(cpu);int status=configure(cpu,mem);
if(status!=M8_AOT_EXIT || M8_OV_CHILD_PC()!=0x2088eu) {M8_OV_CHILD_RETURN(status);}
M8_OV_LOAD(cpu);
if(r_4!=root || r_13!=frame || r_7<left || r_7>=left_end ||
   ((r_7-left)&3u) || r_9!=right+(r_7-left) ||
   m8_oscillator_voice_memory_offset(mem)!=arena ||
   m8_le_read(M8_OV_HOST(frame+12u),4)!=left_end ||
   !m8_oscillator_voice_qualify(cpu,mem,root,r_7,r_9,
       (left_end-r_7)/4u,frame,r_11)) {
    r_15=0x2088eu;goto done;}
#if defined(M8_AOT_OSCILLATOR_SPANS)
M8_OV_REFRESH_SPANS(r_11);
#endif
}
goto L_2088e;
L_2088e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2088eu)) {r_15=0x2088eu;goto done;}
#endif
/* 2088e: ldrb.w r3, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20892;
L_20892:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20892u)) {r_15=0x20892u;goto done;}
#endif
/* 20892: vldr s13, [r4, #0xa0] */
{
{
uint32_t fp_addr = (r_4 + 160u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_20896;
L_20896:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20896u)) {r_15=0x20896u;goto done;}
#endif
/* 20896: cmp r3, #8 */
{
{
uint32_t a = r_3;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20898;
L_20898:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20898u)) {r_15=0x20898u;goto done;}
#endif
/* 20898: ldrh.w r3, [r4, #0x9a] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (154u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_3 = (uint32_t)(value);
}
}
goto L_2089c;
L_2089c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2089cu)) {r_15=0x2089cu;goto done;}
#endif
/* 2089c: ldr r1, [sp, #4] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (4u);
uint32_t value;
value=m8_le_read(M8_OV_STACK(addr),4);
r_1 = (uint32_t)(value);
}
}
goto L_2089e;
L_2089e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2089eu)) {r_15=0x2089eu;goto done;}
#endif
/* 2089e: vmov s14, r3 */
{
s_14=m8_fp_float(r_3);
}
goto L_208a2;
L_208a2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208a2u)) {r_15=0x208a2u;goto done;}
#endif
/* 208a2: strb.w sl, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value = r_10;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_208a6;
L_208a6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208a6u)) {r_15=0x208a6u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 208a6: beq #0x20978 */
{
goto L_20978;
}
}
goto L_208a8;
L_208a8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208a8u)) {r_15=0x208a8u;goto done;}
#endif
/* 208a8: vcvt.f32.s32 s14, s14 */
{
s_14=m8_fp_float(m8_oscillator_voice_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0));
}
goto L_208ac;
L_208ac:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208acu)) {r_15=0x208acu;goto done;}
#endif
/* 208ac: vmov.f32 s12, s14 */
{
s_12=m8_fp_float(m8_fp_bits32(s_14));
}
goto L_208b0;
L_208b0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208b0u)) {r_15=0x208b0u;goto done;}
#endif
/* 208b0: vmul.f32 s13, s13, s12 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_13), m8_fp_bits32(s_12), 0));
}
goto L_208b4;
L_208b4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208b4u)) {r_15=0x208b4u;goto done;}
#endif
/* 208b4: vldr s15, [r4, #0x70] */
{
{
uint32_t fp_addr = (r_4 + 112u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_208b8;
L_208b8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208b8u)) {r_15=0x208b8u;goto done;}
#endif
/* 208b8: ldrb.w r2, [r4, #0x9e] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (158u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_2 = (uint32_t)(value);
}
}
goto L_208bc;
L_208bc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208bcu)) {r_15=0x208bcu;goto done;}
#endif
/* 208bc: vstr s13, [r4, #0xa4] */
{
{
uint32_t fp_addr = (r_4 + 164u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_13));
}
}
goto L_208c0;
L_208c0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208c0u)) {r_15=0x208c0u;goto done;}
#endif
/* 208c0: b #0x2083c */
{
goto L_2083c;
}
goto L_208c2;
L_208c2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208c2u)) {r_15=0x208c2u;goto done;}
#endif
/* 208c2: vmov s15, r2 */
{
s_15=m8_fp_float(r_2);
}
goto L_208c6;
L_208c6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208c6u)) {r_15=0x208c6u;goto done;}
#endif
/* 208c6: vcvt.f32.s32 s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0));
}
goto L_208ca;
L_208ca:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208cau)) {r_15=0x208cau;goto done;}
#endif
/* 208ca: vdiv.f32 s0, s16, s15 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_DIV, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));
}
goto L_208ce;
L_208ce:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208ceu)) {r_15=0x208ceu;goto done;}
#endif
/* 208ce: ldr r3, [sp, #8] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (8u);
uint32_t value;
value=m8_le_read(M8_OV_STACK(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_208d0;
L_208d0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208d0u)) {r_15=0x208d0u;goto done;}
#endif
/* 208d0: cmp r3, #4 */
{
{
uint32_t a = r_3;
uint32_t b = 0x4u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_208d2;
L_208d2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208d2u)) {r_15=0x208d2u;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 208d2: bls #0x209d4 */
{
goto L_209d4;
}
}
goto L_208d4;
L_208d4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208d4u)) {r_15=0x208d4u;goto done;}
#endif
/* 208d4: cmp r5, #0 */
{
{
uint32_t a = r_5;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_208d6;
L_208d6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208d6u)) {r_15=0x208d6u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 208d6: bne.w #0x20b06 */
{
goto L_20b06;
}
}
goto L_208da;
L_208da:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208dau)) {r_15=0x208dau;goto done;}
#endif
/* 208da: ldrb.w r1, [r4, #0x41] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (65u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_208de;
L_208de:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208deu)) {r_15=0x208deu;goto done;}
#endif
/* 208de: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_208e0;
L_208e0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208e0u)) {r_15=0x208e0u;goto done;}
#endif
/* 208e0: cmp r1, #3 */
{
{
uint32_t a = r_1;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_208e2;
L_208e2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208e2u)) {r_15=0x208e2u;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 208e2: bls #0x20930 */
{
goto L_20930;
}
}
goto L_208e4;
L_208e4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208e4u)) {r_15=0x208e4u;goto done;}
#endif
/* 208e4: ldr r0, [r4, #0x50] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (80u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_208e6;
L_208e6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208e6u)) {r_15=0x208e6u;goto done;}
#endif
/* 208e6: vldr s1, [r3, #0x4c] */
{
{
uint32_t fp_addr = (r_3 + 76u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_1=m8_fp_float(fp_value);
}
}
goto L_208ea;
L_208ea:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208eau)) {r_15=0x208eau;goto done;}
#endif
/* 208ea: ldr r3, [r0] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_SLOT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_208ec;
L_208ec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208ecu)) {r_15=0x208ecu;goto done;}
#endif
/* 208ec: ldr r3, [r3] */
{
{
uint32_t base = r_3;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_TARGET(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_208ee;
L_208ee:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208eeu)) {r_15=0x208eeu;goto done;}
#endif
/* 208ee: blx r3 */
{
if(r_3!=0x1fa5u && r_3!=0x1fedu) {r_15=0x208eeu;goto done;}
r_14=0x208f1u;
r_15=r_3&~1u;
if(r_3==0x1fa5u) goto L_1fa4;
goto L_1fec;
}
goto L_208f0;
L_208f0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208f0u)) {r_15=0x208f0u;goto done;}
#endif
/* 208f0: ldrb.w r3, [r4, #0x41] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (65u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_208f4;
L_208f4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208f4u)) {r_15=0x208f4u;goto done;}
#endif
/* 208f4: cmp r3, #3 */
{
{
uint32_t a = r_3;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_208f6;
L_208f6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208f6u)) {r_15=0x208f6u;goto done;}
#endif
/* 208f6: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_208f8;
L_208f8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208f8u)) {r_15=0x208f8u;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 208f8: bls #0x20930 */
{
goto L_20930;
}
}
goto L_208fa;
L_208fa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208fau)) {r_15=0x208fau;goto done;}
#endif
/* 208fa: vldr s13, [r4, #0x44] */
{
{
uint32_t fp_addr = (r_4 + 68u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_208fe;
L_208fe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x208feu)) {r_15=0x208feu;goto done;}
#endif
/* 208fe: vmul.f32 s15, s0, s21 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_21), 0));
}
goto L_20902;
L_20902:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20902u)) {r_15=0x20902u;goto done;}
#endif
/* 20902: vcmpe.f32 s13, s20 */
{
m8_oscillator_voice_cmp32(cpu, m8_fp_bits32(s_13), m8_fp_bits32(s_20), 1);
}
goto L_20906;
L_20906:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20906u)) {r_15=0x20906u;goto done;}
#endif
/* 20906: vcvt.s32.f32 s15, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_to_int(cpu, m8_fp_bits32(s_15), 0, 0, 3, 32, 0));
}
goto L_2090a;
L_2090a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2090au)) {r_15=0x2090au;goto done;}
#endif
/* 2090a: vmrs apsr_nzcv, fpscr */
{
#ifndef M8_AOT_NATIVE_FP
m8_fp_sync(cpu);
#endif
cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);
}
goto L_2090e;
L_2090e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2090eu)) {r_15=0x2090eu;goto done;}
#endif
if(m8_condition(cpu->xpsr,6)) {
/* 2090e: bpl #0x20982 */
{
goto L_20982;
}
}
goto L_20910;
L_20910:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20910u)) {r_15=0x20910u;goto done;}
#endif
/* 20910: vcvt.u32.f32 s14, s13 */
{
s_14=m8_fp_float(m8_oscillator_voice_to_int(cpu, m8_fp_bits32(s_13), 0, 1, 3, 32, 0));
}
goto L_20914;
L_20914:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20914u)) {r_15=0x20914u;goto done;}
#endif
/* 20914: vmov r2, s14 */
{
r_2 = m8_fp_bits32(s_14);
}
goto L_20918;
L_20918:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20918u)) {r_15=0x20918u;goto done;}
#endif
/* 20918: uxth r0, r2 */
{
{
uint32_t value = r_2;
r_0 = (uint32_t)(value & 65535u);
}
}
goto L_2091a;
L_2091a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2091au)) {r_15=0x2091au;goto done;}
#endif
/* 2091a: lsls r0, r0, #1 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t result = m8_shift(r_0, 2, 0x1u, &carry);
m8_logic_flags(cpu, result, carry);
r_0 = (uint32_t)(result);
}
}
goto L_2091c;
L_2091c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2091cu)) {r_15=0x2091cu;goto done;}
#endif
/* 2091c: vldr s14, [r4, #0x48] */
{
{
uint32_t fp_addr = (r_4 + 72u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_20920;
L_20920:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20920u)) {r_15=0x20920u;goto done;}
#endif
/* 20920: vmov r2, s15 */
{
r_2 = m8_fp_bits32(s_15);
}
goto L_20924;
L_20924:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20924u)) {r_15=0x20924u;goto done;}
#endif
/* 20924: ldr r1, [r4, #0x58] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (88u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_1 = (uint32_t)(value);
}
}
goto L_20926;
L_20926:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20926u)) {r_15=0x20926u;goto done;}
#endif
/* 20926: vadd.f32 s14, s14, s13 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));
}
goto L_2092a;
L_2092a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2092au)) {r_15=0x2092au;goto done;}
#endif
/* 2092a: strh r2, [r1, r0] */
{
{
uint32_t base = r_1;
uint32_t addr = base + ((0u + r_0));
uint32_t value = r_2;
m8_le_write(M8_OV_RING(addr),2,value);
}
}
goto L_2092c;
L_2092c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2092cu)) {r_15=0x2092cu;goto done;}
#endif
/* 2092c: vstr s14, [r4, #0x44] */
{
{
uint32_t fp_addr = (r_4 + 68u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_14));
}
}
goto L_20930;
L_20930:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20930u)) {r_15=0x20930u;goto done;}
#endif
/* 20930: vldr s15, [pc, #0x74] */
{
{
uint32_t fp_addr = (0x20934u + 116u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_20934;
L_20934:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20934u)) {r_15=0x20934u;goto done;}
#endif
/* 20934: vldr s14, [r3, #0x2c] */
{
{
uint32_t fp_addr = (r_3 + 44u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_20938;
L_20938:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20938u)) {r_15=0x20938u;goto done;}
#endif
/* 20938: vmul.f32 s15, s0, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_15), 0));
}
goto L_2093c;
L_2093c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2093cu)) {r_15=0x2093cu;goto done;}
#endif
/* 2093c: ldr r3, [sp, #0xc] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (12u);
uint32_t value;
value=m8_le_read(M8_OV_STACK(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_2093e;
L_2093e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2093eu)) {r_15=0x2093eu;goto done;}
#endif
/* 2093e: vmul.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_20942;
L_20942:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20942u)) {r_15=0x20942u;goto done;}
#endif
/* 20942: vmul.f32 s24, s15, s24 */
{
s_24=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_24), 0));
}
goto L_20946;
L_20946:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20946u)) {r_15=0x20946u;goto done;}
#endif
/* 20946: vmul.f32 s15, s15, s23 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_23), 0));
}
goto L_2094a;
L_2094a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2094au)) {r_15=0x2094au;goto done;}
#endif
/* 2094a: vstmia r7!, {s24} */
{
{
uint32_t fp_addr = r_7;
uint32_t fp_value;
m8_le_write(M8_OV_LEFT(fp_addr + 0u),4,m8_fp_bits32(s_24));
r_7 += 4u;
}
}
goto L_2094e;
L_2094e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2094eu)) {r_15=0x2094eu;goto done;}
#endif
/* 2094e: cmp r3, r7 */
{
{
uint32_t a = r_3;
uint32_t b = r_7;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20950;
L_20950:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20950u)) {r_15=0x20950u;goto done;}
#endif
/* 20950: vstmia sb!, {s15} */
{
{
uint32_t fp_addr = r_9;
uint32_t fp_value;
m8_le_write(M8_OV_RIGHT(fp_addr + 0u),4,m8_fp_bits32(s_15));
r_9 += 4u;
}
}
goto L_20954;
L_20954:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20954u)) {r_15=0x20954u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 20954: bne.w #0x206b6 */
{
goto L_206b6;
}
}
goto L_20958;
L_20958:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20958u)) {r_15=0x20958u;goto done;}
#endif
/* 20958: movs r0, #1 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x1u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_0 = (uint32_t)(result);
}
}
goto L_2095a;
L_2095a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2095au)) {r_15=0x2095au;goto done;}
#endif
/* 2095a: add sp, #0x14 */
{
{
uint32_t a = r_13;
uint32_t b = 0x14u;
uint32_t result = a + b + 0u;
r_13 = (uint32_t)(result);
}
}
goto L_2095c;
L_2095c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2095cu)) {r_15=0x2095cu;goto done;}
#endif
/* 2095c: vpop {d8, d9, d10, d11, d12} */
{
{
uint32_t fp_addr = r_13;
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 0u),4);
s_16=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 4u),4);
s_17=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 8u),4);
s_18=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 12u),4);
s_19=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 16u),4);
s_20=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 20u),4);
s_21=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 24u),4);
s_22=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 28u),4);
s_23=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 32u),4);
s_24=m8_fp_float(fp_value);
fp_value=m8_le_read(M8_OV_STACK(fp_addr + 36u),4);
s_25=m8_fp_float(fp_value);
r_13 += 40u;
}
}
goto L_20960;
L_20960:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20960u)) {r_15=0x20960u;goto done;}
#endif
/* 20960: pop.w {r4, r5, r6, r7, r8, sb, sl, fp, pc} */
{
{
uint32_t base = r_13;
uint32_t addr = base + 0u;
uint32_t value0;
value0=m8_le_read(M8_OV_STACK(addr + 0u),4);
uint32_t value1;
value1=m8_le_read(M8_OV_STACK(addr + 4u),4);
uint32_t value2;
value2=m8_le_read(M8_OV_STACK(addr + 8u),4);
uint32_t value3;
value3=m8_le_read(M8_OV_STACK(addr + 12u),4);
uint32_t value4;
value4=m8_le_read(M8_OV_STACK(addr + 16u),4);
uint32_t value5;
value5=m8_le_read(M8_OV_STACK(addr + 20u),4);
uint32_t value6;
value6=m8_le_read(M8_OV_STACK(addr + 24u),4);
uint32_t value7;
value7=m8_le_read(M8_OV_STACK(addr + 28u),4);
uint32_t value8;
value8=m8_le_read(M8_OV_STACK(addr + 32u),4);
r_13 = base + 36u;
r_4 = (uint32_t)(value0);
r_5 = (uint32_t)(value1);
r_6 = (uint32_t)(value2);
r_7 = (uint32_t)(value3);
r_8 = (uint32_t)(value4);
r_9 = (uint32_t)(value5);
r_10 = (uint32_t)(value6);
r_11 = (uint32_t)(value7);
r_15 = (value8) & ~1u;
goto done;
}
}
goto L_20964;
L_20964:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20964u)) {r_15=0x20964u;goto done;}
#endif
/* 20964: movs r6, #2 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x2u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_6 = (uint32_t)(result);
}
}
goto L_20966;
L_20966:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20966u)) {r_15=0x20966u;goto done;}
#endif
/* 20966: b #0x20664 */
{
goto L_20664;
}
goto L_20968;
L_20968:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20968u)) {r_15=0x20968u;goto done;}
#endif
/* 20968: vadd.f32 s23, s23, s23 */
{
s_23=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_23), m8_fp_bits32(s_23), 0));
}
goto L_2096c;
L_2096c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2096cu)) {r_15=0x2096cu;goto done;}
#endif
/* 2096c: vmov.f32 s24, #1.000000e+00 */
{
s_24=m8_fp_float(0x3f800000u);
}
goto L_20970;
L_20970:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20970u)) {r_15=0x20970u;goto done;}
#endif
/* 20970: b #0x207b6 */
{
goto L_207b6;
}
goto L_20972;
L_20972:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20972u)) {r_15=0x20972u;goto done;}
#endif
/* 20972: mov.w sl, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
r_10 = (uint32_t)(result);
}
}
goto L_20976;
L_20976:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20976u)) {r_15=0x20976u;goto done;}
#endif
/* 20976: b #0x206ec */
{
goto L_206ec;
}
goto L_20978;
L_20978:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20978u)) {r_15=0x20978u;goto done;}
#endif
/* 20978: vmov.f32 s12, s17 */
{
s_12=m8_fp_float(m8_fp_bits32(s_17));
}
goto L_2097c;
L_2097c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x2097cu)) {r_15=0x2097cu;goto done;}
#endif
/* 2097c: vcvt.f32.s32 s14, s14 */
{
s_14=m8_fp_float(m8_oscillator_voice_from_int(cpu, m8_fp_bits32(s_14), 0, 0, 32, 0, 0));
}
goto L_20980;
L_20980:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20980u)) {r_15=0x20980u;goto done;}
#endif
/* 20980: b #0x208b0 */
{
goto L_208b0;
}
goto L_20982;
L_20982:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20982u)) {r_15=0x20982u;goto done;}
#endif
/* 20982: vmov.f32 s13, s22 */
{
s_13=m8_fp_float(m8_fp_bits32(s_22));
}
goto L_20986;
L_20986:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20986u)) {r_15=0x20986u;goto done;}
#endif
/* 20986: movs r0, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_0 = (uint32_t)(result);
}
}
goto L_20988;
L_20988:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20988u)) {r_15=0x20988u;goto done;}
#endif
/* 20988: b #0x2091c */
{
goto L_2091c;
}
r_15=0x2098au;goto done;
L_209d4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209d4u)) {r_15=0x209d4u;goto done;}
#endif
/* 209d4: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_209d6;
L_209d6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209d6u)) {r_15=0x209d6u;goto done;}
#endif
/* 209d6: vmov.f32 s15, #1.000000e+00 */
{
s_15=m8_fp_float(0x3f800000u);
}
goto L_209da;
L_209da:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209dau)) {r_15=0x209dau;goto done;}
#endif
/* 209da: cmp r5, #5 */
{
{
uint32_t a = r_5;
uint32_t b = 0x5u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_209dc;
L_209dc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209dcu)) {r_15=0x209dcu;goto done;}
#endif
/* 209dc: vldr s13, [r3, #0x44] */
{
{
uint32_t fp_addr = (r_3 + 68u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_13=m8_fp_float(fp_value);
}
}
goto L_209e0;
L_209e0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209e0u)) {r_15=0x209e0u;goto done;}
#endif
/* 209e0: vldr s10, [r3, #0x48] */
{
{
uint32_t fp_addr = (r_3 + 72u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_10=m8_fp_float(fp_value);
}
}
goto L_209e4;
L_209e4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209e4u)) {r_15=0x209e4u;goto done;}
#endif
/* 209e4: vminnm.f32 s13, s13, s15 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MINNM, m8_fp_bits32(s_13), m8_fp_bits32(s_15), 0));
}
goto L_209e8;
L_209e8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209e8u)) {r_15=0x209e8u;goto done;}
#endif
/* 209e8: vmaxnm.f32 s13, s13, s22 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_13), m8_fp_bits32(s_22), 0));
}
goto L_209ec;
L_209ec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209ecu)) {r_15=0x209ecu;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 209ec: beq.w #0x20c1c */
{
goto L_20c1c;
}
}
goto L_209f0;
L_209f0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209f0u)) {r_15=0x209f0u;goto done;}
#endif
/* 209f0: vldr s7, [pc, #-0x48] */
{
{
uint32_t fp_addr = (0x209f4u - 72u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_7=m8_fp_float(fp_value);
}
}
goto L_209f4;
L_209f4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209f4u)) {r_15=0x209f4u;goto done;}
#endif
/* 209f4: vldr s15, [r4, #0x24] */
{
{
uint32_t fp_addr = (r_4 + 36u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_209f8;
L_209f8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209f8u)) {r_15=0x209f8u;goto done;}
#endif
/* 209f8: vmov.f32 s12, #1.000000e+00 */
{
s_12=m8_fp_float(0x3f800000u);
}
goto L_209fc;
L_209fc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x209fcu)) {r_15=0x209fcu;goto done;}
#endif
/* 209fc: vldr s14, [r4, #0x20] */
{
{
uint32_t fp_addr = (r_4 + 32u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_20a00;
L_20a00:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a00u)) {r_15=0x20a00u;goto done;}
#endif
/* 20a00: cmp r5, #1 */
{
{
uint32_t a = r_5;
uint32_t b = 0x1u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20a02;
L_20a02:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a02u)) {r_15=0x20a02u;goto done;}
#endif
/* 20a02: vsub.f32 s11, s0, s15 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_0), m8_fp_bits32(s_15), 0));
}
goto L_20a06;
L_20a06:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a06u)) {r_15=0x20a06u;goto done;}
#endif
/* 20a06: vldr s8, [pc, #-0x58] */
{
{
uint32_t fp_addr = (0x20a08u - 88u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_8=m8_fp_float(fp_value);
}
}
goto L_20a0a;
L_20a0a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a0au)) {r_15=0x20a0au;goto done;}
#endif
/* 20a0a: vldr s9, [pc, #-0x58] */
{
{
uint32_t fp_addr = (0x20a0cu - 88u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_9=m8_fp_float(fp_value);
}
}
goto L_20a0e;
L_20a0e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a0eu)) {r_15=0x20a0eu;goto done;}
#endif
/* 20a0e: vfms.f32 s11, s14, s10 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, (m8_fp_bits32(s_14) ^ 0x80000000u), m8_fp_bits32(s_10), m8_fp_bits32(s_11)));
}
goto L_20a12;
L_20a12:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a12u)) {r_15=0x20a12u;goto done;}
#endif
/* 20a12: vfma.f32 s14, s13, s11 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_11), m8_fp_bits32(s_14)));
}
goto L_20a16;
L_20a16:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a16u)) {r_15=0x20a16u;goto done;}
#endif
/* 20a16: vmov.f32 s11, s0 */
{
s_11=m8_fp_float(m8_fp_bits32(s_0));
}
goto L_20a1a;
L_20a1a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a1au)) {r_15=0x20a1au;goto done;}
#endif
/* 20a1a: vfma.f32 s15, s13, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));
}
goto L_20a1e;
L_20a1e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a1eu)) {r_15=0x20a1eu;goto done;}
#endif
/* 20a1e: vfms.f32 s11, s10, s14 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, (m8_fp_bits32(s_10) ^ 0x80000000u), m8_fp_bits32(s_14), m8_fp_bits32(s_11)));
}
goto L_20a22;
L_20a22:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a22u)) {r_15=0x20a22u;goto done;}
#endif
/* 20a22: vsub.f32 s11, s11, s15 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_11), m8_fp_bits32(s_15), 0));
}
goto L_20a26;
L_20a26:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a26u)) {r_15=0x20a26u;goto done;}
#endif
/* 20a26: vfma.f32 s14, s13, s11 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_11), m8_fp_bits32(s_14)));
}
goto L_20a2a;
L_20a2a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a2au)) {r_15=0x20a2au;goto done;}
#endif
/* 20a2a: vfma.f32 s15, s13, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));
}
goto L_20a2e;
L_20a2e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a2eu)) {r_15=0x20a2eu;goto done;}
#endif
/* 20a2e: vmaxnm.f32 s14, s14, s8 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_14), m8_fp_bits32(s_8), 0));
}
goto L_20a32;
L_20a32:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a32u)) {r_15=0x20a32u;goto done;}
#endif
/* 20a32: vmov.f32 s13, s12 */
{
s_13=m8_fp_float(m8_fp_bits32(s_12));
}
goto L_20a36;
L_20a36:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a36u)) {r_15=0x20a36u;goto done;}
#endif
/* 20a36: vminnm.f32 s14, s14, s17 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MINNM, m8_fp_bits32(s_14), m8_fp_bits32(s_17), 0));
}
goto L_20a3a;
L_20a3a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a3au)) {r_15=0x20a3au;goto done;}
#endif
/* 20a3a: vabs.f32 s10, s14 */
{
s_10=m8_fp_float((m8_fp_bits32(s_14) & ~0x80000000u));
}
goto L_20a3e;
L_20a3e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a3eu)) {r_15=0x20a3eu;goto done;}
#endif
/* 20a3e: vfms.f32 s13, s10, s9 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, (m8_fp_bits32(s_10) ^ 0x80000000u), m8_fp_bits32(s_9), m8_fp_bits32(s_13)));
}
goto L_20a42;
L_20a42:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a42u)) {r_15=0x20a42u;goto done;}
#endif
/* 20a42: vmaxnm.f32 s15, s15, s8 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_15), m8_fp_bits32(s_8), 0));
}
goto L_20a46;
L_20a46:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a46u)) {r_15=0x20a46u;goto done;}
#endif
/* 20a46: vminnm.f32 s15, s15, s17 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MINNM, m8_fp_bits32(s_15), m8_fp_bits32(s_17), 0));
}
goto L_20a4a;
L_20a4a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a4au)) {r_15=0x20a4au;goto done;}
#endif
/* 20a4a: vabs.f32 s10, s15 */
{
s_10=m8_fp_float((m8_fp_bits32(s_15) & ~0x80000000u));
}
goto L_20a4e;
L_20a4e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a4eu)) {r_15=0x20a4eu;goto done;}
#endif
/* 20a4e: vfms.f32 s12, s10, s9 */
{
s_12=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, (m8_fp_bits32(s_10) ^ 0x80000000u), m8_fp_bits32(s_9), m8_fp_bits32(s_12)));
}
goto L_20a52;
L_20a52:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a52u)) {r_15=0x20a52u;goto done;}
#endif
/* 20a52: vmul.f32 s14, s14, s13 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));
}
goto L_20a56;
L_20a56:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a56u)) {r_15=0x20a56u;goto done;}
#endif
/* 20a56: vstr s14, [r4, #0x20] */
{
{
uint32_t fp_addr = (r_4 + 32u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_14));
}
}
goto L_20a5a;
L_20a5a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a5au)) {r_15=0x20a5au;goto done;}
#endif
/* 20a5a: vmul.f32 s15, s15, s12 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_12), 0));
}
goto L_20a5e;
L_20a5e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a5eu)) {r_15=0x20a5eu;goto done;}
#endif
/* 20a5e: vstr s15, [r4, #0x24] */
{
{
uint32_t fp_addr = (r_4 + 36u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_20a62;
L_20a62:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a62u)) {r_15=0x20a62u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20a62: beq #0x20a78 */
{
goto L_20a78;
}
}
goto L_20a64;
L_20a64:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a64u)) {r_15=0x20a64u;goto done;}
#endif
/* 20a64: cmp r5, #2 */
{
{
uint32_t a = r_5;
uint32_t b = 0x2u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20a66;
L_20a66:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a66u)) {r_15=0x20a66u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20a66: beq.w #0x20c4e */
{
goto L_20c4e;
}
}
goto L_20a6a;
L_20a6a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a6au)) {r_15=0x20a6au;goto done;}
#endif
/* 20a6a: cmp r5, #3 */
{
{
uint32_t a = r_5;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20a6c;
L_20a6c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a6cu)) {r_15=0x20a6cu;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20a6c: beq.w #0x20c66 */
{
goto L_20c66;
}
}
goto L_20a70;
L_20a70:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a70u)) {r_15=0x20a70u;goto done;}
#endif
/* 20a70: cmp r5, #4 */
{
{
uint32_t a = r_5;
uint32_t b = 0x4u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20a72;
L_20a72:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a72u)) {r_15=0x20a72u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 20a72: bne #0x20a78 */
{
goto L_20a78;
}
}
goto L_20a74;
L_20a74:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a74u)) {r_15=0x20a74u;goto done;}
#endif
/* 20a74: vsub.f32 s15, s0, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_0), m8_fp_bits32(s_14), 0));
}
goto L_20a78;
L_20a78:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a78u)) {r_15=0x20a78u;goto done;}
#endif
/* 20a78: vldr s0, [r4, #0x28] */
{
{
uint32_t fp_addr = (r_4 + 40u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_0=m8_fp_float(fp_value);
}
}
goto L_20a7c;
L_20a7c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a7cu)) {r_15=0x20a7cu;goto done;}
#endif
/* 20a7c: vldr s14, [r4, #0x2c] */
{
{
uint32_t fp_addr = (r_4 + 44u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_20a80;
L_20a80:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a80u)) {r_15=0x20a80u;goto done;}
#endif
/* 20a80: ldrb.w r1, [r4, #0x41] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (65u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_20a84;
L_20a84:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a84u)) {r_15=0x20a84u;goto done;}
#endif
/* 20a84: vfnms.f32 s0, s7, s14 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_7), m8_fp_bits32(s_14), (m8_fp_bits32(s_0) ^ 0x80000000u)));
}
goto L_20a88;
L_20a88:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a88u)) {r_15=0x20a88u;goto done;}
#endif
/* 20a88: vstr s15, [r4, #0x28] */
{
{
uint32_t fp_addr = (r_4 + 40u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_15));
}
}
goto L_20a8c;
L_20a8c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a8cu)) {r_15=0x20a8cu;goto done;}
#endif
/* 20a8c: cmp r1, #3 */
{
{
uint32_t a = r_1;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20a8e;
L_20a8e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a8eu)) {r_15=0x20a8eu;goto done;}
#endif
/* 20a8e: vadd.f32 s0, s15, s0 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_0), 0));
}
goto L_20a92;
L_20a92:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a92u)) {r_15=0x20a92u;goto done;}
#endif
/* 20a92: vstr s0, [r4, #0x2c] */
{
{
uint32_t fp_addr = (r_4 + 44u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_0));
}
}
goto L_20a96;
L_20a96:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a96u)) {r_15=0x20a96u;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 20a96: bls.w #0x20c00 */
{
goto L_20c00;
}
}
goto L_20a9a;
L_20a9a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a9au)) {r_15=0x20a9au;goto done;}
#endif
/* 20a9a: ldr r0, [r4, #0x50] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (80u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_20a9c;
L_20a9c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20a9cu)) {r_15=0x20a9cu;goto done;}
#endif
/* 20a9c: vldr s1, [r3, #0x4c] */
{
{
uint32_t fp_addr = (r_3 + 76u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_1=m8_fp_float(fp_value);
}
}
goto L_20aa0;
L_20aa0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aa0u)) {r_15=0x20aa0u;goto done;}
#endif
/* 20aa0: ldr r3, [r0] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_SLOT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20aa2;
L_20aa2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aa2u)) {r_15=0x20aa2u;goto done;}
#endif
/* 20aa2: ldr r3, [r3] */
{
{
uint32_t base = r_3;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_TARGET(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20aa4;
L_20aa4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aa4u)) {r_15=0x20aa4u;goto done;}
#endif
/* 20aa4: blx r3 */
{
if(r_3!=0x1fa5u && r_3!=0x1fedu) {r_15=0x20aa4u;goto done;}
r_14=0x20aa7u;
r_15=r_3&~1u;
if(r_3==0x1fa5u) goto L_1fa4;
goto L_1fec;
}
goto L_20aa6;
L_20aa6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aa6u)) {r_15=0x20aa6u;goto done;}
#endif
/* 20aa6: cmp r5, #0 */
{
{
uint32_t a = r_5;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20aa8;
L_20aa8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aa8u)) {r_15=0x20aa8u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20aa8: beq.w #0x208f0 */
{
goto L_208f0;
}
}
goto L_20aac;
L_20aac:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aacu)) {r_15=0x20aacu;goto done;}
#endif
/* 20aac: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20aae;
L_20aae:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aaeu)) {r_15=0x20aaeu;goto done;}
#endif
/* 20aae: b #0x208fa */
{
goto L_208fa;
}
goto L_20ab0;
L_20ab0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ab0u)) {r_15=0x20ab0u;goto done;}
#endif
/* 20ab0: mov r0, r4 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_4;
uint32_t result = b;
r_0 = (uint32_t)(result);
}
}
goto L_20ab2;
L_20ab2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ab2u)) {r_15=0x20ab2u;goto done;}
#endif
/* 20ab2: bl #0x2003c */
{
r_14=0x20ab7u;r_15=0x2003cu;
M8_OV_SAVE(cpu);int status=update(cpu,mem);
if(status!=M8_AOT_EXIT || M8_OV_CHILD_PC()!=0x20ab6u) {M8_OV_CHILD_RETURN(status);}
M8_OV_LOAD(cpu);
if(r_4!=root || r_13!=frame || r_7<left || r_7>=left_end ||
   ((r_7-left)&3u) || r_9!=right+(r_7-left) ||
   m8_oscillator_voice_memory_offset(mem)!=arena ||
   m8_le_read(M8_OV_HOST(frame+12u),4)!=left_end ||
   !m8_oscillator_voice_qualify(cpu,mem,root,r_7,r_9,
       (left_end-r_7)/4u,frame,r_11)) {
    r_15=0x20ab6u;goto done;}
#if defined(M8_AOT_OSCILLATOR_SPANS)
M8_OV_REFRESH_SPANS(r_11);
#endif
}
goto L_20ab6;
L_20ab6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ab6u)) {r_15=0x20ab6u;goto done;}
#endif
/* 20ab6: ldrb.w r5, [r4, #0x40] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (64u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_5 = (uint32_t)(value);
}
}
goto L_20aba;
L_20aba:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20abau)) {r_15=0x20abau;goto done;}
#endif
/* 20aba: cmp r5, #7 */
{
{
uint32_t a = r_5;
uint32_t b = 0x7u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20abc;
L_20abc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20abcu)) {r_15=0x20abcu;goto done;}
#endif
if(m8_condition(cpu->xpsr,9)) {
/* 20abc: bhi.w #0x20c08 */
{
goto L_20c08;
}
}
goto L_20ac0;
L_20ac0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ac0u)) {r_15=0x20ac0u;goto done;}
#endif
/* 20ac0: add.w r8, r5, #-1 */
{
{
uint32_t a = r_5;
uint32_t b = 0xffffffffu;
uint32_t result = a + b + 0u;
r_8 = (uint32_t)(result);
}
}
goto L_20ac4;
L_20ac4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ac4u)) {r_15=0x20ac4u;goto done;}
#endif
/* 20ac4: uxtb.w r3, r8 */
{
{
uint32_t value = r_8;
r_3 = (uint32_t)(value & 255u);
}
}
goto L_20ac8;
L_20ac8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ac8u)) {r_15=0x20ac8u;goto done;}
#endif
/* 20ac8: str r3, [sp, #8] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (8u);
uint32_t value = r_3;
m8_le_write(M8_OV_STACK(addr),4,value);
}
}
goto L_20aca;
L_20aca:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20acau)) {r_15=0x20acau;goto done;}
#endif
/* 20aca: ldrb.w r3, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20ace;
L_20ace:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aceu)) {r_15=0x20aceu;goto done;}
#endif
/* 20ace: cmp r3, #8 */
{
{
uint32_t a = r_3;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20ad0;
L_20ad0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ad0u)) {r_15=0x20ad0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20ad0: beq.w #0x20c18 */
{
goto L_20c18;
}
}
goto L_20ad4;
L_20ad4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ad4u)) {r_15=0x20ad4u;goto done;}
#endif
/* 20ad4: ldrh.w r6, [r4, #0x98] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (152u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),2);
r_6 = (uint32_t)(value);
}
}
goto L_20ad8;
L_20ad8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ad8u)) {r_15=0x20ad8u;goto done;}
#endif
/* 20ad8: cmp r6, #1 */
{
{
uint32_t a = r_6;
uint32_t b = 0x1u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20ada;
L_20ada:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20adau)) {r_15=0x20adau;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 20ada: bls.w #0x20c3e */
{
goto L_20c3e;
}
}
goto L_20ade;
L_20ade:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20adeu)) {r_15=0x20adeu;goto done;}
#endif
/* 20ade: cmp r6, #0xff */
{
{
uint32_t a = r_6;
uint32_t b = 0xffu;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20ae0;
L_20ae0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ae0u)) {r_15=0x20ae0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,3)) {
/* 20ae2: movhs r6, #0xff */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0xffu;
uint32_t result = b;
r_6 = (uint32_t)(result);
}
}
}
goto L_20ae4;
L_20ae4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ae4u)) {r_15=0x20ae4u;goto done;}
#endif
/* 20ae4: uxtb r6, r6 */
{
{
uint32_t value = r_6;
r_6 = (uint32_t)(value & 255u);
}
}
goto L_20ae6;
L_20ae6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ae6u)) {r_15=0x20ae6u;goto done;}
#endif
/* 20ae6: ldrb.w r3, [r4, #0x6d] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (109u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20aea;
L_20aea:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aeau)) {r_15=0x20aeau;goto done;}
#endif
/* 20aea: adds r3, #1 */
{
{
uint32_t a = r_3;
uint32_t b = 0x1u;
uint32_t result = m8_add_flags(cpu, a, b, 0u);
r_3 = (uint32_t)(result);
}
}
goto L_20aec;
L_20aec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aecu)) {r_15=0x20aecu;goto done;}
#endif
/* 20aec: uxtb r3, r3 */
{
{
uint32_t value = r_3;
r_3 = (uint32_t)(value & 255u);
}
}
goto L_20aee;
L_20aee:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20aeeu)) {r_15=0x20aeeu;goto done;}
#endif
/* 20aee: cmp r3, #7 */
{
{
uint32_t a = r_3;
uint32_t b = 0x7u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20af0;
L_20af0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20af0u)) {r_15=0x20af0u;goto done;}
#endif
if(m8_condition(cpu->xpsr,9)) {
/* 20af0: bhi #0x20bf2 */
{
goto L_20bf2;
}
}
goto L_20af2;
L_20af2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20af2u)) {r_15=0x20af2u;goto done;}
#endif
/* 20af2: strb.w r3, [r4, #0x6d] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (109u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_20af6;
L_20af6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20af6u)) {r_15=0x20af6u;goto done;}
#endif
/* 20af6: cmp r3, #0 */
{
{
uint32_t a = r_3;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20af8;
L_20af8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20af8u)) {r_15=0x20af8u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20af8: beq #0x20bf8 */
{
goto L_20bf8;
}
}
goto L_20afa;
L_20afa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20afau)) {r_15=0x20afau;goto done;}
#endif
/* 20afa: ldrb.w sl, [r4, #0x6c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (108u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_10 = (uint32_t)(value);
}
}
goto L_20afe;
L_20afe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20afeu)) {r_15=0x20afeu;goto done;}
#endif
/* 20afe: b #0x206cc */
{
goto L_206cc;
}
goto L_20b00;
L_20b00:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b00u)) {r_15=0x20b00u;goto done;}
#endif
/* 20b00: vmov.f32 s14, s17 */
{
s_14=m8_fp_float(m8_fp_bits32(s_17));
}
goto L_20b04;
L_20b04:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b04u)) {r_15=0x20b04u;goto done;}
#endif
/* 20b04: b #0x20800 */
{
goto L_20800;
}
goto L_20b06;
L_20b06:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b06u)) {r_15=0x20b06u;goto done;}
#endif
/* 20b06: ldr r3, [r4, #0x64] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (100u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20b08;
L_20b08:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b08u)) {r_15=0x20b08u;goto done;}
#endif
/* 20b08: vmov.f32 s13, #1.000000e+00 */
{
s_13=m8_fp_float(0x3f800000u);
}
goto L_20b0c;
L_20b0c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b0cu)) {r_15=0x20b0cu;goto done;}
#endif
/* 20b0c: vldr s12, [pc, #-0x158] */
{
{
uint32_t fp_addr = (0x20b10u - 344u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_12=m8_fp_float(fp_value);
}
}
goto L_20b10;
L_20b10:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b10u)) {r_15=0x20b10u;goto done;}
#endif
/* 20b10: cmp r5, #6 */
{
{
uint32_t a = r_5;
uint32_t b = 0x6u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20b12;
L_20b12:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b12u)) {r_15=0x20b12u;goto done;}
#endif
/* 20b12: vldr s14, [r3, #0x44] */
{
{
uint32_t fp_addr = (r_3 + 68u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_14=m8_fp_float(fp_value);
}
}
goto L_20b16;
L_20b16:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b16u)) {r_15=0x20b16u;goto done;}
#endif
/* 20b16: vldr s15, [pc, #-0x15c] */
{
{
uint32_t fp_addr = (0x20b18u - 348u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_15=m8_fp_float(fp_value);
}
}
goto L_20b1a;
L_20b1a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b1au)) {r_15=0x20b1au;goto done;}
#endif
/* 20b1a: vmaxnm.f32 s14, s14, s12 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_14), m8_fp_bits32(s_12), 0));
}
goto L_20b1e;
L_20b1e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b1eu)) {r_15=0x20b1eu;goto done;}
#endif
/* 20b1e: vldr s6, [pc, #-0x160] */
{
{
uint32_t fp_addr = (0x20b20u - 352u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_6=m8_fp_float(fp_value);
}
}
goto L_20b22;
L_20b22:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b22u)) {r_15=0x20b22u;goto done;}
#endif
/* 20b22: vmul.f32 s15, s14, s15 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));
}
goto L_20b26;
L_20b26:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b26u)) {r_15=0x20b26u;goto done;}
#endif
/* 20b26: vldr s8, [pc, #-0x164] */
{
{
uint32_t fp_addr = (0x20b28u - 356u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_8=m8_fp_float(fp_value);
}
}
goto L_20b2a;
L_20b2a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b2au)) {r_15=0x20b2au;goto done;}
#endif
/* 20b2a: vldr s11, [r4, #0x28] */
{
{
uint32_t fp_addr = (r_4 + 40u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_11=m8_fp_float(fp_value);
}
}
goto L_20b2e;
L_20b2e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b2eu)) {r_15=0x20b2eu;goto done;}
#endif
/* 20b2e: vldr s12, [r4, #0x20] */
{
{
uint32_t fp_addr = (r_4 + 32u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_12=m8_fp_float(fp_value);
}
}
goto L_20b32;
L_20b32:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b32u)) {r_15=0x20b32u;goto done;}
#endif
/* 20b32: vcvt.f64.f32 d7, s15 */
{
{uint64_t pair_value=m8_fp_convert(cpu, m8_fp_bits32(s_15), 0); s_14=m8_fp_float((uint32_t)pair_value); s_15=m8_fp_float((uint32_t)(pair_value >> 32));}
}
goto L_20b36;
L_20b36:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b36u)) {r_15=0x20b36u;goto done;}
#endif
/* 20b36: vldr s10, [r3, #0x48] */
{
{
uint32_t fp_addr = (r_3 + 72u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_10=m8_fp_float(fp_value);
}
}
goto L_20b3a;
L_20b3a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b3au)) {r_15=0x20b3au;goto done;}
#endif
/* 20b3a: vsub.f32 s7, s0, s11 */
{
s_7=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_0), m8_fp_bits32(s_11), 0));
}
goto L_20b3e;
L_20b3e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b3eu)) {r_15=0x20b3eu;goto done;}
#endif
/* 20b3e: vldr s5, [pc, #-0x178] */
{
{
uint32_t fp_addr = (0x20b40u - 376u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_5=m8_fp_float(fp_value);
}
}
goto L_20b42;
L_20b42:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b42u)) {r_15=0x20b42u;goto done;}
#endif
/* 20b42: vsub.f32 s10, s13, s10 */
{
s_10=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_13), m8_fp_bits32(s_10), 0));
}
goto L_20b46;
L_20b46:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b46u)) {r_15=0x20b46u;goto done;}
#endif
/* 20b46: vldr s9, [r4, #0x24] */
{
{
uint32_t fp_addr = (r_4 + 36u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_OBJECT(fp_addr + 0u),4);
s_9=m8_fp_float(fp_value);
}
}
goto L_20b4a;
L_20b4a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b4au)) {r_15=0x20b4au;goto done;}
#endif
/* 20b4a: vmul.f64 d7, d7, d9 */
{
{uint64_t pair_value=m8_fp_op64(cpu, M8_FP_MUL, ((uint64_t)m8_fp_bits32(s_14) | ((uint64_t)m8_fp_bits32(s_15) << 32)), ((uint64_t)m8_fp_bits32(s_18) | ((uint64_t)m8_fp_bits32(s_19) << 32)), 0); s_14=m8_fp_float((uint32_t)pair_value); s_15=m8_fp_float((uint32_t)(pair_value >> 32));}
}
goto L_20b4e;
L_20b4e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b4eu)) {r_15=0x20b4eu;goto done;}
#endif
/* 20b4e: vmul.f32 s10, s10, s5 */
{
s_10=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_10), m8_fp_bits32(s_5), 0));
}
goto L_20b52;
L_20b52:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b52u)) {r_15=0x20b52u;goto done;}
#endif
/* 20b52: vcvt.f32.f64 s14, d7 */
{
s_14=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_14) | ((uint64_t)m8_fp_bits32(s_15) << 32)), 1));
}
goto L_20b56;
L_20b56:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b56u)) {r_15=0x20b56u;goto done;}
#endif
/* 20b56: vfma.f32 s8, s14, s6 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_6), m8_fp_bits32(s_8)));
}
goto L_20b5a;
L_20b5a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b5au)) {r_15=0x20b5au;goto done;}
#endif
/* 20b5a: vmul.f32 s15, s14, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_14), 0));
}
goto L_20b5e;
L_20b5e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b5eu)) {r_15=0x20b5eu;goto done;}
#endif
/* 20b5e: vmul.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_20b62;
L_20b62:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b62u)) {r_15=0x20b62u;goto done;}
#endif
/* 20b62: vfma.f32 s14, s8, s15 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_8), m8_fp_bits32(s_15), m8_fp_bits32(s_14)));
}
goto L_20b66;
L_20b66:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b66u)) {r_15=0x20b66u;goto done;}
#endif
/* 20b66: vadd.f32 s8, s14, s13 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));
}
goto L_20b6a;
L_20b6a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b6au)) {r_15=0x20b6au;goto done;}
#endif
/* 20b6a: vdiv.f32 s15, s14, s8 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_DIV, m8_fp_bits32(s_14), m8_fp_bits32(s_8), 0));
}
goto L_20b6e;
L_20b6e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b6eu)) {r_15=0x20b6eu;goto done;}
#endif
/* 20b6e: vmov.f32 s14, s12 */
{
s_14=m8_fp_float(m8_fp_bits32(s_12));
}
goto L_20b72;
L_20b72:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b72u)) {r_15=0x20b72u;goto done;}
#endif
/* 20b72: vfma.f32 s14, s15, s7 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_7), m8_fp_bits32(s_14)));
}
goto L_20b76;
L_20b76:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b76u)) {r_15=0x20b76u;goto done;}
#endif
/* 20b76: vmul.f32 s6, s15, s15 */
{
s_6=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_20b7a;
L_20b7a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b7au)) {r_15=0x20b7au;goto done;}
#endif
/* 20b7a: vadd.f32 s5, s15, s15 */
{
s_5=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));
}
goto L_20b7e;
L_20b7e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b7eu)) {r_15=0x20b7eu;goto done;}
#endif
/* 20b7e: vsub.f32 s8, s6, s15 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_6), m8_fp_bits32(s_15), 0));
}
goto L_20b82;
L_20b82:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b82u)) {r_15=0x20b82u;goto done;}
#endif
/* 20b82: vadd.f32 s11, s11, s14 */
{
s_11=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_11), m8_fp_bits32(s_14), 0));
}
goto L_20b86;
L_20b86:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b86u)) {r_15=0x20b86u;goto done;}
#endif
/* 20b86: vsub.f32 s14, s7, s14 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_7), m8_fp_bits32(s_14), 0));
}
goto L_20b8a;
L_20b8a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b8au)) {r_15=0x20b8au;goto done;}
#endif
/* 20b8a: vfma.f32 s13, s10, s8 */
{
s_13=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_10), m8_fp_bits32(s_8), m8_fp_bits32(s_13)));
}
goto L_20b8e;
L_20b8e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b8eu)) {r_15=0x20b8eu;goto done;}
#endif
/* 20b8e: vsub.f32 s8, s11, s9 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_11), m8_fp_bits32(s_9), 0));
}
goto L_20b92;
L_20b92:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b92u)) {r_15=0x20b92u;goto done;}
#endif
/* 20b92: vfma.f32 s12, s5, s14 */
{
s_12=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_5), m8_fp_bits32(s_14), m8_fp_bits32(s_12)));
}
goto L_20b96;
L_20b96:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b96u)) {r_15=0x20b96u;goto done;}
#endif
/* 20b96: vmov.f32 s14, s9 */
{
s_14=m8_fp_float(m8_fp_bits32(s_9));
}
goto L_20b9a;
L_20b9a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b9au)) {r_15=0x20b9au;goto done;}
#endif
/* 20b9a: vfms.f32 s8, s15, s11 */
{
s_8=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, (m8_fp_bits32(s_15) ^ 0x80000000u), m8_fp_bits32(s_11), m8_fp_bits32(s_8)));
}
goto L_20b9e;
L_20b9e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20b9eu)) {r_15=0x20b9eu;goto done;}
#endif
/* 20b9e: vmul.f32 s15, s15, s12 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_12), 0));
}
goto L_20ba2;
L_20ba2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ba2u)) {r_15=0x20ba2u;goto done;}
#endif
/* 20ba2: vstr s12, [r4, #0x20] */
{
{
uint32_t fp_addr = (r_4 + 32u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_12));
}
}
goto L_20ba6;
L_20ba6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20ba6u)) {r_15=0x20ba6u;goto done;}
#endif
/* 20ba6: vfma.f32 s14, s5, s8 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_5), m8_fp_bits32(s_8), m8_fp_bits32(s_14)));
}
goto L_20baa;
L_20baa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20baau)) {r_15=0x20baau;goto done;}
#endif
/* 20baa: vfma.f32 s15, s6, s0 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_FMA, m8_fp_bits32(s_6), m8_fp_bits32(s_0), m8_fp_bits32(s_15)));
}
goto L_20bae;
L_20bae:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20baeu)) {r_15=0x20baeu;goto done;}
#endif
/* 20bae: vstr s14, [r4, #0x24] */
{
{
uint32_t fp_addr = (r_4 + 36u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_14));
}
}
goto L_20bb2;
L_20bb2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bb2u)) {r_15=0x20bb2u;goto done;}
#endif
/* 20bb2: vadd.f32 s15, s15, s14 */
{
s_15=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));
}
goto L_20bb6;
L_20bb6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bb6u)) {r_15=0x20bb6u;goto done;}
#endif
/* 20bb6: vdiv.f32 s14, s15, s13 */
{
s_14=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_DIV, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));
}
goto L_20bba;
L_20bba:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bbau)) {r_15=0x20bbau;goto done;}
#endif
/* 20bba: vmul.f32 s10, s10, s14 */
{
s_10=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_MUL, m8_fp_bits32(s_10), m8_fp_bits32(s_14), 0));
}
goto L_20bbe;
L_20bbe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bbeu)) {r_15=0x20bbeu;goto done;}
#endif
/* 20bbe: vstr s10, [r4, #0x28] */
{
{
uint32_t fp_addr = (r_4 + 40u);
uint32_t fp_value;
m8_le_write(M8_OV_OBJECT(fp_addr + 0u),4,m8_fp_bits32(s_10));
}
}
goto L_20bc2;
L_20bc2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bc2u)) {r_15=0x20bc2u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20bc2: beq #0x20c2a */
{
goto L_20c2a;
}
}
goto L_20bc4;
L_20bc4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bc4u)) {r_15=0x20bc4u;goto done;}
#endif
/* 20bc4: ldrb.w r1, [r4, #0x41] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (65u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_20bc8;
L_20bc8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bc8u)) {r_15=0x20bc8u;goto done;}
#endif
/* 20bc8: vsub.f32 s0, s14, s0 */
{
s_0=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_0), 0));
}
goto L_20bcc;
L_20bcc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bccu)) {r_15=0x20bccu;goto done;}
#endif
/* 20bcc: cmp r1, #3 */
{
{
uint32_t a = r_1;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20bce;
L_20bce:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bceu)) {r_15=0x20bceu;goto done;}
#endif
if(m8_condition(cpu->xpsr,10)) {
/* 20bce: bls.w #0x208fa */
{
goto L_208fa;
}
}
goto L_20bd2;
L_20bd2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bd2u)) {r_15=0x20bd2u;goto done;}
#endif
/* 20bd2: ldr r0, [r4, #0x50] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (80u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_20bd4;
L_20bd4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bd4u)) {r_15=0x20bd4u;goto done;}
#endif
/* 20bd4: vldr s1, [r3, #0x4c] */
{
{
uint32_t fp_addr = (r_3 + 76u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_1=m8_fp_float(fp_value);
}
}
goto L_20bd8;
L_20bd8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bd8u)) {r_15=0x20bd8u;goto done;}
#endif
/* 20bd8: ldr r3, [r0] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_SLOT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20bda;
L_20bda:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bdau)) {r_15=0x20bdau;goto done;}
#endif
/* 20bda: ldr r3, [r3] */
{
{
uint32_t base = r_3;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_TARGET(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20bdc;
L_20bdc:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bdcu)) {r_15=0x20bdcu;goto done;}
#endif
/* 20bdc: blx r3 */
{
if(r_3!=0x1fa5u && r_3!=0x1fedu) {r_15=0x20bdcu;goto done;}
r_14=0x20bdfu;
r_15=r_3&~1u;
if(r_3==0x1fa5u) goto L_1fa4;
goto L_1fec;
}
goto L_20bde;
L_20bde:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bdeu)) {r_15=0x20bdeu;goto done;}
#endif
/* 20bde: b #0x20aac */
{
goto L_20aac;
}
goto L_20be0;
L_20be0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20be0u)) {r_15=0x20be0u;goto done;}
#endif
/* 20be0: ldrb.w r3, [r4, #0x9c] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (156u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20be4;
L_20be4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20be4u)) {r_15=0x20be4u;goto done;}
#endif
/* 20be4: cbz r3, #0x20c42 */
{
if (r_3 == 0) {
goto L_20c42;
}
}
goto L_20be6;
L_20be6:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20be6u)) {r_15=0x20be6u;goto done;}
#endif
/* 20be6: ldrb.w r1, [r4, #0x9d] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (157u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_20bea;
L_20bea:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20beau)) {r_15=0x20beau;goto done;}
#endif
/* 20bea: cmp r1, #0 */
{
{
uint32_t a = r_1;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20bec;
L_20bec:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20becu)) {r_15=0x20becu;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20bec: beq.w #0x207d0 */
{
goto L_207d0;
}
}
goto L_20bf0;
L_20bf0:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bf0u)) {r_15=0x20bf0u;goto done;}
#endif
/* 20bf0: b #0x2080e */
{
goto L_2080e;
}
goto L_20bf2;
L_20bf2:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bf2u)) {r_15=0x20bf2u;goto done;}
#endif
/* 20bf2: movs r3, #0 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x0u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_3 = (uint32_t)(result);
}
}
goto L_20bf4;
L_20bf4:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bf4u)) {r_15=0x20bf4u;goto done;}
#endif
/* 20bf4: strb.w r3, [r4, #0x6d] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (109u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_20bf8;
L_20bf8:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bf8u)) {r_15=0x20bf8u;goto done;}
#endif
/* 20bf8: movs r3, #1 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x1u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_3 = (uint32_t)(result);
}
}
goto L_20bfa;
L_20bfa:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bfau)) {r_15=0x20bfau;goto done;}
#endif
/* 20bfa: strb.w r3, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value = r_3;
m8_le_write(M8_OV_OBJECT(addr),1,value);
}
}
goto L_20bfe;
L_20bfe:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20bfeu)) {r_15=0x20bfeu;goto done;}
#endif
/* 20bfe: b #0x20afa */
{
goto L_20afa;
}
goto L_20c00;
L_20c00:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c00u)) {r_15=0x20c00u;goto done;}
#endif
/* 20c00: cmp r5, #0 */
{
{
uint32_t a = r_5;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20c02;
L_20c02:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c02u)) {r_15=0x20c02u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 20c02: bne.w #0x208fa */
{
goto L_208fa;
}
}
goto L_20c06;
L_20c06:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c06u)) {r_15=0x20c06u;goto done;}
#endif
/* 20c06: b #0x20930 */
{
goto L_20930;
}
goto L_20c08;
L_20c08:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c08u)) {r_15=0x20c08u;goto done;}
#endif
/* 20c08: movs r3, #0xff */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0xffu;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_3 = (uint32_t)(result);
}
}
goto L_20c0a;
L_20c0a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c0au)) {r_15=0x20c0au;goto done;}
#endif
/* 20c0a: mov r5, sl */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = r_10;
uint32_t result = b;
r_5 = (uint32_t)(result);
}
}
goto L_20c0c;
L_20c0c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c0cu)) {r_15=0x20c0cu;goto done;}
#endif
/* 20c0c: str r3, [sp, #8] */
{
{
uint32_t base = r_13;
uint32_t addr = base + (8u);
uint32_t value = r_3;
m8_le_write(M8_OV_STACK(addr),4,value);
}
}
goto L_20c0e;
L_20c0e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c0eu)) {r_15=0x20c0eu;goto done;}
#endif
/* 20c0e: ldrb.w r3, [r4, #0x94] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (148u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_3 = (uint32_t)(value);
}
}
goto L_20c12;
L_20c12:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c12u)) {r_15=0x20c12u;goto done;}
#endif
/* 20c12: cmp r3, #8 */
{
{
uint32_t a = r_3;
uint32_t b = 0x8u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20c14;
L_20c14:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c14u)) {r_15=0x20c14u;goto done;}
#endif
if(m8_condition(cpu->xpsr,2)) {
/* 20c14: bne.w #0x20ad4 */
{
goto L_20ad4;
}
}
goto L_20c18;
L_20c18:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c18u)) {r_15=0x20c18u;goto done;}
#endif
/* 20c18: movs r6, #0xff */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0xffu;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_6 = (uint32_t)(result);
}
}
goto L_20c1a;
L_20c1a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c1au)) {r_15=0x20c1au;goto done;}
#endif
/* 20c1a: b #0x20ae6 */
{
goto L_20ae6;
}
goto L_20c1c;
L_20c1c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c1cu)) {r_15=0x20c1cu;goto done;}
#endif
/* 20c1c: vldr s7, [pc, #-0x254] */
{
{
uint32_t fp_addr = (0x20c20u - 596u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_7=m8_fp_float(fp_value);
}
}
goto L_20c20;
L_20c20:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c20u)) {r_15=0x20c20u;goto done;}
#endif
/* 20c20: vsub.f32 s7, s10, s7 */
{
s_7=m8_fp_float(m8_oscillator_voice_op32(cpu, M8_FP_SUB, m8_fp_bits32(s_10), m8_fp_bits32(s_7), 0));
}
goto L_20c24;
L_20c24:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c24u)) {r_15=0x20c24u;goto done;}
#endif
/* 20c24: vmov.f32 s10, s15 */
{
s_10=m8_fp_float(m8_fp_bits32(s_15));
}
goto L_20c28;
L_20c28:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c28u)) {r_15=0x20c28u;goto done;}
#endif
/* 20c28: b #0x209f4 */
{
goto L_209f4;
}
goto L_20c2a;
L_20c2a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c2au)) {r_15=0x20c2au;goto done;}
#endif
/* 20c2a: ldrb.w r1, [r4, #0x41] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (65u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_20c2e;
L_20c2e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c2eu)) {r_15=0x20c2eu;goto done;}
#endif
/* 20c2e: cmp r1, #3 */
{
{
uint32_t a = r_1;
uint32_t b = 0x3u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20c30;
L_20c30:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c30u)) {r_15=0x20c30u;goto done;}
#endif
if(m8_condition(cpu->xpsr,9)) {
/* 20c30: bhi #0x20c54 */
{
goto L_20c54;
}
}
goto L_20c32;
L_20c32:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c32u)) {r_15=0x20c32u;goto done;}
#endif
/* 20c32: vmov.f32 s0, s14 */
{
s_0=m8_fp_float(m8_fp_bits32(s_14));
}
goto L_20c36;
L_20c36:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c36u)) {r_15=0x20c36u;goto done;}
#endif
/* 20c36: b #0x208fa */
{
goto L_208fa;
}
goto L_20c38;
L_20c38:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c38u)) {r_15=0x20c38u;goto done;}
#endif
/* 20c38: vldr s0, [pc, #-0x26c] */
{
{
uint32_t fp_addr = (0x20c3cu - 620u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_HOST(fp_addr + 0u),4);
s_0=m8_fp_float(fp_value);
}
}
goto L_20c3c;
L_20c3c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c3cu)) {r_15=0x20c3cu;goto done;}
#endif
/* 20c3c: b #0x208ce */
{
goto L_208ce;
}
goto L_20c3e;
L_20c3e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c3eu)) {r_15=0x20c3eu;goto done;}
#endif
/* 20c3e: movs r6, #2 */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0x2u;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_6 = (uint32_t)(result);
}
}
goto L_20c40;
L_20c40:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c40u)) {r_15=0x20c40u;goto done;}
#endif
/* 20c40: b #0x20ae6 */
{
goto L_20ae6;
}
goto L_20c42;
L_20c42:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c42u)) {r_15=0x20c42u;goto done;}
#endif
/* 20c42: ldrb.w r1, [r4, #0xc9] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (201u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),1);
r_1 = (uint32_t)(value);
}
}
goto L_20c46;
L_20c46:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c46u)) {r_15=0x20c46u;goto done;}
#endif
/* 20c46: cmp r1, #0 */
{
{
uint32_t a = r_1;
uint32_t b = 0x0u;
uint32_t result = m8_sub_flags(cpu, a, b, 0u);
}
}
goto L_20c48;
L_20c48:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c48u)) {r_15=0x20c48u;goto done;}
#endif
if(m8_condition(cpu->xpsr,1)) {
/* 20c48: beq.w #0x20814 */
{
goto L_20814;
}
}
goto L_20c4c;
L_20c4c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c4cu)) {r_15=0x20c4cu;goto done;}
#endif
/* 20c4c: b #0x207d0 */
{
goto L_207d0;
}
goto L_20c4e;
L_20c4e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c4eu)) {r_15=0x20c4eu;goto done;}
#endif
/* 20c4e: vmov.f32 s15, s11 */
{
s_15=m8_fp_float(m8_fp_bits32(s_11));
}
goto L_20c52;
L_20c52:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c52u)) {r_15=0x20c52u;goto done;}
#endif
/* 20c52: b #0x20a78 */
{
goto L_20a78;
}
goto L_20c54;
L_20c54:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c54u)) {r_15=0x20c54u;goto done;}
#endif
/* 20c54: ldr r0, [r4, #0x50] */
{
{
uint32_t base = r_4;
uint32_t addr = base + (80u);
uint32_t value;
value=m8_le_read(M8_OV_OBJECT(addr),4);
r_0 = (uint32_t)(value);
}
}
goto L_20c56;
L_20c56:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c56u)) {r_15=0x20c56u;goto done;}
#endif
/* 20c56: vmov.f32 s0, s14 */
{
s_0=m8_fp_float(m8_fp_bits32(s_14));
}
goto L_20c5a;
L_20c5a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c5au)) {r_15=0x20c5au;goto done;}
#endif
/* 20c5a: vldr s1, [r3, #0x4c] */
{
{
uint32_t fp_addr = (r_3 + 76u);
uint32_t fp_value;
fp_value=m8_le_read(M8_OV_CONTROL(fp_addr + 0u),4);
s_1=m8_fp_float(fp_value);
}
}
goto L_20c5e;
L_20c5e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c5eu)) {r_15=0x20c5eu;goto done;}
#endif
/* 20c5e: ldr r3, [r0] */
{
{
uint32_t base = r_0;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_SLOT(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20c60;
L_20c60:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c60u)) {r_15=0x20c60u;goto done;}
#endif
/* 20c60: ldr r3, [r3] */
{
{
uint32_t base = r_3;
uint32_t addr = base + (0u);
uint32_t value;
value=m8_le_read(M8_OV_METHOD_TARGET(addr),4);
r_3 = (uint32_t)(value);
}
}
goto L_20c62;
L_20c62:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c62u)) {r_15=0x20c62u;goto done;}
#endif
/* 20c62: blx r3 */
{
if(r_3!=0x1fa5u && r_3!=0x1fedu) {r_15=0x20c62u;goto done;}
r_14=0x20c65u;
r_15=r_3&~1u;
if(r_3==0x1fa5u) goto L_1fa4;
goto L_1fec;
}
goto L_20c64;
L_20c64:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c64u)) {r_15=0x20c64u;goto done;}
#endif
/* 20c64: b #0x20aac */
{
goto L_20aac;
}
goto L_20c66;
L_20c66:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c66u)) {r_15=0x20c66u;goto done;}
#endif
/* 20c66: vmov.f32 s15, s14 */
{
s_15=m8_fp_float(m8_fp_bits32(s_14));
}
goto L_20c6a;
L_20c6a:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c6au)) {r_15=0x20c6au;goto done;}
#endif
/* 20c6a: b #0x20a78 */
{
goto L_20a78;
}
goto L_20c6c;
L_20c6c:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c6cu)) {r_15=0x20c6cu;goto done;}
#endif
/* 20c6c: movs r6, #0xff */
{
{
uint32_t carry = (cpu->xpsr >> 29) & 1u;
uint32_t b = 0xffu;
uint32_t result = b;
m8_logic_flags(cpu, result, carry);
r_6 = (uint32_t)(result);
}
}
goto L_20c6e;
L_20c6e:
#ifdef M8_OSCILLATOR_VOICE_STOP
if(M8_OSCILLATOR_VOICE_STOP(0x20c6eu)) {r_15=0x20c6eu;goto done;}
#endif
/* 20c6e: b #0x20664 */
{
goto L_20664;
}
r_15=0x20c70u;goto done;
method_return:
switch(r_15) {
case 0x208f0u:goto L_208f0;
case 0x20aa6u:goto L_20aa6;
case 0x20bdeu:goto L_20bde;
case 0x20c64u:goto L_20c64;
default:goto done;
}
done:
{int returned=M8_AOT_EXIT; M8_OV_PUBLISH(output); return returned;}
}
#undef M8_OV_SAVE
#undef M8_OV_LOAD
#undef M8_OV_PUBLISH
#undef M8_OV_HOST
#undef M8_OV_CHILD_RETURN
#undef M8_OV_CHILD_PC
#undef M8_OV_OBJECT
#undef M8_OV_STACK
#undef M8_OV_LEFT
#undef M8_OV_RIGHT
#undef M8_OV_CONTROL
#undef M8_OV_WAVE
#undef M8_OV_AUXILIARY
#undef M8_OV_RING
#undef M8_OV_METHOD_SLOT
#undef M8_OV_METHOD_TARGET
#if defined(M8_AOT_OSCILLATOR_SPANS)
#undef M8_OV_REFRESH_SPANS
#undef M8_OV_CACHED
#endif
