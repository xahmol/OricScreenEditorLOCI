// menu.c - Menu system for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Based on locifilemanager-v2 src/menu.c by Xander Mol (local reference at
// /home/xahmol/git/locifilemanager-v2/src/menu.c). Adapted: MENUBAR_MAXOPTIONS
// reduced to 4, window save/restore copies to a main-RAM buffer
// (menu_winbuf[]) instead of overlay RAM (no loci_present()/enable_overlay_ram
// gating needed), menu_getkey() replaced by a direct key_read() call
// (src/input.h, keyboard+IJK joystick), menu_placeheader/menu_placetop/menu_popup_open/
// menu_popup_close/menu_fileerrormessage/menu_option_select/menu_confirm_file
// dropped (not needed until later phases).

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "charwin.h"
#include "strings.h"
#include "input.h"
#include "menu.h"

// -------------------------------------------------------------------------
// Screen RAM access helpers
// -------------------------------------------------------------------------

// Direct pointer to screen row y (avoids importing charwin's row_base table).
// Oscar64 -O2 handles 16-bit multiply; menus are called infrequently.
#define MENU_ROW(y) ((uint8_t *)((uint16_t)TEXTVRAM + (uint16_t)(y) * (uint16_t)SCREEN_COLS))

/**
 * Write a single raw byte directly to screen RAM at (x, y).
 *
 * @param x Column (0-39).
 * @param y Row (0-27).
 * @param b Raw byte to write (character code or attribute).
 * @return (none)
 */
static void menu_screen_putb(uint8_t x, uint8_t y, uint8_t b)
{
    MENU_ROW(y)[x] = b;
}

/**
 * Write a NUL-terminated string directly to screen RAM starting at (x, y),
 * left to right with no wrapping or clipping.
 *
 * @param x Starting column (0-39).
 * @param y Row (0-27).
 * @param s NUL-terminated string to write.
 * @return (none)
 */
static void menu_screen_puts(uint8_t x, uint8_t y, const char *s)
{
    uint8_t *p = MENU_ROW(y) + x;
    while (*s) *p++ = (uint8_t)*s++;
}

/**
 * Write exactly n bytes from raw buffer buf to screen row y starting at x.
 * Replaces any byte < 0x20 (attribute codes, NUL) with a space to prevent
 * unintended attribute pollution from zero-padded pulldown_titles entries.
 *
 * @param x   Starting column (0-39).
 * @param y   Row (0-27).
 * @param buf Buffer of n raw bytes to write.
 * @param n   Number of bytes to write from buf.
 * @return (none)
 */
static void menu_screen_putn(uint8_t x, uint8_t y, const uint8_t *buf, uint8_t n)
{
    uint8_t *p = MENU_ROW(y) + x;
    for (uint8_t i = 0; i < n; i++)
    {
        uint8_t c = buf[i];
        *p++ = (c >= 0x20) ? c : CH_SPACE;
    }
}

// -------------------------------------------------------------------------
// Window save/restore - full-row (40-byte) main-RAM copies
// -------------------------------------------------------------------------

static uint8_t       menu_winbuf[MENU_WINBUF_SIZE];
static MenuWinRecord menu_win_stack[MENU_WIN_DEPTH];
static uint8_t       menu_win_depth;
static uint16_t      menu_win_ptr;

// Real, linker-allocated scratch buffer for menu_draw_item()'s Oscar64
// register-pressure workaround (see that function's own comment) -- NOT
// the literal `(uint8_t *)0xA000` this used to be. That address was
// documented as "unused scratch space in the heap region", which was
// wrong for this project's actual memory map (build/oseloci.map: a
// degenerate `HEAP, heap` section since malloc/free are fully stubbed;
// 0xA000 falls inside the program's own live BSS) -- flagged as exactly
// this landmine in src/filepicker.c's own analogous workaround, and
// confirmed live 2026-06-23 (user report: "Charset save on save project
// now creates garbled chars in the Standard charset for lowercase
// q-z"): this session's code-size changes shifted BSS layout enough
// that 0xA000 started landing inside charsetswap.c's backup_std[768]
// array, so every pulldown-item redraw silently overwrote part of the
// real backed-up Standard charset with this debug string -- exactly
// reproduced by dumping a saved project's CS.BIN and finding this
// function's own format string instead of glyph bytes at the q-z
// offset. Fixed the same way src/filepicker.c's picker_regpressure_
// scratch[] already was: a real static array can never alias another
// object's storage. Sized above the dummy sprintf's own worst-case
// formatted length (~58 bytes of literal text + 5 numeric fields up to
// 3 digits each (15) + a title up to PULLDOWN_MAXLENGTH-1=16 chars =
// 89 bytes, rounded up).
static uint8_t menu_regpressure_scratch[100];

/**
 * Push a record onto the window-save stack for height rows starting at
 * screen row ypos, and copy those rows from screen RAM into menu_winbuf[] at
 * the current menu_win_ptr, advancing menu_win_ptr by height*SCREEN_COLS
 * bytes. No-op if the window-save stack (MENU_WIN_DEPTH) is full or the
 * buffer (MENU_WINBUF_SIZE) is exhausted.
 *
 * @param ypos         First screen row to save.
 * @param height       Number of rows to save.
 * @param swap_charset 1 = also call charsetswap_enter() (popup chrome opts
 *                     in to the ROM-standard charset); 0 = opt out (character
 *                     editor, so live glyph edits stay visible).
 * @return (none) -- result is written to menu_win_stack[], menu_win_depth,
 *         and menu_win_ptr.
 */
void menu_winsave(uint8_t ypos, uint8_t height, uint8_t swap_charset)
{
    uint16_t len = (uint16_t)height * (uint16_t)SCREEN_COLS;

    if (menu_win_depth >= MENU_WIN_DEPTH) return;
    if ((uint32_t)menu_win_ptr + len > MENU_WINBUF_SIZE) return;

    menu_win_stack[menu_win_depth].offset       = menu_win_ptr;
    menu_win_stack[menu_win_depth].ypos         = ypos;
    menu_win_stack[menu_win_depth].height       = height;
    menu_win_stack[menu_win_depth].swap_charset = swap_charset;
    menu_win_depth++;

    uint8_t *src = MENU_ROW(ypos);
    uint8_t *dst = &menu_winbuf[menu_win_ptr];
    for (uint16_t i = 0; i < len; i++)
        dst[i] = src[i];

    menu_win_ptr += len;

    if (swap_charset) charsetswap_enter();
}

/**
 * Pop the most recent record from the window-save stack and copy its saved
 * rows back from menu_winbuf[] to screen RAM, restoring menu_win_ptr to the
 * popped record's offset. No-op if the window-save stack is empty.
 *
 * @return (none) -- result is written to screen RAM, menu_win_depth, and
 *         menu_win_ptr.
 */
void menu_winrestore(void)
{
    if (menu_win_depth == 0) return;
    menu_win_depth--;

    uint16_t offset       = menu_win_stack[menu_win_depth].offset;
    uint8_t  ypos         = menu_win_stack[menu_win_depth].ypos;
    uint8_t  height       = menu_win_stack[menu_win_depth].height;
    uint8_t  swap_charset = menu_win_stack[menu_win_depth].swap_charset;

    menu_win_ptr = offset;

    uint8_t  *src = &menu_winbuf[offset];
    uint8_t  *dst = MENU_ROW(ypos);
    uint16_t  len = (uint16_t)height * (uint16_t)SCREEN_COLS;

    for (uint16_t i = 0; i < len; i++)
        dst[i] = src[i];

    if (swap_charset) charsetswap_exit();
}

/**
 * Draw a white-paper popup background for rows ypos..ypos+height-1: ink
 * (A_FWBLACK=0x00) at col 0, paper (A_BGWHITE) at col 1 (the same
 * col0/col1 attribute-byte convention include/charwin.c's row_setattr()
 * uses), spaces at cols 2-39. Previously left cols 0-4 untouched
 * ("part of the saved background") -- fixed (user-reported, 2026-06-20):
 * that left whatever canvas content was underneath visibly bleeding
 * through to the left of the popup text instead of a clean white box.
 *
 * @param ypos   First screen row to paint.
 * @param height Number of rows to paint.
 * @return (none)
 */
void menu_wininit(uint8_t ypos, uint8_t height)
{
    for (uint8_t y = 0; y < height; y++)
    {
        uint8_t *row = MENU_ROW(ypos + y);
        row[0] = A_FWBLACK;   // 0x00 -- must write directly (not in string literal)
        row[1] = A_BGWHITE;
        row[2] = A_STD;       // reset charset-mode latch per row (persists across rows,
                               // does not reset per scanline -- see CLAUDE.md screen model)
        for (uint8_t x = 3; x < SCREEN_COLS; x++)
            row[x] = CH_SPACE;
    }
}

// -------------------------------------------------------------------------
// Init
// -------------------------------------------------------------------------

/**
 * Initialise the menu module: reset the window-save stack (depth and
 * buffer pointer). Call once at startup.
 *
 * @return (none) -- result is written to menu_win_depth and menu_win_ptr.
 */
void menu_init(void)
{
    menu_win_depth = 0;
    menu_win_ptr   = 0;
}

// -------------------------------------------------------------------------
// Bar
// -------------------------------------------------------------------------

/**
 * Draw the menu bar at row y: black ink, green paper across the row, with
 * each top-level menubar.titles[] entry printed left to right with a
 * one-column gap on each side. Computes and stores each item's highlight
 * column in menubar.xstart[], clamping the last item's start column so the
 * trailing un-highlight writes in menu_main() stay within the 40-column row.
 *
 * @param y Screen row to draw the bar on.
 * @return (none) -- result is written to menubar.ypos and menubar.xstart[].
 */
void menu_placebar(uint8_t y)
{
    uint8_t *row = MENU_ROW(y);
    row[0] = A_FWBLACK;
    row[1] = A_BGGREEN;
    row[2] = A_STD;    // reset charset-mode latch (persists into all pulldown rows below)
    for (uint8_t x = 3; x < SCREEN_COLS; x++) row[x] = CH_SPACE;

    menubar.ypos = y;

    uint8_t xcoord = 1;
    for (uint8_t i = 0; i < MENUBAR_MAXOPTIONS; i++)
    {
        uint8_t len = (uint8_t)strlen(menubar.titles[i]);
        // Keep room for the trailing un-highlight writes in menu_main()
        // (row[xi+1+len] and row[xi+2+len]): require xi <= 37-len so
        // row[xi+2+len] <= row[39], the last byte of the 40-byte row.
        if (xcoord + len + 2 > 39)
            xcoord = 37 - len;
        menubar.xstart[i] = xcoord;
        menu_screen_puts(xcoord + 1, y, menubar.titles[i]);
        xcoord += (uint8_t)(len + 2);
    }
}

// -------------------------------------------------------------------------
// Pulldown menu
// -------------------------------------------------------------------------

/**
 * Draw one pulldown item row at screen row y, columns xpos-1..xpos+2+width,
 * showing pulldown_titles[menunumber][item] either highlighted (yellow
 * paper, '-' prefix) or unhighlighted (cyan paper, ' ' prefix), padded with
 * spaces to width and followed by endcolor.
 *
 * @param y          Screen row to draw on.
 * @param xpos       Column of the highlight-bar ink byte (item text starts
 *                    at xpos+2).
 * @param menunumber Index into pulldown_titles[]/pulldown_options[].
 * @param item       Index of the item within this pulldown to draw.
 * @param selected   Non-zero to draw this item highlighted (yellow paper,
 *                    '-' prefix), zero for unhighlighted (cyan paper,
 *                    ' ' prefix).
 * @param endcolor   Attribute byte written immediately after the padded
 *                    label (A_BGBLACK for top-menu pulldowns, A_BGWHITE for
 *                    popup sub-menus).
 * @param width      Width (in characters) every row is padded to -- the
 *                    longest item label in this pulldown (computed by the
 *                    caller from the live strings, NOT PULLDOWN_MAXLENGTH),
 *                    so every row's highlight bar is the same width
 *                    regardless of that item's own label length.
 * @return (none)
 */
static void menu_draw_item(uint8_t y, uint8_t xpos, uint8_t menunumber,
                            uint8_t item, uint8_t selected, uint8_t endcolor,
                            uint8_t width)
{
    uint8_t *row   = MENU_ROW(y);
    // WORKAROUND for an Oscar64 -O2 whole-program register-allocator bug
    // (carried over from locifilemanager-v2, see local reference above):
    // without this dummy sprintf, menu_pulldown()'s call-site save/restore
    // set can be under-counted, clobbering a live caller variable and
    // producing stray garbage on screen. The sprintf call's register
    // pressure forces the correct (larger) save set. Writes into
    // menu_regpressure_scratch[] (a real static buffer, see its own
    // declaration above for why this is no longer a magic address)
    // -- do not remove or "clean up" this call without re-testing every
    // pulldown in the emulator (corruption appears immediately on open).
    uint8_t *debug = menu_regpressure_scratch;
    uint8_t  paper = selected ? A_BGYELLOW : A_BGCYAN;
    uint8_t  pfx   = selected ? '-' : ' ';
    const char *title = pulldown_titles[menunumber][item];
    uint8_t  len   = (uint8_t)strlen(title);

    row[xpos - 1] = A_FWBLACK;   // ink (0x00 -- direct write)
    row[xpos]     = paper;
    row[xpos + 1] = pfx;
    menu_screen_putn(xpos + 2, y, (const uint8_t *)title, len);
    // Pad to width+1, not width -- the extra column is a trailing space
    // of breathing room after the longest item's own text (user-
    // requested 2026-06-20), not just after shorter items. Safe against
    // the screen's 40-column width even at the worst case (xpos clamped
    // to 20, width up to PULLDOWN_MAXLENGTH-1=16): rightmost column
    // becomes xpos+2+width+1 = 20+2+16+1 = 39, the last valid column.
    for (uint8_t x = len; x < (uint8_t)(width + 1); x++)
        row[xpos + 2 + x] = CH_SPACE;
    row[xpos + 2 + width + 1] = endcolor;

    sprintf((char *)debug, "draw_item: y=%u, menunumber=%u, item=%u, sel=%u, title=\"%s\", width=%u", y, menunumber, item, selected, title, width);
}

/**
 * Open a pulldown menu at (xpos, ypos) listing pulldown_titles[menunumber],
 * saving the covered rows first (menu_winsave()) and restoring them on exit
 * (menu_winrestore()). Highlights the current item and handles KEY_UP/DOWN
 * to move the selection, KEY_ENTER to choose, and (for top-menu pulldowns,
 * menunumber < MENUBAR_MAXOPTIONS) KEY_LEFT/KEY_RIGHT to request the
 * previous/next bar item. If escapable, KEY_ESC cancels.
 *
 * @param xpos       Column of the highlight-bar ink byte for every item row.
 * @param ypos       First screen row of the pulldown (item 1).
 * @param menunumber Index into pulldown_titles[]/pulldown_options[] for the
 *                    pulldown to display.
 * @param escapable  Non-zero to allow KEY_ESC to cancel (returns
 *                    MENU_CANCEL); zero to require a choice.
 * @return 1..pulldown_options[menunumber] (the chosen item), MENU_CANCEL (0)
 *         if escapable and ESC was pressed, or MENU_LEFT_ARROW/
 *         MENU_RIGHT_ARROW if a top-menu pulldown's KEY_LEFT/KEY_RIGHT was
 *         pressed.
 */
uint8_t menu_pulldown(uint8_t xpos, uint8_t ypos,
                      uint8_t menunumber, uint8_t escapable)
{
    uint8_t height     = (uint8_t)pulldown_options[menunumber];
    uint8_t topmenu    = (menunumber < MENUBAR_MAXOPTIONS) ? 1 : 0;
    uint8_t endcolor   = topmenu ? A_BGBLACK : A_BGWHITE;
    uint8_t menuchoice = 1;
    uint8_t exit_flag  = 0;

    // Longest current item label in THIS pulldown -- drives a consistent
    // highlight-bar width. Computed from the live strings (some entries
    // are sprintf'd at runtime with varying lengths), not assumed from
    // PULLDOWN_MAXLENGTH, so short pulldowns (e.g. Yes/No) get a compact
    // bar instead of one stretched to the global max.
    uint8_t width = 0;
    for (uint8_t i = 0; i < height; i++)
    {
        uint8_t l = (uint8_t)strlen(pulldown_titles[menunumber][i]);
        if (l > width) width = l;
    }

    menu_winsave(ypos, height, 1);

    // Blank every covered row in full (col 0/1 = ink/paper attribute
    // bytes, matching include/charwin.c's row_setattr() convention; cols
    // 2-39 = spaces) before drawing items -- without this, only the
    // narrow strip menu_draw_item() itself writes (xpos-1 onward, up to
    // the item's own padded width + endcolor) is ever touched, leaving
    // whatever was on screen before showing through to the left of
    // xpos-1 and to the right past the drawn item (user-reported,
    // 2026-06-20: background canvas content visibly bleeding through
    // every pulldown, not just Are-you-sure/message popups, which had
    // their own separate fix in menu_wininit()).
    {
        uint8_t blankink = topmenu ? A_FWWHITE : A_FWBLACK;
        for (uint8_t y = 0; y < height; y++)
        {
            uint8_t *row = MENU_ROW(ypos + y);
            row[0] = blankink;
            row[1] = endcolor;
            row[2] = A_STD;    // reset charset-mode latch (doesn't reset per scanline)
            for (uint8_t x = 3; x < SCREEN_COLS; x++)
                row[x] = CH_SPACE;
        }
    }

    // Draw all items (unselected)
    for (uint8_t y = 0; y < height; y++)
        menu_draw_item(ypos + y, xpos, menunumber, y, 0, endcolor, width);

    do {
        // Materialize the row index in its own variable so the compiler
        // can't (mis-)share the "ypos + menuchoice" subexpression between
        // the highlight-draw call and the un-highlight call below.
        uint8_t row_y = (uint8_t)(ypos + menuchoice - 1);

        // Highlight current choice
        menu_draw_item(row_y, xpos, menunumber,
                       menuchoice - 1, 1, endcolor, width);

        uint8_t key = key_read();

        // Un-highlight before acting
        menu_draw_item(row_y, xpos, menunumber,
                       menuchoice - 1, 0, endcolor, width);

        switch (key)
        {
        case KEY_ENTER:
            exit_flag = 1;
            break;

        case KEY_ESC:
            if (escapable)
            {
                exit_flag  = 1;
                menuchoice = MENU_CANCEL;
            }
            break;

        case KEY_LEFT:
            if (topmenu)
            {
                exit_flag  = 1;
                menuchoice = MENU_LEFT_ARROW;
            }
            break;

        case KEY_RIGHT:
            if (topmenu)
            {
                exit_flag  = 1;
                menuchoice = MENU_RIGHT_ARROW;
            }
            break;

        case KEY_UP:
            if (menuchoice > 1) menuchoice--;
            else menuchoice = height;
            break;

        case KEY_DOWN:
            if (menuchoice < height) menuchoice++;
            else menuchoice = 1;
            break;

        default:
            break;
        }
    } while (!exit_flag);

    menu_winrestore();
    return menuchoice;
}

// -------------------------------------------------------------------------
// Main menu bar loop
// -------------------------------------------------------------------------

/**
 * Run the menu bar navigation loop: highlight bar items left/right with
 * KEY_LEFT/KEY_RIGHT, and on KEY_ENTER open the corresponding pulldown
 * (menu_pulldown()). If the pulldown returns MENU_LEFT_ARROW/
 * MENU_RIGHT_ARROW, move to the adjacent bar item and reopen its pulldown;
 * otherwise return the encoded choice. KEY_ESC at the bar level exits with
 * menuoptionchoice=99 (so the return value is menubarchoice*10+99, >= 99).
 *
 * @return menubarchoice*10 + menuoptionchoice (1..49 = a choice), or
 *         menubarchoice*10 + 99 (>= 99) if ESC was pressed at the bar level.
 */
uint8_t menu_main(void)
{
    uint8_t menubarchoice    = 1;
    uint8_t menuoptionchoice = 0;

    menu_placebar(menubar.ypos);

    do {
        uint8_t key;
        do {
            // Highlight current bar item
            uint8_t  xi  = menubar.xstart[menubarchoice - 1];
            uint8_t  y   = menubar.ypos;
            uint8_t *row = MENU_ROW(y);
            uint8_t  len = (uint8_t)strlen(menubar.titles[menubarchoice - 1]);

            row[xi] = A_BGWHITE;
            menu_screen_puts(xi + 1, y, menubar.titles[menubarchoice - 1]);
            row[xi + 1 + len] = CH_SPACE;
            row[xi + 2 + len] = A_BGGREEN;

            key = key_read();

            // Un-highlight
            row[xi] = A_BGGREEN;
            menu_screen_puts(xi + 1, y, menubar.titles[menubarchoice - 1]);

            if (key == KEY_LEFT)
            {
                if (menubarchoice > 1) menubarchoice--;
                else menubarchoice = MENUBAR_MAXOPTIONS;
            }
            else if (key == KEY_RIGHT)
            {
                menubarchoice++;
                if (menubarchoice > MENUBAR_MAXOPTIONS) menubarchoice = 1;
            }
        } while (key != KEY_ENTER && key != KEY_ESC);

        if (key != KEY_ESC)
        {
            // Clamp xpos so pulldown fits within 40-col screen
            // Item occupies cols xpos-1..xpos+19; rightmost = xpos+19 <= 39 -> xpos <= 20
            uint8_t xpos = menubar.xstart[menubarchoice - 1];
            if (xpos > 20) xpos = 20;

            menuoptionchoice = menu_pulldown(xpos, menubar.ypos + 1,
                                             menubarchoice - 1, 1);

            if (menuoptionchoice == MENU_LEFT_ARROW)
            {
                menuoptionchoice = 0;
                if (menubarchoice > 1) menubarchoice--;
                else menubarchoice = MENUBAR_MAXOPTIONS;
            }
            else if (menuoptionchoice == MENU_RIGHT_ARROW)
            {
                menuoptionchoice = 0;
                menubarchoice++;
                if (menubarchoice > MENUBAR_MAXOPTIONS) menubarchoice = 1;
            }
        }
        else
        {
            menuoptionchoice = 99;
        }
    } while (menuoptionchoice == 0);

    return (uint8_t)(menubarchoice * 10 + menuoptionchoice);
}

// -------------------------------------------------------------------------
// Popup helpers
// -------------------------------------------------------------------------

/**
 * Show a popup with message followed by "Are you sure?" and a Yes/No
 * pulldown (MENU_YESNO), then restore the screen.
 *
 * @param message NUL-terminated message text to display above the prompt.
 * @return 1 if Yes was chosen, 2 if No.
 */
uint8_t menu_areyousure(const char *message)
{
    uint8_t choice;
    menu_winsave(8, 6, 1);
    menu_wininit(8, 6);
    menu_screen_puts(7, 9,  message);
    menu_screen_puts(7, 10, MSG_MENU_AREYOUSURE);
    choice = menu_pulldown(20, 11, MENU_YESNO, 0);
    menu_winrestore();
    return choice;
}

/**
 * Show a popup with message and a "press a key" prompt, wait for a keypress,
 * then restore the screen.
 *
 * @param message NUL-terminated message text to display.
 * @return (none)
 */
void menu_messagepopup(const char *message)
{
    menu_winsave(8, 6, 1);
    menu_wininit(8, 6);
    menu_screen_puts(7, 9,  message);
    menu_screen_puts(7, 11, MSG_MENU_PRESSAKEY);
    key_read();
    menu_winrestore();
}
