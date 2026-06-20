// colourpicker.c - Colour picker popup for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'c' from main mode (src/editor.c). New OSE-LOCI feature over
// V1 (see README.md "Planned feature additions over V1": "Enhanced palette
// mode also showing inverse ink/paper colour combinations"). Popup showing
// an 8x8 ink x paper grid, split into two 4-wide ink halves stacked
// vertically per paper value (16 grid rows total: paper 0's ink 0-3, paper
// 0's ink 4-7, paper 1's ink 0-3, ...) -- each cell shows both the normal
// (paper-colour) and inverse (ink-colour) swatch for that combination --
// plus live Ink:/Paper:/Result: feedback lines, for selecting
// app.plotink/app.plotpaper.
//
// Adapted from V1's colourpicker()/colorpicker_cursorplot() (local reference
// at /home/xahmol/git/OricScreenEditor, archived nonworkingcc65:
// src/colorpicker.c): same ink x paper grid and Ink:/Paper:/Result:
// feedback concept, RETURN/ESC to select/cancel. The per-cell border-drawing
// cursor (colorpicker_cursorplot, 4 ORIC_HChar/cputcxy calls per move) is
// replaced with a simpler XOR-swap of the two swatch characters within the
// cell (matches charsetedit's ^0x80 cursor convention).
//
// **Hardware rendering bug found and fixed (real-hardware-only, never
// reproduced in emulation -- see CLAUDE.md "Colour picker rendering fix")**:
// the original 4-byte-per-cell layout (`ink, paper+16, swatch, swatch`,
// 8 cells across one row) dropped a leading attribute byte V1's own
// version always has (`A_BGWHITE, ink, paper+16, '-', '-'+128`). That
// leading byte isn't a no-op: an attribute byte's own character-cell-wide
// slot renders using whatever colour state was active *before* it takes
// effect, so without it, each cell's first attribute byte visibly showed
// the *previous* cell's actual colour for one cell-width, producing
// exactly the banding artifact reported. Restoring V1's reset-byte pattern
// needs a 5th column per cell, which doesn't fit if all 8 ink values share
// one row (8*5=40 columns, plus this popup reserves 2 border columns V1's
// own window system didn't) -- hence the 4-wide/16-row split here, keeping
// both swatches and the reset byte without exceeding the screen width.
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
// Popup window layout
// -------------------------------------------------------------------------
// 4 ink columns x 16 grid rows (8 paper values x 2 ink-halves), each cell
// 5 columns wide (reset byte, ink attr, paper attr, 2 swatches).

#define CP_WIN_SX   2
#define CP_WIN_SY   0
#define CP_WIN_WX  36
#define CP_WIN_WY  20

#define CP_TITLE_X    2   // x of MSG_COLOURPICKER_TITLE
#define CP_GRID_X0    2   // window-relative x of grid column 0 (ink%4==0)
#define CP_CELL_STEP  5   // columns per grid cell (reset, ink, paper, 2 swatches)
#define CP_INKS_PER_ROW 4 // ink values per grid row (the split: 0-3, then 4-7)
#define CP_RESET_X   (CP_GRID_X0 + CP_INKS_PER_ROW * CP_CELL_STEP) // end-of-row reset pair

#define CP_ROW_TITLE   0
#define CP_ROW_GRID0   1  // first grid row; 16 rows follow (paper 0's ink 0-3, 4-7, paper 1's ..., ...)
#define CP_ROW_INK    17  // "Ink:    N" feedback line
#define CP_ROW_PAPER  18  // "Paper:  N" feedback line
#define CP_ROW_RESULT 19  // "Result:" normal/inverse preview line

#define CP_SWATCH_X    9  // x of the 1-cell swatch on the Ink:/Paper: rows

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static OricCharWin cp_win;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Compute the grid column/row for ink/paper, per the 4-wide/16-row split
 * (ink 0-3 of a given paper value on one grid row, ink 4-7 of that same
 * paper value on the next).
 *
 * @param ink     Ink value, 0-7.
 * @param paper   Paper value, 0-7.
 * @param outcol  Out: grid column, 0-3.
 * @param outrow  Out: grid row, 0-15.
 * @return (none)
 */
static void cp_grid_pos(uint8_t ink, uint8_t paper, uint8_t *outcol, uint8_t *outrow)
{
    *outcol = (uint8_t)(ink % CP_INKS_PER_ROW);
    *outrow = (uint8_t)(paper * 2 + ink / CP_INKS_PER_ROW);
}

/**
 * Draw (or erase) the cursor highlight on grid cell (ink, paper) by
 * rewriting its 5-byte cell: a leading A_BGWHITE reset (matching V1's
 * colorpicker() exactly -- see this file's header comment for why this
 * byte is required, not cosmetic), the ink/paper attribute bytes, then
 * the two swatch characters with CH_SPACE/CH_INVSPACE swapped to show
 * the highlight (matching charsetedit's ^0x80 cursor-rendering
 * convention applied to a 2-char cell).
 *
 * @param ink       Ink value, 0-7.
 * @param paper     Paper value, 0-7.
 * @param highlight 1 to draw the swapped/highlighted pair, 0 for normal
 *                  (normal=paper-colour swatch first, inverse=ink-colour
 *                  swatch second).
 * @return (none)
 */
static void cp_draw_cell(uint8_t ink, uint8_t paper, uint8_t highlight)
{
    uint8_t col, row, x, y;
    uint8_t normal  = highlight ? CH_INVSPACE : CH_SPACE;
    uint8_t inverse = highlight ? CH_SPACE : CH_INVSPACE;

    cp_grid_pos(ink, paper, &col, &row);
    x = (uint8_t)(CP_GRID_X0 + col * CP_CELL_STEP);
    y = (uint8_t)(CP_ROW_GRID0 + row);

    cwin_putat_char(&cp_win, x, y, A_BGWHITE);
    cwin_putat_char(&cp_win, (uint8_t)(x + 1), y, ink);
    cwin_putat_char(&cp_win, (uint8_t)(x + 2), y, (uint8_t)(16 + paper));
    cwin_putat_char(&cp_win, (uint8_t)(x + 3), y, normal);
    cwin_putat_char(&cp_win, (uint8_t)(x + 4), y, inverse);
}

/**
 * Draw the title and the full 8x8 ink x paper grid (split 4-wide/16-row,
 * see cp_grid_pos()) in normal (unhighlighted) video, including the
 * end-of-row ink/paper reset pair on every grid row so the spare columns
 * to the right of the grid don't inherit the last cell's colours.
 *
 * @return (none)
 */
static void cp_draw_grid(void)
{
    uint8_t ink, paper, row;

    cwin_putat_string(&cp_win, CP_TITLE_X, CP_ROW_TITLE, MSG_COLOURPICKER_TITLE);

    for (paper = 0; paper < 8; paper++)
        for (ink = 0; ink < 8; ink++)
            cp_draw_cell(ink, paper, 0);

    for (row = 0; row < 16; row++)
    {
        uint8_t y = (uint8_t)(CP_ROW_GRID0 + row);
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
 * Colour picker popup, entered via 'c' from main mode. Opens a popup
 * showing the title, an 8x8 ink x paper grid (split 4-wide/16-row, each
 * cell showing the normal/paper-colour and inverse/ink-colour swatches
 * for that combination) and live Ink:/Paper:/Result: feedback lines, and
 * loops handling: cursor keys move/wrap the highlighted cell (LEFT/RIGHT
 * cycle ink, UP/DOWN cycle paper, both wrap 0-7); SPACE/ENTER commit the
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
