// charsetswap.c - Charset-swap mechanism for popup chrome
// (OricScreenEditorLOCI, Oric Atmos, Oscar64)
//
// See charsetswap.h for the design rationale (std-only, charset_changed gate,
// depth-counting). The actual charset-bank copy operations are generic
// primitives from include/charset.h.

#include "oric.h"
#include "charset.h"
#include "charsetswap.h"

static uint8_t swap_depth;
static uint8_t charset_changed;
static uint8_t backup_std[CHARSET_GLYPH_AREA_SIZE];

void charsetswap_mark_changed(void)
{
    charset_changed = 1;
}

void charsetswap_enter(void)
{
    if (swap_depth == 0 && charset_changed)
    {
        charset_save(CHARSET_STD, backup_std);
        charset_load(CHARSET_STD, (const uint8_t *)CHARSETROM);
    }
    swap_depth++;
}

void charsetswap_exit(void)
{
    swap_depth--;
    if (swap_depth == 0 && charset_changed)
    {
        charset_load(CHARSET_STD, backup_std);
    }
}
