//------------------------------------------------------------------------------
//  sokol-shape-test.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#define SOKOL_SHAPE_IMPL
#include "sokol_shape.h"
#include "utest.h"

#define T(b) EXPECT_TRUE(b)
#define TFLT(f0,f1,epsilon) {T(fabs((f0)-(f1))<=(epsilon));}

UTEST(sokol_shape, color4f) {
    T(sshape_color_4f(1.0f, 0.0f, 0.0f, 0.0f) == 0x000000FF);
    T(sshape_color_4f(0.0f, 1.0f, 0.0f, 0.0f) == 0x0000FF00);
    T(sshape_color_4f(0.0f, 0.0f, 1.0f, 0.0f) == 0x00FF0000);
    T(sshape_color_4f(0.0f, 0.0f, 0.0f, 1.0f) == 0xFF000000);
}

UTEST(sokol_shape, color3f) {
    T(sshape_color_3f(1.0f, 0.0f, 0.0f) == 0xFF0000FF);
    T(sshape_color_3f(0.0f, 1.0f, 0.0f) == 0xFF00FF00);
    T(sshape_color_3f(0.0f, 0.0f, 1.0f) == 0xFFFF0000);
}

UTEST(sokol_shape, color4b) {
    T(sshape_color_4b(255, 0, 0, 0) == 0x000000FF);
    T(sshape_color_4b(0, 255, 0, 0) == 0x0000FF00);
    T(sshape_color_4b(0, 0, 255, 0) == 0x00FF0000);
    T(sshape_color_4b(0, 0, 0, 255) == 0xFF000000);
}

UTEST(sokol_shape, color3b) {
    T(sshape_color_3b(255, 0, 0) == 0xFF0000FF);
    T(sshape_color_3b(0, 255, 0) == 0xFF00FF00);
    T(sshape_color_3b(0, 0, 255) == 0xFFFF0000);
}

UTEST(sokol_shape, mat4) {
    float values[16] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    };
    sshape_mat4_t m = sshape_mat4(values);
    T(m.v[0][0] == 1.0f);
    T(m.v[0][1] == 2.0f);
    T(m.v[0][2] == 3.0f);
    T(m.v[0][3] == 4.0f);
    T(m.v[1][0] == 5.0f);
    T(m.v[1][1] == 6.0f);
    T(m.v[1][2] == 7.0f);
    T(m.v[1][3] == 8.0f);
    T(m.v[2][0] == 9.0f);
    T(m.v[2][1] == 10.0f);
    T(m.v[2][2] == 11.0f);
    T(m.v[2][3] == 12.0f);
    T(m.v[3][0] == 13.0f);
    T(m.v[3][1] == 14.0f);
    T(m.v[3][2] == 15.0f);
    T(m.v[3][3] == 16.0f);
}

UTEST(sokol_shape, mat4_transpose) {
    float values[16] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    };
    sshape_mat4_t m = sshape_mat4_transpose(values);
    T(m.v[0][0] == 1.0f);
    T(m.v[1][0] == 2.0f);
    T(m.v[2][0] == 3.0f);
    T(m.v[3][0] == 4.0f);
    T(m.v[0][1] == 5.0f);
    T(m.v[1][1] == 6.0f);
    T(m.v[2][1] == 7.0f);
    T(m.v[3][1] == 8.0f);
    T(m.v[0][2] == 9.0f);
    T(m.v[1][2] == 10.0f);
    T(m.v[2][2] == 11.0f);
    T(m.v[3][2] == 12.0f);
    T(m.v[0][3] == 13.0f);
    T(m.v[1][3] == 14.0f);
    T(m.v[2][3] == 15.0f);
    T(m.v[3][3] == 16.0f);
}

UTEST(sokol_shape, plane_buffer_sizes) {
    sshape_sizes_t res;

    res = sshape_plane_buffer_sizes(1);
    T(4 == res.vertices.num);
    T(6 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);

    res = sshape_plane_buffer_sizes(2);
    T(9 == res.vertices.num);
    T(24 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);
}

UTEST(sokol_shape, box_buffer_sizes) {
    sshape_sizes_t res;

    res = sshape_box_buffer_sizes(1);
    T(24 == res.vertices.num);
    T(36 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);

    res = sshape_box_buffer_sizes(2);
    T(54 == res.vertices.num);
    T(144 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);
}

UTEST(sokol_shape, sphere_buffer_sizes) {
    sshape_sizes_t res;

    res = sshape_sphere_buffer_sizes(3, 2);
    T(12 == res.vertices.num);
    T(18 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);

    res = sshape_sphere_buffer_sizes(36, 12);
    T(481 ==  res.vertices.num);
    T(2376 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);
}

UTEST(sokol_shape, cylinder_buffer_sizes) {
    sshape_sizes_t res;

    res = sshape_cylinder_buffer_sizes(3, 1);
    T(24 == res.vertices.num);
    T(36 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);

    res = sshape_cylinder_buffer_sizes(5, 2);
    T(42 == res.vertices.num);
    T(90 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);
}

UTEST(sokol_shape, torus_buffer_sizes) {
    sshape_sizes_t res;

    res = sshape_torus_buffer_sizes(3, 3);
    T(16 == res.vertices.num);
    T(54 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);

    res = sshape_torus_buffer_sizes(4, 5);
    T(30 == res.vertices.num);
    T(120 == res.indices.num);
    T(res.vertices.num * sizeof(sshape_vertex_t) == res.vertices.size);
    T(res.indices.num * sizeof(uint16_t) == res.indices.size);
}

UTEST(sokol_shape, buffer_layout_desc) {
    const sg_buffer_layout_desc desc = sshape_buffer_layout_desc();
    T(sizeof(sshape_vertex_t) == desc.stride);
    T(0 == desc.step_func);
    T(0 == desc.step_rate);
}

UTEST(sokol_shape, attr_descs) {
    {
        const sg_vertex_attr_desc desc = sshape_position_attr_desc();
        T(offsetof(sshape_vertex_t, position) == desc.offset);
        T(SG_VERTEXFORMAT_FLOAT3 == desc.format);
        T(0 == desc.buffer_index);
    }
    {
        const sg_vertex_attr_desc desc = sshape_normal_attr_desc();
        T(offsetof(sshape_vertex_t, normal) == desc.offset);
        T(SG_VERTEXFORMAT_FLOAT3 == desc.format);
        T(0 == desc.buffer_index);
    }
    {
        const sg_vertex_attr_desc desc = sshape_texcoord_attr_desc();
        T(offsetof(sshape_vertex_t, texcoord) == desc.offset);
        T(SG_VERTEXFORMAT_FLOAT2 == desc.format);
        T(0 == desc.buffer_index);
    }
    {
        const sg_vertex_attr_desc desc = sshape_color_attr_desc();
        T(offsetof(sshape_vertex_t, color) == desc.offset);
        T(SG_VERTEXFORMAT_UBYTE4N == desc.format);
        T(0 == desc.buffer_index);
    }
}

UTEST(sokol_shape, buffer_descs_elm_range) {
    sshape_vertex_t vx[128] = { 0 };
    uint16_t ix[128] = { 0 };
    sshape_build_t state = {
        .vertices = { .buffer_ptr = vx, .buffer_size = sizeof(vx) },
        .indices = { .buffer_ptr = ix, .buffer_size = sizeof(ix) }
    };

    // build a box...
    {
        state = sshape_build_box(&state, &(sshape_box_t) {0});
        const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&state);
        const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&state);
        const sshape_element_range_t elm_range = sshape_element_range(&state);
        T(vbuf_desc.size == 24 * sizeof(sshape_vertex_t));
        T(vbuf_desc.type == SG_BUFFERTYPE_VERTEXBUFFER);
        T(vbuf_desc.usage == SG_USAGE_IMMUTABLE);
        T(vbuf_desc.content == vx);
        T(ibuf_desc.size == 36 * sizeof(sshape_index_t));
        T(ibuf_desc.type == SG_BUFFERTYPE_INDEXBUFFER);
        T(ibuf_desc.usage == SG_USAGE_IMMUTABLE);
        T(ibuf_desc.content == ix);
        T(elm_range.base_element == 0);
        T(elm_range.num_elements == 36);
    }

    // append a plane...
    {
        state = sshape_build_plane(&state, &(sshape_plane_t) {0});
        const sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&state);
        const sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&state);
        const sshape_element_range_t elm_range = sshape_element_range(&state);
        T(vbuf_desc.size == 28 * sizeof(sshape_vertex_t));
        T(vbuf_desc.type == SG_BUFFERTYPE_VERTEXBUFFER);
        T(vbuf_desc.usage == SG_USAGE_IMMUTABLE);
        T(vbuf_desc.content == vx);
        T(ibuf_desc.size == 42 * sizeof(sshape_index_t));
        T(ibuf_desc.type == SG_BUFFERTYPE_INDEXBUFFER);
        T(ibuf_desc.usage == SG_USAGE_IMMUTABLE);
        T(ibuf_desc.content == ix);
        T(elm_range.base_element == 36);
        T(elm_range.num_elements == 6);
    }
}

SOKOL_API_DECL sg_buffer_desc sshape_vertex_buffer_desc(const sshape_build_t* state);
SOKOL_API_DECL sg_buffer_desc sshape_index_buffer_desc(const sshape_build_t* state);
SOKOL_API_DECL sshape_element_range_t sshape_element_range(const sshape_build_t* state);

UTEST(sokol_shape, build_plane_defaults) {
    sshape_vertex_t vx[64] = { 0 };
    uint16_t ix[64] = { 0 };

    sshape_build_t state = {
        .vertices = { .buffer_ptr = vx, .buffer_size = sizeof(vx) },
        .indices  = { .buffer_ptr = ix, .buffer_size = sizeof(ix) }
    };
    state = sshape_build_plane(&state, &(sshape_plane_t) { 0 });

    T(state.valid);
    T(0 == state.vertices.shape_offset);
    T(4 * sizeof(sshape_vertex_t) == state.vertices.data_size);
    T(0 == state.indices.shape_offset);
    T(6 * sizeof(uint16_t) == state.indices.data_size);
    for (int i = 0; i < 4; i++) {
        T(vx[i].color = 0xFFFFFFFF);
    }
    T(ix[0] == 0);
    T(ix[1] == 1);
    T(ix[2] == 3);
    T(ix[3] == 0);
    T(ix[4] == 3);
    T(ix[5] == 2);
}

UTEST(sokol_shape, build_plane_validate) {
    sshape_vertex_t vx[64] = { 0 };
    uint16_t ix[64] = { 0 };
    const sshape_plane_t params = { 0 };

    // vertex buffer too small
    {
        sshape_build_t state = {
            .vertices = { .buffer_ptr = vx, .buffer_size = 3 * sizeof(sshape_vertex_t) },
            .indices  = { .buffer_ptr = ix, .buffer_size = sizeof(ix) }
        };
        T(!sshape_build_plane(&state, &params).valid);
    }

    // index buffer too small
    {
        sshape_build_t state = {
            .vertices = { .buffer_ptr = vx, .buffer_size = sizeof(vx) },
            .indices  = { .buffer_ptr = ix, .buffer_size = 5 * sizeof(uint16_t) }
        };
        T(!sshape_build_plane(&state, &params).valid);
    }
    // just the right size
    {
        sshape_build_t state = {
            .vertices = { .buffer_ptr = vx, .buffer_size = 4 * sizeof(sshape_vertex_t) },
            .indices  = { .buffer_ptr = ix, .buffer_size = 6 * sizeof(uint16_t) }
        };
        T(sshape_build_plane(&state, &params).valid);
    }

    // too small for two planes
    {
        sshape_build_t state = {
            .vertices = { .buffer_ptr = vx, .buffer_size = 5 * sizeof(sshape_vertex_t) },
            .indices  = { .buffer_ptr = ix, .buffer_size = 7 * sizeof(uint16_t) }
        };
        state = sshape_build_plane(&state, &params);
        T(state.valid);
        state = sshape_build_plane(&state, &params);
        T(!state.valid);
    }

    // just the right size for two planes
    {
        sshape_build_t state = {
            .vertices = { .buffer_ptr = vx, .buffer_size = 8 * sizeof(sshape_vertex_t) },
            .indices  = { .buffer_ptr = ix, .buffer_size = 12 * sizeof(uint16_t) }
        };
        state = sshape_build_plane(&state, &params);
        T(state.valid);
        T(state.vertices.shape_offset == 0);
        T(state.vertices.data_size == 4 * sizeof(sshape_vertex_t));
        T(state.indices.shape_offset == 0);
        T(state.indices.data_size == 6 * sizeof(uint16_t));
        state = sshape_build_plane(&state, &params);
        T(state.valid);
        T(state.vertices.shape_offset == 4 * sizeof(sshape_vertex_t));
        T(state.vertices.data_size == 8 * sizeof(sshape_vertex_t));
        T(state.indices.shape_offset == 6 * sizeof(uint16_t));
        T(state.indices.data_size == 12 * sizeof(uint16_t));
    }
}

UTEST(sokol_shape, build_box_defaults) {
    sshape_vertex_t vx[128] = { 0 };
    uint16_t ix[128] = { 0 };

    sshape_build_t state = {
        .vertices = { .buffer_ptr = vx, .buffer_size = sizeof(vx) },
        .indices = { .buffer_ptr = ix, .buffer_size = sizeof(ix) }
    };
    state = sshape_build_box(&state, &(sshape_box_t) { .color = 0xFF0000FF });
    T(state.valid);
    T(state.vertices.shape_offset == 0);
    T(state.vertices.data_size == 24 * sizeof(sshape_vertex_t));
    T(state.indices.shape_offset == 0);
    T(state.indices.data_size == 36 * sizeof(uint16_t));
}

UTEST(sokol_shape, build_sphere_defaults) {
    sshape_vertex_t vx[128] = { 0 };
    uint16_t ix[128] = { 0 };

    sshape_build_t state = {
        .vertices = { .buffer_ptr = vx, .buffer_size = sizeof(vx) },
        .indices = { .buffer_ptr = vx, .buffer_size = sizeof(ix) }
    };
    state = sshape_build_sphere(&state, &(sshape_sphere_t) { .color = 0xFF0000FF });
    T(state.valid);
    T(state.vertices.shape_offset == 0);
    T(state.vertices.data_size == 30 * sizeof(sshape_vertex_t));
    T(state.indices.shape_offset == 0);
    T(state.indices.data_size == 90 * sizeof(uint16_t));
}
