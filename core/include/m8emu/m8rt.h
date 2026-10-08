#ifndef M8RT_H
#define M8RT_H
#include <stdint.h>
#include <stddef.h>
#include "m8_aot.h"
typedef struct m8rt m8rt;
#ifdef M8RT_NATIVE_ENGINE
#include "native-engine.h"
#endif
typedef struct m8rt_options {
    uint32_t reference_policy, strict, host_handshake, profile_blocks;
} m8rt_options;
enum m8rt_field {
    M8RT_MAIN_ITERS, M8RT_CYCCNT, M8RT_PC, M8RT_BOOT_DONE, M8RT_PCM_BYTES,
    M8RT_SLICES, M8RT_UC_NS, M8RT_CALLBACK_NS, M8RT_CALLBACK_COUNT,
    M8RT_CDC_BYTES, M8RT_CONFIGURED, M8RT_DTR, M8RT_HOST_QUEUED_BYTES,
    M8RT_MAIN_SELECTED, M8RT_MAIN_EXPANDED, M8RT_MAIN_CYCLES,
    M8RT_LOCAL_PCM_BYTES, M8RT_LOCAL_QUEUED_BYTES, M8RT_LOCAL_BLOCKS,
    M8RT_GUEST_CYCLES, M8RT_LOCAL_LAST_CYCLES,
    M8RT_AOT_TIMING_ENABLED, M8RT_AOT_CALLS, M8RT_AOT_NATIVE_NS, M8RT_AOT_HOOK_NS,
    M8RT_GUEST_COUNT_ENABLED, M8RT_JIT_GUEST_INSTRUCTIONS
};
typedef struct m8rt_aot_timing {
    uint32_t pc;
    uint64_t calls, native_ns, hook_ns;
} m8rt_aot_timing;
typedef struct m8rt_audio_position {
    uint64_t frame, cycles; /* Output frame index and modeled 600 MHz clock. */
    uint32_t frames, reserved;
} m8rt_audio_position;
m8rt *m8rt_create(const char *firmware, const char *image, const m8rt_options *opts);
#define M8RT_AOT_REQUIRE_FIXED_MEMORY 1u
/* Read the loaded AOT library's requirements before creation. Required fixed
 * memory takes precedence over M8RT_FIXED_MEMORY=0. No environment is changed.
 * The original constructor keeps zero requirements and its existing ABI. */
m8rt *m8rt_create_aot(const char *firmware, const char *image,
                      const m8rt_options *opts, unsigned requirements);
void m8rt_destroy(m8rt *rt);
const char *m8rt_error(m8rt *rt);
/* Select an EEPROM NOR file before execution; NULL keeps flash in memory.
 * A missing file uses erased firmware bytes. A present file is exactly 252 KiB.
 * Run returns flush changed bytes atomically; failures stop the runtime.
 * create with an image selects IMAGE.settings.nor automatically. */
int m8rt_nor_set_file(m8rt *rt, const char *path);
/* Select an output without loading it, before execution. Preserve source NOR
 * when the application's saved card has another path. NULL disables flush. */
int m8rt_nor_set_output(m8rt *rt, const char *path);
int m8rt_nor_flush(m8rt *rt);
/* 0 commands, 1 programs, 2 programmed bytes, 3 erases, 4 erased bytes,
 * 5 atomic flushes, 6 last program address, 7 last length, 8 pending, 9 dirty. */
uint64_t m8rt_nor_get(m8rt *rt, unsigned field);
/* Absolute limits. A zero limit is disabled. Return -1 on failure, 0 on a
 * completed limit/slice, and 1 on a host stop. No asynchronous timer is used. */
int m8rt_run(m8rt *rt, uint64_t until_iters, uint64_t until_frames, uint32_t wall_us, int yield_boot);
int m8rt_run_main_iters(m8rt *rt, uint64_t n);
int m8rt_run_until_frames(m8rt *rt, uint64_t n);
int m8rt_queue_host_bytes(m8rt *rt, const void *data, size_t n);
/* Host audio: interleaved stereo S16LE, 44100 Hz. Calls belong to the guest
 * execution thread. Input is disabled until enabled or nonempty audio is queued.
 * A fixed 16384-frame queue admits the whole payload: 0 success, -2 full,
 * -1 invalid input. Missing source frames become silence at the USB cadence.
 * Disabling input stops packet delivery and retains queued source frames. */
int m8rt_set_audio_input(m8rt *rt, int enabled);
int m8rt_queue_audio_input(m8rt *rt, const void *pcm, size_t bytes);
/* 0 queued frames, 1 accepted frames, 2 delivered source frames,
 * 3 delivered silence frames, 4 completed packets, 5 capacity frames. */
uint64_t m8rt_audio_input_get(m8rt *rt, unsigned field);
size_t m8rt_read_cdc(m8rt *rt, void *out, size_t n);
typedef struct m8rt_midi_position {
    uint64_t frame, cycles; /* Selected audio tap frame and 600 MHz clock. */
} m8rt_midi_position;
/* MIDI capture is opt-in. Enable before guest execution; disable clears queues.
 * Calls, like other host I/O, require an idle runtime and a single owner. */
int m8rt_set_midi(m8rt *rt, int enabled);
/* One complete MIDI message, cable 0, explicit status (no running status).
 * SysEx must include F0/F7. Return 0, -1 runtime failure, -2 invalid message,
 * or -3 queue full; invalid/full calls have no effects. Each queue holds 4096
 * USB event packets. send also enables capture. */
int m8rt_midi_send(m8rt *rt, const void *data, size_t n);
/* Raw byte stream; USB padding is removed. */
size_t m8rt_midi_read(m8rt *rt, void *out, size_t n);
/* Up to one USB event payload (SysEx chunks stay separate). n >= 3.
 * Timestamp is guest USB IN completion, not external-device delivery. */
size_t m8rt_midi_read_timed(m8rt *rt, void *out, size_t n, m8rt_midi_position *position);
/* Return up to one original USB packet. Repeated reads yield the raw stream. */
size_t m8rt_read_pcm(m8rt *rt, void *out, size_t n);
size_t m8rt_read_pcm_timed(m8rt *rt, void *out, size_t n, m8rt_audio_position *position);
/* One completed 64-frame publication. n must be at least 256 bytes. */
size_t m8rt_read_local_pcm(m8rt *rt, void *out, size_t n, m8rt_audio_position *position);
/* Opt in before execution. The original USB stream remains available. */
int m8rt_set_audio_tap(m8rt *rt, int local);
/* Absolute frame limit for the selected tap, plus an optional host slice. */
int m8rt_run_audio_frames(m8rt *rt, uint64_t frames, uint32_t wall_us);
/* M8RT_GUEST_COUNT=1 before creation enables instruction callbacks for exact
 * Unicorn dispatch counts. AOT redirects and pre-access stops are excluded.
 * Predicate-suppressed IT members are excluded. Counting adds large host cost;
 * compare PCM, and time a separate ordinary run. */
uint64_t m8rt_get(m8rt *rt, unsigned field);
/* Install/remove instruction counting outside execution. Enable resets the
 * counter to zero. Limit counting to playback so setup remains fast. */
int m8rt_set_guest_count(m8rt *rt, int enabled);
#ifdef M8RT_ACCESS_PROFILE
/* Optional diagnostic build only. Reset/enable after project loading, then
 * disable before post-playback checks. Path 1 is TCG MMIO; path 2 is AOT. */
int m8rt_set_access_profile(m8rt *rt,int enabled);
int m8rt_dump_access_profile(m8rt *rt,const char *path);
#endif
#ifdef M8RT_GUEST_PC_PROFILE
typedef struct { uint32_t pc; uint64_t instructions; } m8rt_guest_pc;
/* Playback-only exact Unicorn PC counts. Enable also resets guest counting.
 * Disable retains the histogram. Read outside execution; NULL returns size. */
int m8rt_set_guest_pc_profile(m8rt *rt, int enabled);
size_t m8rt_read_guest_pc_profile(m8rt *rt, m8rt_guest_pc *out, size_t count);
#endif
/* Setter field 0 selects the host handshake; field 1 selects PCM capture. */
int m8rt_set(m8rt *rt, unsigned field, uint64_t value);
/* Owned-buffer copies default on with a guarded engine. M8RT_DIRECT_MEMORY=0
 * at creation selects Unicorn's original memory API. While direct copies are
 * active, engine remapping, protection changes and memory snapshots are not
 * supported: a changed layout generation fails before further direct access.
 * CPU-only contexts remain supported. Engines without the guard use the API. */
int m8rt_mem_read(m8rt *rt, uint32_t addr, void *out, size_t n);
int m8rt_mem_write(m8rt *rt, uint32_t addr, const void *in, size_t n);
/* Optional Linux 64-bit arena, selected by an AOT creation requirement or
 * M8RT_FIXED_MEMORY=1 at creation.
 * Mapped guest RAM/flash bytes are at offset + guest_address. Holes remain
 * inaccessible. NULL selects the ordinary independent buffer allocations. */
uint8_t *m8rt_memory_offset(m8rt *rt);
uint32_t m8rt_reg_read(m8rt *rt, int reg);
void m8rt_request_stop(m8rt *rt);
/* Build-time aggregate callback timing status; NULL returns zero. Callback
 * counts and explicit AOT timing remain available when this returns zero. */
int m8rt_callback_timing_enabled(const m8rt *rt);
int m8rt_set_aot(m8rt *rt, m8_aot_fn (*lookup)(uint32_t), uint32_t (*entries)(const uint32_t **));
/* Opt-in memory services requested by the installed AOT library. Zero
 * preserves an exit after each completed device access. Cooperative code
 * must exit before an instruction can change interrupt masks; mask snapshots
 * are valid only within one native call. */
int m8rt_set_aot_services(m8rt *rt, unsigned flags);
/* Optional playback diagnostics. Enable resets the counters; disable retains
 * them. Collection adds lookup work, so time a separate ordinary run. */
typedef struct {
    uint64_t aot_calls, same_pc, lookup_resumes, installed_resumes, pure_resumes;
    uint64_t memory_reads, memory_writes, ram_reads, ram_writes;
} m8rt_dispatch_stats;
int m8rt_set_dispatch_stats(m8rt *rt, int enabled);
int m8rt_read_dispatch_stats(m8rt *rt, m8rt_dispatch_stats *out);
/* Set M8RT_AOT_TIMING=1 before creation to enable measurement. Hook time
 * includes native time. With NULL output, return the installed entry count. */
size_t m8rt_read_aot_timing(m8rt *rt, m8rt_aot_timing *out, size_t count);
int m8rt_dump_profile(m8rt *rt, const char *path);
/* Clear block counts outside guest execution. Keep the installed hook. */
int m8rt_reset_profile(m8rt *rt);
size_t m8rt_read_irqs(m8rt *rt, uint32_t *out, size_t count);
/* Optional, observational audit. Enable before execution. Entries retain the
 * first operands and count repeated keys; capacity loss is explicit. */
enum m8rt_audit_kind {
    M8RT_AUDIT_GENERIC_MMIO = 1, M8RT_AUDIT_UNMAPPED, M8RT_AUDIT_FAULT,
    M8RT_AUDIT_HANG, M8RT_AUDIT_UNEXPECTED_IRQ, M8RT_AUDIT_PROTECTED
};
enum m8rt_audit_direction { M8RT_AUDIT_NONE, M8RT_AUDIT_READ, M8RT_AUDIT_WRITE, M8RT_AUDIT_FETCH };
typedef struct m8rt_audit_entry {
    uint64_t address, value, count, first_cycles, first_main_iters;
    uint32_t kind, size, direction, first_pc, code, reserved;
} m8rt_audit_entry;
typedef struct m8rt_audit_status {
    uint64_t entries, dropped, generic_mmio, unmapped, protected_accesses;
    uint64_t faults, hangs, unexpected_irqs, max_main_gap_cycles;
    uint64_t audio_blocks, audio_samples, nonfinite_blocks, nonfinite_samples;
    uint64_t nan_samples, infinity_samples, denormal_blocks, denormal_samples;
    uint64_t first_nonfinite_cycles, first_denormal_cycles;
    uint32_t enabled, hang_ms, first_nonfinite_pc, first_denormal_pc;
    uint32_t first_nonfinite_bits, first_denormal_bits;
} m8rt_audit_status;
int m8rt_set_audit(m8rt *rt, int enabled, uint32_t hang_ms);
int m8rt_get_audit_status(m8rt *rt, m8rt_audit_status *out);
/* WDOG1 state. Deadlines use the extended 600 MHz guest clock. Counters survive
 * a guest watchdog reset; control registers return to their hardware defaults. */
typedef struct m8rt_watchdog_status {
    uint64_t deadline_cycles, last_refresh_cycles, refreshes, rejected_services;
    uint64_t interrupts, resets, last_reset_cycles;
    uint64_t fallback_cycles, fallback_slots, idle_batches;
    uint32_t control, service, reset_status, interrupt_control, miscellaneous;
    uint32_t enabled, reset_pending;
} m8rt_watchdog_status;
int m8rt_get_watchdog_status(m8rt *rt, m8rt_watchdog_status *out);
/* Non-destructive copy in first-observation order. Returns the copied count. */
size_t m8rt_copy_audit_entries(m8rt *rt, m8rt_audit_entry *out, size_t count);
#endif
