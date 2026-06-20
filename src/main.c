// main.c - OricScreenEditorLOCI entry point
//
// Phase 1: splash screen, canvas + statusbar init, main-mode editor loop.
// Phase 9 (revised after Phase 9c shipped): the splash is V1's actual
// title screen image, matching V1's own boot sequence (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c: loads "OSETSC.BIN" into
// SCREENMEMORY, overlays "Press key." at row 26, waits for a key). The
// image's own rows 24-25 already contain V1's "IDreamtIn8Bits.com /
// Written in 2022 by Xander Mol" credit text baked in; row 26 is blank,
// reserved for the "press key" overlay.
//
// Loaded from LOCI at runtime (assets/OSETSC.BIN, distributed via `make
// usb` to the LOCI device's root, see Makefile) -- a deliberate reversal
// of the earlier #embed-at-compile-time approach (Phase 9b/9c), which
// cost ~5.4KB of the $0580-$B200 code/data/bss region purely to keep
// these 5 screens (this title image + the 4 help screens, see
// src/help.c) usable with no LOCI device attached. Now that LOCI is
// mandatory to even boot (below), that tradeoff buys nothing any more --
// loading from LOCI instead frees that headroom AND makes the title
// image editable directly in OSE itself (dog-fooding, since OSE can now
// load/save its own bare screen-dump format, see "LOCI file I/O").
// Missing/unreadable file degrades gracefully (skips the image, still
// shows the press-key prompt and proceeds) rather than erroring -- same
// spirit as V1's own helpscreen_load(), whose "Insert application disk."
// fallback popup is commented out in V1's actual source.
//
// LOCI is now REQUIRED, not optional: screenmap[] (src/canvas.h) lives in
// Oric-side overlay RAM, which only exists with a LOCI device attached.
// The hard loci_present() gate below, shown before anything else (even
// the splash), is adapted from locifilemanager-v2's own main() (local
// reference, /home/xahmol/git/locifilemanager-v2/src/main.c lines
// 485-497) -- same MSG_LOCI_NOT_FOUND/MSG_PRESS_KEY_EXIT popup shape;
// those two strings were already defined in strings_en.h/strings_fr.h,
// unused, pre-staged for exactly this. See CLAUDE.md "Memory Layout".

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
#include "homedir.h"

AppState app;

/**
 * Program entry point. Shows the splash screen (V1's title image, loaded
 * from LOCI at runtime, MSG_SPLASH_PRESSKEY overlaid at row 26) and waits
 * for a keypress, then initialises the canvas, statusbar, and menu system
 * before handing control to the main-mode editor loop (editor_run(),
 * never returns).
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
    homedir_init();

    {
        char    path[HOMEDIR_MAXLEN + 16];
        int16_t fd;
        homedir_join(path, "OSETSC.BIN");
        fd = loci_open(path, O_RDONLY);
        if (fd >= 0)
        {
            loci_read(fd, (void *)TEXTVRAM, VIEWPORT_HEIGHT * SCREEN_COLS);
            loci_close(fd);
        }
    }
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
