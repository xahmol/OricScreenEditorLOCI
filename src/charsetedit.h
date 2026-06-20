#ifndef CHARSETEDIT_H
#define CHARSETEDIT_H

#include <stdint.h>
#include "charset.h"

// Character editor grid: 6 columns x 8 rows (Oric chars are 6x8 pixels,
// stored as 8 bytes/glyph; bits 5..0 of each byte = pixels left-to-right,
// bit5 = leftmost of the 6 visible columns). Aliases for charset.h's generic
// glyph-dimension constants.
#define CHAREDIT_GRID_W  CHARSET_GLYPH_WIDTH
#define CHAREDIT_GRID_H  CHARSET_GLYPH_BYTES

// Number of editable screencodes per charset (codes start at 0x20).
#define CHAREDIT_STD_COUNT  96   // codes 0x20-0x7F
#define CHAREDIT_ALT_COUNT  80   // codes 0x20-0x6F

// Run the character editor (entered via 'e' from main mode). Edits the 6x8
// glyph for app.plotscreencode/app.plotaltchar directly in live charset RAM
// (Strategy A -- see CLAUDE.md "Character editor"); ESC commits the
// current screencode/charset back to app.plotscreencode/app.plotaltchar.
void charsetedit_run(void);

#pragma compile("charsetedit.c")

#endif // CHARSETEDIT_H
