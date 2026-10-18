#ifndef M8_TEXT_FONT_H
#define M8_TEXT_FONT_H
#include <SDL3/SDL.h>
typedef struct m8_text_font m8_text_font;
/* JetBrains Mono at a 12 px em, rasterized at the window's display scale.
 * The atlas contains glyph indices, so every supported UTF-8 name is usable. */
m8_text_font *m8_text_font_create(SDL_Renderer *,float display_scale);
void m8_text_font_destroy(m8_text_font *);
float m8_text_font_glyph(m8_text_font *,uint32_t,float x,float y,SDL_Color);
float m8_text_font_advance(const m8_text_font *);
#endif
