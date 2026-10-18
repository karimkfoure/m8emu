/* Original SAMPLER shape-5 continuations and pinned 1fa4/1fec methods.
 * Generated from Headless 6.5.2C by prepare_sampler_extended.py.
 * The methods use registers and the separately qualified 2070..207b
 * literal span. They do not dereference the method object in r0. */
L_1d122:
/* 1d122: ldr r0, [r4, #0x50] */ {{ uint32_t base = r_4; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_0 = (uint32_t)(value); }}
goto L_1d124;
L_1d124:
/* 1d124: vmov.f32 s0, s16 */ {s_0=m8_fp_float(m8_fp_bits32(s_16));}
goto L_1d128;
L_1d128:
/* 1d128: vldr s1, [r4, #0x12c] */ {{ uint32_t fp_addr = (r_4 + 300u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); }}
goto L_1d12c;
L_1d12c:
/* 1d12c: ldr r3, [r0] */ {{ uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d12e;
L_1d12e:
/* 1d12e: ldr r3, [r3] */ {{ uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d130;
L_1d130:
/* 1d130: blx r3 */ {M8_SV_METHOD(0x1d132u,0x1d130u);}
goto L_1d132;
L_1d132:
/* 1d132: ldrb.w r3, [r4, #0x57] */ {{ uint32_t base = r_4; uint32_t addr = base + (87u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1d136;
L_1d136:
/* 1d136: vmov.f32 s16, s0 */ {s_16=m8_fp_float(m8_fp_bits32(s_0));}
goto L_1d13a;
L_1d13a:
/* 1d13a: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d13c;
L_1d13c:
/* 1d13c: beq.w #0x1d346 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d346;}}
goto L_1d140;
L_1d140:
/* 1d140: ldrb.w r2, [r4, #0x41] */ {{ uint32_t base = r_4; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1d144;
L_1d144:
/* 1d144: cmp r2, #3 */ {{ uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d146;
L_1d146:
/* 1d146: bls.w #0x1d354 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d354;}}
goto L_1d14a;
L_1d14a:
/* 1d14a: ldrb.w r3, [r4, #0x40] */ {{ uint32_t base = r_4; uint32_t addr = base + (64u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1d14e;
L_1d14e:
/* 1d14e: subs r1, r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_1 = (uint32_t)(result); }}
goto L_1d150;
L_1d150:
/* 1d150: cmp r1, #4 */ {{ uint32_t a = r_1; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d152;
L_1d152:
/* 1d152: bls.w #0x1d378 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d378;}}
goto L_1d156;
L_1d160:
/* 1d160: ldr r0, [r4, #0x50] */ {{ uint32_t base = r_4; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_0 = (uint32_t)(value); }}
goto L_1d162;
L_1d162:
/* 1d162: vmov.f32 s0, s30 */ {s_0=m8_fp_float(m8_fp_bits32(s_30));}
goto L_1d166;
L_1d166:
/* 1d166: vldr s1, [r4, #0x12c] */ {{ uint32_t fp_addr = (r_4 + 300u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); }}
goto L_1d16a;
L_1d16a:
/* 1d16a: ldr r3, [r0] */ {{ uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d16c;
L_1d16c:
/* 1d16c: ldr r3, [r3] */ {{ uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d16e;
L_1d16e:
/* 1d16e: blx r3 */ {M8_SV_METHOD(0x1d170u,0x1d16eu);}
goto L_1d170;
L_1d170:
/* 1d170: vmov.f32 s30, s0 */ {s_30=m8_fp_float(m8_fp_bits32(s_0));}
goto L_1d174;
L_1fa4:
/* 1fa4: vmov.f32 s14, #-1.000000e+00 */ {s_14=m8_fp_float(0xbf800000u);}
goto L_1fa8;
L_1fa8:
/* 1fa8: vmov.f32 s15, #1.000000e+00 */ {s_15=m8_fp_float(0x3f800000u);}
goto L_1fac;
L_1fac:
/* 1fac: vmul.f32 s0, s0, s1 */ {s_0=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_1), 0));}
goto L_1fb0;
L_1fb0:
/* 1fb0: vmaxnm.f32 s0, s0, s14 */ {s_0=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_0), m8_fp_bits32(s_14), 0));}
goto L_1fb4;
L_1fb4:
/* 1fb4: vminnm.f32 s0, s0, s15 */ {s_0=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_0), m8_fp_bits32(s_15), 0));}
goto L_1fb8;
L_1fb8:
/* 1fb8: bx lr */ {r_15=r_14&~1u;goto sampler_method_return;}
L_1fec:
/* 1fec: vmov.f32 s15, #-1.500000e+01 */ {s_15=m8_fp_float(0xc1700000u);}
goto L_1ff0;
L_1ff0:
/* 1ff0: vcmpe.f32 s1, s15 */ {m8_fp_cmp32(cpu, m8_fp_bits32(s_1), m8_fp_bits32(s_15), 1);}
goto L_1ff4;
L_1ff4:
/* 1ff4: vmrs apsr_nzcv, fpscr */ {cpu->xpsr = (cpu->xpsr & 0x0fffffffu) | (cpu->fpscr & 0xf0000000u);}
goto L_1ff8;
L_1ff8:
/* 1ff8: bmi #0x206a */ {if(m8_condition(cpu->xpsr,5)) {goto L_206a;}}
goto L_1ffa;
L_1ffa:
/* 1ffa: vmov.f32 s13, #1.000000e+00 */ {s_13=m8_fp_float(0x3f800000u);}
goto L_1ffe;
L_1ffe:
/* 1ffe: vldr s12, [pc, #0x70] */ {{ uint32_t fp_addr = (0x2000u + 112u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_2002;
L_2002:
/* 2002: vldr s14, [pc, #0x70] */ {{ uint32_t fp_addr = (0x2004u + 112u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_2006;
L_2006:
/* 2006: vmov.f32 s15, s13 */ {s_15=m8_fp_float(m8_fp_bits32(s_13));}
goto L_200a;
L_200a:
/* 200a: vfma.f32 s15, s1, s12 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_1), m8_fp_bits32(s_12), m8_fp_bits32(s_15)));}
goto L_200e;
L_200e:
/* 200e: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2012;
L_2012:
/* 2012: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2016;
L_2016:
/* 2016: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_201a;
L_201a:
/* 201a: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_201e;
L_201e:
/* 201e: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2022;
L_2022:
/* 2022: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2026;
L_2026:
/* 2026: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_202a;
L_202a:
/* 202a: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_202e;
L_202e:
/* 202e: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2032;
L_2032:
/* 2032: vmul.f32 s15, s15, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_15), 0));}
goto L_2036;
L_2036:
/* 2036: vmul.f32 s0, s15, s0 */ {s_0=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_0), 0));}
goto L_203a;
L_203a:
/* 203a: vmul.f32 s15, s0, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_0), m8_fp_bits32(s_14), 0));}
goto L_203e;
L_203e:
/* 203e: vmov r3, s0 */ {r_3 = m8_fp_bits32(s_0);}
goto L_2042;
L_2042:
/* 2042: and r3, r3, #0x80000000 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; carry = (0x80000000u) >> 31; uint32_t a = r_3; uint32_t b = 0x80000000u; uint32_t result = a & b; r_3 = (uint32_t)(result); }}
goto L_2046;
L_2046:
/* 2046: vabs.f32 s15, s15 */ {s_15=m8_fp_float((m8_fp_bits32(s_15) & ~0x80000000u));}
goto L_204a;
L_204a:
/* 204a: vmov.f32 s14, s15 */ {s_14=m8_fp_float(m8_fp_bits32(s_15));}
goto L_204e;
L_204e:
/* 204e: vadd.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_2052;
L_2052:
/* 2052: vfma.f32 s14, s0, s0 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_0), m8_fp_bits32(s_0), m8_fp_bits32(s_14)));}
goto L_2056;
L_2056:
/* 2056: vadd.f32 s15, s15, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_14), 0));}
goto L_205a;
L_205a:
/* 205a: vdiv.f32 s15, s14, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_DIV, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));}
goto L_205e;
L_205e:
/* 205e: vmov r2, s15 */ {r_2 = m8_fp_bits32(s_15);}
goto L_2062;
L_2062:
/* 2062: orrs r3, r2 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = r_2; uint32_t result = a | b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); }}
goto L_2064;
L_2064:
/* 2064: vmov s0, r3 */ {s_0=m8_fp_float(r_3);}
goto L_2068;
L_2068:
/* 2068: bx lr */ {r_15=r_14&~1u;goto sampler_method_return;}
L_206a:
/* 206a: vldr s0, [pc, #0xc] */ {{ uint32_t fp_addr = (0x206cu + 12u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_0=m8_fp_float(fp_value); }}
goto L_206e;
L_206e:
/* 206e: bx lr */ {r_15=r_14&~1u;goto sampler_method_return;}
sampler_method_return:
switch(r_15) {
case 0x1d102u:goto L_1d102;
case 0x1d132u:goto L_1d132;
case 0x1d170u:goto L_1d170;
case 0x1d364u:goto L_1d364;
default:goto done;
}
