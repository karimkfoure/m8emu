#define _GNU_SOURCE
#include "pc-profile.h"
#include <errno.h>

#if !defined(__linux__) || (!defined(__x86_64__) && !defined(__aarch64__))
int m8_profile_set_top_symbols(unsigned count) {
    if (!count || count > M8_PROFILE_MAX_TOP_SYMBOLS) { errno = EINVAL; return -1; }
    return 0;
}
int m8_profile_prepare(unsigned hz) {
    if (!hz) return 0;
    errno = ENOTSUP;
    return -1;
}
void m8_profile_set_context(volatile sig_atomic_t *scope) { (void)scope; }
int m8_profile_start(const char *aot_path) { (void)aot_path; return 0; }
void m8_profile_stop(void) {}
void m8_profile_write_json(FILE *out) { fputs("null", out); }
void m8_profile_write_raw_json(FILE *out) { fputs("null", out); }
void m8_profile_write_pcs_json(FILE *out, const char *aot_path, const uintptr_t *pcs, size_t count) {
    (void)aot_path; (void)pcs; (void)count;
    fputs("null", out);
}
void m8_profile_destroy(void) {}
#else

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <link.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

/* The handler only touches this buffer and sig_atomic_t values. CPU-time
 * signals target the calling thread, so there is exactly one producer. */
enum { SAMPLE_CAPACITY = 262144, DEFAULT_TOP_SYMBOLS = 30 };
static unsigned top_symbol_limit = DEFAULT_TOP_SYMBOLS;
static uintptr_t *sample_pc;
static uint8_t *sample_scope;
static volatile sig_atomic_t *scope_pointer;
static volatile sig_atomic_t sample_count, dropped_count, timer_overruns, sampling;
static unsigned requested_hz;
static timer_t profile_timer;
static int timer_created, handler_installed, prepared, stopped;
static struct sigaction previous_action;
static sigset_t previous_mask;
static struct timespec cpu_start, cpu_end;
static char aot_real[PATH_MAX], executable_real[PATH_MAX];

int m8_profile_set_top_symbols(unsigned count) {
    if (!count || count > M8_PROFILE_MAX_TOP_SYMBOLS) { errno = EINVAL; return -1; }
    if (sampling) { errno = EBUSY; return -1; }
    top_symbol_limit = count;
    return 0;
}

enum pc_class { CLASS_AOT, CLASS_TCG, CLASS_UNICORN, CLASS_RUNTIME,
    CLASS_LIBC_LIBM, CLASS_BENCHMARK, CLASS_OTHER, CLASS_UNKNOWN,
    CLASS_NATIVE_FALLBACK, CLASS_NATIVE_REGIONS, CLASS_COUNT };
static const char *const class_names[CLASS_COUNT] = {
    "aot", "tcg", "libunicorn", "m8rt", "libc_libm", "benchmark", "other", "unknown",
    "native_fallback", "native_regions"
};
enum profile_context { CONTEXT_OUTSIDE, CONTEXT_RENDER, CONTEXT_GRAPH, CONTEXT_INVALID, CONTEXT_COUNT };
static const char *const scope_names[8] = {
    "outside", "FM", "MACRO", "WAV", "HYPER", "GRAPH", "VOICE", "invalid"
};
typedef struct {
    char aot[PATH_MAX], executable[PATH_MAX];
} attribution_paths;
typedef struct {
    const uintptr_t *pcs;
    const uint8_t *scopes;
    size_t count;
    const char *aot_path;
    int context_available, from_timer;
    unsigned hz;
    uint64_t dropped, overruns;
    size_t capacity;
    double cpu_seconds;
} attribution_input;
typedef struct {
    uintptr_t begin, end, offset;
    char permissions[5];
    char *path;
    enum pc_class class_id;
    uint64_t samples;
    int deleted;
} executable_map;
typedef struct {
    uint64_t begin, size;
    const char *name;
} function_symbol;
typedef struct {
    char *path;
    uintptr_t bias;
    int bias_known;
    void *image;
    size_t image_size;
    function_symbol *functions;
    size_t function_count;
    int symtab, dynsym, error;
} module_symbols;
typedef struct {
    enum pc_class class_id;
    const char *module;
    const char *name;
    uint64_t offset, samples;
    uint64_t contexts[CONTEXT_COUNT];
    int unresolved;
} symbol_bin;

static void record_pc(int signo, siginfo_t *info, void *context) {
    (void)signo;
    if (!sampling) return;
    if (info->si_code == SI_TIMER && info->si_overrun > 0) {
        sig_atomic_t overrun = info->si_overrun;
        timer_overruns = timer_overruns > INT_MAX - overrun ? INT_MAX : timer_overruns + overrun;
    }
    uintptr_t pc;
#if defined(__x86_64__)
    pc = (uintptr_t)((ucontext_t *)context)->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
    pc = (uintptr_t)((ucontext_t *)context)->uc_mcontext.pc;
#else
    (void)context;
    pc = 0;
#endif
    sig_atomic_t count = sample_count;
    if (count < SAMPLE_CAPACITY) {
        sample_pc[count] = pc;
        sig_atomic_t scope = scope_pointer ? *scope_pointer : 0;
        sample_scope[count] = scope >= 0 && scope <= 6 ? (uint8_t)scope : 7;
        sample_count = count + 1;
    } else if (dropped_count < INT_MAX) {
        dropped_count++;
    }
}

int m8_profile_prepare(unsigned hz) {
    if (!hz) return 0;
    if (prepared || hz > 10000) { errno = EINVAL; return -1; }
    sample_pc = calloc(SAMPLE_CAPACITY, sizeof(*sample_pc));
    if (!sample_pc) return -1;
    sample_scope = calloc(SAMPLE_CAPACITY, sizeof(*sample_scope));
    if (!sample_scope) goto fail;
    /* Fault in each page before measurement. calloc can otherwise defer this
     * work to the handler on the first sample in a page. */
    for (size_t i = 0; i < SAMPLE_CAPACITY; i += 4096 / sizeof(*sample_pc))
        ((volatile uintptr_t *)sample_pc)[i] = 0;
    for (size_t i = 0; i < SAMPLE_CAPACITY; i += 4096)
        ((volatile uint8_t *)sample_scope)[i] = 0;
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPROF);
    if (sigprocmask(SIG_BLOCK, &blocked, &previous_mask)) goto fail;
    struct sigaction action = {0};
    action.sa_sigaction = record_pc;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGPROF, &action, &previous_action)) {
        sigprocmask(SIG_SETMASK, &previous_mask, NULL);
        goto fail;
    }
    handler_installed = 1;
    struct sigevent event = {0};
    event.sigev_notify = SIGEV_THREAD_ID;
    event.sigev_signo = SIGPROF;
    event._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &event, &profile_timer)) {
        sigaction(SIGPROF, &previous_action, NULL);
        sigprocmask(SIG_SETMASK, &previous_mask, NULL);
        handler_installed = 0;
        goto fail;
    }
    timer_created = prepared = 1;
    requested_hz = hz;
    return 0;
fail: {
    int saved_errno = errno;
    free(sample_pc);
    free(sample_scope);
    sample_pc = NULL;
    sample_scope = NULL;
    errno = saved_errno;
    return -1;
}
}

void m8_profile_set_context(volatile sig_atomic_t *scope) {
    if (prepared && !sampling && !stopped) scope_pointer = scope;
}

int m8_profile_start(const char *aot_path) {
    if (!requested_hz) return 0;
    if (!prepared || sampling || stopped) { errno = EINVAL; return -1; }
    if (aot_path && !realpath(aot_path, aot_real)) {
        if (strlen(aot_path) >= sizeof(aot_real)) { errno = ENAMETOOLONG; return -1; }
        strcpy(aot_real, aot_path);
    }
    ssize_t length = readlink("/proc/self/exe", executable_real, sizeof(executable_real) - 1);
    if (length >= 0) executable_real[length] = 0;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start)) return -1;
    struct itimerspec interval = {0};
    interval.it_interval.tv_nsec = 1000000000L / requested_hz;
    interval.it_interval.tv_sec = interval.it_interval.tv_nsec / 1000000000L;
    interval.it_interval.tv_nsec %= 1000000000L;
    interval.it_value = interval.it_interval;
    sampling = 1;
    if (timer_settime(profile_timer, 0, &interval, NULL)) { sampling = 0; return -1; }
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPROF);
    if (sigprocmask(SIG_UNBLOCK, &blocked, NULL)) {
        struct itimerspec off = {0};
        timer_settime(profile_timer, 0, &off, NULL);
        sampling = 0;
        return -1;
    }
    return 0;
}

void m8_profile_stop(void) {
    if (!requested_hz || stopped) return;
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPROF);
    sigprocmask(SIG_BLOCK, &blocked, NULL);
    sampling = 0;
    struct itimerspec off = {0};
    if (timer_created) timer_settime(profile_timer, 0, &off, NULL);
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end);
    /* A signal queued just before disarming belongs to this timer. Consume it
     * before restoring the caller's original disposition during destroy. */
    struct timespec no_wait = {0};
    while (sigtimedwait(&blocked, NULL, &no_wait) >= 0) {}
    stopped = 1;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void initialize_paths(attribution_paths *paths, const char *aot_path) {
    memset(paths, 0, sizeof(*paths));
    if (aot_path && !realpath(aot_path, paths->aot)) {
        if (strlen(aot_path) < sizeof(paths->aot)) strcpy(paths->aot, aot_path);
    }
    ssize_t length = readlink("/proc/self/exe", paths->executable, sizeof(paths->executable) - 1);
    if (length >= 0) paths->executable[length] = 0;
}

static enum pc_class classify_path(const attribution_paths *paths, const char *path) {
    const char *base = base_name(path);
    if (!*path || !strncmp(path, "/dev/zero", 9) || !strncmp(path, "[anon", 5)) return CLASS_TCG;
    if (*paths->aot && !strcmp(path, paths->aot)) return CLASS_AOT;
    if (!strncmp(base, "libunicorn", 10)) return CLASS_UNICORN;
    if (!strncmp(base, "libm8rt", 7)) return CLASS_RUNTIME;
    if (!strncmp(base, "libm8native-step", 16)) return CLASS_NATIVE_FALLBACK;
    if (!strncmp(base, "libm8native-regions", 18)) return CLASS_NATIVE_REGIONS;
    if (!strncmp(base, "libc.so", 7) || !strncmp(base, "libm.so", 7)) return CLASS_LIBC_LIBM;
    if (*paths->executable && !strcmp(path, paths->executable)) return CLASS_BENCHMARK;
    return CLASS_OTHER;
}

static executable_map *read_executable_maps(const attribution_paths *paths, size_t *count, int *error) {
    FILE *file = fopen("/proc/self/maps", "r");
    if (!file) { *error = errno; return NULL; }
    executable_map *maps = NULL;
    size_t capacity = 0, line_capacity = 0;
    char *line = NULL;
    while (getline(&line, &line_capacity, file) >= 0) {
        unsigned long long begin, end, offset;
        char permissions[5];
        int used = 0;
        if (sscanf(line, "%llx-%llx %4s %llx %*s %*s %n", &begin, &end, permissions, &offset, &used) != 4)
            continue;
        if (!strchr(permissions, 'x')) continue;
        char *path = line + used;
        while (*path == ' ' || *path == '\t') path++;
        path[strcspn(path, "\r\n")] = 0;
        size_t length = strlen(path);
        int deleted = length >= 10 && !strcmp(path + length - 10, " (deleted)");
        if (deleted) path[length - 10] = 0;
        if (*count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 16;
            executable_map *new_maps = realloc(maps, new_capacity * sizeof(*maps));
            if (!new_maps) { *error = ENOMEM; break; }
            maps = new_maps;
            capacity = new_capacity;
        }
        char *copy = strdup(path);
        if (!copy) { *error = ENOMEM; break; }
        executable_map *map = &maps[(*count)++];
        *map = (executable_map){.begin = (uintptr_t)begin, .end = (uintptr_t)end,
            .offset = (uintptr_t)offset, .path = copy, .class_id = classify_path(paths, path), .deleted = deleted};
        memcpy(map->permissions, permissions, sizeof(map->permissions));
    }
    free(line);
    fclose(file);
    return maps;
}

typedef struct { module_symbols *module; const attribution_paths *paths; } bias_request;
static int find_load_bias(struct dl_phdr_info *info, size_t size, void *opaque) {
    (void)size;
    bias_request *request = opaque;
    module_symbols *module = request->module;
    const char *name = info->dlpi_name && *info->dlpi_name ? info->dlpi_name : request->paths->executable;
    char canonical[PATH_MAX];
    if (realpath(name, canonical)) name = canonical;
    if (!strcmp(name, module->path)) {
        module->bias = (uintptr_t)info->dlpi_addr;
        module->bias_known = 1;
        return 1;
    }
    return 0;
}

static int inside_file(size_t file_size, uint64_t offset, uint64_t length) {
    return offset <= file_size && length <= file_size - offset;
}
static int function_compare(const void *a, const void *b) {
    const function_symbol *left = a, *right = b;
    if (left->begin != right->begin) return left->begin < right->begin ? -1 : 1;
    if (left->size != right->size) return left->size < right->size ? -1 : 1;
    return strcmp(left->name, right->name);
}

static void read_symbols(module_symbols *module) {
    int fd = open(module->path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { module->error = errno; return; }
    struct stat statbuf;
    if (fstat(fd, &statbuf)) {
        module->error = errno;
        close(fd);
        return;
    }
    if (statbuf.st_size < (off_t)sizeof(Elf64_Ehdr)) {
        module->error = ENOEXEC;
        close(fd);
        return;
    }
    module->image_size = (size_t)statbuf.st_size;
    module->image = mmap(NULL, module->image_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (module->image == MAP_FAILED) { module->image = NULL; module->error = errno; return; }
    const unsigned char *bytes = module->image;
    const Elf64_Ehdr *header = (const Elf64_Ehdr *)bytes;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) || header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB || header->e_shentsize != sizeof(Elf64_Shdr) ||
        !inside_file(module->image_size, header->e_shoff, (uint64_t)header->e_shnum * sizeof(Elf64_Shdr))) {
        module->error = ENOEXEC;
        return;
    }
    const Elf64_Shdr *sections = (const Elf64_Shdr *)(bytes + header->e_shoff);
    size_t capacity = 0;
    for (unsigned i = 0; i < header->e_shnum; i++) {
        const Elf64_Shdr *section = &sections[i];
        if (section->sh_type != SHT_SYMTAB && section->sh_type != SHT_DYNSYM) continue;
        if (section->sh_link >= header->e_shnum || section->sh_entsize != sizeof(Elf64_Sym) ||
            !inside_file(module->image_size, section->sh_offset, section->sh_size)) continue;
        const Elf64_Shdr *strings = &sections[section->sh_link];
        if (!inside_file(module->image_size, strings->sh_offset, strings->sh_size)) continue;
        if (section->sh_type == SHT_SYMTAB) module->symtab = 1;
        else module->dynsym = 1;
        const char *names = (const char *)bytes + strings->sh_offset;
        const Elf64_Sym *symbols = (const Elf64_Sym *)(bytes + section->sh_offset);
        size_t n = (size_t)(section->sh_size / sizeof(*symbols));
        for (size_t j = 0; j < n; j++) {
            const Elf64_Sym *symbol = &symbols[j];
            if (ELF64_ST_TYPE(symbol->st_info) != STT_FUNC || symbol->st_shndx == SHN_UNDEF ||
                !symbol->st_size || symbol->st_name >= strings->sh_size) continue;
            const char *name = names + symbol->st_name;
            if (!memchr(name, 0, (size_t)(strings->sh_size - symbol->st_name)) || !*name) continue;
            if (module->function_count == capacity) {
                size_t new_capacity = capacity ? capacity * 2 : 256;
                function_symbol *new_functions = realloc(module->functions, new_capacity * sizeof(*new_functions));
                if (!new_functions) { module->error = ENOMEM; return; }
                module->functions = new_functions;
                capacity = new_capacity;
            }
            module->functions[module->function_count++] = (function_symbol){
                .begin = symbol->st_value, .size = symbol->st_size, .name = name};
        }
    }
    if (module->function_count > 1)
        qsort(module->functions, module->function_count, sizeof(*module->functions), function_compare);
}

static module_symbols *get_module(module_symbols **modules, size_t *count, const executable_map *map,
    const attribution_paths *paths) {
    for (size_t i = 0; i < *count; i++) if (!strcmp((*modules)[i].path, map->path)) return &(*modules)[i];
    module_symbols *new_modules = realloc(*modules, (*count + 1) * sizeof(**modules));
    if (!new_modules) return NULL;
    *modules = new_modules;
    module_symbols *module = &new_modules[(*count)++];
    *module = (module_symbols){.path = map->path, .bias = map->begin - map->offset};
    bias_request request = {.module = module, .paths = paths};
    dl_iterate_phdr(find_load_bias, &request);
    read_symbols(module);
    return module;
}

static const function_symbol *resolve_function(const module_symbols *module, uint64_t address) {
    size_t low = 0, high = module->function_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (module->functions[middle].begin <= address) low = middle + 1;
        else high = middle;
    }
    /* Symbols can alias one address. Check all aliases at the nearest start.
     * Never assign the nearest name when the PC is outside its ELF extent. */
    if (!low) return NULL;
    uint64_t nearest = module->functions[low - 1].begin;
    do {
        const function_symbol *symbol = &module->functions[--low];
        if (symbol->begin != nearest) break;
        if (address - symbol->begin < symbol->size) return symbol;
    } while (low);
    return NULL;
}

static void json_string(FILE *out, const char *string) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)string; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p < 32) fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}
static int bin_compare(const void *a, const void *b) {
    const symbol_bin *left = a, *right = b;
    if (left->samples != right->samples) return left->samples > right->samples ? -1 : 1;
    int order = strcmp(left->module, right->module);
    if (order) return order;
    if (left->offset != right->offset) return left->offset < right->offset ? -1 : 1;
    return strcmp(left->name, right->name);
}

static void write_classes(FILE *out, const uint64_t *counts, uint64_t total) {
    fputc('{', out);
    for (unsigned i = 0; i < CLASS_COUNT; i++) {
        if (i) fputc(',', out);
        json_string(out, class_names[i]);
        fprintf(out, ":{\"samples\":%" PRIu64 ",\"share\":%.9f}", counts[i], total ? (double)counts[i] / total : 0.0);
    }
    fputc('}', out);
}

static void write_symbol_bin(FILE *out, const symbol_bin *bin, uint64_t samples, uint64_t total) {
    fputs("{\"class\":", out); json_string(out, class_names[bin->class_id]);
    fputs(",\"module\":", out); json_string(out, bin->module);
    fputs(",\"symbol\":", out); json_string(out, bin->name);
    fprintf(out, ",\"offset\":\"0x%" PRIx64 "\",\"unresolved\":%s,\"samples\":%" PRIu64 ",\"share\":%.9f}",
        bin->offset, bin->unresolved ? "true" : "false", samples, total ? (double)samples / total : 0.0);
}

static void write_context_symbols(FILE *out, symbol_bin *bins, size_t count, unsigned context, uint64_t total) {
    symbol_bin *top[M8_PROFILE_MAX_TOP_SYMBOLS];
    size_t top_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (!bins[i].contexts[context]) continue;
        size_t position = 0;
        while (position < top_count && top[position]->contexts[context] >= bins[i].contexts[context]) position++;
        if (position == top_symbol_limit) continue;
        size_t new_count = top_count < top_symbol_limit ? top_count + 1 : top_symbol_limit;
        for (size_t j = new_count - 1; j > position; j--) top[j] = top[j - 1];
        top[position] = &bins[i];
        top_count = new_count;
    }
    fputc('[', out);
    for (size_t i = 0; i < top_count; i++) {
        if (i) fputc(',', out);
        write_symbol_bin(out, top[i], top[i]->contexts[context], total);
    }
    fputc(']', out);
}

static void write_attribution_json(FILE *out, const attribution_input *input) {
    attribution_paths paths;
    initialize_paths(&paths, input->aot_path);
    size_t map_count = 0, module_count = 0, bin_count = 0;
    int maps_error = 0, attribution_error = 0;
    executable_map *maps = read_executable_maps(&paths, &map_count, &maps_error);
    module_symbols *modules = NULL;
    symbol_bin *bins = NULL;
    uint64_t counts[CLASS_COUNT] = {0};
    uint64_t scope_counts[8] = {0};
    uint64_t context_totals[CONTEXT_COUNT] = {0};
    uint64_t context_classes[CONTEXT_COUNT][CLASS_COUNT] = {{0}};
    size_t total = input->count;
    if (!input->pcs && total) { attribution_error = EINVAL; total = 0; }
    for (size_t i = 0; i < total; i++) {
        uintptr_t pc = input->pcs[i];
        executable_map *map = NULL;
        for (size_t j = 0; j < map_count; j++) if (pc >= maps[j].begin && pc < maps[j].end) { map = &maps[j]; break; }
        enum pc_class class_id = map ? map->class_id : CLASS_UNKNOWN;
        uint8_t scope = input->scopes ? input->scopes[i] : 0;
        if (scope > 7) scope = 7;
        enum profile_context context = scope == 0 ? CONTEXT_OUTSIDE :
            scope <= 4 ? CONTEXT_RENDER : scope <= 6 ? CONTEXT_GRAPH : CONTEXT_INVALID;
        counts[class_id]++;
        scope_counts[scope]++;
        context_totals[context]++;
        context_classes[context][class_id]++;
        if (map) map->samples++;
        const char *module_name = map ? map->path : "";
        const char *name = "<unresolved page>";
        uint64_t offset = pc & ~(uint64_t)4095;
        int unresolved = 1;
        if (class_id == CLASS_TCG) {
            name = "<anonymous executable mapping>";
            offset = map->begin;
            unresolved = 0;
        } else if (map && map->path[0] == '/' && !map->deleted) {
            module_symbols *module = get_module(&modules, &module_count, map, &paths);
            if (module) {
                uint64_t address = pc - module->bias;
                const function_symbol *symbol = resolve_function(module, address);
                if (symbol) { name = symbol->name; offset = symbol->begin; unresolved = 0; }
                else offset = address & ~(uint64_t)4095;
            } else attribution_error = ENOMEM;
        }
        size_t index;
        for (index = 0; index < bin_count; index++) {
            symbol_bin *bin = &bins[index];
            if (bin->class_id == class_id && bin->offset == offset && bin->unresolved == unresolved &&
                !strcmp(bin->module, module_name) && !strcmp(bin->name, name)) break;
        }
        if (index == bin_count) {
            symbol_bin *new_bins = realloc(bins, (bin_count + 1) * sizeof(*bins));
            if (!new_bins) { attribution_error = ENOMEM; continue; }
            bins = new_bins;
            bins[bin_count++] = (symbol_bin){.class_id = class_id, .module = module_name,
                .name = name, .offset = offset, .unresolved = unresolved};
        }
        bins[index].samples++;
        bins[index].contexts[context]++;
    }
    if (bin_count > 1) qsort(bins, bin_count, sizeof(*bins), bin_compare);
    if (input->from_timer) {
        fprintf(out, "{\"requested_hz\":%u,\"samples\":%zu,\"dropped\":%" PRIu64 ",\"timer_overruns\":%" PRIu64 ",\"capacity\":%zu,"
            "\"cpu_seconds\":%.9f,\"effective_hz\":%.3f,\"clock\":\"CLOCK_THREAD_CPUTIME_ID\",\"scope\":\"calling thread\",",
            input->hz, total, input->dropped, input->overruns, input->capacity, input->cpu_seconds,
            input->cpu_seconds > 0 ? (double)total / input->cpu_seconds : 0.0);
    } else {
        fprintf(out, "{\"source\":\"provided_pcs\",\"samples\":%zu,", total);
    }
    fprintf(out,
        "\"tcg_identification\":\"anonymous executable mappings; heuristic\","
        "\"maps_error\":%d,\"attribution_error\":%d,\"context_available\":%s,\"classes\":",
        maps_error, attribution_error, input->context_available ? "true" : "false");
    write_classes(out, counts, (uint64_t)total);
    size_t top = bin_count < top_symbol_limit ? bin_count : top_symbol_limit;
    uint64_t top_samples = 0;
    for (size_t i = 0; i < top; i++) top_samples += bins[i].samples;
    fprintf(out, ",\"top_symbol_limit\":%u,\"symbol_bins\":%zu,\"top_symbol_samples\":%" PRIu64 ",\"top_symbols\":[",
        top_symbol_limit, bin_count, top_samples);
    for (size_t i = 0; i < top; i++) {
        if (i) fputc(',', out);
        write_symbol_bin(out, &bins[i], bins[i].samples, (uint64_t)total);
    }
    fputs("],\"scope_samples\":{", out);
    for (unsigned i = 0; i < 8; i++) {
        if (i) fputc(',', out);
        json_string(out, !input->context_available && !i ? "untagged" : scope_names[i]);
        fprintf(out, ":%" PRIu64, scope_counts[i]);
    }
    fputs("},\"contexts\":{", out);
    const char *context_names[CONTEXT_COUNT] = {
        input->context_available ? "outside" : "untagged", "render", "remaining_graph", "invalid"
    };
    for (unsigned i = 0; i < CONTEXT_COUNT; i++) {
        if (i) fputc(',', out);
        json_string(out, context_names[i]);
        fprintf(out, ":{\"samples\":%" PRIu64 ",\"share\":%.9f,\"classes\":", context_totals[i],
            total ? (double)context_totals[i] / total : 0.0);
        write_classes(out, context_classes[i], context_totals[i]);
        fputs(",\"top_symbols\":", out);
        write_context_symbols(out, bins, bin_count, i, context_totals[i]);
        fputc('}', out);
    }
    fputs("},\"symbol_files\":[", out);
    for (size_t i = 0; i < module_count; i++) {
        module_symbols *module = &modules[i];
        if (i) fputc(',', out);
        fputs("{\"path\":", out); json_string(out, module->path);
        fprintf(out, ",\"load_bias\":\"0x%" PRIxPTR "\",\"load_bias_from_loader\":%s,"
            "\"symtab\":%s,\"dynsym\":%s,\"function_entries\":%zu,\"error\":%d}",
            module->bias, module->bias_known ? "true" : "false", module->symtab ? "true" : "false",
            module->dynsym ? "true" : "false", module->function_count, module->error);
    }
    fputs("],\"executable_maps\":[", out);
    for (size_t i = 0; i < map_count; i++) {
        executable_map *map = &maps[i];
        if (i) fputc(',', out);
        fprintf(out, "{\"begin\":\"0x%" PRIxPTR "\",\"end\":\"0x%" PRIxPTR "\",\"bytes\":%" PRIuPTR
            ",\"file_offset\":\"0x%" PRIxPTR "\",\"permissions\":", map->begin, map->end,
            map->end - map->begin, map->offset);
        json_string(out, map->permissions);
        fputs(",\"path\":", out); json_string(out, map->path);
        fputs(",\"class\":", out); json_string(out, class_names[map->class_id]);
        fprintf(out, ",\"deleted\":%s,\"samples\":%" PRIu64 "}", map->deleted ? "true" : "false", map->samples);
    }
    fputs("]}", out);
    free(bins);
    for (size_t i = 0; i < module_count; i++) {
        free(modules[i].functions);
        if (modules[i].image) munmap(modules[i].image, modules[i].image_size);
    }
    free(modules);
    for (size_t i = 0; i < map_count; i++) free(maps[i].path);
    free(maps);
}

void m8_profile_write_json(FILE *out) {
    if (!requested_hz) { fputs("null", out); return; }
    m8_profile_stop();
    attribution_input input = {
        .pcs = sample_pc, .scopes = sample_scope, .count = (size_t)sample_count,
        .aot_path = aot_real, .context_available = scope_pointer != NULL, .from_timer = 1,
        .hz = requested_hz, .dropped = (uint64_t)dropped_count, .overruns = (uint64_t)timer_overruns,
        .capacity = SAMPLE_CAPACITY, .cpu_seconds = (double)(cpu_end.tv_sec - cpu_start.tv_sec) +
            (double)(cpu_end.tv_nsec - cpu_start.tv_nsec) / 1e9
    };
    write_attribution_json(out, &input);
}

void m8_profile_write_raw_json(FILE *out) {
    if (!requested_hz) { fputs("null", out); return; }
    m8_profile_stop();
    fprintf(out,"{\"schema\":\"m8-host-pcs/1\",\"samples\":%zu,\"dropped\":%" PRIu64 ",\"pcs\":[",
            (size_t)sample_count,(uint64_t)dropped_count);
    for (size_t i=0;i<(size_t)sample_count;i++) {
        if(i) fputc(',',out);
        fprintf(out,"\"0x%" PRIxPTR "\"",sample_pc[i]);
    }
    fputs("]}\n",out);
}

void m8_profile_write_pcs_json(FILE *out, const char *aot_path, const uintptr_t *pcs, size_t count) {
    attribution_input input = {.pcs = pcs, .count = count, .aot_path = aot_path};
    write_attribution_json(out, &input);
}

void m8_profile_destroy(void) {
    if (!prepared) return;
    m8_profile_stop();
    if (timer_created) timer_delete(profile_timer);
    if (handler_installed) sigaction(SIGPROF, &previous_action, NULL);
    sigprocmask(SIG_SETMASK, &previous_mask, NULL);
    free(sample_pc);
    free(sample_scope);
    sample_pc = NULL;
    sample_scope = NULL;
    scope_pointer = NULL;
    requested_hz = 0;
    sample_count = dropped_count = timer_overruns = sampling = 0;
    timer_created = handler_installed = prepared = stopped = 0;
    aot_real[0] = executable_real[0] = 0;
}

#ifdef M8_PROFILE_SELF_TEST
#include <assert.h>
static volatile uint64_t self_test_checksum;
__attribute__((noinline)) static void self_test_spin(uint64_t count) {
    for (uint64_t i = 0; i < count; i++) self_test_checksum += i;
}
static double self_test_cpu_now(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) == 0);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}
int main(int argc, char **argv) {
    assert(m8_profile_set_top_symbols(0) == -1 && errno == EINVAL);
    assert(m8_profile_set_top_symbols(M8_PROFILE_MAX_TOP_SYMBOLS + 1) == -1 && errno == EINVAL);
    attribution_paths test_paths = {0};
    assert(classify_path(&test_paths, "/tmp/libm8native-step.so") == CLASS_NATIVE_FALLBACK);
    assert(classify_path(&test_paths, "/tmp/libm8native-step-pc.so") == CLASS_NATIVE_FALLBACK);
    assert(classify_path(&test_paths, "/tmp/libm8native-regions.so") == CLASS_NATIVE_REGIONS);
    function_symbol functions[] = {
        {.begin = 0x100, .size = 0x20, .name = "first"},
        {.begin = 0x130, .size = 0x10, .name = "second"}
    };
    module_symbols module = {.functions = functions, .function_count = 2};
    assert(resolve_function(&module, 0xff) == NULL);
    assert(resolve_function(&module, 0x100) == &functions[0]);
    assert(resolve_function(&module, 0x11f) == &functions[0]);
    assert(resolve_function(&module, 0x120) == NULL);
    assert(resolve_function(&module, 0x130) == &functions[1]);
    assert(resolve_function(&module, 0x140) == NULL);
    if (argc > 1 && !strcmp(argv[1], "raw-pcs")) {
        uintptr_t pcs[] = {0x1010, UINT64_C(0xabcdef12345678), 0x1010};
        requested_hz=997;sample_pc=pcs;sample_count=3;dropped_count=2;
        m8_profile_write_raw_json(stdout);
        sample_pc=NULL;sample_count=requested_hz=dropped_count=0;
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "many-pcs")) {
        if (argc > 2) assert(m8_profile_set_top_symbols((unsigned)strtoul(argv[2], NULL, 10)) == 0);
        uintptr_t pcs[110];
        for (size_t i = 0; i < sizeof(pcs) / sizeof(*pcs); i++) pcs[i] = (i + 1) * 4096;
        m8_profile_write_pcs_json(stdout, NULL, pcs, sizeof(pcs) / sizeof(*pcs));
        fputc('\n', stdout);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "pcs")) {
        if (argc > 2) assert(m8_profile_set_top_symbols((unsigned)strtoul(argv[2], NULL, 10)) == 0);
        void *code = mmap(NULL, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(code != MAP_FAILED);
        uintptr_t pcs[] = {(uintptr_t)self_test_spin, (uintptr_t)self_test_spin + 1,
            (uintptr_t)code, (uintptr_t)code + 8, 1};
        attribution_paths paths;
        initialize_paths(&paths, NULL);
        fputs("{\"provided\":", stdout);
        m8_profile_write_pcs_json(stdout, NULL, pcs, sizeof(pcs) / sizeof(*pcs));
        fputs(",\"aot_path\":", stdout);
        m8_profile_write_pcs_json(stdout, paths.executable, pcs, sizeof(pcs) / sizeof(*pcs));
        fputs(",\"empty\":", stdout);
        m8_profile_write_pcs_json(stdout, NULL, NULL, 0);
        fputs(",\"invalid\":", stdout);
        m8_profile_write_pcs_json(stdout, NULL, NULL, 1);
        fputs("}\n", stdout);
        assert(!prepared && !requested_hz && !aot_real[0] && !executable_real[0]);
        assert(munmap(code, 4096) == 0);
        return 0;
    }
    assert(m8_profile_prepare(0) == 0);
    assert(m8_profile_start(NULL) == 0);
    m8_profile_stop();
    m8_profile_destroy();
    assert(m8_profile_prepare(997) == 0);
    volatile sig_atomic_t scope = 1;
    if (argc > 1) m8_profile_set_context(&scope);
    assert(m8_profile_start(NULL) == 0);
    double start = self_test_cpu_now();
    while (self_test_cpu_now() - start < 0.6) self_test_spin(100000);
    void *code = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(code != MAP_FAILED);
#if defined(__x86_64__)
    static const unsigned char machine_code[] = {0x48, 0xff, 0xcf, 0x75, 0xfb, 0xc3};
#else
    static const uint32_t machine_code[] = {0xd1000400, 0xb5ffffe0, 0xd65f03c0};
#endif
    memcpy(code, machine_code, sizeof(machine_code));
    __builtin___clear_cache(code, (char *)code + sizeof(machine_code));
    assert(mprotect(code, 4096, PROT_READ | PROT_EXEC) == 0);
    scope = 5;
    start = self_test_cpu_now();
    while (self_test_cpu_now() - start < 0.6) ((void (*)(uint64_t))code)(10000000);
    if (argc > 1) {
        scope = 0;
        start = self_test_cpu_now();
        while (self_test_cpu_now() - start < 0.3) self_test_spin(100000);
        scope = 99;
        start = self_test_cpu_now();
        while (self_test_cpu_now() - start < 0.02) self_test_spin(100000);
    }
    m8_profile_stop();
    assert(sample_count > 100);
    m8_profile_write_json(stdout);
    fputc('\n', stdout);
    m8_profile_destroy();
    assert(munmap(code, 4096) == 0);
    assert(m8_profile_prepare(997) == 0);
    m8_profile_destroy();
    return 0;
}
#endif
#endif
