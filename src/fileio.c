// fileio.c - LOCI file I/O for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Wires include/loci.c/h (copied verbatim from locifilemanager-v2, not
// previously #included from any src/ file) into OSE's File/Charset menus.
// Calling-convention reference: locifilemanager-v2's loci_present()
// startup gate and file_save()/file_load() single-blob convention
// (src/dir.c config_save()/config_load() there) -- adapted here to a
// per-action presence check (see loci_check_present()) rather than a
// startup-blocking one, since OSE must keep working with no LOCI device
// attached (editing, and Phosphoric/Oricutron testing of everything else).

#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "strings.h"
#include "loci.h"
#include "fileio.h"

/**
 * Check whether a LOCI device is present, showing MSG_LOCI_NOT_FOUND via
 * menu_messagepopup() if not. Called as the first step of every File/
 * Charset menu action, so editing and every other OSE feature keep
 * working with no LOCI device attached.
 *
 * @return 1 if a LOCI device is present, 0 if absent.
 */
uint8_t loci_check_present(void)
{
    if (loci_present()) return 1;
    menu_messagepopup(MSG_LOCI_NOT_FOUND);
    return 0;
}

/**
 * Prompt for a LOCI base filename in a small popup: title (the calling
 * action's MSG_FILE_.../MSG_CHARSET_... label) on row 1, MSG_FILE_PROMPT_
 * FILENAME on row 3, then a cwin_textinput field pre-filled with
 * app.filename (persisting the last-used name as the next prompt's
 * default, matching V1's UX).
 *
 * @param title Title line (the calling action's label).
 * @return 1 if ENTER accepted (app.filename updated), 0 if ESC cancelled
 *         (app.filename unchanged).
 */
uint8_t fileio_get_filename(const char *title)
{
    OricCharWin win;
    uint8_t result;

    menu_winsave(8, 7, 1);
    cwin_init(&win, 5, 8, 30, 7, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);

    cwin_putat_string(&win, 2, 1, title);
    cwin_putat_string(&win, 2, 3, MSG_FILE_PROMPT_FILENAME);

    result = (cwin_textinput(&win, 2, 4, 24, app.filename, FILENAME_MAXLEN, VINPUT_ALPHA) >= 0) ? 1 : 0;

    menu_winrestore();
    return result;
}
