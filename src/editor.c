#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "menu.h"
#include "menudata.h"
#include "charsetedit.h"
#include "editor.h"

#define PLOT_MIN 0x20
#define PLOT_MAX 0x7E

/**
 * Main-mode editor loop. Initialises cursor/viewport/plot state and the
 * favourites table, draws the canvas and statusbar, then loops reading
 * keys: cursor keys move the cursor, SPACE/DEL plot/clear the cell under
 * the cursor with app.plotscreencode/CH_SPACE, '+'/'-' cycle the plot
 * screencode, FUNCT+6 toggles the statusbar, FUNCT+1 opens the menu bar
 * (menu_run()), and 'e' opens the character editor (charsetedit_run()).
 * Never returns.
 *
 * @return (none)
 */
void editor_run(void)
{
    uint8_t i;

    app.cursor_x = 0;
    app.cursor_y = 0;
    app.xoffset = 0;
    app.yoffset = 0;
    app.plotscreencode = 0x41; // 'A'
    app.plotaltchar = 0;
    for (i = 0; i < FAVOURITES_COUNT; i++) app.favourites[i] = 33; // '!'
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
            if (app.cursor_y < VIEWPORT_HEIGHT - 1) app.cursor_y++;
            break;

        case KEY_LEFT:
            if (app.cursor_x > 0) app.cursor_x--;
            break;

        case KEY_RIGHT:
            if (app.cursor_x < VIEWPORT_WIDTH - 1) app.cursor_x++;
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

        case KEY_F1:
            menu_run();
            break;

        case 'e':
            charsetedit_run();
            break;

        default:
            break;
        }

        statusbar_draw();
        canvas_cell_invert(app.cursor_x, app.cursor_y); // show cursor at new pos
    }
}
