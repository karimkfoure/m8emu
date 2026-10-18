#include "checkpoint.h"
#include "bridge.h"
#include "render.h"
#include "fonts/fonts.h"
#include <string.h>
int m8_bridge_restore_display(const M8WebDisplay *d,int render) {
    if(!d||d->packet_size>M8_DISPLAY_PACKET_BYTES||d->escaped>1||d->discard)return -1;
    if(render) {
        uint8_t identity[]={255,0,6,5,2,(uint8_t)d->font};
        if(!process_command(identity,sizeof identity))return -1;
        const struct inline_font *font=fonts_get(d->font);if(!font)return -1;
        struct draw_rectangle_command rect={{0,0},{(uint16_t)d->width,(uint16_t)(d->height-font->screen_offset_y)},
            {(uint8_t)(d->background_color>>16),(uint8_t)(d->background_color>>8),(uint8_t)d->background_color}};
        draw_rectangle(&rect);
        struct draw_oscilloscope_waveform_command wave={0};
        wave.waveform_size=(uint16_t)d->waveform_size;draw_waveform(&wave);
        if(d->waveform_cleared){wave.waveform_size=0;draw_waveform(&wave);}
        for(unsigned y=0;y<d->height;y++)for(unsigned x=0;x<d->width;) {
            const uint8_t *p=d->pixels+((size_t)y*d->width+x)*4;
            unsigned end=x+1;
            while(end<d->width&&!memcmp(p,d->pixels+((size_t)y*d->width+end)*4,4))end++;
            rect=(struct draw_rectangle_command){{(uint16_t)x,(uint16_t)(y-font->screen_offset_y)},
                {(uint16_t)(end-x),1},{p[0],p[1],p[2]}};
            draw_rectangle(&rect);x=end;
        }
        /* Restore the compact rectangle protocol's implicit colour. */
        uint8_t color[]={254,0,0,0,0,0,0,0,0,(uint8_t)(d->rectangle_color>>16),
            (uint8_t)(d->rectangle_color>>8),(uint8_t)d->rectangle_color};
        if(!process_command(color,sizeof color))return -1;
    }
    return m8_bridge_restore_parser(d->packet,d->packet_size,d->escaped);
}
