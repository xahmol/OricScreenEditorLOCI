// main.c - OricScreenEditorLOCI entry point
//
// Phase 1: splash screen, canvas + statusbar init, main-mode editor loop.
// Phase 9 (revised after Phase 9c shipped): the splash is V1's actual
// title screen image (assets/OSEforLOCI-Title.bin, carried over from V1
// -- see ARCHITECTURE.md/CLAUDE.md "Source Layout"), matching V1's own
// boot sequence (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c: loads "OSETSC.BIN" into
// SCREENMEMORY, overlays "Press key." at row 26, waits for a key). OSE
// embeds the image at compile time (#embed lzo, no LOCI dependency)
// instead of V1's tape load, and overlays MSG_SPLASH_PRESSKEY the same
// way. The image's own rows 24-25 already contain V1's "IDreamtIn8Bits.com
// / Written in 2022 by Xander Mol" credit text baked in; row 26 is blank,
// reserved for the "press key" overlay.
//
// LOCI is now REQUIRED, not optional: screenmap[] (src/canvas.h) lives in
// Oric-side overlay RAM, which only exists with a LOCI device attached.
// The hard loci_present() gate below, shown before anything else (even
// the splash), is adapted from locifilemanager-v2's own main() (local
// reference, /home/xahmol/git/locifilemanager-v2/src/main.c lines
// 485-497) -- same MSG_LOCI_NOT_FOUND/MSG_PRESS_KEY_EXIT popup shape;
// those two strings were already defined in strings_en.h/strings_fr.h,
// unused, pre-staged for exactly this. See CLAUDE.md "Memory Layout".

#include <oscar.h>
#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "menu.h"
#include "menudata.h"
#include "editor.h"
#include "strings.h"
#include "ijk.h"
#include "input.h"
#include "loci.h"

AppState app;

static const char title_screen[] = {
    #embed lzo "../assets/OSEforLOCI-Title.bin"
};

/**
 * Program entry point. Shows the splash screen (V1's title image,
 * MSG_SPLASH_PRESSKEY overlaid at row 26) and waits for a keypress, then
 * initialises the canvas, statusbar, and menu system before handing
 * control to the main-mode editor loop (editor_run(), never returns).
 *
 * @return 0 (never reached).
 */
int main(void)
{
    charwin_init();
    ijk_detect();

    if (!loci_present())
    {
        OricCharWin scr;
        cwin_init(&scr, 2, 0, 38, 28, A_FWWHITE, A_BGBLACK);
        cwin_clear(&scr);
        cwin_putat_string(&scr, 0, 0, MSG_LOCI_NOT_FOUND);
        cwin_putat_string(&scr, 0, 2, MSG_PRESS_KEY_EXIT);
        key_read();
        return 0;
    }
    enable_overlay_ram();

    oscar_expand_lzo((char *)TEXTVRAM, title_screen);
    {
        uint8_t *row = (uint8_t *)TEXTVRAM + 26 * SCREEN_COLS;
        const char *msg = MSG_SPLASH_PRESSKEY;
        uint8_t col = 0;
        while (msg[col]) { row[col] = (uint8_t)msg[col]; col++; }
    }
    key_read();

    canvas_init();
    statusbar_init();
    menu_init();
    menudata_init();
    editor_run();

    return 0;
}
