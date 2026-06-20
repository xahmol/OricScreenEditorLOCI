// select.c - Shared rectangle-grower, Line/Box and Select modes for
// OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Adapted from V1's lineandbox()/selectmode() (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c). V1 grows the rect by
// tracking which edge is "trailing" the fixed origin and redrawing only
// the single edge line that moved each keypress (plotvisible() -- which
// draws app.plotscreencode^0x80 as a preview, not a XOR-toggle of
// whatever's already there). This port instead always recomputes the
// rect as [min(origin,cursor) .. max(origin,cursor)] from scratch on
// every keypress, and redraws by clearing (restoring real content on)
// the OLD perimeter then setting (drawing the plotscreencode preview on)
// the NEW perimeter -- same visual result as V1's edge-tracking without
// needing to port its four-way trailing-edge comparison logic. When
// app.xoffset/yoffset change mid-grow (cursor_move_scroll() auto-
// scrolled, see src/canvas.c), that call already canvas_blit()s, which
// clears all stale preview bytes -- so only the new perimeter needs
// drawing in that case, not an erase pass.

#include <string.h>
#include "oric.h"
#include "keyboard.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "menu.h"
#include "strings.h"
#include "undo.h"
#include "input.h"
#include "help.h"
#include "select.h"

// Selected/grown rectangle, in absolute canvas coordinates (inclusive
// bounds) -- file-scope statics, not AppState fields: nothing outside this
// file needs to read them (Select mode's fill actions live here too).
static uint16_t select_startx, select_starty, select_endx, select_endy;
static uint16_t select_width, select_height;

// Line/Box-only: reset to 0 by rect_select() (shared start point with
// Select mode, which never reads it), toggled by 'o' in linebox_run()'s
// own post-ENTER shape prompt (not during rect_select()'s grow loop --
// see linebox_run()'s header comment) to decide fill-vs-outline. A
// file-scope static rather than a parameter/return value, same
// convention as select_startx/etc. above.
static uint8_t select_hollow;

// Line/Box-only, same convention as select_hollow above: toggled by 'c'
// in linebox_run()'s post-ENTER shape prompt to decide
// rectangle-vs-ellipse. Independent of select_hollow -- the two combine
// for a 4-way dispatch (filled/hollow box, filled/hollow ellipse).
static uint8_t select_ellipse;

/**
 * Draw the cursor preview (canvas_cursor_show()) on one canvas cell that is
 * currently within the viewport (app.xoffset/yoffset), converting from
 * canvas-absolute to viewport-relative coordinates. No-op if the cell is
 * scrolled out of view.
 *
 * @param cx Canvas-absolute column.
 * @param cy Canvas-absolute row.
 * @return (none)
 */
static void rect_show_if_visible(uint16_t cx, uint16_t cy)
{
    if (cx < app.xoffset || cx >= (uint16_t)(app.xoffset + VIEWPORT_WIDTH)) return;
    if (cy < app.yoffset || cy >= (uint16_t)(app.yoffset + VIEWPORT_HEIGHT)) return;
    canvas_cursor_show((uint16_t)(cx - app.xoffset), (uint16_t)(cy - app.yoffset));
}

/**
 * Restore real content (canvas_cursor_hide()) on one canvas cell that is
 * currently within the viewport -- the inverse of rect_show_if_visible().
 *
 * @param cx Canvas-absolute column.
 * @param cy Canvas-absolute row.
 * @return (none)
 */
static void rect_hide_if_visible(uint16_t cx, uint16_t cy)
{
    if (cx < app.xoffset || cx >= (uint16_t)(app.xoffset + VIEWPORT_WIDTH)) return;
    if (cy < app.yoffset || cy >= (uint16_t)(app.yoffset + VIEWPORT_HEIGHT)) return;
    canvas_cursor_hide((uint16_t)(cx - app.xoffset), (uint16_t)(cy - app.yoffset));
}

/**
 * Draw the cursor preview along the perimeter (border only, not the fill)
 * of the rectangle [sx..ex] x [sy..ey] (inclusive, canvas-absolute
 * coordinates), clipped to the current viewport. A single-cell rectangle
 * (sx==ex && sy==ey) shows just that one cell.
 *
 * @param sx Left column.
 * @param sy Top row.
 * @param ex Right column.
 * @param ey Bottom row.
 * @return (none)
 */
static void rect_perimeter_set(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint16_t x, y;

    for (x = sx; x <= ex; x++)
    {
        rect_show_if_visible(x, sy);
        if (ey != sy) rect_show_if_visible(x, ey);
    }
    for (y = (uint16_t)(sy + 1); y < ey; y++)
    {
        rect_show_if_visible(sx, y);
        if (ex != sx) rect_show_if_visible(ex, y);
    }
}

/**
 * Restore real content along the perimeter of the rectangle [sx..ex] x
 * [sy..ey] -- the inverse of rect_perimeter_set(), same cell walk.
 *
 * @param sx Left column.
 * @param sy Top row.
 * @param ex Right column.
 * @param ey Bottom row.
 * @return (none)
 */
static void rect_perimeter_clear(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint16_t x, y;

    for (x = sx; x <= ex; x++)
    {
        rect_hide_if_visible(x, sy);
        if (ey != sy) rect_hide_if_visible(x, ey);
    }
    for (y = (uint16_t)(sy + 1); y < ey; y++)
    {
        rect_hide_if_visible(sx, y);
        if (ex != sx) rect_hide_if_visible(ex, y);
    }
}

/**
 * Plot value along just the perimeter (border, not the fill) of the
 * rectangle [sx..ex] x [sy..ey] (inclusive, canvas-absolute coordinates)
 * -- same walk as rect_perimeter_set(), but canvas_put() instead of
 * canvas_cursor_show(), and unconditional rather than viewport-clipped
 * (this writes to the canvas model itself, not a transient preview).
 * Used by linebox_run() for the hollow-box ('o' toggle) case.
 *
 * @param sx    Left column.
 * @param sy    Top row.
 * @param ex    Right column.
 * @param ey    Bottom row.
 * @param value Screencode/attribute byte to plot along the border.
 * @return (none)
 */
static void rect_perimeter_plot(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint8_t value)
{
    uint16_t x, y;

    for (x = sx; x <= ex; x++)
    {
        canvas_put(x, sy, value);
        if (ey != sy) canvas_put(x, ey, value);
    }
    for (y = (uint16_t)(sy + 1); y < ey; y++)
    {
        canvas_put(sx, y, value);
        if (ex != sx) canvas_put(ex, y, value);
    }
}

/**
 * Test whether canvas cell (x, y) lies within the ellipse inscribed in
 * the bounding box [sx..ex] x [sy..ey] (inclusive). Uses doubled
 * coordinates (dx2 = 2*x - (sx+ex)) so the center is always exact for
 * both even and odd bounding-box dimensions, with no fractional or
 * floating-point math (this project builds with -dNOFLOAT). A
 * degenerate 1-cell-wide or -tall bounding box collapses correctly to
 * a straight line via this same test, with no special-casing needed.
 *
 * @param x  Canvas-absolute column to test.
 * @param y  Canvas-absolute row to test.
 * @param sx Left column of the bounding box.
 * @param sy Top row of the bounding box.
 * @param ex Right column of the bounding box.
 * @param ey Bottom row of the bounding box.
 * @return 1 if (x, y) is inside or on the ellipse, 0 otherwise.
 */
static uint8_t ellipse_inside(uint16_t x, uint16_t y, uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    int32_t dx2 = (int32_t)(2 * (int32_t)x - ((int32_t)sx + (int32_t)ex));
    int32_t dy2 = (int32_t)(2 * (int32_t)y - ((int32_t)sy + (int32_t)ey));
    int32_t rx2 = (int32_t)ex - (int32_t)sx;
    int32_t ry2 = (int32_t)ey - (int32_t)sy;
    int32_t dx2sq = dx2 * dx2;
    int32_t dy2sq = dy2 * dy2;
    int32_t rx2sq = rx2 * rx2;
    int32_t ry2sq = ry2 * ry2;
    return (uint8_t)(dx2sq * ry2sq + dy2sq * rx2sq <= rx2sq * ry2sq);
}

/**
 * Plot value into every cell of the ellipse inscribed in [sx..ex] x
 * [sy..ey] (inclusive, canvas-absolute). One-shot operation (called
 * once on ENTER, not live), so the straightforward O(width*height)
 * per-cell test is cheap enough on 6502 -- no need for an incremental
 * rasterization algorithm.
 *
 * @param sx    Left column of the bounding box.
 * @param sy    Top row of the bounding box.
 * @param ex    Right column of the bounding box.
 * @param ey    Bottom row of the bounding box.
 * @param value Screencode/attribute byte to plot.
 * @return (none)
 */
static void ellipse_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint8_t value)
{
    uint16_t x, y;

    for (y = sy; y <= ey; y++)
        for (x = sx; x <= ex; x++)
            if (ellipse_inside(x, y, sx, sy, ex, ey))
                canvas_put(x, y, value);
}

/**
 * Plot value along just the outline of the ellipse inscribed in
 * [sx..ex] x [sy..ey]: for each row, the leftmost and rightmost inside
 * cell; for each column, the topmost and bottommost inside cell (the
 * ellipse's convexity makes each row/column's inside cells a single
 * contiguous span, so tracking just the extremes during one forward
 * scan is sufficient -- no reverse-scan/underflow-prone loop needed).
 *
 * @param sx    Left column of the bounding box.
 * @param sy    Top row of the bounding box.
 * @param ex    Right column of the bounding box.
 * @param ey    Bottom row of the bounding box.
 * @param value Screencode/attribute byte to plot.
 * @return (none)
 */
static void ellipse_outline(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint8_t value)
{
    uint16_t x, y, leftx = sx, rightx = sx, topy = sy, boty = sy;
    uint8_t found;

    for (y = sy; y <= ey; y++)
    {
        found = 0;
        for (x = sx; x <= ex; x++)
            if (ellipse_inside(x, y, sx, sy, ex, ey))
            {
                if (!found) { leftx = x; found = 1; }
                rightx = x;
            }
        if (found)
        {
            canvas_put(leftx, y, value);
            if (rightx != leftx) canvas_put(rightx, y, value);
        }
    }

    for (x = sx; x <= ex; x++)
    {
        found = 0;
        for (y = sy; y <= ey; y++)
            if (ellipse_inside(x, y, sx, sy, ex, ey))
            {
                if (!found) { topy = y; found = 1; }
                boty = y;
            }
        if (found)
        {
            canvas_put(x, topy, value);
            if (boty != topy) canvas_put(x, boty, value);
        }
    }
}

/**
 * Grow a rectangle from the cursor's current position using cursor keys
 * (cursor_move_scroll() handles auto-scroll on oversized canvases, see
 * src/canvas.c), with a live perimeter preview of app.plotscreencode
 * (canvas_cursor_show(), matching V1's plotvisible()). ENTER accepts (populating
 * select_startx/starty/endx/endy/width/height) and returns 1; ESC cancels
 * and returns 0. FUNCT+6 toggles the statusbar without affecting the rect.
 * Sets app.mode to MODE_LINEBOX or MODE_SELECT for the duration (per
 * draworselect) and restores MODE_MAIN before returning.
 *
 * select_hollow/select_ellipse are reset to 0 here (shared start point for
 * both callers) but are no longer toggled during this grow loop -- per
 * user request 2026-06-20, linebox_run()'s own secondary prompt (shown
 * only *after* this function returns 1) is where 'o'/'c' are read, the
 * same "rect first, choice second" shape select_run()'s own
 * MSG_SELECT_ACTION_HINT prompt already uses. An earlier version of this
 * function read 'o'/'c' live during the grow loop and showed
 * MSG_LINEBOX_MODE_HINT for its entire duration -- both removed, since
 * showing/accepting a "what shape" choice before the region is even
 * selected was the bug being fixed.
 *
 * @param draworselect 1 for Line/Box mode, 0 for Select mode (only affects
 *                      app.mode/the statusbar's Mode field while running).
 * @return 1 if ENTER accepted the rectangle, 0 if ESC cancelled it.
 */
uint8_t rect_select(uint8_t draworselect)
{
    uint16_t orgx = (uint16_t)(app.cursor_x + app.xoffset);
    uint16_t orgy = (uint16_t)(app.cursor_y + app.yoffset);
    uint16_t curx = orgx, cury = orgy;
    uint16_t prev_xoffset = app.xoffset, prev_yoffset = app.yoffset;
    uint16_t prev_startx = orgx, prev_starty = orgy, prev_endx = orgx, prev_endy = orgy;
    uint8_t key;

    app.mode = draworselect ? MODE_LINEBOX : MODE_SELECT;
    select_hollow = 0;
    select_ellipse = 0;

    rect_perimeter_set(orgx, orgy, orgx, orgy);
    statusbar_draw();

    for (;;)
    {
        key = key_read();

        if (key == KEY_ENTER || key == KEY_ESC) break;

        switch (key)
        {
        case KEY_LEFT:  cursor_move_scroll(-1, 0); break;
        case KEY_RIGHT: cursor_move_scroll(1, 0);  break;
        case KEY_UP:    cursor_move_scroll(0, -1); break;
        case KEY_DOWN:  cursor_move_scroll(0, 1);  break;

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
            continue;

        case KEY_F8:
            if (curx == orgx && cury == orgy)
            {
                help_show(3);
                rect_perimeter_set(orgx, orgy, orgx, orgy);
                statusbar_draw();
            }
            continue;

        default:
            continue;
        }

        curx = (uint16_t)(app.cursor_x + app.xoffset);
        cury = (uint16_t)(app.cursor_y + app.yoffset);

        select_startx = (orgx < curx) ? orgx : curx;
        select_endx   = (orgx < curx) ? curx : orgx;
        select_starty = (orgy < cury) ? orgy : cury;
        select_endy   = (orgy < cury) ? cury : orgy;

        if (app.xoffset != prev_xoffset || app.yoffset != prev_yoffset)
        {
            // cursor_move_scroll() already canvas_blit()'d, clearing the old
            // preview -- only the new perimeter needs drawing.
            rect_perimeter_set(select_startx, select_starty, select_endx, select_endy);
        }
        else
        {
            rect_perimeter_clear(prev_startx, prev_starty, prev_endx, prev_endy);
            rect_perimeter_set(select_startx, select_starty, select_endx, select_endy);
        }

        prev_xoffset = app.xoffset;
        prev_yoffset = app.yoffset;
        prev_startx = select_startx;
        prev_starty = select_starty;
        prev_endx   = select_endx;
        prev_endy   = select_endy;

        statusbar_draw();
    }

    canvas_blit();
    app.mode = MODE_MAIN;
    statusbar_draw();

    if (key != KEY_ENTER) return 0;

    select_width  = (uint16_t)(select_endx - select_startx + 1);
    select_height = (uint16_t)(select_endy - select_starty + 1);
    return 1;
}

/**
 * Line/Box mode, entered via 'l' from Main mode. Grows a rectangle
 * (rect_select(1)); if accepted, prompts for the shape (MSG_LINEBOX_
 * MODE_HINT, same statusbar-override mechanism as select_run()'s own
 * MSG_SELECT_ACTION_HINT): 'o' toggles hollow/filled, 'c' toggles
 * box/ellipse (both repeatable, independent of each other), ENTER plots
 * app.plotscreencode with whatever combination is currently toggled --
 * filled box (default, neither toggle), hollow box ('o' only,
 * rect_perimeter_plot()), filled ellipse ('c' only, ellipse_fill()), or
 * hollow ellipse (both, ellipse_outline()). ESC at this prompt (or at
 * the rect-grow stage) leaves the canvas unchanged.
 *
 * Per user request 2026-06-20: this prompt -- and the 'o'/'c' keys --
 * used to be live during rect_select()'s grow loop, before the region
 * was even selected. Moved here, after rect_select() returns 1, so the
 * shape choice only becomes available once the rectangle is confirmed --
 * the same "rect first, choice second" shape select_run()'s own
 * action-key prompt already uses.
 *
 * @return (none)
 */
void linebox_run(void)
{
    uint16_t x, y;
    uint8_t key;

    if (!rect_select(1)) return;

    app.mode = MODE_LINEBOX;
    statusbar_set_override(MSG_LINEBOX_MODE_HINT);
    statusbar_draw();

    do
    {
        key = key_read();

        switch (key)
        {
        case 'o': select_hollow  = !select_hollow;  break;
        case 'c': select_ellipse = !select_ellipse; break;
        case KEY_F6: statusbar_show((uint8_t)!app.showstatusbar); break;
        default: break;
        }
    } while (key != KEY_ENTER && key != KEY_ESC);

    statusbar_clear_override();
    app.mode = MODE_MAIN;
    statusbar_draw();

    if (key == KEY_ESC) return;

    undo_snapshot(select_startx, select_starty, select_width, select_height);

    if (select_ellipse)
    {
        if (select_hollow)
            ellipse_outline(select_startx, select_starty, select_endx, select_endy, app.plotscreencode);
        else
            ellipse_fill(select_startx, select_starty, select_endx, select_endy, app.plotscreencode);
    }
    else if (select_hollow)
    {
        rect_perimeter_plot(select_startx, select_starty, select_endx, select_endy, app.plotscreencode);
    }
    else
    {
        for (y = select_starty; y < select_starty + select_height; y++)
            for (x = select_startx; x < select_startx + select_width; x++)
                canvas_put(x, y, app.plotscreencode);
    }

    canvas_blit();
}

/**
 * Paste the accepted selection (select_startx/starty/width/height) at
 * (destx, desty), direct port of V1's per-row selectmode() cut/copy loop:
 * each row is copied through canvas_rowbuf[] (src/canvas.h, shared with
 * canvas_resize()) one at a time, in top-to-bottom or bottom-to-top order
 * depending on whether the destination is below or above the source --
 * the same overlap-safety direction choice canvas_resize() uses for its
 * row reflow. If cut, the source row is blanked to CH_SPACE immediately
 * after being copied into canvas_rowbuf (before the destination write),
 * so a source/destination overlap can't lose data.
 *
 * @param destx Canvas-absolute left column of the paste destination.
 * @param desty Canvas-absolute top row of the paste destination.
 * @param cut   1 to blank the source after copying (cut), 0 to leave it
 *              unchanged (copy).
 * @return (none)
 */
static void select_paste(uint16_t destx, uint16_t desty, uint8_t cut)
{
    uint16_t ycount, srcy, dsty;

    for (ycount = 0; ycount < select_height; ycount++)
    {
        if (desty >= select_starty)
        {
            srcy = (uint16_t)(select_starty + select_height - ycount - 1);
            dsty = (uint16_t)(desty + select_height - ycount - 1);
        }
        else
        {
            srcy = (uint16_t)(select_starty + ycount);
            dsty = (uint16_t)(desty + ycount);
        }

        memcpy(canvas_rowbuf, &screenmap[srcy * app.canvas_width + select_startx], select_width);
        if (cut) memset(&screenmap[srcy * app.canvas_width + select_startx], CH_SPACE, select_width);
        memcpy(&screenmap[dsty * app.canvas_width + destx], canvas_rowbuf, select_width);
    }

    canvas_blit();
}

/**
 * Select mode, entered via 's' from Main mode. Grows a rectangle
 * (rect_select(0)); if accepted, prompts for an action: 'd' clears the
 * rect to CH_SPACE, 'i' fills it with app.plotink, 'p' fills it with
 * 16+app.plotpaper, 'm' fills it with the modifier-attribute byte for
 * app.plotaltchar/plotdouble/plotblink (the same bit-packing as oric.h's
 * A_STD(8)/A_ALT(9)/A_STD2H(10)/.../A_BLINK2HALT(15): base 8, bit0=altchar,
 * bit1=double, bit2=blink), or 'x'/'c' to cut/copy: a destination-picking
 * sub-loop (cursor keys move via cursor_move_scroll(), ENTER confirms,
 * ESC cancels) followed by select_paste() if the destination fits within
 * the canvas (MSG_SELECT_NOFIT popup otherwise). ESC at any stage leaves
 * the canvas unchanged. Cutting takes two undo_snapshot() calls (source
 * and destination rects are usually disjoint) so reverting a cut needs
 * two 'z' presses -- a minor known wrinkle, not fixed here.
 *
 * @return (none)
 */
void select_run(void)
{
    uint16_t x, y;
    uint8_t key, fillvalue;

    if (!rect_select(0)) return;

    app.mode = MODE_SELECT;
    statusbar_set_override(MSG_SELECT_ACTION_HINT);
    statusbar_draw();

    do
    {
        key = key_read();

        if (key == KEY_F6) statusbar_show((uint8_t)!app.showstatusbar);
    } while (key != 'd' && key != 'i' && key != 'p' && key != 'm'
          && key != 'x' && key != 'c' && key != KEY_ESC);

    statusbar_clear_override();

    if (key == 'x' || key == 'c')
    {
        uint8_t movekey;
        uint16_t destx, desty;

        canvas_cursor_show(app.cursor_x, app.cursor_y); // show dest cursor

        do
        {
            movekey = key_read();

            canvas_cursor_hide(app.cursor_x, app.cursor_y); // hide

            switch (movekey)
            {
            case KEY_LEFT:  cursor_move_scroll(-1, 0); break;
            case KEY_RIGHT: cursor_move_scroll(1, 0);  break;
            case KEY_UP:    cursor_move_scroll(0, -1); break;
            case KEY_DOWN:  cursor_move_scroll(0, 1);  break;
            case KEY_F6:    statusbar_show((uint8_t)!app.showstatusbar); break;
            default: break;
            }

            statusbar_draw();
            canvas_cursor_show(app.cursor_x, app.cursor_y); // show at new pos
        } while (movekey != KEY_ENTER && movekey != KEY_ESC);

        canvas_cursor_hide(app.cursor_x, app.cursor_y); // hide before popup/paste

        app.mode = MODE_MAIN;

        if (movekey == KEY_ENTER)
        {
            destx = (uint16_t)(app.cursor_x + app.xoffset);
            desty = (uint16_t)(app.cursor_y + app.yoffset);

            if (destx + select_width > app.canvas_width || desty + select_height > app.canvas_height)
            {
                menu_messagepopup(MSG_SELECT_NOFIT);
            }
            else
            {
                undo_snapshot(destx, desty, select_width, select_height);
                if (key == 'x')
                    undo_snapshot(select_startx, select_starty, select_width, select_height);
                select_paste(destx, desty, (uint8_t)(key == 'x'));
            }
        }

        statusbar_draw();
        return;
    }

    app.mode = MODE_MAIN;

    if (key == KEY_ESC)
    {
        statusbar_draw();
        return;
    }

    switch (key)
    {
    case 'd': fillvalue = CH_SPACE;                break;
    case 'i': fillvalue = app.plotink;              break;
    case 'p': fillvalue = (uint8_t)(16 + app.plotpaper); break;
    case 'm':
    default:
        fillvalue = modifier_attr_byte();
        break;
    }

    undo_snapshot(select_startx, select_starty, select_width, select_height);

    for (y = select_starty; y < select_starty + select_height; y++)
        for (x = select_startx; x < select_startx + select_width; x++)
            canvas_put(x, y, fillvalue);

    canvas_blit();
    statusbar_draw();
}
