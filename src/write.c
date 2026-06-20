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
//
// New functionality, no V1 precedent: FUNCT+4 (KEY_F4) opens a hex-direct
// attribute-entry popup (write_hex_attr()). This was briefly bound to
// FUNCT+5 instead, because include/keyboard.c's decode_funct[] table had
// a transcription bug binding KEY_F4 to FUNCT+R instead of the real
// hardware's FUNCT+digit-4 (since fixed -- see CLAUDE.md "FUNCT+digit
// keys now match real hardware exactly") -- moved back to the originally
// intended FUNCT+4 once that was fixed.

#include <string.h>
#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "undo.h"
#include "input.h"
#include "help.h"
#include "menu.h"
#include "strings.h"
#include "write.h"

/**
 * Parse a single hexadecimal digit character.
 *
 * @param c Character '0'-'9', 'a'-'f', or 'A'-'F'.
 * @return Numeric value 0-15, or 0 if c is not a hex digit.
 */
static uint8_t write_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

/**
 * FUNCT+4 in Write mode: an alternate, hex-direct input method alongside
 * the existing CTRL+Z/X (ink)/CTRL+C/V (paper) cycling and FUNCT+1/2/3
 * (plot at cursor) -- prompts for which attribute (1=Ink/2=Paper/3=Mod),
 * then a single hex digit 0-7, and plots the resulting attribute byte at
 * the cursor exactly like FUNCT+1/2/3 do (advance right after). New
 * functionality, no V1 precedent.
 *
 * @return (none)
 */
static void write_hex_attr(void)
{
    OricCharWin win;
    char        buf[2];
    uint8_t     key, val, attrbyte;

    menu_winsave(5, 8, 1);
    cwin_init(&win, 5, 5, 30, 8, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&win);

    cwin_putat_string(&win, 2, 1, MSG_WRITE_HEX_TITLE);
    cwin_putat_string(&win, 2, 3, MSG_WRITE_HEX_PROMPT);

    do
    {
        key = key_read();
        if (key == KEY_ESC) { menu_winrestore(); return; }
    } while (key != '1' && key != '2' && key != '3');

    cwin_putat_string(&win, 2, 5, MSG_WRITE_HEX_VALUE);
    strcpy(buf, "0");
    if (cwin_textinput(&win, 2, 6, 4, buf, 1, VINPUT_NUMS | VINPUT_ALPHA) < 0) { menu_winrestore(); return; }

    menu_winrestore();

    val = write_hex_digit(buf[0]);
    if (val > 7) val = 7;

    switch (key)
    {
    case '1': attrbyte = val;                       break; // Ink
    case '2': attrbyte = (uint8_t)(16 + val);        break; // Paper
    default:  attrbyte = (uint8_t)(8 | val);         break; // Modifier (altchar/double/blink bits)
    }

    undo_snapshot(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, 1, 1);
    canvas_put(app.cursor_x + app.xoffset, app.cursor_y + app.yoffset, attrbyte);
    cursor_move_scroll(1, 0);
}

/**
 * Write mode, entered via 'w' from Main mode. Cursor keys move (with
 * auto-scroll, cursor_move_scroll()); CTRL+B/A/D toggle blink/altchar/
 * double; CTRL+Z/X cycle ink down/up; CTRL+C/V cycle paper down/up;
 * FUNCT+1/2/3 plot ink/paper/modifier-attribute at the cursor and advance
 * right; FUNCT+4 is an alternate hex-direct attribute entry (see
 * write_hex_attr()); DEL clears the cell under the cursor (no advance);
 * CTRL+R toggles a local reverse-video flag; any other printable key
 * plots its screencode (+0x80 if reverse-video is on) and advances
 * right. FUNCT+6 toggles the statusbar. ESC exits to Main. The cursor
 * shows a canvas_cursor_show() preview of app.plotscreencode at all
 * times (canvas_blit() each iteration already redraws the just-plotted
 * real content first, see V1's plotmove(), which this matches) -- a
 * parity fix, this had no on-screen indication at all before.
 *
 * @return (none)
 */
void write_run(void)
{
    uint8_t key, rvs = 0;

    app.mode = MODE_WRITE;
    statusbar_draw();
    canvas_cursor_show(app.cursor_x, app.cursor_y);

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

        case KEY_F4:
            write_hex_attr();
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

        case KEY_F8:
            help_show(4);
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
        canvas_cursor_show(app.cursor_x, app.cursor_y);
        statusbar_draw();
    }

    app.mode = MODE_MAIN;
    statusbar_draw();
}
