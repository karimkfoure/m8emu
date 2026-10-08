/* Exact guest-boundary input for application differential checks. */
#include "session_script.h"
#include "native.h"
#include "native_boot.h"
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicorn/arm.h>

typedef struct script_run {
    m8_session *session;
    FILE *pcm, *display, *blocks, *states;
    m8_session_script_stats stats;
    unsigned held, note;
} script_run;

static int fail(script_run *r, const char *format, ...)
{
    if (!r->session->error[0]) {
        va_list args;
        va_start(args, format);
        vsnprintf(r->session->error, sizeof r->session->error, format, args);
        va_end(args);
    }
    return -1;
}

static FILE *create(script_run *r, const char *directory, const char *name)
{
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/%s", directory, name);
    if (n < 0 || (size_t)n >= sizeof path) {
        fail(r, "Session output path is too long: file=%s", name);
        return NULL;
    }
    FILE *file = fopen(path, "wbx");
    if (!file) fail(r, "Cannot create session output: file=%s error=%d", name, errno);
    return file;
}

static int pump(void *context)
{
    script_run *r = context;
    m8_session *s = r->session;
    unsigned char bytes[4096];
    size_t n;
    m8rt_audio_position position;
    while ((n = m8rt_read_local_pcm(s->rt, bytes, sizeof bytes, &position))) {
        const char *first = n % 4 || n != position.frames*4 ? "complete_block" :
            !r->stats.blocks && position.frame ? "initial_frame" : r->stats.blocks && position.frame != r->stats.end_frame ? "contiguous_frames" : NULL;
        if (first)
            return fail(r, "Session PCM failed: first=%s bytes=%zu frames=%u frame=%" PRIu64
                        " expected=%" PRIu64, first, n, position.frames, position.frame, r->stats.end_frame);
        if (!r->stats.blocks) r->stats.first_frame = position.frame;
        if (fwrite(bytes, 1, n, r->pcm) != n ||
            fprintf(r->blocks, "{\"frame\":%" PRIu64 ",\"cycles\":%" PRIu64
                    ",\"frames\":%u,\"bytes\":%zu}\n",
                    position.frame, position.cycles, position.frames, n) < 0)
            return fail(r, "Cannot write session PCM: error=%d", errno);
        r->stats.pcm_bytes += n;
        r->stats.end_frame = position.frame + position.frames;
        ++r->stats.blocks;
    }
    while (m8rt_read_pcm(s->rt, bytes, sizeof bytes)) {}
    while ((n = m8rt_read_cdc(s->rt, bytes, sizeof bytes))) {
        if (fwrite(bytes, 1, n, r->display) != n)
            return fail(r, "Cannot write session display: error=%d", errno);
        r->stats.display_bytes += n;
        if (m8_demo_feed_cdc(s->demo, bytes, n) < 0)
            return fail(r, "%s", m8_demo_error(s->demo));
    }
    return 0;
}

static int json_string(FILE *file, const char *text)
{
    if (fputc('"', file) == EOF) return -1;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            if (fputc('\\', file) == EOF || fputc(*p, file) == EOF) return -1;
        } else if (*p < 32) {
            if (fprintf(file, "\\u%04x", *p) < 0) return -1;
        } else if (fputc(*p, file) == EOF) return -1;
    }
    return fputc('"', file) == EOF ? -1 : 0;
}

typedef struct register_snapshot {
    uint32_t r[16], s[32], special[10];
} register_snapshot;

static const char *const special_names[] = {
    "xpsr", "fpscr", "cpsr", "ipsr", "msp", "psp", "control",
    "primask", "basepri", "faultmask"
};

static int read_registers(script_run *r, register_snapshot *snapshot)
{
    static const int special_ids[] = {
        UC_ARM_REG_XPSR, UC_ARM_REG_FPSCR, UC_ARM_REG_CPSR, UC_ARM_REG_IPSR,
        UC_ARM_REG_MSP, UC_ARM_REG_PSP, UC_ARM_REG_CONTROL, UC_ARM_REG_PRIMASK,
        UC_ARM_REG_BASEPRI, UC_ARM_REG_FAULTMASK
    };
    for (unsigned i = 0; i < 16; ++i) {
        int id = i < 13 ? UC_ARM_REG_R0 + (int)i :
                 i == 13 ? UC_ARM_REG_SP : i == 14 ? UC_ARM_REG_LR : UC_ARM_REG_PC;
        snapshot->r[i] = m8rt_reg_read(r->session->rt, id);
        if (*m8rt_error(r->session->rt))
            return fail(r, "Session register read failed: register=r%u detail=%s",
                        i, m8rt_error(r->session->rt));
    }
    for (unsigned i = 0; i < 32; ++i) {
        snapshot->s[i] = m8rt_reg_read(r->session->rt, UC_ARM_REG_S0 + (int)i);
        if (*m8rt_error(r->session->rt))
            return fail(r, "Session register read failed: register=s%u detail=%s",
                        i, m8rt_error(r->session->rt));
    }
    for (unsigned i = 0; i < 10; ++i) {
        snapshot->special[i] = m8rt_reg_read(r->session->rt, special_ids[i]);
        if (*m8rt_error(r->session->rt))
            return fail(r, "Session register read failed: register=%s detail=%s",
                        special_names[i], m8rt_error(r->session->rt));
    }
    return 0;
}

static int registers_to(FILE *file, const register_snapshot *snapshot)
{
    if (fputs(",\"registers\":{\"r\":[", file) == EOF) return -1;
    for (unsigned i = 0; i < 16; ++i)
        if (fprintf(file, "%s%" PRIu32, i ? "," : "", snapshot->r[i]) < 0) return -1;
    if (fputs("],\"s\":[", file) == EOF) return -1;
    for (unsigned i = 0; i < 32; ++i)
        if (fprintf(file, "%s%" PRIu32, i ? "," : "", snapshot->s[i]) < 0) return -1;
    if (fputc(']', file) == EOF) return -1;
    for (unsigned i = 0; i < 10; ++i)
        if (fprintf(file, ",\"%s\":%" PRIu32, special_names[i], snapshot->special[i]) < 0) return -1;
    return fputc('}', file) == EOF ? -1 : 0;
}

static int state_to(script_run *r, FILE *file, const char *label,
                    const register_snapshot *registers)
{
    m8_session *s = r->session;
    unsigned x = 0, y = 0, model = 0, major = 0, minor = 0, patch = 0, font = 0;
    int cursor = m8_demo_cursor(s->demo, &x, &y);
    int identity = m8_demo_identity(s->demo, &model, &major, &minor, &patch, &font);
    if (fputs("{\"label\":", file) == EOF || json_string(file, label) < 0 ||
        fprintf(file, ",\"iters\":%" PRIu64 ",\"cycles\":%" PRIu64
                ",\"cyccnt\":%" PRIu64 ",\"pc\":%" PRIu64 ",\"frames\":%" PRIu64
                ",\"held\":%u,\"note\":%u,\"queued_input\":%" PRIu64
                ",\"cursor_present\":%d,\"cursor_x\":%u,\"cursor_y\":%u,\"playhead\":%d"
                ",\"identity_present\":%d,\"model\":%u,\"major\":%u,\"minor\":%u,\"patch\":%u,\"font\":%u"
                ",\"pcm_bytes\":%" PRIu64 ",\"display_bytes\":%" PRIu64 ",\"blocks\":%" PRIu64 ",\"selected\":",
                m8rt_get(s->rt, M8RT_MAIN_ITERS), m8rt_get(s->rt, M8RT_GUEST_CYCLES),
                m8rt_get(s->rt, M8RT_CYCCNT), m8rt_get(s->rt, M8RT_PC),
                m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES)/4, r->held, r->note,
                m8rt_get(s->rt, M8RT_HOST_QUEUED_BYTES), cursor, x, y,
                m8_demo_playhead(s->demo), identity, model, major, minor, patch, font,
                r->stats.pcm_bytes, r->stats.display_bytes, r->stats.blocks) < 0) return -1;
    char text[512];
    text[0] = 0;
    m8_demo_selected(s->demo, text, sizeof text);
    if (json_string(file, text) < 0 || fputs(",\"lines\":[", file) == EOF) return -1;
    for (unsigned i = 0; m8_demo_line(s->demo, i, text, sizeof text); ++i)
        if ((i && fputc(',', file) == EOF) || json_string(file, text) < 0) return -1;
    if (fputs("],\"cursor\":", file) == EOF) return -1;
    if (cursor) { if (fprintf(file, "[%u,%u]", x, y) < 0) return -1; }
    else if (fputs("null", file) == EOF) return -1;
    if (fputs(",\"firmware\":", file) == EOF) return -1;
    if (identity) { if (fprintf(file, "[%u,%u,%u,%u]", model, major, minor, patch) < 0) return -1; }
    else if (fputs("null", file) == EOF) return -1;
    if (registers_to(file, registers) < 0) return -1;
    m8rt_native_live_stats native;
    if (m8rt_read_native_live_stats(s->rt, &native) ||
        fputs(",\"native\":", file) == EOF || m8_host_native_stats_json(file, &native)) return -1;
    m8rt_native_boot_app_stats boot;
    if (m8rt_read_native_boot_app_stats(s->rt, &boot) ||
        fputs(",\"native_boot\":", file) == EOF || m8_host_native_boot_stats_json(file, &boot)) return -1;
    return fputs("}\n", file) == EOF ? -1 : 0;
}

static int state(script_run *r, const char *label)
{
    /* Observe both CPU owners through the public API; do not publish or step. */
    register_snapshot registers;
    if (read_registers(r, &registers) < 0) return -1;
    if (state_to(r, r->states, label, &registers) < 0 ||
        fputs("session_state=", stdout) == EOF || state_to(r, stdout, label, &registers) < 0 ||
        fflush(r->states) || fflush(stdout))
        return fail(r, "Cannot write session state: error=%d", errno);
    return 0;
}

static int number(const char *text, unsigned base, uint64_t limit, uint64_t *value)
{
    if (!*text || *text == '-' || *text == '+') return -1;
    char *end;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, (int)base);
    if (errno || end == text || *end || parsed > limit) return -1;
    *value = parsed;
    return 0;
}

int m8_session_script_run(m8_session *s, const char *script, const char *output,
                          const _Atomic int *stop, m8_session_script_stats *stats)
{
    script_run r = {.session=s, .note=255};
    FILE *input = !strcmp(script, "-") ? stdin : fopen(script, "rb");
    int result = -1;
    if (stats) *stats = (m8_session_script_stats){0};
    if (!input) return fail(&r, "Cannot open session script: error=%d", errno);
    r.pcm = create(&r, output, "pcm.bin");
    r.display = create(&r, output, "display.bin");
    r.blocks = create(&r, output, "blocks.jsonl");
    r.states = create(&r, output, "states.jsonl");
    if (!r.pcm || !r.display || !r.blocks || !r.states) goto close;
    s->demo = m8_demo_create(s->rt, pump, &r);
    if (!s->demo || m8_demo_enable_selection(s->demo) < 0) {
        fail(&r, "Cannot create session display observer"); goto close;
    }
    char line[512];
    unsigned line_number = 0;
    while (!atomic_load_explicit(stop, memory_order_acquire) && fgets(line, sizeof line, input)) {
        ++line_number;
        if (!strchr(line, '\n') && !feof(input)) {
            fail(&r, "Session script line is too long: line=%u", line_number); goto close;
        }
        char command[64], arg[128], extra[128], trailing[2];
        char *comment = strchr(line, '#');
        if (comment) *comment = 0;
        int fields = sscanf(line, "%63s %127s %127s %1s", command, arg, extra, trailing);
        if (fields < 1) continue;
        uint64_t value;
        if (!strcmp(command, "main") && fields == 2 && !number(arg, 10, 10000000, &value)) {
            while (value) {
                if (atomic_load_explicit(stop, memory_order_acquire)) {
                    fail(&r, "Session interrupted: line=%u", line_number); goto close;
                }
                uint64_t chunk = value > 200 ? 200 : value;
                int rc = m8rt_run_main_iters(s->rt, chunk);
                if (rc || pump(&r) < 0) {
                    fail(&r, "Session guest execution failed: line=%u result=%d detail=%s",
                         line_number, rc, m8rt_error(s->rt)); goto close;
                }
                value -= chunk;
            }
        } else if (!strcmp(command, "keys") && fields == 2 && !number(arg, 16, 255, &value)) {
            unsigned char packet[2] = {'C', (unsigned char)value};
            if (m8rt_queue_host_bytes(s->rt, packet, sizeof packet) < 0) {
                fail(&r, "Session controller queue failed: line=%u", line_number); goto close;
            }
            r.held = (unsigned)value;
        } else if (!strcmp(command, "keyjazz") && fields == 3 && !number(arg, 16, 255, &value)) {
            uint64_t velocity;
            if (number(extra, 16, 127, &velocity) || (value > 127 && value != 255)) {
                fail(&r, "Session keyjazz values are invalid: line=%u", line_number); goto close;
            }
            unsigned char packet[3] = {'K', (unsigned char)value, (unsigned char)velocity};
            if (m8rt_queue_host_bytes(s->rt, packet, value == 255 ? 2 : 3) < 0) {
                fail(&r, "Session keyjazz queue failed: line=%u", line_number); goto close;
            }
            r.note = (unsigned)value;
        } else if (!strcmp(command, "state") && fields == 2) {
            if (state(&r, arg) < 0) goto close;
        } else if (!strcmp(command, "end") && fields == 1) {
            uint64_t queued = m8rt_get(s->rt, M8RT_HOST_QUEUED_BYTES);
            uint64_t published_pcm = m8rt_get(s->rt, M8RT_LOCAL_PCM_BYTES);
            uint64_t published_display = m8rt_get(s->rt, M8RT_CDC_BYTES);
            unsigned model = 0, major = 0, minor = 0, patch = 0, font = 0;
            int identity = m8_demo_identity(s->demo, &model, &major, &minor, &patch, &font);
            const char *first = r.held ? "released_keys" : r.note != 255 ? "released_keyjazz" :
                queued ? "input_drained" : published_pcm != r.stats.pcm_bytes ? "complete_pcm" :
                published_display != r.stats.display_bytes ? "complete_display" : !identity ? "display_identity" :
                model || major != 6 || minor != 5 || patch != 2 ? "supported_firmware" : NULL;
            if (first) {
                fail(&r, "Session final boundary failed: first=%s held=%u note=%u queued=%" PRIu64
                     " pcm=%" PRIu64 "/%" PRIu64 " display=%" PRIu64 "/%" PRIu64
                     " identity=%d model=%u version=%u.%u.%u font=%u",
                     first, r.held, r.note, queued, r.stats.pcm_bytes, published_pcm,
                     r.stats.display_bytes, published_display, identity, model, major, minor, patch, font); goto close;
            }
            if (state(&r, "final") < 0) goto close;
            ++r.stats.commands;
            r.stats.complete = 1;
            result = 0;
            goto close;
        } else {
            fail(&r, "Session script command is invalid: line=%u command=%s", line_number, command);
            goto close;
        }
        ++r.stats.commands;
    }
    fail(&r, "Session script ended before end: line=%u interrupted=%d", line_number,
         atomic_load_explicit(stop, memory_order_acquire));
close:
    if (input != stdin && fclose(input) && !result) result = fail(&r, "Session input close failed");
    FILE *files[] = {r.pcm, r.display, r.blocks, r.states};
    for (unsigned i = 0; i < sizeof files/sizeof files[0]; ++i)
        if (files[i] && fclose(files[i]) && !result) result = fail(&r, "Session output close failed: file=%u", i);
    if (stats) *stats = r.stats;
    return result;
}
