#ifndef FILEPICKER_H
#define FILEPICKER_H

#include <stdint.h>

// Type filters for filepicker_run()/filepicker_run_save() -- which files
// are shown, beyond directories (always shown so navigation works
// regardless of filter). File-type filtering by suffix is reserved for
// the Project family alone (user-requested: every other Load/Save
// action should be able to pick *any* file, including ones not
// following OSE's own naming convention, e.g. a raw binary dump saved
// by another tool). PICKER_FILTER_PLAIN (the old *.BIN-excluding-
// project-suffixes filter, used only by the now-deleted
// filepicker_browse_dir()) was removed 2026-06-22 alongside that
// function -- nothing passes it any more.
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

// Same browser/navigation as filepicker_run(), but for picking a Save
// target instead of a file to load (2026-06-22 redesign, replacing the
// old filepicker_browse_dir()-then-separate-typed-name-popup flow,
// which left no way to actually finish other than a 's' key with no
// on-screen hint, and never asked before silently overwriting). A
// synthetic "<new file>" entry is always pinned above the real listing
// in every directory: ENTER on it returns 2 (caller must still prompt
// for a typed name -- see fileio_get_filename()); ENTER on a real file
// shows an overwrite-confirm popup (menu_areyousure(), see
// MSG_FILE_OVERWRITE_Q) and, on Yes, returns 1 with app.filename/
// app.filedir already set (suffix-stripped the same way Load does);
// ENTER/RIGHT on a directory still just descends, unchanged. UP/DOWN
// move between the sentinel and the real list seamlessly. If
// app.filename (+ this action's suffix) matches an entry already
// present in the starting directory, the cursor defaults there instead
// of on the sentinel -- "save back to the file I just opened" needs no
// extra navigation.
//
// @param title  Title row text (the calling action's MSG_FILE_*/
//               MSG_CHARSET_* label).
// @param filter PICKER_FILTER_PROJECT (Save Project only) or
//               PICKER_FILTER_NONE (every other Save action).
// @return 0 if cancelled (ESC); 1 if an existing file was picked and
//         overwrite-confirmed (app.filename/app.filedir set, ready to
//         write); 2 if "<new file>" was picked (app.filedir set,
//         app.filename still needs a typed name from the caller).
uint8_t filepicker_run_save(const char *title, uint8_t filter);

#pragma compile("filepicker.c")

#endif // FILEPICKER_H
