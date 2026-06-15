// charsetedit.c - Character editor for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Entered via 'e' from main mode (src/editor.c). Edits the 6x8-pixel glyph
// for app.plotscreencode/app.plotaltchar directly in live charset RAM
// (Strategy A, confirmed by the Phase 3a spike -- both CHARSET_STD and
// CHARSET_ALT banks can be edited in place with no shadow buffer; see
// CLAUDE.md "Character editor (Phase 3)"). ESC commits the current
// screencode/charset back to app.plotscreencode/app.plotaltchar.
//
// Pixel-toggle/scroll/mirror algorithms ported from V1 OricScreenEditor
// chareditor() (local reference at /home/xahmol/git/OricScreenEditor/src/main.c).

#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "charsetedit.h"

// -------------------------------------------------------------------------
// Popup window layout (38 cols x 16 rows, screen rows 1-16)
// -------------------------------------------------------------------------

#define CE_WIN_SX  2
#define CE_WIN_SY  1
#define CE_WIN_WX  38
#define CE_WIN_WY  16

#define CE_GRID_X  1    // window-relative x of grid column 0
#define CE_GRID_Y  4    // window-relative y of grid row 0

#define CE_FAV_Y        1   // favourite digit labels ('0'-'9')
#define CE_FAV_VALUE_Y  2   // favourite screencodes
#define CE_FAV_X       13   // first favourite column

#define CE_PREVIEW_ATTR_X  8   // charset-mode attribute byte (live preview)
#define CE_PREVIEW_CHAR_X  9   // live-preview character cell

#define CE_HEX_LABEL_X 12
#define CE_HEX_INPUT_X 22
#define CE_HEX_Y        5

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

uint16_t charset_address(uint8_t screencode, uint8_t altorstd)
{
    uint16_t base = (altorstd == 0) ? CHARSET_STD : CHARSET_ALT;
    return base + (uint16_t)screencode * 8;
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

// Pointer to the 8-byte glyph currently being edited, in live charset RAM.
static volatile uint8_t *ce_glyph(void)
{
    return (volatile uint8_t *)charset_address(ce_code, ce_altorstd);
}

// Highest valid screencode for the current charset (std: 0x7F, alt: 0x6F).
static uint8_t ce_max_code(void)
{
    return (ce_altorstd == 0) ? (0x20 + CHAREDIT_STD_COUNT - 1)
                               : (0x20 + CHAREDIT_ALT_COUNT - 1);
}

// Snapshot the current glyph into ce_undo[] (call before any destructive edit).
static void ce_snapshot(void)
{
    volatile uint8_t *g = ce_glyph();
    uint8_t y;
    for (y = 0; y < CHAREDIT_GRID_H; y++) ce_undo[y] = g[y];
}

static uint8_t ce_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

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

static void ce_draw_header(void)
{
    cwin_putat_printf(&ce_win, 1, 0, "Character editor   Code:$%02x  Set:%s",
                      ce_code, ce_altorstd ? "Alt" : "Std");

    // Live preview: charset-mode attribute + the screencode itself. Reads
    // back the SAME charset-RAM bytes the grid edits, so it updates as soon
    // as the grid is redrawn -- no separate buffer needed (Strategy A).
    cwin_putat_char(&ce_win, CE_PREVIEW_ATTR_X, CE_GRID_Y, ce_altorstd ? A_ALT : A_STD);
    cwin_putat_char(&ce_win, CE_PREVIEW_CHAR_X, CE_GRID_Y, ce_code);
}

static void ce_draw_favourites(void)
{
    uint8_t i;

    cwin_putat_string(&ce_win, 1, CE_FAV_Y, "Favourites:");
    for (i = 0; i < FAVOURITES_COUNT; i++)
    {
        cwin_putat_char(&ce_win, CE_FAV_X + i, CE_FAV_Y, (uint8_t)('0' + i));
        cwin_putat_char(&ce_win, CE_FAV_X + i, CE_FAV_VALUE_Y, app.favourites[i]);
    }
}

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

static void ce_draw_hints(void)
{
    cwin_putat_string(&ce_win, 1, 12, "u/d/l/r scroll  h hex row  DEL clear");
    cwin_putat_string(&ce_win, 1, 13, "+/-/= code  0-9 fav  SHIFT+0-9 store");
    cwin_putat_string(&ce_win, 1, 14, "SPACE plot  i invert  z undo  s ROM");
    cwin_putat_string(&ce_win, 1, 15, "a set  c/v copy  x/y mirror  ESC exit");
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------

void charsetedit_run(void)
{
    ce_code     = app.plotscreencode;
    ce_altorstd = app.plotaltchar;
    ce_cx       = 0;
    ce_cy       = 0;
    ce_copy_valid = 0;

    if (ce_code > ce_max_code()) ce_code = ce_max_code();

    menu_winsave(CE_WIN_SY, CE_WIN_WY);
    cwin_init(&ce_win, CE_WIN_SX, CE_WIN_SY, CE_WIN_WX, CE_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&ce_win);

    ce_snapshot();
    ce_draw_header();
    ce_draw_favourites();
    ce_draw_hints();
    ce_draw_grid();

    for (;;)
    {
        uint8_t c = cwin_getch();
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
            for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] ^= 0x3F;
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
                volatile uint8_t *rom = (volatile uint8_t *)
                    (CHARSETROM + (uint16_t)(ce_code - 0x20) * 8);
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
        {
            uint8_t buf[CHAREDIT_GRID_H];
            g = ce_glyph();
            ce_snapshot();
            for (y = 0; y < CHAREDIT_GRID_H; y++) buf[y] = g[CHAREDIT_GRID_H - 1 - y];
            for (y = 0; y < CHAREDIT_GRID_H; y++) g[y] = buf[y];
            redraw = 1;
            break;
        }

        case 'x':
            g = ce_glyph();
            ce_snapshot();
            for (y = 0; y < CHAREDIT_GRID_H; y++)
            {
                uint8_t present = g[y];
                uint8_t v = 0;
                uint8_t bx;
                for (bx = 0; bx < CHAREDIT_GRID_W; bx++)
                    if (present & (1 << (5 - bx))) v |= (uint8_t)(1 << bx);
                g[y] = v;
            }
            redraw = 1;
            break;

        case 'u':
            g = ce_glyph();
            ce_snapshot();
            {
                uint8_t first = g[0];
                for (y = 0; y < CHAREDIT_GRID_H - 1; y++) g[y] = g[y + 1];
                g[CHAREDIT_GRID_H - 1] = first;
            }
            redraw = 1;
            break;

        case 'd':
            g = ce_glyph();
            ce_snapshot();
            {
                uint8_t last = g[CHAREDIT_GRID_H - 1];
                for (y = CHAREDIT_GRID_H - 1; y > 0; y--) g[y] = g[y - 1];
                g[0] = last;
            }
            redraw = 1;
            break;

        case 'r':
            g = ce_glyph();
            ce_snapshot();
            for (y = 0; y < CHAREDIT_GRID_H; y++)
            {
                uint8_t v = g[y] >> 1;
                if (g[y] & 0x01) v |= 0x20;
                g[y] = v;
            }
            redraw = 1;
            break;

        case 'l':
            g = ce_glyph();
            ce_snapshot();
            for (y = 0; y < CHAREDIT_GRID_H; y++)
            {
                uint8_t v = (uint8_t)(g[y] << 1);
                if (g[y] & 0x20) v |= 0x01;
                g[y] = v & 0x3F;
            }
            redraw = 1;
            break;

        case 'h':
        {
            char hexbuf[3];
            hexbuf[0] = '\0';
            cwin_putat_string(&ce_win, CE_HEX_LABEL_X, CE_HEX_Y, "Hex row: $");
            if (cwin_textinput(&ce_win, CE_HEX_INPUT_X, CE_HEX_Y, 2, hexbuf, 2, VINPUT_ALPHA) >= 0)
            {
                g = ce_glyph();
                ce_snapshot();
                g[ce_cy] = (uint8_t)(ce_parse_hex(hexbuf) & 0x3F);
            }
            cwin_fill_rect(&ce_win, CE_HEX_LABEL_X, CE_HEX_Y, 12, 1, CH_SPACE);
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
