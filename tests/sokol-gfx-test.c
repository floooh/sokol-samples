//------------------------------------------------------------------------------
//  sokol-gfx-test.c
//  NOTE: this is not only testing the public API behaviour, but also
//  accesses private functions and data. It may make sense to split
//  these into two separate tests.
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#define SOKOL_DUMMY_BACKEND
#include "sokol_gfx.h"
#include "test.h"

static void test_init_shutdown(void) {
    test("init/shutdown");
    sg_setup(&(sg_desc){0});
    T(sg_isvalid());
    sg_shutdown();
    T(!sg_isvalid());
}

static void test_pool_size(void) {
    test("pool size");
    sg_setup(&(sg_desc){
        .buffer_pool_size = 1024,
        .image_pool_size = 2048,
        .shader_pool_size = 128,
        .pipeline_pool_size = 256,
        .pass_pool_size = 64,
        .context_pool_size = 64
    });
    T(sg_isvalid());
    /* pool slot 0 is reserved (this is the "invalid slot") */
    T(_sg.pools.buffer_pool.size == 1025);
    T(_sg.pools.image_pool.size == 2049);
    T(_sg.pools.shader_pool.size == 129);
    T(_sg.pools.pipeline_pool.size == 257);
    T(_sg.pools.pass_pool.size == 65);
    T(_sg.pools.context_pool.size == 65);
    T(_sg.pools.buffer_pool.queue_top == 1024);
    T(_sg.pools.image_pool.queue_top == 2048);
    T(_sg.pools.shader_pool.queue_top == 128);
    T(_sg.pools.pipeline_pool.queue_top == 256);
    T(_sg.pools.pass_pool.queue_top == 64);
    T(_sg.pools.context_pool.queue_top == 63);  /* default context has been created already */
    sg_shutdown();
}

static void test_alloc_fail_destroy_buffers(void) {
    test("buffer alloc=>fail=>destroy");
    sg_setup(&(sg_desc){
        .buffer_pool_size = 3
    });
    T(sg_isvalid());

    sg_buffer buf[3] = { {0} };
    for (int i = 0; i < 3; i++) {
        buf[i] = sg_alloc_buffer();
        T(buf[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.buffer_pool.queue_top);
        T(sg_query_buffer_state(buf[i]) == SG_RESOURCESTATE_ALLOC);
    }
    /* the next alloc will fail because the pool is exhausted */
    sg_buffer b3 = sg_alloc_buffer();
    T(b3.id == SG_INVALID_ID);
    T(sg_query_buffer_state(b3) == SG_RESOURCESTATE_INVALID);

    /* before destroying, the resources must be either in valid or failed state */
    for (int i = 0; i < 3; i++) {
        sg_fail_buffer(buf[i]);
        T(sg_query_buffer_state(buf[i]) == SG_RESOURCESTATE_FAILED);
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_buffer(buf[i]);
        T(sg_query_buffer_state(buf[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.buffer_pool.queue_top);
    }
    sg_shutdown();
}

static void test_alloc_fail_destroy_images(void) {
    test("image alloc=>fail=>destroy");
    sg_setup(&(sg_desc){
        .image_pool_size = 3
    });
    T(sg_isvalid());

    sg_image img[3] = { {0} };
    for (int i = 0; i < 3; i++) {
        img[i] = sg_alloc_image();
        T(img[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.image_pool.queue_top);
        T(sg_query_image_state(img[i]) == SG_RESOURCESTATE_ALLOC);
    }
    /* the next alloc will fail because the pool is exhausted */
    sg_image i3 = sg_alloc_image();
    T(i3.id == SG_INVALID_ID);
    T(sg_query_image_state(i3) == SG_RESOURCESTATE_INVALID);

    /* before destroying, the resources must be either in valid or failed state */
    for (int i = 0; i < 3; i++) {
        sg_fail_image(img[i]);
        T(sg_query_image_state(img[i]) == SG_RESOURCESTATE_FAILED);
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_image(img[i]);
        T(sg_query_image_state(img[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.image_pool.queue_top);
    }
    sg_shutdown();
}

static void test_alloc_fail_destroy_shaders(void) {
    test("shader alloc=>fail=>destroy");
    sg_setup(&(sg_desc){
        .shader_pool_size = 3
    });
    T(sg_isvalid());

    sg_shader shd[3] = { {0} };
    for (int i = 0; i < 3; i++) {
        shd[i] = sg_alloc_shader();
        T(shd[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.shader_pool.queue_top);
        T(sg_query_shader_state(shd[i]) == SG_RESOURCESTATE_ALLOC);
    }
    /* the next alloc will fail because the pool is exhausted */
    sg_shader s3 = sg_alloc_shader();
    T(s3.id == SG_INVALID_ID);
    T(sg_query_shader_state(s3) == SG_RESOURCESTATE_INVALID);

    /* before destroying, the resources must be either in valid or failed state */
    for (int i = 0; i < 3; i++) {
        sg_fail_shader(shd[i]);
        T(sg_query_shader_state(shd[i]) == SG_RESOURCESTATE_FAILED);
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_shader(shd[i]);
        T(sg_query_shader_state(shd[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.shader_pool.queue_top);
    }
    sg_shutdown();
}

static void test_alloc_fail_destroy_pipelines(void) {
    test("pipeline alloc=>fail=>destroy");
    sg_setup(&(sg_desc){
        .pipeline_pool_size = 3
    });
    T(sg_isvalid());

    sg_pipeline pip[3] = { {0} };
    for (int i = 0; i < 3; i++) {
        pip[i] = sg_alloc_pipeline();
        T(pip[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.pipeline_pool.queue_top);
        T(sg_query_pipeline_state(pip[i]) == SG_RESOURCESTATE_ALLOC);
    }
    
    /* the next alloc will fail because the pool is exhausted */
    sg_pipeline p3 = sg_alloc_pipeline();
    T(p3.id == SG_INVALID_ID);
    T(sg_query_pipeline_state(p3) == SG_RESOURCESTATE_INVALID);

    /* before destroying, the resources must be either in valid or failed state */
    for (int i = 0; i < 3; i++) {
        sg_fail_pipeline(pip[i]);
        T(sg_query_pipeline_state(pip[i]) == SG_RESOURCESTATE_FAILED);
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_pipeline(pip[i]);
        T(sg_query_pipeline_state(pip[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.pipeline_pool.queue_top);
    }
    sg_shutdown();
}

static void test_alloc_fail_destroy_passes(void) {
    test("pass alloc=>fail=>destroy");
    sg_setup(&(sg_desc){
        .pass_pool_size = 3
    });
    T(sg_isvalid());

    sg_pass pass[3] = { {0} };
    for (int i = 0; i < 3; i++) {
        pass[i] = sg_alloc_pass();
        T(pass[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.pass_pool.queue_top);
        T(sg_query_pass_state(pass[i]) == SG_RESOURCESTATE_ALLOC);
    }
    /* the next alloc will fail because the pool is exhausted */
    sg_pass p3 = sg_alloc_pass();
    T(p3.id == SG_INVALID_ID);
    T(sg_query_pass_state(p3) == SG_RESOURCESTATE_INVALID);

    /* before destroying, the resources must be either in valid or failed state */
    for (int i = 0; i < 3; i++) {
        sg_fail_pass(pass[i]);
        T(sg_query_pass_state(pass[i]) == SG_RESOURCESTATE_FAILED);
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_pass(pass[i]);
        T(sg_query_pass_state(pass[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.pass_pool.queue_top);
    }
    sg_shutdown();
}

static void test_make_destroy_buffers(void) {
    test("buffer make=>destroy");
    sg_setup(&(sg_desc){
        .buffer_pool_size = 3
    });
    T(sg_isvalid());

    float data[] = { 1.0f, 2.0f, 3.0f, 4.0f };

    sg_buffer buf[3] = { {0} };
    sg_buffer_desc desc = {
        .size = sizeof(data),
        .content = data
    };
    for (int i = 0; i < 3; i++) {
        buf[i] = sg_make_buffer(&desc);
        T(buf[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.buffer_pool.queue_top);
        T(sg_query_buffer_state(buf[i]) == SG_RESOURCESTATE_VALID);
        const _sg_buffer* bufptr = _sg_lookup_buffer(&_sg.pools, buf[i].id);
        T(bufptr);
        T(bufptr->slot.id == buf[i].id);
        T(bufptr->slot.ctx_id == _sg.active_context.id);
        T(bufptr->slot.state == SG_RESOURCESTATE_VALID);
        T(bufptr->size == sizeof(data));
        T(bufptr->append_pos == 0);
        T(!bufptr->append_overflow);
        T(bufptr->type == SG_BUFFERTYPE_VERTEXBUFFER);
        T(bufptr->usage == SG_USAGE_IMMUTABLE);
        T(bufptr->update_frame_index == 0);
        T(bufptr->append_frame_index == 0);
        T(bufptr->num_slots == 1);
        T(bufptr->active_slot == 0);
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sg_make_buffer(&desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sg_destroy_buffer(buf[i]);
        T(sg_query_buffer_state(buf[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.buffer_pool.queue_top);
    }
    sg_shutdown();
}

static void test_make_destroy_images(void) {
    test("image make=>destroy");
    sg_setup(&(sg_desc){
        .image_pool_size = 3
    });
    T(sg_isvalid());

    uint32_t data[8*8] = { 0 };

    sg_image img[3] = { {0} };
    sg_image_desc desc = {
        .width = 8,
        .height = 8,
        .content.subimage[0][0] = {
            .ptr = data,
            .size = sizeof(data)
        }

    };
    for (int i = 0; i < 3; i++) {
        img[i] = sg_make_image(&desc);
        T(img[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.image_pool.queue_top);
        T(sg_query_image_state(img[i]) == SG_RESOURCESTATE_VALID);
        const _sg_image* imgptr = _sg_lookup_image(&_sg.pools, img[i].id);
        T(imgptr);
        T(imgptr->slot.id == img[i].id);
        T(imgptr->slot.ctx_id == _sg.active_context.id);
        T(imgptr->slot.state == SG_RESOURCESTATE_VALID);
        T(imgptr->type == SG_IMAGETYPE_2D);
        T(!imgptr->render_target);
        T(imgptr->width == 8);
        T(imgptr->height == 8);
        T(imgptr->depth == 1);
        T(imgptr->num_mipmaps == 1);
        T(imgptr->usage == SG_USAGE_IMMUTABLE);
        T(imgptr->pixel_format == SG_PIXELFORMAT_RGBA8);
        T(imgptr->sample_count == 1);
        T(imgptr->min_filter == SG_FILTER_NEAREST);
        T(imgptr->mag_filter == SG_FILTER_NEAREST);
        T(imgptr->wrap_u == SG_WRAP_REPEAT);
        T(imgptr->wrap_v == SG_WRAP_REPEAT);
        T(imgptr->wrap_w == SG_WRAP_REPEAT);
        T(imgptr->max_anisotropy == 1);
        T(imgptr->upd_frame_index == 0);
        T(imgptr->num_slots == 1);
        T(imgptr->active_slot == 0);
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sg_make_image(&desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sg_destroy_image(img[i]);
        T(sg_query_image_state(img[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.image_pool.queue_top);
    }
    sg_shutdown();
}

static void test_make_destroy_shaders(void) {
    test("shader make=>destroy");
    sg_setup(&(sg_desc){
        .shader_pool_size = 3
    });
    T(sg_isvalid());

    sg_shader shd[3] = { {0} };
    sg_shader_desc desc = {
        .vs.uniform_blocks[0] = {
            .size = 16
        }
    };
    for (int i = 0; i < 3; i++) {
        shd[i] = sg_make_shader(&desc);
        T(shd[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.shader_pool.queue_top);
        T(sg_query_shader_state(shd[i]) == SG_RESOURCESTATE_VALID);
        const _sg_shader* shdptr = _sg_lookup_shader(&_sg.pools, shd[i].id);
        T(shdptr);
        T(shdptr->slot.id == shd[i].id);
        T(shdptr->slot.ctx_id == _sg.active_context.id);
        T(shdptr->slot.state == SG_RESOURCESTATE_VALID);
        T(shdptr->stage[SG_SHADERSTAGE_VS].num_uniform_blocks == 1);
        T(shdptr->stage[SG_SHADERSTAGE_VS].num_images == 0);
        T(shdptr->stage[SG_SHADERSTAGE_VS].uniform_blocks[0].size == 16);
        T(shdptr->stage[SG_SHADERSTAGE_FS].num_uniform_blocks == 0);
        T(shdptr->stage[SG_SHADERSTAGE_FS].num_images == 0);
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sg_make_shader(&desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sg_destroy_shader(shd[i]);
        T(sg_query_shader_state(shd[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.shader_pool.queue_top);
    }
    sg_shutdown();
}

static void test_make_destroy_pipelines(void) {
    test("pipeline make=>destroy");
    sg_setup(&(sg_desc){
        .pipeline_pool_size = 3
    });
    T(sg_isvalid());

    sg_pipeline pip[3] = { {0} };
    sg_pipeline_desc desc = {
        .shader = sg_make_shader(&(sg_shader_desc){ 0 }),
        .layout = {
            .attrs = {
                [0] = { .name="position", .format=SG_VERTEXFORMAT_FLOAT3 },
                [1] = { .name="color0", .format=SG_VERTEXFORMAT_FLOAT4 }
            }
        },
    };
    for (int i = 0; i < 3; i++) {
        pip[i] = sg_make_pipeline(&desc);
        T(pip[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.pipeline_pool.queue_top);
        T(sg_query_pipeline_state(pip[i]) == SG_RESOURCESTATE_VALID);
        const _sg_pipeline* pipptr = _sg_lookup_pipeline(&_sg.pools, pip[i].id);
        T(pipptr);
        T(pipptr->slot.id == pip[i].id);
        T(pipptr->slot.ctx_id == _sg.active_context.id);
        T(pipptr->slot.state == SG_RESOURCESTATE_VALID);
        T(pipptr->shader == _sg_lookup_shader(&_sg.pools, desc.shader.id));
        T(pipptr->shader_id.id == desc.shader.id);
        T(pipptr->color_attachment_count == 1);
        T(pipptr->color_format == SG_PIXELFORMAT_RGBA8);
        T(pipptr->depth_format == SG_PIXELFORMAT_DEPTHSTENCIL);
        T(pipptr->sample_count == 1);
        T(pipptr->index_type == SG_INDEXTYPE_NONE);
        T(pipptr->vertex_layout_valid[0]);
        T(!pipptr->vertex_layout_valid[1]);
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sg_make_pipeline(&desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sg_destroy_pipeline(pip[i]);
        T(sg_query_pipeline_state(pip[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.pipeline_pool.queue_top);
    }
    sg_shutdown();
}

static void test_make_destroy_passes(void) {
    test("pass make=>destroy");
    sg_setup(&(sg_desc){
        .pass_pool_size = 3
    });
    T(sg_isvalid());

    sg_pass pass[3] = { {0} };

    sg_image_desc img_desc = {
        .render_target = true,
        .width = 128,
        .height = 128,
    };
    sg_pass_desc pass_desc = (sg_pass_desc){
        .color_attachments = {
            [0].image = sg_make_image(&img_desc),
            [1].image = sg_make_image(&img_desc),
            [2].image = sg_make_image(&img_desc)
        },
    };
    for (int i = 0; i < 3; i++) {
        pass[i] = sg_make_pass(&pass_desc);
        T(pass[i].id != SG_INVALID_ID);
        T((2-i) == _sg.pools.pass_pool.queue_top);
        T(sg_query_pass_state(pass[i]) == SG_RESOURCESTATE_VALID);
        const _sg_pass* passptr = _sg_lookup_pass(&_sg.pools, pass[i].id);
        T(passptr);
        T(passptr->slot.id == pass[i].id);
        T(passptr->slot.ctx_id == _sg.active_context.id);
        T(passptr->slot.state == SG_RESOURCESTATE_VALID);
        T(passptr->num_color_atts == 3);
        for (int ai = 0; ai < 3; ai++) {
            T(passptr->color_atts[ai].image == _sg_lookup_image(&_sg.pools, pass_desc.color_attachments[ai].image.id));
            T(passptr->color_atts[ai].image_id.id == pass_desc.color_attachments[ai].image.id);
        }
    }
    /* trying to create another one fails because buffer is exhausted */
    T(sg_make_pass(&pass_desc).id == SG_INVALID_ID);

    for (int i = 0; i < 3; i++) {
        sg_destroy_pass(pass[i]);
        T(sg_query_pass_state(pass[i]) == SG_RESOURCESTATE_INVALID);
        T((i+1) == _sg.pools.pass_pool.queue_top);
    }
    sg_shutdown();
}

static void test_generation_counter(void) {
    test("pool slot generation counter");
    sg_setup(&(sg_desc){
        .buffer_pool_size = 1,
    });

    static float data[] = { 1.0f, 2.0f, 3.0f, 4.0f };
    for (int i = 0; i < 64; i++) {
        sg_buffer buf = sg_make_buffer(&(sg_buffer_desc){
            .size = sizeof(data),
            .content = data
        });
        T(buf.id != SG_INVALID_ID);
        T(sg_query_buffer_state(buf) == SG_RESOURCESTATE_VALID);
        T((buf.id >> 16) == (uint32_t)(i + 1));   /* this is the generation counter */
        T(_sg_slot_index(buf.id) == 1); /* slot index should remain the same */
        sg_destroy_buffer(buf);
        T(sg_query_buffer_state(buf) == SG_RESOURCESTATE_INVALID);
    }
    sg_shutdown();
}

int main() {
    test_begin("sokol-gfx-test");
    test_init_shutdown();
    test_pool_size();
    test_alloc_fail_destroy_buffers();
    test_alloc_fail_destroy_images();
    test_alloc_fail_destroy_shaders();
    test_alloc_fail_destroy_pipelines();
    test_alloc_fail_destroy_passes();
    test_make_destroy_buffers();
    test_make_destroy_images(); 
    test_make_destroy_shaders();
    test_make_destroy_pipelines();
    test_make_destroy_passes();
    test_generation_counter();
    return test_end();
}
