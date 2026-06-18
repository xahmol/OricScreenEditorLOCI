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
