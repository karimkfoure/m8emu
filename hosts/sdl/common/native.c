#include "native.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M8_NATIVE_DEADLINE_YIELD_DEFAULT
#define M8_NATIVE_DEADLINE_YIELD_DEFAULT 1
#endif

struct m8_host_native {
    SDL_SharedObject *step, *region;
    char *step_path, *region_path;
    m8rt_native_live_config config;
    int enabled, attached;
    char fallback[512];
};

static void fail(char *error, size_t capacity, const char *format, ...)
{
    if (!error || !capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, capacity, format, args);
    va_end(args);
}

int m8_host_native_choose(int cli, const char *environment, int target_default,
                          char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
    if (cli == 0 || cli == 1) return cli;
    if (!environment) return target_default != 0;
    if (!strcmp(environment, "0")) return 0;
    if (!strcmp(environment, "1")) return 1;
    fail(error, capacity, "M8_NATIVE_ENGINE requires 0 or 1: value=%s", environment);
    return -1;
}

int m8_host_native_default_environment(char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
    static const struct { const char *name, *value; } defaults[] = {
        {"M8RT_STALL_POLICY", "deterministic"},
        {"M8RT_NATIVE_EXCLUSIVE_TRANSFER", "1"},
        {"M8RT_NATIVE_DEVICE_DIRECT", "1"},
        {"M8RT_NATIVE_EVENT_LOOP", "1"},
        {"M8RT_NATIVE_SERVICE_CONTINUE", "1"},
        {"M8RT_NATIVE_DEADLINE_YIELD", M8_NATIVE_DEADLINE_YIELD_DEFAULT ? "1" : "0"},
        {"M8RT_AOT_TIMING", "0"}
    };
    for (unsigned i = 0; i < sizeof defaults/sizeof defaults[0]; ++i)
        if (!SDL_getenv(defaults[i].name) &&
            SDL_setenv_unsafe(defaults[i].name, defaults[i].value, 0)) {
            fail(error, capacity, "Cannot set native default: variable=%s", defaults[i].name);
            return -1;
        }
    return 0;
}

void m8_host_native_report_fallback(FILE *file, const char *phase, const char *reason)
{
    fprintf(file, "native_fallback=reference phase=%s reason=", phase);
    for (const char *p = reason; *p; ++p) fputc(*p == '\n' || *p == '\r' ? ' ' : *p, file);
    fputc('\n', file);
    fflush(file);
}

static int validate_environment(char *error, size_t capacity)
{
    static const char *const flags[] = {
        "M8RT_NATIVE_EXCLUSIVE_TRANSFER", "M8RT_NATIVE_EVENT_LOOP",
        "M8RT_NATIVE_DEVICE_DIRECT", "M8RT_NATIVE_SERVICE_CONTINUE",
        "M8RT_NATIVE_LOOKUP_CACHE", "M8RT_NATIVE_LIVE_VERIFY_CACHE",
        "M8RT_NATIVE_LIVE_REUSE", "M8RT_NATIVE_LIVE_DELTA", "M8RT_AOT_TIMING",
        "M8RT_NATIVE_DEADLINE_YIELD"
    };
    for (unsigned i = 0; i < sizeof flags/sizeof flags[0]; ++i) {
        const char *value = SDL_getenv(flags[i]);
        if (value && strcmp(value, "0") && strcmp(value, "1")) {
            fail(error, capacity, "%s requires 0 or 1: value=%s", flags[i], value);
            return -1;
        }
    }
    return 0;
}

static char *path(const char *explicit_path, const char *env, const char *filename)
{
    if (explicit_path) return SDL_strdup(explicit_path);
    const char *value = SDL_getenv(env);
    if (value) return SDL_strdup(value);
    const char *base = SDL_GetBasePath();
    char *bundled = NULL;
    if (base && SDL_asprintf(&bundled, "%slib/%s", base, filename) >= 0) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(bundled, &info)) return bundled;
        char *portable = NULL;
        if (SDL_asprintf(&portable, "%slibs.aarch64/%s", base, filename) >= 0 &&
            SDL_GetPathInfo(portable, &info) && info.type == SDL_PATHTYPE_FILE) {
            SDL_free(bundled);
            return portable;
        }
        SDL_free(portable);
        return bundled;
    }
    return NULL;
}

m8_host_native *m8_host_native_preload(int enabled, const char *step_path,
    const char *region_path, m8_host_aot *aot, char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
    m8_host_native *native = calloc(1, sizeof *native);
    if (!native) { fail(error, capacity, "Cannot allocate native loader"); return NULL; }
    if (!enabled) return native;
    if (validate_environment(error, capacity)) goto failed;
    if (!m8rt_native_live_available()) {
        fail(error, capacity, "Native switching is unavailable in this runtime build");
        goto failed;
    }
    const char *clock = SDL_getenv("M8RT_STALL_POLICY");
    if (!clock || strcmp(clock, "deterministic")) {
        fail(error, capacity,
             "Native switching requires M8RT_STALL_POLICY=deterministic: value=%s",
             clock ? clock : "<unset>");
        goto failed;
    }
    if (m8_host_aot_native_contract(aot, &native->config.verify, error, capacity)) goto failed;
    native->step_path = path(step_path, "M8RT_NATIVE_LIBRARY", "libm8native-step" M8_LIBRARY_SUFFIX);
    native->region_path = path(region_path, "M8RT_NATIVE_REGIONS", "libm8native-regions" M8_LIBRARY_SUFFIX);
    if (!native->step_path || !*native->step_path || !native->region_path || !*native->region_path) {
        fail(error, capacity, "Native switching requires instruction and region library paths");
        goto failed;
    }
    native->step = SDL_LoadObject(native->step_path);
    if (!native->step) {
        fail(error, capacity, "Cannot load native instructions %s: %s", native->step_path, SDL_GetError());
        goto failed;
    }
    native->config.step = (m8rt_native_step_fn)SDL_LoadFunction(native->step, "m8_native_step");
    if (!native->config.step) {
        fail(error, capacity, "Native instructions %s have no m8_native_step", native->step_path);
        goto failed;
    }
    native->region = SDL_LoadObject(native->region_path);
    if (!native->region) {
        fail(error, capacity, "Cannot load native regions %s: %s", native->region_path, SDL_GetError());
        goto failed;
    }
    native->config.region = (m8rt_native_step_fn)SDL_LoadFunction(native->region, "m8_native_region_step");
    native->config.region_verify = (int (*)(m8_mem *, uint32_t *))
        SDL_LoadFunction(native->region, "m8_native_region_verify_memory");
    if (!native->config.region || !native->config.region_verify) {
        fail(error, capacity, "Native regions %s lack execution or memory verification", native->region_path);
        goto failed;
    }
    native->config.chain = (m8_native_region_chain_fn)
        SDL_LoadFunction(native->region, "m8_native_region_chain");
    size_t (*slots)(void) = (size_t (*)(void))
        SDL_LoadFunction(native->region, "m8_native_region_chain_cache_slots");
    if ((native->config.chain != NULL) != (slots != NULL)) {
        fail(error, capacity, "Native regions %s have an incomplete chain interface", native->region_path);
        goto failed;
    }
    if (slots) {
        const char *timing = SDL_getenv("M8RT_AOT_TIMING");
        if (timing && !strcmp(timing, "1")) {
            fail(error, capacity, "Native region chains require M8RT_AOT_TIMING=0");
            goto failed;
        }
        native->config.chain_cache_slots = slots();
        if (!native->config.chain_cache_slots || native->config.chain_cache_slots > 65536u) {
            fail(error, capacity, "Native regions %s have invalid chain slots: slots=%zu",
                 native->region_path, native->config.chain_cache_slots);
            goto failed;
        }
    }
    int (*inline_services)(void) = (int (*)(void))
        SDL_LoadFunction(native->region, "m8_native_region_inline_services");
    int (*ram_exclusives)(void) = (int (*)(void))
        SDL_LoadFunction(native->region, "m8_native_region_ram_exclusives");
    const char *monitor = SDL_getenv("M8RT_NATIVE_EXCLUSIVE_TRANSFER");
    if (monitor && strcmp(monitor, "0") && strcmp(monitor, "1")) {
        fail(error, capacity,
             "M8RT_NATIVE_EXCLUSIVE_TRANSFER must be 0 or 1: value=%s", monitor);
        goto failed;
    }
    int transfer = monitor && !strcmp(monitor, "1");
    if (transfer && !m8rt_native_exclusive_transfer_available()) {
        fail(error, capacity, "Native exclusive transfer requires Unicorn monitor ABI 1");
        goto failed;
    }
    if (((inline_services && inline_services()) || (ram_exclusives && ram_exclusives())) && !transfer) {
        fail(error, capacity,
             "Native RAM exclusives require M8RT_NATIVE_EXCLUSIVE_TRANSFER=1 and Unicorn monitor ABI 1");
        goto failed;
    }
    const char *events = SDL_getenv("M8RT_NATIVE_EVENT_LOOP");
    if (events && strcmp(events, "0") && strcmp(events, "1")) {
        fail(error, capacity, "M8RT_NATIVE_EVENT_LOOP must be 0 or 1: value=%s", events);
        goto failed;
    }
    if (events && !strcmp(events, "1")) {
        int (*marker)(void) = (int (*)(void))
            SDL_LoadFunction(native->region, "m8_native_region_event_loop");
        native->config.event_chain = (m8_native_region_event_fn)
            SDL_LoadFunction(native->region, "m8_native_region_event_chain");
        if (!marker || marker() != 1 || !native->config.event_chain || !native->config.chain) {
            fail(error, capacity, "Native event loop requires matching event and chain exports");
            goto failed;
        }
    }
    native->enabled = 1;
    SDL_ClearError();
    return native;
failed:
    m8_host_native_close(native);
    return NULL;
}

m8_host_native *m8_host_native_preload_policy(int enabled, int fallback,
    const char *step_path, const char *region_path, m8_host_aot *aot,
    char *error, size_t capacity)
{
    char detail[512];
    m8_host_native *native = m8_host_native_preload(enabled, step_path, region_path,
                                                  aot, detail, sizeof detail);
    if (!native && enabled && fallback && strcmp(detail, "Cannot allocate native loader")) {
        native = calloc(1, sizeof *native);
        if (native) snprintf(native->fallback, sizeof native->fallback, "%s", detail);
    }
    if (!native) fail(error, capacity, "%s", detail);
    else if (error && capacity) *error = 0;
    return native;
}

int m8_host_native_enabled(const m8_host_native *native)
{
    return native && native->enabled;
}
const char *m8_host_native_fallback(const m8_host_native *native)
{
    return native ? native->fallback : "";
}
const m8rt_native_live_config *m8_host_native_config(const m8_host_native *native)
{
    return native && native->enabled ? &native->config : NULL;
}

unsigned m8_host_native_requirements(const m8_host_native *native)
{
    return native && native->enabled ? M8RT_AOT_REQUIRE_FIXED_MEMORY : 0;
}

int m8_host_native_attach(m8rt *rt, m8_host_native *native, char *error, size_t capacity)
{
    if (error && capacity) *error = 0;
    if (!native || !rt || *m8rt_error(rt) || native->attached) {
        fail(error, capacity, "Native attach needs an unused handle and a valid idle runtime");
        return -1;
    }
    if (!native->enabled) return 0;
    if (m8rt_set_native_live(rt, &native->config)) {
        fail(error, capacity,
             "Cannot enable native switching: fixed_memory=%d clock=%s direct_memory=%s detail=%s",
             m8rt_memory_offset(rt) != NULL,
             SDL_getenv("M8RT_STALL_POLICY") ? SDL_getenv("M8RT_STALL_POLICY") : "<unset>",
             SDL_getenv("M8RT_DIRECT_MEMORY") ? SDL_getenv("M8RT_DIRECT_MEMORY") : "<unset>",
             *m8rt_error(rt) ? m8rt_error(rt) : "runtime admission rejected configuration");
        return -1;
    }
    native->attached = 1;
    return 0;
}

int m8_host_native_attach_policy(m8rt *rt, m8_host_native *native, int fallback,
                                 char *error, size_t capacity)
{
    char detail[512];
    if (!m8_host_native_attach(rt, native, detail, sizeof detail)) {
        if (error && capacity) *error = 0;
        return 0;
    }
    /* This path precedes worker creation. Never recover an execution fault or
     * replay a device effect. A declined configuration has executed no guest. */
    if (fallback && rt && native && native->enabled && !*m8rt_error(rt) &&
        !m8rt_get(rt, M8RT_MAIN_ITERS) && !m8rt_get(rt, M8RT_UC_NS) &&
        !m8rt_get(rt, M8RT_BOOT_DONE) && !m8rt_get(rt, M8RT_GUEST_CYCLES) &&
        !m8rt_set_native_live(rt, NULL)) {
        native->enabled = 0;
        snprintf(native->fallback, sizeof native->fallback, "%s", detail);
        if (error && capacity) *error = 0;
        return 0;
    }
    fail(error, capacity, "%s", detail);
    return -1;
}

const char *m8_host_native_step_path(const m8_host_native *native)
{
    return native ? native->step_path : NULL;
}
const char *m8_host_native_region_path(const m8_host_native *native)
{
    return native ? native->region_path : NULL;
}
size_t m8_host_native_chain_slots(const m8_host_native *native)
{
    return native ? native->config.chain_cache_slots : 0;
}
void m8_host_native_close(m8_host_native *native)
{
    if (!native) return;
    if (native->region) SDL_UnloadObject(native->region);
    if (native->step) SDL_UnloadObject(native->step);
    SDL_free(native->region_path);
    SDL_free(native->step_path);
    free(native);
}

int m8_host_native_stats_json(FILE *file, const m8rt_native_live_stats *s)
{
    if (!file || !s) return -1;
    if (fprintf(file, "{\"enabled\":%d,\"active\":%d,\"code_disabled\":%d,"
                "\"enters\":%" PRIu64 ",\"leaves\":%" PRIu64 ",\"rejected_entries\":%" PRIu64 ","
                "\"aot_calls\":%" PRIu64 ",\"region_calls\":%" PRIu64 ",\"region_guest\":%" PRIu64 ","
                "\"step_calls\":%" PRIu64 ",\"step_guest\":%" PRIu64 ","
                "\"source_checks\":%" PRIu64 ","
                "\"cpu_import_words\":%" PRIu64 ",\"cpu_export_words\":%" PRIu64 ",\"pc_redirects\":%" PRIu64 ","
                "\"last_leave_pc\":%u,\"last_leave_reason\":%u,\"reasons\":{",
                s->enabled, s->active, s->code_disabled, s->enters, s->leaves,
                s->rejected_entries, s->aot_calls, s->region_calls, s->region_guest,
                s->step_calls, s->step_guest, s->source_checks,
                s->cpu_import_words, s->cpu_export_words, s->pc_redirects,
                s->last_leave_pc, s->last_leave_reason) < 0) return -1;
    for (unsigned i = 0; i < M8RT_NATIVE_LEAVE_COUNT; ++i)
        if (fprintf(file, "%s\"%s\":%" PRIu64, i ? "," : "",
                    m8rt_native_leave_name(i), s->reasons[i]) < 0) return -1;
    if (fputs("},\"architecture\":{", file) == EOF) return -1;
    for (unsigned i = 0; i < M8RT_NATIVE_ARCH_COUNT; ++i)
        if (fprintf(file, "%s\"%s\":%" PRIu64, i ? "," : "",
                    m8rt_native_arch_leave_name(i), s->architecture[i]) < 0) return -1;
    if (fprintf(file, "},\"handoff_pc_overflow\":%" PRIu64 ",\"handoff_pcs\":[", s->handoff_pc_overflow) < 0) return -1;
    unsigned written = 0;
    for (unsigned i = 0; i < M8RT_NATIVE_HANDOFF_PCS; ++i) {
        const m8rt_native_handoff_pc *p = &s->handoff_pcs[i];
        if (!p->count) continue;
        if (fprintf(file, "%s{\"pc\":%u,\"reason\":%u,\"architecture\":%u,\"count\":%" PRIu64 "}",
                    written++ ? "," : "", p->pc, p->reason, p->architecture, p->count) < 0) return -1;
    }
    if (fprintf(file, "],\"event_loop\":%d,\"events\":{", s->event_loop) < 0) return -1;
#define EVENT(field) if (fprintf(file, "\"" #field "\":%" PRIu64 ",", s->events.field) < 0) return -1
    EVENT(entries);EVENT(completed);EVENT(logical_slices);EVENT(enum_ticks);
    EVENT(main_hooks);EVENT(wait_hooks);EVENT(delay_hooks);EVENT(sampling_hooks);EVENT(audio_hooks);
    EVENT(irq_entries);EVENT(exception_returns);EVENT(memory_returns);EVENT(required_returns);EVENT(budget_returns);
#undef EVENT
    return fprintf(file, "\"host_returns\":%" PRIu64 "}}", s->events.host_returns) < 0 ? -1 : 0;
}

void m8_host_native_report_runtime(FILE *file, m8_host_native_notices *n,
    const m8rt_native_live_stats *s, const m8rt_native_live_rejection *r, uint32_t sample_pc)
{
    int written = 0;
    for (unsigned i = 0; i < M8RT_NATIVE_HANDOFF_PCS; ++i) {
        const m8rt_native_handoff_pc *p = &s->handoff_pcs[i];
        if (!p->count) continue;
        unsigned j;
        for (j = 0; j < n->handoff_count; ++j)
            if (n->handoffs[j].pc == p->pc && n->handoffs[j].reason == p->reason &&
                n->handoffs[j].architecture == p->architecture) break;
        if (j < n->handoff_count || n->handoff_count == M8RT_NATIVE_HANDOFF_PCS) continue;
        n->handoffs[n->handoff_count++] = *p;
        written = 1;
        fprintf(file, "native_handoff=unicorn reason=%s cause=%s pc=0x%08x count=%" PRIu64 "\n",
                m8rt_native_leave_name(p->reason),
                p->reason == M8RT_NATIVE_LEAVE_ARCHITECTURE ?
                    m8rt_native_arch_leave_name(p->architecture) : "none", p->pc, p->count);
    }
    if (s->handoff_pc_overflow && !n->overflow) {
        n->overflow = 1;
        written = 1;
        fprintf(file, "native_handoff=unicorn reason=diagnostic_table_full counts_retained=%" PRIu64 "\n",
                s->handoff_pc_overflow);
    }
    for (unsigned i = 0; i < M8RT_NATIVE_REJECT_COUNT; ++i) {
        if (!r->reasons[i] || n->rejected[i]) continue;
        n->rejected[i] = 1;
        written = 1;
        fprintf(file, "native_rejected=unicorn predicate=%s pc=0x%08x value=0x%08x count=%" PRIu64 "\n",
                m8rt_native_reject_name(i), r->first_pc[i], r->first_value[i], r->reasons[i]);
    }
    if (s->code_disabled && !n->code_disabled) {
        n->code_disabled = 1;
        written = 1;
        fprintf(file, "native_fallback=reference phase=runtime reason=compiled_code_disabled "
                "sample_pc=0x%08x aot_disabled=1\n", sample_pc);
    }
    if (written) fflush(file);
}
