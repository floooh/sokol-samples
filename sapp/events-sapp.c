//------------------------------------------------------------------------------
//  events-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include <stdio.h>  // printf

sg_pass_action pass_action = {
    .colors[0] = { .action=SG_ACTION_CLEAR, .val={1.0f, 0.5f, 0.0f, 1.0f} }
};

void init(void) {
    sg_setup(&(sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });
}

void print_touches(const sapp_event* e) {
    for (int i = 0; i < e->num_touches; i++) {
        const sapp_touchpoint* p = &e->touches[i];
        printf("  id:%p pos_x:%.2f pos_y:%.2f changed=%s\n",
            (void*)p->identifier, p->pos_x, p->pos_y, p->changed?"true":"false");
    }
}

void event(const sapp_event* e) {
    switch (e->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            printf("KEY_DOWN        frame_count:%d key_code:%d modifiers:%x\n", e->frame_count, e->key_code, e->modifiers);
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            printf("KEY_UP          frame_count:%d key_code:%d modifiers:%x\n", e->frame_count, e->key_code, e->modifiers);
            break;
        case SAPP_EVENTTYPE_CHAR:
            printf("CHAR            frame_count:%d char_code:%d modifiers:%x\n", e->frame_count, e->char_code, e->modifiers);
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            printf("MOUSE_DOWN      frame_count:%d mouse_button:%d modifiers:%x mouse_x:%.2f mouse_y:%.2f\n",
                e->frame_count, e->mouse_button, e->modifiers, e->mouse_x, e->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            printf("MOUSE_UP        frame_count:%d mouse_button:%d modifiers:%x mouse_x:%.2f mouse_y:%.2f\n",
                e->frame_count, e->mouse_button, e->modifiers, e->mouse_x, e->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            printf("MOUSE_SCROLL    frame_count:%d modifiers:%x scroll_x:%.2f scroll_y:%.2f\n",
                e->frame_count, e->modifiers, e->scroll_x, e->scroll_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            printf("MOUSE_MOVE      frame_count:%d modifiers:%x mouse_x:%.2f mouse_y:%.2f\n",
                e->frame_count, e->modifiers, e->mouse_x, e->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_ENTER: 
            printf("MOUSE_ENTER     frame_count:%d modifiers:%x mouse_x:%.2f mouse_y:%.2f\n",
                e->frame_count, e->modifiers, e->mouse_x, e->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_LEAVE:
            printf("MOUSE_LEAVE     frame_count:%d modifiers:%x mouse_x:%.2f mouse_y:%.2f\n",
                e->frame_count, e->modifiers, e->mouse_x, e->mouse_y);
            break;
        case SAPP_EVENTTYPE_TOUCHES_BEGAN:
            printf("TOUCHES_BEGIN       frame_count:%d num_touches:%d\n", e->frame_count, e->num_touches);
            print_touches(e);
            break;
        case SAPP_EVENTTYPE_TOUCHES_MOVED:
            printf("TOUCHES_MOVED       frame_count:%d num_touches:%d\n", e->frame_count, e->num_touches);
            print_touches(e);
            break;
        case SAPP_EVENTTYPE_TOUCHES_ENDED:
            printf("TOUCHES_ENDED       frame_count:%d num_touches:%d\n", e->frame_count, e->num_touches);
            print_touches(e);
            break;
        case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
            printf("TOUCHES_CANCELLED   frame_count:%d num_touches:%d\n", e->frame_count, e->num_touches);
            print_touches(e);
            break;
        case SAPP_EVENTTYPE_RESIZED:
            printf("RESIZED         frame_count:%d win_width:%d win_height:%d fb_width:%d fb_height:%d\n",
                e->frame_count, e->window_width, e->window_height, e->framebuffer_width, e->framebuffer_height);
            break;
        case SAPP_EVENTTYPE_ICONIFIED:
            printf("ICONIFIED       frame_count:%d win_width:%d win_height:%d fb_width:%d fb_height:%d\n",
                e->frame_count, e->window_width, e->window_height, e->framebuffer_width, e->framebuffer_height);
            break;
        case SAPP_EVENTTYPE_RESTORED:
            printf("RESTORED        frame_count:%d win_width:%d win_height:%d fb_width:%d fb_height:%d\n",
                e->frame_count, e->window_width, e->window_height, e->framebuffer_width, e->framebuffer_height);
            break;
        case SAPP_EVENTTYPE_SUSPENDED:
            printf("SUSPENDED       frame_count:%d\n", e->frame_count);
            break;
        case SAPP_EVENTTYPE_RESUMED:
            printf("RESUMED         frame_count:%d\n", e->frame_count);
            break;
        default:
            printf("Unknown event!\n");
            break;
    }
}

void frame(void) {
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .width = 400,
        .height = 300,
        .window_title = "Clear (sokol app)",
    };
}
