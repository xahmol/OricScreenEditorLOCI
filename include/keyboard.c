// keyboard.c - Oric Atmos keyboard scanner (direct VIA, no ROM calls)
//
// Scanning algorithm based on LOCI ROM keyboard.s by Sodiumlightbaby, 2024
// https://github.com/sodiumlb/loci-rom
// Adapted: Oscar64 C + inline __asm block; polling only; no AY audio init side-effects.
//
// Hardware: AY-3-8912 register $0E (IOA) drives keyboard columns (active-low,
// one bit at a time via PCR/CB2 gating); VIA Port B bits 0-2 select the row
// via a 4051 analog mux; VIA Port B bit 3 reads back the key sense signal.

#include "oric.h"
#include "keyboard.h"

// -------------------------------------------------------------------------
// Key decode tables (64 entries each: row 0 col 0 ... row 7 col 7)
//
// Source: LOCI ROM keyboard.s _KeyAsciiUpper / _KeyAsciiLower tables,
// combined with CC65 atmos.h CH_* constants for special keys.
// Index = row * 8 + col.
// 0 = no key / modifier handled separately.
// -------------------------------------------------------------------------

static const uint8_t decode_normal[64] = {
    // Row 0: 7   N   5   V  RCTRL  1   X   3
    '7', 'n', '5', 'v',  0,   '1', 'x', '3',
    // Row 1: J   T   R   F   --   ESC   Q   D
    'j', 't', 'r', 'f',  0,  KEY_ESC, 'q', 'd',
    // Row 2: M   6   B   4  LCTRL  Z   2   C
    'm', '6', 'b', '4',  0,  'z', '2', 'c',
    // Row 3: K   9   ;   -  (0)   (0)  \   '
    'k', '9', ';', '-',  0,   0,   '\\','\'',
    // Row 4: SP  ,   .  UP  LSHIFT LEFT DOWN RIGHT
    ' ', ',', '.', KEY_UP, 0,  KEY_LEFT, KEY_DOWN, KEY_RIGHT,
    // Row 5: U   I   O   P  FUNCT DEL  ]   [
    'u', 'i', 'o', 'p',  0,   KEY_DEL, ']', '[',
    // Row 6: Y   H   G   E  RALT  A    S   W
    'y', 'h', 'g', 'e',  0,  'a', 's', 'w',
    // Row 7: 8   L   0   /  RSHIFT RETURN  =   --
    '8', 'l', '0', '/',  0,   KEY_ENTER, '=', 0,
};

static const uint8_t decode_shifted[64] = {
    // Row 0: &   N   %   V  RCTRL  !   X   #
    '&', 'N', '%', 'V',  0,   '!', 'X', '#',
    // Row 1: J   T   R   F   --   ESC   Q   D
    'J', 'T', 'R', 'F',  0,  KEY_ESC, 'Q', 'D',
    // Row 2: M   ^   B   $  LCTRL  Z   @   C
    'M', '^', 'B', '$',  0,  'Z', '@', 'C',
    // Row 3: K   (   :   _  (0)   (0)  |   "
    'K', '(', ':', '_',  0,   0,   '|', '"',
    // Row 4: SP  <   >  UP  LSHIFT LEFT DOWN RIGHT
    ' ', '<', '>', KEY_UP, 0,  KEY_LEFT, KEY_DOWN, KEY_RIGHT,
    // Row 5: U   I   O   P  FUNCT DEL  }   {
    'U', 'I', 'O', 'P',  0,   KEY_DEL, '}', '{',
    // Row 6: Y   H   G   E  RALT  A    S   W
    'Y', 'H', 'G', 'E',  0,  'A', 'S', 'W',
    // Row 7: *   L   )   ?  RSHIFT RETURN  +  --
    '*', 'L', ')', '?',  0,   KEY_ENTER, '+', 0,
};

// FUNCT + digit/key maps: FUNCT+1=F1, FUNCT+2=F2, ... FUNCT+0=F10
static const uint8_t decode_funct[64] = {
    // Row 0: FUNCT+7=F7  FUNCT+N  FUNCT+5=F5  FUNCT+V  --  FUNCT+1=F1  FUNCT+X  FUNCT+3=F3
    KEY_F7,  0,      KEY_F5, 0,  0,  KEY_F1, 0,  KEY_F3,
    // Row 1: FUNCT+R=F4 (per v1 convention), FUNCT+ESC=ESC at col 5
    0, 0, KEY_F4, 0, 0, KEY_ESC, 0, 0,
    // Row 2: FUNCT+6=F6  FUNCT+2=F2
    0, KEY_F6, 0, 0,  0,  0,  KEY_F2, 0,
    // Row 3: FUNCT+9=F9
    0, KEY_F9, 0, 0,  0,  0,   0,     0,
    // Row 4: unused
    0, 0, 0,  0,      0,  0,   0,     0,
    // Row 5: FUNCT+U=F8, FUNCT+I=F8, FUNCT+O=F10
    0, KEY_F8, KEY_F10, 0,  0,  0,  0, 0,
    // Row 6: unused
    0, 0, 0,  0,  0, 0, 0,  0,
    // Row 7: FUNCT+0=F10
    0, 0, KEY_F10, 0,  0,  0, 0, 0,
};

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

uint8_t           keyb_matrix[8];
volatile uint8_t  keyb_char;
uint8_t           keyb_modifiers;

static uint8_t     keyb_capslock;
static uint8_t     prev_key;        // scan code of key held last poll
static uint16_t    rep_count;       // repeat countdown
static uint8_t     release_count;   // required no-key polls before next press accepted

// Calibrated to match Oric ROM defaults: KBDLY=$024E=40, KBRPT=$024F=4 (units ~30ms).
// keyb_scan takes ~370 cycles at 1 MHz with one key held → ~0.37 ms per poll.
// REP_DELAY=3000 → ~1110 ms (ROM KBDLY=40×30ms=1200ms).
// REP_RATE=300   → ~110 ms  (ROM KBRPT=4×30ms=120ms).
// RELEASE_DEBOUNCE: polls of no-key required after each accepted press before a
// new key is registered. Eliminates switch bounce (< 5 ms) and cross-section
// carryover without affecting typing speed (20 × ~0.37 ms ≈ 7 ms << 100 ms/char).
#define REP_DELAY        3000
#define REP_RATE          300
#define RELEASE_DEBOUNCE   20

// -------------------------------------------------------------------------
// ZP temporaries used by keyb_scan assembly
// -------------------------------------------------------------------------

static __zeropage uint8_t _kbz0;   // candidate matrix byte
static __zeropage uint8_t _kbz1;   // current row index

// -------------------------------------------------------------------------
// keyb_scan -- scan the full 8x8 key matrix into keyb_matrix[]
//
// Populates keyb_matrix[8] via direct VIA/AY access.
// Mirrors LOCI ROM ReadKeyboard (keyboard.s, Sodiumlightbaby):
//   outer loop: rows 7->0, selected via VIA Port B bits 0-2
//   inner loop: columns 7->0, driven via AY register $0E (IOA) active-low
//   sense:      VIA Port B bit 3 = 1 when key pressed at current row+col
//
// PCR ($030C) values toggle CB2 to gate AY column output.
// $FF = CB2 high (deassert), $DD = CB2 low (assert/latch).
//
// Oscar64 inline asm syntax notes:
//   - Immediate values: use decimal or 0x (not $hex)
//   - Hardware addresses > $FF: use [0x030f] bracket notation
//   - C variable names are valid operands directly
// -------------------------------------------------------------------------

/**
 * Scan the full 8x8 keyboard matrix via direct AY/VIA access and populate
 * keyb_matrix[8] (bit N of keyb_matrix[row] = 1 if the key at that row/col
 * is currently pressed). Takes ~1-2 ms (64 AY+VIA write/read cycles at
 * 1 MHz). Call from a polling loop.
 *
 * @return (none) -- result is written to the global keyb_matrix[].
 */
void keyb_scan(void)
{
    // Oscar64 inline asm decimal bug: values 10-15 compile to wrong immediates.
    // Use C volatile writes to select AY register $0E (IOA = keyboard columns).
    *((volatile uint8_t *)0x030F) = 14;   // VIA Port A: select AY reg $0E
    *((volatile uint8_t *)0x030C) = 255;  // PCR = $FF: CB2 high
    *((volatile uint8_t *)0x030C) = 221;  // PCR = $DD: CB2 low, latch reg $0E

    __asm {
        ldx     #7               // row counter: 7 down to 0

    brow:
        // Clear this row's matrix entry
        lda     #0
        sta     keyb_matrix, x

        // Write 0 to AY IOA (all columns driven simultaneously for quick sense)
        sta     [0x030f]
        lda     #253             // $FD
        sta     [0x030c]
        lda     #221             // $DD
        sta     [0x030c]

        // Set row select in VIA Port B bits 0-2 (preserve bits 3-7)
        lda     [0x0300]         // read current VIA PRB
        and     #248             // $F8: keep bits 3-7
        stx     _kbz1
        ora     _kbz1
        sta     [0x0300]

        // Quick sense: any key on this row? (Port B bit 3 = 1 means key active)
        ldy     #128             // $80: column bit mask (starts at bit 7)
        lda     #8
        and     [0x0300]         // bit 3 test
        beq     bskip_row        // 0 means no key on row, skip column scan

    bcol:
        // Drive one column low: col_mask = ~y (y = $80->$40->...->$01)
        tya
        eor     #255             // $FF: invert
        sta     [0x030f]         // VIA Port A = AY column drive (active-low)
        lda     #253             // $FD
        sta     [0x030c]
        lda     #221             // $DD
        sta     [0x030c]

        // Restore row (Port B write for AY above may disturb bits 0-2)
        lda     [0x0300]
        and     #248             // $F8
        ora     _kbz1
        sta     [0x0300]

        // Prepare candidate: OR col bit into matrix byte
        tya
        ora     keyb_matrix, x
        sta     _kbz0

        // Sense key at this row+col (bit 3 of Port B = 1 means pressed)
        lda     #8
        and     [0x0300]
        beq     bskip_col        // 0 means not pressed

        lda     _kbz0
        sta     keyb_matrix, x   // store: key pressed at this position

    bskip_col:
        // Advance to next column (shift mask right)
        tya
        lsr
        tay
        bcc     bcol             // carry clear means more columns to scan

    bskip_row:
        dex
        bpl     brow
    }
}

// -------------------------------------------------------------------------
// keyb_decode -- decode keyb_matrix[] into an ASCII key code
// -------------------------------------------------------------------------

/**
 * Decode the current keyb_matrix[] (as populated by keyb_scan()) into a
 * single key code, updating the global keyb_modifiers (MOD_SHIFT/MOD_CTRL/
 * MOD_FUNCT/MOD_CAPSLOCK) along the way. Applies SHIFT/FUNCT lookup tables,
 * Caps Lock case-folding for letters, and CTRL masking (letter & 0x1F).
 * Only the first pressed key found (scanning row 0..7, col 0..7) is
 * decoded; pure modifier keys (SHIFT/CTRL/FUNCT) are skipped.
 *
 * @return Decoded ASCII/KEY_* code, or KEY_NONE (0) if no non-modifier key
 *         is pressed.
 */
uint8_t keyb_decode(void)
{
    uint8_t mods = 0;
    if (keyb_matrix[VKEY_LSHIFT >> 3] & (1 << (VKEY_LSHIFT & 7))) mods |= MOD_SHIFT;
    if (keyb_matrix[VKEY_RSHIFT >> 3] & (1 << (VKEY_RSHIFT & 7))) mods |= MOD_SHIFT;
    if (keyb_matrix[VKEY_LCTRL  >> 3] & (1 << (VKEY_LCTRL  & 7))) mods |= MOD_CTRL;
    if (keyb_matrix[VKEY_RCTRL  >> 3] & (1 << (VKEY_RCTRL  & 7))) mods |= MOD_CTRL;
    if (keyb_matrix[VKEY_FUNCT  >> 3] & (1 << (VKEY_FUNCT  & 7))) mods |= MOD_FUNCT;
    if (keyb_capslock) mods |= MOD_CAPSLOCK;
    keyb_modifiers = mods;

    for (uint8_t row = 0; row < 8; row++)
    {
        uint8_t rowbits = keyb_matrix[row];
        if (!rowbits) continue;

        for (uint8_t col = 0; col < 8; col++)
        {
            if (!(rowbits & (1 << col))) continue;

            uint8_t pos = (uint8_t)(row * 8 + col);

            if (pos == VKEY_LSHIFT || pos == VKEY_RSHIFT ||
                pos == VKEY_LCTRL  || pos == VKEY_RCTRL  ||
                pos == VKEY_FUNCT)
                continue;

            uint8_t ch;
            if (mods & MOD_FUNCT)
            {
                ch = decode_funct[pos];
            }
            else if (mods & MOD_SHIFT)
            {
                ch = decode_shifted[pos];
            }
            else
            {
                ch = decode_normal[pos];
                if ((mods & MOD_CAPSLOCK) && ch >= 'a' && ch <= 'z')
                    ch = (uint8_t)(ch - 32);
            }

            if (!ch) continue;

            if ((mods & MOD_CTRL) && ch >= 'a' && ch <= 'z')
                ch = (uint8_t)(ch & 0x1F);
            else if ((mods & MOD_CTRL) && ch >= 'A' && ch <= 'Z')
                ch = (uint8_t)(ch & 0x1F);

            return ch;
        }
    }

    return KEY_NONE;
}

// -------------------------------------------------------------------------
// keyb_poll -- scan + decode + key repeat
// -------------------------------------------------------------------------

/**
 * Scan and decode the keyboard (via keyb_scan()/keyb_decode()), then apply
 * key-repeat and release-debounce logic: a newly-pressed key is returned
 * immediately and then suppressed for REP_DELAY polls, after which it
 * repeats every REP_RATE polls while held; after any key is released,
 * RELEASE_DEBOUNCE no-key polls must elapse before a new key is accepted.
 * Updates the global keyb_char to the same value as the return value.
 *
 * @return Decoded ASCII/KEY_* code for this poll, or KEY_NONE (0) if no key
 *         event should be reported this poll.
 */
uint8_t keyb_poll(void)
{
    keyb_scan();
    uint8_t ch = keyb_decode();

    if (!ch)
    {
        if (release_count > 0) release_count--;
        prev_key  = KEY_NONE;
        rep_count = 0;
        keyb_char = KEY_NONE;
        return KEY_NONE;
    }

    // Block new-key detection until release debounce window has elapsed.
    // release_count only decrements when ch==0, so held-key auto-repeat is unaffected.
    if (release_count > 0) return KEY_NONE;

    if (ch == prev_key)
    {
        if (rep_count == 0)
        {
            rep_count = REP_RATE;
            keyb_char = ch;
            return ch;
        }
        else
        {
            rep_count--;
            keyb_char = KEY_NONE;
            return KEY_NONE;
        }
    }
    else
    {
        prev_key      = ch;
        rep_count     = REP_DELAY;
        release_count = RELEASE_DEBOUNCE;
        keyb_char     = ch;
        return ch;
    }
}

// -------------------------------------------------------------------------
// keyb_getch -- blocking key read
// -------------------------------------------------------------------------

/**
 * Block until a key is pressed, by repeatedly calling keyb_poll() until it
 * returns non-zero.
 *
 * @return Decoded ASCII/KEY_* code of the pressed key (never KEY_NONE).
 */
uint8_t keyb_getch(void)
{
    uint8_t ch;
    do {
        ch = keyb_poll();
    } while (!ch);
    return ch;
}

// -------------------------------------------------------------------------
// keyb_check -- non-blocking: return current keyb_char
// -------------------------------------------------------------------------

/**
 * Non-blocking keyboard check: performs one keyb_poll() and returns its
 * result without waiting for a key.
 *
 * @return Decoded ASCII/KEY_* code if a key event is pending this poll, or
 *         KEY_NONE (0) otherwise.
 */
uint8_t keyb_check(void)
{
    return keyb_poll();
}
