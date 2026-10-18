/* Ordered button press and release sequences for project setup with
 * the original Headless firmware. */
#include "demo.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCREEN_WIDTH = 320, SCREEN_HEIGHT = 320, MAX_LINES = 40,
       LINE_SIZE = 128, PACKET_SIZE = 512 };

struct m8_demo {
    m8rt *rt;
    m8_demo_pump_fn pump;
    void *context;
    uint64_t deadline;
    unsigned held, actions;
    int fixed_slices;
    unsigned model, major, minor, patch, font, have_identity;
    unsigned cursor_x, cursor_y, have_cursor;
    unsigned text_offset;
    unsigned line_count, lines_dirty;
    uint8_t last_color[3];
    uint8_t *selection;
    uint8_t text_rows[SCREEN_HEIGHT];
    unsigned dense_lines;
    char cells[SCREEN_HEIGHT][SCREEN_WIDTH];
    char lines[MAX_LINES][LINE_SIZE];
    uint8_t packet[PACKET_SIZE];
    size_t packet_length;
    unsigned escaped;
    char error[512];
};

static int fail(m8_demo *d, const char *format, ...) {
    if (!d->error[0]) {
        va_list args;
        va_start(args, format);
        vsnprintf(d->error, sizeof d->error, format, args);
        va_end(args);
    }
    return -1;
}

static unsigned u16(const uint8_t *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static int cyan(const uint8_t *color) {
    return color[0] <= 110 && color[1] >= 140 && color[2] >= 140;
}

static int apply_packet(m8_demo *d) {
    const uint8_t *p = d->packet;
    size_t n = d->packet_length;
    if (!n) return 0;
    if (p[0] == 0xff) {
        if (n != 6) return fail(d, "Display identity packet has %zu bytes; expected 6", n);
        d->model = p[1]; d->major = p[2]; d->minor = p[3];
        d->patch = p[4]; d->font = p[5]; d->have_identity = 1;
        if (d->model != 0 || d->font > 1)
            return fail(d, "Unsupported display model %u, font %u", d->model, d->font);
        d->text_offset = d->font ? 4 : 3;
    } else if (p[0] == 0xfd) {
        if (n != 12) return fail(d, "Display character packet has %zu bytes; expected 12", n);
        unsigned x = u16(p + 2), y = u16(p + 4) + d->text_offset;
        if (x < SCREEN_WIDTH && y < SCREEN_HEIGHT) {
            d->cells[y][x] = p[1] >= 32 && p[1] < 127 ? (char)p[1] : '.';
            d->text_rows[y] = 1;
            if (d->selection) {
                unsigned selected = cyan(p + 6) || cyan(p + 9);
                for (unsigned yy = y; yy < SCREEN_HEIGHT && yy < y+7; ++yy)
                    for (unsigned xx = x; xx < SCREEN_WIDTH && xx < x+5; ++xx)
                        d->selection[yy*SCREEN_WIDTH+xx] = (uint8_t)selected;
            }
            d->lines_dirty = 1;
        }
    } else if (p[0] == 0xfe) {
        if (n != 5 && n != 8 && n != 9 && n != 12)
            return fail(d, "Display rectangle packet has invalid length %zu", n);
        unsigned x = u16(p + 1), y = u16(p + 3), w = 1, h = 1;
        if (n == 9 || n == 12) { w = u16(p + 5); h = u16(p + 7); }
        if (n == 8) memcpy(d->last_color, p + 5, 3);
        if (n == 12) memcpy(d->last_color, p + 9, 3);
        if (w && h && cyan(d->last_color)) {
            d->cursor_x = x; d->cursor_y = y; d->have_cursor = 1;
        }
        unsigned end_x = x + w, end_y = y + h;
        if (end_x > SCREEN_WIDTH) end_x = SCREEN_WIDTH;
        if (end_y > SCREEN_HEIGHT) end_y = SCREEN_HEIGHT;
        for (unsigned yy = y; yy < end_y; ++yy)
            if (x < end_x) {
                memset(d->cells[yy] + x, 0, end_x - x);
                if (!x && end_x == SCREEN_WIDTH) d->text_rows[yy] = 0;
                if (d->selection)
                    memset(d->selection + yy*SCREEN_WIDTH+x, cyan(d->last_color), end_x-x);
            }
        d->lines_dirty = 1;
    }
    return 0;
}

m8_demo *m8_demo_create(m8rt *rt, m8_demo_pump_fn pump, void *context) {
    if (!rt || !pump) return NULL;
    m8_demo *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->rt = rt; d->pump = pump; d->context = context;
    const char *dense = getenv("M8_APP_DENSE_OBSERVER");
    d->dense_lines = !dense || strcmp(dense, "0");
    d->text_offset = 3;
    d->deadline = SDL_GetTicksNS() + UINT64_C(120000000000);
    return d;
}

void m8_demo_destroy(m8_demo *d) { if (d) free(d->selection); free(d); }
void m8_demo_bind(m8_demo *d,m8rt *rt,m8_demo_pump_fn pump,void *context) {
    d->rt=rt;d->pump=pump;d->context=context;
}
m8_demo *m8_demo_copy(const m8_demo *source) {
    if(!source)return NULL;
    m8_demo *copy=malloc(sizeof *copy);if(!copy)return NULL;
    memcpy(copy,source,sizeof *copy);copy->rt=NULL;copy->pump=NULL;copy->context=NULL;copy->selection=NULL;
    if(source->selection) {
        copy->selection=malloc(SCREEN_HEIGHT*SCREEN_WIDTH);
        if(!copy->selection){free(copy);return NULL;}
        memcpy(copy->selection,source->selection,SCREEN_HEIGHT*SCREEN_WIDTH);
    }
    return copy;
}

const char *m8_demo_error(const m8_demo *d) {
    return d ? d->error : "Demo allocation failed";
}

int m8_demo_feed_cdc(m8_demo *d, const void *bytes, size_t count) {
    if (!d || d->error[0]) return -1;
    const uint8_t *input = bytes;
    for (size_t i = 0; i < count; ++i) {
        uint8_t b = input[i];
        if (d->escaped) {
            d->escaped = 0;
            if (b == 0xdc) b = 0xc0;
            else if (b == 0xdd) b = 0xdb;
            else return fail(d, "Invalid display escape byte %02X", b);
        } else if (b == 0xc0) {
            if (apply_packet(d) < 0) return -1;
            d->packet_length = 0;
            continue;
        } else if (b == 0xdb) {
            d->escaped = 1;
            continue;
        }
        if (d->packet_length == sizeof d->packet)
            return fail(d, "Display packet exceeds %zu bytes", sizeof d->packet);
        d->packet[d->packet_length++] = b;
    }
    return 0;
}

static void update_lines(m8_demo *d) {
    if (!d->lines_dirty) return;
    d->line_count = 0;
    for (unsigned y = 0; y < SCREEN_HEIGHT && d->line_count < MAX_LINES; ++y) {
        /* Glyphs occupy few pixel rows. Partial erases keep a conservative
         * flag; only a full-row erase proves that a flagged row is empty. */
        if (!d->dense_lines && !d->text_rows[y]) continue;
        char *line = d->lines[d->line_count];
        unsigned used = 0;
        int have_cells = 0;
        for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
            char c = d->cells[y][x];
            if (c) have_cells = 1;
            if (c && c != ' ' && used + 1 < LINE_SIZE) line[used++] = c;
        }
        if (have_cells) { line[used] = 0; ++d->line_count; }
    }
    d->lines_dirty = 0;
}

int m8_demo_line(m8_demo *d, unsigned line, char *out, size_t capacity) {
    if (!d || !out || !capacity) return 0;
    update_lines(d);
    if (line >= d->line_count) { out[0] = 0; return 0; }
    snprintf(out, capacity, "%s", d->lines[line]);
    return 1;
}

static int contains(m8_demo *d, const char *text) {
    update_lines(d);
    for (unsigned i = 0; i < d->line_count; ++i)
        if (strstr(d->lines[i], text)) return 1;
    return 0;
}

static int require(m8_demo *d, const char *text) {
    if (contains(d, text)) return 0;
    return fail(d, "Expected display %s; observed %s | %s | %s", text,
                d->line_count > 0 ? d->lines[0] : "<empty>",
                d->line_count > 1 ? d->lines[1] : "",
                d->line_count > 2 ? d->lines[2] : "");
}

static int slice(m8_demo *d, unsigned frames) {
    if (d->error[0]) return -1;
    if (SDL_GetTicksNS() >= d->deadline) return fail(d, "Demo setup exceeded 120 seconds");
    uint64_t target = m8rt_get(d->rt, M8RT_LOCAL_PCM_BYTES) / 4 + frames;
    int result = m8rt_run_audio_frames(d->rt, target, d->fixed_slices?0:25000);
    if (result < 0) return fail(d, "Guest execution failed: %s", m8rt_error(d->rt));
    if (result > 0) return fail(d, "Guest execution stopped during demo setup");
    if (d->pump(d->context) < 0) return fail(d, "Host pump failed during demo setup");
    return d->error[0] ? -1 : 0;
}

int m8_demo_advance(m8_demo *d, double seconds) {
    if (!d || d->error[0]) return -1;
    if (!(seconds >= 0 && seconds < 7.15)) return fail(d, "Invalid guest dwell %.6f", seconds);
    uint32_t cycles = (uint32_t)(seconds * 600000000.0 + 0.5);
    uint32_t start = (uint32_t)m8rt_get(d->rt, M8RT_CYCCNT);
    uint64_t deadline = SDL_GetTicksNS() + UINT64_C(30000000000);
    for (;;) {
        uint32_t elapsed = (uint32_t)m8rt_get(d->rt, M8RT_CYCCNT) - start;
        if (elapsed >= cycles) break;
        if (SDL_GetTicksNS() >= deadline)
            return fail(d, "Guest clock failed to advance: start=%u, elapsed=%u, target=%u",
                        start, elapsed, cycles);
        uint64_t frames = (uint64_t)(cycles - elapsed) * 44100 / 600000000 + 1;
        if (frames > 256) frames = 256;
        if (slice(d, (unsigned)frames) < 0) return -1;
    }
    return 0;
}

static int mask(m8_demo *d, unsigned keys) {
    uint8_t command[2] = {'C', (uint8_t)keys};
    if (m8rt_queue_host_bytes(d->rt, command, sizeof command) < 0)
        return fail(d, "Controller queue failed at action %u: %s", d->actions, m8rt_error(d->rt));
    d->held = keys; ++d->actions;
    return m8_demo_advance(d, .080);
}

int m8_demo_tap(m8_demo *d, unsigned keys) {
    if (!d || d->error[0]) return -1;
    if (keys > 0xff) return fail(d, "Invalid controller mask %X", keys);
    return mask(d, keys) < 0 ? -1 : mask(d, 0);
}

static int chord(m8_demo *d, unsigned first, unsigned second) {
    if (mask(d, first) < 0 || mask(d, first | second) < 0 ||
        mask(d, first) < 0 || mask(d, 0) < 0) return -1;
    return 0;
}

int m8_demo_boot(m8_demo *d) {
    if (!d || d->error[0]) return -1;
    uint64_t target = m8rt_get(d->rt, M8RT_MAIN_ITERS);
    if (target < 10000) target = 10000;
    while (m8rt_get(d->rt, M8RT_MAIN_ITERS) < target)
        if (slice(d, 256) < 0) return -1;
    if (require(d, "SONG") < 0) return -1;
    if (!d->have_identity || d->model != 0 || d->major != 6 || d->minor != 5 || d->patch != 2)
        return fail(d, "Expected Headless 6.5.2 identity; present=%u, model=%u, version=%u.%u.%u",
                    d->have_identity, d->model, d->major, d->minor, d->patch);
    if (mask(d, 0) < 0) return -1;
    d->deadline = UINT64_MAX;
    return 0;
}
int m8_demo_boot_paused(m8_demo *d) {
    if(!d)return -1;
    d->fixed_slices=1;int result=m8_demo_boot(d);d->fixed_slices=0;return result;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int phrase_row(m8_demo *d, unsigned row, char *out) {
    update_lines(d);
    for (unsigned i = 5; i < d->line_count && i < 21; ++i) {
        char clean[LINE_SIZE];
        unsigned used = 0;
        for (const char *p = d->lines[i]; *p; ++p)
            if (*p != '<' && *p != '>') clean[used++] = *p;
        clean[used] = 0;
        if (hex_value(clean[0]) == (int)row) {
            snprintf(out, LINE_SIZE, "%s", clean + 1);
            return 0;
        }
    }
    return fail(d, "Phrase row %X is not visible", row);
}

int m8_demo_playhead(m8_demo *d) {
    if (!d) return -1;
    update_lines(d);
    if (!contains(d, "PHRASE")) return -1;
    for (unsigned i = 5; i < d->line_count && i < 21; ++i) {
        const char *p = d->lines[i];
        if (*p == '<') ++p;
        int row = hex_value(*p);
        if (row >= 0 && p[1] == '>') return row;
    }
    return -1;
}

static int set_note(m8_demo *d, unsigned row, int wanted, const char *label) {
    static const char names[12][3] = {"C-", "C#", "D-", "D#", "E-", "F-",
                                    "F#", "G-", "G#", "A-", "A#", "B-"};
    char text[LINE_SIZE];
    if (m8_demo_tap(d, M8_DEMO_EDIT) < 0 || phrase_row(d, row, text) < 0) return -1;
    if (strlen(text) < 3) return fail(d, "Row %X has incomplete note %s", row, text);
    int pitch = -1;
    for (int i = 0; i < 12; ++i) if (!strncmp(text, names[i], 2)) pitch = i;
    int octave = hex_value(text[2]);
    if (pitch < 0 || octave < 1) return fail(d, "Row %X has invalid note %s", row, text);
    int value = pitch + (octave - 1) * 12;
    if (mask(d, M8_DEMO_EDIT) < 0) return -1;
    while (value != wanted) {
        int delta = wanted - value, step = abs(delta) >= 12 ? 12 : 1;
        unsigned direction = step == 12 ? (delta > 0 ? M8_DEMO_UP : M8_DEMO_DOWN)
                                               : (delta > 0 ? M8_DEMO_RIGHT : M8_DEMO_LEFT);
        value += delta > 0 ? step : -step;
        if (mask(d, M8_DEMO_EDIT | direction) < 0 || mask(d, M8_DEMO_EDIT) < 0) return -1;
    }
    if (mask(d, 0) < 0 || phrase_row(d, row, text) < 0) return -1;
    if (strncmp(text, label, 3)) return fail(d, "Row %X: expected %s; observed %s", row, label, text);
    return 0;
}

int m8_demo_prepare_simple(m8_demo *d) {
    static const unsigned rows[4] = {0, 4, 8, 12};
    static const int notes[4] = {48, 52, 55, 60};
    static const char *labels[4] = {"C-5", "E-5", "G-5", "C-6"};
    if (!d || d->error[0]) return -1;
    d->deadline = SDL_GetTicksNS() + UINT64_C(120000000000);
    if (require(d, "SONG") < 0 || m8_demo_tap(d, M8_DEMO_EDIT) < 0 ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_RIGHT) < 0 || require(d, "CHAIN00") < 0 ||
        chord(d, M8_DEMO_EDIT, M8_DEMO_RIGHT) < 0 ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_RIGHT) < 0 || require(d, "PHRASE01") < 0 ||
        set_note(d, 0, notes[0], labels[0]) < 0 ||
        m8_demo_tap(d, M8_DEMO_RIGHT) < 0 || m8_demo_tap(d, M8_DEMO_RIGHT) < 0 ||
        m8_demo_tap(d, M8_DEMO_EDIT) < 0 || m8_demo_tap(d, M8_DEMO_EDIT) < 0 ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_RIGHT) < 0 || require(d, "INST.01") < 0 ||
        chord(d, M8_DEMO_EDIT, M8_DEMO_RIGHT) < 0 || require(d, "TYPEWAVSYNTH") < 0 ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_LEFT) < 0 ||
        m8_demo_tap(d, M8_DEMO_LEFT) < 0 || m8_demo_tap(d, M8_DEMO_LEFT) < 0) return -1;
    unsigned current = 0;
    for (unsigned i = 1; i < 4; ++i) {
        for (; current < rows[i]; ++current) if (m8_demo_tap(d, M8_DEMO_DOWN) < 0) return -1;
        if (set_note(d, rows[i], notes[i], labels[i]) < 0 ||
            m8_demo_tap(d, M8_DEMO_RIGHT) < 0 || m8_demo_tap(d, M8_DEMO_RIGHT) < 0 ||
            m8_demo_tap(d, M8_DEMO_EDIT) < 0 ||
            m8_demo_tap(d, M8_DEMO_LEFT) < 0 || m8_demo_tap(d, M8_DEMO_LEFT) < 0) return -1;
    }
    for (; current < 15; ++current) if (m8_demo_tap(d, M8_DEMO_DOWN) < 0) return -1;
    if (chord(d, M8_DEMO_OPTION, M8_DEMO_EDIT) < 0) return -1;
    for (unsigned i = 0; i < 15; ++i) if (m8_demo_tap(d, M8_DEMO_UP) < 0) return -1;
    if (require(d, "PHRASE01") < 0) return -1;
    char text[LINE_SIZE], expected[16];
    for (unsigned i = 0; i < 4; ++i) {
        if (phrase_row(d, rows[i], text) < 0) return -1;
        snprintf(expected, sizeof expected, "%s6401", labels[i]);
        if (strncmp(text, expected, strlen(expected)))
            return fail(d, "Row %X: expected note/volume/instrument %s; observed %s", rows[i], expected, text);
    }
    if (phrase_row(d, 15, text) < 0) return -1;
    if (strncmp(text, "OFF", 3)) return fail(d, "Row F: expected OFF; observed %s", text);
    if (m8_demo_playhead(d) >= 0) return fail(d, "Simple demo is playing during setup");
    if (d->held) return fail(d, "Simple demo left keys held: %02X", d->held);
    d->deadline = UINT64_MAX;
    return 0;
}

static void selected_line(m8_demo *d, char out[SCREEN_WIDTH+1]) {
    int best = -1, count = -1;
    for (unsigned y = 0; y < SCREEN_HEIGHT; ++y) {
        unsigned chars = 0, score = 0;
        for (unsigned x = 0; x < SCREEN_WIDTH; ++x) chars += d->cells[y][x] != 0;
        if (!chars) continue;
        for (unsigned yy = y; yy < SCREEN_HEIGHT && yy < y+7; ++yy)
            for (unsigned x = 0; x < SCREEN_WIDTH; ++x) score += d->selection[yy*SCREEN_WIDTH+x];
        if ((int)score > count) { best = (int)y; count = (int)score; }
    }
    unsigned used = 0;
    if (best >= 0) for (unsigned x = 0; x < SCREEN_WIDTH; ++x)
        if (d->cells[best][x] && d->cells[best][x] != ' ') out[used++] = d->cells[best][x];
    out[used] = 0;
}

int m8_demo_load_project(m8_demo *d, const char *name) {
    if (!d || !name || d->error[0]) return -1;
    size_t length = strlen(name);
    if (!length || length > 8) return fail(d, "Project name must have 1..8 characters");
    for (size_t i = 0; i < length; ++i)
        if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '_'))
            return fail(d, "Project name must use uppercase letters, digits or underscore");
    d->selection = calloc(SCREEN_HEIGHT*SCREEN_WIDTH, 1);
    if (!d->selection) return fail(d, "Cannot allocate Project selection observer");
    d->deadline = SDL_GetTicksNS() + UINT64_C(120000000000);
    int result = -1;
    char line[SCREEN_WIDTH+1], title[32];
    if (require(d, "SONG") || mask(d, 0) || chord(d, M8_DEMO_SHIFT, M8_DEMO_UP) || require(d, "PROJECT")) goto finish;
    selected_line(d, line);
    unsigned direction = strstr(line, "TEMPO") ? M8_DEMO_DOWN : M8_DEMO_UP;
    unsigned i;
    for (i = 0; i < 24; ++i) {
        selected_line(d, line);
        if (strstr(line, "PROJECTLOADSAVENEW")) break;
        if (m8_demo_tap(d, direction)) goto finish;
    }
    if (i == 24) { fail(d, "Project LOAD row not selected: observed=%s", line); goto finish; }
    if (m8_demo_tap(d, M8_DEMO_EDIT)) goto finish;
    for (i = 0; i < 32; ++i) {
        selected_line(d, line);
        const char *match = strstr(line, name);
        if (match && (match[length] == '.' || match[length] == 0)) {
            if (m8_demo_tap(d, M8_DEMO_EDIT)) goto finish;
            break;
        }
        if (contains(d, name)) {
            if (m8_demo_tap(d, M8_DEMO_DOWN)) goto finish;
        } else if (!strcmp(line, "/..") || !strcmp(line, "..") || strstr(line, "SONGS") ||
                   contains(d, "YESNO") || contains(d, "OKCANCEL")) {
            if (m8_demo_tap(d, M8_DEMO_EDIT)) goto finish;
        } else if (m8_demo_tap(d, M8_DEMO_DOWN)) goto finish;
    }
    if (i == 32) { fail(d, "Project file not selected: name=%s observed=%s", name, line); goto finish; }
    for (i = 0; i < 10 && !contains(d, "SONGLOADED"); ++i) {
        if (contains(d, "YESNO") || contains(d, "OKCANCEL")) {
            if (m8_demo_tap(d, M8_DEMO_EDIT)) goto finish;
        } else if (m8_demo_advance(d, .5)) goto finish;
    }
    snprintf(title, sizeof title, "NAME%s", name);
    if (require(d, "SONGLOADED") || require(d, title) ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_DOWN) || require(d, "SONG") ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_RIGHT) || require(d, "CHAIN") ||
        chord(d, M8_DEMO_SHIFT, M8_DEMO_RIGHT) || require(d, "PHRASE")) goto finish;
    if (m8_demo_playhead(d) >= 0 || d->held) {
        fail(d, "Loaded project must be stopped with released keys: playhead=%d held=%u", m8_demo_playhead(d), d->held);
        goto finish;
    }
    printf("project=%s loaded=1 stopped=1 held=0 view=PHRASE\n", name);
    result = 0;
finish:
    free(d->selection); d->selection = NULL;
    d->deadline = UINT64_MAX;
    return result;
}

int m8_demo_transport(m8_demo *d, int playing) {
    if (!d || d->error[0]) return -1;
    if (require(d, "PHRASE") < 0) return -1;
    if ((m8_demo_playhead(d) >= 0) != (playing != 0))
        if (m8_demo_tap(d, M8_DEMO_PLAY) < 0) return -1;
    int observed = m8_demo_playhead(d);
    if ((observed >= 0) != (playing != 0))
        return fail(d, "Phrase transport expected %s; observed playhead=%d, held=%02X",
                    playing ? "PLAY" : "STOP", observed, d->held);
    return d->held ? fail(d, "Transport left keys held: %02X", d->held) : 0;
}

int m8_demo_cursor(const m8_demo *d, unsigned *x, unsigned *y) {
    if (!d || !d->have_cursor) return 0;
    if (x) *x = d->cursor_x;
    if (y) *y = d->cursor_y;
    return 1;
}

unsigned m8_demo_held_keys(const m8_demo *d) { return d ? d->held : 0; }

int m8_demo_enable_selection(m8_demo *d) {
    if (!d || d->error[0]) return -1;
    if (!d->selection) d->selection = calloc(SCREEN_HEIGHT*SCREEN_WIDTH, 1);
    return d->selection ? 0 : fail(d, "Cannot allocate display selection observer");
}
int m8_demo_selected(m8_demo *d, char *out, size_t capacity) {
    if (!d || !out || !capacity || !d->selection) return 0;
    char line[SCREEN_WIDTH+1];
    selected_line(d, line);
    snprintf(out, capacity, "%s", line);
    return *line != 0;
}
int m8_demo_identity(const m8_demo *d, unsigned *model,
                     unsigned *major, unsigned *minor, unsigned *patch, unsigned *font) {
    if (!d || !d->have_identity) return 0;
    if (model) *model = d->model;
    if (major) *major = d->major;
    if (minor) *minor = d->minor;
    if (patch) *patch = d->patch;
    if (font) *font = d->font;
    return 1;
}

#include "session-codec.h"
void m8_demo_session(m8_demo *d,m8_session_io *c) {
    S32(d->held);S32(d->actions);
    S32(d->model);S32(d->major);S32(d->minor);S32(d->patch);S32(d->font);S32(d->have_identity);
    S32(d->cursor_x);S32(d->cursor_y);S32(d->have_cursor);S32(d->text_offset);
    session_bytes(c,d->last_color,3);session_bytes(c,d->cells,sizeof d->cells);
    session_bytes(c,d->text_rows,sizeof d->text_rows);
    uint32_t selection=session_number(c,d->selection!=NULL,1);
    if(selection>1){c->result=M8RT_SESSION_INVALID;return;}
    if(c->pass==2 && selection && !d->selection) {
        d->selection=malloc(SCREEN_WIDTH*SCREEN_HEIGHT);
        if(!d->selection){c->result=M8RT_SESSION_NOMEM;return;}
    }
    if(selection)session_bytes(c,d->selection,SCREEN_WIDTH*SCREEN_HEIGHT);
    if(c->pass==2&&!selection){free(d->selection);d->selection=NULL;}
    SSZ(d->packet_length);S32(d->escaped);session_bytes(c,d->packet,sizeof d->packet);
    if(c->pass==2) {
        if(d->packet_length>sizeof d->packet||d->escaped>1||d->font>4)c->result=M8RT_SESSION_INVALID;
        d->lines_dirty=1;d->deadline=UINT64_MAX;
    }
}
