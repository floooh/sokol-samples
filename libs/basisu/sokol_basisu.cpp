//-----------------------------------------------------------------------------
//  basisu_sokol.cpp
//-----------------------------------------------------------------------------
#ifdef __APPLE__
    #include <TargetConditionals.h>
#endif
#if !(TARGET_OS_IPHONE || defined(__EMSCRIPTEN__) || defined(__ANDROID__))
    #define BASISD_SUPPORT_BC7 (0)
    #define BASISD_SUPPORT_ATC (1)
#endif
#if defined(__ANDROID__)
    #define BASISD_SUPPORT_PVRTC2 (0)
#endif
#define BASISU_NO_ITERATOR_DEBUG_LEVEL (1)
#include "basisu_transcoder.cpp"
#include "sokol_gfx.h"
#include "sokol_basisu.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
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
    } else if (!has_alpha && sg_query_pixelformat(SG_PIXELFORMAT_BC1_RGBA).sample) {
        return basist::transcoder_texture_format::cTFBC1_RGB;
    } else if (has_alpha && sg_query_pixelformat(SG_PIXELFORMAT_ETC2_RGBA8).sample) {
        return basist::transcoder_texture_format::cTFETC2_RGBA;
    } else if (!has_alpha && sg_query_pixelformat(SG_PIXELFORMAT_ETC2_RGB8).sample) {
        return basist::transcoder_texture_format::cTFETC1_RGB;
    } else {
        // UGH...
        return basist::transcoder_texture_format::cTFRGBA32;
    }
}

static sg_pixel_format basis_to_sg_pixelformat(basist::transcoder_texture_format fmt) {
    switch (fmt) {
        case basist::transcoder_texture_format::cTFBC3_RGBA: return SG_PIXELFORMAT_BC3_RGBA;
        case basist::transcoder_texture_format::cTFBC1_RGB: return SG_PIXELFORMAT_BC1_RGBA;
        case basist::transcoder_texture_format::cTFETC2_RGBA: return SG_PIXELFORMAT_ETC2_RGBA8;
        case basist::transcoder_texture_format::cTFETC1_RGB: return SG_PIXELFORMAT_ETC2_RGB8;
        case basist::transcoder_texture_format::cTFRGBA32: return SG_PIXELFORMAT_RGBA8;
        default: return _SG_PIXELFORMAT_DEFAULT;
    }
}

sg_image sbasisu_make_image(sg_range basisu_data) {
    assert(g_pGlobal_codebook);
    basist::basisu_transcoder transcoder(g_pGlobal_codebook);
    transcoder.start_transcoding(basisu_data.ptr, (uint32_t)basisu_data.size);

    basist::basisu_image_info img_info;
    transcoder.get_image_info(basisu_data.ptr, (uint32_t)basisu_data.size, img_info, 0);
    const basist::transcoder_texture_format fmt = select_basis_textureformat(img_info.m_alpha_flag);

    sg_image_desc desc = { };
    desc.usage.write_unsealed = true;
    desc.type = SG_IMAGETYPE_2D;
    desc.width = (int) img_info.m_width;
    desc.height = (int) img_info.m_height;
    desc.num_mipmaps = (int) img_info.m_total_levels;
    assert(desc.num_mipmaps <= SG_MAX_MIPMAPS);
    desc.pixel_format = basis_to_sg_pixelformat(fmt);
    sg_image img = sg_make_image(&desc);
    for (int mip_level = 0; mip_level < desc.num_mipmaps; mip_level++) {
        const uint32_t bytes_per_block = basist::basis_get_bytes_per_block_or_pixel(fmt);
        uint32_t orig_width, orig_height, total_blocks;
        transcoder.get_image_level_desc(basisu_data.ptr, (uint32_t)basisu_data.size, 0, mip_level, orig_width, orig_height, total_blocks);
        uint32_t required_size = total_blocks * bytes_per_block;
        void* ptr = malloc(required_size);
        bool res = transcoder.transcode_image_level(
            basisu_data.ptr,
            (uint32_t)basisu_data.size,
            0,          // image index
            mip_level,  // level index
            ptr,
            required_size / bytes_per_block,
            fmt,
            0);          // decode_flags
        assert(res); (void)res;
        sg_write_image_desc write_desc = { };
        write_desc.dst.image = img;
        write_desc.dst.mip_level = mip_level;
        write_desc.src.data.ptr = ptr;
        write_desc.src.data.size = required_size;
        sg_write_image_unsealed(&write_desc);
        free(ptr);
    }
    sg_seal_image(img);
    return img;
}

sg_pixel_format sbasisu_pixelformat(bool has_alpha) {
    return basis_to_sg_pixelformat(select_basis_textureformat(has_alpha));
}
