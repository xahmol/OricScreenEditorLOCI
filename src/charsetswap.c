// charsetswap.c - Charset-swap mechanism for popup chrome
// (OricScreenEditorLOCI, Oric Atmos, Oscar64)
//
// See charsetswap.h for the design rationale (std-only, charset_changed gate,
// depth-counting). The actual charset-bank copy operations are generic
// primitives from include/charset.h.

#include "oric.h"
#include "charset.h"
#include "loci.h"
#include "charsetswap.h"

static uint8_t swap_depth;
static uint8_t charset_changed;
static uint8_t backup_std[CHARSET_GLYPH_AREA_SIZE];
static uint8_t backup_alt_current[CHARSET_ALT_GLYPH_AREA_SIZE];

/**
 * Record that the user has redefined at least one glyph. Until this is
 * called, charsetswap_enter()/exit() are no-ops -- popup chrome on an
 * unmodified charset renders identically with or without the swap. Called
 * once, from charsetedit.c's ce_snapshot().
 *
 * @return (none)
 */
void charsetswap_mark_changed(void)
{
    charset_changed = 1;
}

/**
 * Opt-in: back up CHARSET_STD's displayable glyph range and overwrite it
 * with the ROM-standard glyphs (CHARSETROM), so popup chrome renders with
 * normal characters even if the user has redefined them. Depth-counted --
 * only the outermost call (swap_depth==0) performs the backup/restore, so
 * nested popups share one backup buffer safely. No-op until
 * charsetswap_mark_changed() has been called at least once.
 *
 * CHARSETROM ($FC78) lives inside $C000-$FFFF, the same address range
 * screenmap[]/undo now occupy as overlay RAM (enabled for the whole
 * session, see src/main.c) -- briefly disable_overlay_ram() to expose
 * the real ROM for this read, then enable_overlay_ram() again
 * immediately after, so canvas/undo access keeps working everywhere else.
 *
 * @return (none)
 */
void charsetswap_enter(void)
{
    if (swap_depth == 0 && charset_changed)
    {
        charset_save(CHARSET_STD, backup_std);
        disable_overlay_ram();
        charset_load(CHARSET_STD, (const uint8_t *)CHARSETROM);
        enable_overlay_ram();
    }
    swap_depth++;
}

/**
 * Opt-in counterpart to charsetswap_enter(): decrements swap_depth, and
 * once it returns to 0 restores CHARSET_STD's displayable glyph range from
 * the backup taken by the matching charsetswap_enter() call.
 *
 * @return (none)
 */
void charsetswap_exit(void)
{
    swap_depth--;
    if (swap_depth == 0 && charset_changed)
    {
        charset_load(CHARSET_STD, backup_std);
    }
}

/**
 * Back up CHARSET_ALT's displayable range into backup_alt_current, then
 * overwrite it with the ROM-stock Alt glyphs (CHARSETROM -- a valid
 * source for Alt too, see charsetswap.h's header comment). Unconditional:
 * no charset_changed gate, no depth-counting -- this has exactly one call
 * site (src/info.c's idi8b_logo draw), never nested with itself.
 *
 * @return (none)
 */
void charsetswap_alt_enter(void)
{
    charset_save(CHARSET_ALT, backup_alt_current);
    disable_overlay_ram();
    charset_load(CHARSET_ALT, (const uint8_t *)CHARSETROM);
    enable_overlay_ram();
}

/**
 * Counterpart to charsetswap_alt_enter(): restores CHARSET_ALT's
 * displayable range from the backup taken there.
 *
 * @return (none)
 */
void charsetswap_alt_exit(void)
{
    charset_load(CHARSET_ALT, backup_alt_current);
}
