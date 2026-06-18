// findreplace.c - Unified Find/Replace for OricScreenEditorLOCI (Oric
// Atmos, Oscar64)
//
// New functionality, no V1/VDCScreenEditor2 precedent -- inspired by a
// feature-scoping pass over VDCSE2's feature set (which has separate
// global find/replace-by-screencode and recolor actions) but designed
// fresh for OSE as a single entry point covering both, plus a
// find-only mode, rather than three separate menu items/keys.
//
// Replace-all scans the whole canvas and is therefore the same scale
// of operation as Screen > Clear/Fill -- it snapshots the whole canvas
// for undo first, which on a maximised canvas can exceed the 6KB undo
// region. This deliberately inherits undo_snapshot()'s existing
// graceful "too big to undo" skip (see src/undo.c) rather than adding
// new safety logic.

#include <string.h>
#include "oric.h"
#include "charwin.h"
#include "keyboard.h"
#include "appstate.h"
#include "canvas.h"
#include "menu.h"
#include "strings.h"
#include "undo.h"
#include "input.h"
#include "findreplace.h"

#define FR_TARGET_CODE  0
#define FR_TARGET_INK   1
#define FR_TARGET_PAPER 2

/**
 * Parse a single hexadecimal digit character.
 *
 * @param c Character '0'-'9', 'a'-'f', or 'A'-'F'.
 * @return Numeric value 0-15, or 0 if c is not a hex digit.
 */
static uint8_t fr_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

/**
 * Parse a NUL-terminated hex digit string into a byte, most significant
 * digit first.
 *
 * @param s NUL-terminated hex digit string (up to 2 digits expected).
 * @return Parsed numeric value.
 */
static uint8_t fr_parse_hex(const char *s)
{
    uint8_t v = 0;
    while (*s) { v = (uint8_t)((v << 4) | fr_hex_digit(*s)); s++; }
    return v;
}

/**
 * Parse a NUL-terminated decimal digit string into a byte.
 *
 * @param s NUL-terminated decimal digit string.
 * @return Parsed numeric value.
 */
static uint8_t fr_parse_dec(const char *s)
{
    uint8_t v = 0;
    while (*s >= '0' && *s <= '9') { v = (uint8_t)(v * 10 + (uint8_t)(*s - '0')); s++; }
    return v;
}

/**
 * Test whether a screenmap byte matches the given target/value: an
 * exact match for FR_TARGET_CODE; for ink/paper, the byte must first be
 * a serial attribute byte ((b & 0x60) == 0 -- the same test used
 * elsewhere in this codebase) and fall in the matching 0x00-0x07
 * (ink)/0x10-0x17 (paper) sub-range.
 *
 * @param b      Screenmap byte to test.
 * @param target FR_TARGET_CODE/INK/PAPER.
 * @param value  Value to match (raw screencode for CODE, 0-7 for
 *               INK/PAPER).
 * @return 1 if b matches, 0 otherwise.
 */
static uint8_t fr_matches(uint8_t b, uint8_t target, uint8_t value)
{
    switch (target)
    {
    case FR_TARGET_INK:
        return (uint8_t)(((b & 0x60) == 0) && (b < 0x08) && (b == value));
    case FR_TARGET_PAPER:
        return (uint8_t)(((b & 0x60) == 0) && (b >= 0x10) && (b < 0x18) && (b == (uint8_t)(0x10 + value)));
    default:
        return (uint8_t)(b == value);
    }
}

/**
 * Unified Find/Replace, entered via 'f' from Main mode. See
 * findreplace.h for the full contract.
 *
 * @return (none)
 */
void findreplace_run(void)
{
    OricCharWin win;
    char        buf[6];
    uint8_t     target, key;
    uint8_t     findval, replaceval = 0, hasreplace;
    uint16_t    total, i, startidx;

    menu_winsave(5, 14, 1);
    cwin_init(&win, 5, 5, 35, 14, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);

    cwin_putat_string(&win, 2, 1, MSG_FR_TITLE);
    cwin_putat_string(&win, 2, 3, MSG_FR_TARGET_PROMPT);

    do
    {
        key = key_read();
        if (key == KEY_ESC) { menu_winrestore(); return; }
    } while (key != '1' && key != '2' && key != '3');

    target = (uint8_t)(key - '1'); // '1'->CODE(0), '2'->INK(1), '3'->PAPER(2)

    // Both fields are pre-filled with a non-empty default ("00"/"0") and
    // navigated with \l-style left-arrow + overwrite, the same convention
    // every other cwin_textinput() call in this codebase uses (see
    // resize_dialog()/goto_dialog()). Found while debugging this: digits
    // alone don't actually need this (an empty initial buffer extends
    // fine) -- the real bug was passing bare VINPUT_ALPHA for hex input
    // (see the VINPUT_NUMS|VINPUT_ALPHA fix below and its comment).
    cwin_putat_string(&win, 2, 5, (target == FR_TARGET_CODE) ? MSG_FR_FIND_CODE : MSG_FR_FIND_COLOR);
    strcpy(buf, (target == FR_TARGET_CODE) ? "00" : "0");
    if (target == FR_TARGET_CODE)
    {
        if (cwin_textinput(&win, 2, 6, 8, buf, 2, VINPUT_NUMS | VINPUT_ALPHA) < 0) { menu_winrestore(); return; }
        findval = fr_parse_hex(buf);
    }
    else
    {
        if (cwin_textinput(&win, 2, 6, 8, buf, 1, VINPUT_NUMS) < 0) { menu_winrestore(); return; }
        findval = fr_parse_dec(buf);
        if (findval > 7) findval = 7;
    }

    // The replace field's ESC means "find-only" (not "cancel everything")
    // -- distinguishing find-only from replace-all by ESC vs ENTER here,
    // rather than by an empty string, since starting this field's buffer
    // genuinely empty was the original (broken) design.
    cwin_putat_string(&win, 2, 8, MSG_FR_REPLACE_PROMPT);
    strcpy(buf, (target == FR_TARGET_CODE) ? "00" : "0");
    if (target == FR_TARGET_CODE)
        hasreplace = (uint8_t)(cwin_textinput(&win, 2, 9, 8, buf, 2, VINPUT_NUMS | VINPUT_ALPHA) >= 0);
    else
        hasreplace = (uint8_t)(cwin_textinput(&win, 2, 9, 8, buf, 1, VINPUT_NUMS) >= 0);

    if (hasreplace)
    {
        replaceval = (target == FR_TARGET_CODE) ? fr_parse_hex(buf) : fr_parse_dec(buf);
        if (target != FR_TARGET_CODE && replaceval > 7) replaceval = 7;
    }

    menu_winrestore();

    total    = (uint16_t)(app.canvas_width * app.canvas_height);
    startidx = (uint16_t)((app.cursor_y + app.yoffset) * app.canvas_width + (app.cursor_x + app.xoffset));

    if (!hasreplace)
    {
        // Find-only: scan forward from just after the cursor, wrapping
        // around the whole canvas back to (and including) the start.
        for (i = 1; i <= total; i++)
        {
            uint16_t idx = (uint16_t)((startidx + i) % total);
            if (fr_matches(screenmap[idx], target, findval))
            {
                canvas_goto((uint16_t)(idx % app.canvas_width), (uint16_t)(idx / app.canvas_width));
                return;
            }
        }
        return; // no match anywhere
    }

    // Replace-all: whole-canvas undo_snapshot() first, same scale as
    // Screen > Clear/Fill -- gracefully skipped by undo_snapshot() if
    // it doesn't fit the undo region (see src/undo.c).
    undo_snapshot(0, 0, app.canvas_width, app.canvas_height);
    {
        uint8_t newbyte = (target == FR_TARGET_PAPER) ? (uint8_t)(0x10 + replaceval) : replaceval;
        for (i = 0; i < total; i++)
            if (fr_matches(screenmap[i], target, findval))
                screenmap[i] = newbyte;
    }
    canvas_blit();
}
