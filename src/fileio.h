#ifndef FILEIO_H
#define FILEIO_H

#include <stdint.h>

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

// File > Save/Load Combined: app.filename+".BIN", no header.
// Layout mirrors the Oric memory map: CHARSET_STD displayable range
// (768 bytes, $B500-$B7FF), then the full CHARSET_ALT region
// ($B800-$BB7F = 896 bytes: 256-byte non-displayable prefix then
// 640-byte displayable range), then screenmap[] (40*height bytes).
// Canvas MUST be 40x28 (2784 bytes total) or 40x27 (2744 bytes total);
// Save rejects other dimensions, Load auto-detects height from file
// size via loci_lseek(SEEK_END) -- no width/height prompt needed.
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
// 1=alt, 2=combined. Std reads/writes 768 raw bytes from/to CHARSET_STD's
// displayable range ($B500). Alt reads/writes 640 bytes (CHARSET_ALT_
// GLYPH_AREA_SIZE) from/to CHARSET_ALT's displayable range ($B900).
// Combined mirrors File > Combined minus the screen: 768 bytes Std
// displayable + 256 bytes Alt non-displayable prefix ($B800) + 640 bytes
// Alt displayable ($B900) = 1664 bytes total. Save uses loci_write x3,
// Load uses loci_read x2.
void fileio_load_charset(uint8_t altorstd);
void fileio_save_charset(uint8_t altorstd);

#pragma compile("fileio.c")

#endif // FILEIO_H
