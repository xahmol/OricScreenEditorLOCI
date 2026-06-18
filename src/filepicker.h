#ifndef FILEPICKER_H
#define FILEPICKER_H

#include <stdint.h>

// Type filters for filepicker_run() -- which files are shown, beyond
// directories (always shown so navigation works regardless of filter).
#define PICKER_FILTER_PLAIN   0  // *.BIN, excluding PJ/SC/CS/CA.BIN
#define PICKER_FILTER_PROJECT 1  // *PJ.BIN only

// Browse the LOCI device for a file matching `filter`, starting at the
// root. Popup with cursor-key navigation (see src/filepicker.c for full
// key bindings). On selection, sets app.filename to the resolved base
// name (subdirectory-prefixed, matched suffix stripped) and returns 1.
// Returns 0 if cancelled (ESC) or no matching files exist.
//
// @param title  Title row text (the calling action's MSG_FILE_*/
//               MSG_CHARSET_* label).
// @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
uint8_t filepicker_run(const char *title, uint8_t filter);

#pragma compile("filepicker.c")

#endif // FILEPICKER_H
