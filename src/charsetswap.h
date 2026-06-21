#ifndef CHARSETSWAP_H
#define CHARSETSWAP_H

#include <stdint.h>
#include "charset.h"

// Charset-swap mechanism for popup chrome (menu bar, pulldowns, popups).
//
// Popups must render with safe charset content even after the user has
// redefined glyphs (via the character editor) or loaded a project/charset
// file with different glyphs -- otherwise menu/dialog text becomes
// unreadable. charsetswap_enter()/exit() back up and restore BOTH
// CHARSET_STD and CHARSET_ALT around a popup's lifetime; the character
// editor itself opts out (see menu_winsave()'s swap_charset parameter in
// menu.h) so live glyph edits stay visible while editing. Merged into one
// pair 2026-06-21 (was Std-only + a separate one-off Alt pair used by
// exactly one popup) after a real-hardware report showed *any* popup that
// can trigger a Load action needs both banks protected, not just
// Information > Version's idi8b_logo.
//
// CHARSET_STD restores from CHARSETROM (the genuine ROM-standard ASCII
// font -- unambiguous, no real/boot-time-content distinction the way Alt
// has, see below). CHARSET_ALT restores from a one-time boot snapshot
// (charsetswap_capture_boot_alt(), called once at the very start of
// main()) rather than CHARSETROM -- CHARSETROM was tried first and found
// to NOT match CHARSET_ALT's real boot-time content on real hardware
// (only ever verified via Phosphoric before that), breaking the one
// popup that visibly depends on Alt's content.
//
// charset_changed gates the CHARSET_STD half of the work: if the user
// never edited/loaded a Std glyph, enter()/exit() skip backing it up and
// restoring it (matches V1's charsetchanged[0] flag) -- popups on an
// unmodified Std charset render identically with or without the swap.
// The CHARSET_ALT half always runs unconditionally regardless (cheap, no
// harm even when Alt was never touched).
//
// Depth-counted: nested popups (e.g. pulldown -> areyousure) only back up/
// restore once, on the outermost opt-in enter/exit pair.
//
// Mid-popup-load handling: charsetswap_mark_changed() (called by every
// LOCI Load action that overwrites CHARSET_STD/ALT directly, not just the
// character editor) also re-syncs an *already active* swap session if one
// exists -- see its own doc comment in charsetswap.c for why this matters
// (src/menudata.c's menu_run() keeps one swap session active across its
// entire bar-level lifetime, so a File > Load Project action executed
// from within it would otherwise corrupt the popup's chrome and discard
// the freshly-loaded charset on exit without this).

// Call whenever CHARSET_STD or CHARSET_ALT is overwritten by something
// other than charsetswap_enter()/exit() themselves (the character
// editor's ce_snapshot(), or any LOCI Load action in src/fileio.c/
// src/menudata.c). Safe to call even when no swap is currently active.
void charsetswap_mark_changed(void);

// Opt-in: back up CHARSET_STD (if charset_changed) and CHARSET_ALT
// (always) on the outermost call, and load the safe content into both.
void charsetswap_enter(void);

// Opt-in counterpart to charsetswap_enter(): restores CHARSET_STD (if a
// backup was taken) and CHARSET_ALT from their respective backups on the
// outermost call.
void charsetswap_exit(void);

// Call once, as early as possible in main() -- before anything else in the
// session can write to CHARSET_ALT -- to capture its genuine boot-time
// content. See charsetswap.c's doc comment for why CHARSETROM doesn't
// work as a stand-in for this.
void charsetswap_capture_boot_alt(void);

// Restore CHARSET_ALT from the boot-time snapshot above, not CHARSETROM
// (see charsetswap.c's doc comment for why). Used by Charset > Reset
// Alt->ROM (src/menudata.c).
void charsetswap_reset_alt_from_boot(void);

#pragma compile("charsetswap.c")

#endif // CHARSETSWAP_H
