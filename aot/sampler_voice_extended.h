/* Pinned Headless 6.5.2C instruction paths for the default-off web
 * SAMPLER extension. Regenerate with tests/web/prepare_sampler_extended.py.
 * The parent qualifies object, stack, outputs, ring and sample backing.
 * Cache guards precede the first original fetch instruction. Slice-table
 * reads retain a checked RAM boundary and resume before a failed read.
 * Every FP operation retains the exact software API and instruction order. */
L_1cd1c:
/* 1cd1c: vcvt.u32.f64 s15, d5 */ {s_15=m8_fp_float(m8_fp_to_int(cpu, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), 1, 1, 3, 32, 0));}
goto L_1cd20;
L_1cd20:
/* 1cd20: movw r1, #0xfffe */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0xfffeu; uint32_t result = b; r_1 = (uint32_t)(result); }}
goto L_1cd24;
L_1cd24:
/* 1cd24: vmov.f64 d6, #5.000000e-01 */ {{uint64_t pair=UINT64_C(0x3fe0000000000000);s_12=m8_fp_float((uint32_t)pair);s_13=m8_fp_float((uint32_t)(pair>>32));}}
goto L_1cd28;
L_1cd28:
/* 1cd28: ldrb.w r2, [r4, #0x54] */ {{ uint32_t base = r_4; uint32_t addr = base + (84u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1cd2c;
L_1cd2c:
/* 1cd2c: vmov r3, s15 */ {r_3 = m8_fp_bits32(s_15);}
goto L_1cd30;
L_1cd30:
/* 1cd30: cmp r2, #0x18 */ {{ uint32_t a = r_2; uint32_t b = 0x18u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cd32;
L_1cd32:
/* 1cd32: and.w r3, r3, r1 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = r_1; uint32_t result = a & b; r_3 = (uint32_t)(result); }}
goto L_1cd36;
L_1cd36:
/* 1cd36: vmov s15, r3 */ {s_15=m8_fp_float(r_3);}
goto L_1cd3a;
L_1cd3a:
/* 1cd3a: vcvt.f64.s32 d7, s15 */ {{uint64_t pair=m8_fp_from_int(cpu, m8_fp_bits32(s_15), 1, 0, 32, 0, 0);s_14=m8_fp_float((uint32_t)pair);s_15=m8_fp_float((uint32_t)(pair>>32));}}
goto L_1cd3e;
L_1cd3e:
/* 1cd3e: vsub.f64 d7, d5, d7 */ {{uint64_t pair=m8_fp_op64(cpu, M8_FP_SUB, ((uint64_t)m8_fp_bits32(s_10)|((uint64_t)m8_fp_bits32(s_11)<<32)), ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 0);s_14=m8_fp_float((uint32_t)pair);s_15=m8_fp_float((uint32_t)(pair>>32));}}
goto L_1cd42;
L_1cd42:
/* 1cd42: vmul.f64 d7, d7, d6 */ {{uint64_t pair=m8_fp_op64(cpu, M8_FP_MUL, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), ((uint64_t)m8_fp_bits32(s_12)|((uint64_t)m8_fp_bits32(s_13)<<32)), 0);s_14=m8_fp_float((uint32_t)pair);s_15=m8_fp_float((uint32_t)(pair>>32));}}
goto L_1cd46;
L_1cd46:
/* 1cd46: vcvt.f32.f64 s14, d7 */ {s_14=m8_fp_float(m8_fp_convert(cpu, ((uint64_t)m8_fp_bits32(s_14)|((uint64_t)m8_fp_bits32(s_15)<<32)), 1));}
goto L_1cd4a;
L_1cd4a:
/* 1cd4a: beq.w #0x1dda0 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1dda0;}}
goto L_1cd4e;
L_1cd4e:
/* 1cd4e: cmp r2, #0x20 */ {{ uint32_t a = r_2; uint32_t b = 0x20u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cd50;
L_1cd50:
/* 1cd50: beq.w #0x1da9a */ {if(m8_condition(cpu->xpsr,1)) {goto L_1da9a;}}
goto L_1cd54;
L_1cd54:
/* 1cd54: cmp r2, #0x10 */ {{ uint32_t a = r_2; uint32_t b = 0x10u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cd56;
L_1cd56:
/* 1cd56: beq.w #0x1d9ee */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d9ee;}}
goto L_1cd5a;
L_1ce60:
/* 1ce60: ldr r2, [sp, #4] */ {{ uint32_t base = r_13; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); }}
goto L_1ce62;
L_1ce62:
/* 1ce62: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1ce64;
L_1ce64:
/* 1ce64: beq.w #0x1d58c */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d58c;}}
goto L_1ce68;
L_1ce68:
/* 1ce68: ldrb.w r2, [r4, #0x63] */ {{ uint32_t base = r_4; uint32_t addr = base + (99u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1ce6c;
L_1ce6c:
/* 1ce6c: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1ce6e;
L_1ce6e:
/* 1ce6e: beq.w #0x1db7c */ {if(m8_condition(cpu->xpsr,1)) {goto L_1db7c;}}
goto L_1ce72;
L_1ce72:
/* 1ce72: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1ce76;
L_1ce76:
/* 1ce76: ldr.w r1, [r4, #0x1cc] */ {{ uint32_t base = r_4; uint32_t addr = base + (460u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_1 = (uint32_t)(value); }}
goto L_1ce7a;
L_1ce7a:
/* 1ce7a: ldr.w r6, [r4, #0x1b0] */ {{ uint32_t base = r_4; uint32_t addr = base + (432u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1ce7e;
L_1ce7e:
/* 1ce7e: subs r6, r6, r1 */ {{ uint32_t a = r_6; uint32_t b = r_1; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_6 = (uint32_t)(result); }}
goto L_1ce80;
L_1ce80:
/* 1ce80: mov r7, r6 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_6; uint32_t result = b; r_7 = (uint32_t)(result); }}
goto L_1ce82;
L_1ce82:
/* 1ce82: ldr.w r6, [r4, #0x1b4] */ {{ uint32_t base = r_4; uint32_t addr = base + (436u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1ce86;
L_1ce86:
/* 1ce86: cmp r7, r3 */ {{ uint32_t a = r_7; uint32_t b = r_3; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1ce88;
L_1ce88:
/* 1ce88: sub.w lr, r6, r1 */ {{ uint32_t a = r_6; uint32_t b = r_1; uint32_t result = a - b - 0u; r_14 = (uint32_t)(result); }}
goto L_1ce8c;
L_1ce8c:
/* 1ce8c: str r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1ce8e;
L_1ce8e:
/* 1ce90: vldreq s6, [r4, #0xd0] */ {if(m8_condition(cpu->xpsr,1)) {{ uint32_t fp_addr = (r_4 + 208u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); }}}
goto L_1ce94;
L_1ce94:
/* 1ce94: cmp lr, sb */ {{ uint32_t a = r_14; uint32_t b = r_9; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1ce96;
L_1ce96:
/* 1ce96: bhi #0x1ceac */ {if(m8_condition(cpu->xpsr,9)) {goto L_1ceac;}}
goto L_1ce98;
L_1ce98:
/* 1ce98: add.w sb, r1, #4 */ {{ uint32_t a = r_1; uint32_t b = 0x4u; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1ce9c;
L_1ce9c:
/* 1ce9c: ldr r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1ce9e;
L_1ce9e:
/* 1ce9e: sub.w sb, sb, r6 */ {{ uint32_t a = r_9; uint32_t b = r_6; uint32_t result = a - b - 0u; r_9 = (uint32_t)(result); }}
goto L_1cea2;
L_1cea2:
/* 1cea2: add sb, r3 */ {{ uint32_t a = r_9; uint32_t b = r_3; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cea4;
L_1cea4:
/* 1cea4: add.w sb, r4, sb, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_9, 2, 2, NULL); uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cea8;
L_1cea8:
/* 1cea8: vldr s15, [sb, #0xd0] */ {{ uint32_t fp_addr = (r_9 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1cea8u;goto done;}fp_value=m8_le_read(span,4);} s_15=m8_fp_float(fp_value); }}
goto L_1ceac;
L_1ceac:
/* 1ceac: ldr r6, [sp, #0x10] */ {{ uint32_t base = r_13; uint32_t addr = base + (16u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1ceae;
L_1ceae:
/* 1ceae: cmp lr, r6 */ {{ uint32_t a = r_14; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1ceb0;
L_1ceb0:
/* 1ceb0: bhi #0x1cec6 */ {if(m8_condition(cpu->xpsr,9)) {goto L_1cec6;}}
goto L_1ceb2;
L_1ceb2:
/* 1ceb2: add.w sb, r1, #6 */ {{ uint32_t a = r_1; uint32_t b = 0x6u; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1ceb6;
L_1ceb6:
/* 1ceb6: ldr r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1ceb8;
L_1ceb8:
/* 1ceb8: sub.w sb, sb, r6 */ {{ uint32_t a = r_9; uint32_t b = r_6; uint32_t result = a - b - 0u; r_9 = (uint32_t)(result); }}
goto L_1cebc;
L_1cebc:
/* 1cebc: add sb, r3 */ {{ uint32_t a = r_9; uint32_t b = r_3; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cebe;
L_1cebe:
/* 1cebe: add.w sb, r4, sb, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_9, 2, 2, NULL); uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cec2;
L_1cec2:
/* 1cec2: vldr s3, [sb, #0xd0] */ {{ uint32_t fp_addr = (r_9 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1cec2u;goto done;}fp_value=m8_le_read(span,4);} s_3=m8_fp_float(fp_value); }}
goto L_1cec6;
L_1cec6:
/* 1cec6: vsub.f32 s5, s15, s6 */ {s_5=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_15), m8_fp_bits32(s_6), 0));}
goto L_1ceca;
L_1ceca:
/* 1ceca: cmp r7, r3 */ {{ uint32_t a = r_7; uint32_t b = r_3; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cecc;
L_1cecc:
/* 1cecc: vsub.f32 s15, s16, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));}
goto L_1ced0;
L_1ced0:
/* 1ced0: vmov.f32 s6, #5.000000e-01 */ {s_6=m8_fp_float(0x3f000000u);}
goto L_1ced4;
L_1ced4:
/* 1ced6: vldreq s8, [r4, #0xd4] */ {if(m8_condition(cpu->xpsr,1)) {{ uint32_t fp_addr = (r_4 + 212u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); }}}
goto L_1ceda;
L_1ceda:
/* 1ceda: vsub.f32 s3, s3, s16 */ {s_3=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_3), m8_fp_bits32(s_16), 0));}
goto L_1cede;
L_1cede:
/* 1cede: cmp lr, ip */ {{ uint32_t a = r_14; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cee0;
L_1cee0:
/* 1cee0: vmov.f32 s13, s15 */ {s_13=m8_fp_float(m8_fp_bits32(s_15));}
goto L_1cee4;
L_1cee4:
/* 1cee4: vmov.f32 s7, #1.000000e+00 */ {s_7=m8_fp_float(0x3f800000u);}
goto L_1cee8;
L_1cee8:
/* 1cee8: vfma.f32 s13, s5, s6 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_5), m8_fp_bits32(s_6), m8_fp_bits32(s_13)));}
goto L_1ceec;
L_1ceec:
/* 1ceec: vsub.f32 s7, s14, s7 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_7), 0));}
goto L_1cef0;
L_1cef0:
/* 1cef0: vadd.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1cef4;
L_1cef4:
/* 1cef4: vfma.f32 s15, s3, s6 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_3), m8_fp_bits32(s_6), m8_fp_bits32(s_15)));}
goto L_1cef8;
L_1cef8:
/* 1cef8: vfnms.f32 s13, s15, s7 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_7), (m8_fp_bits32(s_13) ^ 0x80000000u)));}
goto L_1cefc;
L_1cefc:
/* 1cefc: vmul.f32 s13, s14, s13 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));}
goto L_1cf00;
L_1cf00:
/* 1cf00: vfma.f32 s13, s5, s6 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_5), m8_fp_bits32(s_6), m8_fp_bits32(s_13)));}
goto L_1cf04;
L_1cf04:
/* 1cf04: vfma.f32 s16, s14, s13 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_13), m8_fp_bits32(s_16)));}
goto L_1cf08;
L_1cf08:
/* 1cf08: bhs #0x1cf1e */ {if(m8_condition(cpu->xpsr,3)) {goto L_1cf1e;}}
goto L_1cf0a;
L_1cf0a:
/* 1cf0a: add.w sb, r1, #5 */ {{ uint32_t a = r_1; uint32_t b = 0x5u; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cf0e;
L_1cf0e:
/* 1cf0e: ldr r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1cf10;
L_1cf10:
/* 1cf10: sub.w sb, sb, r6 */ {{ uint32_t a = r_9; uint32_t b = r_6; uint32_t result = a - b - 0u; r_9 = (uint32_t)(result); }}
goto L_1cf14;
L_1cf14:
/* 1cf14: add sb, r3 */ {{ uint32_t a = r_9; uint32_t b = r_3; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cf16;
L_1cf16:
/* 1cf16: add.w sb, r4, sb, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_9, 2, 2, NULL); uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1cf1a;
L_1cf1a:
/* 1cf1a: vldr s12, [sb, #0xd0] */ {{ uint32_t fp_addr = (r_9 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1cf1au;goto done;}fp_value=m8_le_read(span,4);} s_12=m8_fp_float(fp_value); }}
goto L_1cf1e;
L_1cf1e:
/* 1cf1e: ldr r6, [sp, #0xc] */ {{ uint32_t base = r_13; uint32_t addr = base + (12u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1cf20;
L_1cf20:
/* 1cf20: cmp lr, r6 */ {{ uint32_t a = r_14; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1cf22;
L_1cf22:
/* 1cf22: bhs #0x1cf38 */ {if(m8_condition(cpu->xpsr,3)) {goto L_1cf38;}}
goto L_1cf24;
L_1cf24:
/* 1cf24: add.w ip, r1, #7 */ {{ uint32_t a = r_1; uint32_t b = 0x7u; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1cf28;
L_1cf28:
/* 1cf28: ldr r1, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_1 = (uint32_t)(value); }}
goto L_1cf2a;
L_1cf2a:
/* 1cf2a: sub.w ip, ip, r1 */ {{ uint32_t a = r_12; uint32_t b = r_1; uint32_t result = a - b - 0u; r_12 = (uint32_t)(result); }}
goto L_1cf2e;
L_1cf2e:
/* 1cf2e: add r3, ip */ {{ uint32_t a = r_3; uint32_t b = r_12; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1cf30;
L_1cf30:
/* 1cf30: add.w r3, r4, r3, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_3, 2, 2, NULL); uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1cf34;
L_1cf34:
/* 1cf34: vldr s9, [r3, #0xd0] */ {{ uint32_t fp_addr = (r_3 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1cf34u;goto done;}fp_value=m8_le_read(span,4);} s_9=m8_fp_float(fp_value); }}
goto L_1cf38;
L_1cf38:
/* 1cf38: vsub.f32 s13, s17, s12 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_17), m8_fp_bits32(s_12), 0));}
goto L_1cf3c;
L_1cf3c:
/* 1cf3c: vsub.f32 s8, s12, s8 */ {s_8=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_12), m8_fp_bits32(s_8), 0));}
goto L_1cf40;
L_1cf40:
/* 1cf40: vmov.f32 s12, #5.000000e-01 */ {s_12=m8_fp_float(0x3f000000u);}
goto L_1cf44;
L_1cf44:
/* 1cf44: vmov.f32 s15, s13 */ {s_15=m8_fp_float(m8_fp_bits32(s_13));}
goto L_1cf48;
L_1cf48:
/* 1cf48: vsub.f32 s9, s9, s17 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_9), m8_fp_bits32(s_17), 0));}
goto L_1cf4c;
L_1cf4c:
/* 1cf4c: vfma.f32 s15, s8, s12 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_8), m8_fp_bits32(s_12), m8_fp_bits32(s_15)));}
goto L_1cf50;
L_1cf50:
/* 1cf50: vadd.f32 s13, s15, s13 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1cf54;
L_1cf54:
/* 1cf54: vfma.f32 s13, s9, s12 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_9), m8_fp_bits32(s_12), m8_fp_bits32(s_13)));}
goto L_1cf58;
L_1cf58:
/* 1cf58: vfnms.f32 s15, s13, s7 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_7), (m8_fp_bits32(s_15) ^ 0x80000000u)));}
goto L_1cf5c;
L_1cf5c:
/* 1cf5c: vmul.f32 s15, s14, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));}
goto L_1cf60;
L_1cf60:
/* 1cf60: vfma.f32 s15, s8, s12 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_8), m8_fp_bits32(s_12), m8_fp_bits32(s_15)));}
goto L_1cf64;
L_1cf64:
/* 1cf64: vmov.f32 s30, s17 */ {s_30=m8_fp_float(m8_fp_bits32(s_17));}
goto L_1cf68;
L_1cf68:
/* 1cf68: vfma.f32 s30, s14, s15 */ {s_30=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_15), m8_fp_bits32(s_30)));}
goto L_1cf6c;
L_1cf6c:
/* 1cf6c: vcvt.s32.f32 s15, s30 */ {s_15=m8_fp_float(m8_fp_to_int(cpu, m8_fp_bits32(s_30), 0, 0, 3, 32, 0));}
goto L_1cf70;
L_1cf70:
/* 1cf70: vmov r3, s15 */ {r_3 = m8_fp_bits32(s_15);}
goto L_1cf74;
L_1cf74:
/* 1cf74: sxth r3, r3 */ {{ uint32_t value = r_3; r_3 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1cf76;
L_1d156:
/* 1d156: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d158;
L_1d158:
/* 1d158: bne.w #0x1d924 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1d924;}}
goto L_1d15c;
L_1d15c:
/* 1d15c: cmp r2, #3 */ {{ uint32_t a = r_2; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d15e;
L_1d15e:
/* 1d15e: bls #0x1d174 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d174;}}
goto L_1d160;
L_1d354:
/* 1d354: ldr r0, [r4, #0x50] */ {{ uint32_t base = r_4; uint32_t addr = base + (80u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_0 = (uint32_t)(value); }}
goto L_1d356;
L_1d356:
/* 1d356: vmov.f32 s0, s30 */ {s_0=m8_fp_float(m8_fp_bits32(s_30));}
goto L_1d35a;
L_1d35a:
/* 1d35a: vldr s1, [r4, #0x12c] */ {{ uint32_t fp_addr = (r_4 + 300u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_1=m8_fp_float(fp_value); }}
goto L_1d35e;
L_1d35e:
/* 1d35e: ldr r3, [r0] */ {{ uint32_t base = r_0; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d360;
L_1d360:
/* 1d360: ldr r3, [r3] */ {{ uint32_t base = r_3; uint32_t addr = base + (0u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d362;
L_1d362:
#ifdef M8_SV_SHAPE
/* 1d362: blx r3 */ {M8_SV_METHOD(0x1d364u,0x1d362u);}
#else
/* 1d362: blx r3 */ {r_14=0x1d365u; s_14=m8_fp_float(0xbf800000u);s_15=m8_fp_float(0x3f800000u); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MUL,m8_fp_bits32(s_0),m8_fp_bits32(s_1),0)); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MAXNM,m8_fp_bits32(s_0),m8_fp_bits32(s_14),0)); s_0=m8_fp_float(m8_fp_eval32(cpu,M8_FP_MINNM,m8_fp_bits32(s_0),m8_fp_bits32(s_15),0));}
#endif
goto L_1d364;
L_1d364:
/* 1d364: ldrb.w r3, [r4, #0x40] */ {{ uint32_t base = r_4; uint32_t addr = base + (64u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1d368;
L_1d368:
/* 1d368: vmov.f32 s30, s0 */ {s_30=m8_fp_float(m8_fp_bits32(s_0));}
goto L_1d36c;
L_1d36c:
/* 1d36c: ldrb.w r2, [r4, #0x41] */ {{ uint32_t base = r_4; uint32_t addr = base + (65u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1d370;
L_1d370:
/* 1d370: subs r1, r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_1 = (uint32_t)(result); }}
goto L_1d372;
L_1d372:
/* 1d372: cmp r1, #4 */ {{ uint32_t a = r_1; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d374;
L_1d374:
/* 1d374: bhi.w #0x1d156 */ {if(m8_condition(cpu->xpsr,9)) {goto L_1d156;}}
goto L_1d378;
L_1d378:
/* 1d378: vmov.f32 s15, #1.000000e+00 */ {s_15=m8_fp_float(0x3f800000u);}
goto L_1d37c;
L_1d37c:
/* 1d37c: vldr s13, [r4, #0x124] */ {{ uint32_t fp_addr = (r_4 + 292u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1d380;
L_1d380:
/* 1d380: vldr s14, [pc, #0x130] */ {{ uint32_t fp_addr = (0x1d384u + 304u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d384;
L_1d384:
/* 1d384: cmp r3, #5 */ {{ uint32_t a = r_3; uint32_t b = 0x5u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d386;
L_1d386:
/* 1d386: vldr s10, [r4, #0x128] */ {{ uint32_t fp_addr = (r_4 + 296u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_10=m8_fp_float(fp_value); }}
goto L_1d38a;
L_1d38a:
/* 1d38a: vminnm.f32 s13, s13, s15 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_13), m8_fp_bits32(s_15), 0));}
goto L_1d38e;
L_1d38e:
/* 1d38e: vmaxnm.f32 s13, s13, s14 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_13), m8_fp_bits32(s_14), 0));}
goto L_1d392;
L_1d392:
/* 1d392: beq.w #0x1df2c */ {if(m8_condition(cpu->xpsr,1)) {goto L_1df2c;}}
goto L_1d396;
L_1d396:
/* 1d396: vldr s11, [pc, #0xf8] */ {{ uint32_t fp_addr = (0x1d398u + 248u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); }}
goto L_1d39a;
L_1d39a:
/* 1d39a: vldr s15, [r4, #0xa8] */ {{ uint32_t fp_addr = (r_4 + 168u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1d39e;
L_1d39e:
/* 1d39e: cmp r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d3a0;
L_1d3a0:
/* 1d3a0: vldr s14, [r4, #0xa4] */ {{ uint32_t fp_addr = (r_4 + 164u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d3a4;
L_1d3a4:
/* 1d3a4: vsub.f32 s12, s30, s15 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_30), m8_fp_bits32(s_15), 0));}
goto L_1d3a8;
L_1d3a8:
/* 1d3a8: vfms.f32 s12, s14, s10 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_14) ^ 0x80000000u), m8_fp_bits32(s_10), m8_fp_bits32(s_12)));}
goto L_1d3ac;
L_1d3ac:
/* 1d3ac: vfma.f32 s14, s13, s12 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_12), m8_fp_bits32(s_14)));}
goto L_1d3b0;
L_1d3b0:
/* 1d3b0: vmov.f32 s12, s30 */ {s_12=m8_fp_float(m8_fp_bits32(s_30));}
goto L_1d3b4;
L_1d3b4:
/* 1d3b4: vfma.f32 s15, s13, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));}
goto L_1d3b8;
L_1d3b8:
/* 1d3b8: vfms.f32 s12, s10, s14 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_10) ^ 0x80000000u), m8_fp_bits32(s_14), m8_fp_bits32(s_12)));}
goto L_1d3bc;
L_1d3bc:
/* 1d3bc: vsub.f32 s10, s12, s15 */ {s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_12), m8_fp_bits32(s_15), 0));}
goto L_1d3c0;
L_1d3c0:
/* 1d3c0: vldr s12, [pc, #0xd0] */ {{ uint32_t fp_addr = (0x1d3c4u + 208u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1d3c4;
L_1d3c4:
/* 1d3c4: vfma.f32 s14, s13, s10 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_10), m8_fp_bits32(s_14)));}
goto L_1d3c8;
L_1d3c8:
/* 1d3c8: vfma.f32 s15, s13, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));}
goto L_1d3cc;
L_1d3cc:
/* 1d3cc: vldr s13, [pc, #0xc8] */ {{ uint32_t fp_addr = (0x1d3d0u + 200u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1d3d0;
L_1d3d0:
/* 1d3d0: vmaxnm.f32 s14, s14, s13 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));}
goto L_1d3d4;
L_1d3d4:
/* 1d3d4: vminnm.f32 s14, s14, s12 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_14), m8_fp_bits32(s_12), 0));}
goto L_1d3d8;
L_1d3d8:
/* 1d3d8: vabs.f32 s8, s14 */ {s_8=m8_fp_float((m8_fp_bits32(s_14) & ~0x80000000u));}
goto L_1d3dc;
L_1d3dc:
/* 1d3dc: vmaxnm.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1d3e0;
L_1d3e0:
/* 1d3e0: vmov.f32 s13, #1.000000e+00 */ {s_13=m8_fp_float(0x3f800000u);}
goto L_1d3e4;
L_1d3e4:
/* 1d3e4: vminnm.f32 s15, s15, s12 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_15), m8_fp_bits32(s_12), 0));}
goto L_1d3e8;
L_1d3e8:
/* 1d3e8: vldr s12, [pc, #0xb0] */ {{ uint32_t fp_addr = (0x1d3ecu + 176u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1d3ec;
L_1d3ec:
/* 1d3ec: vmov.f32 s9, s13 */ {s_9=m8_fp_float(m8_fp_bits32(s_13));}
goto L_1d3f0;
L_1d3f0:
/* 1d3f0: vfms.f32 s9, s8, s12 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_8) ^ 0x80000000u), m8_fp_bits32(s_12), m8_fp_bits32(s_9)));}
goto L_1d3f4;
L_1d3f4:
/* 1d3f4: vmul.f32 s14, s14, s9 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_9), 0));}
goto L_1d3f8;
L_1d3f8:
/* 1d3f8: vabs.f32 s9, s15 */ {s_9=m8_fp_float((m8_fp_bits32(s_15) & ~0x80000000u));}
goto L_1d3fc;
L_1d3fc:
/* 1d3fc: vstr s14, [r4, #0xa4] */ {{ uint32_t fp_addr = (r_4 + 164u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); }}
goto L_1d400;
L_1d400:
/* 1d400: vfms.f32 s13, s9, s12 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_9) ^ 0x80000000u), m8_fp_bits32(s_12), m8_fp_bits32(s_13)));}
goto L_1d404;
L_1d404:
/* 1d404: vmul.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1d408;
L_1d408:
/* 1d408: vstr s15, [r4, #0xa8] */ {{ uint32_t fp_addr = (r_4 + 168u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); }}
goto L_1d40c;
L_1d40c:
/* 1d40c: beq #0x1d422 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d422;}}
goto L_1d40e;
L_1d40e:
/* 1d40e: cmp r3, #2 */ {{ uint32_t a = r_3; uint32_t b = 0x2u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d410;
L_1d410:
/* 1d410: beq.w #0x1e062 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1e062;}}
goto L_1d414;
L_1d414:
/* 1d414: cmp r3, #3 */ {{ uint32_t a = r_3; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d416;
L_1d416:
/* 1d416: beq.w #0x1e1d6 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1e1d6;}}
goto L_1d41a;
L_1d41a:
/* 1d41a: cmp r3, #4 */ {{ uint32_t a = r_3; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d41c;
L_1d41c:
/* 1d41c: bne #0x1d422 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1d422;}}
goto L_1d41e;
L_1d41e:
/* 1d41e: vsub.f32 s15, s30, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_30), m8_fp_bits32(s_14), 0));}
goto L_1d422;
L_1d422:
/* 1d422: vldr s30, [r4, #0xac] */ {{ uint32_t fp_addr = (r_4 + 172u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_30=m8_fp_float(fp_value); }}
goto L_1d426;
L_1d426:
/* 1d426: vldr s14, [r4, #0xb0] */ {{ uint32_t fp_addr = (r_4 + 176u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d42a;
L_1d42a:
/* 1d42a: vstr s15, [r4, #0xac] */ {{ uint32_t fp_addr = (r_4 + 172u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); }}
goto L_1d42e;
L_1d42e:
/* 1d42e: vfnms.f32 s30, s11, s14 */ {s_30=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_11), m8_fp_bits32(s_14), (m8_fp_bits32(s_30) ^ 0x80000000u)));}
goto L_1d432;
L_1d432:
/* 1d432: vadd.f32 s30, s15, s30 */ {s_30=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_30), 0));}
goto L_1d436;
L_1d436:
/* 1d436: vstr s30, [r4, #0xb0] */ {{ uint32_t fp_addr = (r_4 + 176u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_30)); }}
goto L_1d43a;
L_1d43a:
/* 1d43a: b #0x1d15c */ {goto L_1d15c;}
L_1d4b8:
/* 1d4b8: vmov.f32 s15, #1.000000e+00 */ {s_15=m8_fp_float(0x3f800000u);}
goto L_1d4bc;
L_1d4bc:
/* 1d4bc: vldr s13, [r4, #0x124] */ {{ uint32_t fp_addr = (r_4 + 292u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1d4c0;
L_1d4c0:
/* 1d4c0: vldr s14, [pc, #-0x10] */ {{ uint32_t fp_addr = (0x1d4c4u - 16u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d4c4;
L_1d4c4:
/* 1d4c4: cmp r3, #5 */ {{ uint32_t a = r_3; uint32_t b = 0x5u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d4c6;
L_1d4c6:
/* 1d4c6: vldr s6, [r4, #0x128] */ {{ uint32_t fp_addr = (r_4 + 296u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); }}
goto L_1d4ca;
L_1d4ca:
/* 1d4ca: vminnm.f32 s13, s13, s15 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_13), m8_fp_bits32(s_15), 0));}
goto L_1d4ce;
L_1d4ce:
/* 1d4ce: vmaxnm.f32 s13, s13, s14 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_13), m8_fp_bits32(s_14), 0));}
goto L_1d4d2;
L_1d4d2:
/* 1d4d2: beq.w #0x1d916 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d916;}}
goto L_1d4d6;
L_1d4d6:
/* 1d4d6: vldr s11, [pc, #-0x48] */ {{ uint32_t fp_addr = (0x1d4d8u - 72u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); }}
goto L_1d4da;
L_1d4da:
/* 1d4da: vldr s15, [r4, #0x24] */ {{ uint32_t fp_addr = (r_4 + 36u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1d4de;
L_1d4de:
/* 1d4de: vmov.f32 s10, s16 */ {s_10=m8_fp_float(m8_fp_bits32(s_16));}
goto L_1d4e2;
L_1d4e2:
/* 1d4e2: vldr s14, [r4, #0x20] */ {{ uint32_t fp_addr = (r_4 + 32u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d4e6;
L_1d4e6:
/* 1d4e6: cmp r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d4e8;
L_1d4e8:
/* 1d4e8: vsub.f32 s12, s16, s15 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));}
goto L_1d4ec;
L_1d4ec:
/* 1d4ec: vldr s8, [pc, #-0x5c] */ {{ uint32_t fp_addr = (0x1d4f0u - 92u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); }}
goto L_1d4f0;
L_1d4f0:
/* 1d4f0: vldr s7, [pc, #-0x5c] */ {{ uint32_t fp_addr = (0x1d4f4u - 92u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); }}
goto L_1d4f4;
L_1d4f4:
/* 1d4f4: vldr s9, [pc, #-0x5c] */ {{ uint32_t fp_addr = (0x1d4f8u - 92u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); }}
goto L_1d4f8;
L_1d4f8:
/* 1d4f8: vfms.f32 s12, s14, s6 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_14) ^ 0x80000000u), m8_fp_bits32(s_6), m8_fp_bits32(s_12)));}
goto L_1d4fc;
L_1d4fc:
/* 1d4fc: vfma.f32 s14, s13, s12 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_12), m8_fp_bits32(s_14)));}
goto L_1d500;
L_1d500:
/* 1d500: vmov.f32 s12, #1.000000e+00 */ {s_12=m8_fp_float(0x3f800000u);}
goto L_1d504;
L_1d504:
/* 1d504: vfma.f32 s15, s13, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));}
goto L_1d508;
L_1d508:
/* 1d508: vfms.f32 s10, s6, s14 */ {s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_6) ^ 0x80000000u), m8_fp_bits32(s_14), m8_fp_bits32(s_10)));}
goto L_1d50c;
L_1d50c:
/* 1d50c: vmov.f32 s6, s12 */ {s_6=m8_fp_float(m8_fp_bits32(s_12));}
goto L_1d510;
L_1d510:
/* 1d510: vsub.f32 s10, s10, s15 */ {s_10=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_10), m8_fp_bits32(s_15), 0));}
goto L_1d514;
L_1d514:
/* 1d514: vfma.f32 s14, s13, s10 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_10), m8_fp_bits32(s_14)));}
goto L_1d518;
L_1d518:
/* 1d518: vfma.f32 s15, s13, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_14), m8_fp_bits32(s_15)));}
goto L_1d51c;
L_1d51c:
/* 1d51c: vmaxnm.f32 s14, s14, s7 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_14), m8_fp_bits32(s_7), 0));}
goto L_1d520;
L_1d520:
/* 1d520: vminnm.f32 s14, s14, s8 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_14), m8_fp_bits32(s_8), 0));}
goto L_1d524;
L_1d524:
/* 1d524: vabs.f32 s13, s14 */ {s_13=m8_fp_float((m8_fp_bits32(s_14) & ~0x80000000u));}
goto L_1d528;
L_1d528:
/* 1d528: vfms.f32 s6, s13, s9 */ {s_6=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_13) ^ 0x80000000u), m8_fp_bits32(s_9), m8_fp_bits32(s_6)));}
goto L_1d52c;
L_1d52c:
/* 1d52c: vmov.f32 s13, s12 */ {s_13=m8_fp_float(m8_fp_bits32(s_12));}
goto L_1d530;
L_1d530:
/* 1d530: vmaxnm.f32 s15, s15, s7 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MAXNM, m8_fp_bits32(s_15), m8_fp_bits32(s_7), 0));}
goto L_1d534;
L_1d534:
/* 1d534: vminnm.f32 s15, s15, s8 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MINNM, m8_fp_bits32(s_15), m8_fp_bits32(s_8), 0));}
goto L_1d538;
L_1d538:
/* 1d538: vabs.f32 s8, s15 */ {s_8=m8_fp_float((m8_fp_bits32(s_15) & ~0x80000000u));}
goto L_1d53c;
L_1d53c:
/* 1d53c: vfms.f32 s13, s8, s9 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, (m8_fp_bits32(s_8) ^ 0x80000000u), m8_fp_bits32(s_9), m8_fp_bits32(s_13)));}
goto L_1d540;
L_1d540:
/* 1d540: vmul.f32 s14, s14, s6 */ {s_14=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_6), 0));}
goto L_1d544;
L_1d544:
/* 1d544: vstr s14, [r4, #0x20] */ {{ uint32_t fp_addr = (r_4 + 32u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_14)); }}
goto L_1d548;
L_1d548:
/* 1d548: vmul.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1d54c;
L_1d54c:
/* 1d54c: vstr s15, [r4, #0x24] */ {{ uint32_t fp_addr = (r_4 + 36u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); }}
goto L_1d550;
L_1d550:
/* 1d550: beq #0x1d566 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d566;}}
goto L_1d552;
L_1d552:
/* 1d552: cmp r3, #2 */ {{ uint32_t a = r_3; uint32_t b = 0x2u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d554;
L_1d554:
/* 1d554: beq.w #0x1d9e8 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1d9e8;}}
goto L_1d558;
L_1d558:
/* 1d558: cmp r3, #3 */ {{ uint32_t a = r_3; uint32_t b = 0x3u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d55a;
L_1d55a:
/* 1d55a: beq.w #0x1df58 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1df58;}}
goto L_1d55e;
L_1d55e:
/* 1d55e: cmp r3, #4 */ {{ uint32_t a = r_3; uint32_t b = 0x4u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d560;
L_1d560:
/* 1d560: bne #0x1d566 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1d566;}}
goto L_1d562;
L_1d562:
/* 1d562: vsub.f32 s15, s16, s14 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_14), 0));}
goto L_1d566;
L_1d566:
/* 1d566: vldr s16, [r4, #0x28] */ {{ uint32_t fp_addr = (r_4 + 40u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_16=m8_fp_float(fp_value); }}
goto L_1d56a;
L_1d56a:
/* 1d56a: vldr s14, [r4, #0x2c] */ {{ uint32_t fp_addr = (r_4 + 44u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_14=m8_fp_float(fp_value); }}
goto L_1d56e;
L_1d56e:
/* 1d56e: vstr s15, [r4, #0x28] */ {{ uint32_t fp_addr = (r_4 + 40u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_15)); }}
goto L_1d572;
L_1d572:
/* 1d572: vfnms.f32 s16, s11, s14 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_11), m8_fp_bits32(s_14), (m8_fp_bits32(s_16) ^ 0x80000000u)));}
goto L_1d576;
L_1d576:
/* 1d576: vadd.f32 s16, s15, s16 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_16), 0));}
goto L_1d57a;
L_1d57a:
/* 1d57a: vstr s16, [r4, #0x2c] */ {{ uint32_t fp_addr = (r_4 + 44u); uint32_t fp_value; m8_le_write(M8_SV_HOST(fp_addr + 0u),4,m8_fp_bits32(s_16)); }}
goto L_1d57e;
L_1d57e:
/* 1d57e: b #0x1d11c */ {goto L_1d11c;}
L_1d624:
/* 1d624: ldrb.w r3, [r4, #0x63] */ {{ uint32_t base = r_4; uint32_t addr = base + (99u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1d628;
L_1d628:
/* 1d628: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d62a;
L_1d62a:
/* 1d62a: beq.w #0x1db0c */ {if(m8_condition(cpu->xpsr,1)) {goto L_1db0c;}}
goto L_1d62e;
L_1d62e:
/* 1d62e: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1d632;
L_1d632:
/* 1d632: ldr.w lr, [r4, #0x1cc] */ {{ uint32_t base = r_4; uint32_t addr = base + (460u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_14 = (uint32_t)(value); }}
goto L_1d636;
L_1d636:
/* 1d636: ldr.w r3, [r4, #0x1b0] */ {{ uint32_t base = r_4; uint32_t addr = base + (432u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1d63a;
L_1d63a:
/* 1d63a: ldr.w r6, [r4, #0x1b4] */ {{ uint32_t base = r_4; uint32_t addr = base + (436u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1d63e;
L_1d63e:
/* 1d63e: sub.w r3, r3, lr */ {{ uint32_t a = r_3; uint32_t b = r_14; uint32_t result = a - b - 0u; r_3 = (uint32_t)(result); }}
goto L_1d642;
L_1d642:
/* 1d642: sub.w sb, r6, lr */ {{ uint32_t a = r_6; uint32_t b = r_14; uint32_t result = a - b - 0u; r_9 = (uint32_t)(result); }}
goto L_1d646;
L_1d646:
/* 1d646: cmp r3, r1 */ {{ uint32_t a = r_3; uint32_t b = r_1; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d648;
L_1d648:
/* 1d64a: vldreq s7, [r4, #0xd0] */ {if(m8_condition(cpu->xpsr,1)) {{ uint32_t fp_addr = (r_4 + 208u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); }}}
goto L_1d64e;
L_1d64e:
/* 1d64e: cmp sb, r7 */ {{ uint32_t a = r_9; uint32_t b = r_7; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d650;
L_1d650:
/* 1d650: bhi #0x1d662 */ {if(m8_condition(cpu->xpsr,9)) {goto L_1d662;}}
goto L_1d652;
L_1d652:
/* 1d652: add.w r3, lr, #5 */ {{ uint32_t a = r_14; uint32_t b = 0x5u; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d656;
L_1d656:
/* 1d656: subs r3, r3, r6 */ {{ uint32_t a = r_3; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); }}
goto L_1d658;
L_1d658:
/* 1d658: add r3, r1 */ {{ uint32_t a = r_3; uint32_t b = r_1; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d65a;
L_1d65a:
/* 1d65a: add.w r3, r4, r3, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_3, 2, 2, NULL); uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d65e;
L_1d65e:
/* 1d65e: vldr s15, [r3, #0xd0] */ {{ uint32_t fp_addr = (r_3 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1d65eu;goto done;}fp_value=m8_le_read(span,4);} s_15=m8_fp_float(fp_value); }}
goto L_1d662;
L_1d662:
/* 1d662: cmp sb, ip */ {{ uint32_t a = r_9; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d664;
L_1d664:
/* 1d664: bhi #0x1d676 */ {if(m8_condition(cpu->xpsr,9)) {goto L_1d676;}}
goto L_1d666;
L_1d666:
/* 1d666: add.w r3, lr, #6 */ {{ uint32_t a = r_14; uint32_t b = 0x6u; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d66a;
L_1d66a:
/* 1d66a: subs r3, r3, r6 */ {{ uint32_t a = r_3; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); }}
goto L_1d66c;
L_1d66c:
/* 1d66c: add r3, r1 */ {{ uint32_t a = r_3; uint32_t b = r_1; uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d66e;
L_1d66e:
/* 1d66e: add.w r3, r4, r3, lsl #2 */ {{ uint32_t a = r_4; uint32_t b = m8_shift(r_3, 2, 2, NULL); uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1d672;
L_1d672:
/* 1d672: vldr s13, [r3, #0xd0] */ {{ uint32_t fp_addr = (r_3 + 208u); uint32_t fp_value; {uint8_t *span=m8_region_pointer(&mem->dtcm,fp_addr + 0u,4);if(!span)span=m8_region_pointer(&mem->ocram,fp_addr + 0u,4);if(!span){r_15=0x1d672u;goto done;}fp_value=m8_le_read(span,4);} s_13=m8_fp_float(fp_value); }}
goto L_1d676;
L_1d676:
/* 1d676: vsub.f32 s7, s15, s7 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_15), m8_fp_bits32(s_7), 0));}
goto L_1d67a;
L_1d67a:
/* 1d67a: vldr s30, [pc, #-0x1c8] */ {{ uint32_t fp_addr = (0x1d67cu - 456u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_30=m8_fp_float(fp_value); }}
goto L_1d67e;
L_1d67e:
/* 1d67e: vsub.f32 s15, s16, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));}
goto L_1d682;
L_1d682:
/* 1d682: movs r3, #0 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); }}
goto L_1d684;
L_1d684:
/* 1d684: vmov.f32 s9, #5.000000e-01 */ {s_9=m8_fp_float(0x3f000000u);}
goto L_1d688;
L_1d688:
/* 1d688: vsub.f32 s13, s13, s16 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_13), m8_fp_bits32(s_16), 0));}
goto L_1d68c;
L_1d68c:
/* 1d68c: vmov.f32 s12, s15 */ {s_12=m8_fp_float(m8_fp_bits32(s_15));}
goto L_1d690;
L_1d690:
/* 1d690: vmov.f32 s8, #1.000000e+00 */ {s_8=m8_fp_float(0x3f800000u);}
goto L_1d694;
L_1d694:
/* 1d694: vfma.f32 s12, s7, s9 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_7), m8_fp_bits32(s_9), m8_fp_bits32(s_12)));}
goto L_1d698;
L_1d698:
/* 1d698: vsub.f32 s8, s14, s8 */ {s_8=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_8), 0));}
goto L_1d69c;
L_1d69c:
/* 1d69c: vadd.f32 s15, s15, s12 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_12), 0));}
goto L_1d6a0;
L_1d6a0:
/* 1d6a0: vfma.f32 s15, s13, s9 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_9), m8_fp_bits32(s_15)));}
goto L_1d6a4;
L_1d6a4:
/* 1d6a4: vfnms.f32 s12, s8, s15 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_8), m8_fp_bits32(s_15), (m8_fp_bits32(s_12) ^ 0x80000000u)));}
goto L_1d6a8;
L_1d6a8:
/* 1d6a8: vmul.f32 s12, s14, s12 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_12), 0));}
goto L_1d6ac;
L_1d6ac:
/* 1d6ac: vfma.f32 s12, s7, s9 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_7), m8_fp_bits32(s_9), m8_fp_bits32(s_12)));}
goto L_1d6b0;
L_1d6b0:
/* 1d6b0: vfma.f32 s16, s14, s12 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_12), m8_fp_bits32(s_16)));}
goto L_1d6b4;
L_1d6b4:
/* 1d6b4: b #0x1cf76 */ {goto L_1cf76;}
L_1d816:
/* 1d816: vldr s9, [pc, #-0x364] */ {{ uint32_t fp_addr = (0x1d818u - 868u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_9=m8_fp_float(fp_value); }}
goto L_1d81a;
L_1d81a:
/* 1d81a: b.w #0x1ce60 */ {goto L_1ce60;}
L_1d916:
/* 1d916: vldr s11, [pc, #0x398] */ {{ uint32_t fp_addr = (0x1d918u + 920u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); }}
goto L_1d91a;
L_1d91a:
/* 1d91a: vsub.f32 s11, s6, s11 */ {s_11=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_6), m8_fp_bits32(s_11), 0));}
goto L_1d91e;
L_1d91e:
/* 1d91e: vmov.f32 s6, s15 */ {s_6=m8_fp_float(m8_fp_bits32(s_15));}
goto L_1d922;
L_1d922:
/* 1d922: b #0x1d4da */ {goto L_1d4da;}
L_1d9e8:
/* 1d9e8: vmov.f32 s15, s10 */ {s_15=m8_fp_float(m8_fp_bits32(s_10));}
goto L_1d9ec;
L_1d9ec:
/* 1d9ec: b #0x1d566 */ {goto L_1d566;}
L_1d9ee:
if(r_3>8186u) {r_15=0x1d9eeu;goto done;}
/* 1d9ee: cmp r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1d9f0;
L_1d9f0:
/* 1d9f0: bls.w #0x1e17c */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e17c;}}
goto L_1d9f4;
L_1d9f4:
/* 1d9f4: ldr r1, [pc, #0x2ac] */ {{ uint32_t base = (0x1d9f8u & ~UINT32_C(3)); uint32_t addr = base + (684u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_1 = (uint32_t)(value); }}
goto L_1d9f6;
L_1d9f6:
/* 1d9f6: ldr.w r2, [r4, #0xbc] */ {{ uint32_t base = r_4; uint32_t addr = base + (188u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); }}
goto L_1d9fa;
L_1d9fa:
/* 1d9fa: add r1, r3 */ {{ uint32_t a = r_1; uint32_t b = r_3; uint32_t result = a + b + 0u; r_1 = (uint32_t)(result); }}
goto L_1d9fc;
L_1d9fc:
/* 1d9fc: ldrsh.w r1, [r2, r1, lsl #1] */ {{ uint32_t base = r_2; uint32_t addr = base + ((0u + m8_shift(r_1, 2, 1, NULL))); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_1 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da00;
L_1da00:
/* 1da00: vmov s6, r1 */ {s_6=m8_fp_float(r_1);}
goto L_1da04;
L_1da04:
/* 1da04: vcvt.f32.s32 s6, s6 */ {s_6=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_6), 0, 0, 32, 0, 0));}
goto L_1da08;
L_1da08:
/* 1da08: ldrsh.w r1, [r2, r3, lsl #1] */ {{ uint32_t base = r_2; uint32_t addr = base + ((0u + m8_shift(r_3, 2, 1, NULL))); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_1 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da0c;
L_1da0c:
/* 1da0c: add.w sb, r3, #2 */ {{ uint32_t a = r_3; uint32_t b = 0x2u; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1da10;
L_1da10:
/* 1da10: lsls r7, r3, #1 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x1u, &carry); m8_logic_flags(cpu, result, carry); r_7 = (uint32_t)(result); }}
goto L_1da12;
L_1da12:
/* 1da12: vmov s16, r1 */ {s_16=m8_fp_float(r_1);}
goto L_1da16;
L_1da16:
/* 1da16: ldrh.w r1, [r4, #0x1b8] */ {{ uint32_t base = r_4; uint32_t addr = base + (440u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_1 = (uint32_t)(value); }}
goto L_1da1a;
L_1da1a:
/* 1da1a: add r2, r7 */ {{ uint32_t a = r_2; uint32_t b = r_7; uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); }}
goto L_1da1c;
L_1da1c:
/* 1da1c: cmp sb, r1 */ {{ uint32_t a = r_9; uint32_t b = r_1; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1da1e;
L_1da1e:
/* 1da1e: vcvt.f32.s32 s16, s16 */ {s_16=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_16), 0, 0, 32, 0, 0));}
goto L_1da22;
L_1da22:
/* 1da22: bhs.w #0x1e0a6 */ {if(m8_condition(cpu->xpsr,3)) {goto L_1e0a6;}}
goto L_1da26;
L_1da26:
/* 1da26: ldrsh.w r6, [r2, #4] */ {{ uint32_t base = r_2; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da2a;
L_1da2a:
/* 1da2a: vmov s15, r6 */ {s_15=m8_fp_float(r_6);}
goto L_1da2e;
L_1da2e:
/* 1da2e: vcvt.f32.s32 s15, s15 */ {s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0));}
goto L_1da32;
L_1da32:
/* 1da32: adds r6, r3, #4 */ {{ uint32_t a = r_3; uint32_t b = 0x4u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_6 = (uint32_t)(result); }}
goto L_1da34;
L_1da34:
/* 1da34: cmp r1, r6 */ {{ uint32_t a = r_1; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1da36;
L_1da36:
/* 1da36: str r6, [sp, #0x10] */ {{ uint32_t base = r_13; uint32_t addr = base + (16u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1da38;
L_1da38:
/* 1da38: bls.w #0x1e096 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e096;}}
goto L_1da3c;
L_1da3c:
/* 1da3c: ldrsh.w r6, [r2, #8] */ {{ uint32_t base = r_2; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da40;
L_1da40:
/* 1da40: vmov s3, r6 */ {s_3=m8_fp_float(r_6);}
goto L_1da44;
L_1da44:
/* 1da44: vcvt.f32.s32 s3, s3 */ {s_3=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_3), 0, 0, 32, 0, 0));}
goto L_1da48;
L_1da48:
/* 1da48: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1da4a;
L_1da4a:
/* 1da4a: beq.w #0x1e0a0 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1e0a0;}}
goto L_1da4e;
L_1da4e:
/* 1da4e: ldrsh r6, [r2, #-0x2] */ {{ uint32_t base = r_2; uint32_t addr = base + (4294967294u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da52;
L_1da52:
/* 1da52: vmov s8, r6 */ {s_8=m8_fp_float(r_6);}
goto L_1da56;
L_1da56:
/* 1da56: vcvt.f32.s32 s8, s8 */ {s_8=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_8), 0, 0, 32, 0, 0));}
goto L_1da5a;
L_1da5a:
/* 1da5a: ldrsh.w r6, [r2, #2] */ {{ uint32_t base = r_2; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da5e;
L_1da5e:
/* 1da5e: add.w ip, r3, #3 */ {{ uint32_t a = r_3; uint32_t b = 0x3u; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1da62;
L_1da62:
/* 1da62: add.w lr, r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = a + b + 0u; r_14 = (uint32_t)(result); }}
goto L_1da66;
L_1da66:
/* 1da66: vmov s17, r6 */ {s_17=m8_fp_float(r_6);}
goto L_1da6a;
L_1da6a:
/* 1da6a: cmp r1, ip */ {{ uint32_t a = r_1; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1da6c;
L_1da6c:
/* 1da6c: vcvt.f32.s32 s17, s17 */ {s_17=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_17), 0, 0, 32, 0, 0));}
goto L_1da70;
L_1da70:
/* 1da70: bls.w #0x1e0cc */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e0cc;}}
goto L_1da74;
L_1da74:
/* 1da74: ldrsh.w r6, [r2, #6] */ {{ uint32_t base = r_2; uint32_t addr = base + (6u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_6 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da78;
L_1da78:
/* 1da78: vmov s12, r6 */ {s_12=m8_fp_float(r_6);}
goto L_1da7c;
L_1da7c:
/* 1da7c: vcvt.f32.s32 s12, s12 */ {s_12=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_12), 0, 0, 32, 0, 0));}
goto L_1da80;
L_1da80:
/* 1da80: adds r6, r3, #5 */ {{ uint32_t a = r_3; uint32_t b = 0x5u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_6 = (uint32_t)(result); }}
goto L_1da82;
L_1da82:
/* 1da82: cmp r1, r6 */ {{ uint32_t a = r_1; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1da84;
L_1da84:
/* 1da84: str r6, [sp, #0xc] */ {{ uint32_t base = r_13; uint32_t addr = base + (12u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1da86;
L_1da86:
/* 1da86: bls.w #0x1d816 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d816;}}
goto L_1da8a;
L_1da8a:
/* 1da8a: ldrsh.w r2, [r2, #0xa] */ {{ uint32_t base = r_2; uint32_t addr = base + (10u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_2 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1da8e;
L_1da8e:
/* 1da8e: vmov s9, r2 */ {s_9=m8_fp_float(r_2);}
goto L_1da92;
L_1da92:
/* 1da92: vcvt.f32.s32 s9, s9 */ {s_9=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_9), 0, 0, 32, 0, 0));}
goto L_1da96;
L_1da96:
/* 1da96: b.w #0x1ce60 */ {goto L_1ce60;}
L_1db0c:
/* 1db0c: ldrb.w r3, [r4, #0x59] */ {{ uint32_t base = r_4; uint32_t addr = base + (89u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1db10;
L_1db10:
/* 1db10: cbz r3, #0x1db1c */ {if (r_3 == 0) { goto L_1db1c; }}
goto L_1db12;
L_1db12:
/* 1db12: ldr.w r3, [r4, #0x1a4] */ {{ uint32_t base = r_4; uint32_t addr = base + (420u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1db16;
L_1db16:
/* 1db16: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db18;
L_1db18:
/* 1db18: bne.w #0x1e1c8 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1e1c8;}}
goto L_1db1c;
L_1db1c:
/* 1db1c: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1db20;
L_1db20:
/* 1db20: cmp r1, #0 */ {{ uint32_t a = r_1; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db22;
L_1db22:
/* 1db22: vldr s12, [pc, #0x184] */ {{ uint32_t fp_addr = (0x1db24u + 388u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1db26;
L_1db26:
/* 1db26: vseleq.f32 s7, s12, s7 */ {s_7=m8_fp_float(((cpu->xpsr & 0x40000000u)) ? m8_fp_bits32(s_12) : m8_fp_bits32(s_7));}
goto L_1db2a;
L_1db2a:
/* 1db2a: cmp lr, r7 */ {{ uint32_t a = r_14; uint32_t b = r_7; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db2c;
L_1db2c:
/* 1db2c: bls.w #0x1e15e */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e15e;}}
goto L_1db30;
L_1db30:
/* 1db30: vsub.f32 s9, s16, s15 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));}
goto L_1db34;
L_1db34:
/* 1db34: vmov.f32 s8, #5.000000e-01 */ {s_8=m8_fp_float(0x3f000000u);}
goto L_1db38;
L_1db38:
/* 1db38: cmp lr, ip */ {{ uint32_t a = r_14; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db3a;
L_1db3a:
/* 1db3a: vsub.f32 s15, s15, s7 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_15), m8_fp_bits32(s_7), 0));}
goto L_1db3e;
L_1db3e:
/* 1db3e: vldr s30, [pc, #0x168] */ {{ uint32_t fp_addr = (0x1db40u + 360u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_30=m8_fp_float(fp_value); }}
goto L_1db42;
L_1db42:
/* 1db42: vmov.f32 s12, s9 */ {s_12=m8_fp_float(m8_fp_bits32(s_9));}
goto L_1db46;
L_1db46:
/* 1db46: mov.w r3, #0 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_3 = (uint32_t)(result); }}
goto L_1db4a;
L_1db4a:
/* 1db4c: vmovls.f32 s13, s30 */ {if(m8_condition(cpu->xpsr,10)) {s_13=m8_fp_float(m8_fp_bits32(s_30));}}
goto L_1db50;
L_1db50:
/* 1db50: vfma.f32 s12, s15, s8 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_8), m8_fp_bits32(s_12)));}
goto L_1db54;
L_1db54:
/* 1db54: vsub.f32 s13, s13, s16 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_13), m8_fp_bits32(s_16), 0));}
goto L_1db58;
L_1db58:
/* 1db58: vadd.f32 s9, s12, s9 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_12), m8_fp_bits32(s_9), 0));}
goto L_1db5c;
L_1db5c:
/* 1db5c: vfma.f32 s9, s13, s8 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_8), m8_fp_bits32(s_9)));}
goto L_1db60;
L_1db60:
/* 1db60: vmov.f32 s13, #1.000000e+00 */ {s_13=m8_fp_float(0x3f800000u);}
goto L_1db64;
L_1db64:
/* 1db64: vsub.f32 s13, s14, s13 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));}
goto L_1db68;
L_1db68:
/* 1db68: vfnms.f32 s12, s13, s9 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_9), (m8_fp_bits32(s_12) ^ 0x80000000u)));}
goto L_1db6c;
L_1db6c:
/* 1db6c: vmul.f32 s12, s14, s12 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_12), 0));}
goto L_1db70;
L_1db70:
/* 1db70: vfma.f32 s12, s15, s8 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_8), m8_fp_bits32(s_12)));}
goto L_1db74;
L_1db74:
/* 1db74: vfma.f32 s16, s14, s12 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_12), m8_fp_bits32(s_16)));}
goto L_1db78;
L_1db78:
/* 1db78: b.w #0x1cf76 */ {goto L_1cf76;}
L_1db7c:
/* 1db7c: ldrb.w r2, [r4, #0x59] */ {{ uint32_t base = r_4; uint32_t addr = base + (89u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1db80;
L_1db80:
/* 1db80: cbz r2, #0x1db8c */ {if (r_2 == 0) { goto L_1db8c; }}
goto L_1db82;
L_1db82:
/* 1db82: ldr.w r2, [r4, #0x1a4] */ {{ uint32_t base = r_4; uint32_t addr = base + (420u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); }}
goto L_1db86;
L_1db86:
/* 1db86: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db88;
L_1db88:
/* 1db88: bne.w #0x1e1ba */ {if(m8_condition(cpu->xpsr,2)) {goto L_1e1ba;}}
goto L_1db8c;
L_1db8c:
/* 1db8c: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1db90;
L_1db90:
/* 1db90: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db92;
L_1db92:
/* 1db92: vldr s13, [pc, #0x114] */ {{ uint32_t fp_addr = (0x1db94u + 276u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1db96;
L_1db96:
/* 1db96: vseleq.f32 s6, s13, s6 */ {s_6=m8_fp_float(((cpu->xpsr & 0x40000000u)) ? m8_fp_bits32(s_13) : m8_fp_bits32(s_6));}
goto L_1db9a;
L_1db9a:
/* 1db9a: cmp r1, lr */ {{ uint32_t a = r_1; uint32_t b = r_14; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1db9c;
L_1db9c:
/* 1db9c: bls.w #0x1e154 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e154;}}
goto L_1dba0;
L_1dba0:
/* 1dba0: vsub.f32 s7, s16, s15 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_16), m8_fp_bits32(s_15), 0));}
goto L_1dba4;
L_1dba4:
/* 1dba4: vsub.f32 s15, s15, s6 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_15), m8_fp_bits32(s_6), 0));}
goto L_1dba8;
L_1dba8:
/* 1dba8: cmp r1, sb */ {{ uint32_t a = r_1; uint32_t b = r_9; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dbaa;
L_1dbaa:
/* 1dbaa: vmov.f32 s5, #5.000000e-01 */ {s_5=m8_fp_float(0x3f000000u);}
goto L_1dbae;
L_1dbae:
/* 1dbae: vldr s4, [pc, #0xf8] */ {{ uint32_t fp_addr = (0x1dbb0u + 248u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_4=m8_fp_float(fp_value); }}
goto L_1dbb2;
L_1dbb2:
/* 1dbb2: vmov.f32 s13, s7 */ {s_13=m8_fp_float(m8_fp_bits32(s_7));}
goto L_1dbb6;
L_1dbb6:
/* 1dbb8: vmovls.f32 s3, s4 */ {if(m8_condition(cpu->xpsr,10)) {s_3=m8_fp_float(m8_fp_bits32(s_4));}}
goto L_1dbbc;
L_1dbbc:
/* 1dbbc: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dbbe;
L_1dbbe:
/* 1dbbe: vmov.f32 s6, #1.000000e+00 */ {s_6=m8_fp_float(0x3f800000u);}
goto L_1dbc2;
L_1dbc2:
/* 1dbc2: vfma.f32 s13, s15, s5 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_5), m8_fp_bits32(s_13)));}
goto L_1dbc6;
L_1dbc6:
/* 1dbc6: vseleq.f32 s8, s4, s8 */ {s_8=m8_fp_float(((cpu->xpsr & 0x40000000u)) ? m8_fp_bits32(s_4) : m8_fp_bits32(s_8));}
goto L_1dbca;
L_1dbca:
/* 1dbca: vsub.f32 s3, s3, s16 */ {s_3=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_3), m8_fp_bits32(s_16), 0));}
goto L_1dbce;
L_1dbce:
/* 1dbce: cmp r1, ip */ {{ uint32_t a = r_1; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dbd0;
L_1dbd0:
/* 1dbd0: vsub.f32 s6, s14, s6 */ {s_6=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_14), m8_fp_bits32(s_6), 0));}
goto L_1dbd4;
L_1dbd4:
/* 1dbd4: vadd.f32 s7, s13, s7 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_13), m8_fp_bits32(s_7), 0));}
goto L_1dbd8;
L_1dbd8:
/* 1dbd8: vfma.f32 s7, s3, s5 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_3), m8_fp_bits32(s_5), m8_fp_bits32(s_7)));}
goto L_1dbdc;
L_1dbdc:
/* 1dbdc: vfnms.f32 s13, s6, s7 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_6), m8_fp_bits32(s_7), (m8_fp_bits32(s_13) ^ 0x80000000u)));}
goto L_1dbe0;
L_1dbe0:
/* 1dbe0: vmul.f32 s13, s14, s13 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_13), 0));}
goto L_1dbe4;
L_1dbe4:
/* 1dbe4: vfma.f32 s13, s15, s5 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_15), m8_fp_bits32(s_5), m8_fp_bits32(s_13)));}
goto L_1dbe8;
L_1dbe8:
/* 1dbe8: vfma.f32 s16, s14, s13 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_14), m8_fp_bits32(s_13), m8_fp_bits32(s_16)));}
goto L_1dbec;
L_1dbec:
/* 1dbec: bls.w #0x1e14a */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e14a;}}
goto L_1dbf0;
L_1dbf0:
/* 1dbf0: vsub.f32 s7, s17, s12 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_17), m8_fp_bits32(s_12), 0));}
goto L_1dbf4;
L_1dbf4:
/* 1dbf4: ldr r3, [sp, #0xc] */ {{ uint32_t base = r_13; uint32_t addr = base + (12u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_3 = (uint32_t)(value); }}
goto L_1dbf6;
L_1dbf6:
/* 1dbf6: vsub.f32 s13, s12, s8 */ {s_13=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_12), m8_fp_bits32(s_8), 0));}
goto L_1dbfa;
L_1dbfa:
/* 1dbfa: vldr s15, [pc, #0xac] */ {{ uint32_t fp_addr = (0x1dbfcu + 172u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1dbfe;
L_1dbfe:
/* 1dbfe: vmov.f32 s8, #5.000000e-01 */ {s_8=m8_fp_float(0x3f000000u);}
goto L_1dc02;
L_1dc02:
/* 1dc02: cmp r1, r3 */ {{ uint32_t a = r_1; uint32_t b = r_3; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dc04;
L_1dc04:
/* 1dc06: vmovls.f32 s9, s15 */ {if(m8_condition(cpu->xpsr,10)) {s_9=m8_fp_float(m8_fp_bits32(s_15));}}
goto L_1dc0a;
L_1dc0a:
/* 1dc0a: vmov.f32 s15, s7 */ {s_15=m8_fp_float(m8_fp_bits32(s_7));}
goto L_1dc0e;
L_1dc0e:
/* 1dc0e: vsub.f32 s9, s9, s17 */ {s_9=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_9), m8_fp_bits32(s_17), 0));}
goto L_1dc12;
L_1dc12:
/* 1dc12: vfma.f32 s15, s13, s8 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_8), m8_fp_bits32(s_15)));}
goto L_1dc16;
L_1dc16:
/* 1dc16: vadd.f32 s7, s15, s7 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_ADD, m8_fp_bits32(s_15), m8_fp_bits32(s_7), 0));}
goto L_1dc1a;
L_1dc1a:
/* 1dc1a: vfma.f32 s7, s9, s8 */ {s_7=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_9), m8_fp_bits32(s_8), m8_fp_bits32(s_7)));}
goto L_1dc1e;
L_1dc1e:
/* 1dc1e: vfnms.f32 s15, s6, s7 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_6), m8_fp_bits32(s_7), (m8_fp_bits32(s_15) ^ 0x80000000u)));}
goto L_1dc22;
L_1dc22:
/* 1dc22: vmul.f32 s15, s14, s15 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_14), m8_fp_bits32(s_15), 0));}
goto L_1dc26;
L_1dc26:
/* 1dc26: vfma.f32 s15, s13, s8 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_FMA, m8_fp_bits32(s_13), m8_fp_bits32(s_8), m8_fp_bits32(s_15)));}
goto L_1dc2a;
L_1dc2a:
/* 1dc2a: b.w #0x1cf64 */ {goto L_1cf64;}
L_1dcf0:
if(r_1>5458u) {r_15=0x1dcf0u;goto done;}
/* 1dcf0: cmp r1, #0 */ {{ uint32_t a = r_1; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dcf2;
L_1dcf2:
/* 1dcf2: beq.w #0x1e0fa */ {if(m8_condition(cpu->xpsr,1)) {goto L_1e0fa;}}
goto L_1dcf6;
L_1dcf6:
/* 1dcf6: subs r3, r1, #1 */ {{ uint32_t a = r_1; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_3 = (uint32_t)(result); }}
goto L_1dcf8;
L_1dcf8:
/* 1dcf8: ldr.w ip, [r4, #0xc4] */ {{ uint32_t base = r_4; uint32_t addr = base + (196u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_12 = (uint32_t)(value); }}
goto L_1dcfc;
L_1dcfc:
/* 1dcfc: add.w r3, r3, r3, lsl #1 */ {{ uint32_t a = r_3; uint32_t b = m8_shift(r_3, 2, 1, NULL); uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1dd00;
L_1dd00:
/* 1dd00: add.w r2, ip, r3 */ {{ uint32_t a = r_12; uint32_t b = r_3; uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); }}
goto L_1dd04;
L_1dd04:
/* 1dd04: ldrb.w r7, [ip, r3] */ {{ uint32_t base = r_12; uint32_t addr = base + ((0u + r_3)); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_7 = (uint32_t)(value); }}
goto L_1dd08;
L_1dd08:
/* 1dd08: ldrb r3, [r2, #2] */ {{ uint32_t base = r_2; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1dd0a;
L_1dd0a:
/* 1dd0a: ldrb r2, [r2, #1] */ {{ uint32_t base = r_2; uint32_t addr = base + (1u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1dd0c;
L_1dd0c:
/* 1dd0c: lsls r3, r3, #0x18 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x18u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); }}
goto L_1dd0e;
L_1dd0e:
/* 1dd0e: orr.w r3, r3, r2, lsl #16 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_2, 2, 16, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd12;
L_1dd12:
/* 1dd12: orr.w r3, r3, r7, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_7, 2, 8, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd16;
L_1dd16:
/* 1dd16: vmov s7, r3 */ {s_7=m8_fp_float(r_3);}
goto L_1dd1a;
L_1dd1a:
/* 1dd1a: vcvt.f32.s32 s7, s7, #0x10 */ {s_7=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_7), 0, 0, 32, 16, 1));}
goto L_1dd1e;
L_1dd1e:
/* 1dd1e: add.w r3, r1, r1, lsl #1 */ {{ uint32_t a = r_1; uint32_t b = m8_shift(r_1, 2, 1, NULL); uint32_t result = a + b + 0u; r_3 = (uint32_t)(result); }}
goto L_1dd22;
L_1dd22:
/* 1dd22: adds r7, r1, #1 */ {{ uint32_t a = r_1; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_7 = (uint32_t)(result); }}
goto L_1dd24;
L_1dd24:
/* 1dd24: ldrh.w lr, [r4, #0x1b8] */ {{ uint32_t base = r_4; uint32_t addr = base + (440u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_14 = (uint32_t)(value); }}
goto L_1dd28;
L_1dd28:
/* 1dd28: add.w r2, ip, r3 */ {{ uint32_t a = r_12; uint32_t b = r_3; uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); }}
goto L_1dd2c;
L_1dd2c:
/* 1dd2c: ldrb.w r3, [ip, r3] */ {{ uint32_t base = r_12; uint32_t addr = base + ((0u + r_3)); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1dd30;
L_1dd30:
/* 1dd30: vldr s13, [pc, #-0x88] */ {{ uint32_t fp_addr = (0x1dd34u - 136u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1dd34;
L_1dd34:
/* 1dd34: cmp r7, lr */ {{ uint32_t a = r_7; uint32_t b = r_14; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dd36;
L_1dd36:
/* 1dd36: ldrb.w ip, [r2, #1] */ {{ uint32_t base = r_2; uint32_t addr = base + (1u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dd3a;
L_1dd3a:
/* 1dd3a: lsl.w r3, r3, #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x8u, &carry); r_3 = (uint32_t)(result); }}
goto L_1dd3e;
L_1dd3e:
/* 1dd3e: orr.w r3, r3, ip, lsl #16 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_12, 2, 16, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd42;
L_1dd42:
/* 1dd42: ldrb.w ip, [r2, #2] */ {{ uint32_t base = r_2; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dd46;
L_1dd46:
/* 1dd46: orr.w r3, r3, ip, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_12, 2, 24, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd4a;
L_1dd4a:
/* 1dd4a: vmov s16, r3 */ {s_16=m8_fp_float(r_3);}
goto L_1dd4e;
L_1dd4e:
/* 1dd4e: vcvt.f32.s32 s16, s16 */ {s_16=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_16), 0, 0, 32, 0, 0));}
goto L_1dd52;
L_1dd52:
/* 1dd52: vmul.f32 s16, s16, s13 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_16), m8_fp_bits32(s_13), 0));}
goto L_1dd56;
L_1dd56:
/* 1dd56: bhs.w #0x1e0f4 */ {if(m8_condition(cpu->xpsr,3)) {goto L_1e0f4;}}
goto L_1dd5a;
L_1dd5a:
/* 1dd5a: ldrb r3, [r2, #4] */ {{ uint32_t base = r_2; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1dd5c;
L_1dd5c:
/* 1dd5c: ldrb.w ip, [r2, #3] */ {{ uint32_t base = r_2; uint32_t addr = base + (3u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dd60;
L_1dd60:
/* 1dd60: lsls r3, r3, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x10u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); }}
goto L_1dd62;
L_1dd62:
/* 1dd62: orr.w r3, r3, ip, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_12, 2, 8, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd66;
L_1dd66:
/* 1dd66: ldrb.w ip, [r2, #5] */ {{ uint32_t base = r_2; uint32_t addr = base + (5u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dd6a;
L_1dd6a:
/* 1dd6a: orr.w r3, r3, ip, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_12, 2, 24, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd6e;
L_1dd6e:
/* 1dd6e: vmov s15, r3 */ {s_15=m8_fp_float(r_3);}
goto L_1dd72;
L_1dd72:
/* 1dd72: vcvt.f32.s32 s15, s15 */ {s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0));}
goto L_1dd76;
L_1dd76:
/* 1dd76: vmul.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1dd7a;
L_1dd7a:
/* 1dd7a: add.w ip, r1, #2 */ {{ uint32_t a = r_1; uint32_t b = 0x2u; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1dd7e;
L_1dd7e:
/* 1dd7e: cmp lr, ip */ {{ uint32_t a = r_14; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dd80;
L_1dd80:
/* 1dd80: bls.w #0x1d810 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d810;}}
goto L_1dd84;
L_1dd84:
/* 1dd84: ldrb r3, [r2, #7] */ {{ uint32_t base = r_2; uint32_t addr = base + (7u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_3 = (uint32_t)(value); }}
goto L_1dd86;
L_1dd86:
/* 1dd86: ldrb.w sb, [r2, #6] */ {{ uint32_t base = r_2; uint32_t addr = base + (6u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_9 = (uint32_t)(value); }}
goto L_1dd8a;
L_1dd8a:
/* 1dd8a: lsls r3, r3, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_3, 2, 0x10u, &carry); m8_logic_flags(cpu, result, carry); r_3 = (uint32_t)(result); }}
goto L_1dd8c;
L_1dd8c:
/* 1dd8c: ldrb r2, [r2, #8] */ {{ uint32_t base = r_2; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1dd8e;
L_1dd8e:
/* 1dd8e: orr.w r3, r3, sb, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_9, 2, 8, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd92;
L_1dd92:
/* 1dd92: orr.w r3, r3, r2, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_3; uint32_t b = m8_shift(r_2, 2, 24, &carry); uint32_t result = a | b; r_3 = (uint32_t)(result); }}
goto L_1dd96;
L_1dd96:
/* 1dd96: vmov s13, r3 */ {s_13=m8_fp_float(r_3);}
goto L_1dd9a;
L_1dd9a:
/* 1dd9a: vcvt.f32.s32 s13, s13, #0x10 */ {s_13=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_13), 0, 0, 32, 16, 1));}
goto L_1dd9e;
L_1dd9e:
/* 1dd9e: b #0x1d61e */ {goto L_1d61e;}
L_1dda0:
if(r_3>5454u) {r_15=0x1dda0u;goto done;}
/* 1dda0: cmp r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dda2;
L_1dda2:
/* 1dda2: bls.w #0x1e168 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e168;}}
goto L_1dda6;
L_1dda6:
/* 1dda6: subs r2, r3, #2 */ {{ uint32_t a = r_3; uint32_t b = 0x2u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); r_2 = (uint32_t)(result); }}
goto L_1dda8;
L_1dda8:
/* 1dda8: ldr.w r7, [r4, #0xc4] */ {{ uint32_t base = r_4; uint32_t addr = base + (196u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_7 = (uint32_t)(value); }}
goto L_1ddac;
L_1ddac:
/* 1ddac: add.w r2, r2, r2, lsl #1 */ {{ uint32_t a = r_2; uint32_t b = m8_shift(r_2, 2, 1, NULL); uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); }}
goto L_1ddb0;
L_1ddb0:
/* 1ddb0: adds r1, r7, r2 */ {{ uint32_t a = r_7; uint32_t b = r_2; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_1 = (uint32_t)(result); }}
goto L_1ddb2;
L_1ddb2:
/* 1ddb2: ldrb.w ip, [r7, r2] */ {{ uint32_t base = r_7; uint32_t addr = base + ((0u + r_2)); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1ddb6;
L_1ddb6:
/* 1ddb6: ldrb r2, [r1, #2] */ {{ uint32_t base = r_1; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1ddb8;
L_1ddb8:
/* 1ddb8: ldrb r1, [r1, #1] */ {{ uint32_t base = r_1; uint32_t addr = base + (1u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_1 = (uint32_t)(value); }}
goto L_1ddba;
L_1ddba:
/* 1ddba: lsls r2, r2, #0x18 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_2, 2, 0x18u, &carry); m8_logic_flags(cpu, result, carry); r_2 = (uint32_t)(result); }}
goto L_1ddbc;
L_1ddbc:
/* 1ddbc: orr.w r2, r2, r1, lsl #16 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_2; uint32_t b = m8_shift(r_1, 2, 16, &carry); uint32_t result = a | b; r_2 = (uint32_t)(result); }}
goto L_1ddc0;
L_1ddc0:
/* 1ddc0: orr.w r2, r2, ip, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_2; uint32_t b = m8_shift(r_12, 2, 8, &carry); uint32_t result = a | b; r_2 = (uint32_t)(result); }}
goto L_1ddc4;
L_1ddc4:
/* 1ddc4: vmov s6, r2 */ {s_6=m8_fp_float(r_2);}
goto L_1ddc8;
L_1ddc8:
/* 1ddc8: vcvt.f32.s32 s6, s6, #0x10 */ {s_6=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_6), 0, 0, 32, 16, 1));}
goto L_1ddcc;
L_1ddcc:
/* 1ddcc: add.w ip, r3, r3, lsl #1 */ {{ uint32_t a = r_3; uint32_t b = m8_shift(r_3, 2, 1, NULL); uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1ddd0;
L_1ddd0:
/* 1ddd0: add.w sb, r3, #2 */ {{ uint32_t a = r_3; uint32_t b = 0x2u; uint32_t result = a + b + 0u; r_9 = (uint32_t)(result); }}
goto L_1ddd4;
L_1ddd4:
/* 1ddd4: ldrh.w r1, [r4, #0x1b8] */ {{ uint32_t base = r_4; uint32_t addr = base + (440u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_1 = (uint32_t)(value); }}
goto L_1ddd8;
L_1ddd8:
/* 1ddd8: add.w r2, r7, ip */ {{ uint32_t a = r_7; uint32_t b = r_12; uint32_t result = a + b + 0u; r_2 = (uint32_t)(result); }}
goto L_1dddc;
L_1dddc:
/* 1dddc: ldrb.w ip, [r7, ip] */ {{ uint32_t base = r_7; uint32_t addr = base + ((0u + r_12)); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dde0;
L_1dde0:
/* 1dde0: vldr s13, [pc, #-0x138] */ {{ uint32_t fp_addr = (0x1dde4u - 312u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_13=m8_fp_float(fp_value); }}
goto L_1dde4;
L_1dde4:
/* 1dde4: cmp sb, r1 */ {{ uint32_t a = r_9; uint32_t b = r_1; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1dde6;
L_1dde6:
/* 1dde6: ldrb.w lr, [r2, #1] */ {{ uint32_t base = r_2; uint32_t addr = base + (1u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1ddea;
L_1ddea:
/* 1ddea: lsl.w ip, ip, #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_12, 2, 0x8u, &carry); r_12 = (uint32_t)(result); }}
goto L_1ddee;
L_1ddee:
/* 1ddee: orr.w ip, ip, lr, lsl #16 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 16, &carry); uint32_t result = a | b; r_12 = (uint32_t)(result); }}
goto L_1ddf2;
L_1ddf2:
/* 1ddf2: ldrb.w lr, [r2, #2] */ {{ uint32_t base = r_2; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1ddf6;
L_1ddf6:
/* 1ddf6: orr.w r6, ip, lr, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 24, &carry); uint32_t result = a | b; r_6 = (uint32_t)(result); }}
goto L_1ddfa;
L_1ddfa:
/* 1ddfa: vmov s16, r6 */ {s_16=m8_fp_float(r_6);}
goto L_1ddfe;
L_1ddfe:
/* 1ddfe: vcvt.f32.s32 s16, s16 */ {s_16=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_16), 0, 0, 32, 0, 0));}
goto L_1de02;
L_1de02:
/* 1de02: vmul.f32 s16, s16, s13 */ {s_16=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_16), m8_fp_bits32(s_13), 0));}
goto L_1de06;
L_1de06:
/* 1de06: bhs.w #0x1e104 */ {if(m8_condition(cpu->xpsr,3)) {goto L_1e104;}}
goto L_1de0a;
L_1de0a:
/* 1de0a: ldrb.w ip, [r2, #7] */ {{ uint32_t base = r_2; uint32_t addr = base + (7u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1de0e;
L_1de0e:
/* 1de0e: ldrb.w lr, [r2, #6] */ {{ uint32_t base = r_2; uint32_t addr = base + (6u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1de12;
L_1de12:
/* 1de12: lsl.w ip, ip, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_12, 2, 0x10u, &carry); r_12 = (uint32_t)(result); }}
goto L_1de16;
L_1de16:
/* 1de16: orr.w ip, ip, lr, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 8, &carry); uint32_t result = a | b; r_12 = (uint32_t)(result); }}
goto L_1de1a;
L_1de1a:
/* 1de1a: ldrb.w lr, [r2, #8] */ {{ uint32_t base = r_2; uint32_t addr = base + (8u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1de1e;
L_1de1e:
/* 1de1e: orr.w r6, ip, lr, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 24, &carry); uint32_t result = a | b; r_6 = (uint32_t)(result); }}
goto L_1de22;
L_1de22:
/* 1de22: vmov s15, r6 */ {s_15=m8_fp_float(r_6);}
goto L_1de26;
L_1de26:
/* 1de26: vcvt.f32.s32 s15, s15 */ {s_15=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_15), 0, 0, 32, 0, 0));}
goto L_1de2a;
L_1de2a:
/* 1de2a: vmul.f32 s15, s15, s13 */ {s_15=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_15), m8_fp_bits32(s_13), 0));}
goto L_1de2e;
L_1de2e:
/* 1de2e: adds r6, r3, #4 */ {{ uint32_t a = r_3; uint32_t b = 0x4u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_6 = (uint32_t)(result); }}
goto L_1de30;
L_1de30:
/* 1de30: cmp r1, r6 */ {{ uint32_t a = r_1; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1de32;
L_1de32:
/* 1de32: str r6, [sp, #0x10] */ {{ uint32_t base = r_13; uint32_t addr = base + (16u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1de34;
L_1de34:
/* 1de34: bls.w #0x1e0ee */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e0ee;}}
goto L_1de38;
L_1de38:
/* 1de38: ldrb.w ip, [r2, #0xd] */ {{ uint32_t base = r_2; uint32_t addr = base + (13u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1de3c;
L_1de3c:
/* 1de3c: ldrb.w lr, [r2, #0xc] */ {{ uint32_t base = r_2; uint32_t addr = base + (12u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1de40;
L_1de40:
/* 1de40: lsl.w ip, ip, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_12, 2, 0x10u, &carry); r_12 = (uint32_t)(result); }}
goto L_1de44;
L_1de44:
/* 1de44: orr.w ip, ip, lr, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 8, &carry); uint32_t result = a | b; r_12 = (uint32_t)(result); }}
goto L_1de48;
L_1de48:
/* 1de48: ldrb.w lr, [r2, #0xe] */ {{ uint32_t base = r_2; uint32_t addr = base + (14u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1de4c;
L_1de4c:
/* 1de4c: orr.w r6, ip, lr, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_12; uint32_t b = m8_shift(r_14, 2, 24, &carry); uint32_t result = a | b; r_6 = (uint32_t)(result); }}
goto L_1de50;
L_1de50:
/* 1de50: vmov s3, r6 */ {s_3=m8_fp_float(r_6);}
goto L_1de54;
L_1de54:
/* 1de54: vcvt.f32.s32 s3, s3, #0x10 */ {s_3=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_3), 0, 0, 32, 16, 1));}
goto L_1de58;
L_1de58:
/* 1de58: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1de5a;
L_1de5a:
/* 1de5a: beq.w #0x1e0d8 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1e0d8;}}
goto L_1de5e;
L_1de5e:
/* 1de5e: add.w lr, r3, #-1 */ {{ uint32_t a = r_3; uint32_t b = 0xffffffffu; uint32_t result = a + b + 0u; r_14 = (uint32_t)(result); }}
goto L_1de62;
L_1de62:
/* 1de62: add.w lr, lr, lr, lsl #1 */ {{ uint32_t a = r_14; uint32_t b = m8_shift(r_14, 2, 1, NULL); uint32_t result = a + b + 0u; r_14 = (uint32_t)(result); }}
goto L_1de66;
L_1de66:
/* 1de66: add.w ip, r7, lr */ {{ uint32_t a = r_7; uint32_t b = r_14; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1de6a;
L_1de6a:
/* 1de6a: ldrb.w r7, [r7, lr] */ {{ uint32_t base = r_7; uint32_t addr = base + ((0u + r_14)); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_7 = (uint32_t)(value); }}
goto L_1de6e;
L_1de6e:
/* 1de6e: ldrb.w lr, [ip, #1] */ {{ uint32_t base = r_12; uint32_t addr = base + (1u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_14 = (uint32_t)(value); }}
goto L_1de72;
L_1de72:
/* 1de72: lsls r7, r7, #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_7, 2, 0x8u, &carry); m8_logic_flags(cpu, result, carry); r_7 = (uint32_t)(result); }}
goto L_1de74;
L_1de74:
/* 1de74: ldrb.w ip, [ip, #2] */ {{ uint32_t base = r_12; uint32_t addr = base + (2u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1de78;
L_1de78:
/* 1de78: orr.w r7, r7, lr, lsl #16 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_14, 2, 16, &carry); uint32_t result = a | b; r_7 = (uint32_t)(result); }}
goto L_1de7c;
L_1de7c:
/* 1de7c: orr.w r6, r7, ip, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_12, 2, 24, &carry); uint32_t result = a | b; r_6 = (uint32_t)(result); }}
goto L_1de80;
L_1de80:
/* 1de80: vmov s8, r6 */ {s_8=m8_fp_float(r_6);}
goto L_1de84;
L_1de84:
/* 1de84: vcvt.f32.s32 s8, s8, #0x10 */ {s_8=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_8), 0, 0, 32, 16, 1));}
goto L_1de88;
L_1de88:
/* 1de88: ldrb r7, [r2, #4] */ {{ uint32_t base = r_2; uint32_t addr = base + (4u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_7 = (uint32_t)(value); }}
goto L_1de8a;
L_1de8a:
/* 1de8a: add.w lr, r3, #1 */ {{ uint32_t a = r_3; uint32_t b = 0x1u; uint32_t result = a + b + 0u; r_14 = (uint32_t)(result); }}
goto L_1de8e;
L_1de8e:
/* 1de8e: ldrb.w ip, [r2, #3] */ {{ uint32_t base = r_2; uint32_t addr = base + (3u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1de92;
L_1de92:
/* 1de92: lsls r7, r7, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_7, 2, 0x10u, &carry); m8_logic_flags(cpu, result, carry); r_7 = (uint32_t)(result); }}
goto L_1de94;
L_1de94:
/* 1de94: vldr s12, [pc, #-0x1ec] */ {{ uint32_t fp_addr = (0x1de98u - 492u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1de98;
L_1de98:
/* 1de98: orr.w r7, r7, ip, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_12, 2, 8, &carry); uint32_t result = a | b; r_7 = (uint32_t)(result); }}
goto L_1de9c;
L_1de9c:
/* 1de9c: ldrb.w ip, [r2, #5] */ {{ uint32_t base = r_2; uint32_t addr = base + (5u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_12 = (uint32_t)(value); }}
goto L_1dea0;
L_1dea0:
/* 1dea0: orr.w r7, r7, ip, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_12, 2, 24, &carry); uint32_t result = a | b; r_7 = (uint32_t)(result); }}
goto L_1dea4;
L_1dea4:
/* 1dea4: add.w ip, r3, #3 */ {{ uint32_t a = r_3; uint32_t b = 0x3u; uint32_t result = a + b + 0u; r_12 = (uint32_t)(result); }}
goto L_1dea8;
L_1dea8:
/* 1dea8: vmov s13, r7 */ {s_13=m8_fp_float(r_7);}
goto L_1deac;
L_1deac:
/* 1deac: cmp r1, ip */ {{ uint32_t a = r_1; uint32_t b = r_12; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1deae;
L_1deae:
/* 1deae: vcvt.f32.s32 s17, s13 */ {s_17=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 0, 0, 32, 0, 0));}
goto L_1deb2;
L_1deb2:
/* 1deb2: vmul.f32 s17, s17, s12 */ {s_17=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_17), m8_fp_bits32(s_12), 0));}
goto L_1deb6;
L_1deb6:
/* 1deb6: bls.w #0x1e0d2 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1e0d2;}}
goto L_1deba;
L_1deba:
/* 1deba: ldrb r7, [r2, #0xa] */ {{ uint32_t base = r_2; uint32_t addr = base + (10u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_7 = (uint32_t)(value); }}
goto L_1debc;
L_1debc:
/* 1debc: ldrb r6, [r2, #9] */ {{ uint32_t base = r_2; uint32_t addr = base + (9u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_6 = (uint32_t)(value); }}
goto L_1debe;
L_1debe:
/* 1debe: lsls r7, r7, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_7, 2, 0x10u, &carry); m8_logic_flags(cpu, result, carry); r_7 = (uint32_t)(result); }}
goto L_1dec0;
L_1dec0:
/* 1dec0: orr.w r7, r7, r6, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_6, 2, 8, &carry); uint32_t result = a | b; r_7 = (uint32_t)(result); }}
goto L_1dec4;
L_1dec4:
/* 1dec4: ldrb r6, [r2, #0xb] */ {{ uint32_t base = r_2; uint32_t addr = base + (11u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_6 = (uint32_t)(value); }}
goto L_1dec6;
L_1dec6:
/* 1dec6: orr.w r7, r7, r6, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_7; uint32_t b = m8_shift(r_6, 2, 24, &carry); uint32_t result = a | b; r_7 = (uint32_t)(result); }}
goto L_1deca;
L_1deca:
/* 1deca: vmov s13, r7 */ {s_13=m8_fp_float(r_7);}
goto L_1dece;
L_1dece:
/* 1dece: vcvt.f32.s32 s13, s13 */ {s_13=m8_fp_float(m8_fp_from_int(cpu, m8_fp_bits32(s_13), 0, 0, 32, 0, 0));}
goto L_1ded2;
L_1ded2:
/* 1ded2: vmul.f32 s12, s13, s12 */ {s_12=m8_fp_float(m8_fp_eval32(cpu, M8_FP_MUL, m8_fp_bits32(s_13), m8_fp_bits32(s_12), 0));}
goto L_1ded6;
L_1ded6:
/* 1ded6: adds r6, r3, #5 */ {{ uint32_t a = r_3; uint32_t b = 0x5u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_6 = (uint32_t)(result); }}
goto L_1ded8;
L_1ded8:
/* 1ded8: cmp r1, r6 */ {{ uint32_t a = r_1; uint32_t b = r_6; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1deda;
L_1deda:
/* 1deda: str r6, [sp, #0xc] */ {{ uint32_t base = r_13; uint32_t addr = base + (12u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1dedc;
L_1dedc:
/* 1dedc: bls.w #0x1d816 */ {if(m8_condition(cpu->xpsr,10)) {goto L_1d816;}}
goto L_1dee0;
L_1dee0:
/* 1dee0: ldrb r6, [r2, #0x10] */ {{ uint32_t base = r_2; uint32_t addr = base + (16u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_6 = (uint32_t)(value); }}
goto L_1dee2;
L_1dee2:
/* 1dee2: lsls r6, r6, #0x10 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t result = m8_shift(r_6, 2, 0x10u, &carry); m8_logic_flags(cpu, result, carry); r_6 = (uint32_t)(result); }}
goto L_1dee4;
L_1dee4:
/* 1dee4: str r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value = r_6; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1dee6;
L_1dee6:
/* 1dee6: ldrb r6, [r2, #0xf] */ {{ uint32_t base = r_2; uint32_t addr = base + (15u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_6 = (uint32_t)(value); }}
goto L_1dee8;
L_1dee8:
/* 1dee8: ldrb r2, [r2, #0x11] */ {{ uint32_t base = r_2; uint32_t addr = base + (17u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1deea;
L_1deea:
/* 1deea: mov r7, r6 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = r_6; uint32_t result = b; r_7 = (uint32_t)(result); }}
goto L_1deec;
L_1deec:
/* 1deec: ldr r6, [sp, #0x18] */ {{ uint32_t base = r_13; uint32_t addr = base + (24u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_6 = (uint32_t)(value); }}
goto L_1deee;
L_1deee:
/* 1deee: orr.w r6, r6, r7, lsl #8 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_6; uint32_t b = m8_shift(r_7, 2, 8, &carry); uint32_t result = a | b; r_6 = (uint32_t)(result); }}
goto L_1def2;
L_1def2:
/* 1def2: orr.w r2, r6, r2, lsl #24 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_6; uint32_t b = m8_shift(r_2, 2, 24, &carry); uint32_t result = a | b; r_2 = (uint32_t)(result); }}
goto L_1def6;
L_1def6:
/* 1def6: vmov s9, r2 */ {s_9=m8_fp_float(r_2);}
goto L_1defa;
L_1defa:
/* 1defa: vcvt.f32.s32 s9, s9, #0x10 */ {s_9=m8_fp_float(m8_fp_from_int(cpu, (uint32_t)m8_fp_bits32(s_9), 0, 0, 32, 16, 1));}
goto L_1defe;
L_1defe:
/* 1defe: b.w #0x1ce60 */ {goto L_1ce60;}
L_1df2c:
/* 1df2c: vldr s11, [pc, #-0x280] */ {{ uint32_t fp_addr = (0x1df30u - 640u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_11=m8_fp_float(fp_value); }}
goto L_1df30;
L_1df30:
/* 1df30: vsub.f32 s11, s10, s11 */ {s_11=m8_fp_float(m8_fp_eval32(cpu, M8_FP_SUB, m8_fp_bits32(s_10), m8_fp_bits32(s_11), 0));}
goto L_1df34;
L_1df34:
/* 1df34: vmov.f32 s10, s15 */ {s_10=m8_fp_float(m8_fp_bits32(s_15));}
goto L_1df38;
L_1df38:
/* 1df38: b.w #0x1d39a */ {goto L_1d39a;}
L_1df58:
/* 1df58: vmov.f32 s15, s14 */ {s_15=m8_fp_float(m8_fp_bits32(s_14));}
goto L_1df5c;
L_1df5c:
/* 1df5c: b.w #0x1d566 */ {goto L_1d566;}
L_1e062:
/* 1e062: vmov.f32 s15, s10 */ {s_15=m8_fp_float(m8_fp_bits32(s_10));}
goto L_1e066;
L_1e066:
/* 1e066: b.w #0x1d422 */ {goto L_1d422;}
L_1e06a:
/* 1e06a: ldrsh.w r1, [r4, #0x92] */ {{ uint32_t base = r_4; uint32_t addr = base + (146u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),2); r_1 = (uint32_t)((uint32_t)m8_signed(value & 65535u, 16)); }}
goto L_1e06e;
L_1e06e:
/* 1e06e: cmp r1, #0 */ {{ uint32_t a = r_1; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e070;
L_1e070:
/* 1e070: bge.w #0x1cfa0 */ {if(m8_condition(cpu->xpsr,11)) {goto L_1cfa0;}}
goto L_1e074;
L_1e074:
/* 1e074: adds r7, #1 */ {{ uint32_t a = r_7; uint32_t b = 0x1u; uint32_t result = m8_add_flags(cpu, a, b, 0u); r_7 = (uint32_t)(result); }}
goto L_1e076;
L_1e076:
/* 1e076: ldr r2, [sp, #0x1c] */ {{ uint32_t base = r_13; uint32_t addr = base + (28u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); }}
goto L_1e078;
L_1e078:
/* 1e078: cmp r7, #0x40 */ {{ uint32_t a = r_7; uint32_t b = 0x40u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e07a;
L_1e07a:
/* 1e07c: movne r2, #0 */ {if(m8_condition(cpu->xpsr,2)) {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x0u; uint32_t result = b; r_2 = (uint32_t)(result); }}}
/* 1e07e: andeq r2, r2, #1 */ {if(m8_condition(cpu->xpsr,1)) {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t a = r_2; uint32_t b = 0x1u; uint32_t result = a & b; r_2 = (uint32_t)(result); }}}
goto L_1e082;
L_1e082:
/* 1e082: str.w r7, [r4, #0x1a8] */ {{ uint32_t base = r_4; uint32_t addr = base + (424u); uint32_t value = r_7; m8_le_write(M8_SV_HOST(addr),4,value); }}
goto L_1e086;
L_1e086:
/* 1e086: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e088;
L_1e088:
/* 1e088: beq.w #0x1cfa0 */ {if(m8_condition(cpu->xpsr,1)) {goto L_1cfa0;}}
goto L_1e08c;
L_1e08c:
/* 1e08c: movs r1, #1 */ {{ uint32_t carry = (cpu->xpsr >> 29) & 1u; uint32_t b = 0x1u; uint32_t result = b; m8_logic_flags(cpu, result, carry); r_1 = (uint32_t)(result); }}
goto L_1e08e;
L_1e08e:
/* 1e08e: strb.w r1, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value = r_1; m8_le_write(M8_SV_HOST(addr),1,value); }}
goto L_1e092;
L_1e092:
/* 1e092: b.w #0x1cfa0 */ {goto L_1cfa0;}
L_1e096:
/* 1e096: vldr s3, [pc, #0x178] */ {{ uint32_t fp_addr = (0x1e098u + 376u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_3=m8_fp_float(fp_value); }}
goto L_1e09a;
L_1e09a:
/* 1e09a: cmp r3, #0 */ {{ uint32_t a = r_3; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e09c;
L_1e09c:
/* 1e09c: bne.w #0x1da4e */ {if(m8_condition(cpu->xpsr,2)) {goto L_1da4e;}}
goto L_1e0a0;
L_1e0a0:
/* 1e0a0: vldr s8, [pc, #0x16c] */ {{ uint32_t fp_addr = (0x1e0a4u + 364u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); }}
goto L_1e0a4;
L_1e0a4:
/* 1e0a4: b #0x1da5a */ {goto L_1da5a;}
L_1e0a6:
/* 1e0a6: vldr s15, [pc, #0x168] */ {{ uint32_t fp_addr = (0x1e0a8u + 360u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1e0aa;
L_1e0aa:
/* 1e0aa: b #0x1da32 */ {goto L_1da32;}
L_1e0cc:
/* 1e0cc: vldr s12, [pc, #0x140] */ {{ uint32_t fp_addr = (0x1e0d0u + 320u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1e0d0;
L_1e0d0:
/* 1e0d0: b #0x1da80 */ {goto L_1da80;}
L_1e0d2:
/* 1e0d2: vldr s12, [pc, #0x13c] */ {{ uint32_t fp_addr = (0x1e0d4u + 316u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_12=m8_fp_float(fp_value); }}
goto L_1e0d6;
L_1e0d6:
/* 1e0d6: b #0x1ded6 */ {goto L_1ded6;}
L_1e0d8:
/* 1e0d8: vldr s8, [pc, #0x134] */ {{ uint32_t fp_addr = (0x1e0dcu + 308u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_8=m8_fp_float(fp_value); }}
goto L_1e0dc;
L_1e0dc:
/* 1e0dc: b #0x1de88 */ {goto L_1de88;}
L_1e0ee:
/* 1e0ee: vldr s3, [pc, #0x120] */ {{ uint32_t fp_addr = (0x1e0f0u + 288u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_3=m8_fp_float(fp_value); }}
goto L_1e0f2;
L_1e0f2:
/* 1e0f2: b #0x1de58 */ {goto L_1de58;}
L_1e0f4:
/* 1e0f4: vldr s15, [pc, #0x118] */ {{ uint32_t fp_addr = (0x1e0f8u + 280u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1e0f8;
L_1e0f8:
/* 1e0f8: b #0x1dd7a */ {goto L_1dd7a;}
L_1e0fa:
/* 1e0fa: vldr s7, [pc, #0x114] */ {{ uint32_t fp_addr = (0x1e0fcu + 276u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_7=m8_fp_float(fp_value); }}
goto L_1e0fe;
L_1e0fe:
/* 1e0fe: ldr.w ip, [r4, #0xc4] */ {{ uint32_t base = r_4; uint32_t addr = base + (196u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_12 = (uint32_t)(value); }}
goto L_1e102;
L_1e102:
/* 1e102: b #0x1dd1e */ {goto L_1dd1e;}
L_1e104:
/* 1e104: vldr s15, [pc, #0x108] */ {{ uint32_t fp_addr = (0x1e108u + 264u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_15=m8_fp_float(fp_value); }}
goto L_1e108;
L_1e108:
/* 1e108: b #0x1de2e */ {goto L_1de2e;}
L_1e14a:
/* 1e14a: vmov.f32 s12, s4 */ {s_12=m8_fp_float(m8_fp_bits32(s_4));}
goto L_1e14e;
L_1e14e:
/* 1e14e: vmov.f32 s7, s17 */ {s_7=m8_fp_float(m8_fp_bits32(s_17));}
goto L_1e152;
L_1e152:
/* 1e152: b #0x1dbf4 */ {goto L_1dbf4;}
L_1e154:
/* 1e154: vmov.f32 s15, s13 */ {s_15=m8_fp_float(m8_fp_bits32(s_13));}
goto L_1e158;
L_1e158:
/* 1e158: vmov.f32 s7, s16 */ {s_7=m8_fp_float(m8_fp_bits32(s_16));}
goto L_1e15c;
L_1e15c:
/* 1e15c: b #0x1dba4 */ {goto L_1dba4;}
L_1e15e:
/* 1e15e: vmov.f32 s15, s12 */ {s_15=m8_fp_float(m8_fp_bits32(s_12));}
goto L_1e162;
L_1e162:
/* 1e162: vmov.f32 s9, s16 */ {s_9=m8_fp_float(m8_fp_bits32(s_16));}
goto L_1e166;
L_1e166:
/* 1e166: b #0x1db34 */ {goto L_1db34;}
L_1e168:
/* 1e168: vldr s6, [pc, #0xa4] */ {{ uint32_t fp_addr = (0x1e16cu + 164u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); }}
goto L_1e16c;
L_1e16c:
/* 1e16c: ldr.w r7, [r4, #0xc4] */ {{ uint32_t base = r_4; uint32_t addr = base + (196u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_7 = (uint32_t)(value); }}
goto L_1e170;
L_1e170:
/* 1e170: b #0x1ddcc */ {goto L_1ddcc;}
L_1e17c:
/* 1e17c: vldr s6, [pc, #0x90] */ {{ uint32_t fp_addr = (0x1e180u + 144u); uint32_t fp_value; fp_value=m8_le_read(M8_SV_HOST(fp_addr + 0u),4); s_6=m8_fp_float(fp_value); }}
goto L_1e180;
L_1e180:
/* 1e180: ldr.w r2, [r4, #0xbc] */ {{ uint32_t base = r_4; uint32_t addr = base + (188u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),4); r_2 = (uint32_t)(value); }}
goto L_1e184;
L_1e184:
/* 1e184: b #0x1da08 */ {goto L_1da08;}
L_1e1ba:
/* 1e1ba: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1e1be;
L_1e1be:
/* 1e1be: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e1c0;
L_1e1c0:
/* 1e1c0: bne.w #0x1db90 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1db90;}}
goto L_1e1c4;
L_1e1c4:
/* 1e1c4: b.w #0x1ce76 */ {goto L_1ce76;}
L_1e1c8:
/* 1e1c8: ldrb.w r2, [r4, #0x60] */ {{ uint32_t base = r_4; uint32_t addr = base + (96u); uint32_t value; value=m8_le_read(M8_SV_HOST(addr),1); r_2 = (uint32_t)(value); }}
goto L_1e1cc;
L_1e1cc:
/* 1e1cc: cmp r2, #0 */ {{ uint32_t a = r_2; uint32_t b = 0x0u; uint32_t result = m8_sub_flags(cpu, a, b, 0u); }}
goto L_1e1ce;
L_1e1ce:
/* 1e1ce: bne.w #0x1db20 */ {if(m8_condition(cpu->xpsr,2)) {goto L_1db20;}}
goto L_1e1d2;
L_1e1d2:
/* 1e1d2: b.w #0x1d632 */ {goto L_1d632;}
L_1e1d6:
/* 1e1d6: vmov.f32 s15, s14 */ {s_15=m8_fp_float(m8_fp_bits32(s_14));}
goto L_1e1da;
L_1e1da:
/* 1e1da: b.w #0x1d422 */ {goto L_1d422;}
L_1cd5a: r_15=0x1cd5au;goto done;
#ifndef M8_SV_SHAPE
L_1d160: r_15=0x1d160u;goto done;
#endif
L_1d924: r_15=0x1d924u;goto done;
L_1da9a: r_15=0x1da9au;goto done;
