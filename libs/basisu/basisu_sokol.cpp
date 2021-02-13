//-----------------------------------------------------------------------------
//  basisu_sokol.cpp
//-----------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "basisu_sokol.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "basisu_transcoder.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static basist::etc1_global_selector_codebook *g_pGlobal_codebook;

void sbasisu_setup(void) {
    basist::basisu_transcoder_init();
    if (!g_pGlobal_codebook) {
        g_pGlobal_codebook = new basist::etc1_global_selector_codebook(
            basist::g_global_selector_cb_size,
            basist::g_global_selector_cb);
    }
}

void sbasisu_shutdown(void) {
    if (g_pGlobal_codebook) {
        delete g_pGlobal_codebook;
        g_pGlobal_codebook = nullptr;
    }
}

static basist::transcoder_texture_format select_basis_textureformat(bool has_alpha) {
    if (has_alpha && sg_query_pixelformat(SG_PIXELFORMAT_BC3_RGBA).sample) {
        return basist::transcoder_texture_format::cTFBC3_RGBA;
    }
    else if (!has_alpha && sg_query_pixelformat(SG_PIXELFORMAT_BC1_RGBA).sample) {
        return basist::transcoder_texture_format::cTFBC1_RGB;
    }
    else if (sg_query_pixelformat(SG_PIXELFORMAT_PVRTC_RGB_4BPP).sample) {
        return basist::transcoder_texture_format::cTFPVRTC1_4_RGB;
    }
    else {
        return basist::transcoder_texture_format::cTFETC2_RGBA;
    }
}

static sg_pixel_format basis_to_sg_pixelformat(basist::transcoder_texture_format fmt) {
    switch (fmt) {
        case basist::transcoder_texture_format::cTFBC1_RGB: return SG_PIXELFORMAT_BC1_RGBA;
        case basist::transcoder_texture_format::cTFPVRTC1_4_RGB: return SG_PIXELFORMAT_PVRTC_RGB_4BPP;
        case basist::transcoder_texture_format::cTFETC2_RGBA: return SG_PIXELFORMAT_ETC2_RGB8;
        case basist::transcoder_texture_format::cTFBC3_RGBA: return SG_PIXELFORMAT_BC3_RGBA;
        default: return _SG_PIXELFORMAT_DEFAULT;
    }
}

sg_image_desc sbasisu_transcode(const void* data, uint32_t num_bytes) {
    assert(g_pGlobal_codebook);
    basist::basisu_transcoder transcoder(g_pGlobal_codebook);
    transcoder.start_transcoding(data, num_bytes);

    basist::basisu_image_info img_info;
    transcoder.get_image_info(data, num_bytes, img_info, 0);
    basist::transcoder_texture_format fmt = select_basis_textureformat(img_info.m_alpha_flag);

    sg_image_desc desc = { };
    desc.type = SG_IMAGETYPE_2D;
    desc.width = (int) img_info.m_width;
    desc.height = (int) img_info.m_height;
    desc.num_mipmaps = (int) img_info.m_total_levels;
    assert(desc.num_mipmaps <= SG_MAX_MIPMAPS);
    desc.usage = SG_USAGE_IMMUTABLE;
    desc.min_filter = (desc.num_mipmaps > 1) ? SG_FILTER_LINEAR_MIPMAP_LINEAR : SG_FILTER_LINEAR;
    desc.mag_filter = SG_FILTER_LINEAR;
    desc.max_anisotropy = 8;
    desc.pixel_format = basis_to_sg_pixelformat(fmt);
    for (int i = 0; i < desc.num_mipmaps; i++) {
        uint32_t bytes_per_block = basist::basis_get_bytes_per_block_or_pixel(fmt);
        uint32_t orig_width, orig_height, total_blocks;
        transcoder.get_image_level_desc(data, num_bytes, 0, i, orig_width, orig_height, total_blocks);
        uint32_t required_size = total_blocks * bytes_per_block;
        if (fmt == basist::transcoder_texture_format::cTFPVRTC1_4_RGB) {
            // For PVRTC1, Basis only writes (or requires) total_blocks * bytes_per_block. But GL requires extra padding for very small textures:
            // https://www.khronos.org/registry/OpenGL/extensions/IMG/IMG_texture_compression_pvrtc.txt
            const uint32_t width = (orig_width + 3) & ~3;
            const uint32_t height = (orig_height + 3) & ~3;
            required_size = (std::max(8U, width) * std::max(8U, height) * 4 + 7) / 8;
        }

        void* ptr = malloc(required_size);
        desc.data.subimage[0][i].ptr = ptr;
        desc.data.subimage[0][i].size = required_size;
        bool res = transcoder.transcode_image_level(
            data,
            num_bytes,
            0,          // image index
            i,          // level index
            ptr,
            required_size / bytes_per_block,
            fmt,
            0);          // decode_flags
        assert(res); (void)res;
    }
    return desc;
}

void sbasisu_free(const sg_image_desc* desc) {
    assert(desc);
    for (int i = 0; i < desc->num_mipmaps; i++) {
        if (desc->data.subimage[0][i].ptr) {
            free((void*)desc->data.subimage[0][i].ptr);
        }
    }
}
