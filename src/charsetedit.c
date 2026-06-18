// charsetedit.c - Character editor for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'e' from main mode (src/editor.c). Edits the 6x8-pixel glyph
// for app.plotscreencode/app.plotaltchar directly in live charset RAM
// (Strategy A, confirmed by the Phase 3a spike -- both CHARSET_STD and
// CHARSET_ALT banks can be edited in place with no shadow buffer; see
// CLAUDE.md "Character editor (Phase 3)"). ESC commits the current
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
#include "strings.h"
#include "input.h"
#include "charsetedit.h"

// -------------------------------------------------------------------------
// Popup window layout (13 cols x 15 rows, screen cols 27-39, rows 0-14)
// -------------------------------------------------------------------------

#define CE_WIN_SX  27
#define CE_WIN_SY   0
#define CE_WIN_WX  13
#define CE_WIN_WY  15

#define CE_HEADER_Y        0   // "Code:$xx" + live preview
#define CE_PREVIEW_ATTR_X  9   // charset-mode attribute byte (live preview)
#define CE_PREVIEW_CHAR_X 10   // live-preview character cell

#define CE_SET_Y           1   // "Set:Std" / "Set:Alt"

#define CE_FAV_Y           3   // favourite digit labels ('0'-'9')
#define CE_FAV_VALUE_Y     4   // favourite screencodes
#define CE_FAV_X           1   // first favourite column

#define CE_GRID_X          3   // window-relative x of grid column 0
#define CE_GRID_Y          6   // window-relative y of grid row 0

#define CE_HEX_LABEL_X     0
#define CE_HEX_INPUT_X     5
#define CE_HEX_Y          14

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
    cwin_putat_printf(&ce_win, 0, CE_HEADER_Y, MSG_CE_CODE_FMT, ce_code);
    cwin_putat_string(&ce_win, 0, CE_SET_Y, ce_altorstd ? MSG_CE_SET_ALT : MSG_CE_SET_STD);

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
 * variants) -- see CLAUDE.md "Character editor (Phase 3)".
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
    cwin_clear(&ce_win);

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
                for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = rom[y];
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
            char hexbuf[3];
            hexbuf[0] = '\0';
            cwin_putat_string(&ce_win, CE_HEX_LABEL_X, CE_HEX_Y, MSG_CE_HEX_LABEL);
            if (cwin_textinput(&ce_win, CE_HEX_INPUT_X, CE_HEX_Y, 2, hexbuf, 2, VINPUT_ALPHA) >= 0)
            {
                g = ce_glyph();
                ce_snapshot();
                g[ce_cy] = (uint8_t)(ce_parse_hex(hexbuf) & 0x3F);
            }
            cwin_fill_rect(&ce_win, CE_HEX_LABEL_X, CE_HEX_Y, 7, 1, CH_SPACE);
            redraw = 1;
            break;
        }

        case KEY_F6:
            statusbar_show((uint8_t)!app.showstatusbar);
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
