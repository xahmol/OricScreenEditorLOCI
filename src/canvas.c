#include <string.h>
#include "oric.h"
#include "appstate.h"
#include "canvas.h"

uint8_t screenmap[CANVAS_MAX_SIZE];

// Scratch row buffer for canvas_resize()'s width reflow. Sized to the widest
// possible row (CANVAS_MAX_SIZE / VIEWPORT_HEIGHT ~= 303, rounded up).
// File-scope static, not stack-local: the Oscar64 software stack
// ($B200-$B3FF) is only 512 bytes.
#define CANVAS_MAX_ROW 320
static uint8_t canvas_rowbuf[CANVAS_MAX_ROW];

/**
 * Fill the entire canvas buffer (app.canvas_width * app.canvas_height
 * cells) with a single screencode/attribute value.
 *
 * @param value Screencode or attribute byte to write into every cell.
 * @return (none)
 */
void canvas_fill(uint8_t value)
{
    uint16_t total = app.canvas_width * app.canvas_height;
    for (uint16_t i = 0; i < total; i++)
        screenmap[i] = value;
}

/**
 * Fill the entire canvas buffer with blank space (CH_SPACE).
 *
 * @return (none)
 */
void canvas_clear(void)
{
    canvas_fill(CH_SPACE);
}

/**
 * Initialise the canvas to its default size, clear it, and blit the
 * viewport to screen RAM. Called once at startup.
 *
 * @return (none)
 */
void canvas_init(void)
{
    app.canvas_width  = CANVAS_WIDTH;
    app.canvas_height = CANVAS_HEIGHT;
    canvas_clear();
    canvas_blit();
}

/**
 * Read a single cell from the canvas buffer.
 *
 * @param x Canvas-relative column (0 .. app.canvas_width-1).
 * @param y Canvas-relative row (0 .. app.canvas_height-1).
 * @return Screencode/attribute byte stored at (x, y).
 */
uint8_t canvas_get(uint16_t x, uint16_t y)
{
    return screenmap[y * app.canvas_width + x];
}

/**
 * Write a single cell in the canvas buffer.
 *
 * @param x     Canvas-relative column (0 .. app.canvas_width-1).
 * @param y     Canvas-relative row (0 .. app.canvas_height-1).
 * @param value Screencode/attribute byte to store at (x, y).
 * @return (none)
 */
void canvas_put(uint16_t x, uint16_t y, uint8_t value)
{
    screenmap[y * app.canvas_width + x] = value;
}

/**
 * Copy the VIEWPORT_WIDTH x VIEWPORT_HEIGHT window of the canvas starting
 * at (app.xoffset, app.yoffset) into screen RAM ($BB80), one row at a
 * time. Bypasses OricCharWin/cwin_viewport_blit -- see CLAUDE.md "Canvas
 * architecture".
 *
 * @return (none)
 */
void canvas_blit(void)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM;
    for (uint16_t row = 0; row < VIEWPORT_HEIGHT; row++)
    {
        uint8_t *src = &screenmap[(app.yoffset + row) * app.canvas_width + app.xoffset];
        for (uint16_t col = 0; col < VIEWPORT_WIDTH; col++)
            dst[col] = src[col];
        dst += SCREEN_COLS;
    }
}

/**
 * Toggle the inverse-video bit (0x80) of a single screen-RAM cell, for
 * cursor highlighting. Operates directly on $BB80, in screen/viewport
 * coordinates (not canvas coordinates).
 *
 * @param x Screen column (0 .. VIEWPORT_WIDTH-1).
 * @param y Screen row (0 .. VIEWPORT_HEIGHT-1).
 * @return (none)
 */
void canvas_cell_invert(uint16_t x, uint16_t y)
{
    uint8_t *cell = (uint8_t *)TEXTVRAM + y * SCREEN_COLS + x;
    *cell ^= 0x80;
}

/**
 * Resize the canvas to neww x newh cells, reflowing existing rows in place
 * via canvas_rowbuf[] when the width changes. Growing height just blanks the
 * new rows (row stride doesn't change); shrinking height needs no data
 * changes (trailing rows simply become unused).
 *
 * Overlap safety for the width reflow: growing processes rows high-to-low
 * (each new row's write region starts strictly after the previous row's old
 * data ends, since neww > oldw); shrinking processes rows low-to-high (each
 * new row's read region starts at or after the previous row's new write
 * region ends, since neww < oldw). Both go through canvas_rowbuf[] so a
 * row's own read/write overlap (when neww == oldw, a no-op) is also safe.
 *
 * @param neww New canvas width in cells.
 * @param newh New canvas height in cells.
 * @return 1 if applied, 0 if neww/newh are out of range (too small for the
 *         viewport, or neww*newh exceeds CANVAS_MAX_SIZE) -- canvas unchanged.
 */
uint8_t canvas_resize(uint16_t neww, uint16_t newh)
{
    uint16_t oldw = app.canvas_width;
    uint16_t oldh = app.canvas_height;

    if (neww < VIEWPORT_WIDTH || newh < VIEWPORT_HEIGHT) return 0;
    if ((uint32_t)neww * newh > CANVAS_MAX_SIZE) return 0;

    if (neww > oldw)
    {
        for (uint16_t y = oldh; y > 0; y--)
        {
            uint16_t row = y - 1;
            memcpy(canvas_rowbuf, &screenmap[row * oldw], oldw);
            memcpy(&screenmap[row * neww], canvas_rowbuf, oldw);
            memset(&screenmap[row * neww + oldw], CH_SPACE, neww - oldw);
        }
    }
    else if (neww < oldw)
    {
        for (uint16_t row = 0; row < oldh; row++)
        {
            memcpy(canvas_rowbuf, &screenmap[row * oldw], neww);
            memcpy(&screenmap[row * neww], canvas_rowbuf, neww);
        }
    }

    if (newh > oldh)
        memset(&screenmap[oldh * neww], CH_SPACE, (uint32_t)(newh - oldh) * neww);

    app.canvas_width  = neww;
    app.canvas_height = newh;
    return 1;
}
