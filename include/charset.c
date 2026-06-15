// charset.c - Generic Oric charset-bank addressing and copy primitives
//
// See charset.h for the design rationale.

#include <string.h>
#include "oric.h"
#include "charset.h"

uint16_t charset_address(uint8_t screencode, uint8_t altorstd)
{
    uint16_t base = (altorstd == 0) ? CHARSET_STD : CHARSET_ALT;
    return base + (uint16_t)screencode * 8;
}

void charset_save(uint16_t base, uint8_t *dest)
{
    memcpy(dest, (uint8_t *)(base + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
}

void charset_load(uint16_t base, const uint8_t *src)
{
    memcpy((uint8_t *)(base + CHARSET_GLYPH_AREA_OFFSET), src, CHARSET_GLYPH_AREA_SIZE);
}

const uint8_t *charset_rom_glyph(uint8_t screencode)
{
    return (const uint8_t *)(CHARSETROM + (uint16_t)(screencode - 0x20) * 8);
}

// Bitmask covering the CHARSET_GLYPH_WIDTH visible bits of a glyph row.
#define CHARSET_GLYPH_MASK   ((1 << CHARSET_GLYPH_WIDTH) - 1)
// Leftmost-bit (bit5) value.
#define CHARSET_GLYPH_MSB    (1 << (CHARSET_GLYPH_WIDTH - 1))

void charset_glyph_invert(volatile uint8_t *glyph)
{
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++) glyph[y] ^= CHARSET_GLYPH_MASK;
}

void charset_glyph_mirror_v(volatile uint8_t *glyph)
{
    uint8_t buf[CHARSET_GLYPH_BYTES];
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++) buf[y] = glyph[CHARSET_GLYPH_BYTES - 1 - y];
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++) glyph[y] = buf[y];
}

void charset_glyph_mirror_h(volatile uint8_t *glyph)
{
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++)
    {
        uint8_t present = glyph[y];
        uint8_t v = 0;
        uint8_t bx;
        for (bx = 0; bx < CHARSET_GLYPH_WIDTH; bx++)
            if (present & (1 << (CHARSET_GLYPH_WIDTH - 1 - bx))) v |= (uint8_t)(1 << bx);
        glyph[y] = v;
    }
}

void charset_glyph_scroll_up(volatile uint8_t *glyph)
{
    uint8_t first = glyph[0];
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES - 1; y++) glyph[y] = glyph[y + 1];
    glyph[CHARSET_GLYPH_BYTES - 1] = first;
}

void charset_glyph_scroll_down(volatile uint8_t *glyph)
{
    uint8_t last = glyph[CHARSET_GLYPH_BYTES - 1];
    uint8_t y;
    for (y = CHARSET_GLYPH_BYTES - 1; y > 0; y--) glyph[y] = glyph[y - 1];
    glyph[0] = last;
}

void charset_glyph_rotate_left(volatile uint8_t *glyph)
{
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++)
    {
        uint8_t v = (uint8_t)(glyph[y] << 1);
        if (glyph[y] & CHARSET_GLYPH_MSB) v |= 0x01;
        glyph[y] = v & CHARSET_GLYPH_MASK;
    }
}

void charset_glyph_rotate_right(volatile uint8_t *glyph)
{
    uint8_t y;
    for (y = 0; y < CHARSET_GLYPH_BYTES; y++)
    {
        uint8_t v = glyph[y] >> 1;
        if (glyph[y] & 0x01) v |= CHARSET_GLYPH_MSB;
        glyph[y] = v;
    }
}
