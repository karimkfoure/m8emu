#include "handheld.h"
#include "gamepad_view.h"
#include "bridge.h"
#include "common.h"
#include "input.h"
#include "ini.h"
#include "settings.h"
#include "backends/m8.h"
#include <stdio.h>
#include <string.h>

/* m8c's settings view reads this array. Keep the same size and symbol. */
SDL_Gamepad *game_controllers[M8_MAX_GAMEPADS];

static bool gamepads_started, quit_latched, back_tap, overlay_seen;
static m8_pad_options options = {.exit_combo = true};
static SDL_JoystickID last_used;
static struct {
    SDL_JoystickID id;
    uint64_t activity;
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    signed char axes[SDL_GAMEPAD_AXIS_COUNT];
} input_devices[M8_MAX_GAMEPADS];

void m8_handheld_config_defaults(config_params_s *conf) {
    conf->gamepad_select = SDL_GAMEPAD_BUTTON_WEST;
    conf->gamepad_start = SDL_GAMEPAD_BUTTON_NORTH;
}

static char *options_path(void) {
    char *preferences = SDL_GetPrefPath("", "m8c"), *path = NULL;
    if (preferences && SDL_asprintf(&path, "%sm8emu_gamepad.ini", preferences) < 0) path = NULL;
    SDL_free(preferences);
    return path;
}

static void options_load(void) {
    char *path = options_path();
    ini_t *ini = path ? ini_load(path) : NULL;
    SDL_free(path);
    if (!ini) return;
    const char *combo = ini_get(ini, "gamepad", "exit_combo");
    const char *background = ini_get(ini, "gamepad", "background_events");
    const char *active = ini_get(ini, "gamepad", "active");
    if (combo) options.exit_combo = SDL_strcasecmp(combo, "false") != 0;
    if (background) options.background = SDL_strcasecmp(background, "true") == 0;
    if (active) SDL_strlcpy(options.active, active, sizeof options.active);
    ini_free(ini);
}

m8_pad_options *m8_handheld_options(void) { return &options; }

void m8_handheld_options_changed(void) {
    /* SDL accepts this hint at any time, for every joystick driver. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, options.background ? "1" : "0");
    char *path = options_path();
    SDL_IOStream *file = path ? SDL_IOFromFile(path, "w") : NULL;
    SDL_free(path);
    if (!file) { SDL_Log("Cannot write gamepad options: %s", SDL_GetError()); return; }
    SDL_IOprintf(file, "[gamepad]\nexit_combo=%s\nbackground_events=%s\nactive=%s\n",
                 options.exit_combo ? "true" : "false", options.background ? "true" : "false", options.active);
    SDL_CloseIO(file);
}

bool m8_handheld_guid(SDL_JoystickID id, char *text, int capacity) {
    SDL_GUID guid = SDL_GetGamepadGUIDForID(id);
    SDL_GUIDToString(guid, text, capacity);
    for (size_t i = 0; i < sizeof guid.data; ++i) if (guid.data[i]) return true;
    return false;
}

SDL_Gamepad *m8_handheld_active(void) {
    char guid[64];
    if (!options.active[0]) return NULL;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (game_controllers[slot] && m8_handheld_guid(SDL_GetGamepadID(game_controllers[slot]), guid, sizeof guid) &&
            !strcmp(guid, options.active)) return game_controllers[slot];
    }
    return NULL;
}

SDL_Gamepad *m8_handheld_label_pad(void) {
    SDL_Gamepad *pad = m8_handheld_active(), *first = NULL;
    if (pad) return pad;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (!game_controllers[slot]) continue;
        if (SDL_GetGamepadID(game_controllers[slot]) == last_used) return game_controllers[slot];
        if (!first) first = game_controllers[slot];
    }
    return first;
}

uint64_t m8_handheld_activity(SDL_JoystickID id) {
    for (int i = 0; i < M8_MAX_GAMEPADS; ++i) if (input_devices[i].id == id) return input_devices[i].activity;
    return 0;
}

static int load_database(const char *directory) {
    if (!directory) return 0;
    char *path = NULL;
    if (SDL_asprintf(&path, "%sgamecontrollerdb.txt", directory) < 0) return -1;
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    SDL_free(path);
    if (!file) return 0;
    int count = SDL_AddGamepadMappingsFromIO(file, true);
    if (count < 0) return -1;
    SDL_Log("Loaded %d gamepad mappings", count);
    return 1;
}

static int initialize_database(void) {
    const char *override = SDL_getenv("M8_GAMEPAD_DB");
    if (override && *override) {
        int count = SDL_AddGamepadMappingsFromFile(override);
        if (count < 0) return -1;
        SDL_Log("Loaded %d gamepad mappings from %s", count, override);
        return 0;
    }
    char *preferences = SDL_GetPrefPath("", "m8c");
    int loaded = load_database(preferences);
    SDL_free(preferences);
    if (loaded < 0) return -1;
    if (!loaded) loaded = load_database(SDL_GetBasePath());
    if (loaded < 0) return -1;
    if (!loaded) SDL_Log("No gamecontrollerdb.txt found; using SDL built-in mappings");
    return 0;
}

int gamepads_initialize(void) {
    if (!gamepads_started) {
        options_load();
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, options.background ? "1" : "0");
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return -1;
        gamepads_started = true;
        if (initialize_database() < 0) {
            gamepads_close();
            return -1;
        }
    }
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids) return -1;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (game_controllers[slot] && !SDL_GamepadConnected(game_controllers[slot])) {
            SDL_CloseGamepad(game_controllers[slot]);
            game_controllers[slot] = NULL;
        }
    }
    for (int i = 0; i < count; ++i) {
        bool opened = false;
        int free_slot = -1;
        for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
            if (!game_controllers[slot]) {
                if (free_slot < 0) free_slot = slot;
            } else if (SDL_GetGamepadID(game_controllers[slot]) == ids[i]) {
                opened = true;
            }
        }
        if (opened || free_slot < 0) continue;
        /* One device that fails to open must not disable the others. */
        game_controllers[free_slot] = SDL_OpenGamepad(ids[i]);
        if (!game_controllers[free_slot]) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Gamepad %u: %s", (unsigned)ids[i], SDL_GetError());
            continue;
        }
        SDL_Log("Gamepad: %s", SDL_GetGamepadName(game_controllers[free_slot]));
    }
    SDL_free(ids);
    int opened = 0;
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (game_controllers[slot]) ++opened;
    }
    return opened;
}

void gamepads_close(void) {
    m8_gamepad_view_close(NULL);
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (game_controllers[slot]) SDL_CloseGamepad(game_controllers[slot]);
        game_controllers[slot] = NULL;
    }
    memset(input_devices, 0, sizeof input_devices);
    quit_latched = back_tap = false;
    if (gamepads_started) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    gamepads_started = false;
}

static int input_slot(SDL_JoystickID id) {
    int free_slot = -1;
    for (int i = 0; i < M8_MAX_GAMEPADS; ++i) {
        if (input_devices[i].id == id) return i;
        if (!input_devices[i].id && free_slot < 0) free_slot = i;
    }
    if (free_slot >= 0) input_devices[free_slot].id = id;
    return free_slot;
}

static void release_device(struct app_context *ctx, int slot) {
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
        if (button == SDL_GAMEPAD_BUTTON_BACK || (button == SDL_GAMEPAD_BUTTON_START && options.exit_combo)) continue;
        if (input_devices[slot].buttons[button])
            input_handle_gamepad_button(ctx, (SDL_GamepadButton)button, false);
    }
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        if (input_devices[slot].axes[axis])
            input_handle_gamepad_axis(ctx, (SDL_GamepadAxis)axis, 0);
    }
    SDL_JoystickID id = input_devices[slot].id;
    uint64_t activity = input_devices[slot].activity;
    memset(&input_devices[slot], 0, sizeof input_devices[slot]);
    input_devices[slot].id = id; input_devices[slot].activity = activity;
}

void m8_handheld_release(struct app_context *ctx) {
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot)
        if (input_devices[slot].id && !quit_latched) release_device(ctx, slot);
    back_tap = false;
    if (m8_bridge_held()) m8_send_msg_controller(0);
}

/* A threshold zone, as m8c evaluates it: -1, 0 or 1. */
static signed char axis_zone(const config_params_s *conf, Sint16 value) {
    Sint16 effective = conf->gamepad_analog_invert ? (Sint16)(-value) : value;
    return effective < -conf->gamepad_analog_threshold ? -1 : effective > conf->gamepad_analog_threshold;
}

/* Another connected pad is selected as the only M8 controller. */
static bool foreign(SDL_JoystickID id) {
    SDL_Gamepad *active = m8_handheld_active();
    return active && SDL_GetGamepadID(active) != id;
}

bool m8_handheld_event(struct app_context *ctx, const SDL_Event *event) {
    int slot;
    /* Either overlay owns input while open. Opening or closing one releases
     * the M8 mask, since m8c bindings may change before the matching release. */
    bool overlay = settings_is_open() || m8_gamepad_view_is_open();
    if (overlay != overlay_seen) { overlay_seen = overlay; m8_handheld_release(ctx); }
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMAPPED:
        if (gamepads_initialize() < 0) SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Gamepad: %s", SDL_GetError());
        m8_gamepad_view_refresh();
        return true;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
            if (input_devices[slot].id == event->gdevice.which) {
                if (!quit_latched) release_device(ctx, slot);
                memset(&input_devices[slot], 0, sizeof input_devices[slot]);
            }
        }
        if (gamepads_initialize() < 0) SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Gamepad: %s", SDL_GetError());
        m8_gamepad_view_refresh();
        return true;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat && !settings_is_open() &&
            (event->key.key == SDLK_F3 || (event->key.key == SDLK_G && (event->key.mod & SDL_KMOD_GUI)))) {
            m8_gamepad_view_toggle(ctx);
            return true;
        }
        return m8_gamepad_view_is_open() && m8_gamepad_view_event(ctx, event);
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (quit_latched) return true;
        slot = input_slot(event->gbutton.which);
        if (slot >= 0 && event->gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
            input_devices[slot].buttons[event->gbutton.button] = event->gbutton.down;
            input_devices[slot].activity = SDL_GetTicksNS();
        }
        last_used = event->gbutton.which;
        if (event->gbutton.down) back_tap = event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK;
        /* Back/Select and Start belong to the application. Swallow both even
         * when a user config maps one to an M8 key; the first press must not
         * change the instrument before the second press completes the chord. */
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK ||
            (event->gbutton.button == SDL_GAMEPAD_BUTTON_START && options.exit_combo)) {
            if (options.exit_combo && slot >= 0 && input_devices[slot].buttons[SDL_GAMEPAD_BUTTON_BACK] &&
                input_devices[slot].buttons[SDL_GAMEPAD_BUTTON_START]) {
                quit_latched = true;
                m8_send_msg_controller(0);
                ctx->app_state = QUIT;
            } else if (!event->gbutton.down && back_tap && !settings_is_open()) {
                back_tap = false;
                m8_gamepad_view_toggle(ctx);
            } else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_START && m8_gamepad_view_is_open()) {
                m8_gamepad_view_event(ctx, event);
            }
            return true;
        }
        if (m8_gamepad_view_is_open()) return m8_gamepad_view_event(ctx, event);
        return foreign(event->gbutton.which);
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (quit_latched) return true;
        if (m8_gamepad_view_is_open()) return m8_gamepad_view_event(ctx, event);
        if (foreign(event->gaxis.which)) return true;
        slot = input_slot(event->gaxis.which);
        if (slot >= 0 && event->gaxis.axis < SDL_GAMEPAD_AXIS_COUNT && !settings_is_open()) {
            /* Forward only threshold crossings. Stick noise inside one zone
             * would otherwise clear a direction held on the D-pad. */
            signed char zone = axis_zone(&ctx->conf, event->gaxis.value);
            if (zone == input_devices[slot].axes[event->gaxis.axis]) return true;
            input_devices[slot].axes[event->gaxis.axis] = zone;
            if (zone) { input_devices[slot].activity = SDL_GetTicksNS(); last_used = event->gaxis.which; }
        }
        break;
    default:
        break;
    }
    return false;
}
