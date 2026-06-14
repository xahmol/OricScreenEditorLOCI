#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "editor.h"

#define PLOT_MIN 0x20
#define PLOT_MAX 0x7E

void editor_run(void)
{
    app.canvas_width  = CANVAS_WIDTH;
    app.canvas_height = CANVAS_HEIGHT;
    app.cursor_x = 0;
    app.cursor_y = 0;
    app.xoffset = 0;
    app.yoffset = 0;
    app.plotscreencode = 0x41; // 'A'
    app.mode = MODE_MAIN;
    app.showstatusbar = 1;

    canvas_blit();
    statusbar_draw();
    canvas_cell_invert(app.cursor_x, app.cursor_y); // show cursor

    while (1)
    {
        uint8_t c = cwin_getch();

        canvas_cell_invert(app.cursor_x, app.cursor_y); // hide cursor

        switch (c)
        {
        case KEY_UP:
            if (app.cursor_y > 0) app.cursor_y--;
            break;

        case KEY_DOWN:
            if (app.cursor_y < app.canvas_height - 1) app.cursor_y++;
            break;

        case KEY_LEFT:
            if (app.cursor_x > 0) app.cursor_x--;
            break;

        case KEY_RIGHT:
            if (app.cursor_x < app.canvas_width - 1) app.cursor_x++;
            break;

        case KEY_SPACE:
            canvas_put(app.cursor_x, app.cursor_y, app.plotscreencode);
            canvas_blit();
            break;

        case KEY_DEL:
            canvas_put(app.cursor_x, app.cursor_y, CH_SPACE);
            canvas_blit();
            break;

        case '+':
            app.plotscreencode = (app.plotscreencode < PLOT_MAX) ? app.plotscreencode + 1 : PLOT_MIN;
            break;

        case '-':
            app.plotscreencode = (app.plotscreencode > PLOT_MIN) ? app.plotscreencode - 1 : PLOT_MAX;
            break;

        case KEY_F6:
            statusbar_show(!app.showstatusbar);
            break;

        default:
            break;
        }

        statusbar_draw();
        canvas_cell_invert(app.cursor_x, app.cursor_y); // show cursor at new pos
    }
}
