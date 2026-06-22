#ifndef FILEPICKER_H
#define FILEPICKER_H

#include <stdint.h>

// Type filters for filepicker_run() -- which files are shown, beyond
// directories (always shown so navigation works regardless of filter).
// PICKER_FILTER_PLAIN is unused by any filepicker_run() call site as of
// 2026-06-22 (kept only for filepicker_browse_dir()'s context-listing
// use, see below) -- file-type filtering by suffix is now reserved for
// Load Project alone (user-requested: Screen/Combined/Charset Load
// should be able to pick *any* file, including ones not following
// OSE's own naming convention, e.g. a raw binary dump saved by another
// tool).
#define PICKER_FILTER_PLAIN   0  // *.BIN, excluding PJ/SC/CS/CA.BIN
#define PICKER_FILTER_PROJECT 1  // *PJ.BIN only
#define PICKER_FILTER_NONE    2  // every file, no suffix filtering at all

// Browse the LOCI device for a file matching `filter`, starting at
// app.filedir (defaults to app.homedir, or "/", the first time it's
// used this session -- src/homedir.c's filedir_init_default()). Popup
// with cursor-key navigation: UP/DOWN move, LEFT ascends to the parent
// (floored at a drive's own root), RIGHT/ENTER descend into a
// directory, ENTER also selects a file, 't'/'b' jump to top/bottom,
// 'd'/'p' page down/up, '\' jumps to the current drive's root, '.'/','
// switch drives (cycling 0-9), 'e' creates a new subdirectory, FUNCT+6
// toggles the statusbar, ESC cancels (see src/filepicker.c for the
// full implementation). On selection, sets app.filename to the
// resolved base name (subdirectory-prefixed; the matched suffix is
// stripped for PICKER_FILTER_PROJECT, but PICKER_FILTER_NONE stores the
// selected filename verbatim, extension included -- see fileio.c's
// filedir_join() vs. filedir_join_suffix() call sites) and app.filedir
// to the directory it was found in, then returns 1. Returns 0 if
// cancelled (ESC) or no matching files exist.
//
// @param title  Title row text (the calling action's MSG_FILE_*/
//               MSG_CHARSET_* label).
// @param filter PICKER_FILTER_PROJECT (Load Project only) or
//               PICKER_FILTER_NONE (every other Load action).
uint8_t filepicker_run(const char *title, uint8_t filter);

// Same browser/navigation as filepicker_run(), but for choosing a
// directory to save into (added 2026-06-21) instead of selecting a
// file: 's' confirms the currently-browsed directory (app.filedir set,
// app.filename untouched) and returns 1; ENTER never selects a file in
// this mode (files are still shown, for context, but only directories
// can be entered). Returns 0 if cancelled (ESC) or the starting
// directory has no matching entries.
//
// @param title Title row text (typically MSG_FILE_PICKER_SAVEDIR).
uint8_t filepicker_browse_dir(const char *title);

#pragma compile("filepicker.c")

#endif // FILEPICKER_H
