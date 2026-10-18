#ifndef M8_NATIVE_STEP_SPECIAL_H
#define M8_NATIVE_STEP_SPECIAL_H
#include "native_step.h"
#include "runtime.h"
#include "integer.h"
#include "fp.h"

static inline int m8_native_code_matches(m8_mem *mem, uint32_t pc,
                                        unsigned size, uint32_t expected) {
    uint8_t *p = m8_region_pointer(&mem->itcm, pc, size);
    if (!p) p = m8_region_pointer(&mem->flash, pc, size);
    return p && m8_le_read(p, size) == expected;
}
static inline int m8_native_read(m8_mem *mem, uint32_t addr, unsigned size,
                                 uint32_t *value, int *slow) {
    uint8_t *p = m8_pointer(mem, addr, size);
    if (p) { *value = m8_le_read(p, size); return 0; }
    *slow = 1;
    int result = mem->read ? mem->read(mem->ctx, addr, size, value) : 1;
    return result != 0 && result != M8_AOT_MEM_CONTINUE;
}
static inline int m8_native_write(m8_mem *mem, uint32_t addr, unsigned size,
                                  uint32_t value, int *slow) {
    uint8_t *p = m8_region_pointer(&mem->dtcm, addr, size);
    if (!p) p = m8_region_pointer(&mem->ocram, addr, size);
    if (p) { m8_le_write(p, size, value); return 0; }
    *slow = 1;
    int result = mem->write ? mem->write(mem->ctx, addr, size, value) : 1;
    return result != 0 && result != M8_AOT_MEM_CONTINUE;
}

/* The reference TCG engine saves both address and value, and STREX compares
 * the current word with that value. These helpers reproduce that rule for
 * the observed single-threaded, aligned RAM word accesses. Device exclusives
 * need a separate contract and therefore return unhandled before effects. */
static inline int m8_native_ldrex(m8_cpu *cpu, m8_mem *mem,
                                 m8_native_arch *arch, unsigned dst,
                                 uint32_t addr) {
    uint8_t *p = m8_region_pointer(&mem->dtcm, addr, 4);
    if (!p) p = m8_region_pointer(&mem->ocram, addr, 4);
    if (!p) return 2;
    if (addr & 3u) return 1;
    uint32_t value = m8_le_read(p, 4);
    cpu->r[dst] = value;
    arch->exclusive_address = addr;
    arch->exclusive_value = value;
    arch->exclusive_valid = 1;
    return 0;
}
static inline int m8_native_strex(m8_cpu *cpu, m8_mem *mem,
                                 m8_native_arch *arch, unsigned dst,
                                 uint32_t value, uint32_t addr) {
    uint8_t *p = m8_region_pointer(&mem->dtcm, addr, 4);
    if (!p) p = m8_region_pointer(&mem->ocram, addr, 4);
    if (!p) return 2;
    /* Unicorn rejects a non-matching monitor before address translation or
     * alignment checks. A failed STREX must not read the RAM word. */
    if (!arch->exclusive_valid || arch->exclusive_address != addr) {
        cpu->r[dst] = 1;
        arch->exclusive_valid = 0;
        return 0;
    }
    if (addr & 3u) return 1;
    uint32_t fail = arch->exclusive_value != m8_le_read(p, 4);
    if (!fail) m8_le_write(p, 4, value);
    cpu->r[dst] = fail;
    arch->exclusive_valid = 0;
    return 0;
}
static inline void m8_native_advance_it(m8_cpu *cpu, m8_native_arch *arch) {
    uint32_t state = arch->itstate;
    arch->itstate = !(state & 7u) ? 0 :
        (state & 0xe0u) | ((state << 1) & 0x1fu);
    m8_native_publish_it(cpu, arch);
}
static inline int m8_native_using_psp(m8_native_arch *arch) {
    return !arch->ipsr && (arch->control & 2u);
}
static inline void m8_native_save_sp(m8_cpu *cpu, m8_native_arch *arch) {
    if (m8_native_using_psp(arch)) arch->psp = cpu->r[13];
    else arch->msp = cpu->r[13];
}

/* MRS of EPSR returns zero in the reference. APSR remains readable in
 * unprivileged mode; IPSR and privileged masks follow the reference rules. */
static inline int m8_native_mrs(m8_cpu *cpu, m8_native_arch *arch,
                               unsigned sysm, uint32_t *value) {
    int privileged = arch->ipsr || !(arch->control & 1u);
    if (sysm <= 7) {
        *value = (!(sysm & 4u) ? cpu->xpsr & 0xf80f0000u : 0) |
            ((sysm & 1u) && privileged ? arch->ipsr & 0x1ffu : 0);
        return 0;
    }
    if (sysm == 20) { *value = arch->control; return 0; }
    if (sysm != 8 && sysm != 9 && sysm != 16 && sysm != 17 &&
        sysm != 18 && sysm != 19) return 2;
    if (!privileged) { *value = 0; return 0; }
    m8_native_save_sp(cpu, arch);
    switch (sysm) {
    case 8: *value = arch->msp; break;
    case 9: *value = arch->psp; break;
    case 16: *value = arch->primask; break;
    case 17: case 18: *value = arch->basepri; break;
    case 19: *value = arch->faultmask; break;
    }
    return 0;
}
static inline int m8_native_msr(m8_cpu *cpu, m8_native_arch *arch,
                               unsigned sysm, unsigned mask, uint32_t value) {
    int privileged = arch->ipsr || !(arch->control & 1u);
    if (sysm <= 7) {
        uint32_t bits = !(sysm & 4u) ?
            ((mask & 8u ? 0xf8000000u : 0) |
             (mask & 4u ? 0x000f0000u : 0)) : 0;
        cpu->xpsr = (cpu->xpsr & ~bits) | (value & bits);
        return 0;
    }
    if (sysm != 8 && sysm != 9 && sysm != 16 && sysm != 17 &&
        sysm != 18 && sysm != 19) return 2;
    if (!privileged) return 0;
    m8_native_save_sp(cpu, arch);
    switch (sysm) {
    case 8: arch->msp = value; break;
    case 9: arch->psp = value; break;
    case 16: arch->primask = value & 1u; break;
    case 17: arch->basepri = value & 255u; break;
    case 18:
        value &= 255u;
        if (value && (!arch->basepri || value < arch->basepri))
            arch->basepri = value;
        break;
    case 19: arch->faultmask = value & 1u; break;
    }
    cpu->r[13] = m8_native_using_psp(arch) ? arch->psp : arch->msp;
    return 0;
}
#endif
