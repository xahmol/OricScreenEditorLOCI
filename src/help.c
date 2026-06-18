// help.c - Help screens for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Ported from V1 OricScreenEditor's helpscreen_load(screennumber)
// (/home/xahmol/git/OricScreenEditor/src/main.c): same 4 screens (Main,
// Character editor, Select/Move/Line-Box, Write), same charset-swap
// around the raw screen-RAM blit. Adapted: V1 tape-loads each 1080-byte
// dump at runtime and must manually save/restore the underlying screen
// RAM; OSE embeds the dumps at compile time (#embed, no LOCI dependency)
// and restores the real canvas/statusbar from screenmap[]/AppState
// instead of a manual save buffer.

#include "oric.h"
#include "charsetswap.h"
#include "canvas.h"
#include "statusbar.h"
#include "input.h"
#include "help.h"

// #embed must be the only thing on its line -- oscar64 mis-tokenizes the
// embedded byte stream if `{ #embed ... }` shares a line with other code.
static const uint8_t help_screen1[] = {
    #embed "../assets/OSEforLOCI-Help1.bin"
};
static const uint8_t help_screen2[] = {
    #embed "../assets/OSEforLOCI-Help2.bin"
};
static const uint8_t help_screen3[] = {
    #embed "../assets/OSEforLOCI-Help3.bin"
};
static const uint8_t help_screen4[] = {
    #embed "../assets/OSEforLOCI-Help4.bin"
};

/**
 * Show the help screen for screennumber (1=Main, 2=Character editor,
 * 3=Select/Move/Line-Box, 4=Write mode), waiting for a keypress, then
 * restore the real canvas and statusbar. No-op for any other value.
 *
 * @param screennumber Which help screen to show (1-4).
 * @return (none)
 */
void help_show(uint8_t screennumber)
{
    const uint8_t *src;

    switch (screennumber)
    {
        case 1: src = help_screen1; break;
        case 2: src = help_screen2; break;
        case 3: src = help_screen3; break;
        case 4: src = help_screen4; break;
        default: return;
    }

    charsetswap_enter();

    uint8_t *dst = (uint8_t *)TEXTVRAM;
    for (uint16_t i = 0; i < 1080; i++)
        dst[i] = src[i];

    key_read();

    charsetswap_exit();

    canvas_blit();
    statusbar_draw();
}
