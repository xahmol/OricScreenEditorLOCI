// main.c - OricScreenEditorLOCI entry point
//
// Entry point: splash screen, canvas + statusbar init, main-mode editor loop.
// The splash is V1's actual title screen image, matching V1's own boot sequence (local reference at
// /home/xahmol/git/OricScreenEditor/src/main.c: loads "OSETSC.BIN" into
// SCREENMEMORY, overlays "Press key." at row 26, waits for a key). The
// image's own rows 24-25 already contain V1's "IDreamtIn8Bits.com /
// Written in 2022 by Xander Mol" credit text baked in; row 26 is blank,
// reserved for the "press key" overlay.
//
// Loaded from LOCI at runtime (assets/OSETSC.BIN, distributed via `make
// usb` to the LOCI device's root, see Makefile) -- a deliberate reversal
// of an earlier #embed-at-compile-time approach, which
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
#include "charsetswap.h"

AppState app;

/**
 * Program entry point. Shows the splash screen (V1's title image, loaded
 * from LOCI at runtime, MSG_SPLASH_PRESSKEY overlaid at row 27, the
 * screen's true last row -- post-launch correction, see "Boot splash" in
 * CLAUDE.md) and waits
 * for a keypress, then initialises the canvas, statusbar, and menu system
 * before handing control to the main-mode editor loop (editor_run(),
 * never returns).
 *
 * @return 0 (never reached).
 */
int main(void)
{
    // Must be the very first thing this program does, before anything
    // else can write to CHARSET_ALT ($B800) -- captures its genuine
    // boot/RESET-time content for Information > Version's idi8b_logo
    // popup to restore from later. See charsetswap.c's doc comment.
    charsetswap_capture_boot_alt();

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
    // homedir_init()/the title-screen load deliberately run BEFORE
    // enable_overlay_ram() (below) -- found 2026-06-21, real hardware:
    // enable_overlay_ram() writes MICRODISCCFG ($0314), adjacent to
    // LOCI's own register block ($0315/$0319, include/loci.h), and
    // issuing LOCI's first file ops of the session right after that
    // write consistently failed on real hardware (title/help screens
    // never loaded) even though every later LOCI call succeeded fine.
    // Moving these boot-time loads before the overlay-RAM toggle fixed
    // it -- don't move enable_overlay_ram() back above this block.
    homedir_init();

    // Populates locicfg.validdev[]/devnr so the file picker's drive-switch
    // keys (','/'.') can skip drives that don't actually exist, instead of
    // cycling through all 10 unconditionally -- must run in this same
    // pre-enable_overlay_ram() block (see the comment above) since it's
    // itself a round of LOCI file ops, same as homedir_init()/the
    // title-screen load.
    get_locicfg();

    {
        char    path[HOMEDIR_MAXLEN + 16];
        int16_t fd;
        homedir_join(path, MSG_TITLE_SCREEN_FILE);
        fd = loci_open(path, O_RDONLY);
        if (fd >= 0)
        {
            loci_read(fd, (void *)TEXTVRAM, VIEWPORT_HEIGHT * SCREEN_COLS);
            loci_close(fd);
        }
    }
    {
        // Row 27 is the screen's true last row (28 rows total, 0-27).
        // assets/OSETSC.BIN is now 1120 bytes (28 rows, padded blank on
        // this row -- see VIEWPORT_HEIGHT's appstate.h comment) so the
        // image load above already covers it; still blanked explicitly
        // here in case the file is missing/unreadable and the load was
        // skipped entirely, leaving stale screen content otherwise.
        uint8_t *row = (uint8_t *)TEXTVRAM + 27 * SCREEN_COLS;
        const char *msg = MSG_SPLASH_PRESSKEY;
        uint8_t col;
        for (col = 0; col < SCREEN_COLS; col++) row[col] = CH_SPACE;
        row[0] = A_FWWHITE;
        row[1] = A_BGBLACK;
        for (col = 0; msg[col]; col++) row[col + 2] = (uint8_t)msg[col];
    }
    key_read();

    enable_overlay_ram();
    canvas_init();
    statusbar_init();
    menu_init();
    menudata_init();
    editor_run();

    return 0;
}
