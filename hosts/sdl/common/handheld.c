#include "handheld.h"
#include "common.h"
#include "input.h"
#include "backends/m8.h"
#include <stdio.h>
#include <string.h>

#define M8_MAX_GAMEPADS 4

/* m8c's settings view reads this array. Keep the same size and symbol. */
SDL_Gamepad *game_controllers[M8_MAX_GAMEPADS];

static bool gamepads_started;
static bool quit_latched;
static struct {
    SDL_JoystickID id;
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    bool axes[SDL_GAMEPAD_AXIS_COUNT];
} input_devices[M8_MAX_GAMEPADS];

void m8_handheld_config_defaults(config_params_s *conf) {
    conf->gamepad_select = SDL_GAMEPAD_BUTTON_WEST;
    conf->gamepad_start = SDL_GAMEPAD_BUTTON_NORTH;
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
        game_controllers[free_slot] = SDL_OpenGamepad(ids[i]);
        if (!game_controllers[free_slot]) {
            SDL_free(ids);
            return -1;
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
    for (int slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
        if (game_controllers[slot]) SDL_CloseGamepad(game_controllers[slot]);
        game_controllers[slot] = NULL;
    }
    memset(input_devices, 0, sizeof input_devices);
    quit_latched = false;
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
        if (button == SDL_GAMEPAD_BUTTON_BACK || button == SDL_GAMEPAD_BUTTON_START) continue;
        if (input_devices[slot].buttons[button])
            input_handle_gamepad_button(ctx, (SDL_GamepadButton)button, false);
    }
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        if (input_devices[slot].axes[axis])
            input_handle_gamepad_axis(ctx, (SDL_GamepadAxis)axis, 0);
    }
    memset(&input_devices[slot], 0, sizeof input_devices[slot]);
}

bool m8_handheld_event(struct app_context *ctx, const SDL_Event *event) {
    int slot;
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMAPPED:
        if (gamepads_initialize() < 0) SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Gamepad: %s", SDL_GetError());
        return true;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (slot = 0; slot < M8_MAX_GAMEPADS; ++slot) {
            if (input_devices[slot].id == event->gdevice.which) {
                if (quit_latched) memset(&input_devices[slot], 0, sizeof input_devices[slot]);
                else release_device(ctx, slot);
            }
        }
        if (gamepads_initialize() < 0) SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Gamepad: %s", SDL_GetError());
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (quit_latched) return true;
        slot = input_slot(event->gbutton.which);
        if (slot >= 0 && event->gbutton.button < SDL_GAMEPAD_BUTTON_COUNT)
            input_devices[slot].buttons[event->gbutton.button] = event->gbutton.down;
        /* Back/Select and Start belong to the application. Swallow both even
         * when a user config maps one to an M8 key; the first press must not
         * change the instrument before the second press completes the chord. */
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK ||
            event->gbutton.button == SDL_GAMEPAD_BUTTON_START) {
            if (slot >= 0 && input_devices[slot].buttons[SDL_GAMEPAD_BUTTON_BACK] &&
                input_devices[slot].buttons[SDL_GAMEPAD_BUTTON_START]) {
                quit_latched = true;
                m8_send_msg_controller(0);
                ctx->app_state = QUIT;
            }
            return true;
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (quit_latched) return true;
        slot = input_slot(event->gaxis.which);
        if (slot >= 0 && event->gaxis.axis < SDL_GAMEPAD_AXIS_COUNT)
            input_devices[slot].axes[event->gaxis.axis] = event->gaxis.value != 0;
        break;
    default:
        break;
    }
    return false;
}
