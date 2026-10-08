#include "internal.h"
#include <stdlib.h>
#include <string.h>

#define AUDIT_CAPACITY 8192u
#define AUDIT_SLOTS (AUDIT_CAPACITY * 2u)
#define CYCLES_PER_MS UINT64_C(600000)

struct m8rt_audit {
    m8rt_audit_status status;
    m8rt_audit_entry entries[AUDIT_CAPACITY];
    uint32_t slots[AUDIT_SLOTS]; /* Index plus one; zero is empty. */
    uint64_t main_cycles;
    bool main_seen, hang_reported;
    uc_hook protected_hook;
};

static unsigned access_direction(uc_mem_type type)
{
    switch (type) {
    case UC_MEM_WRITE_UNMAPPED: case UC_MEM_WRITE_PROT: return M8RT_AUDIT_WRITE;
    case UC_MEM_FETCH_UNMAPPED: case UC_MEM_FETCH_PROT: return M8RT_AUDIT_FETCH;
    default: return M8RT_AUDIT_READ;
    }
}

static bool protected_hook(uc_engine *uc, uc_mem_type type, uint64_t address,
                           int size, int64_t value, void *opaque)
{
    (void)uc;
    m8rt_audit_access(opaque, M8RT_AUDIT_PROTECTED, address, (unsigned)size,
                      access_direction(type), (unsigned)type, (uint64_t)value);
    return false; /* Observation does not repair or map an access. */
}

int m8rt_set_audit(m8rt *rt, int enabled, uint32_t hang_ms)
{
    if (!rt || rt->fatal || rt->in_emu || rt->started) return -1;
    if (enabled && !rt->audit) {
        m8rt_audit *audit = calloc(1, sizeof(*audit));
        if (!audit) return -1;
        uc_err err = uc_hook_add(rt->uc, &audit->protected_hook, UC_HOOK_MEM_PROT,
                                 protected_hook, rt, 1, 0);
        if (err) { free(audit); return -1; }
        rt->audit = audit;
        /* The existing producer hooks copy binary32 data before formatting.
         * Keep the caller's selected output; audit does not select local PCM. */
        bool selected = rt->local_selected;
        if (m8rt_set_audio_tap(rt, 1)) {
            m8rt_audit_destroy(rt);
            return -1;
        }
        rt->local_selected = selected;
    }
    if (rt->audit) {
        rt->audit->status.enabled = enabled != 0;
        rt->audit->status.hang_ms = hang_ms;
    }
    return 0;
}

void m8rt_audit_destroy(m8rt *rt)
{
    /* uc_close owns all hooks during normal destruction. */
    if (rt->audit && rt->uc && !rt->started)
        uc_hook_del(rt->uc, rt->audit->protected_hook);
    free(rt->audit);
    rt->audit = NULL;
}

void m8rt_audit_access(m8rt *rt, unsigned kind, uint64_t address, unsigned size,
                       unsigned direction, unsigned code, uint64_t value)
{
    m8rt_audit *audit = rt->audit;
    if (!audit || !audit->status.enabled) return;
    m8rt_audit_status *status = &audit->status;
    switch (kind) {
    case M8RT_AUDIT_GENERIC_MMIO: ++status->generic_mmio; break;
    case M8RT_AUDIT_UNMAPPED: ++status->unmapped; break;
    case M8RT_AUDIT_PROTECTED: ++status->protected_accesses; break;
    case M8RT_AUDIT_FAULT: ++status->faults; break;
    case M8RT_AUDIT_HANG: ++status->hangs; break;
    case M8RT_AUDIT_UNEXPECTED_IRQ: ++status->unexpected_irqs; break;
    default: return;
    }
    uint64_t hash = address ^ (address >> 32) ^ ((uint64_t)kind << 24) ^
                    ((uint64_t)size << 16) ^ ((uint64_t)direction << 8) ^ code;
    /* The first WCR operand cannot establish that WDE stayed clear. Retain
     * every distinct control write, including byte accesses to either half,
     * while leaving high-frequency refresh writes aggregated as before. */
    bool watchdog_control = kind == M8RT_AUDIT_GENERIC_MMIO &&
                            direction == M8RT_AUDIT_WRITE &&
                            address >= UINT32_C(0x400b8000) &&
                            address < UINT32_C(0x400b8002);
    if (watchdog_control) hash ^= value * UINT64_C(0x9e3779b97f4a7c15);
    size_t slot = (size_t)(hash * UINT64_C(11400714819323198485)) & (AUDIT_SLOTS - 1);
    for (size_t i = 0; i < AUDIT_SLOTS; ++i) {
        uint32_t index = audit->slots[slot];
        if (!index) {
            if (status->entries == AUDIT_CAPACITY) { ++status->dropped; return; }
            index = (uint32_t)++status->entries;
            audit->slots[slot] = index;
            m8rt_audit_entry *entry = &audit->entries[index - 1];
            *entry = (m8rt_audit_entry){
                .address = address, .value = value, .count = 1,
                .first_cycles = rt->guest_cycles, .first_main_iters = rt->main_iters,
                .kind = kind, .size = size, .direction = direction,
                .first_pc = rt->uc ? m8rt_reg_read(rt, UC_ARM_REG_PC) : 0, .code = code
            };
            return;
        }
        m8rt_audit_entry *entry = &audit->entries[index - 1];
        if (entry->address == address && entry->kind == kind && entry->size == size &&
            entry->direction == direction && entry->code == code &&
            (!watchdog_control || entry->value == value)) {
            ++entry->count;
            return;
        }
        slot = (slot + 1) & (AUDIT_SLOTS - 1);
    }
    ++status->dropped;
}

void m8rt_audit_main(m8rt *rt)
{
    m8rt_audit *audit = rt->audit;
    if (!audit || !audit->status.enabled) return;
    m8rt_audit_time(rt); /* Preserve a completed gap before resetting its origin. */
    audit->main_cycles = rt->guest_cycles;
    audit->main_seen = true;
    audit->hang_reported = false;
}

void m8rt_audit_time(m8rt *rt)
{
    m8rt_audit *audit = rt->audit;
    if (!audit || !audit->status.enabled || !audit->main_seen) return;
    uint64_t elapsed = rt->guest_cycles - audit->main_cycles;
    if (elapsed > audit->status.max_main_gap_cycles)
        audit->status.max_main_gap_cycles = elapsed;
    if (audit->status.hang_ms && elapsed >= (uint64_t)audit->status.hang_ms * CYCLES_PER_MS &&
        !audit->hang_reported) {
        audit->hang_reported = true;
        m8rt_audit_access(rt, M8RT_AUDIT_HANG, 0, 0, M8RT_AUDIT_NONE,
                          audit->status.hang_ms, elapsed);
    }
}

void m8rt_audit_audio(m8rt *rt, uint32_t pc)
{
    m8rt_audit *audit = rt->audit;
    if (!audit || !audit->status.enabled) return;
    m8rt_audit_status *status = &audit->status;
    bool nonfinite = false, denormal = false;
    ++status->audio_blocks;
    for (unsigned channel = 0; channel < 2; ++channel) {
        if (!rt->local_flags[channel]) continue;
        for (unsigned frame = 0; frame < 64; ++frame) {
            uint32_t bits = m8rt_le32(rt->local_raw + (channel * 64 + frame) * 4);
            uint32_t exponent = bits & UINT32_C(0x7f800000);
            uint32_t fraction = bits & UINT32_C(0x007fffff);
            ++status->audio_samples;
            if (exponent == UINT32_C(0x7f800000)) {
                if (!status->nonfinite_samples) {
                    status->first_nonfinite_cycles = rt->guest_cycles;
                    status->first_nonfinite_pc = pc;
                    status->first_nonfinite_bits = bits;
                }
                ++status->nonfinite_samples;
                if (fraction) ++status->nan_samples;
                else ++status->infinity_samples;
                nonfinite = true;
            } else if (!exponent && fraction) {
                if (!status->denormal_samples) {
                    status->first_denormal_cycles = rt->guest_cycles;
                    status->first_denormal_pc = pc;
                    status->first_denormal_bits = bits;
                }
                ++status->denormal_samples;
                denormal = true;
            }
        }
    }
    status->nonfinite_blocks += nonfinite;
    status->denormal_blocks += denormal;
}

int m8rt_get_audit_status(m8rt *rt, m8rt_audit_status *out)
{
    if (!rt || !out || rt->in_emu) return -1;
    if (rt->audit) *out = rt->audit->status;
    else memset(out, 0, sizeof(*out));
    return 0;
}

size_t m8rt_copy_audit_entries(m8rt *rt, m8rt_audit_entry *out, size_t count)
{
    if (!rt || !rt->audit || rt->in_emu || !out) return 0;
    size_t used = (size_t)rt->audit->status.entries;
    if (count > used) count = used;
    memcpy(out, rt->audit->entries, count * sizeof(*out));
    return count;
}
