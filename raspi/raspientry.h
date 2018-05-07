#pragma once
/*
    app wrapper for Raspberry Pi
*/
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void raspi_init(int sample_count);
extern void raspi_shutdown();
extern void raspi_present();
extern bool raspi_process_events();
extern int raspi_width();
extern int raspi_height();

/*
typedef void(*raspi_key_func)(int key);
typedef void(*raspi_char_func)(wchar_t c);
typedef void(*raspi_mouse_btn_func)(int btn);
typedef void(*raspi_mouse_pos_func)(float x, float y);
typedef void(*raspi_mouse_wheel_func)(float v);

extern void raspi_key_down(raspi_key_func);
extern void raspi_key_up(raspi_key_func);
extern void raspi_char(raspi_char_func);
extern void raspi_mouse_btn_down(raspi_mouse_btn_func);
extern void raspi_mouse_btn_up(raspi_mouse_btn_func);
extern void raspi_mouse_pos(raspi_mouse_pos_func);
extern void raspi_mouse_wheel(raspi_mouse_wheel_func);
*/

#ifdef __cplusplus
} /* extern "C" */
#endif
