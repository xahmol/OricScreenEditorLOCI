#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "strings.h"
#include "modes.h"

OricCharWin statusbar_win;

// Width of the Mode field (cols 0-9), blank-padded -- see statusbar_draw().
#define STATUSBAR_MODE_WIDTH 10

// Screen row the statusbar overlays -- the viewport's own last row, not a
// row beyond it (the viewport now spans the full 28-row screen, see
// appstate.h's VIEWPORT_HEIGHT comment).
#define STATUSBAR_ROW (VIEWPORT_HEIGHT - 1)

// When set (statusbar_set_override()), replaces the Mode field's normal
// mode_name(app.mode) text -- ported from V1's lineandbox()/selectmode(),
// which overwrite programmode itself with a key-hint string (e.g.
// "x/c/d/ipm?") while waiting for a secondary key. Must fit
// STATUSBAR_MODE_WIDTH (10 chars); longer strings are silently truncated
// by statusbar_draw()'s existing fixed-width loop.
static const char *mode_override = nullptr;

// Tracks whether the statusbar is currently auto-hidden (cursor sitting on
// STATUSBAR_ROW while app.showstatusbar is on) -- see statusbar_draw().
static uint8_t autohidden;

/**
 * Initialise the statusbar window (overlaying the viewport's last row) and
 * draw its initial content. Called once at startup.
 *
 * @return (none)
 */
void statusbar_init(void)
{
    cwin_init(&statusbar_win, 2, STATUSBAR_ROW, 38, 1, A_FWBLACK, A_BGWHITE);
    statusbar_draw();
}

/**
 * Redraw the statusbar with the current cursor position, the screencode
 * under the cursor, and the current plot attributes (screencode/glyph,
 * ink/paper colour incl. swatches, and altchar/double/blink flags).
 *
 * No-op if the statusbar is currently hidden (app.showstatusbar == 0) --
 * STATUSBAR_ROW already shows real canvas content in that case (see
 * statusbar_show()). Also auto-hides whenever the cursor is on
 * STATUSBAR_ROW, even if app.showstatusbar is on (found 2026-06-21: this
 * is the only screen row the cursor could otherwise never reach, since the
 * statusbar used to permanently occupy a row beyond the viewport instead
 * of the viewport's own last row) -- canvas_blit() reveals the real
 * content there the moment the cursor arrives; moving away restores the
 * statusbar normally on the next call.
 *
 * @return (none)
 */
void statusbar_draw(void)
{
    if (!app.showstatusbar) return;

    if (app.cursor_y == STATUSBAR_ROW)
    {
        if (!autohidden)
        {
            canvas_blit();
            autohidden = 1;
        }
        return;
    }
    autohidden = 0;

    cwin_clear(&statusbar_win);

    // Mode field (cols 0-9): _cwin_vformat's %s has no width/left-pad
    // support, so write it directly and blank-pad to STATUSBAR_MODE_WIDTH.
    {
        const char *name = mode_override ? mode_override : mode_name(app.mode);
        uint8_t x;
        for (x = 0; x < STATUSBAR_MODE_WIDTH && name[x]; x++)
            cwin_putat_char(&statusbar_win, x, 0, (uint8_t)name[x]);
        for (; x < STATUSBAR_MODE_WIDTH; x++)
            cwin_putat_char(&statusbar_win, x, 0, CH_SPACE);
    }

    cwin_putat_printf(&statusbar_win, STATUSBAR_MODE_WIDTH, 0, MSG_STATUSBAR_MAIN_FMT,
                       (uint16_t)(app.cursor_x + app.xoffset),
                       (uint16_t)(app.cursor_y + app.yoffset),
                       app.plotscreencode,
                       canvas_get(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset),
                       app.plotink, app.plotpaper,
                       app.plotaltchar ? 'A' : 'S',
                       app.plotdouble ? 'D' : ' ',
                       app.plotblink ? 'B' : ' ');

    cwin_putat_char(&statusbar_win, 20, 0, app.plotscreencode);
    cwin_putat_char(&statusbar_win, 27, 0, 16 + app.plotink);
    cwin_putat_char(&statusbar_win, 28, 0, A_BGWHITE);
    cwin_putat_char(&statusbar_win, 31, 0, 16 + app.plotpaper);
    cwin_putat_char(&statusbar_win, 32, 0, A_BGWHITE);
}

/**
 * Toggle the statusbar on or off (FUNCT+6). When turning on, redraws its
 * content (or, if the cursor is on STATUSBAR_ROW, leaves the real canvas
 * content showing there via the same auto-hide path statusbar_draw()
 * already uses). When turning off, reveals the real canvas content at
 * STATUSBAR_ROW -- it's a genuine viewport row now, not a dedicated
 * statusbar-only row, so blanking it with spaces (the old behaviour, back
 * when it was a separate row beyond the viewport) would erase real canvas
 * content.
 *
 * @param on 1 to show the statusbar, 0 to hide it.
 * @return (none)
 */
void statusbar_show(uint8_t on)
{
    app.showstatusbar = on;

    if (on)
    {
        statusbar_draw();
    }
    else
    {
        autohidden = 1;
        canvas_blit();
    }
}

/**
 * Override the statusbar's Mode field with a fixed key-hint string instead
 * of mode_name(app.mode), for the duration of a secondary-key prompt (see
 * "mode_override" above). Takes effect on the next statusbar_draw() --
 * does not redraw by itself.
 *
 * @param text Hint string to show (must fit STATUSBAR_MODE_WIDTH).
 * @return (none)
 */
void statusbar_set_override(const char *text)
{
    mode_override = text;
}

/**
 * Clear a previously-set Mode field override, reverting to
 * mode_name(app.mode) on the next statusbar_draw().
 *
 * @return (none)
 */
void statusbar_clear_override(void)
{
    mode_override = nullptr;
}
