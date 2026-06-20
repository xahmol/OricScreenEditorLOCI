// move.c - Move mode (content nudge) for OricScreenEditorLOCI (Oric Atmos,
// Oscar64)
//
// Adapted from V1's movemode() (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c). V1 shifts the *currently
// visible* viewport's content by one row/col using a hardware scroll, then
// (only on ENTER) bakes that shift back into screenmap permanently -- ESC
// effectively "cancels" only because nothing had been written back yet.
// OSE has no hardware-scroll primitive and canvas_blit() is a pure
// software redraw, so this port edits screenmap[] directly on every
// keypress (each shift is immediately "real", canvas_blit() just shows it)
// -- there is no separate scratch copy, so unlike V1, ESC here does NOT
// roll back shifts already applied during this Move session. This is a
// deliberate simplification (flagged in the original plan for this mode), not a bug: V1's
// own ESC-cancel only worked by accident of not having committed yet
// either, for the exact same single-buffer reason.
//
// Distinct from the cursor-auto-scroll fix in canvas.c's
// cursor_move_scroll(): that changes app.xoffset/yoffset to navigate a
// canvas larger than the viewport (cursor stays in the same canvas cell,
// the *view* moves). Move mode keeps app.xoffset/yoffset fixed and
// rewrites screenmap[] cells within the current viewport instead (the view
// stays fixed, the *content* moves).

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "undo.h"
#include "input.h"
#include "help.h"
#include "move.h"

/**
 * Shift the visible viewport's content (app.xoffset/yoffset ..
 * +VIEWPORT_WIDTH/HEIGHT-1) by one cell in the given direction (exactly one
 * of dx/dy non-zero). The cell row/column revealed at the leading edge is
 * filled with CH_SPACE; the trailing edge's content is lost. Iterates in
 * the order that avoids clobbering source cells before they're read
 * (descending when shifting right/down, ascending when shifting
 * left/up -- the same direction-choice principle canvas_resize() uses for
 * its row reflow, src/canvas.c).
 *
 * @param dx -1/0/+1 horizontal shift direction.
 * @param dy -1/0/+1 vertical shift direction.
 * @return (none)
 */
static void move_shift(int8_t dx, int8_t dy)
{
    uint16_t x, y;
    uint16_t x0 = app.xoffset, x1 = (uint16_t)(app.xoffset + VIEWPORT_WIDTH - 1);
    uint16_t y0 = app.yoffset, y1 = (uint16_t)(app.yoffset + VIEWPORT_HEIGHT - 1);

    undo_snapshot(x0, y0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    if (dx > 0)
    {
        for (y = y0; y <= y1; y++)
        {
            for (x = x1; x > x0; x--)
                canvas_put(x, y, canvas_get((uint16_t)(x - 1), y));
            canvas_put(x0, y, CH_SPACE);
        }
    }
    else if (dx < 0)
    {
        for (y = y0; y <= y1; y++)
        {
            for (x = x0; x < x1; x++)
                canvas_put(x, y, canvas_get((uint16_t)(x + 1), y));
            canvas_put(x1, y, CH_SPACE);
        }
    }
    else if (dy > 0)
    {
        for (y = y1; y > y0; y--)
            for (x = x0; x <= x1; x++)
                canvas_put(x, y, canvas_get(x, (uint16_t)(y - 1)));
        for (x = x0; x <= x1; x++)
            canvas_put(x, y0, CH_SPACE);
    }
    else if (dy < 0)
    {
        for (y = y0; y < y1; y++)
            for (x = x0; x <= x1; x++)
                canvas_put(x, y, canvas_get(x, (uint16_t)(y + 1)));
        for (x = x0; x <= x1; x++)
            canvas_put(x, y1, CH_SPACE);
    }
}

/**
 * Move mode, entered via 'm' from Main mode. Cursor keys nudge the visible
 * canvas content by one row/col (move_shift()), re-blitting after each
 * move; FUNCT+6 toggles the statusbar. ENTER and ESC both exit to Main --
 * equivalent here since every shift is already applied directly to
 * screenmap[] (see file header comment).
 *
 * @return (none)
 */
void move_run(void)
{
    uint8_t key;

    app.mode = MODE_MOVE;
    statusbar_draw();

    for (;;)
    {
        key = key_read();

        if (key == KEY_ENTER || key == KEY_ESC) break;

        switch (key)
        {
        case KEY_RIGHT: move_shift(1, 0);  canvas_blit(); break;
        case KEY_LEFT:  move_shift(-1, 0); canvas_blit(); break;
        case KEY_UP:    move_shift(0, -1); canvas_blit(); break;
        case KEY_DOWN:  move_shift(0, 1);  canvas_blit(); break;

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
            break;

        case KEY_F8:
            help_show(3);
            break;

        default:
            break;
        }

        statusbar_draw();
    }

    app.mode = MODE_MAIN;
    canvas_blit();
    statusbar_draw();
}
