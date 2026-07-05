// colourpicker.c - Colour picker popup for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'c' from main mode (src/editor.c). New OSE-LOCI feature over
// V1 (see README.md "Planned feature additions over V1": "Enhanced palette
// mode also showing inverse ink/paper colour combinations"). Popup showing
// a single 8x8 ink x paper grid (one screen row per paper value, all 8 ink
// columns on it) -- each cell shows both the normal (paper-colour) and
// inverse (ink-colour) swatch for that combination -- plus live Ink:/Paper:/
// Result: feedback lines, for selecting app.plotink/app.plotpaper.
//
// **Rewritten 2026-06-21 to match V1's colourpicker()/colorpicker_cursorplot()
// (archived nonworkingcc65:src/colorpicker.c) exactly, after two earlier
// rounds of incremental fixes (see CLAUDE.md "Colour picker rendering fix")
// failed to hold up on real hardware.** V1's own grid is 8 ink columns wide
// in a single row per paper value (5 bytes/cell: a leading A_BGWHITE reset,
// the ink attribute, the paper attribute, then two swatch characters) --
// 8*5=40 columns exactly, with no leftover columns needing a separate
// end-of-row reset the way this port's earlier 4-wide/16-row split did.
// Confirmed against the literal V1 source rather than re-deriving an
// assumed-simpler layout (see memory feedback_dont_oversimplify_ported_
// designs): V1's cell order really is reset-then-ink-then-paper, the same
// order round 1 of the earlier fix used and round 2 "corrected" away from
// based on Phosphoric pixel sampling that, per the user, did not hold up on
// real hardware -- so this rewrite trusts V1's own long-tested byte order
// over that sampling.
//
// **The "first two columns" fix**: V1's grid is drawn via direct screen-RAM
// writes (ORIC_HChar/cputcxy in the original, ported here as cp_screen_put()
// using the same raw-TEXTVRAM-pointer technique canvas.c's canvas_blit()
// already uses) for every grid row, not through this popup's own OricCharWin
// (which reserves columns 0-1 of every window row for its own border ink/
// paper attribute bytes, see charwin.c's row_setattr()). Each grid row spans
// the *entire* physical screen width (columns 0-39), genuinely overwriting
// those two border columns within the grid's own row range -- exactly what
// V1's window system did too (its border only protects rows *outside* the
// grid). Routing the grid through cwin_putat_*() instead (as the earlier
// 4-wide/16-row version did, with grid content starting only at window-
// relative x=0, i.e. absolute column 2) is what made the ink=0 cell's
// leading reset byte land on the wrong column -- this rewrite bypasses cwin
// entirely for the 8 grid rows so the first cell's reset byte lands on
// literal column 0, matching V1.
//
// The title and Ink:/Paper:/Result: feedback rows are unaffected by this --
// they still use the popup's normal cwin_putat_*() bordered window, since
// only the grid itself needs the direct-write treatment.
//
// Adapted from V1's colourpicker()/colorpicker_cursorplot()
// (https://github.com/xahmol/OricScreenEditor, archived nonworkingcc65:
// src/colorpicker.c): same ink x paper grid and Ink:/Paper:/Result:
// feedback concept, RETURN/ESC to select/cancel. The per-cell border-drawing
// cursor (colorpicker_cursorplot, 4 ORIC_HChar/cputcxy calls per move) is
// replaced with a simpler XOR-swap of the two swatch characters within the
// cell (matches charsetedit's ^0x80 cursor convention).
//
// **Swatch glyph is '*'/inverse-'*' (CP_STAR/CP_INVSTAR), not a blank/solid
// space (user-requested 2026-06-21)**: a blank CH_SPACE swatch only shows
// the paper colour (no foreground pixels to show ink at all); a solid
// CH_INVSPACE swatch only shows ink (the whole cell inverts to one solid
// colour). Neither lets the user see the actual ink-on-paper combination at
// a glance. A '*' glyph has both visible foreground pixels (ink colour) and
// background (paper colour) in the same cell, so both colours of the pair
// are visible together -- used for both the grid's per-cell swatches and
// the Result line's normal/inverse preview pair.
//
// Opts IN to the charset-swap mechanism (menu_winsave(..., 1)) -- the swatch
// glyphs are plain Std-charset chars, no live-edit-preview requirement.

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
// 8 ink columns x 8 paper rows, one screen row per paper value, each cell
// 5 columns wide (reset byte, ink attr, paper attr, 2 swatches) -- 8*5=40,
// exactly the screen width, so the grid rows need no separate end-of-row
// reset pair.

#define CP_WIN_SX   2
#define CP_WIN_SY   0
#define CP_WIN_WX  36
#define CP_WIN_WY  12

#define CP_TITLE_X    0   // window-relative x of MSG_COLOURPICKER_TITLE --
                          // 0, same as the Ink:/Paper:/Result: feedback
                          // lines below, so all four left-align together

#define CP_ROW_GRID0  1   // absolute screen row of paper=0's grid row; paper 1-7 follow
#define CP_CELL_STEP  5   // columns per grid cell (reset, ink, paper, 2 swatches)

#define CP_ROW_INK     9  // "Ink:    N" feedback line
#define CP_ROW_PAPER  10  // "Paper:  N" feedback line
#define CP_ROW_RESULT 11  // "Result:" normal/inverse preview line

#define CP_SWATCH_X    9  // x of the 1-cell swatch on the Ink:/Paper: rows

// Swatch glyph: a '*' (not CH_SPACE/CH_INVSPACE) so a swatch's foreground
// (ink) and background (paper) are both visible at once -- a blank space
// only shows paper, a solid inverse block only shows ink. Used by both the
// grid cells (when not the highlighted cell, see CP_DASH below) and the
// Result line's normal/inverse preview pair.
#define CP_STAR        ((uint8_t)'*')
#define CP_INVSTAR      (uint8_t)(CP_STAR | 0x80)

// The highlighted/selected grid cell uses '-' instead of '*' so it stands
// out from every other cell at a glance, rather than relying only on the
// swatch pair's left-right order (which is still swapped too, see
// cp_draw_cell()).
#define CP_DASH        ((uint8_t)'-')
#define CP_INVDASH      (uint8_t)(CP_DASH | 0x80)

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static OricCharWin cp_win;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Write a byte directly to screen RAM at absolute (col, row), bypassing
 * any OricCharWin -- used for the grid rows only, which must be able to
 * write columns 0-1 (a popup window's own reserved border columns), see
 * this file's header comment for why.
 *
 * @param col   Absolute screen column, 0-39.
 * @param row   Absolute screen row, 0-27.
 * @param value Byte to write.
 * @return (none)
 */
static void cp_screen_put(uint8_t col, uint8_t row, uint8_t value)
{
    uint8_t *cell = (uint8_t *)TEXTVRAM + (uint16_t)row * SCREEN_COLS + col;
    *cell = value;
}

/**
 * Draw (or erase) the cursor highlight on grid cell (ink, paper) by
 * rewriting its 5-byte cell directly to screen RAM: a leading A_BGWHITE
 * reset byte, the ink attribute byte, the paper attribute byte, then the
 * two swatch characters. The highlighted cell uses CP_DASH/CP_INVDASH
 * ('-') instead of every other cell's CP_STAR/CP_INVSTAR ('*'), so the
 * selected cell stands out by glyph, not just by the swap below. Byte
 * order and the leading reset byte match V1's colorpicker_cursorplot()/
 * colourpicker() exactly (see header comment).
 *
 * @param ink       Ink value, 0-7.
 * @param paper     Paper value, 0-7.
 * @param highlight 1 to draw the highlighted ('-') pair, 0 for normal
 *                  ('*') -- either way, swatch order is paper-colour
 *                  swatch first, ink-colour swatch second when not
 *                  highlighted, swapped when highlighted.
 * @return (none)
 */
static void cp_draw_cell(uint8_t ink, uint8_t paper, uint8_t highlight)
{
    uint8_t row = (uint8_t)(CP_ROW_GRID0 + paper);
    uint8_t x0  = (uint8_t)(ink * CP_CELL_STEP);
    uint8_t normal  = highlight ? CP_INVDASH : CP_STAR;
    uint8_t inverse = highlight ? CP_DASH : CP_INVSTAR;

    cp_screen_put(x0,                row, A_BGWHITE);
    cp_screen_put((uint8_t)(x0 + 1), row, ink);
    cp_screen_put((uint8_t)(x0 + 2), row, (uint8_t)(16 + paper));
    cp_screen_put((uint8_t)(x0 + 3), row, normal);
    cp_screen_put((uint8_t)(x0 + 4), row, inverse);
}

/**
 * Override the popup's left-edge border columns (0-1) for every row of the
 * window, in V1's own order -- paper (A_BGWHITE) at column 0, ink
 * (A_FWBLACK) at column 1. `cwin_clear()` already wrote these two columns
 * for every row, but in this codebase's usual ink-then-paper order
 * (`row_setattr()`); V1's window border (and every grid cell's own leading
 * reset/ink pair, see cp_draw_cell()) uses the opposite, paper-then-ink
 * order. Left as cwin's default, the title/feedback rows would show
 * black-then-white at columns 0-1 while the grid rows show white-then-
 * white (the grid's own ink=0 cell always lands on column 0-1) -- a
 * visible jump in the left-edge colour exactly where the grid starts and
 * ends (user-reported 2026-06-21: "On the left, the white is now not
 * aligned"). Overriding every row to V1's order makes the whole popup's
 * left edge consistently white, matching the grid's own ink=0 cell with
 * no special-casing needed there.
 *
 * @return (none)
 */
static void cp_draw_border(void)
{
    uint8_t row;

    for (row = CP_WIN_SY; row < (uint8_t)(CP_WIN_SY + CP_WIN_WY); row++)
    {
        cp_screen_put(0, row, A_BGWHITE);
        cp_screen_put(1, row, A_FWBLACK);
    }
}

/**
 * Draw the title (via the popup's own bordered window) and the full 8x8
 * ink x paper grid (via direct screen-RAM writes, see cp_screen_put()) in
 * normal (unhighlighted) video.
 *
 * @return (none)
 */
static void cp_draw_grid(void)
{
    uint8_t ink, paper;

    cwin_putat_string(&cp_win, CP_TITLE_X, 0, MSG_COLOURPICKER_TITLE);

    for (paper = 0; paper < 8; paper++)
        for (ink = 0; ink < 8; ink++)
            cp_draw_cell(ink, paper, 0);
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
    cwin_putat_char(&cp_win, 9, CP_ROW_RESULT, CP_STAR);
    cwin_putat_char(&cp_win, 10, CP_ROW_RESULT, CP_INVSTAR);
    cwin_putat_char(&cp_win, 11, CP_ROW_RESULT, A_FWBLACK);
    cwin_putat_char(&cp_win, 12, CP_ROW_RESULT, A_BGWHITE);
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

/**
 * Colour picker popup, entered via 'c' from main mode. Opens a popup
 * showing the title, an 8x8 ink x paper grid (one screen row per paper
 * value, each cell showing the normal/paper-colour and inverse/ink-colour
 * swatches for that combination) and live Ink:/Paper:/Result: feedback
 * lines, and loops handling: cursor keys move/wrap the highlighted cell
 * (LEFT/RIGHT cycle ink, UP/DOWN cycle paper, both wrap 0-7); SPACE/ENTER
 * commit the highlighted cell's ink/paper into app.plotink/app.plotpaper
 * and close the popup; FUNCT+6 toggles the statusbar; ESC closes the popup
 * leaving app.plotink/app.plotpaper unchanged.
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
    cp_draw_border();

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
