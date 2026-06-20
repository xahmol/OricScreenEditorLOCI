// menudata.c - OricScreenEditorLOCI menu tables, dialogs and dispatch
//
// Menu bar layout and Screen-menu semantics (Width:/Height:/Clear/Fill,
// shrink-confirm) follow V1 OricScreenEditor (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c, menubartitles/
// pulldownmenutitles and resizewidth()/resizeheight()/case 11-14).

#include <stdint.h>
#include <stdio.h>
#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "strings.h"
#include "menu.h"
#include "menudata.h"
#include "fileio.h"
#include "input.h"
#include "undo.h"
#include "info.h"

// -------------------------------------------------------------------------
// Menu bar and pulldown tables
// -------------------------------------------------------------------------

MenuBar menubar = {
    { MSG_MENU_BAR_SCREEN, MSG_MENU_BAR_FILE, MSG_MENU_BAR_CHARSET, MSG_MENU_BAR_INFO },
    { 0, 0, 0, 0 },
    0
};

// pulldown_titles[0][0]/[0][1] (Width:/Height:) are rewritten by
// update_size_titles() whenever the canvas size changes.
char pulldown_options[PULLDOWN_NUMBER] = { 4, 6, 7, 2, 2 };

char pulldown_titles[PULLDOWN_NUMBER][PULLDOWN_MAXOPTIONS][PULLDOWN_MAXLENGTH] = {
    // 0 -- Screen
    { "", "", MSG_SCREEN_CLEAR, MSG_SCREEN_FILL },
    // 1 -- File
    { MSG_FILE_SAVE_SCREEN, MSG_FILE_LOAD_SCREEN, MSG_FILE_SAVE_PROJECT, MSG_FILE_LOAD_PROJECT,
      MSG_FILE_SAVE_COMBINED, MSG_FILE_LOAD_COMBINED },
    // 2 -- Charset
    { MSG_CHARSET_LOAD_STD, MSG_CHARSET_LOAD_ALT, MSG_CHARSET_SAVE_STD, MSG_CHARSET_SAVE_ALT,
      MSG_CHARSET_LOAD_COMBINED, MSG_CHARSET_SAVE_COMBINED, MSG_CHARSET_RESET_STD },
    // 3 -- Information
    { MSG_INFO_VERSION, MSG_INFO_EXIT },
    // 4 -- Yes/No
    { MSG_MENU_YN_YES, MSG_MENU_YN_NO }
};

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Rewrite the Screen > Width:/Height: pulldown titles from the current
 * canvas size. Public (not static) so src/fileio.c can refresh them after
 * a Project/Screen/Combined load resizes the canvas outside resize_dialog().
 *
 * @return (none) -- result is written to pulldown_titles[0][0]/[0][1].
 */
void update_size_titles(void)
{
    sprintf(pulldown_titles[0][0], MSG_SCREEN_WIDTH_FMT, app.canvas_width);
    sprintf(pulldown_titles[0][1], MSG_SCREEN_HEIGHT_FMT, app.canvas_height);
}

/**
 * Parse a decimal digit string (as produced by cwin_textinput with
 * VINPUT_NUMS) into a uint16_t. Stops at the first non-digit. Returns 0 for
 * an empty string.
 *
 * @param s NUL-terminated digit string.
 * @return Parsed value.
 */
static uint16_t parse_uint(const char *s)
{
    uint16_t v = 0;
    while (*s >= '0' && *s <= '9')
    {
        v = v * 10 + (uint16_t)(*s - '0');
        s++;
    }
    return v;
}

/**
 * Shared implementation of Screen > Width and Screen > Height: opens a
 * popup, lets the user type a new size, validates it against the viewport
 * minimum and the CANVAS_MAX_SIZE budget, asks for confirmation
 * (menu_areyousure()) if the change would shrink the canvas, then applies it
 * via canvas_resize() and refreshes the pulldown titles.
 *
 * @param is_height Zero to resize width, non-zero to resize height.
 * @return (none)
 */
static void resize_dialog(uint8_t is_height)
{
    OricCharWin win;
    char        buf[6];
    uint16_t    curw = app.canvas_width;
    uint16_t    curh = app.canvas_height;
    uint16_t    cur  = is_height ? curh : curw;
    uint16_t    minval = is_height ? VIEWPORT_HEIGHT : VIEWPORT_WIDTH;

    menu_winsave(5, 12, 1);
    cwin_init(&win, 5, 5, 35, 12, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&win);

    cwin_putat_string(&win, 2, 1, is_height ? MSG_RESIZE_TITLE_HEIGHT : MSG_RESIZE_TITLE_WIDTH);
    cwin_putat_string(&win, 2, 3, is_height ? MSG_RESIZE_PROMPT_HEIGHT : MSG_RESIZE_PROMPT_WIDTH);

    sprintf(buf, "%u", cur);
    if (cwin_textinput(&win, 2, 4, 8, buf, 5, VINPUT_NUMS) >= 0)
    {
        uint16_t newval = parse_uint(buf);
        uint16_t neww   = is_height ? curw : newval;
        uint16_t newh   = is_height ? newval : curh;
        uint8_t  valid  = (newval >= minval) &&
                          ((uint32_t)neww * (uint32_t)newh <= CANVAS_MAX_SIZE);

        if (!valid)
        {
            cwin_putat_string(&win, 2, 6, MSG_RESIZE_INVALID);
            cwin_putat_string(&win, 2, 7, MSG_MENU_PRESSAKEY);
            key_read();
        }
        else
        {
            uint8_t proceed = 1;

            if (newval < cur)
                proceed = (menu_areyousure(MSG_MENU_SHRINKWARN) == 1);

            if (proceed)
            {
                canvas_resize(neww, newh);
                update_size_titles();
            }
        }
    }

    menu_winrestore();
}

/**
 * Goto coordinates popup ('j' in Main mode): prompts for an X then a Y
 * canvas coordinate (each pre-filled with the cursor's current absolute
 * position), and on confirming both, jumps the cursor + viewport there
 * via canvas_goto() (src/canvas.h). ESC at either field cancels with no
 * change, matching every other popup's ESC convention in this codebase.
 *
 * @return (none)
 */
void goto_dialog(void)
{
    OricCharWin win;
    char        bufx[6], bufy[6];

    menu_winsave(5, 12, 1);
    cwin_init(&win, 5, 5, 35, 12, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&win);

    cwin_putat_string(&win, 2, 1, MSG_GOTO_TITLE);
    cwin_putat_string(&win, 2, 3, MSG_GOTO_PROMPT_X);
    sprintf(bufx, "%u", (uint16_t)(app.cursor_x + app.xoffset));

    if (cwin_textinput(&win, 2, 4, 8, bufx, 5, VINPUT_NUMS) >= 0)
    {
        cwin_putat_string(&win, 2, 6, MSG_GOTO_PROMPT_Y);
        sprintf(bufy, "%u", (uint16_t)(app.cursor_y + app.yoffset));

        if (cwin_textinput(&win, 2, 7, 8, bufy, 5, VINPUT_NUMS) >= 0)
        {
            canvas_goto(parse_uint(bufx), parse_uint(bufy));
        }
    }

    menu_winrestore();
}

/**
 * Charset > Reset Std->ROM: after a menu_areyousure() confirmation,
 * restores all 768 displayable bytes of CHARSET_STD from CHARSETROM in
 * one step (charset_load(), include/charset.h -- the same primitive
 * charsetswap.c already uses for popup chrome), instead of the
 * character editor's glyph-by-glyph 's' key. Sets app.stdchanged so
 * File > Save Project knows the standard charset changed this session.
 * Std-only, same as every other ROM-restore path in this codebase (the
 * Oric ROM has no alternate-charset table to restore from).
 *
 * @return (none)
 */
static void charset_reset_std(void)
{
    if (menu_areyousure(MSG_CHARSET_RESETWARN) != 1) return;

    charset_load(CHARSET_STD, (const uint8_t *)CHARSETROM);
    app.stdchanged = 1;
}

// -------------------------------------------------------------------------
// Init and dispatch
// -------------------------------------------------------------------------

/**
 * Initialise menu tables from the current canvas size. Call once at startup,
 * after canvas_init().
 *
 * @return (none) -- result is written to pulldown_titles[0][0]/[0][1].
 */
void menudata_init(void)
{
    update_size_titles();
}

/**
 * Open the menu bar (row 0), dispatch Screen menu choices (Width/Height/
 * Clear/Fill), all File/Charset menu items (src/fileio.c) and the
 * Information menu items (src/info.c), and run until ESC is pressed at
 * the bar level (any menu_main() return value >= 99). Restores row 0 and
 * redraws the canvas + statusbar on exit.
 *
 * @return (none)
 */
void menu_run(void)
{
    uint8_t choice;

    menu_winsave(0, 1, 1);
    menu_placebar(0);

    do {
        choice = menu_main();

        switch (choice)
        {
        case 11:
            resize_dialog(0);
            break;

        case 12:
            resize_dialog(1);
            break;

        case 13:
            undo_snapshot(0, 0, app.canvas_width, app.canvas_height);
            canvas_clear();
            break;

        case 14:
            undo_snapshot(0, 0, app.canvas_width, app.canvas_height);
            canvas_fill(app.plotscreencode);
            break;

        case 21:
            fileio_save_screen();
            break;

        case 22:
            fileio_load_screen();
            break;

        case 23:
            fileio_save_project();
            break;

        case 24:
            fileio_load_project();
            break;

        case 25:
            fileio_save_combined();
            break;

        case 26:
            fileio_load_combined();
            break;

        case 31:
            fileio_load_charset(0);
            break;

        case 32:
            fileio_load_charset(1);
            break;

        case 33:
            fileio_save_charset(0);
            break;

        case 34:
            fileio_save_charset(1);
            break;

        case 35:
            fileio_load_charset(2);
            break;

        case 36:
            fileio_save_charset(2);
            break;

        case 37:
            charset_reset_std();
            break;

        case 41:
            info_version_show();
            break;

        case 42:
            info_exit();
            break;

        default:
            if (choice < 99) menu_messagepopup(MSG_MENU_NOTIMPL);
            break;
        }
    } while (choice < 99);

    menu_winrestore();
    canvas_blit();
    statusbar_draw();
}
