#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#else
#import <UIKit/UIKit.h>
#endif

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "osxentry.h"

#if !TARGET_OS_IPHONE
@interface SokolApp : NSApplication
@end
@interface SokolAppDelegate : NSObject<NSApplicationDelegate>
@end
@interface SokolWindowDelegate : NSObject<NSWindowDelegate>
@end
#else
@interface SokolAppDelegate : NSObject<UIApplicationDelegate>
@end
#endif
@interface SokolViewDelegate : NSObject<MTKViewDelegate>
@end
@interface SokolMTKView : MTKView
@end

static int width;
static int height;
static int sample_count;
static const char* window_title;
static osx_init_func init_func;
static osx_frame_func frame_func;
static osx_shutdown_func shutdown_func;
static osx_key_func key_down_func;
static osx_key_func key_up_func;
static osx_char_func char_func;
static osx_mouse_btn_func mouse_btn_down_func;
static osx_mouse_btn_func mouse_btn_up_func;
static osx_mouse_pos_func mouse_pos_func;
static osx_mouse_wheel_func mouse_wheel_func;
static id window_delegate;
static id window;
static id<MTLDevice> mtl_device;
static id mtk_view_delegate;
static MTKView* mtk_view;
#if TARGET_OS_IPHONE
static id mtk_view_controller;
#endif

#if !TARGET_OS_IPHONE
//------------------------------------------------------------------------------
@implementation SokolApp
// From http://cocoadev.com/index.pl?GameKeyboardHandlingAlmost
// This works around an AppKit bug, where key up events while holding
// down the command key don't get sent to the key window.
- (void)sendEvent:(NSEvent*) event {
    if ([event type] == NSEventTypeKeyUp && ([event modifierFlags] & NSEventModifierFlagCommand)) {
        [[self keyWindow] sendEvent:event];
    }
    else {
        [super sendEvent:event];
    }
}
@end
#endif

//------------------------------------------------------------------------------
@implementation SokolAppDelegate
#if !TARGET_OS_IPHONE
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
#else
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
#endif
    // window delegate and main window
    #if TARGET_OS_IPHONE
        CGRect mainScreenBounds = [[UIScreen mainScreen] bounds];
        window = [[UIWindow alloc] initWithFrame:mainScreenBounds];
    #else
        window_delegate = [[SokolWindowDelegate alloc] init];
        const NSUInteger style =
            NSWindowStyleMaskTitled |
            NSWindowStyleMaskClosable |
            NSWindowStyleMaskMiniaturizable |
            NSWindowStyleMaskResizable;
        window = [[NSWindow alloc]
            initWithContentRect:NSMakeRect(0, 0, width, height)
            styleMask:style
            backing:NSBackingStoreBuffered
            defer:NO];
        [window setTitle:[NSString stringWithUTF8String:window_title]];
        [window setAcceptsMouseMovedEvents:YES];
        [window center];
        [window setRestorable:YES];
        [window setDelegate:window_delegate];
    #endif

    // view delegate, MTKView and Metal device
    mtk_view_delegate = [[SokolViewDelegate alloc] init];
    mtl_device = MTLCreateSystemDefaultDevice();
    mtk_view = [[SokolMTKView alloc] init];
    [mtk_view setPreferredFramesPerSecond:60];
    [mtk_view setDelegate:mtk_view_delegate];
    [mtk_view setDevice: mtl_device];
    [mtk_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
    [mtk_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    [mtk_view setSampleCount:sample_count];
    #if !TARGET_OS_IPHONE
        [window setContentView:mtk_view];
        CGSize drawable_size = { (CGFloat) width, (CGFloat) height };
        [mtk_view setDrawableSize:drawable_size];
        [[mtk_view layer] setMagnificationFilter:kCAFilterNearest];
        [window makeKeyAndOrderFront:nil];
    #else
        [mtk_view setContentScaleFactor:1.0f];
        [mtk_view setUserInteractionEnabled:YES];
        [mtk_view setMultipleTouchEnabled:YES];
        [window addSubview:mtk_view];
        mtk_view_controller = [[UIViewController<MTKViewDelegate> alloc] init];
        [mtk_view_controller setView:mtk_view];
        [window setRootViewController:mtk_view_controller];
        [window makeKeyAndVisible];
    #endif

    // call the init function
    init_func((__bridge const void*)mtl_device);
    #if TARGET_OS_IPHONE
        return YES;
    #endif
}

#if !TARGET_OS_IPHONE
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}
#endif
@end

//------------------------------------------------------------------------------
#if !TARGET_OS_IPHONE
@implementation SokolWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    shutdown_func();
    return YES;
}

- (void)windowDidResize:(NSNotification*)notification {
    // FIXME
}

- (void)windowDidMove:(NSNotification*)notification {
    // FIXME
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    // FIXME
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    // FIXME
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    // FIXME
}

- (void)windowDidResignKey:(NSNotification*)notification {
    // FIXME
}
@end
#endif

//------------------------------------------------------------------------------
@implementation SokolViewDelegate

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size {
    // FIXME
}

- (void)drawInMTKView:(nonnull MTKView*)view {
    @autoreleasepool {
        frame_func();
    }
}
@end

//------------------------------------------------------------------------------
@implementation SokolMTKView

- (BOOL) isOpaque {
    return YES;
}

#if !TARGET_OS_IPHONE 
- (BOOL)canBecomeKey {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)mouseDown:(NSEvent*)event {
    if (mouse_btn_down_func) {
        mouse_btn_down_func(0);
    }
}

- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent*)event {
    if (mouse_btn_up_func) {
        mouse_btn_up_func(0);
    }
}

- (void)mouseMoved:(NSEvent*)event {
    if (mouse_pos_func) {
        const NSRect content_rect = [mtk_view frame];
        const NSPoint pos = [event locationInWindow];
        mouse_pos_func(pos.x, content_rect.size.height - pos.y);
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    if (mouse_btn_down_func) {
        mouse_btn_down_func(1);
    }
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent*)event {
    if (mouse_btn_up_func) {
        mouse_btn_up_func(1);
    }
}

- (void)keyDown:(NSEvent*)event {
    if (key_down_func) {
        key_down_func([event keyCode]);
    }
    if (char_func) {
        const NSString* characters = [event characters];
        const NSUInteger length = [characters length];
        for (NSUInteger i = 0; i < length; i++) {
            const unichar codepoint = [characters characterAtIndex:i];
            if ((codepoint & 0xFF00) == 0xF700) {
                continue;
            }
            char_func(codepoint);
        }
    }
}

- (void)flagsChanged:(NSEvent*)event {
    if (key_up_func) {
        key_up_func([event keyCode]);
    }
}

- (void)keyUp:(NSEvent*)event {
    if (key_up_func) {
        key_up_func([event keyCode]);
    }
}

- (void)scrollWheel:(NSEvent*)event {
    if (mouse_wheel_func) {
        double dy = [event scrollingDeltaY];
        if ([event hasPreciseScrollingDeltas]) {
            dy *= 0.1;
        }
        mouse_wheel_func(dy);
    }
}
#endif
@end

//------------------------------------------------------------------------------
void osx_start(int w, int h, int smp_count, const char* title, osx_init_func ifun, osx_frame_func ffun, osx_shutdown_func sfun) {
    width = w;
    height = h;
    sample_count = smp_count;
    window_title = title;
    init_func = ifun;
    frame_func = ffun;
    shutdown_func = sfun;
    key_down_func = 0;
    key_up_func = 0;
    char_func = 0;
    mouse_btn_down_func = 0;
    mouse_btn_up_func = 0;
    mouse_pos_func = 0;
    mouse_wheel_func = 0;
    #if !TARGET_OS_IPHONE
    [SokolApp sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    id delg = [[SokolAppDelegate alloc] init];
    [NSApp setDelegate:delg];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
    #else
    @autoreleasepool {
        int argc = 0;
        char* argv[] = {};
        UIApplicationMain(argc, argv, nil, NSStringFromClass([SokolAppDelegate class]));
    }
    #endif
}

/* get an MTLRenderPassDescriptor from the MTKView */
const void* osx_mtk_get_render_pass_descriptor() {
    return (__bridge const void*) [mtk_view currentRenderPassDescriptor];
}

/* get the current CAMetalDrawable from MTKView */
const void* osx_mtk_get_drawable() {
    return (__bridge const void*) [mtk_view currentDrawable];
}

/* return current MTKView drawable width */
int osx_width() {
    return (int) [mtk_view drawableSize].width;
}

/* return current MTKView drawable height */
int osx_height() {
    return (int) [mtk_view drawableSize].height;
}

/* register input callbacks */
void osx_key_down(osx_key_func fn) {
    key_down_func = fn;
}
void osx_key_up(osx_key_func fn) {
    key_up_func = fn;
}
void osx_char(osx_char_func fn) {
    char_func = fn;
}
void osx_mouse_btn_down(osx_mouse_btn_func fn) {
    mouse_btn_down_func = fn;
}
void osx_mouse_btn_up(osx_mouse_btn_func fn) {
    mouse_btn_up_func = fn;
}
void osx_mouse_pos(osx_mouse_pos_func fn) {
    mouse_pos_func = fn;
}
void osx_mouse_wheel(osx_mouse_wheel_func fn) {
    mouse_wheel_func = fn;
}

#if defined(__OBJC__)
id<MTLDevice> osx_mtl_device() {
    return mtl_device;
}
#endif
