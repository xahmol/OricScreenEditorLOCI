#ifndef CHARSETEDIT_H
#define CHARSETEDIT_H

#include <stdint.h>

// Character editor grid: 6 columns x 8 rows (Oric chars are 6x8 pixels,
// stored as 8 bytes/glyph; bits 5..0 of each byte = pixels left-to-right,
// bit5 = leftmost of the 6 visible columns).
#define CHAREDIT_GRID_W  6
#define CHAREDIT_GRID_H  8

// Number of editable screencodes per charset (codes start at 0x20).
#define CHAREDIT_STD_COUNT  96   // codes 0x20-0x7F
#define CHAREDIT_ALT_COUNT  80   // codes 0x20-0x6F

// Compute the charset-RAM address of screencode's 8-byte glyph data
// (address = base + screencode*8, codes 0x00-0x7F; see oric.h).
// altorstd: 0 = standard charset (CHARSET_STD), 1 = alternate (CHARSET_ALT).
uint16_t charset_address(uint8_t screencode, uint8_t altorstd);

// Run the character editor (entered via 'e' from main mode). Edits the 6x8
// glyph for app.plotscreencode/app.plotaltchar directly in live charset RAM
// (Strategy A -- see CLAUDE.md "Character editor (Phase 3)"); ESC commits the
// current screencode/charset back to app.plotscreencode/app.plotaltchar.
void charsetedit_run(void);

#pragma compile("charsetedit.c")

#endif // CHARSETEDIT_H
