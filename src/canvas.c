#include <string.h>
#include "oric.h"
#include "appstate.h"
#include "canvas.h"

// screenmap[] itself is now a pointer macro into overlay RAM -- see
// canvas.h. No array definition needed here any more (that's the main-RAM
// space this change frees up).

// Scratch row buffer for canvas_resize()'s width reflow, also reused by
// src/select.c's cut/copy (CANVAS_MAX_ROW declared in canvas.h) for the
// same overlap-safety reason -- both uses are mutually exclusive in time,
// so sharing the single buffer is safe and avoids doubling the allocation.
// Not stack-local: the Oscar64 software stack ($B200-$B3FF) is only
// 512 bytes.
uint8_t canvas_rowbuf[CANVAS_MAX_ROW];

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
 * Bounds-checks both dimensions against the real canvas extent (not just
 * VIEWPORT_WIDTH/HEIGHT), filling any phantom rows/columns beyond it with
 * CH_SPACE instead of reading past the canvas -- needed since the canvas
 * can now be smaller than the viewport in either dimension (Load Project
 * loading a genuine V1 sub-27-row project, or an interactive Screen >
 * Width/Height resize below VIEWPORT_WIDTH/HEIGHT, see
 * canvas_resize()). Without the column bound, a canvas narrower
 * than the viewport would have each displayed row spill into the next
 * row's data (screenmap[] rows are packed at stride=canvas_width, not a
 * fixed 40) -- a real bug, unlike the row bound, which canvas_resize()'s
 * own one-time blank-pad already covered for height alone.
 *
 * @return (none)
 */
void canvas_blit(void)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM;
    uint16_t realrows = (app.yoffset < app.canvas_height) ? (uint16_t)(app.canvas_height - app.yoffset) : 0;
    uint16_t realcols = (app.xoffset < app.canvas_width)  ? (uint16_t)(app.canvas_width  - app.xoffset) : 0;
    uint16_t validrows = (realrows < VIEWPORT_HEIGHT) ? realrows : VIEWPORT_HEIGHT;
    uint16_t validcols = (realcols < VIEWPORT_WIDTH)  ? realcols : VIEWPORT_WIDTH;

    for (uint16_t row = 0; row < VIEWPORT_HEIGHT; row++)
    {
        uint16_t col = 0;
        if (row < validrows)
        {
            uint8_t *src = &screenmap[(app.yoffset + row) * app.canvas_width + app.xoffset];
            for (; col < validcols; col++)
                dst[col] = src[col];
        }
        for (; col < VIEWPORT_WIDTH; col++)
            dst[col] = CH_SPACE;
        dst += SCREEN_COLS;
    }
}

/**
 * Draw the cursor preview at viewport position (x, y): app.plotscreencode
 * with the inverse-video bit set, overwriting whatever was shown there --
 * ported from V1's plotmove()/plotvisible() (cputcxy(col, row,
 * plotscreencode+128); adding 128 to an unsigned byte and XORing 0x80 are
 * the same operation). Operates directly on $BB80, in screen/viewport
 * coordinates (not canvas coordinates).
 *
 * @param x Screen column (0 .. VIEWPORT_WIDTH-1).
 * @param y Screen row (0 .. VIEWPORT_HEIGHT-1).
 * @return (none)
 */
void canvas_cursor_show(uint16_t x, uint16_t y)
{
    uint8_t *cell = (uint8_t *)TEXTVRAM + y * SCREEN_COLS + x;
    *cell = (uint8_t)(app.plotscreencode ^ 0x80);
}

/**
 * Restore the real canvas content at viewport position (x, y), undoing a
 * prior canvas_cursor_show() -- ported from V1's plotmove()/plotvisible()
 * (cputcxy(col, row, PEEK(screenmap_screenaddr(...)))).
 *
 * @param x Screen column (0 .. VIEWPORT_WIDTH-1).
 * @param y Screen row (0 .. VIEWPORT_HEIGHT-1).
 * @return (none)
 */
void canvas_cursor_hide(uint16_t x, uint16_t y)
{
    uint8_t *cell = (uint8_t *)TEXTVRAM + y * SCREEN_COLS + x;
    *cell = canvas_get((uint16_t)(x + app.xoffset), (uint16_t)(y + app.yoffset));
}

/**
 * Move app.cursor_x/cursor_y by one cell in the given direction (exactly
 * one of dx/dy non-zero, the other 0). At a viewport edge on a canvas
 * larger than the viewport, scrolls app.xoffset/yoffset instead of moving
 * the cursor (and re-blits), so every cell of an oversized canvas stays
 * reachable -- ported from V1's cursormove()/plotmove(), adapted to OSE's
 * pure-software canvas_blit() (no hardware-scroll patch step needed).
 *
 * The forward bound is `min(VIEWPORT_WIDTH/HEIGHT, canvas_width/height)
 * - 1`, not the bare viewport constant -- needed since
 * canvas_resize() (Load Project only) can produce a canvas
 * *smaller* than the viewport (V1's genuine 27-row default). Without
 * this, the cursor could move one cell past the real canvas into
 * canvas_resize()'s blank display-only padding (added so
 * canvas_blit() doesn't show stale overlay-RAM content there) and plot
 * into it -- a real edit that renders but is silently discarded on the
 * next save, since Save only ever writes canvas_width*canvas_height
 * bytes. For every canvas >= the viewport (the normal case, and the
 * only case before this session), `min()` always picks the viewport
 * constant -- identical to the previous behaviour.
 *
 * @param dx -1/0/+1 horizontal direction.
 * @param dy -1/0/+1 vertical direction.
 * @return (none)
 */
void cursor_move_scroll(int8_t dx, int8_t dy)
{
    uint16_t maxcursorx = (app.canvas_width  < VIEWPORT_WIDTH)  ? app.canvas_width  : VIEWPORT_WIDTH;
    uint16_t maxcursory = (app.canvas_height < VIEWPORT_HEIGHT) ? app.canvas_height : VIEWPORT_HEIGHT;

    if (dx < 0)
    {
        if (app.cursor_x > 0) app.cursor_x--;
        else if (app.xoffset > 0) { app.xoffset--; canvas_blit(); }
    }
    else if (dx > 0)
    {
        if (app.cursor_x < maxcursorx - 1) app.cursor_x++;
        else if (app.xoffset + VIEWPORT_WIDTH < app.canvas_width) { app.xoffset++; canvas_blit(); }
    }

    if (dy < 0)
    {
        if (app.cursor_y > 0) app.cursor_y--;
        else if (app.yoffset > 0) { app.yoffset--; canvas_blit(); }
    }
    else if (dy > 0)
    {
        if (app.cursor_y < maxcursory - 1) app.cursor_y++;
        else if (app.yoffset + VIEWPORT_HEIGHT < app.canvas_height) { app.yoffset++; canvas_blit(); }
    }
}

/**
 * Jump the cursor + viewport directly to canvas-absolute (x, y). Clamps
 * x/y to the canvas extent, then centers the viewport on the target
 * (clamped at the canvas edges so the viewport never reads outside the
 * canvas), and sets app.cursor_x/y to the target's position within the
 * resulting viewport. Always re-blits, even when the offset doesn't
 * change, since the cursor position itself may have moved.
 *
 * @param x Canvas-absolute target column.
 * @param y Canvas-absolute target row.
 * @return (none)
 */
void canvas_goto(uint16_t x, uint16_t y)
{
    uint16_t maxxoff, maxyoff;

    if (x >= app.canvas_width)  x = (uint16_t)(app.canvas_width - 1);
    if (y >= app.canvas_height) y = (uint16_t)(app.canvas_height - 1);

    maxxoff = (app.canvas_width  > VIEWPORT_WIDTH)  ? (uint16_t)(app.canvas_width  - VIEWPORT_WIDTH)  : 0;
    maxyoff = (app.canvas_height > VIEWPORT_HEIGHT) ? (uint16_t)(app.canvas_height - VIEWPORT_HEIGHT) : 0;

    app.xoffset = (x > VIEWPORT_WIDTH / 2)  ? (uint16_t)(x - VIEWPORT_WIDTH / 2)  : 0;
    app.yoffset = (y > VIEWPORT_HEIGHT / 2) ? (uint16_t)(y - VIEWPORT_HEIGHT / 2) : 0;
    if (app.xoffset > maxxoff) app.xoffset = maxxoff;
    if (app.yoffset > maxyoff) app.yoffset = maxyoff;

    app.cursor_x = (uint16_t)(x - app.xoffset);
    app.cursor_y = (uint16_t)(y - app.yoffset);

    canvas_blit();
}

/**
 * Pack the current app.plotaltchar/plotdouble/plotblink flags into the
 * matching modifier-attribute byte (A_STD(8)/A_ALT(9)/.../A_BLINK2HALT(15)
 * in oric.h: base 8, bit0=altchar, bit1=double, bit2=blink).
 *
 * @return The packed modifier-attribute byte, 8-15.
 */
uint8_t modifier_attr_byte(void)
{
    return (uint8_t)(8 | (app.plotaltchar ? 1 : 0)
                        | (app.plotdouble  ? 2 : 0)
                        | (app.plotblink   ? 4 : 0));
}

/**
 * Resize the canvas to neww x newh cells, reflowing existing rows in
 * place via canvas_rowbuf[] when the width changes. Growing height just
 * blanks the new rows (row stride doesn't change); shrinking height
 * needs no data changes (trailing rows simply become unused). No
 * VIEWPORT_WIDTH/HEIGHT floor -- only a degenerate 0-sized dimension or
 * exceeding CANVAS_MAX_SIZE is rejected.
 *
 * Used by every resize path: the interactive Screen > Width/Height
 * dialog, Load Screen/Combined's fileio_get_dimensions() prompt, and
 * fileio_load_project() (the dimensions come straight from the loaded
 * file's own header there). Originally two separate functions --
 * canvas_resize() enforced a VIEWPORT_WIDTH/HEIGHT floor (catching an
 * accidental too-small typed value) and canvas_resize() didn't
 * (needed for fileio_load_project(), since V1's own canvas was always
 * 40x27 -- VIEWPORT_HEIGHT was 27 before this port grew it to 28, see
 * CLAUDE.md "VIEWPORT_HEIGHT 27->28" -- so any genuine V1 project,
 * including the checked-in PETSCII demo, has canvas_height=27 in its
 * PJ.BIN header, which the floor rejected as "invalid" even though the
 * file was perfectly well-formed). Merged back into one function
 * 2026-06-23 (user report: the interactive dialog gave an "unsupported"
 * error shrinking below 40x28) once canvas_blit() learned to handle a
 * sub-viewport canvas in both dimensions, removing any remaining reason
 * for the floor to exist at all.
 *
 * Overlap safety for the width reflow: growing processes rows high-to-low
 * (each new row's write region starts strictly after the previous row's old
 * data ends, since neww > oldw); shrinking processes rows low-to-high (each
 * new row's read region starts at or after the previous row's new write
 * region ends, since neww < oldw). Both go through canvas_rowbuf[] so a
 * row's own read/write overlap (when neww == oldw, a no-op) is also safe.
 *
 * Blank-pads screenmap[] from row `newh` through `VIEWPORT_HEIGHT-1`
 * (using `neww` as the row stride) when `newh < VIEWPORT_HEIGHT`, for any
 * caller that doesn't immediately overwrite that range itself (e.g. a
 * Load action about to read the rest of a file into it) -- belt-and-
 * braces alongside canvas_blit()'s own row/column bounds check, not
 * strictly required by it any more, but cheap and still correct. Assumes
 * app.xoffset/yoffset are 0 at this point for the height-pad's row
 * indexing to land in the right place -- true for every V1 project (no
 * scrolling concept) and for a freshly-typed interactive resize (the
 * caller re-clamps the cursor/viewport via canvas_goto() right after).
 *
 * @param neww New canvas width in cells.
 * @param newh New canvas height in cells.
 * @return 1 if applied, 0 if neww/newh are degenerate (0) or neww*newh
 *         exceeds CANVAS_MAX_SIZE -- canvas unchanged.
 */
uint8_t canvas_resize(uint16_t neww, uint16_t newh)
{
    uint16_t oldw = app.canvas_width;
    uint16_t oldh = app.canvas_height;

    if (neww == 0 || newh == 0) return 0;
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

    if (newh < VIEWPORT_HEIGHT)
        memset(&screenmap[(uint32_t)newh * neww], CH_SPACE, (uint32_t)(VIEWPORT_HEIGHT - newh) * neww);

    return 1;
}
