#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

extern uint8_t screenmap[];

void canvas_init(void);
void canvas_clear(void);
uint8_t canvas_get(uint16_t x, uint16_t y);
void canvas_put(uint16_t x, uint16_t y, uint8_t value);
void canvas_blit(void);
void canvas_cell_invert(uint16_t x, uint16_t y);

#pragma compile("canvas.c")

#endif // CANVAS_H
