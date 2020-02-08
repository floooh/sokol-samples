/* macOS+Metal specific WGPU demo scaffold functions */
#if !defined(__APPLE__)
#error "please compile this file on macOS!"
#endif

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <dawn_native/DawnNative.h>
#include <dawn/dawn_proc.h>
#include <dawn/dawn_wsi.h>
#include <dawn/webgpu_cpp.h>

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

static dawn_native::Instance* dawn_instance;

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
        initWithContentRect:NSMakeRect(0, 0, wgpu_state.width, wgpu_state.height)
        styleMask:style
        backing:NSBackingStoreBuffered
        defer:NO];
    [window setTitle:[NSString stringWithUTF8String:wgpu_state.desc.title]];
    [window setAcceptsMouseMovedEvents:YES];
    [window center];
    [window setRestorable:YES];
    [window setDelegate:window_delegate];

    // view delegate, MTKView and Metal device
/*
    mtk_view_delegate = [[WGPUViewDelegate alloc] init];
    mtl_device = MTLCreateSystemDefaultDevice();
    mtk_view = [[WGPUMTKView alloc] init];
    [mtk_view setPreferredFramesPerSecond:60];
    [mtk_view setDelegate:mtk_view_delegate];
    [mtk_view setDevice: mtl_device];
    [mtk_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
    [mtk_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    [mtk_view setSampleCount:wgpu_state.desc.sample_count];
    [window setContentView:mtk_view];
    CGSize drawable_size = { (CGFloat)wgpu_state.width, (CGFloat)wgpu_state.height };
    [mtk_view setDrawableSize:drawable_size];
    [[mtk_view layer] setMagnificationFilter:kCAFilterNearest];
    [window makeKeyAndOrderFront:nil];
*/

    // FIXME: setup WGPU device and swap chain
    DawnProcTable procs = dawn_native::GetProcs();
    dawnProcSetProcs(&procs);
    dawn_instance = new dawn_native::Instance();
    dawn_instance->DiscoverDefaultAdapters();
    dawn_native::Adapter backendAdapter;
    std::vector<dawn_native::Adapter> adapters = dawn_instance->GetAdapters();
    for (const auto& a: adapters) {
        wgpu::AdapterProperties props;
        a.GetProperties(&props);
        if (props.backendType == wgpu::BackendType::Metal) {
            backendAdapter = a;
            break;
        }
    }
    assert(backendAdapter);
    wgpu_state.dev = backendAdapter.CreateDevice();

    // setup swap chain
    DawnSwapChainImplementation swapchain_impl = {
        .Init = swapchain_init,
        .Destroy = swapchain_destroy,
        .Configure = swapchain_configure,
        .GetNextTexture = swapchain_get_next_texture,
        .Present = swapchain_present
    };

    wgpu_state.desc.init_cb(wgpu_state.dev, wgpu_state.swap);
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
        wgpu_state.desc.frame_cb();
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
    if (wgpu_state.desc.mouse_btn_down_cb) {
        wgpu_state.desc.mouse_btn_down_cb(0);
    }
}

- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent*)event {
    if (wgpu_state.desc.mouse_btn_up_cb) {
        wgpu_state.desc.mouse_btn_up_cb(0);
    }
}

- (void)mouseMoved:(NSEvent*)event {
    if (wgpu_state.desc.mouse_pos_cb) {
        const NSRect content_rect = [mtk_view frame];
        const NSPoint pos = [event locationInWindow];
        wgpu_state.desc.mouse_pos_cb(pos.x, content_rect.size.height - pos.y);
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    if (wgpu_state.desc.mouse_btn_down_cb) {
        wgpu_state.desc.mouse_btn_down_cb(1);
    }
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent*)event {
    if (wgpu_state.desc.mouse_btn_up_cb) {
        wgpu_state.desc.mouse_btn_up_cb(1);
    }
}

- (void)keyDown:(NSEvent*)event {
    if (wgpu_state.desc.key_down_cb) {
        wgpu_state.desc.key_down_cb([event keyCode]);
    }
    if (wgpu_state.desc.char_cb) {
        const NSString* characters = [event characters];
        const NSUInteger length = [characters length];
        for (NSUInteger i = 0; i < length; i++) {
            const unichar codepoint = [characters characterAtIndex:i];
            if ((codepoint & 0xFF00) == 0xF700) {
                continue;
            }
            wgpu_state.desc.char_cb(codepoint);
        }
    }
}

- (void)flagsChanged:(NSEvent*)event {
    if (wgpu_state.desc.key_up_cb) {
        wgpu_state.desc.key_up_cb([event keyCode]);
    }
}

- (void)keyUp:(NSEvent*)event {
    if (wgpu_state.desc.key_up_cb) {
        wgpu_state.desc.key_up_cb([event keyCode]);
    }
}

- (void)scrollWheel:(NSEvent*)event {
    if (wgpu_state.desc.mouse_wheel_cb) {
        double dy = [event scrollingDeltaY];
        if ([event hasPreciseScrollingDeltas]) {
            dy *= 0.1;
        }
        wgpu_state.desc.mouse_wheel_cb(dy);
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
