#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#include "osxentry.h"

@interface SokolApp : NSApplication
@end
@interface SokolAppDelegate<NSApplicationDelegate> : NSObject
@end
@interface SokolWindowDelegate<NSWindowDelegate> : NSObject
@end
@interface SokolViewDelegate<MTKViewDelegate> : NSObject
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
static id window_delegate;
static id window;
static id<MTLDevice> mtl_device;
static id mtk_view_delegate;
static MTKView* mtk_view;

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

//------------------------------------------------------------------------------
@implementation SokolAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
    // window delegate
    window_delegate = [[SokolWindowDelegate alloc] init];

    // window
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

    // view delegate, MTKView and Metal device
    mtk_view_delegate = [[SokolViewDelegate alloc] init];
    mtl_device = MTLCreateSystemDefaultDevice();
    mtk_view = [[SokolMTKView alloc] init];
    [mtk_view setPreferredFramesPerSecond:60];
    [mtk_view setDelegate:mtk_view_delegate];
    [mtk_view setDevice: mtl_device];
    [mtk_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
    [mtk_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    CGSize drawableSize = { width, height };
    [mtk_view setDrawableSize:drawableSize];
    [mtk_view setSampleCount:sample_count];
    [window setContentView:mtk_view];
    [window makeKeyAndOrderFront:nil];

    // call the init function
    init_func(CFBridgingRetain(mtl_device));
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}
@end

//------------------------------------------------------------------------------
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

- (BOOL)canBecomeKey {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)mouseDown:(NSEvent*)event {
    // FIXME
}

- (void)mouseDragged:(NSEvent*)event {
    // FIXME
}

- (void)mouseUp:(NSEvent*)event {
    // FIXME
}

- (void)rightMouseDown:(NSEvent*)event {
    // FIXME
}

- (void)rightMouseDragged:(NSEvent*)event {
    // FIXME
}

- (void)rightMouseUp:(NSEvent*)event {
    // FIXME
}

- (void)keyDown:(NSEvent*)event {
    // FIXME
} 

- (void)flagsChanged:(NSEvent*)event {
    // FIXME
}

- (void)keyUp:(NSEvent*)event {
    // FIXME
}

- (void)scrollWheel:(NSEvent*)event {
    // FIXME
}
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
    [SokolApp sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    id delg = [[SokolAppDelegate alloc] init];
    [NSApp setDelegate:delg];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}

const void* osx_mtk_get_render_pass_descriptor() {
    return CFBridgingRetain([mtk_view currentRenderPassDescriptor]);
}

const void* osx_mtk_get_drawable() {
    return CFBridgingRetain([mtk_view currentDrawable]);
}

int osx_width() {
    return (int)[mtk_view drawableSize].width;
}

int osx_height() {
    return (int)[mtk_view drawableSize].height;
}
