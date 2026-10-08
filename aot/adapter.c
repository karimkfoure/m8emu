/* Native Unicorn entry hooks. The emulator remains the interrupt owner. */
#define _POSIX_C_SOURCE 200809L
#include "m8_aot.h"
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CPU_REGS 50
_Static_assert(sizeof(m8_cpu) == 200, "m8_cpu ABI size changed");
_Static_assert(offsetof(m8_cpu, r) == 0, "m8_cpu r offset changed");
_Static_assert(offsetof(m8_cpu, xpsr) == 64, "m8_cpu xpsr offset changed");
_Static_assert(offsetof(m8_cpu, s) == 68, "m8_cpu s offset changed");
_Static_assert(offsetof(m8_cpu, fpscr) == 196, "m8_cpu fpscr offset changed");

typedef struct m8_adapter_entry {
    uint32_t pc;
    uint32_t reserved;
    uint64_t calls, native_ns, total_ns, exits, faults, skipped, no_progress;
    m8_aot_fn fn;
    uc_hook hook;
} m8_adapter_entry;

typedef struct m8_adapter {
    uc_engine *uc;
    m8_mem mem;
    uint32_t count;
    int error;
    int busy;
    m8_adapter_entry *entries;
    void (*set_slow_fallback)(int);
} m8_adapter;

static const int regids[CPU_REGS] = {
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
    UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
    UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
    UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
    UC_ARM_REG_XPSR,
    UC_ARM_REG_S0, UC_ARM_REG_S1, UC_ARM_REG_S2, UC_ARM_REG_S3,
    UC_ARM_REG_S4, UC_ARM_REG_S5, UC_ARM_REG_S6, UC_ARM_REG_S7,
    UC_ARM_REG_S8, UC_ARM_REG_S9, UC_ARM_REG_S10, UC_ARM_REG_S11,
    UC_ARM_REG_S12, UC_ARM_REG_S13, UC_ARM_REG_S14, UC_ARM_REG_S15,
    UC_ARM_REG_S16, UC_ARM_REG_S17, UC_ARM_REG_S18, UC_ARM_REG_S19,
    UC_ARM_REG_S20, UC_ARM_REG_S21, UC_ARM_REG_S22, UC_ARM_REG_S23,
    UC_ARM_REG_S24, UC_ARM_REG_S25, UC_ARM_REG_S26, UC_ARM_REG_S27,
    UC_ARM_REG_S28, UC_ARM_REG_S29, UC_ARM_REG_S30, UC_ARM_REG_S31,
    UC_ARM_REG_FPSCR
};

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

/* m8_cpu is exactly fifty uint32_t words; batch calls avoid Python work. */
int m8_adapter_read_cpu(uc_engine *uc, m8_cpu *cpu) {
    void *values[CPU_REGS];
    uint8_t *bytes = (uint8_t *)cpu;
    for (unsigned i = 0; i < CPU_REGS; ++i) values[i] = bytes + i * sizeof(uint32_t);
    return uc_reg_read_batch(uc, (int *)regids, values, CPU_REGS);
}

static int write_cpu_impl(uc_engine *uc, const m8_cpu *cpu, int redirect) {
    void *values[CPU_REGS];
    /* Copy because Unicorn's PC write can normalize the Thumb bit. */
    m8_cpu tmp = *cpu;
    uint8_t *bytes = (uint8_t *)&tmp;
    for (unsigned i = 0; i < CPU_REGS; ++i) values[i] = bytes + i * sizeof(uint32_t);
    /* Set PC last: state writes must not overwrite a redirected PC. */
    int ids[CPU_REGS];
    memcpy(ids, regids, sizeof(ids));
    ids[15] = UC_ARM_REG_FPSCR; values[15] = &tmp.fpscr;
    ids[49] = UC_ARM_REG_PC; values[49] = &tmp.r[15];
    tmp.r[15] |= 1u;
    uc_err err = uc_reg_write_batch(uc, ids, values, CPU_REGS - 1);
    if (err != UC_ERR_OK) return err;
    /* Unicorn's XPSR write does not update GE; the APSR view does. */
    err = uc_reg_write(uc, UC_ARM_REG_APSR_NZCVQG, &tmp.xpsr);
    if (err != UC_ERR_OK) return err;
    /* Singular PC writes notify TCG to abandon the current translated block.
     * Batch PC writes update the register but do not redirect a live hook. */
    return redirect ? uc_reg_write(uc, UC_ARM_REG_PC, &tmp.r[15]) : UC_ERR_OK;
}

int m8_adapter_write_cpu(uc_engine *uc, const m8_cpu *cpu) {
    return write_cpu_impl(uc, cpu, 1);
}

/* Native DSP runs with exclusive CPU ownership. Publish its changed words;
 * unchanged FPSCR/XPSR writes can force unnecessary Unicorn state updates. */
static int write_cpu_delta(uc_engine *uc, const m8_cpu *before,
                          const m8_cpu *after) {
    int ids[CPU_REGS];
    void *values[CPU_REGS];
    int count = 0;
    for (unsigned i = 0; i < 15; ++i) {
        if (before->r[i] != after->r[i]) {
            ids[count] = regids[i];
            values[count++] = (void *)&after->r[i];
        }
    }
    for (unsigned i = 0; i < 32; ++i) {
        if (before->s[i] != after->s[i]) {
            ids[count] = regids[17 + i];
            values[count++] = (void *)&after->s[i];
        }
    }
    if (before->fpscr != after->fpscr) {
        ids[count] = UC_ARM_REG_FPSCR;
        values[count++] = (void *)&after->fpscr;
    }
    uint32_t psr_changed = before->xpsr ^ after->xpsr;
    const uint32_t apsr_mask = UINT32_C(0xf80f0000);
    if (psr_changed & ~apsr_mask) {
        ids[count] = UC_ARM_REG_XPSR;
        values[count++] = (void *)&after->xpsr;
        /* Restore GE through APSR because Unicorn's XPSR view omits it. */
        ids[count] = UC_ARM_REG_APSR_NZCVQG;
        values[count++] = (void *)&after->xpsr;
    } else if (psr_changed) {
        ids[count] = UC_ARM_REG_APSR_NZCVQG;
        values[count++] = (void *)&after->xpsr;
    }
    uc_err err = count ? uc_reg_write_batch(uc, ids, values, count) : UC_ERR_OK;
    if (err != UC_ERR_OK) return err;
    if (before->r[15] != after->r[15]) {
        uint32_t pc = after->r[15] | 1u;
        return uc_reg_write(uc, UC_ARM_REG_PC, &pc);
    }
    return UC_ERR_OK;
}

static int slow_read(void *opaque, uint32_t addr, uint32_t size, uint32_t *value) {
    m8_adapter *adapter = opaque;
    uint8_t bytes[4] = {0};
    if (size > sizeof(bytes) || !size) return -1;
    uc_err err = uc_mem_read(adapter->uc, addr, bytes, size);
    if (err != UC_ERR_OK) { adapter->error = err; return -1; }
    *value = 0;
    for (uint32_t i = 0; i < size; ++i) *value |= (uint32_t)bytes[i] << (i * 8);
    return 0;
}

static int slow_write(void *opaque, uint32_t addr, uint32_t size, uint32_t value) {
    m8_adapter *adapter = opaque;
    uint8_t bytes[4];
    if (size > sizeof(bytes) || !size) return -1;
    for (uint32_t i = 0; i < size; ++i) bytes[i] = (uint8_t)(value >> (i * 8));
    uc_err err = uc_mem_write(adapter->uc, addr, bytes, size);
    if (err != UC_ERR_OK) { adapter->error = err; return -1; }
    return 0;
}

static void entry_hook(uc_engine *uc, uint64_t address, uint32_t size, void *opaque) {
    (void)size;
    m8_adapter *adapter = opaque;
    if (adapter->busy || adapter->error) return;
    m8_adapter_entry *entry = NULL;
    for (uint32_t i = 0; i < adapter->count; ++i)
        if (adapter->entries[i].pc == (uint32_t)address) { entry = &adapter->entries[i]; break; }
    if (!entry) return;
    uint64_t start = now_ns();
    m8_cpu cpu;
    uc_err err = m8_adapter_read_cpu(uc, &cpu);
    if (err != UC_ERR_OK) goto failed;
    if (cpu.xpsr & UINT32_C(0x0600fc00)) { entry->skipped++; return; }
    m8_cpu before = cpu;
    adapter->busy = 1;
    uint64_t native_start = now_ns();
    if (adapter->set_slow_fallback) adapter->set_slow_fallback(1);
    int result = entry->fn(&cpu, &adapter->mem);
    entry->native_ns += now_ns() - native_start;
    adapter->busy = 0;
    entry->calls++;
    if (result == M8_AOT_FAULT) entry->faults++; else entry->exits++;
    int progress = cpu.r[15] != (uint32_t)address;
    if (!progress) entry->no_progress++;
    err = write_cpu_delta(uc, &before, &cpu);
    entry->total_ns += now_ns() - start;
    if (err != UC_ERR_OK) goto failed;
    if (result == M8_AOT_FAULT) {
        if (!adapter->error) adapter->error = UC_ERR_EXCEPTION;
        uc_emu_stop(uc);
    }
    return;
failed:
    adapter->error = err;
    uc_emu_stop(uc);
}

void *m8_adapter_install(uc_engine *uc, const m8_mem *mem,
                         const uint32_t *selected, uint32_t count) {
    const uint32_t *all;
    if (!selected) { count = m8_aot_entries(&all); selected = all; }
    if (!count) return NULL;
    m8_adapter *adapter = calloc(1, sizeof(*adapter));
    if (!adapter) return NULL;
    adapter->uc = uc;
    adapter->mem = *mem;
    adapter->mem.ctx = adapter;
    adapter->mem.read = slow_read;
    adapter->mem.write = slow_write;
    adapter->entries = calloc(count, sizeof(*adapter->entries));
    if (!adapter->entries) { free(adapter); return NULL; }
    adapter->count = count;
    for (uint32_t i = 0; i < count; ++i) {
        m8_adapter_entry *entry = &adapter->entries[i];
        entry->pc = selected[i] & ~1u;
        entry->fn = m8_aot_lookup(entry->pc);
        if (!entry->fn) { adapter->error = UC_ERR_ARG; break; }
        adapter->error = uc_hook_add(uc, &entry->hook, UC_HOOK_CODE,
             (void *)entry_hook, adapter, entry->pc, entry->pc);
        if (adapter->error) break;
    }
    if (adapter->error) {
        for (uint32_t i = 0; i < count; ++i)
            if (adapter->entries[i].hook) uc_hook_del(uc, adapter->entries[i].hook);
        free(adapter->entries); free(adapter); return NULL;
    }
    return adapter;
}

void m8_adapter_set_slow_fallback(void *opaque, void (*setter)(int)) {
    ((m8_adapter *)opaque)->set_slow_fallback = setter;
}

int m8_adapter_error(const void *opaque) {
    return ((const m8_adapter *)opaque)->error;
}

uint32_t m8_adapter_stats(const void *opaque, const m8_adapter_entry **entries) {
    const m8_adapter *adapter = opaque;
    *entries = adapter->entries;
    return adapter->count;
}

int m8_adapter_uninstall(void *opaque) {
    m8_adapter *adapter = opaque;
    int error = 0;
    for (uint32_t i = 0; i < adapter->count; ++i) {
        uc_err err = uc_hook_del(adapter->uc, adapter->entries[i].hook);
        if (err && !error) error = err;
    }
    /* Never free a hook's opaque argument if its deletion failed. */
    if (error) return error;
    free(adapter->entries); free(adapter);
    return 0;
}

int m8_adapter_noop(uint32_t entry, m8_cpu *cpu, m8_mem *mem) {
    (void)entry; (void)cpu; (void)mem; return 0;
}

int m8_adapter_run(uint32_t entry, m8_cpu *cpu, m8_mem *mem) {
    m8_aot_fn fn = m8_aot_lookup(entry);
    return fn ? fn(cpu, mem) : -1;
}
