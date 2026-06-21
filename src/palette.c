// palette.c - Palette popup for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'p' from main mode (src/editor.c). Near-full-width popup
// (cols 2-39, rows 0-12) showing the 10 favourites, the full standard
// charset (0x20-0x7F) and the full alternate charset (0x20-0x7F, optionally
// remapped via visualchar[] -- "visual charmap mode"), for selecting
// app.plotscreencode/app.plotaltchar. Layout, cursor-wrap navigation and key
// bindings are ported from V1 OricScreenEditor's palette() (local reference
// at /home/xahmol/git/OricScreenEditor and the archived
// nonworkingcc65:src/palette.c), adapted to OSE's OricCharWin popup model
// and to a 13-row grid (V1 used 12 rows; OSE adds a 13th row so the full
// 0x20-0x7F alternate range is reachable even with visualmap on -- see
// pal_screencode()). The cursor-wrap post-checks below were re-derived for
// the 13-row layout to avoid an out-of-bounds favourites[] access that V1's
// original 12-row post-check order could reach when wrapping DOWN from the
// last alternate row into a favourites column > 9.
//
// Opts OUT of the charset-swap mechanism (menu_winsave(..., 0)), same as
// charsetedit -- changed 2026-06-21 (user report: "palette view should not
// be charset safe, otherwise you cannot select properly the redefined
// chars"). The palette's whole purpose is letting the user pick a
// character from the *actual current* Std/Alt content, including their
// own edits; swapping to safe/ROM content for both banks (the general
// mechanism's scope since charsetswap_enter()/exit() were merged to cover
// Alt for every popup, not just Information > Version) would hide exactly
// the redefined glyphs the user needs to see and select. Accepts the same
// minor risk charsetedit already does: the palette's own "Fav:"/"Std:"/
// "Alt:" labels render via the live (unprotected) Std charset too, so they
// could look odd if the user has heavily redefined those specific glyphs.

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "strings.h"
#include "input.h"
#include "palette.h"

// -------------------------------------------------------------------------
// Popup window layout (38 cols x 13 rows, screen cols 2-39, rows 0-12)
// -------------------------------------------------------------------------

#define PAL_WIN_SX   2
#define PAL_WIN_SY   0
#define PAL_WIN_WX  38
#define PAL_WIN_WY  13

#define PAL_LABEL_X   0   // "Fav:"/"Std:"/"Alt:" (4 chars)
#define PAL_ATTR_X    4   // A_STD or A_ALT charset-bank byte for this row
#define PAL_GRID_X0   5   // window-relative x of grid column 0
#define PAL_GRID_STEP 2   // columns between grid cells

#define PAL_ROW_FAV    0  // favourites row (10 cols)
#define PAL_ROW_STD0   1  // first standard-charset row (label "Std:")
#define PAL_ROW_ALT0   7  // first alternate-charset row (label "Alt:")
#define PAL_ROWS      13  // 1 fav + 6 std + 6 alt
#define PAL_COLS      16  // grid columns per std/alt row
#define PAL_FAV_COLS  10  // grid columns in the favourites row

// -------------------------------------------------------------------------
// visualchar[80]: PETSCII-style visual ordering for the alternate charset
// -------------------------------------------------------------------------

// Ported verbatim from V1 OricScreenEditor (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c, visualchar[] table),
// credited there to jab / Artline Designs (Jaakko Luoto) for the palette
// mode and PETSCII visual mode (see README.md Credits). No changes other
// than C array syntax. Covers screencodes 0x20-0x6F (rows PAL_ROW_ALT0 ..
// PAL_ROWS-2 below); the final alternate row (0x70-0x7F) is always identity,
// matching V1's y<5 condition in palette_draw().
static const uint8_t visualchar[80] =
{
    0x37,0x4B,0x33,0x43,0x3F,0x4F,0x27,0x2B,0x3C,0x4C,0x40,0x30,0x48,0x34,0x24,0x28,
    0x55,0x5A,0x51,0x52,0x5D,0x5E,0x54,0x58,0x2D,0x2E,0x22,0x21,0x2A,0x25,0x53,0x5F,
    0x36,0x49,0x26,0x29,0x56,0x59,0x57,0x5B,0x3D,0x4E,0x32,0x41,0x50,0x5C,0x2C,0x2F,
    0x45,0x3A,0x44,0x38,0x47,0x3B,0x46,0x39,0x3E,0x4D,0x31,0x42,0x23,0x35,0x4A,0x20,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F
};

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static OricCharWin pal_win;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Screencode shown at grid cell (rowsel, colsel).
 *
 * Row 0 (PAL_ROW_FAV) reads app.favourites[colsel] (10 entries). Rows 1-6
 * are the standard charset, 0x20-0x7F in row-major order. Rows 7-12 are the
 * alternate charset: if app.visualmap is set, rows 7-11 are remapped via
 * visualchar[] (covering 0x20-0x6F); row 12 (and all alternate rows when
 * app.visualmap is clear) is the identity 0x20-0x7F mapping.
 *
 * @param rowsel Grid row, 0..PAL_ROWS-1.
 * @param colsel Grid column, 0..PAL_COLS-1 (0..PAL_FAV_COLS-1 for row 0).
 * @return Screencode to display/select for this cell.
 */
static uint8_t pal_screencode(uint8_t rowsel, uint8_t colsel)
{
    if (rowsel == PAL_ROW_FAV)
        return app.favourites[colsel];

    if (rowsel < PAL_ROW_ALT0)
        return (uint8_t)(0x20 + colsel + (rowsel - PAL_ROW_STD0) * PAL_COLS);

    if (app.visualmap && rowsel < PAL_ROWS - 1)
        return visualchar[colsel + (rowsel - PAL_ROW_ALT0) * PAL_COLS];

    return (uint8_t)(0x20 + colsel + (rowsel - PAL_ROW_ALT0) * PAL_COLS);
}

/**
 * Draw (or erase) the cursor highlight on grid cell (rowsel, colsel) by
 * XOR-ing 0x80 onto its screencode (inverse video), matching charsetedit's
 * cursor-rendering convention.
 *
 * @param rowsel    Grid row, 0..PAL_ROWS-1.
 * @param colsel    Grid column, 0..PAL_COLS-1 (0..PAL_FAV_COLS-1 for row 0).
 * @param highlight 1 to draw inverted (cursor here), 0 for normal video.
 * @return (none)
 */
static void pal_draw_cell(uint8_t rowsel, uint8_t colsel, uint8_t highlight)
{
    uint8_t code = pal_screencode(rowsel, colsel);
    uint8_t x = (uint8_t)(PAL_GRID_X0 + colsel * PAL_GRID_STEP);

    if (highlight) code ^= 0x80;
    cwin_putat_char(&pal_win, x, rowsel, code);
}

/**
 * Draw one full grid row: its label (only rows PAL_ROW_FAV/PAL_ROW_STD0/
 * PAL_ROW_ALT0 have one), the A_STD/A_ALT charset-bank byte at PAL_ATTR_X,
 * and every grid cell in normal video.
 *
 * @param rowsel Grid row to (re)draw, 0..PAL_ROWS-1.
 * @return (none)
 */
static void pal_draw_row(uint8_t rowsel)
{
    uint8_t ncols = (rowsel == PAL_ROW_FAV) ? PAL_FAV_COLS : PAL_COLS;
    uint8_t col;

    switch (rowsel)
    {
    case PAL_ROW_FAV:  cwin_putat_string(&pal_win, PAL_LABEL_X, rowsel, MSG_PALETTE_FAV); break;
    case PAL_ROW_STD0: cwin_putat_string(&pal_win, PAL_LABEL_X, rowsel, MSG_PALETTE_STD); break;
    case PAL_ROW_ALT0: cwin_putat_string(&pal_win, PAL_LABEL_X, rowsel, MSG_PALETTE_ALT); break;
    default: break;
    }

    cwin_putat_char(&pal_win, PAL_ATTR_X, rowsel, (rowsel >= PAL_ROW_ALT0) ? A_ALT : A_STD);

    for (col = 0; col < ncols; col++)
        pal_draw_cell(rowsel, col, 0);
}

/**
 * Apply the cursor-wrap post-checks after a cursor move that may have left
 * *rowsel/*colsel outside the valid grid (rowsel 0..PAL_ROWS-1, colsel
 * 0..PAL_COLS-1, but colsel 0..PAL_FAV_COLS-1 when rowsel==0). Order matters:
 * checking rowsel overflow first, then the favourites-row column limit, then
 * the general column limit, correctly wraps every direction (including DOWN
 * from the last row into a favourites column > 9, which an unordered/V1-
 * original check sequence would leave as an out-of-bounds favourites[]
 * index).
 *
 * @param rowsel Pointer to the current grid row, updated in place.
 * @param colsel Pointer to the current grid column, updated in place.
 * @return (none)
 */
static void pal_wrap(uint8_t *rowsel, uint8_t *colsel)
{
    if (*rowsel > PAL_ROWS - 1) *rowsel = 0;

    if (*rowsel == 0 && *colsel > PAL_FAV_COLS - 1)
    {
        *colsel = 0;
        *rowsel = 1;
    }

    if (*colsel > PAL_COLS - 1)
    {
        *colsel = 0;
        *rowsel = (*rowsel == PAL_ROWS - 1) ? 0 : (uint8_t)(*rowsel + 1);
    }
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

/**
 * Palette popup, entered via 'p' from main mode. Opens a near-full-width
 * popup (cols 2-39, rows 0-12) showing the favourites row, the full standard
 * charset and the full alternate charset (visualmap-dependent), and loops
 * handling: cursor keys move/wrap across all three sections; SPACE/ENTER
 * select the highlighted cell into app.plotscreencode/app.plotaltchar and
 * close the popup; '0'-'9' store the highlighted cell's screencode into
 * app.favourites[n]; 'v' toggles app.visualmap and redraws the alternate
 * section; FUNCT+6 toggles the statusbar; ESC closes the popup leaving
 * app.plotscreencode/app.plotaltchar unchanged.
 *
 * Initial cursor position mirrors V1: row = (plotscreencode-0x20)/16 + 1,
 * col = (plotscreencode-0x20)%16 (clamped to 0x20-0x7F), which always lands
 * in the standard-charset rows regardless of app.plotaltchar.
 *
 * @return (none)
 */
void palette_run(void)
{
    uint8_t rowsel, colsel;
    uint8_t code = app.plotscreencode;
    uint8_t key, i;

    if (code < 0x20) code = 0x20;
    if (code > 0x7F) code = 0x7F;
    code = (uint8_t)(code - 0x20);
    rowsel = (uint8_t)(code / PAL_COLS + PAL_ROW_STD0);
    colsel = (uint8_t)(code % PAL_COLS);

    menu_winsave(PAL_WIN_SY, PAL_WIN_WY, 0);
    cwin_init(&pal_win, PAL_WIN_SX, PAL_WIN_SY, PAL_WIN_WX, PAL_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&pal_win);

    for (i = 0; i < PAL_ROWS; i++) pal_draw_row(i);
    pal_draw_cell(rowsel, colsel, 1);

    for (;;)
    {
        key = key_read();

        switch (key)
        {
        case KEY_RIGHT:
            pal_draw_cell(rowsel, colsel, 0);
            colsel++;
            pal_wrap(&rowsel, &colsel);
            pal_draw_cell(rowsel, colsel, 1);
            break;

        case KEY_LEFT:
            pal_draw_cell(rowsel, colsel, 0);
            if (colsel > 0)
            {
                colsel--;
            }
            else
            {
                colsel = PAL_COLS - 1;
                if (rowsel > 0)
                {
                    rowsel--;
                    if (rowsel == 0) colsel = PAL_FAV_COLS - 1;
                }
                else
                {
                    rowsel = PAL_ROWS - 1;
                }
            }
            pal_wrap(&rowsel, &colsel);
            pal_draw_cell(rowsel, colsel, 1);
            break;

        case KEY_DOWN:
            pal_draw_cell(rowsel, colsel, 0);
            rowsel++;
            pal_wrap(&rowsel, &colsel);
            pal_draw_cell(rowsel, colsel, 1);
            break;

        case KEY_UP:
            pal_draw_cell(rowsel, colsel, 0);
            if (rowsel > 0) rowsel--;
            else rowsel = PAL_ROWS - 1;
            pal_wrap(&rowsel, &colsel);
            pal_draw_cell(rowsel, colsel, 1);
            break;

        case KEY_SPACE:
        case KEY_ENTER:
            app.plotscreencode = pal_screencode(rowsel, colsel);
            app.plotaltchar    = (rowsel >= PAL_ROW_ALT0) ? 1 : 0;
            menu_winrestore();
            return;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            if (rowsel > PAL_ROW_FAV)
            {
                app.favourites[key - '0'] = pal_screencode(rowsel, colsel);
                pal_draw_row(PAL_ROW_FAV);
            }
            break;

        case 'v':
            app.visualmap = !app.visualmap;
            for (i = PAL_ROW_ALT0; i < PAL_ROWS; i++) pal_draw_row(i);
            pal_draw_cell(rowsel, colsel, 1);
            break;

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
            break;

        case KEY_ESC:
            menu_winrestore();
            return;

        default:
            break;
        }
    }
}
