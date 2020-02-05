/* macOS+Metal specific WGPU demo scaffold functions */
#if !defined(__APPLE__)
#error "please compile this file on macOS!"
#endif

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "wgpu_entry.h"

@interface WGPUApp : NSApplication
@end
@interface WGPUAppDelegate : NSObject<NSApplicationDelegate>
@end
@interface WGPUWindowDelegate : NSObject<NSWindowDelegate>
@end
@interface WGPUViewDelegate : NSObject<MTKViewDelegate>
@end
@interface WGPUMTKView : MTKView
@end

static NSWindow* window;
static id window_delegate;
static id<MTLDevice> mtl_device;
static id mtk_view_delegate;
static MTKView* mtk_view;

//------------------------------------------------------------------------------
@implementation WGPUApp
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
@implementation WGPUAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
    // window delegate and main window
    window_delegate = [[WGPUWindowDelegate alloc] init];
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
    mtk_view_delegate = [[WGPUViewDelegate alloc] init];
    mtl_device = MTLCreateSystemDefaultDevice();
    mtk_view = [[WGPUMTKView alloc] init];
    [mtk_view setPreferredFramesPerSecond:60];
    [mtk_view setDelegate:mtk_view_delegate];
    [mtk_view setDevice: mtl_device];
    [mtk_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
    [mtk_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    [mtk_view setSampleCount:sample_count];
    [window setContentView:mtk_view];
    CGSize drawable_size = { (CGFloat) width, (CGFloat) height };
    [mtk_view setDrawableSize:drawable_size];
    [[mtk_view layer] setMagnificationFilter:kCAFilterNearest];
    [window makeKeyAndOrderFront:nil];

    // FIXME: setup WGPU device and swap chain
    // FIXME: call init callback
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}
@end

//------------------------------------------------------------------------------
@implementation WGPUWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    // FIXME: call shutdown callback
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
@implementation WGPUViewDelegate

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
@implementation WGPUMTKView

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
@end

void wgpu_platform_start(const wgpu_desc_t* desc) {
    [WGPUApp sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    id delg = [[WGPUAppDelegate alloc] init];
    [NSApp setDelegate:delg];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}
