// help.c - Help screens for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Ported from V1 OricScreenEditor's helpscreen_load(screennumber)
// (/home/xahmol/git/OricScreenEditor/src/main.c): same 4 screens (Main,
// Character editor, Select/Move/Line-Box, Write), same charset-swap
// around the raw screen-RAM blit. Adapted: V1 tape-loads each 1080-byte
// dump at runtime and must manually save/restore the underlying screen
// RAM; OSE embeds the dumps at compile time (#embed, no LOCI dependency)
// and restores the real canvas/statusbar from screenmap[]/AppState
// instead of a manual save buffer. Embedded LZO-compressed (oscar.h's
// oscar_expand_lzo(), decompressing straight into $BB80 screen RAM, which
// sits outside this project's $0580-$B200 code/data/bss region) --
// raw 1080-byte-per-screen embeds left too little budget once the
// Information menu's title image + QR code (Phase 9c) were added too.

#include "oric.h"
#include "charsetswap.h"
#include "canvas.h"
#include "statusbar.h"
#include "input.h"
#include "help.h"
#include <oscar.h>

// #embed must be the only thing on its line -- oscar64 mis-tokenizes the
// embedded byte stream if `{ #embed ... }` shares a line with other code.
static const char help_screen1[] = {
    #embed lzo "../assets/OSEforLOCI-Help1.bin"
};
static const char help_screen2[] = {
    #embed lzo "../assets/OSEforLOCI-Help2.bin"
};
static const char help_screen3[] = {
    #embed lzo "../assets/OSEforLOCI-Help3.bin"
};
static const char help_screen4[] = {
    #embed lzo "../assets/OSEforLOCI-Help4.bin"
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
    const char *src;

    switch (screennumber)
    {
        case 1: src = help_screen1; break;
        case 2: src = help_screen2; break;
        case 3: src = help_screen3; break;
        case 4: src = help_screen4; break;
        default: return;
    }

    charsetswap_enter();

    oscar_expand_lzo((char *)TEXTVRAM, src);

    key_read();

    charsetswap_exit();

    canvas_blit();
    statusbar_draw();
}
