#include "gamepad_view.h"
#include "handheld.h"
#include "common.h"
#include "input.h"
#include "ini.h"
#include "render.h"
#include "SDL2_inprint.h"
#include "fonts/fonts.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* M8 default theme. Text sits on the firmware's 8x10 cell grid. */
enum { BACKGROUND = 0x000000, EMPTY = 0x202830, INFO = 0x485060, TEXT = 0x90b8b8,
       VALUE = 0xf8fcf8, TITLE = 0xf82040, SELECTED = 0x00fcf8, CURSOR = 0x00dcd8, PLAY = 0x00fc60 };
enum { COLUMNS = 40, CELL_W = 8, CELL_H = 10, TOP = 30, MESSAGE_ROW = 18, HINT_ROW = 22 };
enum { PAGE_PADS, PAGE_MAP, PAGE_SETUP, PAGE_COUNT };
enum { CONTROLS = 8, MAP_LEARN = CONTROLS, MAP_DEFAULTS, MAP_ROWS };
enum { SET_STICK, SET_LTRIGGER, SET_RTRIGGER, SET_THRESHOLD, SET_EXIT, SET_BACKGROUND, SET_DEFAULTS, SET_ROWS };
static const int setup_rows[SET_ROWS] = {5, 6, 7, 8, 10, 11, 13};

static const struct { const char *name; size_t button, axis; int key; } controls[CONTROLS] = {
    {"UP", offsetof(config_params_s, gamepad_up), offsetof(config_params_s, gamepad_analog_axis_updown), key_up},
    {"DOWN", offsetof(config_params_s, gamepad_down), offsetof(config_params_s, gamepad_analog_axis_updown), key_down},
    {"LEFT", offsetof(config_params_s, gamepad_left), offsetof(config_params_s, gamepad_analog_axis_leftright), key_left},
    {"RIGHT", offsetof(config_params_s, gamepad_right), offsetof(config_params_s, gamepad_analog_axis_leftright), key_right},
    {"SHIFT", offsetof(config_params_s, gamepad_select), offsetof(config_params_s, gamepad_analog_axis_select), key_select},
    {"PLAY", offsetof(config_params_s, gamepad_start), offsetof(config_params_s, gamepad_analog_axis_start), key_start},
    {"OPTION", offsetof(config_params_s, gamepad_opt), offsetof(config_params_s, gamepad_analog_axis_opt), key_opt},
    {"EDIT", offsetof(config_params_s, gamepad_edit), offsetof(config_params_s, gamepad_analog_axis_edit), key_edit},
};

static struct {
    struct app_context *ctx;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    bool open, conf_dirty, learn, edit, shift, option, option_used;
    int page, row[PAGE_COUNT], capture;
    char message[COLUMNS + 1];
    uint64_t message_until;
} view = {.capture = -1};

#define FIELD(conf, offset) (*(int *)((char *)(conf) + (offset)))

/* ---- printed names ---- */

/* SDL 3.4 added the GameCube type; the pinned 3.2 source lacks it. */
#if SDL_VERSION_ATLEAST(3, 3, 0)
#define M8_GAMECUBE(type) ((type) == SDL_GAMEPAD_TYPE_GAMECUBE)
#else
#define M8_GAMECUBE(type) 0
#endif

typedef enum { GENERIC, XBOX, PLAYSTATION, NINTENDO } family;
static family pad_family(SDL_Gamepad *pad) {
    switch (pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN) {
    case SDL_GAMEPAD_TYPE_XBOX360: case SDL_GAMEPAD_TYPE_XBOXONE: return XBOX;
    case SDL_GAMEPAD_TYPE_PS3: case SDL_GAMEPAD_TYPE_PS4: case SDL_GAMEPAD_TYPE_PS5: return PLAYSTATION;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return NINTENDO;
    default: return M8_GAMECUBE(SDL_GetGamepadType(pad)) ? NINTENDO : GENERIC;
    }
}

const char *m8_gamepad_label(SDL_Gamepad *pad, int button) {
    static const char *labels[] = {"---", "A", "B", "X", "Y", "CROSS", "CIRCLE", "SQUARE", "TRIANGLE"};
    static const char *pairs[][4] = {{"L1", "R1", "L3", "R3"}, {"LB", "RB", "LS", "RS"},
                                     {"L1", "R1", "L3", "R3"}, {"L", "R", "LS", "RS"}};
    SDL_GamepadType type = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
    family f = pad_family(pad);
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST: case SDL_GAMEPAD_BUTTON_NORTH: {
        SDL_GamepadButtonLabel label = pad ? SDL_GetGamepadButtonLabel(pad, (SDL_GamepadButton)button) :
                                             SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
        if (label > SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN && label <= SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE) return labels[label];
        return button == SDL_GAMEPAD_BUTTON_SOUTH ? "A" : button == SDL_GAMEPAD_BUTTON_EAST ? "B" :
               button == SDL_GAMEPAD_BUTTON_WEST ? "X" : "Y";
    }
    case SDL_GAMEPAD_BUTTON_BACK:
        return type == SDL_GAMEPAD_TYPE_XBOXONE ? "VIEW" : type == SDL_GAMEPAD_TYPE_PS4 ? "SHARE" :
               type == SDL_GAMEPAD_TYPE_PS5 ? "CREATE" : f == NINTENDO ? "MINUS" : f == XBOX ? "BACK" : "SELECT";
    case SDL_GAMEPAD_BUTTON_START:
        return type == SDL_GAMEPAD_TYPE_XBOXONE ? "MENU" : type == SDL_GAMEPAD_TYPE_PS4 ||
               type == SDL_GAMEPAD_TYPE_PS5 ? "OPTIONS" : f == NINTENDO && !M8_GAMECUBE(type) ? "PLUS" : "START";
    case SDL_GAMEPAD_BUTTON_GUIDE: return f == XBOX ? "XBOX" : f == PLAYSTATION ? "PS" : f == NINTENDO ? "HOME" : "GUIDE";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return pairs[f][0];
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return pairs[f][1];
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return pairs[f][2];
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return pairs[f][3];
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-PAD UP";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-PAD DOWN";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-PAD LEFT";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-PAD RIGHT";
    case SDL_GAMEPAD_BUTTON_MISC1:
        return f == XBOX ? "SHARE" : type == SDL_GAMEPAD_TYPE_PS5 ? "MUTE" : f == NINTENDO ? "CAPTURE" : "MISC 1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "P1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "P2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "P3";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "P4";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "TOUCHPAD";
    case SDL_GAMEPAD_BUTTON_MISC2: return "MISC 2";
    case SDL_GAMEPAD_BUTTON_MISC3: return "MISC 3";
    case SDL_GAMEPAD_BUTTON_MISC4: return "MISC 4";
    case SDL_GAMEPAD_BUTTON_MISC5: return "MISC 5";
    case SDL_GAMEPAD_BUTTON_MISC6: return "MISC 6";
    default: return "---";
    }
}

const char *m8_gamepad_axis_label(SDL_Gamepad *pad, int axis) {
    static const char *triggers[][2] = {{"L2", "R2"}, {"LT", "RT"}, {"L2", "R2"}, {"ZL", "ZR"}};
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX: case SDL_GAMEPAD_AXIS_LEFTY: return "L STICK";
    case SDL_GAMEPAD_AXIS_RIGHTX: case SDL_GAMEPAD_AXIS_RIGHTY: return "R STICK";
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return triggers[pad_family(pad)][0];
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return triggers[pad_family(pad)][1];
    default: return "---";
    }
}

const char *m8_gamepad_button_name(SDL_GamepadButton button) { return m8_gamepad_label(m8_handheld_label_pad(), button); }
const char *m8_gamepad_axis_name(SDL_GamepadAxis axis) { return m8_gamepad_axis_label(m8_handheld_label_pad(), axis); }

static const char *type_name(SDL_Gamepad *pad) {
    switch (SDL_GetGamepadType(pad)) {
    case SDL_GAMEPAD_TYPE_STANDARD: return "STD";
    case SDL_GAMEPAD_TYPE_XBOX360: return "X360";
    case SDL_GAMEPAD_TYPE_XBOXONE: return "XBOX";
    case SDL_GAMEPAD_TYPE_PS3: return "PS3";
    case SDL_GAMEPAD_TYPE_PS4: return "PS4";
    case SDL_GAMEPAD_TYPE_PS5: return "PS5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return "NSPRO";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "JOY-L";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "JOY-R";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "JOYLR";
    default: return M8_GAMECUBE(SDL_GetGamepadType(pad)) ? "GC" : "---";
    }
}

/* ---- model ---- */

static config_params_s *conf(void) { return &view.ctx->conf; }
static int count_pads(SDL_Gamepad **pads) {
    int n = 0;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) if (game_controllers[slot]) pads[n++] = game_controllers[slot];
    return n;
}
static int rows(int page) {
    SDL_Gamepad *pads[M8_MAX_GAMEPADS];
    return page == PAGE_PADS ? 1 + count_pads(pads) : page == PAGE_MAP ? MAP_ROWS : SET_ROWS;
}
static void say(const char *text) {
    SDL_strlcpy(view.message, text, sizeof view.message);
    view.message_until = SDL_GetTicksNS() + UINT64_C(2500000000);
}

static int stick_value(void) {
    int y = conf()->gamepad_analog_axis_updown, x = conf()->gamepad_analog_axis_leftright;
    return x < 0 && y < 0 ? 0 : x == SDL_GAMEPAD_AXIS_LEFTX && y == SDL_GAMEPAD_AXIS_LEFTY ? 1 :
           x == SDL_GAMEPAD_AXIS_RIGHTX && y == SDL_GAMEPAD_AXIS_RIGHTY ? 2 : -1;
}
static void set_stick(int value) {
    conf()->gamepad_analog_axis_leftright = value == 1 ? SDL_GAMEPAD_AXIS_LEFTX : value == 2 ? SDL_GAMEPAD_AXIS_RIGHTX : -1;
    conf()->gamepad_analog_axis_updown = value == 1 ? SDL_GAMEPAD_AXIS_LEFTY : value == 2 ? SDL_GAMEPAD_AXIS_RIGHTY : -1;
}
/* 0 = off, else 1 + the action control (SHIFT..EDIT) a trigger presses. */
static int trigger_value(int axis) {
    for (int i = 4; i < CONTROLS; ++i) if (FIELD(conf(), controls[i].axis) == axis) return i - 3;
    return 0;
}
static void set_trigger(int axis, int value) {
    for (int i = 4; i < CONTROLS; ++i) if (FIELD(conf(), controls[i].axis) == axis) FIELD(conf(), controls[i].axis) = -1;
    if (value > 0) FIELD(conf(), controls[value + 3].axis) = axis;
}

static void defaults(bool buttons) {
    config_params_s base = config_initialize(NULL);
    m8_handheld_config_defaults(&base);
    for (int i = 0; i < CONTROLS; ++i) {
        if (buttons) FIELD(conf(), controls[i].button) = FIELD(&base, controls[i].button);
        else FIELD(conf(), controls[i].axis) = FIELD(&base, controls[i].axis);
    }
    if (!buttons) {
        conf()->gamepad_analog_threshold = base.gamepad_analog_threshold;
        conf()->gamepad_analog_invert = base.gamepad_analog_invert;
        m8_handheld_options()->exit_combo = true;
        m8_handheld_options()->background = false;
        m8_handheld_options_changed();
    }
    view.conf_dirty = true;
}

/* Write only gamepad keys. The running config also holds command-line and
 * emulator overrides, such as forced audio, that must not reach m8c. */
static void save_conf(void) {
    if (!view.conf_dirty || !view.ctx) return;
    config_params_s disk = config_initialize(view.ctx->conf.filename);
    m8_handheld_config_defaults(&disk);
    char *preferences = SDL_GetPrefPath("", "m8c"), *path = NULL;
    ini_t *ini = preferences && SDL_asprintf(&path, "%s%s", preferences, disk.filename) >= 0 ? ini_load(path) : NULL;
    SDL_free(preferences); SDL_free(path);
    if (ini) {
        read_audio_config(ini, &disk); read_graphics_config(ini, &disk);
        read_key_config(ini, &disk); read_gamepad_config(ini, &disk);
        ini_free(ini);
    }
    const config_params_s *c = &view.ctx->conf;
#define COPY(field) disk.field = c->field
    COPY(gamepad_up); COPY(gamepad_left); COPY(gamepad_down); COPY(gamepad_right); COPY(gamepad_select);
    COPY(gamepad_start); COPY(gamepad_opt); COPY(gamepad_edit); COPY(gamepad_analog_threshold);
    COPY(gamepad_analog_invert); COPY(gamepad_analog_axis_updown); COPY(gamepad_analog_axis_leftright);
    COPY(gamepad_analog_axis_select); COPY(gamepad_analog_axis_start); COPY(gamepad_analog_axis_opt);
    COPY(gamepad_analog_axis_edit);
#undef COPY
    write_config(&disk);
    SDL_free(disk.audio_device_name);
    view.conf_dirty = false;
}

static void set_active(SDL_Gamepad *pad) {
    m8_pad_options *o = m8_handheld_options();
    if (!pad || !m8_handheld_guid(SDL_GetGamepadID(pad), o->active, sizeof o->active)) o->active[0] = 0;
    if (pad) SDL_RumbleGamepad(pad, 0x5000, 0x5000, 150);
    m8_handheld_options_changed();
}

static void start_capture(int control, bool learn) {
    view.capture = control; view.learn = learn; view.message_until = 0;
}

static void capture(int button) {
    m8_pad_options *o = m8_handheld_options();
    SDL_Gamepad *pad = m8_handheld_label_pad();
    char msg[COLUMNS + 1];
    int i = view.capture;
    if (button == SDL_GAMEPAD_BUTTON_START && o->exit_combo) {
        snprintf(msg, sizeof msg, "%s IS IN THE EXIT COMBO", m8_gamepad_label(pad, button));
        say(msg); return;
    }
    for (int j = 0; j < CONTROLS; ++j) {
        if (j == i || FIELD(conf(), controls[j].button) != button) continue;
        if (view.learn && j < i) {
            snprintf(msg, sizeof msg, "%s IS ALREADY %s", m8_gamepad_label(pad, button), controls[j].name);
            say(msg); return;
        }
        /* Swap, so every control keeps a button. While learning, a later
         * control is asked for again anyway. */
        FIELD(conf(), controls[j].button) = view.learn ? -1 : FIELD(conf(), controls[i].button);
        if (!view.learn) { snprintf(msg, sizeof msg, "SWAPPED WITH %s", controls[j].name); say(msg); }
    }
    FIELD(conf(), controls[i].button) = button;
    view.conf_dirty = true;
    view.row[PAGE_MAP] = i;
    if (view.learn && i + 1 < CONTROLS) { view.capture = i + 1; view.row[PAGE_MAP] = i + 1; }
    else {
        if (view.learn) say("ALL CONTROLS LEARNED");
        view.capture = -1; view.learn = false;
    }
}

static void clear_row(void) {
    int row = view.row[view.page];
    if (view.page == PAGE_PADS) set_active(NULL);
    else if (view.page == PAGE_MAP && row < CONTROLS) { FIELD(conf(), controls[row].button) = -1; view.conf_dirty = true; }
    else if (view.page == PAGE_SETUP && row == SET_STICK) { set_stick(0); view.conf_dirty = true; }
    else if (view.page == PAGE_SETUP && (row == SET_LTRIGGER || row == SET_RTRIGGER)) {
        set_trigger(row == SET_LTRIGGER ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0);
        view.conf_dirty = true;
    }
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* LEFT/RIGHT step by one, UP/DOWN by a coarse step, like M8 value edits. */
static void adjust(int fine, int coarse) {
    int row = view.row[view.page], d = fine ? fine : coarse;
    m8_pad_options *o = m8_handheld_options();
    if (view.page == PAGE_PADS && row == 0) {
        SDL_Gamepad *pads[M8_MAX_GAMEPADS], *active = m8_handheld_active();
        int n = count_pads(pads), index = 0;
        for (int i = 0; i < n; ++i) if (pads[i] == active) index = i + 1;
        int next = clamp(index + d, 0, n);
        if (next != index) set_active(next ? pads[next - 1] : NULL);
        return;
    }
    if (view.page != PAGE_SETUP) return;
    switch (row) {
    case SET_STICK: set_stick(clamp(stick_value() < 0 ? 1 : stick_value() + d, 0, 2)); break;
    case SET_LTRIGGER: case SET_RTRIGGER: {
        int axis = row == SET_LTRIGGER ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        set_trigger(axis, clamp(trigger_value(axis) + d, 0, 4));
        break;
    }
    case SET_THRESHOLD: {
        int step = fine ? fine : coarse * 16;
        conf()->gamepad_analog_threshold = clamp(conf()->gamepad_analog_threshold / 128 + step, 8, 255) * 128;
        break;
    }
    case SET_EXIT: o->exit_combo = d > 0; m8_handheld_options_changed(); return;
    case SET_BACKGROUND: o->background = d > 0; m8_handheld_options_changed(); return;
    default: return;
    }
    view.conf_dirty = true;
}

static void activate(void) {
    int row = view.row[view.page];
    if (view.page == PAGE_PADS && row > 0) {
        SDL_Gamepad *pads[M8_MAX_GAMEPADS];
        int n = count_pads(pads);
        if (row <= n) set_active(pads[row - 1] == m8_handheld_active() ? NULL : pads[row - 1]);
    } else if (view.page == PAGE_MAP) {
        if (row < CONTROLS) start_capture(row, false);
        else if (row == MAP_LEARN) { view.row[PAGE_MAP] = 0; start_capture(0, true); }
        else { defaults(true); say("BUTTONS RESET"); }
    } else if (view.page == PAGE_SETUP && row == SET_DEFAULTS) { defaults(false); say("SETTINGS RESET"); }
}

static void move(int dx, int dy) {
    if (view.shift && dx) {
        view.page = clamp(view.page + dx, 0, PAGE_COUNT - 1);
        view.message_until = 0;
        return;
    }
    if (view.edit || (dx && !dy)) { adjust(dx, -dy); return; }
    view.row[view.page] = clamp(view.row[view.page] + dy, 0, rows(view.page) - 1);
}

/* ---- input ---- */

enum { A_NONE, A_UP, A_DOWN, A_LEFT, A_RIGHT, A_EDIT, A_OPTION, A_SHIFT, A_CLEAR, A_CLOSE };

static int key_action(const config_params_s *c, SDL_Scancode s, SDL_Keycode k) {
    if (k == SDLK_ESCAPE) return A_CLOSE;
    if (k == SDLK_DELETE || k == SDLK_BACKSPACE) return A_CLEAR;
    unsigned u = (unsigned)s;
    if (k == SDLK_UP || u == c->key_up) return A_UP;
    if (k == SDLK_DOWN || u == c->key_down) return A_DOWN;
    if (k == SDLK_LEFT || u == c->key_left) return A_LEFT;
    if (k == SDLK_RIGHT || u == c->key_right) return A_RIGHT;
    if (k == SDLK_RETURN || u == c->key_edit || u == c->key_edit_alt) return A_EDIT;
    if (u == c->key_opt || u == c->key_opt_alt) return A_OPTION;
    if (u == c->key_select || u == c->key_select_alt || k == SDLK_RSHIFT) return A_SHIFT;
    return A_NONE;
}

/* The D-pad, and SOUTH/EAST while EDIT/OPTION are unbound, keep the view usable. */
static int button_action(const config_params_s *c, int b) {
    if (b == c->gamepad_edit || (c->gamepad_edit < 0 && b == SDL_GAMEPAD_BUTTON_SOUTH)) return A_EDIT;
    if (b == c->gamepad_opt || (c->gamepad_opt < 0 && b == SDL_GAMEPAD_BUTTON_EAST)) return A_OPTION;
    if (b == c->gamepad_select) return A_SHIFT;
    if (b == c->gamepad_up || b == SDL_GAMEPAD_BUTTON_DPAD_UP) return A_UP;
    if (b == c->gamepad_down || b == SDL_GAMEPAD_BUTTON_DPAD_DOWN) return A_DOWN;
    if (b == c->gamepad_left || b == SDL_GAMEPAD_BUTTON_DPAD_LEFT) return A_LEFT;
    if (b == c->gamepad_right || b == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) return A_RIGHT;
    return A_NONE;
}

static void act(int action, bool down) {
    if (action == A_EDIT) view.edit = down;
    if (action == A_SHIFT) view.shift = down;
    if (action == A_OPTION) {
        if (down) { view.option = true; view.option_used = false; return; }
        if (!view.option) return;
        view.option = false;
        if (!view.option_used) m8_gamepad_view_close(view.ctx);
        return;
    }
    if (!down) return;
    switch (action) {
    case A_UP: move(0, -1); break;
    case A_DOWN: move(0, 1); break;
    case A_LEFT: move(-1, 0); break;
    case A_RIGHT: move(1, 0); break;
    case A_EDIT: if (view.option) { view.option_used = true; clear_row(); } else activate(); break;
    case A_CLEAR: clear_row(); break;
    case A_CLOSE: m8_gamepad_view_close(view.ctx); break;
    default: break;
    }
}

bool m8_gamepad_view_event(struct app_context *ctx, const SDL_Event *e) {
    if (!view.open) return false;
    view.ctx = ctx;
    renderer_request_redraw();
    if (e->type == SDL_EVENT_KEY_DOWN || e->type == SDL_EVENT_KEY_UP) {
        if (e->key.repeat) return true;
        int action = key_action(&ctx->conf, e->key.scancode, e->key.key);
        if (view.capture >= 0) {
            if (!e->key.down && action == A_EDIT) view.edit = false;
            if (!e->key.down && action == A_SHIFT) view.shift = false;
            if (e->key.down && action == A_CLOSE) { view.capture = -1; view.learn = false; }
            else if (e->key.down && action == A_CLEAR) {
                FIELD(conf(), controls[view.capture].button) = -1;
                view.conf_dirty = true; view.capture = -1; view.learn = false;
            }
            return true;
        }
        act(action, e->key.down);
    } else if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        int action = button_action(&ctx->conf, e->gbutton.button);
        if (view.capture >= 0) {
            if (e->gbutton.down) capture(e->gbutton.button);
            /* Releases still end the EDIT and SHIFT holds that began before. */
            else if (action == A_EDIT) view.edit = false;
            else if (action == A_SHIFT) view.shift = false;
            return true;
        }
        act(action, e->gbutton.down);
    }
    return true;
}

bool m8_gamepad_view_is_open(void) { return view.open; }

void m8_gamepad_view_refresh(void) {
    if (!view.open) return;
    view.row[PAGE_PADS] = clamp(view.row[PAGE_PADS], 0, rows(PAGE_PADS) - 1);
    renderer_request_redraw();
}

void m8_gamepad_view_toggle(struct app_context *ctx) {
    if (view.open && view.capture >= 0) { view.capture = -1; view.learn = false; renderer_request_redraw(); return; }
    if (view.open) { m8_gamepad_view_close(ctx); return; }
    view.ctx = ctx;
    view.open = true;
    view.capture = -1; view.learn = view.edit = view.shift = view.option = false;
    view.message_until = 0;
    m8_handheld_release(ctx);
    m8_gamepad_view_refresh();
}

void m8_gamepad_view_close(struct app_context *ctx) {
    if (!view.open) return;
    if (ctx) view.ctx = ctx;
    view.open = false;
    view.capture = -1; view.learn = false;
    save_conf();
    if (ctx) m8_handheld_release(ctx);
    renderer_request_redraw();
}

/* ---- drawing ---- */

static int text_y;
static void text(int col, int row, const char *s, Uint32 color) {
    char glyph[2] = {0};
    incolor(color);
    for (; *s && col < COLUMNS; ++s, ++col) {
        /* The font covers printable ASCII; m8c prints letters in capitals. */
        glyph[0] = *s >= 'a' && *s <= 'z' ? (char)(*s - 32) : *s > ' ' && *s < 127 ? *s : ' ';
        if (glyph[0] != ' ') inprint(view.renderer, glyph, (Uint32)(col * CELL_W), (Uint32)(row * CELL_H + text_y), color, color);
    }
}
static void fill(int x, int y, int w, int h, Uint32 color) {
    SDL_FRect r = {(float)x, (float)y, (float)w, (float)h};
    SDL_SetRenderDrawColor(view.renderer, (Uint8)(color >> 16), (Uint8)(color >> 8), (Uint8)color, 0xff);
    SDL_RenderFillRect(view.renderer, &r);
}
/* The firmware cursor: four stepped corners around a field of width cells. */
static void cursor(int col, int row, int width) {
    int x0 = col * CELL_W - 4, x1 = (col + (width < 1 ? 1 : width)) * CELL_W, y0 = row * CELL_H, y1 = y0 + 12;
    for (int i = 0; i < 3; ++i) {
        fill(x0, y0 + i, 3 - i, 1, CURSOR); fill(x1 - 2 + i, y0 + i, 3 - i, 1, CURSOR);
        fill(x0, y1 - i, 3 - i, 1, CURSOR); fill(x1 - 2 + i, y1 - i, 3 - i, 1, CURSOR);
    }
}
/* The firmware play marker, a 3x5 triangle, flags live input on a row. */
static void marker(int row) {
    for (int i = 0; i < 3; ++i) fill(1 + i, row * CELL_H + 4 + i, 1, 5 - 2 * i, PLAY);
}
static void field(int col, int row, const char *value, bool selected, Uint32 color) {
    text(col, row, value, selected ? SELECTED : color);
    if (selected) cursor(col, row, (int)strlen(value));
}

static bool accepted(SDL_Gamepad *pad) {
    SDL_Gamepad *active = m8_handheld_active();
    return pad && (!active || active == pad);
}
static int axis_level(int axis) {
    int level = 0;
    if (axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) return 0;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (!accepted(game_controllers[slot])) continue;
        int v = SDL_abs(SDL_GetGamepadAxis(game_controllers[slot], (SDL_GamepadAxis)axis));
        if (v > level) level = v;
    }
    return level;
}
static bool held(int control) {
    const config_params_s *c = conf();
    int button = FIELD(c, controls[control].button), axis = FIELD(c, controls[control].axis);
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        SDL_Gamepad *pad = game_controllers[slot];
        if (!accepted(pad)) continue;
        if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT && SDL_GetGamepadButton(pad, (SDL_GamepadButton)button)) return true;
        if (axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT) {
            int v = SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)axis);
            if (c->gamepad_analog_invert) v = -v;
            if ((control == 0 || control == 2) ? v < -c->gamepad_analog_threshold : v > c->gamepad_analog_threshold) return true;
        }
    }
    return false;
}

static void name_of(SDL_Gamepad *pad, char *out, int capacity) {
    const char *name = SDL_GetGamepadName(pad);
    int n = 0;
    for (; name && *name && n < capacity - 1; ++name)
        if ((unsigned char)*name < 128) out[n++] = *name;
    out[n] = 0;
}

static void draw_pads(void) {
    SDL_Gamepad *pads[M8_MAX_GAMEPADS], *active = m8_handheld_active();
    int n = count_pads(pads), row = view.row[PAGE_PADS];
    char line[COLUMNS + 1];
    text(1, 3, "GAMEPADS", TITLE);
    text(1, 5, "ACTIVE", TEXT);
    const char *value = "ANY";
    char name[17];
    if (active) { name_of(active, name, sizeof name); value = name; }
    else if (m8_handheld_options()->active[0]) value = "SAVED PAD AWAY";
    field(10, 5, value, row == 0, active || !m8_handheld_options()->active[0] ? VALUE : INFO);
    if (!n) {
        text(3, 8, "NO GAMEPAD CONNECTED", VALUE);
#ifdef __APPLE__
        text(3, 9, "PAIR ONE IN BLUETOOTH SETTINGS", INFO);
        text(3, 10, "OR CONNECT IT WITH USB", INFO);
#else
        text(3, 9, "PAIR OR PLUG IN A CONTROLLER", INFO);
#endif
        return;
    }
    text(3, 7, "NAME", INFO); text(20, 7, "TYPE", INFO); text(26, 7, "LINK", INFO); text(31, 7, "POWER", INFO);
    uint64_t now = SDL_GetTicksNS();
    for (int i = 0; i < n; ++i) {
        int r = 8 + i, percent = -1;
        SDL_Gamepad *pad = pads[i];
        uint64_t seen = m8_handheld_activity(SDL_GetGamepadID(pad));
        if (seen && now - seen < UINT64_C(200000000)) marker(r);
        snprintf(line, sizeof line, "%d", i + 1);
        text(1, r, line, pad == active ? SELECTED : INFO);
        name_of(pad, name, sizeof name);
        field(3, r, name, row == i + 1, !active || pad == active ? VALUE : TEXT);
        text(20, r, type_name(pad), TEXT);
        SDL_JoystickConnectionState link = SDL_GetGamepadConnectionState(pad);
        text(26, r, link == SDL_JOYSTICK_CONNECTION_WIRED ? "USB" : link == SDL_JOYSTICK_CONNECTION_WIRELESS ? "BT" : "---",
             link == SDL_JOYSTICK_CONNECTION_WIRED || link == SDL_JOYSTICK_CONNECTION_WIRELESS ? TEXT : INFO);
        SDL_PowerState power = SDL_GetGamepadPowerInfo(pad, &percent);
        Uint32 color = TEXT;
        if (power == SDL_POWERSTATE_ON_BATTERY && percent >= 0) {
            snprintf(line, sizeof line, "%d%%", percent);
            if (percent <= 20) color = TITLE;
        } else {
            snprintf(line, sizeof line, "%s", power == SDL_POWERSTATE_CHARGING ? "CHRG" : power == SDL_POWERSTATE_CHARGED ? "FULL" :
                     power == SDL_POWERSTATE_NO_BATTERY ? "WIRE" : power == SDL_POWERSTATE_ON_BATTERY ? "BATT" : "---");
            if (power == SDL_POWERSTATE_UNKNOWN || power == SDL_POWERSTATE_ERROR) color = INFO;
        }
        text(31, r, line, color);
    }
}

static void draw_map(SDL_Gamepad *label) {
    const config_params_s *c = conf();
    int row = view.row[PAGE_MAP];
    text(1, 3, "GAMEPAD MAPPING", TITLE);
    text(1, 5, "M8", INFO);
    text(10, 5, "BUTTON", row < CONTROLS ? SELECTED : INFO);
    text(25, 5, "ANALOG", INFO);
    for (int i = 0; i < CONTROLS; ++i) {
        int r = 6 + i, axis = FIELD(c, controls[i].axis);
        if (held(i)) marker(r);
        text(1, r, controls[i].name, TEXT);
        if (view.capture == i) field(10, r, "...", true, SELECTED);
        else {
            int button = FIELD(c, controls[i].button);
            field(10, r, m8_gamepad_label(label, button), row == i, button < 0 ? INFO : VALUE);
        }
        if (axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT) text(25, r, m8_gamepad_axis_label(label, axis), INFO);
    }
    field(1, 15, "LEARN ALL", row == MAP_LEARN && view.capture < 0, VALUE);
    field(1, 16, "DEFAULTS", row == MAP_DEFAULTS, VALUE);
}

static void draw_setup(SDL_Gamepad *label) {
    static const char *sticks[] = {"OFF", "LEFT", "RIGHT"}, *targets[] = {"OFF", "SHIFT", "PLAY", "OPTION", "EDIT"};
    const config_params_s *c = conf();
    m8_pad_options *o = m8_handheld_options();
    int row = view.row[PAGE_SETUP], stick = stick_value();
    char line[COLUMNS + 1];
    text(1, 3, "GAMEPAD SETTINGS", TITLE);
    text(1, setup_rows[SET_STICK], "STICK", TEXT);
    field(13, setup_rows[SET_STICK], stick < 0 ? "CUSTOM" : sticks[stick], row == SET_STICK, stick ? VALUE : INFO);
    for (int i = 0; i < 2; ++i) {
        int axis = i ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER, value = trigger_value(axis);
        text(1, setup_rows[SET_LTRIGGER + i], m8_gamepad_axis_label(label, axis), TEXT);
        field(13, setup_rows[SET_LTRIGGER + i], targets[value], row == SET_LTRIGGER + i, value ? VALUE : INFO);
    }
    int r = setup_rows[SET_THRESHOLD], threshold = c->gamepad_analog_threshold;
    text(1, r, "THRESHOLD", TEXT);
    snprintf(line, sizeof line, "%02X", clamp(threshold / 128, 0, 255));
    field(13, r, line, row == SET_THRESHOLD, VALUE);
    /* A 128 px meter: live deflection of the bound axes against the threshold. */
    int level = 0;
    for (int i = 0; i < CONTROLS; ++i) { int v = axis_level(FIELD(c, controls[i].axis)); if (v > level) level = v; }
    int x = 17 * CELL_W, y = r * CELL_H + text_y + 1, width = 16 * CELL_W;
    fill(x, y, width, 5, EMPTY);
    fill(x, y, level * width / 32768, 5, level > threshold ? PLAY : INFO);
    fill(x + clamp(threshold * width / 32768, 0, width - 1), y - 1, 1, 7, VALUE);
    snprintf(line, sizeof line, "%s+%s", m8_gamepad_label(label, SDL_GAMEPAD_BUTTON_BACK),
             m8_gamepad_label(label, SDL_GAMEPAD_BUTTON_START));
    text(1, setup_rows[SET_EXIT], "EXIT COMBO", TEXT);
    field(13, setup_rows[SET_EXIT], o->exit_combo ? line : "OFF", row == SET_EXIT, o->exit_combo ? VALUE : INFO);
    text(1, setup_rows[SET_BACKGROUND], "BACKGROUND", TEXT);
    field(13, setup_rows[SET_BACKGROUND], o->background ? "ON" : "OFF", row == SET_BACKGROUND, o->background ? VALUE : INFO);
    field(1, setup_rows[SET_DEFAULTS], "DEFAULTS", row == SET_DEFAULTS, VALUE);
}

static const char *hint(SDL_Gamepad *label, char *line, int capacity) {
    int row = view.row[view.page];
    if (view.capture >= 0) {
        snprintf(line, capacity, "%s OR ESC CANCELS", m8_gamepad_label(label, SDL_GAMEPAD_BUTTON_BACK));
        return line;
    }
    switch (view.page) {
    case PAGE_PADS: return row ? "EDIT USE ONLY THIS PAD  SHIFT+> MAP" : "EDIT+</> CHOOSE  SHIFT+> MAP";
    case PAGE_MAP: return row < CONTROLS ? "EDIT REMAP  OPTION+EDIT CLEAR" : "EDIT START  OPTION CLOSE";
    default: return row == SET_DEFAULTS ? "EDIT RESET  OPTION CLOSE" : "EDIT+</> CHANGE  OPTION CLOSE";
    }
}

void m8_overlay_render(SDL_Renderer *renderer, int width, int height, SDL_ScaleMode mode, int font_mode) {
    (void)mode; (void)font_mode;
    if (!view.open || !view.ctx) return;
    float tw = 0, th = 0;
    if (view.texture && (!SDL_GetTextureSize(view.texture, &tw, &th) || (int)tw != width || (int)th != height)) {
        SDL_DestroyTexture(view.texture); view.texture = NULL;
    }
    if (!view.texture) {
        view.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
        if (!view.texture) return;
        SDL_SetTextureBlendMode(view.texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(view.texture, SDL_SCALEMODE_NEAREST);
    }
    view.renderer = renderer;
    SDL_Texture *target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, view.texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    /* Keep the firmware's scope rows and level meters live above the page. */
    fill(0, TOP + CELL_H, width, height - TOP - CELL_H, BACKGROUND);
    fill(0, TOP, 34 * CELL_W, CELL_H, BACKGROUND);
    const struct inline_font *previous = inline_font_get_current(), *small = fonts_get(0);
    if (!previous) { SDL_SetRenderTarget(renderer, target); return; }
    if (previous != small) { inline_font_close(); inline_font_initialize(small); }
    text_y = small->text_offset_y;

    SDL_Gamepad *label = m8_handheld_label_pad();
    char line[COLUMNS + 1];
    if (view.page == PAGE_PADS) draw_pads();
    else if (view.page == PAGE_MAP) draw_map(label);
    else draw_setup(label);
    if (view.capture >= 0) {
        if (view.learn) snprintf(line, sizeof line, "LEARN %d/%d: PRESS A BUTTON FOR %s", view.capture + 1, CONTROLS, controls[view.capture].name);
        else snprintf(line, sizeof line, "PRESS A BUTTON FOR %s", controls[view.capture].name);
        text(1, MESSAGE_ROW, line, VALUE);
        if (view.message_until > SDL_GetTicksNS()) text(1, MESSAGE_ROW + 1, view.message, TITLE);
    } else if (view.message_until > SDL_GetTicksNS()) text(1, MESSAGE_ROW, view.message, VALUE);
    text(1, HINT_ROW, hint(label, line, sizeof line), INFO);
    /* The firmware's screen map: one row of pages, the current one lit. */
    for (int i = 0; i < PAGE_COUNT; ++i) text(35 + i, 20, (const char *[]){"G", "M", "S"}[i], i == view.page ? SELECTED : INFO);

    if (previous != small) { inline_font_close(); inline_font_initialize(previous); }
    /* inprint caches the last color; resync it with the font texture. */
    inprint(renderer, " ", 0, 0, 0x010101, 0x010101);
    SDL_SetRenderTarget(renderer, target);
    SDL_RenderTexture(renderer, view.texture, NULL, NULL);
    renderer_request_redraw();
}
