#include "aot.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

struct m8_host_aot {
    SDL_SharedObject *library;
    char *path;
    unsigned entries, services, requirements;
    m8_aot_fn (*lookup)(uint32_t);
    uint32_t (*entry_list)(const uint32_t **);
    m8rt *attached;
};

static void fail(char *error, size_t capacity, const char *format, ...)
{
    if (!error || !capacity) return;
    va_list args;
    va_start(args, format);
    vsnprintf(error, capacity, format, args);
    va_end(args);
}

m8_host_aot *m8_host_aot_preload(const char *path, char *error, size_t error_capacity)
{
    if (error && error_capacity) *error = 0;
    m8_host_aot *aot = calloc(1, sizeof *aot);
    if (!aot) {
        fail(error, error_capacity, "Cannot allocate AOT loader");
        return NULL;
    }
    char *bundled = NULL;
    if (!path) {
        path = SDL_getenv("M8RT_AOT_LIBRARY");
        if (!path) {
            const char *base = SDL_GetBasePath();
            SDL_PathInfo info;
            const char *directories[] = {"lib", "libs.aarch64"};
            for (unsigned i = 0; base && i < sizeof directories/sizeof directories[0]; ++i) {
                SDL_free(bundled); bundled = NULL;
                if (SDL_asprintf(&bundled, "%s%s/libm8aot.so", base, directories[i]) >= 0 &&
                    SDL_GetPathInfo(bundled, &info) && info.type == SDL_PATHTYPE_FILE) {
                    path = bundled;
                    break;
                }
            }
        }
    }
    if (!path || !*path) {
        SDL_free(bundled);
        SDL_ClearError();
        return aot;
    }
    aot->path = SDL_strdup(path);
    SDL_free(bundled);
    path = aot->path;
    if (!aot->path) {
        fail(error, error_capacity, "Cannot allocate AOT path");
        goto failed;
    }
    aot->library = SDL_LoadObject(path);
    if (!aot->library) {
        fail(error, error_capacity, "Cannot load AOT %s: %s", path, SDL_GetError());
        goto failed;
    }
    aot->lookup =
        (m8_aot_fn (*)(uint32_t))SDL_LoadFunction(aot->library, "m8_aot_lookup");
    if (!aot->lookup) {
        fail(error, error_capacity, "AOT %s has no m8_aot_lookup: %s", path, SDL_GetError());
        goto failed;
    }
    aot->entry_list =
        (uint32_t (*)(const uint32_t **))SDL_LoadFunction(aot->library, "m8_aot_entries");
    if (!aot->entry_list) {
        fail(error, error_capacity, "AOT %s has no m8_aot_entries: %s", path, SDL_GetError());
        goto failed;
    }
    const uint32_t *pcs = NULL;
    aot->entries = aot->entry_list(&pcs);
    if (!aot->entries || aot->entries > 4096 || !pcs) {
        fail(error, error_capacity, "AOT %s has invalid entries: count=%u, present=%u",
             path, aot->entries, pcs != NULL);
        goto failed;
    }
    for (unsigned i = 0; i < aot->entries; ++i) {
        if (!aot->lookup(pcs[i] & ~1u)) {
            fail(error, error_capacity, "AOT %s has no function for entry %u: pc=%08x",
                 path, i, pcs[i]);
            goto failed;
        }
        for (unsigned j = 0; j < i; ++j) if ((pcs[i] & ~1u) == (pcs[j] & ~1u)) {
            fail(error, error_capacity, "AOT %s repeats entry %u: pc=%08x, first=%u",
                 path, i, pcs[i], j);
            goto failed;
        }
    }
    uint32_t (*services)(void) =
        (uint32_t (*)(void))SDL_LoadFunction(aot->library, "m8_aot_memory_services");
    aot->services = services ? services() : 0;
    if (aot->services & ~M8_AOT_SERVICE_NVIC) {
        fail(error, error_capacity, "AOT %s requests unsupported services: flags=%08x",
             path, aot->services);
        goto failed;
    }
    uint32_t (*fixed)(void) =
        (uint32_t (*)(void))SDL_LoadFunction(aot->library, "m8_aot_fixed_memory_required");
    aot->requirements = fixed && fixed() ? M8RT_AOT_REQUIRE_FIXED_MEMORY : 0;
    SDL_ClearError();
    return aot;
failed:
    m8_host_aot_close(NULL, aot);
    return NULL;
}

unsigned m8_host_aot_requirements(const m8_host_aot *aot)
{
    return aot ? aot->requirements : 0;
}

int m8_host_aot_attach(m8rt *rt, m8_host_aot *aot, char *error, size_t error_capacity)
{
    if (error && error_capacity) *error = 0;
    if (!aot || !rt || *m8rt_error(rt) || aot->attached) {
        fail(error, error_capacity, "AOT attach needs an unused handle and a valid idle runtime: %s",
             m8rt_error(rt));
        return -1;
    }
    if (!aot->library) return 0;
    if ((aot->requirements & M8RT_AOT_REQUIRE_FIXED_MEMORY) && !m8rt_memory_offset(rt)) {
        const char *env = SDL_getenv("M8RT_FIXED_MEMORY");
        fail(error, error_capacity,
             "AOT %s requires fixed memory: requirements=%u, M8RT_FIXED_MEMORY=%s; "
             "preload before creation and use m8rt_create_aot with these requirements",
             aot->path, aot->requirements, env ? env : "<unset>");
        return -1;
    }
    void (*fallback)(int) =
        (void (*)(int))SDL_LoadFunction(aot->library, "m8_aot_set_slow_fallback");
    if (fallback) fallback(0);
    aot->attached = rt;
    if (m8rt_set_aot(rt, aot->lookup, aot->entry_list)) {
        fail(error, error_capacity, "Cannot install AOT hooks from %s", aot->path);
        goto detach;
    }
    if (m8rt_set_aot_services(rt, aot->services)) {
        fail(error, error_capacity, "Cannot enable AOT services from %s: flags=%08x",
             aot->path, aot->services);
        goto detach;
    }
    SDL_ClearError();
    return 0;
detach:
    /* The documented idle-owner contract makes this detach unconditional.
     * A partially installed hook must not outlive its library. */
    if (m8rt_set_aot(rt, NULL, NULL)) {
        fail(error, error_capacity, "AOT detach failed; runtime must be idle: %s", aot->path);
        return -1;
    }
    aot->attached = NULL;
    return -1;
}

m8_host_aot *m8_host_aot_open(m8rt *rt, const char *path,
                             char *error, size_t error_capacity)
{
    m8_host_aot *aot = m8_host_aot_preload(path, error, error_capacity);
    if (aot && m8_host_aot_attach(rt, aot, error, error_capacity)) {
        if (m8_host_aot_close(rt, aot)) {
            /* Keep code loaded if an installed hook cannot be removed. */
            aot->library = NULL;
            m8_host_aot_close(NULL, aot);
        }
        return NULL;
    }
    return aot;
}

const char *m8_host_aot_path(const m8_host_aot *aot)
{
    return aot ? aot->path : NULL;
}
unsigned m8_host_aot_entries(const m8_host_aot *aot)
{
    return aot ? aot->entries : 0;
}
unsigned m8_host_aot_services(const m8_host_aot *aot)
{
    return aot ? aot->services : 0;
}
int m8_host_aot_native_contract(const m8_host_aot *aot,
    int (**verify)(m8_mem *, uint32_t *), char *error, size_t error_capacity)
{
    if (error && error_capacity) *error = 0;
    if (verify) *verify = NULL;
    if (!aot || !aot->library || !verify) {
        fail(error, error_capacity, "Native execution requires a loaded guarded AOT library");
        return -1;
    }
    uint32_t (*guard)(void) =
        (uint32_t (*)(void))SDL_LoadFunction(aot->library, "m8_aot_native_code_guard");
    int (*check)(m8_mem *, uint32_t *) =
        (int (*)(m8_mem *, uint32_t *))SDL_LoadFunction(aot->library, "m8_aot_verify_memory");
    if (!guard || guard() != 1u || !check) {
        fail(error, error_capacity,
             "Native execution requires AOT code-write guards and memory verification: path=%s",
             aot->path);
        return -1;
    }
    *verify = check;
    SDL_ClearError();
    return 0;
}

void m8_host_aot_snapshot(m8rt *rt, m8_host_aot_stats *out)
{
    if (!out) return;
    *out = (m8_host_aot_stats){0};
    if (!rt) return;
    out->timing_enabled = (unsigned)m8rt_get(rt, M8RT_AOT_TIMING_ENABLED);
    out->calls = m8rt_get(rt, M8RT_AOT_CALLS);
    out->native_ns = m8rt_get(rt, M8RT_AOT_NATIVE_NS);
    out->hook_ns = m8rt_get(rt, M8RT_AOT_HOOK_NS);
    out->runtime_ns = m8rt_get(rt, M8RT_UC_NS);
}
int m8_host_aot_native_dispatch(const m8_host_aot *aot,
    m8_aot_fn (**lookup)(uint32_t), uint32_t (**entries)(const uint32_t **),
    unsigned *services)
{
    if (!aot || !aot->library || !lookup || !entries || !services) return -1;
    *lookup = aot->lookup;
    *entries = aot->entry_list;
    *services = aot->services;
    return 0;
}
int m8_host_aot_close(m8rt *rt, m8_host_aot *aot)
{
    if (!aot) return 0;
    if (rt && aot->attached && (aot->attached != rt || m8rt_set_aot(rt, NULL, NULL))) return -1;
    if (aot->library) SDL_UnloadObject(aot->library);
    SDL_free(aot->path);
    free(aot);
    return 0;
}
