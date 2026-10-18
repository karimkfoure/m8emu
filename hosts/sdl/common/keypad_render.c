#include "keypad.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void m8_keypad_renderer_destroy(M8KeypadRenderer *p) {
    if(!p) return;
    SDL_DestroyTexture(p->texture);m8_keypad_cpu_cache_destroy(&p->cpu);memset(p,0,sizeof *p);
}
bool m8_keypad_render(M8KeypadRenderer *p,const M8Keypad *keys,SDL_Renderer *renderer,
                     const SDL_FRect *viewport,uint8_t external_held,M8KeypadTheme theme) {
    if(!p||!keys||!renderer||!viewport||(unsigned)theme>=M8_KEYPAD_THEME_COUNT||
       !isfinite(viewport->x)||!isfinite(viewport->y)||!isfinite(viewport->w)||
       !isfinite(viewport->h)||viewport->w<1||viewport->h<1||
       viewport->w>16384||viewport->h>16384) return SDL_SetError("Invalid keypad viewport or theme");
    int width=(int)lroundf(viewport->w),height=(int)lroundf(viewport->h);
    uint8_t held=m8_keypad_held(keys)|external_held;
    if(!p->texture||p->renderer!=renderer||p->width!=width||p->height!=height||
       p->theme!=theme||p->held!=held) {
        if(!m8_keypad_cpu_cache_update(&p->cpu,width,height,theme,held)) return SDL_OutOfMemory();
        bool replace=!p->texture||p->renderer!=renderer||p->width!=width||p->height!=height;
        SDL_Texture *texture=replace?SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STATIC,width,height):p->texture;
        bool ready=texture&&SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE)&&
                   SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST)&&
                   SDL_UpdateTexture(texture,NULL,p->cpu.pixels,width*4);
        if(!ready) {if(replace) SDL_DestroyTexture(texture);return false;}
        if(replace) SDL_DestroyTexture(p->texture);
        p->texture=texture;p->renderer=renderer;
        p->width=width;p->height=height;p->theme=theme;p->held=held;p->rasterizations++;
    }
    return SDL_RenderTexture(renderer,p->texture,NULL,viewport);
}
