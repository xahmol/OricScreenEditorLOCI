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

// File > Save/Load Screen: app.filename+".BIN", a 6-byte header (magic,
// width, height) followed by screenmap[]. No charset data.
void fileio_save_screen(void);
void fileio_load_screen(void);

// File > Save/Load Combined: app.filename+".BIN", the same header followed
// by CHARSET_STD's displayable glyph range (768 bytes) then screenmap[].
void fileio_save_combined(void);
void fileio_load_combined(void);

// File > Save/Load Project: V1's 4-file scheme, all sharing app.filename --
// "<name>PJ.BIN" (metadata), "<name>SC.BIN" (screen, same shape as
// fileio_save_screen()), "<name>CS.BIN"/"<name>CA.BIN" (768-byte raw
// charset dumps, written/read only if app.stdchanged/altchanged).
void fileio_save_project(void);
void fileio_load_project(void);

// Charset menu Load/Save Standard/Alternate/Combined: altorstd 0=std,
// 1=alt, 2=combined (matches V1's stdoralt exactly). Std/Alt read/write
// 768 raw bytes directly from/to CHARSET_STD/CHARSET_ALT's displayable
// range. Combined save is identical to Save Std (CHARSET_STD's range is
// the only source); combined load writes the loaded data into *both*
// CHARSET_STD and CHARSET_ALT (charset_load(), include/charset.h) -- the
// closest available equivalent to V1's intent, since the ROM call V1 used
// to regenerate Alt from Std (jsr $F816) is a no-op on this runtime (see
// CLAUDE.md "Charset-swap mechanism").
void fileio_load_charset(uint8_t altorstd);
void fileio_save_charset(uint8_t altorstd);

#pragma compile("fileio.c")

#endif // FILEIO_H
