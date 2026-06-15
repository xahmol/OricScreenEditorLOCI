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
char pulldown_options[PULLDOWN_NUMBER] = { 4, 6, 6, 2, 2 };

char pulldown_titles[PULLDOWN_NUMBER][PULLDOWN_MAXOPTIONS][PULLDOWN_MAXLENGTH] = {
    // 0 -- Screen
    { "", "", MSG_SCREEN_CLEAR, MSG_SCREEN_FILL },
    // 1 -- File
    { MSG_FILE_SAVE_SCREEN, MSG_FILE_LOAD_SCREEN, MSG_FILE_SAVE_PROJECT, MSG_FILE_LOAD_PROJECT,
      MSG_FILE_SAVE_COMBINED, MSG_FILE_LOAD_COMBINED },
    // 2 -- Charset
    { MSG_CHARSET_LOAD_STD, MSG_CHARSET_LOAD_ALT, MSG_CHARSET_SAVE_STD, MSG_CHARSET_SAVE_ALT,
      MSG_CHARSET_LOAD_COMBINED, MSG_CHARSET_SAVE_COMBINED },
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
 * canvas size.
 *
 * @return (none) -- result is written to pulldown_titles[0][0]/[0][1].
 */
static void update_size_titles(void)
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
    cwin_clear(&win);

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
            cwin_getch();
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
 * Clear/Fill), show a "not yet implemented" popup for File/Charset/
 * Information items, and run until ESC is pressed at the bar level (any
 * menu_main() return value >= 99). Restores row 0 and redraws the canvas +
 * statusbar on exit.
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
            canvas_clear();
            break;

        case 14:
            canvas_fill(app.plotscreencode);
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
