#include "text_font.h"
#include "embedded_text_font.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"
#include <math.h>

typedef struct { SDL_FRect source;float x,y,advance; } glyph;
struct m8_text_font {
    SDL_Renderer *renderer;
    SDL_Texture *atlas;
    stbtt_fontinfo info;
    glyph *glyphs;
    float scale,baseline,advance;
};
m8_text_font *m8_text_font_create(SDL_Renderer *renderer,float display_scale) {
    m8_text_font *font=SDL_calloc(1,sizeof *font);if(!font)return NULL;
    font->renderer=renderer;font->scale=display_scale;
    if(!stbtt_InitFont(&font->info,m8_embedded_text_font,0))goto failed;
    float pixels=stbtt_ScaleForMappingEmToPixels(&font->info,12*display_scale);
    int ascent,descent;stbtt_GetFontVMetrics(&font->info,&ascent,&descent,NULL);
    font->baseline=(20.4f*display_scale-(ascent-descent)*pixels)/2+ascent*pixels;
    font->glyphs=SDL_calloc((size_t)font->info.numGlyphs,sizeof *font->glyphs);if(!font->glyphs)goto failed;
    int width=display_scale>1?1024:512,x=1,y=1,row=0;
    for(int i=0;i<font->info.numGlyphs;i++) {
        int x0,y0,x1,y1,advance;stbtt_GetGlyphBitmapBox(&font->info,i,pixels,pixels,&x0,&y0,&x1,&y1);
        stbtt_GetGlyphHMetrics(&font->info,i,&advance,NULL);
        int w=x1-x0,h=y1-y0;
        if(x+w+1>width){x=1;y+=row+1;row=0;}
        glyph *g=&font->glyphs[i];g->source=(SDL_FRect){(float)x,(float)y,(float)w,(float)h};
        g->x=x0/display_scale;g->y=(font->baseline+y0)/display_scale;g->advance=advance*pixels/display_scale;
        x+=w+1;row=SDL_max(row,h);
    }
    int height=y+row+1;SDL_Surface *surface=SDL_CreateSurface(width,height,SDL_PIXELFORMAT_RGBA32);
    if(!surface)goto failed;
    SDL_ClearSurface(surface,1,1,1,0);
    for(int i=0;i<font->info.numGlyphs;i++) {
        glyph *g=&font->glyphs[i];int w=(int)g->source.w,h=(int)g->source.h;
        if(!w||!h)continue;
        unsigned char *coverage=SDL_malloc((size_t)w*h);
        if(!coverage){SDL_DestroySurface(surface);goto failed;}
        stbtt_MakeGlyphBitmap(&font->info,coverage,w,h,w,pixels,pixels,i);
        for(int gy=0;gy<h;gy++)for(int gx=0;gx<w;gx++) {
            uint8_t *p=(uint8_t *)surface->pixels+((int)g->source.y+gy)*surface->pitch+((int)g->source.x+gx)*4;
            p[0]=p[1]=p[2]=255;p[3]=coverage[gy*w+gx];
        }
        SDL_free(coverage);
    }
    font->atlas=SDL_CreateTextureFromSurface(renderer,surface);SDL_DestroySurface(surface);
    if(!font->atlas)goto failed;
    SDL_SetTextureScaleMode(font->atlas,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(font->atlas,SDL_BLENDMODE_BLEND);
    font->advance=font->glyphs[stbtt_FindGlyphIndex(&font->info,'0')].advance;return font;
failed:
    m8_text_font_destroy(font);return NULL;
}
void m8_text_font_destroy(m8_text_font *font) {
    if(font){SDL_DestroyTexture(font->atlas);SDL_free(font->glyphs);SDL_free(font);}
}
float m8_text_font_advance(const m8_text_font *font){return font->advance;}
float m8_text_font_glyph(m8_text_font *font,uint32_t code,float x,float y,SDL_Color color) {
    glyph *g=&font->glyphs[stbtt_FindGlyphIndex(&font->info,(int)code)];
    if(g->source.w&&g->source.h) {
        SDL_FRect to={roundf((x+g->x)*font->scale)/font->scale,
            roundf((y+g->y)*font->scale)/font->scale,g->source.w/font->scale,g->source.h/font->scale};
        SDL_SetTextureColorMod(font->atlas,color.r,color.g,color.b);SDL_SetTextureAlphaMod(font->atlas,color.a);
        SDL_RenderTexture(font->renderer,font->atlas,&g->source,&to);
    }
    return x+g->advance;
}
