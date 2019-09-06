/*
    Embedded Pengo arcade emulator.
*/
#include "sokol_audio.h"
#include "sokol_app.h"
#define CHIPS_IMPL
#define NAMCO_PENGO
#include "emu.h"
#include "pengo-roms.h"

/* audio streaming callback */
static void push_audio(const float* samples, int num_samples, void* user_data) {
    saudio_push(samples, num_samples);
}

/* setup the Pengo arcade machine emulator */
void emu_setup(emu_t* emu) {
    namco_init(&emu->sys, &(namco_desc_t){
        .audio_cb = push_audio,
        .audio_sample_rate = saudio_sample_rate(),
        .pixel_buffer = emu->pixels,
        .pixel_buffer_size = sizeof(emu->pixels),
        .rom_cpu_0000_0FFF = dump_ep5120_8, .rom_cpu_0000_0FFF_size = sizeof(dump_ep5120_8),
        .rom_cpu_1000_1FFF = dump_ep5121_7, .rom_cpu_1000_1FFF_size = sizeof(dump_ep5121_7),
        .rom_cpu_2000_2FFF = dump_ep5122_15, .rom_cpu_2000_2FFF_size = sizeof(dump_ep5122_15),
        .rom_cpu_3000_3FFF = dump_ep5123_14, .rom_cpu_3000_3FFF_size = sizeof(dump_ep5123_14),
        .rom_cpu_4000_4FFF = dump_ep5124_21, .rom_cpu_4000_4FFF_size = sizeof(dump_ep5124_21),
        .rom_cpu_5000_5FFF = dump_ep5125_20, .rom_cpu_5000_5FFF_size = sizeof(dump_ep5125_20),
        .rom_cpu_6000_6FFF = dump_ep5126_32, .rom_cpu_6000_6FFF_size = sizeof(dump_ep5126_32),
        .rom_cpu_7000_7FFF = dump_ep5127_31, .rom_cpu_7000_7FFF_size = sizeof(dump_ep5127_31),
        .rom_gfx_0000_1FFF = dump_ep1640_92, .rom_gfx_0000_1FFF_size = sizeof(dump_ep1640_92),
        .rom_gfx_2000_3FFF = dump_ep1695_105,.rom_gfx_2000_3FFF_size = sizeof(dump_ep1695_105),
        .rom_prom_0000_001F = dump_pr1633_78, .rom_prom_0000_001F_size = sizeof(dump_pr1633_78),
        .rom_prom_0020_041F = dump_pr1634_88, .rom_prom_0020_041F_size = sizeof(dump_pr1634_88),
        .rom_sound_0000_00FF = dump_pr1635_51, .rom_sound_0000_00FF_size = sizeof(dump_pr1635_51),
        .rom_sound_0100_01FF = dump_pr1636_70, .rom_sound_0100_01FF_size = sizeof(dump_pr1636_70)
    });
    emu->width = namco_std_display_width();
    emu->height = namco_std_display_height();
    memset(emu->pixels, 0xFF, sizeof(emu->pixels));
}

void emu_shutdown(emu_t* emu) {
    namco_discard(&emu->sys);
}

void emu_frame(emu_t* emu, double millisec) {
    namco_exec(&emu->sys, (uint32_t) (millisec * 1000));
    namco_decode_video(&emu->sys);
}

void emu_input(emu_t* emu, const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            switch (ev->key_code) {
                case SAPP_KEYCODE_RIGHT:    namco_input_set(&emu->sys, NAMCO_INPUT_P1_RIGHT); break;
                case SAPP_KEYCODE_LEFT:     namco_input_set(&emu->sys, NAMCO_INPUT_P1_LEFT); break;
                case SAPP_KEYCODE_UP:       namco_input_set(&emu->sys, NAMCO_INPUT_P1_UP); break;
                case SAPP_KEYCODE_DOWN:     namco_input_set(&emu->sys, NAMCO_INPUT_P1_DOWN); break;
                case SAPP_KEYCODE_1:        namco_input_set(&emu->sys, NAMCO_INPUT_P1_COIN); break;
                case SAPP_KEYCODE_2:        namco_input_set(&emu->sys, NAMCO_INPUT_P2_COIN); break;
                case SAPP_KEYCODE_SPACE:    namco_input_set(&emu->sys, NAMCO_INPUT_P1_BUTTON); break;
                default:                    namco_input_set(&emu->sys, NAMCO_INPUT_P1_START); break;
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code) {
                case SAPP_KEYCODE_RIGHT:    namco_input_clear(&emu->sys, NAMCO_INPUT_P1_RIGHT); break;
                case SAPP_KEYCODE_LEFT:     namco_input_clear(&emu->sys, NAMCO_INPUT_P1_LEFT); break;
                case SAPP_KEYCODE_UP:       namco_input_clear(&emu->sys, NAMCO_INPUT_P1_UP); break;
                case SAPP_KEYCODE_DOWN:     namco_input_clear(&emu->sys, NAMCO_INPUT_P1_DOWN); break;
                case SAPP_KEYCODE_1:        namco_input_clear(&emu->sys, NAMCO_INPUT_P1_COIN); break;
                case SAPP_KEYCODE_2:        namco_input_clear(&emu->sys, NAMCO_INPUT_P2_COIN); break;
                case SAPP_KEYCODE_SPACE:    namco_input_clear(&emu->sys, NAMCO_INPUT_P1_BUTTON); break;
                default:                    namco_input_clear(&emu->sys, NAMCO_INPUT_P1_START); break;
            }
            break;
        default:
            break;
    }
}
