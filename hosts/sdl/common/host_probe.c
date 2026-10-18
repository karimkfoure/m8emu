#include "host_probe.h"
#include "render_platform.h"
#include <string.h>

typedef struct {
    SDL_LogOutputFunction previous;
    void *userdata;
    SDL_Mutex *mutex;
    char pageflip_error[512];
} probe_log;

static void SDLCALL capture_log(void *userdata, int category, SDL_LogPriority priority,
                               const char *message)
{
    probe_log *log = userdata;
    if (category == SDL_LOG_CATEGORY_VIDEO && priority >= SDL_LOG_PRIORITY_ERROR &&
        (SDL_strstr(message, "Could not queue pageflip") || SDL_strstr(message, "drmModePageFlip"))) {
        SDL_LockMutex(log->mutex);
        if (!*log->pageflip_error)
            SDL_snprintf(log->pageflip_error, sizeof log->pageflip_error, "%s", message);
        SDL_UnlockMutex(log->mutex);
    }
    if (log->previous) log->previous(log->userdata, category, priority, message);
}

static bool read_pageflip_error(probe_log *log, char *error, size_t capacity)
{
    SDL_LockMutex(log->mutex);
    bool failed = *log->pageflip_error != 0;
    if (failed) SDL_snprintf(error, capacity, "%s", log->pageflip_error);
    SDL_UnlockMutex(log->mutex);
    return failed;
}

static void string(FILE *file, const char *value)
{
    if (!value) { fputs("null", file); return; }
    fputc('"', file);
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == '"' || *p == '\\') fprintf(file, "\\%c", *p);
        else if (*p < 0x20) fprintf(file, "\\u%04x", *p);
        else fputc(*p, file);
    }
    fputc('"', file);
}

static const char *requested(const char *hint, const char *legacy)
{
    const char *value = SDL_GetHint(hint);
    return value ? value : SDL_getenv(legacy);
}

static void mode(FILE *file, const SDL_DisplayMode *value)
{
    if (!value) { fputs("null", file); return; }
    fprintf(file, "{\"width\":%d,\"height\":%d,\"refresh_hz\":%.3f,\"pixel_density\":%.3f}",
            value->w, value->h, (double)value->refresh_rate, (double)value->pixel_density);
}

static void drivers(FILE *file, int count, const char *(*name)(int))
{
    fputc('[', file);
    for (int i = 0; i < count; ++i) {
        if (i) fputc(',', file);
        string(file, name(i));
    }
    fputc(']', file);
}

int m8_host_info(FILE *output)
{
    char video_error[512] = "", audio_error[512] = "", display_error[512] = "";
    bool video = SDL_InitSubSystem(SDL_INIT_VIDEO);
    if (!video) SDL_snprintf(video_error, sizeof video_error, "%s", SDL_GetError());
    bool audio = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!audio) SDL_snprintf(audio_error, sizeof audio_error, "%s", SDL_GetError());
    int count = 0;
    SDL_DisplayID *displays = video ? SDL_GetDisplays(&count) : NULL;
    if (video && !displays) SDL_snprintf(display_error, sizeof display_error, "%s", SDL_GetError());
    fprintf(output, "host_info={\"compiled_sdl\":%d,\"linked_sdl\":%d,\"drivers\":{\"video\":",
            SDL_VERSION, SDL_GetVersion());
    drivers(output, SDL_GetNumVideoDrivers(), SDL_GetVideoDriver);
    fputs(",\"audio\":", output);
    drivers(output, SDL_GetNumAudioDrivers(), SDL_GetAudioDriver);
    fputs(",\"render\":", output);
    drivers(output, SDL_GetNumRenderDrivers(), SDL_GetRenderDriver);
    fputs("},\"requested\":{\"video\":", output);
    string(output, requested(SDL_HINT_VIDEO_DRIVER, "SDL_VIDEODRIVER"));
    fputs(",\"audio\":", output);
    string(output, requested(SDL_HINT_AUDIO_DRIVER, "SDL_AUDIODRIVER"));
    fputs(",\"render\":", output); string(output, SDL_GetHint(SDL_HINT_RENDER_DRIVER));
    fputs("},\"selected\":{\"video\":", output); string(output, SDL_GetCurrentVideoDriver());
    fputs(",\"audio\":", output); string(output, SDL_GetCurrentAudioDriver());
    fputs(",\"render\":null},\"video\":{\"initialized\":", output);
    fputs(video ? "true" : "false", output);
    fputs(",\"error\":", output); string(output, *video_error ? video_error : NULL);
    fputs(",\"display_error\":", output); string(output, *display_error ? display_error : NULL);
    fputs(",\"displays\":[", output);
    for (int i = 0; i < count; ++i) {
        char current_error[256] = "", desktop_error[256] = "";
        const SDL_DisplayMode *current = SDL_GetCurrentDisplayMode(displays[i]);
        if (!current) SDL_snprintf(current_error, sizeof current_error, "%s", SDL_GetError());
        const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(displays[i]);
        if (!desktop) SDL_snprintf(desktop_error, sizeof desktop_error, "%s", SDL_GetError());
        fprintf(output, "%s{\"id\":%u,\"name\":", i ? "," : "", displays[i]);
        string(output, SDL_GetDisplayName(displays[i]));
        fputs(",\"current\":", output); mode(output, current);
        fputs(",\"current_error\":", output); string(output, *current_error ? current_error : NULL);
        fputs(",\"desktop\":", output); mode(output, desktop);
        fputs(",\"desktop_error\":", output); string(output, *desktop_error ? desktop_error : NULL);
        fputc('}', output);
    }
    SDL_free(displays);
    fputs("]},\"audio\":{\"initialized\":", output); fputs(audio ? "true" : "false", output);
    fputs(",\"error\":", output); string(output, *audio_error ? audio_error : NULL);
    fputs("}}\n", output);
    int failed = ferror(output) || fflush(output);
    SDL_Quit();
    return failed ? 1 : 0;
}

int m8_host_video_probe(FILE *output, bool fullscreen, bool integer_scale, int vsync)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    const char *stage = "video_init";
    char error[512] = "";
    int width = 0, height = 0, output_width = 0, output_height = 0, actual_vsync = -1;
    int logical_width = 0, logical_height = 0;
    SDL_RendererLogicalPresentation logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    unsigned presents = 0;
    bool ok = false;
    bool gl_context = false, swap_read = false;
    int swap_interval = -1;
    char swap_error[256] = "";
    probe_log log = {0};
    SDL_LogPriority log_priority = SDL_GetLogPriority(SDL_LOG_CATEGORY_VIDEO);
    bool capture_started = false;
    m8_render_configure(fullscreen);
    m8_render_configure_vsync(vsync);
    stage = "log_capture";
    log.mutex = SDL_CreateMutex();
    if (!log.mutex) goto finish;
    SDL_GetLogOutputFunction(&log.previous, &log.userdata);
    if (log_priority > SDL_LOG_PRIORITY_ERROR)
        SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_ERROR);
    SDL_SetLogOutputFunction(capture_log, &log);
    capture_started = true;
    stage = "video_init";
    if (!SDL_Init(SDL_INIT_VIDEO)) goto finish;
    stage = "window_renderer";
    if (!m8_create_window_renderer("M8 video probe", 640, 480,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL,
            &window, &renderer)) goto finish;
    stage = "fullscreen";
    if (fullscreen && !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        SDL_SetError("Requested fullscreen was not applied"); goto finish;
    }
    stage = "vsync";
    if (!m8_set_vsync(renderer, 1) || !SDL_GetRenderVSync(renderer, &actual_vsync)) goto finish;
    const char *backend = SDL_GetRendererName(renderer);
    bool software = backend && !SDL_strcmp(backend, "software");
    if (vsync >= 0 && !software && actual_vsync != vsync) {
        SDL_SetError("Required vsync differs: requested=%d actual=%d", vsync, actual_vsync); goto finish;
    }
    stage = "logical_presentation";
    if (!SDL_SetRenderLogicalPresentation(renderer, 320, 240, integer_scale ?
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX)) goto finish;
    if (!SDL_GetRenderLogicalPresentation(renderer, &logical_width, &logical_height, &logical_mode)) goto finish;
    if (logical_width != 320 || logical_height != 240 || logical_mode != (integer_scale ?
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE : SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_SetError("Requested logical display scaling was not applied"); goto finish;
    }
    stage = "target_texture";
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 320, 240);
    if (!texture || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) goto finish;
    stage = "present";
    for (unsigned i = 0; i < 3; ++i) {
        SDL_FRect rectangle = {16.0f + i * 32.0f, 24.0f, 96.0f, 64.0f};
        if (!SDL_SetRenderTarget(renderer, texture) || !SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) ||
            !SDL_RenderClear(renderer) || !SDL_SetRenderDrawColor(renderer, 0, 192, 255, 255) ||
            !SDL_RenderFillRect(renderer, &rectangle) || !SDL_SetRenderTarget(renderer, NULL) ||
            !SDL_RenderTexture(renderer, texture, NULL, NULL) || !m8_render_present(renderer)) goto finish;
        ++presents;
        if (read_pageflip_error(&log, error, sizeof error)) {
            stage = "pageflip"; SDL_SetError("%s", error); goto finish;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {}
        SDL_Delay(16);
    }
    if (read_pageflip_error(&log, error, sizeof error)) {
        stage = "pageflip"; SDL_SetError("%s", error); goto finish;
    }
    stage = "swap_interval";
    gl_context = SDL_GL_GetCurrentContext() != NULL;
    if (gl_context) {
        swap_read = SDL_GL_GetSwapInterval(&swap_interval);
        if (!swap_read) SDL_snprintf(swap_error, sizeof swap_error, "%s", SDL_GetError());
    }
    if (vsync == 1 && (gl_context || (backend &&
        (!SDL_strcmp(backend, "opengles2") || !SDL_strcmp(backend, "opengl")))) &&
        (!swap_read || swap_interval != 1)) {
        SDL_SetError("OpenGL swap interval must be 1: context=%d actual=%d detail=%s",
                     gl_context, swap_interval, swap_error);
        goto finish;
    }
    stage = "display_mode";
    if (!SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)) ||
        !SDL_GetWindowSize(window, &width, &height) ||
        !SDL_GetRenderOutputSize(renderer, &output_width, &output_height)) goto finish;
    stage = "complete";
    ok = true;
finish:
    if (!ok) SDL_snprintf(error, sizeof error, "%s", SDL_GetError());
    if (capture_started) {
        SDL_SetLogOutputFunction(log.previous, log.userdata);
        SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, log_priority);
    }
    SDL_DestroyMutex(log.mutex);
    fprintf(output, "video_probe={\"ok\":%s,\"stage\":", ok ? "true" : "false");
    string(output, stage); fputs(",\"error\":", output); string(output, ok ? NULL : error);
    fputs(",\"requested\":{\"video\":", output);
    string(output, requested(SDL_HINT_VIDEO_DRIVER, "SDL_VIDEODRIVER"));
    fputs(",\"render\":", output); string(output, SDL_GetHint(SDL_HINT_RENDER_DRIVER));
    fprintf(output, ",\"fullscreen\":%s,\"integer_scale\":%s,\"vsync\":%d}",
            fullscreen ? "true" : "false", integer_scale ? "true" : "false", vsync);
    fputs(",\"selected\":{\"video\":", output); string(output, SDL_GetCurrentVideoDriver());
    fputs(",\"render\":", output); string(output, renderer ? SDL_GetRendererName(renderer) : NULL);
    fputs("},\"mode\":", output);
    mode(output, window ? SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window)) : NULL);
    fprintf(output, ",\"window\":{\"width\":%d,\"height\":%d,\"fullscreen\":%s},"
            "\"output\":{\"width\":%d,\"height\":%d},\"vsync\":%d,\"presents\":%u,"
            "\"logical\":{\"width\":%d,\"height\":%d,\"integer_scale\":%s},"
            "\"gl_context\":%s,\"swap_interval\":",
            width, height, window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) ? "true" : "false",
            output_width, output_height, actual_vsync, presents, logical_width, logical_height,
            logical_mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE ? "true" : "false",
            gl_context ? "true" : "false");
    if (swap_read) fprintf(output, "%d", swap_interval); else fputs("null", output);
    fputs(",\"swap_error\":", output); string(output, *swap_error ? swap_error : NULL);
    fputs("}\n", output);
    int failed = ferror(output) || fflush(output);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return ok && !failed ? 0 : 1;
}
