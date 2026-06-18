#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "menu.h"
#include "menudata.h"
#include "charsetedit.h"
#include "palette.h"
#include "colourpicker.h"
#include "select.h"
#include "move.h"
#include "write.h"
#include "undo.h"
#include "input.h"
#include "help.h"
#include "editor.h"

#define PLOT_MIN 0x20
#define PLOT_MAX 0x7E

/**
 * Main-mode editor loop. Initialises cursor/viewport/plot state and the
 * favourites table, draws the canvas and statusbar, then loops reading
 * keys: cursor keys move the cursor, SPACE/DEL plot/clear the cell under
 * the cursor with app.plotscreencode/CH_SPACE, '+'/'-' cycle the plot
 * screencode, ','/'.' and ';'/'\'' cycle plotink/plotpaper (wrap 0-7),
 * 'b'/'d'/'a' toggle plotblink/plotdouble/plotaltchar, '0'-'9' recall a
 * favourite into plotscreencode and SHIFT+0-9 store plotscreencode into a
 * favourite, FUNCT+6 toggles the statusbar, FUNCT+1 opens the menu bar
 * (menu_run()), 'e' opens the character editor (charsetedit_run()), 'p'/'c'
 * open the palette/colour picker popups, 'l'/'s'/'m'/'w' open Line-Box/
 * Select/Move/Write mode, 'g' grabs the screencode/attribute under the
 * cursor into the matching plot* field, 'i'/'o'/'u' plot ink/paper/the
 * modifier attribute at the cursor and move down, 'r' toggles
 * reverse-video on plotscreencode, and 'z'/'y' undo/redo the most recent
 * canvas edit. Never returns.
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
    app.plotink = A_FWWHITE;
    app.plotpaper = A_FWBLACK;
    app.plotblink = 0;
    app.plotdouble = 0;
    app.visualmap = 0;
    for (i = 0; i < FAVOURITES_COUNT; i++) app.favourites[i] = 33; // '!'
    app.mode = MODE_MAIN;
    app.showstatusbar = 1;
    app.stdchanged = 0;
    app.altchanged = 0;
    app.filename[0] = '\0';
    undo_init();

    canvas_blit();
    statusbar_draw();
    canvas_cell_invert(app.cursor_x, app.cursor_y); // show cursor

    while (1)
    {
        uint8_t c = key_read();

        canvas_cell_invert(app.cursor_x, app.cursor_y); // hide cursor

        switch (c)
        {
        case KEY_UP:
            cursor_move_scroll(0, -1);
            break;

        case KEY_DOWN:
            cursor_move_scroll(0, 1);
            break;

        case KEY_LEFT:
            cursor_move_scroll(-1, 0);
            break;

        case KEY_RIGHT:
            cursor_move_scroll(1, 0);
            break;

        case KEY_SPACE:
            // +xoffset/+yoffset matches the i/o/u/g cases below -- a
            // pre-existing gap fixed here since undo_snapshot() must
            // snapshot the exact same cell canvas_put() writes to.
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, app.plotscreencode);
            canvas_blit();
            break;

        case KEY_DEL:
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, CH_SPACE);
            canvas_blit();
            break;

        case '+':
            app.plotscreencode = (app.plotscreencode < PLOT_MAX) ? app.plotscreencode + 1 : PLOT_MIN;
            break;

        case '-':
            app.plotscreencode = (app.plotscreencode > PLOT_MIN) ? app.plotscreencode - 1 : PLOT_MAX;
            break;

        case ',':
            app.plotink = (app.plotink > 0) ? app.plotink - 1 : 7;
            break;

        case '.':
            app.plotink = (app.plotink < 7) ? app.plotink + 1 : 0;
            break;

        case ';':
            app.plotpaper = (app.plotpaper > 0) ? app.plotpaper - 1 : 7;
            break;

        case '\'':
            app.plotpaper = (app.plotpaper < 7) ? app.plotpaper + 1 : 0;
            break;

        case 'b':
            app.plotblink = !app.plotblink;
            break;

        case 'd':
            app.plotdouble = !app.plotdouble;
            break;

        case 'a':
            app.plotaltchar = !app.plotaltchar;
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            app.plotscreencode = app.favourites[c - '0'];
            break;

        case ')': app.favourites[0] = app.plotscreencode; break;
        case '!': app.favourites[1] = app.plotscreencode; break;
        case '@': app.favourites[2] = app.plotscreencode; break;
        case '#': app.favourites[3] = app.plotscreencode; break;
        case '$': app.favourites[4] = app.plotscreencode; break;
        case '%': app.favourites[5] = app.plotscreencode; break;
        case '^': app.favourites[6] = app.plotscreencode; break;
        case '&': app.favourites[7] = app.plotscreencode; break;
        case '*': app.favourites[8] = app.plotscreencode; break;
        case '(': app.favourites[9] = app.plotscreencode; break;

        case KEY_F6:
            statusbar_show(!app.showstatusbar);
            break;

        case KEY_F8:
            help_show(1);
            break;

        case KEY_F1:
            menu_run();
            break;

        case 'e':
            charsetedit_run();
            break;

        case 'p':
            palette_run();
            break;

        case 'c':
            colourpicker_run();
            break;

        case 'l':
            linebox_run();
            break;

        case 's':
            select_run();
            break;

        case 'm':
            move_run();
            break;

        case 'w':
            write_run();
            break;

        // Grab the screencode/attribute under the cursor into the
        // matching plot* field. Ported from V1 with a bug fix: V1 stores
        // the raw 16-23 paper-attribute byte into plotpaper (instead of
        // subtracting 16) and grab&2/grab&4 (0/2/0/4) into
        // plotdouble/plotblink (instead of normalising to 0/1) -- both
        // would violate this codebase's documented plotpaper (0-7) and
        // plotdouble/plotblink (0/1) invariants, so this port normalises.
        case 'g':
        {
            uint8_t grab = canvas_get(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset);
            if (grab > 31)
            {
                app.plotscreencode = grab;
            }
            else if (grab < 8)
            {
                app.plotink = grab;
            }
            else if (grab > 15)
            {
                app.plotpaper = (uint8_t)(grab - 16);
            }
            else
            {
                grab = (uint8_t)(grab - 8);
                app.plotaltchar = (grab & 1) ? 1 : 0;
                app.plotdouble  = (grab & 2) ? 1 : 0;
                app.plotblink   = (grab & 4) ? 1 : 0;
            }
            break;
        }

        // Plot present ink/paper/modifier attribute at the cursor and move
        // down one row.
        case 'i':
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, app.plotink);
            cursor_move_scroll(0, 1);
            canvas_blit();
            break;

        case 'o':
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, (uint8_t)(16 + app.plotpaper));
            cursor_move_scroll(0, 1);
            canvas_blit();
            break;

        case 'u':
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset,
                       (uint8_t)(8 | (app.plotaltchar ? 1 : 0)
                                   | (app.plotdouble  ? 2 : 0)
                                   | (app.plotblink   ? 4 : 0)));
            cursor_move_scroll(0, 1);
            canvas_blit();
            break;

        // Toggle reverse-video on the current plot screencode.
        case 'r':
            app.plotscreencode ^= 0x80;
            break;

        // Undo/redo the most recent canvas edit (no V1 precedent --
        // genuinely new functionality, see CLAUDE.md "Canvas undo/redo").
        case 'z':
            undo_perform();
            break;

        case 'y':
            redo_perform();
            break;

        default:
            break;
        }

        statusbar_draw();
        canvas_cell_invert(app.cursor_x, app.cursor_y); // show cursor at new pos
    }
}
