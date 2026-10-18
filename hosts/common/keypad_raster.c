#include "keypad_raster.h"
#include <math.h>
typedef struct { int x, y, w, h; } PixelRect;
#define M8_MIN(a,b) ((a)<(b)?(a):(b))
#define M8_MAX(a,b) ((a)>(b)?(a):(b))

/* Distance is in logical units; its one-output-pixel ramp gives analytic edge
 * coverage. All interiors are flat fills. No supersampling, shaders or blur. */
static float rounded_distance(M8KeypadRect r,float radius,float x,float y) {
    float qx=fabsf(x-r.x-r.w*.5f)-r.w*.5f+radius;
    float qy=fabsf(y-r.y-r.h*.5f)-r.h*.5f+radius;
    return hypotf(fmaxf(qx,0),fmaxf(qy,0))+fminf(fmaxf(qx,qy),0)-radius;
}
float m8_keypad_raster_distance(M8KeypadRasterShape shape,float x,float y) {
    float r=shape.radius;
    float distance=rounded_distance(shape.base,r,x,y);
    if(shape.stem.w<=0) return distance;
    distance=fminf(distance,rounded_distance(shape.stem,r,x,y));
    /* Circular cutouts round the concave waist as well as the outer corners.
     * Their boxes overlap the base/stem, so no internal seam remains. */
    float left=shape.stem.x,right=left+shape.stem.w,join=shape.base.y;
    for(unsigned side=0;side<2;side++) {
        float cx=side?right:left,ccx=cx+(side?r:-r);
        M8KeypadRect box={cx-r,join-r,2*r,2*r};
        float cutout=r-hypotf(x-ccx,y-join+r);
        float fillet=fmaxf(rounded_distance(box,0,x,y),cutout);
        distance=fminf(distance,fillet);
    }
    return distance;
}
static void blend(M8KeypadRaster *r,int x,int y,M8KeypadColor c,float distance) {
    float coverage=fminf(1,fmaxf(0,.5f-distance*r->scale));
    if(coverage<=0) return;
    uint8_t *p=r->pixels+4*((size_t)y*r->width+x);
    unsigned a=(unsigned)(coverage*c.a+.5f),b=255-a;
    p[0]=(uint8_t)((c.r*a+p[0]*b+127)/255);
    p[1]=(uint8_t)((c.g*a+p[1]*b+127)/255);
    p[2]=(uint8_t)((c.b*a+p[2]*b+127)/255);p[3]=255;
}
static PixelRect bounds(M8KeypadRaster *r,M8KeypadRect box,float padding) {
    int x=(int)floorf(r->x+(box.x-padding)*r->scale-1);
    int y=(int)floorf(r->y+(box.y-padding)*r->scale-1);
    int right=(int)ceilf(r->x+(box.x+box.w+padding)*r->scale+1);
    int bottom=(int)ceilf(r->y+(box.y+box.h+padding)*r->scale+1);
    x=M8_MAX(x,0);y=M8_MAX(y,0);
    right=M8_MIN(right,r->width);bottom=M8_MIN(bottom,r->height);
    return (PixelRect){x,y,M8_MAX(right-x,0),M8_MAX(bottom-y,0)};
}
void m8_keypad_raster_shape(M8KeypadRaster *r,M8KeypadRasterShape shape,M8KeypadColor color) {
    M8KeypadRect box=shape.base;
    if(shape.stem.w>0) {
        float bottom=fmaxf(box.y+box.h,shape.stem.y+shape.stem.h);
        box.y=fminf(box.y,shape.stem.y);box.h=bottom-box.y;
    }
    PixelRect clip=bounds(r,box,0);
    for(int y=clip.y;y<clip.y+clip.h;y++) {
        float ly=(y+.5f-r->y)/r->scale;
        for(int x=clip.x;x<clip.x+clip.w;x++) {
            float lx=(x+.5f-r->x)/r->scale;
            blend(r,x,y,color,m8_keypad_raster_distance(shape,lx,ly));
        }
    }
}
static float segment_distance(M8KeypadPoint a,M8KeypadPoint b,float x,float y) {
    float vx=b.x-a.x,vy=b.y-a.y,denominator=vx*vx+vy*vy;
    float t=denominator>0?((x-a.x)*vx+(y-a.y)*vy)/denominator:0;
    t=fminf(1,fmaxf(0,t));return hypotf(x-a.x-t*vx,y-a.y-t*vy);
}
void m8_keypad_raster_stroke(void *context,const M8KeypadPoint *points,unsigned count,
                      bool closed,float width,M8KeypadColor color) {
    M8KeypadRaster *r=context;if(count<2||width<=0) return;
    float left=points[0].x,right=left,top=points[0].y,bottom=top;
    for(unsigned i=1;i<count;i++) {
        left=fminf(left,points[i].x);right=fmaxf(right,points[i].x);
        top=fminf(top,points[i].y);bottom=fmaxf(bottom,points[i].y);
    }
    PixelRect clip=bounds(r,(M8KeypadRect){left,top,right-left,bottom-top},width*.5f);
    unsigned segments=closed?count:count-1;
    for(int y=clip.y;y<clip.y+clip.h;y++) {
        float ly=(y+.5f-r->y)/r->scale;
        for(int x=clip.x;x<clip.x+clip.w;x++) {
            float lx=(x+.5f-r->x)/r->scale,distance=INFINITY;
            for(unsigned i=0;i<segments;i++)
                distance=fminf(distance,segment_distance(points[i],points[(i+1)%count],lx,ly));
            blend(r,x,y,color,distance-width*.5f);
        }
    }
}
/* Exact integer rectangles are reserved for the pinned M8 bitmap glyphs and
 * matching pixel icons. They intentionally bypass shape anti-aliasing. */
void m8_keypad_raster_pixels(void *context,int x,int y,int w,int h,M8KeypadColor c) {
    M8KeypadRaster *r=context;
    int right=M8_MIN(x+w,r->width),bottom=M8_MIN(y+h,r->height);
    for(int row=M8_MAX(y,0);row<bottom;row++)
        for(int column=M8_MAX(x,0);column<right;column++) {
            uint8_t *p=r->pixels+4*((size_t)row*r->width+column);
            p[0]=c.r;p[1]=c.g;p[2]=c.b;p[3]=255;
        }
}
