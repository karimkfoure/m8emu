#include "keypad_cpu.h"
#include "keypad_glyphs.h"
#include "keypad_raster.h"
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define M8_ARRAYSIZE(x) (sizeof(x)/sizeof *(x))
static int ascii_casecmp(const char *a, const char *b) {
    while (*a && tolower((unsigned char)*a)==tolower((unsigned char)*b)) {a++;b++;}
    return tolower((unsigned char)*a)-tolower((unsigned char)*b);
}
/* D's shapes and line weights, E's vector typography. All themes use this
 * table, including hit-testing. Outside margins are 16 logical units. */
const M8KeypadLayout m8_keypad_layout={
    .keys={{M8_KEYPAD_UP,{93,38,72,69}},{M8_KEYPAD_OPTION,{187,19,72,72}},
           {M8_KEYPAD_EDIT,{261,19,72,72}},{M8_KEYPAD_LEFT,{19,110,72,72}},
           {M8_KEYPAD_DOWN,{93,110,72,72}},{M8_KEYPAD_RIGHT,{167,110,74,72}},
           {M8_KEYPAD_SHIFT,{93,204,72,72}},{M8_KEYPAD_PLAY,{167,204,74,72}}},
    .groups={{{16,107,228,78},{90,35,78,150}},{{184,16,152,78},{0,0,0,0}},
             {{90,201,154,78},{0,0,0,0}}},
    /* The top legends fit between two frames with at least 2.75 units on
     * each side. This matches the keys' three-unit frame padding. */
    .labels={{"OPT",{223,97.5f},0},{"EDIT",{297,97.5f},1},
             {"SHIFT",{129,287},2},{"PLAY",{204,287},3}},
    .markers={{129,29},{55,192},{129,192},{204,192}},
    .grilles={{16,215,46,5.3f},{290,215,46,5.3f}},
    .grille_pitch=9.2f,.grille_slots=6,
    .frame_radius=4,.key_radius=1.5f,.outer_width=.7f,.inner_width=.4f,
    .outline_gap=1.7f,.key_width=.65f,.label_size=1.22f,.label_weight=.8f,
    .marker_size=1.2f,.marker_weight=.75f
};
typedef struct {
    M8KeypadColor background,frame,outline,cap,edge,text,slot,pressed,accent;
} Palette;
#define RGB(r,g,b) {r,g,b,255}
static const Palette palettes[]={
    {RGB(248,247,243),RGB(248,247,243),RGB(48,51,52),RGB(252,251,248),
     RGB(62,65,66),RGB(36,39,40),RGB(248,247,243),RGB(208,232,226),RGB(56,101,92)},
    {RGB(15,16,18),RGB(26,28,31),RGB(148,153,158),RGB(23,25,28),
     RGB(119,126,132),RGB(200,205,210),RGB(7,9,11),RGB(54,63,69),RGB(196,214,216)},
    {RGB(43,46,50),RGB(58,62,67),RGB(173,179,185),RGB(64,69,75),
     RGB(151,159,167),RGB(230,232,234),RGB(27,31,35),RGB(73,107,103),RGB(178,225,212)},
    {RGB(0,0,0),RGB(7,13,17),RGB(92,119,134),RGB(10,20,26),
     RGB(80,111,128),RGB(172,205,219),RGB(17,34,43),RGB(0,63,57),RGB(0,239,211)},
    {RGB(23,26,30),RGB(35,41,47),RGB(137,152,163),RGB(36,44,51),
     RGB(111,129,142),RGB(209,219,224),RGB(10,16,21),RGB(48,84,77),RGB(163,222,203)}
};
static const char *const theme_names[]={"fine-line-light","bold-hardware",
    "soft-rounded","technical-flat","vector-comparison"};
const char *m8_keypad_theme_name(M8KeypadTheme theme) {
    return (unsigned)theme<M8_KEYPAD_THEME_COUNT?theme_names[theme]:NULL;
}
bool m8_keypad_theme_parse(const char *name,M8KeypadTheme *theme) {
    if(!name||!theme) return false;
    for(unsigned i=0;i<M8_KEYPAD_THEME_COUNT;i++) {
        char alias[2]={(char)('A'+i),0};
        if(!ascii_casecmp(name,theme_names[i])||!ascii_casecmp(name,alias)) {
            *theme=(M8KeypadTheme)i;return true;
        }
    }
    return false;
}
static M8KeypadRect inset(M8KeypadRect r,float n) {
    return (M8KeypadRect){r.x+n,r.y+n,r.w-2*n,r.h-2*n};
}
static M8KeypadRasterShape group_shape(M8KeypadGroup group,float radius,float n) {
    M8KeypadRasterShape shape={inset(group.rectangle,n),group.stem,fmaxf(.1f,radius-n)};
    if(shape.stem.w>0) shape.stem=inset(shape.stem,n);
    return shape;
}
static void draw_key(M8KeypadRaster *r,const Palette *p,const M8KeypadKey *key,uint8_t held) {
    const M8KeypadLayout *l=&m8_keypad_layout;bool pressed=(held&key->key)!=0;
    M8KeypadRasterShape shape={key->rectangle,{0,0,0,0},l->key_radius};
    m8_keypad_raster_shape(r,shape,pressed?p->accent:p->edge);
    shape.base=inset(shape.base,l->key_width);shape.radius-=l->key_width;
    m8_keypad_raster_shape(r,shape,pressed?p->pressed:p->cap);
}
static void draw_layout(M8KeypadRaster *r,M8KeypadTheme theme,uint8_t held,bool caps) {
    const Palette *p=&palettes[theme];const M8KeypadLayout *l=&m8_keypad_layout;
    for(unsigned i=0;i<M8_ARRAYSIZE(l->groups);i++) {
        M8KeypadGroup group=l->groups[i];
        m8_keypad_raster_shape(r,group_shape(group,l->frame_radius,0),p->outline);
        m8_keypad_raster_shape(r,group_shape(group,l->frame_radius,l->outer_width),p->frame);
        m8_keypad_raster_shape(r,group_shape(group,l->frame_radius,l->outline_gap),p->outline);
        m8_keypad_raster_shape(r,group_shape(group,l->frame_radius,l->outline_gap+l->inner_width),p->frame);
    }
    if(caps) for(unsigned i=0;i<M8_ARRAYSIZE(l->keys);i++)
        draw_key(r,p,&l->keys[i],held);
    for(unsigned side=0;side<2;side++) {
        M8KeypadRect slot=l->grilles[side];
        for(unsigned i=0;i<l->grille_slots;i++,slot.y+=l->grille_pitch) {
            M8KeypadRasterShape shape={slot,{0,0,0,0},slot.h*.5f};
            m8_keypad_raster_shape(r,shape,p->outline);
            shape.base=inset(slot,l->inner_width);shape.radius-=l->inner_width;
            m8_keypad_raster_shape(r,shape,p->slot);
        }
    }
    M8KeypadPathSink sink={.context=r,.stroke=m8_keypad_raster_stroke};
    for(unsigned i=0;i<M8_ARRAYSIZE(l->labels);i++) {
        const M8KeypadLabel *label=&l->labels[i];
        m8_keypad_legend(&sink,label->text,label->position.x,label->position.y,
                         label->icon,l->label_size,l->label_weight,p->text);
    }
    for(unsigned i=0;i<M8_ARRAYSIZE(l->markers);i++) {
        M8KeypadPoint point=l->markers[i];
        m8_keypad_chevron(&sink,point.x,point.y,i,l->marker_size,l->marker_weight,p->text);
    }
}
static bool layout(const M8KeypadRect *viewport, float *scale, float *x, float *y) {
    if (!viewport || !isfinite(viewport->x) || !isfinite(viewport->y) ||
        !isfinite(viewport->w) || !isfinite(viewport->h) ||
        viewport->w <= 0 || viewport->h <= 0)
        return false;
    float sx = viewport->w / M8_KEYPAD_WIDTH;
    float sy = viewport->h / M8_KEYPAD_HEIGHT;
    *scale = sx < sy ? sx : sy;
    *x = viewport->x + (viewport->w - M8_KEYPAD_WIDTH * *scale) * .5f;
    *y = viewport->y + (viewport->h - M8_KEYPAD_HEIGHT * *scale) * .5f;
    return true;
}

static M8KeypadRect transform(M8KeypadRect r, float scale, float x, float y) {
    return (M8KeypadRect){x + r.x * scale, y + r.y * scale,
                       r.w * scale, r.h * scale};
}

bool m8_keypad_cpu_button_rect(const M8KeypadRect *viewport, uint8_t key,
                          M8KeypadRect *rectangle) {
    float scale, x, y;
    if (!rectangle || !layout(viewport, &scale, &x, &y))
        return false;
    for (unsigned i = 0; i < M8_ARRAYSIZE(m8_keypad_layout.keys); ++i) {
        if (m8_keypad_layout.keys[i].key == key) {
            *rectangle = transform(m8_keypad_layout.keys[i].rectangle, scale, x, y);
            return true;
        }
    }
    return false;
}

static bool cap_contains(const M8KeypadRect *r, float radius, float x, float y) {
    if (!isfinite(x) || !isfinite(y) || x < r->x || y < r->y ||
        x >= r->x + r->w || y >= r->y + r->h)
        return false;
    float dx = x < r->x + radius ? r->x + radius - x :
               x > r->x + r->w - radius ? x - (r->x + r->w - radius) : 0;
    float dy = y < r->y + radius ? r->y + radius - y :
               y > r->y + r->h - radius ? y - (r->y + r->h - radius) : 0;
    return dx * dx + dy * dy <= radius * radius;
}

uint8_t m8_keypad_cpu_hit_test(const M8KeypadRect *viewport, float x, float y) {
    float scale, offset_x, offset_y;
    if (!layout(viewport, &scale, &offset_x, &offset_y))
        return 0;
    for (unsigned i = 0; i < M8_ARRAYSIZE(m8_keypad_layout.keys); ++i) {
        M8KeypadRect r = transform(m8_keypad_layout.keys[i].rectangle, scale, offset_x, offset_y);
        if (cap_contains(&r, m8_keypad_layout.key_radius * scale, x, y))
            return m8_keypad_layout.keys[i].key;
    }
    return 0;
}


uint8_t m8_keypad_cpu_hit(float x,float y) {
    const M8KeypadRect viewport={0,0,M8_KEYPAD_WIDTH,M8_KEYPAD_HEIGHT};
    return m8_keypad_cpu_hit_test(&viewport,x,y);
}
uint8_t m8_keypad_cpu_hit_pixels(int width,int height,float x,float y) {
    const M8KeypadRect viewport={0,0,(float)width,(float)height};
    return m8_keypad_cpu_hit_test(&viewport,x,y);
}
bool m8_keypad_cpu_render(uint8_t *pixels,int width,int height,
                          M8KeypadTheme theme,uint8_t held) {
    if(!pixels || width<1 || height<1 || width>16384 || height>16384 ||
       (unsigned)theme>=M8_KEYPAD_THEME_COUNT) return false;
    M8KeypadRaster r={pixels,width,height,fminf((float)width/M8_KEYPAD_WIDTH,
                                              (float)height/M8_KEYPAD_HEIGHT),0,0};
    r.x=(width-M8_KEYPAD_WIDTH*r.scale)*.5f;r.y=(height-M8_KEYPAD_HEIGHT*r.scale)*.5f;
    m8_keypad_raster_pixels(&r,0,0,width,height,palettes[theme].background);
    draw_layout(&r,theme,held,true);
    return true;
}

void m8_keypad_cpu_cache_destroy(M8KeypadCpuCache *cache) {
    if(!cache) return;
    free(cache->pixels);free(cache->background);memset(cache,0,sizeof *cache);
}
bool m8_keypad_cpu_cache_update(M8KeypadCpuCache *cache,int width,int height,
                                M8KeypadTheme theme,uint8_t held) {
    if(!cache || width<1 || height<1 || width>16384 || height>16384 ||
       (unsigned)theme>=M8_KEYPAD_THEME_COUNT) return false;
    bool fresh=!cache->pixels || cache->width!=width || cache->height!=height || cache->theme!=theme;
    if(!fresh && cache->held==held) return true;
    size_t bytes=(size_t)width*height*4;
    M8KeypadRaster r={NULL,width,height,fminf((float)width/M8_KEYPAD_WIDTH,
                                           (float)height/M8_KEYPAD_HEIGHT),0,0};
    r.x=(width-M8_KEYPAD_WIDTH*r.scale)*.5f;r.y=(height-M8_KEYPAD_HEIGHT*r.scale)*.5f;
    /* Below logical size, adjacent caps can share edge-coverage pixels.
     * Use the complete inexpensive small raster instead of partial restores. */
    if(r.scale<1) {
        if(fresh) {
            uint8_t *pixels=malloc(bytes);if(!pixels) return false;
            m8_keypad_cpu_cache_destroy(cache);
            *cache=(M8KeypadCpuCache){pixels,NULL,width,height,theme,0};
        }
        m8_keypad_cpu_render(cache->pixels,width,height,theme,held);
        cache->held=held;return true;
    }
    if(fresh) {
        uint8_t *pixels=malloc(bytes),*background=malloc(bytes);
        if(!pixels || !background) {free(pixels);free(background);return false;}
        r.pixels=background;
        m8_keypad_raster_pixels(&r,0,0,width,height,palettes[theme].background);
        /* Caps do not overlap labels or grilles. Preserve a clean backing
         * layer so antialiased edges never blend over an earlier cap. */
        draw_layout(&r,theme,0,false);memcpy(pixels,background,bytes);
        m8_keypad_cpu_cache_destroy(cache);
        *cache=(M8KeypadCpuCache){pixels,background,width,height,theme,0};
    }
    r.pixels=cache->pixels;
    for(unsigned i=0;i<M8_ARRAYSIZE(m8_keypad_layout.keys);i++) {
        const M8KeypadKey *key=&m8_keypad_layout.keys[i];
        if(!fresh && !((cache->held^held)&key->key)) continue;
        if(!fresh) {
            M8KeypadRect box=key->rectangle;
            int x=(int)fmaxf(0,floorf(r.x+box.x*r.scale-1));
            int y=(int)fmaxf(0,floorf(r.y+box.y*r.scale-1));
            int right=(int)fminf(width,ceilf(r.x+(box.x+box.w)*r.scale+1));
            int bottom=(int)fminf(height,ceilf(r.y+(box.y+box.h)*r.scale+1));
            for(int row=y;row<bottom;row++) {
                size_t offset=((size_t)row*width+x)*4;
                memcpy(cache->pixels+offset,cache->background+offset,(size_t)(right-x)*4);
            }
        }
        draw_key(&r,&palettes[theme],key,held);
    }
    cache->held=held;return true;
}
