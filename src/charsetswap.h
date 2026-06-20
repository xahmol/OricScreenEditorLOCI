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
// Std-only for the general popup-chrome mechanism below: a spike confirmed
// `jsr $F816` (ROM_ALTCHARS) is a no-op for CHARSET_ALT in this bare-metal
// context (it needs ROM-reset-time state we don't replicate). General
// popup chrome only ever uses CHARSET_STD glyphs, so CHARSET_ALT keeps
// user content during ordinary popups -- but see charsetswap_alt_enter()/
// exit() below for the one popup (Information > Version's idi8b_logo)
// that genuinely needs CHARSET_ALT's *stock* content too.
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

// CHARSET_ALT equivalent of charsetswap_enter()/exit(), for the one popup
// that needs the *stock* (ROM-original) Alt charset rather than leaving
// the user's edited Alt content in place: Information > Version's
// idi8b_logo[] (src/info.c) embeds A_ALT attribute bytes, so it depends
// on CHARSET_ALT holding the ROM's mosaic-graphics bit patterns, not
// whatever the user may have redefined via the character editor's Alt
// mode. Unlike charsetswap_enter()/exit(), this is unconditional (no
// charset_changed gate, no depth-counting) -- it has exactly one call
// site, never nested with itself, and the always-restore-from-ROM
// behaviour is correct (cheap, no harm) even if Alt was never edited.
//
// CHARSETROM is a valid restore source for CHARSET_ALT too, not just
// CHARSET_STD: per the Oric ROM's own boot sequence (~/.claude/
// oric_atmos_reference.md, "Character Set ROM Data"), $FC78-$FF77 is
// copied to *both* $B400 (Std) and $B800 (Alt) at RESET -- the
// "alternate"/mosaic look comes entirely from the A_ALT attribute byte
// changing how the ULA *interprets* those bytes at render time, not from
// a separate ROM table for Alt. So charset_load(CHARSET_ALT, CHARSETROM)
// reproduces exactly the boot-time content, the same way
// charset_load(CHARSET_STD, CHARSETROM) already does for Std.
void charsetswap_alt_enter(void);
void charsetswap_alt_exit(void);

#pragma compile("charsetswap.c")

#endif // CHARSETSWAP_H
