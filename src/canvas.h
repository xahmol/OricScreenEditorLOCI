#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "oric.h"
#include "appstate.h"

// screenmap[] lives in overlay RAM, not main RAM: a pointer macro into
// CANVAS_REGION_BASE rather than a real array. Every existing
// screenmap[i]/&screenmap[i] call site keeps working unmodified (C
// array-indexing is pointer arithmetic either way) -- verified against
// Oscar64 directly. This is what frees the ~10KB this used to cost in
// main RAM; see CLAUDE.md "Memory Layout". Requires overlay RAM to
// already be enabled (src/main.c's boot gate enables it once, for the
// whole session, right after confirming loci_present()) -- accessing
// screenmap[] with overlay RAM disabled silently reads/writes ROM
// instead, with no error.
#define CANVAS_REGION_BASE OVERLAY_BASE
#define screenmap ((uint8_t *)CANVAS_REGION_BASE)

// Widest possible canvas row (CANVAS_MAX_SIZE / VIEWPORT_HEIGHT ~= 379,
// rounded up). canvas_resize() and select.c's cut/copy both reuse this
// single scratch buffer (mutually exclusive in time). Stays in main RAM
// (a small staging buffer, not the canvas itself) -- no overlay-RAM
// concerns here.
#define CANVAS_MAX_ROW 384
extern uint8_t canvas_rowbuf[CANVAS_MAX_ROW];

void canvas_init(void);
void canvas_clear(void);
void canvas_fill(uint8_t value);
uint8_t canvas_get(uint16_t x, uint16_t y);
void canvas_put(uint16_t x, uint16_t y, uint8_t value);
void canvas_blit(void);
void canvas_cursor_show(uint16_t x, uint16_t y);
void canvas_cursor_hide(uint16_t x, uint16_t y);
uint8_t canvas_resize(uint16_t neww, uint16_t newh);
void cursor_move_scroll(int8_t dx, int8_t dy);

// Jump the cursor + viewport directly to canvas-absolute (x, y): clamps
// to the canvas extent, centers the viewport on the target where the
// canvas is bigger than the viewport (clamped at the canvas edges),
// re-blits. Shared by src/menudata.c's goto_dialog() and src/editor.c's
// 'h' (jump to origin) case.
void canvas_goto(uint16_t x, uint16_t y);

// The modifier-attribute byte (A_STD(8)/A_ALT(9)/.../A_BLINK2HALT(15)
// bit-packing, see oric.h) for the current app.plotaltchar/plotdouble/
// plotblink: base 8, bit0=altchar, bit1=double, bit2=blink. Shared
// helper for what used to be three identical inline expressions
// (src/editor.c's 'u' key, src/select.c's Select-mode 'm' fill,
// src/write.c's FUNCT+3) -- consolidated here since canvas.h is already
// included by all three.
uint8_t modifier_attr_byte(void);

#pragma compile("canvas.c")

#endif // CANVAS_H
