// colourpicker.c - Colour picker popup for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'c' from main mode (src/editor.c). New OSE-LOCI feature over
// V1 (see README.md "Planned feature additions over V1": "Enhanced palette
// mode also showing inverse ink/paper colour combinations"). Popup (cols
// 2-37, rows 0-12) showing an 8x8 ink x paper grid -- each cell shows both
// the normal (paper-colour) and inverse (ink-colour) swatch for that
// combination -- plus live Ink:/Paper:/Result: feedback lines, for selecting
// app.plotink/app.plotpaper.
//
// Adapted from V1's colourpicker()/colorpicker_cursorplot() (local reference
// at /home/xahmol/git/OricScreenEditor, archived nonworkingcc65:
// src/colorpicker.c): same 8x8 ink x paper grid and Ink:/Paper:/Result:
// feedback concept, RETURN/ESC to select/cancel. The per-cell border-drawing
// cursor (colorpicker_cursorplot, 4 ORIC_HChar/cputcxy calls per move) is
// replaced with a simpler XOR-swap of the two swatch characters within the
// 4-column cell (matches charsetedit's ^0x80 cursor convention).
//
// Opts IN to the charset-swap mechanism (menu_winsave(..., 1)) -- the swatch
// glyphs (CH_SPACE/CH_INVSPACE) are plain Std-charset chars, no live-edit-
// preview requirement.

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "strings.h"
#include "input.h"
#include "colourpicker.h"

// -------------------------------------------------------------------------
// Popup window layout (36 cols x 13 rows, screen cols 2-37, rows 0-12)
// -------------------------------------------------------------------------

#define CP_WIN_SX   2
#define CP_WIN_SY   0
#define CP_WIN_WX  36
#define CP_WIN_WY  13

#define CP_TITLE_X    2   // x of MSG_COLOURPICKER_TITLE
#define CP_GRID_X0    2   // window-relative x of grid column 0 (ink=0)
#define CP_CELL_STEP  4   // columns per grid cell (ink attr, paper attr, 2 swatches)
#define CP_RESET_X   34   // x of the end-of-row ink/paper reset pair

#define CP_ROW_TITLE   0
#define CP_ROW_GRID0   1  // first grid row (paper=0); rows 1-8 = paper 0-7
#define CP_ROW_INK    10  // "Ink:    N" feedback line
#define CP_ROW_PAPER  11  // "Paper:  N" feedback line
#define CP_ROW_RESULT 12  // "Result:" normal/inverse preview line

#define CP_SWATCH_X    9  // x of the 1-cell swatch on the Ink:/Paper: rows

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static OricCharWin cp_win;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Draw (or erase) the cursor highlight on grid cell (ink, paper) by swapping
 * its two swatch characters (CH_SPACE <-> CH_INVSPACE), matching
 * charsetedit's ^0x80 cursor-rendering convention applied to a 2-char cell.
 *
 * @param ink       Ink value, 0-7 (grid column).
 * @param paper     Paper value, 0-7 (grid row).
 * @param highlight 1 to draw the swapped/highlighted pair, 0 for normal
 *                  (normal=paper-colour swatch first, inverse=ink-colour
 *                  swatch second).
 * @return (none)
 */
static void cp_draw_cell(uint8_t ink, uint8_t paper, uint8_t highlight)
{
    uint8_t x = (uint8_t)(CP_GRID_X0 + ink * CP_CELL_STEP);
    uint8_t y = (uint8_t)(CP_ROW_GRID0 + paper);
    uint8_t normal  = highlight ? CH_INVSPACE : CH_SPACE;
    uint8_t inverse = highlight ? CH_SPACE : CH_INVSPACE;

    cwin_putat_char(&cp_win, x, y, ink);
    cwin_putat_char(&cp_win, (uint8_t)(x + 1), y, (uint8_t)(16 + paper));
    cwin_putat_char(&cp_win, (uint8_t)(x + 2), y, normal);
    cwin_putat_char(&cp_win, (uint8_t)(x + 3), y, inverse);
}

/**
 * Draw the title and the full 8x8 ink x paper grid in normal (unhighlighted)
 * video, including the end-of-row ink/paper reset pair so the spare columns
 * to the right of the grid don't inherit the last cell's colours.
 *
 * @return (none)
 */
static void cp_draw_grid(void)
{
    uint8_t ink, paper, y;

    cwin_putat_string(&cp_win, CP_TITLE_X, CP_ROW_TITLE, MSG_COLOURPICKER_TITLE);

    for (paper = 0; paper < 8; paper++)
    {
        y = (uint8_t)(CP_ROW_GRID0 + paper);

        for (ink = 0; ink < 8; ink++)
            cp_draw_cell(ink, paper, 0);

        cwin_putat_char(&cp_win, CP_RESET_X, y, A_FWBLACK);
        cwin_putat_char(&cp_win, (uint8_t)(CP_RESET_X + 1), y, A_BGWHITE);
    }
}

/**
 * (Re)draw the Ink:/Paper:/Result: feedback lines for the given cursor
 * position. Called once at popup entry and after every cursor move, so the
 * feedback always reflects the highlighted cell rather than the
 * not-yet-committed app.plotink/app.plotpaper.
 *
 * @param ink   Ink value, 0-7, to show in the feedback lines.
 * @param paper Paper value, 0-7, to show in the feedback lines.
 * @return (none)
 */
static void cp_draw_feedback(uint8_t ink, uint8_t paper)
{
    cwin_putat_printf(&cp_win, 0, CP_ROW_INK, MSG_COLOURPICKER_INK_FMT, ink);
    cwin_putat_char(&cp_win, CP_SWATCH_X, CP_ROW_INK, (uint8_t)(16 + ink));
    cwin_putat_char(&cp_win, (uint8_t)(CP_SWATCH_X + 1), CP_ROW_INK, A_BGWHITE);

    cwin_putat_printf(&cp_win, 0, CP_ROW_PAPER, MSG_COLOURPICKER_PAPER_FMT, paper);
    cwin_putat_char(&cp_win, CP_SWATCH_X, CP_ROW_PAPER, (uint8_t)(16 + paper));
    cwin_putat_char(&cp_win, (uint8_t)(CP_SWATCH_X + 1), CP_ROW_PAPER, A_BGWHITE);

    cwin_putat_string(&cp_win, 0, CP_ROW_RESULT, MSG_COLOURPICKER_RESULT);
    cwin_putat_char(&cp_win, 7, CP_ROW_RESULT, ink);
    cwin_putat_char(&cp_win, 8, CP_ROW_RESULT, (uint8_t)(16 + paper));
    cwin_putat_char(&cp_win, 9, CP_ROW_RESULT, CH_SPACE);
    cwin_putat_char(&cp_win, 10, CP_ROW_RESULT, CH_INVSPACE);
    cwin_putat_char(&cp_win, 11, CP_ROW_RESULT, A_FWBLACK);
    cwin_putat_char(&cp_win, 12, CP_ROW_RESULT, A_BGWHITE);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

/**
 * Colour picker popup, entered via 'c' from main mode. Opens a popup (cols
 * 2-37, rows 0-12) showing the title, an 8x8 ink x paper grid (each cell
 * showing the normal/paper-colour and inverse/ink-colour swatches for that
 * combination) and live Ink:/Paper:/Result: feedback lines, and loops
 * handling: cursor keys move/wrap the highlighted cell (LEFT/RIGHT cycle
 * ink, UP/DOWN cycle paper, both wrap 0-7); SPACE/ENTER commit the
 * highlighted cell's ink/paper into app.plotink/app.plotpaper and close the
 * popup; FUNCT+6 toggles the statusbar; ESC closes the popup leaving
 * app.plotink/app.plotpaper unchanged.
 *
 * Initial cursor position is (app.plotink, app.plotpaper).
 *
 * @return (none)
 */
void colourpicker_run(void)
{
    uint8_t ink = app.plotink;
    uint8_t paper = app.plotpaper;
    uint8_t key;

    menu_winsave(CP_WIN_SY, CP_WIN_WY, 1);
    cwin_init(&cp_win, CP_WIN_SX, CP_WIN_SY, CP_WIN_WX, CP_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&cp_win);

    cp_draw_grid();
    cp_draw_cell(ink, paper, 1);
    cp_draw_feedback(ink, paper);

    for (;;)
    {
        key = key_read();

        switch (key)
        {
        case KEY_RIGHT:
            cp_draw_cell(ink, paper, 0);
            ink = (ink < 7) ? (uint8_t)(ink + 1) : 0;
            cp_draw_cell(ink, paper, 1);
            cp_draw_feedback(ink, paper);
            break;

        case KEY_LEFT:
            cp_draw_cell(ink, paper, 0);
            ink = (ink > 0) ? (uint8_t)(ink - 1) : 7;
            cp_draw_cell(ink, paper, 1);
            cp_draw_feedback(ink, paper);
            break;

        case KEY_DOWN:
            cp_draw_cell(ink, paper, 0);
            paper = (paper < 7) ? (uint8_t)(paper + 1) : 0;
            cp_draw_cell(ink, paper, 1);
            cp_draw_feedback(ink, paper);
            break;

        case KEY_UP:
            cp_draw_cell(ink, paper, 0);
            paper = (paper > 0) ? (uint8_t)(paper - 1) : 7;
            cp_draw_cell(ink, paper, 1);
            cp_draw_feedback(ink, paper);
            break;

        case KEY_SPACE:
        case KEY_ENTER:
            app.plotink = ink;
            app.plotpaper = paper;
            menu_winrestore();
            return;

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
