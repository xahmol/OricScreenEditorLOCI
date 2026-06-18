// select.c - Shared rectangle-grower, Line/Box and Select modes for
// OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Adapted from V1's lineandbox()/selectmode() (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c). V1 grows the rect by
// tracking which edge is "trailing" the fixed origin and redrawing only
// the single edge line that moved each keypress (plotvisible()). This port
// instead always recomputes the rect as
// [min(origin,cursor) .. max(origin,cursor)] from scratch on every
// keypress, and redraws by XOR-toggling (canvas_cell_invert()) the OLD
// perimeter off then the NEW perimeter on: cells in both perimeters are
// toggled twice (net no-op, stay highlighted), cells only in the old one
// are toggled once (un-highlighted), cells only in the new one are toggled
// once (highlighted) -- so the result is identical to V1's edge-tracking
// without needing to port its four-way trailing-edge comparison logic.
// When app.xoffset/yoffset change mid-grow (cursor_move_scroll() auto-
// scrolled, see src/canvas.c), that call already canvas_blit()s, which
// clears all stale highlight bytes -- so only the new perimeter needs
// drawing in that case, not an erase pass.

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "undo.h"
#include "select.h"

// Selected/grown rectangle, in absolute canvas coordinates (inclusive
// bounds) -- file-scope statics, not AppState fields: nothing outside this
// file needs to read them (Select mode's fill actions live here too).
static uint16_t select_startx, select_starty, select_endx, select_endy;
static uint16_t select_width, select_height;

/**
 * XOR-toggle (canvas_cell_invert()) every cell of one canvas cell that is
 * currently within the viewport (app.xoffset/yoffset), converting from
 * canvas-absolute to viewport-relative coordinates. No-op if the cell is
 * scrolled out of view.
 *
 * @param cx Canvas-absolute column.
 * @param cy Canvas-absolute row.
 * @return (none)
 */
static void rect_toggle_if_visible(uint16_t cx, uint16_t cy)
{
    if (cx < app.xoffset || cx >= (uint16_t)(app.xoffset + VIEWPORT_WIDTH)) return;
    if (cy < app.yoffset || cy >= (uint16_t)(app.yoffset + VIEWPORT_HEIGHT)) return;
    canvas_cell_invert((uint16_t)(cx - app.xoffset), (uint16_t)(cy - app.yoffset));
}

/**
 * XOR-toggle the perimeter (border only, not the fill) of the rectangle
 * [sx..ex] x [sy..ey] (inclusive, canvas-absolute coordinates), clipped to
 * the current viewport. A single-cell rectangle (sx==ex && sy==ey) toggles
 * just that one cell.
 *
 * @param sx Left column.
 * @param sy Top row.
 * @param ex Right column.
 * @param ey Bottom row.
 * @return (none)
 */
static void rect_perimeter_toggle(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
    uint16_t x, y;

    for (x = sx; x <= ex; x++)
    {
        rect_toggle_if_visible(x, sy);
        if (ey != sy) rect_toggle_if_visible(x, ey);
    }
    for (y = (uint16_t)(sy + 1); y < ey; y++)
    {
        rect_toggle_if_visible(sx, y);
        if (ex != sx) rect_toggle_if_visible(ex, y);
    }
}

/**
 * Grow a rectangle from the cursor's current position using cursor keys
 * (cursor_move_scroll() handles auto-scroll on oversized canvases, see
 * src/canvas.c), with a live perimeter highlight. ENTER accepts (populating
 * select_startx/starty/endx/endy/width/height) and returns 1; ESC cancels
 * and returns 0. FUNCT+6 toggles the statusbar without affecting the rect.
 * Sets app.mode to MODE_LINEBOX or MODE_SELECT for the duration (per
 * draworselect) and restores MODE_MAIN before returning.
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

    rect_perimeter_toggle(orgx, orgy, orgx, orgy);
    statusbar_draw();

    for (;;)
    {
        key = cwin_getch();

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
            // highlight -- only the new perimeter needs drawing.
            rect_perimeter_toggle(select_startx, select_starty, select_endx, select_endy);
        }
        else
        {
            rect_perimeter_toggle(prev_startx, prev_starty, prev_endx, prev_endy);
            rect_perimeter_toggle(select_startx, select_starty, select_endx, select_endy);
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
 * (rect_select(1)) and, if accepted, fills it with app.plotscreencode.
 * ESC leaves the canvas unchanged.
 *
 * @return (none)
 */
void linebox_run(void)
{
    uint16_t x, y;

    if (!rect_select(1)) return;

    undo_snapshot(select_startx, select_starty, select_width, select_height);

    for (y = select_starty; y < select_starty + select_height; y++)
        for (x = select_startx; x < select_startx + select_width; x++)
            canvas_put(x, y, app.plotscreencode);

    canvas_blit();
}

/**
 * Select mode, entered via 's' from Main mode. Grows a rectangle
 * (rect_select(0)); if accepted, prompts for an action: 'd' clears the
 * rect to CH_SPACE, 'i' fills it with app.plotink, 'p' fills it with
 * 16+app.plotpaper, 'm' fills it with the modifier-attribute byte for
 * app.plotaltchar/plotdouble/plotblink (the same bit-packing as oric.h's
 * A_STD(8)/A_ALT(9)/A_STD2H(10)/.../A_BLINK2HALT(15): base 8, bit0=altchar,
 * bit1=double, bit2=blink). ESC at either stage leaves the canvas
 * unchanged. V1's cut/copy ('x'/'c') are deferred to Phase 8 (overlay-RAM
 * clipboard) -- see CLAUDE.md/the Phase 5 plan.
 *
 * @return (none)
 */
void select_run(void)
{
    uint16_t x, y;
    uint8_t key, fillvalue;

    if (!rect_select(0)) return;

    app.mode = MODE_SELECT;
    statusbar_draw();

    do
    {
        key = cwin_getch();

        if (key == KEY_F6) statusbar_show((uint8_t)!app.showstatusbar);
    } while (key != 'd' && key != 'i' && key != 'p' && key != 'm' && key != KEY_ESC);

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
        fillvalue = (uint8_t)(8 | (app.plotaltchar ? 1 : 0)
                                 | (app.plotdouble  ? 2 : 0)
                                 | (app.plotblink   ? 4 : 0));
        break;
    }

    undo_snapshot(select_startx, select_starty, select_width, select_height);

    for (y = select_starty; y < select_starty + select_height; y++)
        for (x = select_startx; x < select_startx + select_width; x++)
            canvas_put(x, y, fillvalue);

    canvas_blit();
    statusbar_draw();
}
