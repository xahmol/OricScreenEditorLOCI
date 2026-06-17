#ifndef FILEIO_H
#define FILEIO_H

#include <stdint.h>

// Check whether a LOCI device is present (include/loci.h's loci_present()).
// Shows MSG_LOCI_NOT_FOUND via menu_messagepopup() if not.
// Returns 1 if present, 0 if absent (caller should abort the action).
uint8_t loci_check_present(void);

// Prompt for a LOCI base filename (popup, cwin_textinput, VINPUT_ALPHA),
// pre-filled with app.filename (persisted across calls) and a title line.
// Returns 1 if ENTER accepted (app.filename updated), 0 if ESC cancelled
// (app.filename unchanged).
uint8_t fileio_get_filename(const char *title);

#pragma compile("fileio.c")

#endif // FILEIO_H
