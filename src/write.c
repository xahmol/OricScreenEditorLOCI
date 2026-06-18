// write.c - Write mode (free-typing screencodes) for OricScreenEditorLOCI
// (Oric Atmos, Oscar64)
//
// Direct port of V1's writemode() (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c, lines 864-973). One
// deliberate deviation worth flagging: Main mode's 'i'/'o'/'u' keys plot
// ink/paper/modifier at the cursor (src/editor.c), but Write mode can't
// reuse those letters -- they're themselves printable characters to type
// here. V1 uses FUNCT+1/2/3 (CH_F1/F2/F3) for the same three actions inside
// writemode(); this port does the same (KEY_F1/F2/F3).

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "undo.h"
#include "input.h"
#include "write.h"

/**
 * Write mode, entered via 'w' from Main mode. Cursor keys move (with
 * auto-scroll, cursor_move_scroll()); CTRL+B/A/D toggle blink/altchar/
 * double; CTRL+Z/X cycle ink down/up; CTRL+C/V cycle paper down/up;
 * FUNCT+1/2/3 plot ink/paper/modifier-attribute at the cursor and advance
 * right; DEL clears the cell under the cursor (no advance); CTRL+R toggles
 * a local reverse-video flag; any other printable key plots its screencode
 * (+0x80 if reverse-video is on) and advances right. FUNCT+6 toggles the
 * statusbar. ESC exits to Main.
 *
 * @return (none)
 */
void write_run(void)
{
    uint8_t key, rvs = 0;

    app.mode = MODE_WRITE;
    statusbar_draw();

    for (;;)
    {
        key = key_read();

        if (key == KEY_ESC) break;

        switch (key)
        {
        case KEY_LEFT:  cursor_move_scroll(-1, 0); break;
        case KEY_RIGHT: cursor_move_scroll(1, 0);  break;
        case KEY_UP:    cursor_move_scroll(0, -1); break;
        case KEY_DOWN:  cursor_move_scroll(0, 1);  break;

        case KEY_CTRL_B:
            app.plotblink = !app.plotblink;
            break;

        case KEY_CTRL_A:
            app.plotaltchar = !app.plotaltchar;
            break;

        case KEY_CTRL_D:
            app.plotdouble = !app.plotdouble;
            break;

        case KEY_CTRL_Z:
            app.plotink = (app.plotink > 0) ? app.plotink - 1 : 7;
            break;

        case KEY_CTRL_X:
            app.plotink = (app.plotink < 7) ? app.plotink + 1 : 0;
            break;

        case KEY_CTRL_C:
            app.plotpaper = (app.plotpaper > 0) ? app.plotpaper - 1 : 7;
            break;

        case KEY_CTRL_V:
            app.plotpaper = (app.plotpaper < 7) ? app.plotpaper + 1 : 0;
            break;

        case KEY_F1:
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, app.plotink);
            cursor_move_scroll(1, 0);
            break;

        case KEY_F2:
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, (uint8_t)(16 + app.plotpaper));
            cursor_move_scroll(1, 0);
            break;

        case KEY_F3:
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset,
                       (uint8_t)(8 | (app.plotaltchar ? 1 : 0)
                                   | (app.plotdouble  ? 2 : 0)
                                   | (app.plotblink   ? 4 : 0)));
            cursor_move_scroll(1, 0);
            break;

        case KEY_DEL:
            undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
            canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, CH_SPACE);
            break;

        case KEY_CTRL_R:
            rvs = !rvs;
            break;

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
            break;

        default:
            if (key >= 0x20 && key <= 0x7E)
            {
                undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
                canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset,
                           (uint8_t)(key | (rvs ? 0x80 : 0)));
                cursor_move_scroll(1, 0);
            }
            break;
        }

        canvas_blit();
        statusbar_draw();
    }

    app.mode = MODE_MAIN;
    statusbar_draw();
}
