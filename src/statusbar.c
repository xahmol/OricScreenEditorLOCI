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

/**
 * Initialise the row-27 statusbar window and draw its initial content.
 * Called once at startup.
 *
 * @return (none)
 */
void statusbar_init(void)
{
    cwin_init(&statusbar_win, 2, VIEWPORT_HEIGHT, 38, 1, A_FWBLACK, A_BGWHITE);
    statusbar_draw();
}

/**
 * Redraw the statusbar with the current cursor position, the screencode
 * under the cursor, and the current plot attributes (screencode/glyph,
 * ink/paper colour incl. swatches, and altchar/double/blink flags). No-op
 * if the statusbar is currently hidden (app.showstatusbar == 0).
 *
 * @return (none)
 */
void statusbar_draw(void)
{
    if (!app.showstatusbar) return;

    cwin_clear(&statusbar_win);

    // Mode field (cols 0-9): _cwin_vformat's %s has no width/left-pad
    // support, so write it directly and blank-pad to STATUSBAR_MODE_WIDTH.
    {
        const char *name = mode_name(app.mode);
        uint8_t x;
        for (x = 0; x < STATUSBAR_MODE_WIDTH && name[x]; x++)
            cwin_putat_char(&statusbar_win, x, 0, (uint8_t)name[x]);
        for (; x < STATUSBAR_MODE_WIDTH; x++)
            cwin_putat_char(&statusbar_win, x, 0, CH_SPACE);
    }

    cwin_putat_printf(&statusbar_win, STATUSBAR_MODE_WIDTH, 0, MSG_STATUSBAR_MAIN_FMT,
                       app.cursor_x, app.cursor_y,
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
 * content; when turning off, blanks row 27 of screen RAM directly.
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
        uint8_t *row = (uint8_t *)TEXTVRAM + VIEWPORT_HEIGHT * SCREEN_COLS;
        for (uint8_t x = 0; x < SCREEN_COLS; x++)
            row[x] = CH_SPACE;
    }
}
