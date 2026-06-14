#include "oric.h"
#include "appstate.h"
#include "canvas.h"

uint8_t screenmap[CANVAS_WIDTH * CANVAS_HEIGHT];

void canvas_clear(void)
{
    for (uint16_t i = 0; i < CANVAS_WIDTH * CANVAS_HEIGHT; i++)
        screenmap[i] = CH_SPACE;
}

void canvas_init(void)
{
    canvas_clear();
    canvas_blit();
}

uint8_t canvas_get(uint16_t x, uint16_t y)
{
    return screenmap[y * CANVAS_WIDTH + x];
}

void canvas_put(uint16_t x, uint16_t y, uint8_t value)
{
    screenmap[y * CANVAS_WIDTH + x] = value;
}

void canvas_blit(void)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM;
    for (uint16_t i = 0; i < CANVAS_WIDTH * CANVAS_HEIGHT; i++)
        dst[i] = screenmap[i];
}

void canvas_cell_invert(uint16_t x, uint16_t y)
{
    uint8_t *cell = (uint8_t *)TEXTVRAM + y * SCREEN_COLS + x;
    *cell ^= 0x80;
}
