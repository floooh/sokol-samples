#pragma once
/*
    Declarations for embedded Pengo arcade emulator.
*/
#include <stdint.h>
#include "sokol_app.h"
#include "z80.h"
#include "clk.h"
#include "mem.h"
#include "namco.h"

#define EMU_WIDTH (288)
#define EMU_HEIGHT (224)

typedef struct {
    namco_t sys;
    uint32_t width;
    uint32_t height;
    uint32_t pixels[EMU_WIDTH * EMU_HEIGHT];
} emu_t;

void emu_setup(emu_t* emu);
void emu_shutdown(emu_t* emu);
void emu_frame(emu_t* emu, double millisec);
void emu_input(emu_t* emu, const sapp_event* e);


