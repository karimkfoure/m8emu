#include "text_ui.h"
#include <math.h>
#include <string.h>

float m8_ui_point_scale(SDL_Window *window) {
    float density=SDL_GetWindowPixelDensity(window),scale=SDL_GetWindowDisplayScale(window);
    return density>0&&scale>0?scale/density:1;
}
SDL_Color m8_ui_mix(SDL_Color fg,SDL_Color bg,float fraction) {
    return (SDL_Color){(Uint8)lroundf(fg.r*fraction+bg.r*(1-fraction)),
        (Uint8)lroundf(fg.g*fraction+bg.g*(1-fraction)),(Uint8)lroundf(fg.b*fraction+bg.b*(1-fraction)),255};
}
m8_ui_palette m8_ui_colors(M8KeypadTheme theme) {
    /* Page/background/foreground/accent are the web.js THEMES values. */
    static const SDL_Color page[]={{242,241,237,255},{21,22,24,255},{49,52,56,255},{5,6,7,255},{29,32,36,255}};
    static const SDL_Color background[]={{248,247,243,255},{15,16,18,255},{43,46,50,255},{0,0,0,255},{23,26,30,255}};
    static const SDL_Color fg[]={{36,39,40,255},{200,205,210,255},{230,232,234,255},{172,205,219,255},{209,219,224,255}};
    static const SDL_Color accent[]={{56,101,92,255},{196,214,216,255},{178,225,212,255},{0,239,211,255},{163,222,203,255}};
    unsigned i=(unsigned)theme<M8_KEYPAD_THEME_COUNT?(unsigned)theme:0;
    m8_ui_palette p={.page=page[i],.background=background[i],.foreground=fg[i],.accent=accent[i],.dark=i!=0};
    p.link=i?(SDL_Color){138,180,248,255}:(SDL_Color){0,0,238,255};
    p.warning=i?(SDL_Color){255,107,107,255}:(SDL_Color){193,39,39,255};
    p.danger=i?(SDL_Color){224,108,108,255}:(SDL_Color){193,39,39,255};
    p.muted=m8_ui_mix(p.foreground,p.page,.7f);p.line=m8_ui_mix(p.foreground,p.page,.16f);
    p.hover=m8_ui_mix(p.foreground,p.page,.05f);p.selected=m8_ui_mix(p.accent,p.page,.14f);return p;
}
const char *m8_ui_theme_name(M8KeypadTheme theme) {
    static const char *const names[]={"light","dark","soft","teal","slate"};
    return names[(unsigned)theme<M8_KEYPAD_THEME_COUNT?theme:0];
}
static void color(m8_text_ui *ui,SDL_Color c){SDL_SetRenderDrawColor(ui->renderer,c.r,c.g,c.b,c.a);}
bool m8_ui_begin(m8_text_ui *ui,SDL_Renderer *renderer,M8KeypadTheme theme) {
    if(ui->renderer!=renderer){m8_ui_destroy(ui);ui->renderer=renderer;}
    int w,h;SDL_Window *window=SDL_GetRenderWindow(renderer);
    if(!SDL_GetRenderOutputSize(renderer,&w,&h))return false;
    float scale=SDL_GetWindowDisplayScale(window);if(scale<=0)scale=1;
    if(!ui->font||ui->display_scale!=scale) {
        m8_text_font_destroy(ui->font);ui->font=m8_text_font_create(renderer,scale);
        if(!ui->font)return false;
    }
    ui->display_scale=scale;ui->point_scale=m8_ui_point_scale(window);
    ui->cell=m8_text_font_advance(ui->font);ui->width=w/scale;ui->height=h/scale;
    if(!ui->focus_offset)ui->focus_offset=3;
    SDL_SetRenderScale(renderer,scale,scale);SDL_SetRenderClipRect(renderer,NULL);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    ui->line_height=M8_UI_LINE;
    ui->colors=m8_ui_colors(theme);ui->count=0;color(ui,ui->colors.page);return SDL_RenderClear(renderer);
}
void m8_ui_destroy(m8_text_ui *ui){m8_text_font_destroy(ui->font);ui->font=NULL;ui->renderer=NULL;}
static uint32_t next_code(const char **text) {
    const unsigned char *p=(const unsigned char *)*text;unsigned n=1;uint32_t c=p[0];
    if(c>=0xc2&&c<0xe0){c&=31;n=2;}else if(c>=0xe0&&c<0xf0){c&=15;n=3;}
    else if(c>=0xf0&&c<0xf5){c&=7;n=4;}else if(c>=0x80){*text+=1;return '?';}
    for(unsigned i=1;i<n;i++){if((p[i]&0xc0)!=0x80){*text+=1;return '?';}c=(c<<6)|(p[i]&63);}
    *text+=n;return c;
}
float m8_ui_measure(m8_text_ui *ui,const char *text) {
    float width=0;while(*text){next_code(&text);width+=ui->cell;}return width;
}
float m8_ui_text(m8_text_ui *ui,float x,float y,const char *text,SDL_Color c) {
    y+=(ui->line_height-M8_UI_LINE)/2;
    while(*text)x=m8_text_font_glyph(ui->font,next_code(&text),x,y,c);
    return x;
}
static float wrap(m8_text_ui *ui,float x,float y,float width,const char *text,SDL_Color c,bool draw) {
    float at=x;
    while(*text) {
        const char *end=text;while(*end&&*end!=' '&&*end!='\n')next_code(&end);
        float length=0;const char *p=text;while(p<end){next_code(&p);length+=ui->cell;}
        if(at>x&&at+length>x+width){at=x;y+=M8_UI_LINE;}
        while(text<end) {
            p=text;next_code(&text);char glyph[5]={0};memcpy(glyph,p,(size_t)(text-p));
            if(at+ui->cell>x+width+.01f){at=x;y+=M8_UI_LINE;}
            if(draw)at=m8_ui_text(ui,at,y,glyph,c);else at+=ui->cell;
        }
        if(*text=='\n'){at=x;y+=M8_UI_LINE;text++;}else if(*text==' '){at+=ui->cell;text++;}
    }
    return y+M8_UI_LINE;
}
float m8_ui_wrap(m8_text_ui *ui,float x,float y,float width,const char *text,SDL_Color c) {
    return wrap(ui,x,y,width,text,c,true);
}
float m8_ui_wrap_height(m8_text_ui *ui,float width,const char *text) {
    return wrap(ui,0,0,width,text,(SDL_Color){0},false);
}
void m8_ui_fill(m8_text_ui *ui,SDL_FRect rect,SDL_Color c) {
    float scale=ui->display_scale;
    float right=roundf((rect.x+rect.w)*scale)/scale,bottom=roundf((rect.y+rect.h)*scale)/scale;
    rect.x=roundf(rect.x*scale)/scale;rect.y=roundf(rect.y*scale)/scale;
    rect.w=right-rect.x;rect.h=bottom-rect.y;color(ui,c);SDL_RenderFillRect(ui->renderer,&rect);
}
void m8_ui_box(m8_text_ui *ui,SDL_FRect r,SDL_Color c) {
    /* Filled strips give CSS 1 px borders at every display scale. */
    m8_ui_fill(ui,(SDL_FRect){r.x,r.y,r.w,1},c);m8_ui_fill(ui,(SDL_FRect){r.x,r.y+r.h-1,r.w,1},c);
    m8_ui_fill(ui,(SDL_FRect){r.x,r.y,1,r.h},c);m8_ui_fill(ui,(SDL_FRect){r.x+r.w-1,r.y,1,r.h},c);
}
void m8_ui_line(m8_text_ui *ui,float x,float y,float end,SDL_Color c){m8_ui_fill(ui,(SDL_FRect){x,y,end-x,1},c);}
void m8_ui_dashed(m8_text_ui *ui,SDL_FRect r,SDL_Color c) {
    for(float x=r.x;x<r.x+r.w;x+=6) {
        float width=SDL_min(3,r.x+r.w-x);m8_ui_fill(ui,(SDL_FRect){x,r.y,width,1},c);m8_ui_fill(ui,(SDL_FRect){x,r.y+r.h-1,width,1},c);
    }
    for(float y=r.y;y<r.y+r.h;y+=6) {
        float h=SDL_min(3,r.y+r.h-y);m8_ui_fill(ui,(SDL_FRect){r.x,y,1,h},c);m8_ui_fill(ui,(SDL_FRect){r.x+r.w-1,y,1,h},c);
    }
}
void m8_ui_target_add(m8_text_ui *ui,int id,SDL_FRect rect,bool enabled) {
    if(id<=0||ui->count==(int)SDL_arraysize(ui->targets))return;
    ui->targets[ui->count++]=(m8_ui_target){id,rect,enabled};
    if(ui->focus==id&&enabled&&ui->keyboard_focus) {
        float offset=ui->focus_offset;rect.x-=offset+1;rect.y-=offset+1;rect.w+=2*(offset+1);rect.h+=2*(offset+1);
        m8_ui_box(ui,rect,ui->colors.foreground);
    }
}
void m8_ui_button(m8_text_ui *ui,int id,float x,float y,const char *label,bool boxed,bool enabled) {
    SDL_Color c=enabled?ui->colors.foreground:m8_ui_mix(ui->colors.foreground,ui->colors.page,.4f);
    float width=m8_ui_measure(ui,label)+(boxed?22:0),height=ui->line_height+(boxed?8:0);
    m8_ui_text(ui,x+(boxed?11:0),y+(boxed?4:0),label,c);
    SDL_FRect rect={x,y,width,height};
    if(boxed) {
        SDL_Color border=ui->hover==id&&enabled?ui->colors.foreground:ui->colors.line;
        if(!enabled)border=m8_ui_mix(border,ui->colors.page,.4f);
        m8_ui_box(ui,rect,border);
    }
    m8_ui_target_add(ui,id,rect,enabled);
}
void m8_ui_danger_button(m8_text_ui *ui,int id,float x,float y,const char *label,bool boxed) {
    SDL_Color foreground=ui->colors.foreground;ui->colors.foreground=ui->colors.danger;
    m8_ui_button(ui,id,x,y,label,boxed,true);ui->colors.foreground=foreground;
    if(boxed&&ui->hover==id)m8_ui_box(ui,(SDL_FRect){x,y,m8_ui_measure(ui,label)+22,ui->line_height+8},foreground);
}
void m8_ui_link(m8_text_ui *ui,int id,float x,float y,const char *label,bool external,bool enabled) {
    SDL_Color fg=external?ui->colors.link:ui->colors.foreground;
    if(!enabled)fg=m8_ui_mix(fg,ui->colors.page,.4f);
    float end=m8_ui_text(ui,x,y,label,fg);
    SDL_Color line=external||ui->hover==id?fg:ui->colors.line;
    m8_ui_line(ui,x,y+17.5f+(ui->line_height-M8_UI_LINE)/2,end,line);
    m8_ui_target_add(ui,id,(SDL_FRect){x,y,end-x,ui->line_height},enabled);
}
static void check_frame(m8_text_ui *ui,float x,float y,SDL_Color border,SDL_Color fill) {
    /* Browser checkboxes have a 2 px corner radius. Sample only the four
     * corners at physical pixel size; straight edges stay exactly 1 CSS px. */
    float scale=ui->display_scale;x=roundf(x*scale)/scale;y=roundf(y*scale)/scale;
    m8_ui_fill(ui,(SDL_FRect){x+2,y,10,1},border);m8_ui_fill(ui,(SDL_FRect){x+2,y+13,10,1},border);
    m8_ui_fill(ui,(SDL_FRect){x,y+2,1,10},border);m8_ui_fill(ui,(SDL_FRect){x+13,y+2,1,10},border);
    m8_ui_fill(ui,(SDL_FRect){x+2,y+1,10,12},fill);m8_ui_fill(ui,(SDL_FRect){x+1,y+2,12,10},fill);
    int pixels=(int)ceilf(2*scale);
    for(int row=0;row<pixels;row++)for(int column=0;column<pixels;column++) {
        unsigned outside=0,inside=0;
        for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++) {
            float dx=2-(column+(sx+.5f)/4)/scale,dy=2-(row+(sy+.5f)/4)/scale;
            float distance=dx*dx+dy*dy;
            if(distance<=1)inside++;else if(distance<=4)outside++;
        }
        unsigned covered=inside+outside;if(!covered)continue;
        SDL_Color c={(Uint8)((border.r*outside+fill.r*inside)/covered),
            (Uint8)((border.g*outside+fill.g*inside)/covered),(Uint8)((border.b*outside+fill.b*inside)/covered),
            (Uint8)((covered*255+8)/16)};
        color(ui,c);
        for(unsigned corner=0;corner<4;corner++) {
            float px=corner&1?14-(column+1)/scale:column/scale;
            float py=corner&2?14-(row+1)/scale:row/scale;
            SDL_FRect pixel={x+px,y+py,1/scale,1/scale};SDL_RenderFillRect(ui->renderer,&pixel);
        }
    }
}
void m8_ui_check(m8_text_ui *ui,int id,float x,float y,bool checked,bool mixed) {
    SDL_Color border=ui->colors.dark?(SDL_Color){133,133,133,255}:(SDL_Color){118,118,118,255};
    SDL_Color fill=ui->colors.dark?(SDL_Color){59,59,59,255}:(SDL_Color){255,255,255,255};
    if(checked||mixed)border=fill=ui->colors.accent;
    else if(id>0&&ui->hover==id&&ui->pointer_x>=x&&ui->pointer_x<x+14&&
            ui->pointer_y>=y&&ui->pointer_y<y+14)border=ui->colors.foreground;
    check_frame(ui,x,y,border,fill);
    SDL_Color ink=(fill.r*.2126f+fill.g*.7152f+fill.b*.0722f)>150?(SDL_Color){59,59,59,255}:(SDL_Color){255,255,255,255};
    if(mixed)m8_ui_fill(ui,(SDL_FRect){x+3,y+6,8,2},ink);
    else if(checked) {
        const SDL_FPoint shape[]={{2.5f,7},{5.8f,10.3f},{11.5f,4.6f},{10.2f,3.3f},{5.8f,7.7f},{3.8f,5.7f}};
        SDL_Vertex vertices[6];for(unsigned i=0;i<6;i++)vertices[i]=(SDL_Vertex){{x+shape[i].x,y+shape[i].y},{ink.r/255.f,ink.g/255.f,ink.b/255.f,1},{0,0}};
        const int indices[]={0,1,4,0,4,5,1,2,3,1,3,4};SDL_RenderGeometry(ui->renderer,NULL,vertices,6,indices,12);
    }
    m8_ui_target_add(ui,id,(SDL_FRect){x,y,14,14},true);
}
static int hit(m8_text_ui *ui,float x,float y) {
    SDL_FPoint point={x/ui->point_scale,y/ui->point_scale};
    for(int i=ui->count-1;i>=0;i--)if(ui->targets[i].enabled&&SDL_PointInRectFloat(&point,&ui->targets[i].rect))return ui->targets[i].id;
    return 0;
}
int m8_ui_pointer(m8_text_ui *ui,float x,float y){ui->keyboard_focus=false;ui->pointer_x=x/ui->point_scale;ui->pointer_y=y/ui->point_scale;ui->hover=hit(ui,x,y);if(ui->hover)ui->focus=ui->hover;return ui->hover;}
bool m8_ui_motion(m8_text_ui *ui,float x,float y) {
    int previous=ui->hover;ui->pointer_x=x/ui->point_scale;ui->pointer_y=y/ui->point_scale;ui->hover=hit(ui,x,y);return previous!=ui->hover;
}
int m8_ui_input(m8_text_ui *ui,m8_ui_command command) {
    ui->keyboard_focus=true;int at=-1;for(int i=0;i<ui->count;i++)if(ui->targets[i].id==ui->focus)at=i;
    if(command==M8_UI_NEXT||command==M8_UI_PREVIOUS) {
        int step=command==M8_UI_NEXT?1:-1;if(at<0)at=step>0?-1:0;
        for(int n=0;n<ui->count;n++){at=(at+step+ui->count)%ui->count;if(ui->targets[at].enabled){ui->focus=ui->targets[at].id;break;}}return 0;
    }
    return at>=0&&ui->targets[at].enabled?ui->focus:0;
}
bool m8_ui_key(const SDL_KeyboardEvent *key,m8_ui_command *command) {
    switch(key->key) {
    case SDLK_TAB:*command=key->mod&SDL_KMOD_SHIFT?M8_UI_PREVIOUS:M8_UI_NEXT;return true;
    case SDLK_UP:*command=M8_UI_PREVIOUS;return true;case SDLK_DOWN:*command=M8_UI_NEXT;return true;
    case SDLK_RETURN:case SDLK_SPACE:*command=M8_UI_ACTIVATE;return true;
    case SDLK_ESCAPE:*command=M8_UI_BACK;return true;case SDLK_LEFT:*command=M8_UI_DECREASE;return true;
    case SDLK_RIGHT:*command=M8_UI_INCREASE;return true;default:return false;
    }
}
