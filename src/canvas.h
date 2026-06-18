#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>
#include "appstate.h"

extern uint8_t screenmap[CANVAS_MAX_SIZE];

// Widest possible canvas row (CANVAS_MAX_SIZE / VIEWPORT_HEIGHT ~= 303,
// rounded up). canvas_resize() and select.c's cut/copy both reuse this
// single scratch buffer (mutually exclusive in time).
#define CANVAS_MAX_ROW 320
extern uint8_t canvas_rowbuf[CANVAS_MAX_ROW];

void canvas_init(void);
void canvas_clear(void);
void canvas_fill(uint8_t value);
uint8_t canvas_get(uint16_t x, uint16_t y);
void canvas_put(uint16_t x, uint16_t y, uint8_t value);
void canvas_blit(void);
void canvas_cell_invert(uint16_t x, uint16_t y);
uint8_t canvas_resize(uint16_t neww, uint16_t newh);
void cursor_move_scroll(int8_t dx, int8_t dy);

#pragma compile("canvas.c")

#endif // CANVAS_H
