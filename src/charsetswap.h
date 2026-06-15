#ifndef CHARSETSWAP_H
#define CHARSETSWAP_H

#include <stdint.h>
#include "charset.h"

// Charset-swap mechanism for popup chrome (menu bar, pulldowns, popups).
//
// Popups must render with the ROM-standard charset even after the user has
// redefined glyphs via the character editor -- otherwise menu text becomes
// unreadable. charsetswap_enter()/exit() back up and restore the
// user-redefined standard charset around a popup's lifetime; the character
// editor itself opts out (see menu_winsave()'s swap_charset parameter in
// menu.h) so live glyph edits stay visible while editing.
//
// Std-only: a Phase-3a-style spike confirmed `jsr $F816` (ROM_ALTCHARS) is a
// no-op for CHARSET_ALT in this bare-metal context (it needs ROM-reset-time
// state we don't replicate), so only CHARSET_STD is swapped. CHARSET_ALT
// keeps user content during popups.
//
// charset_changed gates all work: if the user never edited a glyph, enter/
// exit are no-ops (matches V1's charsetchanged[0] flag).
//
// Depth-counted: nested popups (e.g. pulldown -> areyousure) only back up/
// restore once, on the outermost opt-in enter/exit pair.

// Call once when the user edits a glyph (from charsetedit's ce_snapshot()).
void charsetswap_mark_changed(void);

// Opt-in: on the outermost call (if a glyph was ever edited), back up
// CHARSET_STD's displayable range and restore it from CHARSETROM.
void charsetswap_enter(void);

// Opt-in: on the outermost call (if a glyph was ever edited), restore
// CHARSET_STD's displayable range from the backup made by charsetswap_enter().
void charsetswap_exit(void);

#pragma compile("charsetswap.c")

#endif // CHARSETSWAP_H
