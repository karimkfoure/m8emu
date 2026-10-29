#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include "native_window.h"

/* SDL updates its fullscreen flag after AppKit finishes the animation. Keep
 * presentation state separate so the first transition frame has no controls. */
@interface M8FullscreenObserver : NSObject {
    SDL_Window *window;
    void (*changed)(void *);
    void (*redraw)(void *);
    void *context;
}
- (id)initWithWindow:(SDL_Window *)value callback:(void (*)(void *))callback redraw:(void (*)(void *))draw context:(void *)data;
- (void)entering:(NSNotification *)notification;
- (void)entered:(NSNotification *)notification;
- (void)leaving:(NSNotification *)notification;
- (void)left:(NSNotification *)notification;
@end

@implementation M8FullscreenObserver
- (id)initWithWindow:(SDL_Window *)value callback:(void (*)(void *))callback redraw:(void (*)(void *))draw context:(void *)data {
    self=[super init];
    if(!self)return nil;
    window=value;changed=callback;redraw=draw;context=data;
    NSWindow *native=(NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,NULL);
    if(!native){[self release];return nil;}
    NSNotificationCenter *center=[NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(entering:) name:NSWindowWillEnterFullScreenNotification object:native];
    [center addObserver:self selector:@selector(entered:) name:NSWindowDidEnterFullScreenNotification object:native];
    [center addObserver:self selector:@selector(leaving:) name:NSWindowWillExitFullScreenNotification object:native];
    [center addObserver:self selector:@selector(left:) name:NSWindowDidExitFullScreenNotification object:native];
    [center addObserver:self selector:@selector(resized:) name:NSWindowDidResizeNotification object:native];
    SDL_SetBooleanProperty(SDL_GetWindowProperties(window),"m8emu.fullscreen.presentation",
        (native.styleMask&NSWindowStyleMaskFullScreen)!=0||(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0);
    return self;
}
- (void)setFullscreen:(bool)fullscreen transition:(Sint64)transition {
    SDL_PropertiesID properties=SDL_GetWindowProperties(window);
    SDL_SetBooleanProperty(properties,"m8emu.fullscreen.presentation",fullscreen);
    SDL_SetNumberProperty(properties,"m8emu.fullscreen.transition",transition);
    if(changed)changed(context);
}
- (void)entering:(NSNotification *)notification {
    (void)notification;[self setFullscreen:true transition:1];
}
- (void)entered:(NSNotification *)notification {
    (void)notification;[self setFullscreen:true transition:0];
}
- (void)leaving:(NSNotification *)notification {
    (void)notification;[self setFullscreen:true transition:-1];
}
- (void)left:(NSNotification *)notification {
    (void)notification;[self setFullscreen:false transition:0];
}
- (void)resized:(NSNotification *)notification {
    if(!SDL_GetNumberProperty(SDL_GetWindowProperties(window),"m8emu.fullscreen.transition",0))return;
    SDL_Renderer *renderer=SDL_GetRenderer(window);
    const char *driver=renderer?SDL_GetRendererName(renderer):NULL;
    if(driver&&!SDL_strcmp(driver,"metal")) {
        /* SDL deliberately postpones Cocoa resize processing until fullscreen
         * completes. Its public Metal layer otherwise retains the old drawable
         * size while AppKit changes the view. Refresh only during this interval. */
        CAMetalLayer *layer=(CAMetalLayer *)SDL_GetRenderMetalLayer(renderer);
        NSWindow *native=(NSWindow *)notification.object;
        NSSize size=native.contentView.bounds.size;
        NSSize pixels=(SDL_GetWindowFlags(window)&SDL_WINDOW_HIGH_PIXEL_DENSITY)?
            [native.contentView convertSizeToBacking:size]:size;
        if(layer&&size.height>0&&pixels.width>0&&pixels.height>0) {
            layer.contentsScale=pixels.height/size.height;
            layer.drawableSize=NSSizeToCGSize(pixels);
        }
    }
    if(redraw)redraw(context);
}
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}
@end
void *m8_native_fullscreen_watch(SDL_Window *window,void (*changed)(void *),void (*redraw)(void *),void *context) {
    return [[M8FullscreenObserver alloc] initWithWindow:window callback:changed redraw:redraw context:context];
}
void m8_native_fullscreen_unwatch(void *observer) {
    [(M8FullscreenObserver *)observer release];
}
