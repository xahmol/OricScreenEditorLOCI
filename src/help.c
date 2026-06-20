// help.c - Help screens for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Ported from V1 OricScreenEditor's helpscreen_load(screennumber)
// (/home/xahmol/git/OricScreenEditor/src/main.c): same 4 screens (Main,
// Character editor, Select/Move/Line-Box, Write), same charset-swap
// around the raw screen-RAM blit, same "OSEHS<n>.BIN" filenames. Adapted:
// V1 tape-loads each 1080-byte dump and must manually save/restore the
// underlying screen RAM; OSE loads from LOCI (loci_open/loci_read/
// loci_close, see include/loci.h) and restores the real canvas/statusbar
// from screenmap[]/AppState instead of a manual save buffer.
//
// Runtime LOCI load is a deliberate reversal of an earlier #embed-at-
// compile-time approach -- see src/main.c's header comment
// for the full rationale (frees ~5.4KB of code/data/bss, makes these
// screens editable in OSE itself). A missing/unreadable file degrades
// gracefully (skips the screen, key_read() still waits so the popup
// doesn't feel broken, then restores the canvas as normal) -- same
// spirit as V1's own commented-out "Insert application disk." fallback.

#include <stdio.h>
#include "oric.h"
#include "appstate.h"
#include "charsetswap.h"
#include "canvas.h"
#include "statusbar.h"
#include "input.h"
#include "loci.h"
#include "homedir.h"
#include "help.h"

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
    char name[12];
    char path[HOMEDIR_MAXLEN + 16];
    int16_t fd;

    if (screennumber < 1 || screennumber > 4) return;

    sprintf(name, "OSEHS%u.BIN", screennumber);
    homedir_join(path, name);

    charsetswap_enter();

    fd = loci_open(path, O_RDONLY);
    if (fd >= 0)
    {
        loci_read(fd, (void *)TEXTVRAM, VIEWPORT_HEIGHT * SCREEN_COLS);
        loci_close(fd);
    }

    key_read();

    charsetswap_exit();

    canvas_blit();
    statusbar_draw();
}
