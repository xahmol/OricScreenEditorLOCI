// main.c - standalone Oric Atmos keyboard matrix test (kbtest)
//
// Purpose: a minimal, general-purpose diagnostic for the keyboard scanner
// (include/keyboard.c's keyb_scan()/keyb_decode(), shared verbatim with
// OricScreenEditorLOCI and locifilemanager-v2 via -i=../include) -- no LOCI
// device requirement, no overlay RAM, no IJK joystick, no charwin/menu
// system, no canvas. A deliberately minimal binary (a few KB, vs OSE-LOCI's
// ~34KB), originally built to help isolate a real-hardware-only bug where
// '=' never registered (resolved 2026-06-22 -- a decode-table transcription
// error, see memory equals_plus_key_not_recognized for the full story) and
// kept as a permanent, reusable tool for diagnosing any future keyboard
// matrix issue on real hardware, independent of the full editor's size and
// complexity.
//
// What it shows, continuously, every scan (no debounce/repeat -- this
// reads keyb_scan()/keyb_decode() directly, not keyb_poll(), so the
// display reflects the scanner's truly raw output every single poll):
//   - The full 8x8 keyb_matrix[] as 8 rows of 8 bits (col0..col7) plus
//     the row's raw hex byte.
//   - The decoded character (hex + printable glyph) and modifier flags
//     from keyb_decode().
//
// Reuses keyboard.c/h and oric.h/oric_crt.c verbatim from
// OricScreenEditorLOCI's include/ (via -i=../include in this directory's
// Makefile) -- not copied, so there is no risk of drift between the
// scanner code being tested here and the scanner code in the real
// program.

#include <stdint.h>
#include "oric.h"
#include "keyboard.h"

// Screen layout (absolute columns/rows, 40x28):
//   row 0-1:  title / instructions
//   row 3:    column header
//   rows 4-11: one matrix row each (R0..R7), format "R0:  0 1 0 0 1 0 0 0  ($24)"
//   row 13:   decoded char: "Decoded ch: $XX 'c'"
//   row 14:   modifier flags
//   row 16-20: '=' / '+' call-out + instructions
#define ROW_MATRIX0   4
#define COL_BIT0      6   // column of col0's bit char; col N's bit char is at COL_BIT0 + N*2
#define COL_HEX       23  // column of the 2 hex digits inside "($XX)"
#define ROW_DECODED   13
#define COL_DECODED_HEX   14  // right after "Decoded ch: $" (13 chars, cols 1-13)
#define COL_DECODED_GLYPH 18  // right after " '" (cols 16-17, after the 2 hex digits)
#define ROW_MODS      14
#define COL_MOD_SHIFT 13  // right after "Mods: SHIFT=" (12 chars, cols 1-12)
#define COL_MOD_CTRL  20  // right after " CTRL=" (cols 14-19)
#define COL_MOD_FUNCT 28  // right after " FUNCT=" (cols 21-27)
#define COL_MOD_CAPS  35  // right after " CAPS=" (cols 29-34)

/**
 * Write a NUL-terminated ASCII string directly to screen RAM at (col, row),
 * bypassing any window/menu abstraction (none exist in this minimal
 * program) -- same raw-TEXTVRAM-pointer technique OSE-LOCI's
 * canvas.c:canvas_blit() uses.
 *
 * @param col Absolute screen column, 0-39.
 * @param row Absolute screen row, 0-27.
 * @param s   NUL-terminated string to write (not wrapped/clipped).
 * @return (none)
 */
static void put_str(uint8_t col, uint8_t row, const char *s)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM + (uint16_t)row * SCREEN_COLS + col;
    while (*s) *dst++ = (uint8_t)*s++;
}

/**
 * Write a single byte directly to screen RAM at (col, row).
 *
 * @param col Absolute screen column, 0-39.
 * @param row Absolute screen row, 0-27.
 * @param ch  Byte to write.
 * @return (none)
 */
static void put_ch(uint8_t col, uint8_t row, uint8_t ch)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM + (uint16_t)row * SCREEN_COLS + col;
    *dst = ch;
}

/**
 * Write a byte as two uppercase hex digits directly to screen RAM at
 * (col, row) and (col+1, row).
 *
 * @param col   Absolute screen column, 0-39.
 * @param row   Absolute screen row, 0-27.
 * @param value Byte to render as hex.
 * @return (none)
 */
static void put_hex2(uint8_t col, uint8_t row, uint8_t value)
{
    uint8_t hi = (uint8_t)(value >> 4);
    uint8_t lo = (uint8_t)(value & 0x0F);
    put_ch(col,             row, (uint8_t)(hi < 10 ? '0' + hi : 'A' + hi - 10));
    put_ch((uint8_t)(col+1), row, (uint8_t)(lo < 10 ? '0' + lo : 'A' + lo - 10));
}

/**
 * Clear the whole 40x28 text screen to spaces.
 *
 * @return (none)
 */
static void clear_screen(void)
{
    uint8_t *dst = (uint8_t *)TEXTVRAM;
    uint16_t i;
    for (i = 0; i < SCREEN_SIZE; i++) dst[i] = ' ';
}

/**
 * Draw every static label exactly once at startup: title, instructions,
 * the "R0:" .. "R7:" row prefixes and their "($..)"-shaped hex
 * placeholders, and the "Decoded ch:" / modifier-flag labels. Nothing
 * here changes after this call -- only the bit/hex/flag *values* drawn
 * over these labels change, every loop iteration in main().
 *
 * @return (none)
 */
static void draw_static_labels(void)
{
    uint8_t row;

    put_str(1, 0, "OSE-LOCI keyboard matrix test");
    put_str(1, 1, "Hold any key to see it light up below.");
    put_str(1, 3, "Row   col0 1 2 3 4 5 6 7      hex");

    for (row = 0; row < 8; row++)
    {
        uint8_t y = (uint8_t)(ROW_MATRIX0 + row);
        put_ch(1, y, 'R');
        put_ch(2, y, (uint8_t)('0' + row));
        put_str(3, y, ":");
        put_str((uint8_t)(COL_HEX - 2), y, "($");
        put_str((uint8_t)(COL_HEX + 2), y, ")");
    }

    put_str(1, ROW_DECODED, "Decoded ch: $");
    put_str((uint8_t)(COL_DECODED_HEX + 2), ROW_DECODED, " '");
    put_str((uint8_t)(COL_DECODED_GLYPH + 1), ROW_DECODED, "'");

    put_str(1, ROW_MODS, "Mods: SHIFT=");
    put_str((uint8_t)(COL_MOD_SHIFT + 1), ROW_MODS, " CTRL=");
    put_str((uint8_t)(COL_MOD_CTRL + 1), ROW_MODS, " FUNCT=");
    put_str((uint8_t)(COL_MOD_FUNCT + 1), ROW_MODS, " CAPS=");

    put_str(1, 16, "Hold any key and check its row/col bit");
    put_str(1, 17, "and decoded character above.");
    put_str(1, 19, "This program never exits on its own --");
    put_str(1, 20, "reset/power off to stop.");
}

/**
 * Entry point. Draws every static label once, then loops forever:
 * keyb_scan() (raw matrix scan) + keyb_decode() (raw decode, no
 * debounce/repeat), redrawing the full matrix, decoded character, and
 * modifier flags on every single iteration. Never returns.
 *
 * @return (never returns)
 */
int main(void)
{
    clear_screen();
    draw_static_labels();

    for (;;)
    {
        uint8_t row, ch;

        keyb_scan();
        ch = keyb_decode();

        for (row = 0; row < 8; row++)
        {
            uint8_t y  = (uint8_t)(ROW_MATRIX0 + row);
            uint8_t mb = keyb_matrix[row];
            uint8_t col;

            for (col = 0; col < 8; col++)
                put_ch((uint8_t)(COL_BIT0 + col * 2), y,
                       (uint8_t)((mb & (1 << col)) ? '1' : '0'));

            put_hex2(COL_HEX, y, mb);
        }

        put_hex2(COL_DECODED_HEX, ROW_DECODED, ch);
        put_ch(COL_DECODED_GLYPH, ROW_DECODED,
               (uint8_t)((ch >= 0x20 && ch < 0x7F) ? ch : ' '));

        {
            uint8_t mods = keyb_modifiers;
            put_ch(COL_MOD_SHIFT, ROW_MODS, (uint8_t)((mods & MOD_SHIFT)    ? '1' : '0'));
            put_ch(COL_MOD_CTRL,  ROW_MODS, (uint8_t)((mods & MOD_CTRL)     ? '1' : '0'));
            put_ch(COL_MOD_FUNCT, ROW_MODS, (uint8_t)((mods & MOD_FUNCT)    ? '1' : '0'));
            put_ch(COL_MOD_CAPS,  ROW_MODS, (uint8_t)((mods & MOD_CAPSLOCK) ? '1' : '0'));
        }
    }
}
