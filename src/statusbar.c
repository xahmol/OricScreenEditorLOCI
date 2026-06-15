#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "statusbar.h"
#include "strings.h"

OricCharWin statusbar_win;

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
 * Redraw the statusbar with the current cursor position, plot screencode,
 * and canvas size. No-op if the statusbar is currently hidden
 * (app.showstatusbar == 0).
 *
 * @return (none)
 */
void statusbar_draw(void)
{
    if (!app.showstatusbar) return;

    cwin_clear(&statusbar_win);
    cwin_putat_printf(&statusbar_win, 0, 0, MSG_STATUSBAR_MAIN_FMT,
                       app.cursor_x, app.cursor_y,
                       app.plotscreencode, app.canvas_width, app.canvas_height);
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
