// charsetedit.c - Character editor for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'e' from main mode (src/editor.c). Edits the 6x8-pixel glyph
// for app.plotscreencode/app.plotaltchar directly in live charset RAM
// (Strategy A, confirmed by an early spike -- both CHARSET_STD and
// CHARSET_ALT banks can be edited in place with no shadow buffer; see
// CLAUDE.md "Character editor"). ESC commits the current
// screencode/charset back to app.plotscreencode/app.plotaltchar.
//
// Layout: narrow sidebar popup (cols 27-39, rows 0-14) so the user's canvas
// (cols 0-26) stays visible to the left while editing -- matches V1
// OricScreenEditor's sidebar intent (see CLAUDE.md "Charset-swap mechanism
// and narrow character editor"). Opts OUT of menu_winsave()'s charset-swap
// (swap_charset=0) so glyph edits stay visible live in the canvas while this
// popup is open.
//
// Pixel-toggle/scroll/mirror algorithms ported from V1 OricScreenEditor
// chareditor() (local reference at /home/xahmol/git/OricScreenEditor/src/main.c).

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "charset.h"
#include "charsetswap.h"
#include "loci.h"
#include "strings.h"
#include "input.h"
#include "help.h"
#include "charsetedit.h"

// -------------------------------------------------------------------------
// Popup window layout (13 cols x 15 rows, screen cols 27-39, rows 0-14)
// -------------------------------------------------------------------------

#define CE_WIN_SX  26
#define CE_WIN_SY   0
#define CE_WIN_WX  14
#define CE_WIN_WY  15

// Window-relative columns 0-2 are reserved for this popup's OWN ink/
// paper/charset-mode attribute triplet (ce_clear(), below) -- all real
// content starts at CE_CONTENT_X0 instead of 0. Needed because
// cwin_clear()'s row_setattr() always writes the ink/paper pair at
// ABSOLUTE screen columns 0-1, regardless of the window's own sx --
// correct for every sx=2/sx=5 popup in this codebase (where columns 0-1
// ARE the window's own reserved border), but wrong for this one sidebar
// popup, whose sx is chosen specifically so canvas columns to its left
// stay visible underneath. Writing the attribute pair at literal column
// 0/1 doesn't erase anything there (cwin_clear()'s own content-fill loop
// correctly only touches sx..sx+wx-1), but the Oric ULA holds an
// ink/paper attribute active from the column it's written at onward
// until the next attribute change -- so with nothing else on the row to
// reset it, the white PAPER byte at column 1 was visibly bleeding the
// popup's white background all the way across the visible canvas too,
// not just its own columns (user report 2026-06-23: "popup makes the
// whole width white, not only the right part of the screen it actually
// uses"). Matches V1's own approach (V1's chareditor() reserves 3 of its
// own panel's columns as an attribute-transition strip immediately
// before its real content -- local reference at
// showchareditfield()/showchareditgrid(),
// /home/xahmol/git/OricScreenEditor/src/main.c).
//
// The 3rd reserved column (charset-mode reset to A_STD) was ADDED
// 2026-06-23 (user report: "Charset editor does not set attribute code
// for std charset before the popup, causing it to show illegible if
// printed over lines that use alternate charset"). Unlike ink/paper,
// which the ULA resets to white-ink/black-paper at the START of every
// raster line automatically (see CLAUDE.md "Oric Screen Model"), the
// charset-mode latch (A_STD/A_ALT/etc, values 8-15) does NOT reset per
// line -- it persists across rows too, from wherever it was last set,
// until the next charset-mode attribute byte anywhere to its right (on
// any later row) changes it again. So if the canvas underneath this
// popup (or even this popup's own live-preview swatch, see
// CE_PREVIEW_ATTR_X below, which deliberately sets A_ALT for one cell)
// ever set A_ALT and nothing further right/down ever reset it, every
// later row's content from that column onward silently inherited Alt
// charset rendering too -- illegible, since this popup's own text is
// authored assuming Standard charset glyphs. Originally omitted on the
// (wrong) assumption that "nothing this popup draws ever uses A_ALT"
// was sufficient -- it isn't: the popup must explicitly assert A_STD
// for its own content regardless of what charset mode was active
// outside it or earlier within it. Widening CE_WIN_WX by 1 (and moving
// CE_WIN_SX 1 column left to compensate) keeps every other CE_*_X
// constant's ABSOLUTE screen column unchanged (they're all defined
// relative to CE_CONTENT_X0, which also shifts by 1) -- the only
// observable effect is the popup now also covers 1 more canvas column
// underneath (was 0-26 visible, now 0-25).
#define CE_CONTENT_X0      3

#define CE_HEADER_Y        0   // "Code:$xx" + live preview
#define CE_PREVIEW_ATTR_X  (CE_CONTENT_X0 + 9)   // charset-mode attribute byte (live preview)
#define CE_PREVIEW_CHAR_X  (CE_CONTENT_X0 + 10)  // live-preview character cell

#define CE_SET_Y           1   // "Set:Std" / "Set:Alt"

#define CE_FAV_Y           3   // favourite digit labels ('0'-'9')
#define CE_FAV_VALUE_Y     4   // favourite screencodes
#define CE_FAV_X           (CE_CONTENT_X0 + 1)   // first favourite column

// Per-row 2-digit hex byte value, always visible immediately left of the
// grid (CE_HEX_VAL_X..+1, same row as that grid row) -- matches V1's
// showchareditgrid() exactly (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c: `gotoxy(30,y+3);
// cprintf("%2X",char_byte);` immediately before that row's own pixel
// grid at column 33). User-requested 2026-06-23 ("chareditor should
// always show the hex values of each byte line left of the edit matrix
// like in v1") -- the earlier OSE port only had a one-off hex-input
// prompt on a separate fixed row (CE_HEX_LABEL_X/CE_HEX_INPUT_X/CE_HEX_Y,
// removed) with no permanent per-row readout at all.
#define CE_HEX_VAL_X       CE_CONTENT_X0   // 2 digits, columns CE_CONTENT_X0/+1
#define CE_GRID_X          (CE_CONTENT_X0 + 3)   // window-relative x of grid column 0
#define CE_GRID_Y          6   // window-relative y of grid row 0

#define CE_PIXEL_CHAR  '#'   // grid: pixel-set marker (XOR 0x80 = cursor highlight)

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

static OricCharWin ce_win;
static uint8_t     ce_code;       // screencode being edited
static uint8_t     ce_altorstd;   // 0 = standard charset, 1 = alternate
static uint8_t     ce_cx, ce_cy;  // cursor within the 6x8 grid
static uint8_t     ce_undo[CHAREDIT_GRID_H];
static uint8_t     ce_copy[CHAREDIT_GRID_H];
static uint8_t     ce_copy_valid;

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

/**
 * Clear the popup, like cwin_clear(&ce_win) -- except the ink/paper/
 * charset-mode attribute triplet is written at the window's OWN first
 * three relative columns (CE_WIN_SX+0/+1/+2) instead of at literal
 * screen columns 0/1, and the content fill starts at CE_CONTENT_X0
 * rather than column 0. See CE_CONTENT_X0's doc comment for why
 * cwin_clear() itself is wrong for this one sidebar popup, and why the
 * charset-mode (A_STD) byte is needed on every row, not just once.
 *
 * @return (none)
 */
static void ce_clear(void)
{
    for (uint8_t y = 0; y < CE_WIN_WY; y++)
    {
        cwin_putat_char(&ce_win, 0, y, ce_win.ink);
        cwin_putat_char(&ce_win, 1, y, ce_win.paper);
        cwin_putat_char(&ce_win, 2, y, A_STD);
    }
    cwin_fill_rect(&ce_win, CE_CONTENT_X0, 0, (uint8_t)(CE_WIN_WX - CE_CONTENT_X0), CE_WIN_WY, CH_SPACE);
    ce_win.cx = 0;
    ce_win.cy = 0;
}

/**
 * Pointer to the 8-byte glyph currently being edited, in live charset RAM
 * (CHARSET_STD or CHARSET_ALT, selected by ce_altorstd).
 *
 * @return Pointer to the 8-byte glyph bitmap for ce_code/ce_altorstd.
 */
static volatile uint8_t *ce_glyph(void)
{
    return (volatile uint8_t *)charset_address(ce_code, ce_altorstd);
}

/**
 * Highest valid screencode for the charset currently selected by
 * ce_altorstd (std: 0x20+CHAREDIT_STD_COUNT-1 = 0x7F, alt:
 * 0x20+CHAREDIT_ALT_COUNT-1 = 0x6F).
 *
 * @return Highest valid screencode for the current charset.
 */
static uint8_t ce_max_code(void)
{
    return (ce_altorstd == 0) ? (0x20 + CHAREDIT_STD_COUNT - 1)
                               : (0x20 + CHAREDIT_ALT_COUNT - 1);
}

/**
 * Snapshot the current glyph into ce_undo[] (call before any destructive
 * edit, so 'z' can restore it), mark the charset as user-edited via
 * charsetswap_mark_changed() (so future popups back up/restore CHARSET_STD
 * around themselves, see charsetswap.h), and set app.stdchanged/altchanged
 * (so File > Save Project, src/fileio.c, knows which charset file(s) to
 * write).
 *
 * @return (none)
 */
static void ce_snapshot(void)
{
    volatile uint8_t *g = ce_glyph();
    uint8_t y;
    charsetswap_mark_changed();
    if (ce_altorstd) app.altchanged = 1; else app.stdchanged = 1;
    for (y = 0; y < CHAREDIT_GRID_H; y++) ce_undo[y] = g[y];
}

/**
 * Parse a single hexadecimal digit character.
 *
 * @param c Character '0'-'9', 'a'-'f', or 'A'-'F'.
 * @return Numeric value 0-15, or 0 if c is not a hex digit.
 */
static uint8_t ce_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

/**
 * Parse a NUL-terminated string of hexadecimal digits into a byte,
 * accumulating digit-by-digit (most significant digit first).
 *
 * @param s NUL-terminated hex digit string (e.g. up to 2 digits for a
 *          charset glyph row).
 * @return Parsed numeric value.
 */
static uint8_t ce_parse_hex(const char *s)
{
    uint8_t v = 0;
    while (*s)
    {
        v = (uint8_t)((v << 4) | ce_hex_digit(*s));
        s++;
    }
    return v;
}

// -------------------------------------------------------------------------
// Drawing
// -------------------------------------------------------------------------

/**
 * Draw the popup's header: "Code:$xx" (CE_HEADER_Y), "Set:Std"/"Set:Alt"
 * (CE_SET_Y), and the live attribute+character preview
 * (CE_PREVIEW_ATTR_X/CE_PREVIEW_CHAR_X) for the screencode/charset
 * currently being edited.
 *
 * @return (none)
 */
static void ce_draw_header(void)
{
    cwin_putat_printf(&ce_win, CE_CONTENT_X0, CE_HEADER_Y, MSG_CE_CODE_FMT, ce_code);
    cwin_putat_string(&ce_win, CE_CONTENT_X0, CE_SET_Y, ce_altorstd ? MSG_CE_SET_ALT : MSG_CE_SET_STD);

    // Live preview: charset-mode attribute + the screencode itself. Reads
    // back the SAME charset-RAM bytes the grid edits, so it updates as soon
    // as the grid is redrawn -- no separate buffer needed (Strategy A).
    cwin_putat_char(&ce_win, CE_PREVIEW_ATTR_X, CE_HEADER_Y, ce_altorstd ? A_ALT : A_STD);
    cwin_putat_char(&ce_win, CE_PREVIEW_CHAR_X, CE_HEADER_Y, ce_code);
}

/**
 * Draw the favourites row: digit labels '0'-'9' (CE_FAV_Y) above the
 * screencodes currently stored in app.favourites[] (CE_FAV_VALUE_Y).
 *
 * @return (none)
 */
static void ce_draw_favourites(void)
{
    uint8_t i;

    for (i = 0; i < FAVOURITES_COUNT; i++)
    {
        cwin_putat_char(&ce_win, CE_FAV_X + i, CE_FAV_Y, (uint8_t)('0' + i));
        cwin_putat_char(&ce_win, CE_FAV_X + i, CE_FAV_VALUE_Y, app.favourites[i]);
    }
}

/**
 * Draw the 6x8 pixel grid for the glyph being edited, using the 4-state
 * pixel/cursor rendering (CH_SPACE / CE_PIXEL_CHAR / their inverse-video
 * variants) -- see CLAUDE.md "Character editor" -- plus each row's raw
 * byte value as 2 hex digits immediately to its left (CE_HEX_VAL_X),
 * matching V1's showchareditgrid() (see CE_HEX_VAL_X's doc comment).
 *
 * @return (none)
 */
static void ce_draw_grid(void)
{
    volatile uint8_t *g = ce_glyph();
    uint8_t x, y;

    for (y = 0; y < CHAREDIT_GRID_H; y++)
    {
        uint8_t row = g[y];
        cwin_putat_printf(&ce_win, CE_HEX_VAL_X, CE_GRID_Y + y, "%02x", row);
        for (x = 0; x < CHAREDIT_GRID_W; x++)
        {
            uint8_t ch = (row & (1 << (5 - x))) ? CE_PIXEL_CHAR : CH_SPACE;
            if (x == ce_cx && y == ce_cy) ch ^= 0x80;
            cwin_putat_char(&ce_win, CE_GRID_X + x, CE_GRID_Y + y, ch);
        }
    }
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

/**
 * Character editor popup, entered via 'e' from main mode. Opens a narrow
 * 13x15 sidebar popup (cols 27-39, rows 0-14) leaving the canvas (cols
 * 0-26) visible, and loops handling the full V1-derived key-binding set:
 * cursor keys move within the grid; '+'/'-'/'=' cycle the screencode;
 * '0'-'9' recall favourites, SHIFT+0-9 store the current code into them;
 * SPACE toggles a pixel, DEL clears the glyph; 'i' inverts, 'z' undoes the
 * last edit, 's' restores from CHARSETROM (std only); 'c'/'v' copy/paste a
 * glyph; 'x'/'y' mirror horizontally/vertically; 'u'/'d'/'l'/'r' scroll/
 * rotate the glyph; 'h' opens a hex-row input; 'a' toggles Std/Alt; FUNCT+6
 * toggles the statusbar. ESC commits ce_code/ce_altorstd back to
 * app.plotscreencode/app.plotaltchar and closes the popup.
 *
 * @return (none)
 */
void charsetedit_run(void)
{
    ce_code     = app.plotscreencode;
    ce_altorstd = app.plotaltchar;
    ce_cx       = 0;
    ce_cy       = 0;
    ce_copy_valid = 0;

    if (ce_code > ce_max_code()) ce_code = ce_max_code();

    menu_winsave(CE_WIN_SY, CE_WIN_WY, 0);
    cwin_init(&ce_win, CE_WIN_SX, CE_WIN_SY, CE_WIN_WX, CE_WIN_WY, A_FWBLACK, A_BGWHITE);
    ce_clear();

    ce_snapshot();
    ce_draw_header();
    ce_draw_favourites();
    ce_draw_grid();

    for (;;)
    {
        uint8_t c = key_read();
        uint8_t changed = 0;
        uint8_t redraw  = 0;
        volatile uint8_t *g;
        uint8_t y;

        switch (c)
        {
        case KEY_LEFT:
            if (ce_cx > 0) { ce_cx--; redraw = 1; }
            break;

        case KEY_RIGHT:
            if (ce_cx < CHAREDIT_GRID_W - 1) { ce_cx++; redraw = 1; }
            break;

        case KEY_UP:
            if (ce_cy > 0) { ce_cy--; redraw = 1; }
            break;

        case KEY_DOWN:
            if (ce_cy < CHAREDIT_GRID_H - 1) { ce_cy++; redraw = 1; }
            break;

        case KEY_SPACE:
            g = ce_glyph();
            ce_snapshot();
            g[ce_cy] ^= (uint8_t)(1 << (5 - ce_cx));
            redraw = 1;
            break;

        case KEY_DEL:
            g = ce_glyph();
            ce_snapshot();
            for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = 0;
            redraw = 1;
            break;

        case '+':
        case '=':
            ce_code = (ce_code < ce_max_code()) ? ce_code + 1 : 0x20;
            changed = 1;
            break;

        case '-':
            ce_code = (ce_code > 0x20) ? ce_code - 1 : ce_max_code();
            changed = 1;
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            ce_code = app.favourites[c - '0'];
            changed = 1;
            break;

        case ')': app.favourites[0] = ce_code; ce_draw_favourites(); break;
        case '!': app.favourites[1] = ce_code; ce_draw_favourites(); break;
        case '@': app.favourites[2] = ce_code; ce_draw_favourites(); break;
        case '#': app.favourites[3] = ce_code; ce_draw_favourites(); break;
        case '$': app.favourites[4] = ce_code; ce_draw_favourites(); break;
        case '%': app.favourites[5] = ce_code; ce_draw_favourites(); break;
        case '^': app.favourites[6] = ce_code; ce_draw_favourites(); break;
        case '&': app.favourites[7] = ce_code; ce_draw_favourites(); break;
        case '*': app.favourites[8] = ce_code; ce_draw_favourites(); break;
        case '(': app.favourites[9] = ce_code; ce_draw_favourites(); break;

        case 'i':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_invert(g);
            redraw = 1;
            break;

        case 'z':
            g = ce_glyph();
            for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = ce_undo[y];
            redraw = 1;
            break;

        case 's':
            if (ce_altorstd == 0)
            {
                const uint8_t *rom = charset_rom_glyph(ce_code);
                g = ce_glyph();
                ce_snapshot();
                // CHARSETROM ($FC78) is in $C000-$FFFF, overlay-RAM
                // territory now (screenmap[]/undo) -- briefly borrow ROM
                // for this read, same as charsetswap_enter().
                disable_overlay_ram();
                for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = rom[y];
                enable_overlay_ram();
                redraw = 1;
            }
            break;

        case 'a':
            ce_altorstd = !ce_altorstd;
            changed = 1;
            break;

        case 'c':
            g = ce_glyph();
            for (y = 0; y < CHAREDIT_GRID_H; y++) ce_copy[y] = g[y];
            ce_copy_valid = 1;
            break;

        case 'v':
            if (ce_copy_valid)
            {
                g = ce_glyph();
                ce_snapshot();
                for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = ce_copy[y];
                redraw = 1;
            }
            break;

        case 'y':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_mirror_v(g);
            redraw = 1;
            break;

        case 'x':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_mirror_h(g);
            redraw = 1;
            break;

        case 'u':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_scroll_up(g);
            redraw = 1;
            break;

        case 'd':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_scroll_down(g);
            redraw = 1;
            break;

        case 'r':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_rotate_right(g);
            redraw = 1;
            break;

        case 'l':
            g = ce_glyph();
            ce_snapshot();
            charset_glyph_rotate_left(g);
            redraw = 1;
            break;

        case 'h':
        {
            // Inline-edits the cursor's current row (ce_cy) directly on
            // top of its own always-visible hex readout (CE_HEX_VAL_X,
            // ce_draw_grid()) -- matches V1's chareditor() 'h' case
            // exactly (local reference at
            // /home/xahmol/git/OricScreenEditor/src/main.c:
            // `sprintf(buffer,"%2x",char_present[ypos]);
            // textInput(30,ypos+3,buffer,2,3);`), including taking the
            // row's current value as the input's pre-filled default
            // (2026-06-23 user request -- the earlier OSE port started
            // from an empty buffer on a separate fixed-row prompt
            // instead). vwidth=4 (not 2) leaves room for the cursor slot
            // cwin_textinput() needs alongside both digits -- vwidth==
            // maxlen was the root cause of a separate reported bug
            // (typing a 2nd hex digit scrolled the 1st out of view,
            // "length of input seems too short with 1 only"); every
            // other cwin_textinput() call site in this codebase already
            // uses vwidth > maxlen, this was the one exception.
            char hexbuf[3];
            uint8_t val;
            g = ce_glyph();
            val = g[ce_cy];
            hexbuf[0] = "0123456789ABCDEF"[(val >> 4) & 0x0F];
            hexbuf[1] = "0123456789ABCDEF"[val & 0x0F];
            hexbuf[2] = '\0';
            // VINPUT_ALPHA alone does NOT include digits despite its
            // header comment ("Alpha + digits") -- charwin.c's validation
            // checks (validation & VINPUT_NUMS)/(validation & VINPUT_ALPHA)
            // as independent bits, so a hex field needs both ORed
            // together.
            if (cwin_textinput(&ce_win, CE_HEX_VAL_X, CE_GRID_Y + ce_cy, 4, hexbuf, 2, VINPUT_NUMS | VINPUT_ALPHA) >= 0)
            {
                ce_snapshot();
                g[ce_cy] = (uint8_t)(ce_parse_hex(hexbuf) & 0x3F);
            }
            redraw = 1;
            break;
        }

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
            break;

        case KEY_F8:
            help_show(2);
            ce_clear();
            ce_draw_header();
            ce_draw_favourites();
            ce_draw_grid();
            break;

        case KEY_ESC:
            app.plotscreencode = ce_code;
            app.plotaltchar    = ce_altorstd;
            menu_winrestore();
            return;

        default:
            break;
        }

        if (changed)
        {
            uint8_t maxc = ce_max_code();
            if (ce_code > maxc) ce_code = maxc;
            ce_snapshot();
            ce_draw_header();
            ce_draw_grid();
        }
        else if (redraw)
        {
            ce_draw_grid();
        }
    }
}
