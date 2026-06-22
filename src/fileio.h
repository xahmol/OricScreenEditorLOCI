#ifndef FILEIO_H
#define FILEIO_H

#include <stdint.h>

// Check whether a LOCI device is present (include/loci.h's loci_present()).
// Shows MSG_LOCI_NOT_FOUND via menu_messagepopup() if not.
// Returns 1 if present, 0 if absent (caller should abort the action).
uint8_t loci_check_present(void);

// Pick a Save target via the unified filepicker_run_save() (existing
// file to overwrite, confirmed, or the "<new file>" entry, prompting
// for a typed name). filter is PICKER_FILTER_PROJECT (Save Project) or
// PICKER_FILTER_NONE (every other Save action). Returns 1 if a target
// is ready (app.filename/app.filedir set), 0 if cancelled at any step.
uint8_t fileio_get_filename(const char *title, uint8_t filter);

// File > Save/Load Screen: app.filename+".BIN", a bare raw dump of
// screenmap[] -- no header, no metadata, by design (matches V1 exactly):
// a saved screen is meant to be a portable, tool-agnostic raw dump,
// loadable/embeddable from any source, not just OSE's own saves. Load
// prompts for width then height (V1's exact "Enter screen width:"/
// "Enter screen height:" wording, pre-filled with the current canvas
// size) since there is no embedded size to auto-detect -- this is the
// accepted cost of that portability, not a gap.
void fileio_save_screen(void);
void fileio_load_screen(void);

// File > Save/Load Combined: app.filename+".BIN", CHARSET_STD's
// displayable glyph range (768 bytes) immediately followed by
// screenmap[] -- no header, same bare-format rationale as Screen above
// (and the same width/height prompt on load).
void fileio_save_combined(void);
void fileio_load_combined(void);

// File > Save/Load Project: V1's 4-file scheme, all sharing app.filename --
// "<name>PJ.BIN" (metadata, including canvas size -- so, unlike standalone
// Screen/Combined, Project's "<name>SC.BIN" needs no width/height prompt
// of its own, exactly like V1's loadproject()), "<name>SC.BIN" (bare
// screen dump, same shape as fileio_save_screen() minus any prompt),
// "<name>CS.BIN"/"<name>CA.BIN" (raw charset dumps -- 768 bytes for Std,
// 640 for Alt, see CHARSET_ALT_GLYPH_AREA_SIZE -- written/read only if
// app.stdchanged/altchanged). Load Project also transparently accepts
// V1's original 19-byte "<name>PJ.BIN" layout (no magic number in V1's
// own format, but its first 2 bytes can never collide with
// FILEIO_MAGIC) -- see fileio_parse_v1_project() in fileio.c.
void fileio_save_project(void);
void fileio_load_project(void);

// Charset menu Load/Save Standard/Alternate/Combined: altorstd 0=std,
// 1=alt, 2=combined (matches V1's stdoralt exactly). Std reads/writes 768
// raw bytes, Alt 640 (charset_area_size(), include/charset.h --
// CHARSET_ALT only has 640 bytes of safely-addressable RAM) directly
// from/to CHARSET_STD/CHARSET_ALT's displayable range. Combined save is
// identical to Save Std (CHARSET_STD's range is the only source);
// combined load writes the loaded data into *both* CHARSET_STD and
// CHARSET_ALT (charset_load(), include/charset.h) -- the closest
// available equivalent to V1's intent, since the ROM call V1 used to
// regenerate Alt from Std (jsr $F816) is a no-op on this runtime (see
// CLAUDE.md "Charset-swap mechanism").
void fileio_load_charset(uint8_t altorstd);
void fileio_save_charset(uint8_t altorstd);

#pragma compile("fileio.c")

#endif // FILEIO_H
