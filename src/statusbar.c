#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "statusbar.h"

OricCharWin statusbar_win;

void statusbar_init(void)
{
    cwin_init(&statusbar_win, 2, VIEWPORT_HEIGHT, 38, 1, A_FWWHITE, A_BGBLUE);
    statusbar_draw();
}

void statusbar_draw(void)
{
    if (!app.showstatusbar) return;

    cwin_clear(&statusbar_win);
    cwin_putat_printf(&statusbar_win, 0, 0, "Main  X:%02u Y:%02u  C:$%02x  S:%ux%u",
                       app.cursor_x, app.cursor_y,
                       app.plotscreencode, app.canvas_width, app.canvas_height);
}

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
