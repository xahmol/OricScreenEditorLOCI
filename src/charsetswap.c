// charsetswap.c - Charset-swap mechanism for popup chrome
// (OricScreenEditorLOCI, Oric Atmos, Oscar64)
//
// See charsetswap.h for the design rationale (gate, depth-counting,
// Std+Alt merged). The actual charset-bank copy operations are generic
// primitives from include/charset.h.

#include "oric.h"
#include "charset.h"
#include "loci.h"
#include "charsetswap.h"

static uint8_t swap_depth;
static uint8_t charset_changed;
static uint8_t std_swap_active;
static uint8_t backup_std[CHARSET_GLYPH_AREA_SIZE];
static uint8_t backup_alt_boot[CHARSET_ALT_GLYPH_AREA_SIZE];
static uint8_t backup_alt_current[CHARSET_ALT_GLYPH_AREA_SIZE];

/**
 * Record that CHARSET_STD or CHARSET_ALT has been overwritten by
 * something other than charsetswap_enter()/exit() themselves (the
 * character editor's ce_snapshot(), or any LOCI Load action in
 * src/fileio.c/src/menudata.c that writes directly into either bank).
 *
 * Until this has been called at least once, charsetswap_enter()/exit()
 * skip the CHARSET_STD half of their work entirely -- popup chrome on an
 * unmodified charset renders identically with or without the swap (the
 * CHARSET_ALT half always runs regardless, see charsetswap_enter()).
 *
 * Found 2026-06-21 (real hardware, user report): if a Load action
 * overwrites CHARSET_STD/ALT *while* a charsetswap_enter()/exit() pair is
 * already active (e.g. src/menudata.c's menu_run() keeps one active for
 * its entire bar-level session, across every pulldown action including
 * File > Load Project) -- two things broke: the popup's still-visible
 * chrome immediately started rendering with the newly-loaded (and
 * possibly unreadable) glyphs instead of the safe ones it swapped in, and
 * the eventual exit() restored the *stale* pre-load backup, discarding
 * the freshly-loaded charset entirely. Fixed: when a swap is currently
 * active (swap_depth > 0), this function now also re-captures the
 * just-overwritten live content as the new "restore on exit" target,
 * then re-applies the safe chrome glyphs so the popup's remaining
 * lifetime stays readable. Safe to call unconditionally even when no
 * swap is active (the character editor's own call site always has
 * swap_depth == 0, since it opts out via menu_winsave(..., 0)).
 *
 * @return (none)
 */
void charsetswap_mark_changed(void)
{
    charset_changed = 1;

    if (swap_depth > 0)
    {
        charset_save(CHARSET_STD, backup_std);
        std_swap_active = 1;
        disable_overlay_ram();
        charset_load(CHARSET_STD, (const uint8_t *)CHARSETROM);
        enable_overlay_ram();

        charset_save(CHARSET_ALT, backup_alt_current);
        charset_load(CHARSET_ALT, backup_alt_boot);
    }
}

/**
 * Snapshot CHARSET_ALT's current content into backup_alt_boot. Must be
 * called once, as early as possible in main() -- before anything else in
 * the session (the character editor, Charset Load Combined, etc.) can
 * write to CHARSET_ALT -- so this captures the genuine boot/RESET state,
 * not a copy of CHARSET_STD.
 *
 * Found 2026-06-21, real hardware: an earlier version of this mechanism
 * restored CHARSET_ALT from CHARSETROM on the (disproven) assumption
 * that CHARSETROM holds byte-identical content to CHARSET_ALT's
 * boot-time state. It doesn't -- restoring from CHARSETROM made
 * Information > Version's idi8b_logo[] render with plain
 * ROM-standard-font shapes (readable digit/letter outlines) instead of
 * its intended colourful mosaic blocks, on real hardware (only ever
 * verified via Phosphoric before that). Capturing the *real* boot-time
 * content directly, instead of trusting CHARSETROM as a stand-in for it,
 * is the fix.
 *
 * @return (none)
 */
void charsetswap_capture_boot_alt(void)
{
    charset_save(CHARSET_ALT, backup_alt_boot);
}

/**
 * Restore CHARSET_ALT's displayable range from the boot-time snapshot
 * charsetswap_capture_boot_alt() took. Used by Charset > Reset Alt->ROM
 * (`src/menudata.c`) -- deliberately does NOT source from CHARSETROM,
 * for the same reason charsetswap_alt_enter() doesn't (see this file's
 * other doc comments): CHARSETROM does not reproduce a useful "mosaic"
 * look for Alt on real hardware, only the genuine boot-time content does.
 *
 * @return (none)
 */
void charsetswap_reset_alt_from_boot(void)
{
    charset_load(CHARSET_ALT, backup_alt_boot);
}

/**
 * Return a pointer to CHARSET_STD's genuine, un-swapped displayable range
 * (CHARSET_GLYPH_AREA_SIZE bytes). If a swap session currently has Std
 * swapped to chrome glyphs (std_swap_active), the real content is sitting
 * in backup_std, not live CHARSET_STD -- point there instead. Found
 * 2026-06-23 (user report: saving a project's charset, then loading it
 * back, didn't actually restore the edited glyphs): every fileio.c Save
 * action that touched CHARSET_STD read live charset RAM directly, which
 * silently saved the swapped-in ROM chrome glyphs instead of the user's
 * real edits whenever the save was triggered from inside an active
 * charsetswap_enter() session -- which is *always* the case for any
 * menu-invoked Save, since src/menudata.c's menu_run() keeps one swap
 * session open for its entire bar-level lifetime. Returns a pointer
 * rather than copying into a caller buffer to avoid a 768-byte static
 * buffer that didn't fit the FR build's smaller remaining BSS budget.
 *
 * @return Pointer to CHARSET_GLYPH_AREA_SIZE bytes of real Std content.
 */
const uint8_t *charsetswap_real_std(void)
{
    if (std_swap_active)
        return backup_std;
    return (const uint8_t *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET);
}

/**
 * Return a pointer to CHARSET_ALT's genuine, un-swapped displayable range
 * (CHARSET_ALT_GLYPH_AREA_SIZE bytes). Unlike Std, Alt is swapped
 * unconditionally on every charsetswap_enter() (regardless of
 * charset_changed), so the real content lives in backup_alt_current
 * whenever swap_depth > 0 -- see charsetswap_real_std()'s doc comment for
 * the bug this fixes.
 *
 * @return Pointer to CHARSET_ALT_GLYPH_AREA_SIZE bytes of real Alt content.
 */
const uint8_t *charsetswap_real_alt(void)
{
    if (swap_depth > 0)
        return backup_alt_current;
    return (const uint8_t *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET);
}

/**
 * Opt-in: back up CHARSET_STD's displayable glyph range and overwrite it
 * with the ROM-standard glyphs (CHARSETROM) if it's ever been changed
 * (see charsetswap_mark_changed()), and unconditionally back up
 * CHARSET_ALT and overwrite it with the genuine boot-time content
 * (charsetswap_capture_boot_alt()) -- every popup needs CHARSET_ALT safe
 * too, not just Information > Version (found 2026-06-21: any popup that
 * can trigger a Load action touching CHARSET_ALT needs the same
 * protection, not a one-off special case). Depth-counted -- only the
 * outermost call (swap_depth==0) performs the backup/restore, so nested
 * popups share one pair of backup buffers safely.
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
    if (swap_depth == 0)
    {
        if (charset_changed)
        {
            charset_save(CHARSET_STD, backup_std);
            std_swap_active = 1;
            disable_overlay_ram();
            charset_load(CHARSET_STD, (const uint8_t *)CHARSETROM);
            enable_overlay_ram();
        }
        else
        {
            std_swap_active = 0;
        }

        charset_save(CHARSET_ALT, backup_alt_current);
        charset_load(CHARSET_ALT, backup_alt_boot);
    }
    swap_depth++;
}

/**
 * Opt-in counterpart to charsetswap_enter(): decrements swap_depth, and
 * once it returns to 0 restores CHARSET_STD's displayable glyph range
 * (only if std_swap_active -- it may have been set after entry, by
 * charsetswap_mark_changed(), even if charset_changed was still false at
 * the matching charsetswap_enter() call) and CHARSET_ALT's, both from
 * their respective backups.
 *
 * @return (none)
 */
void charsetswap_exit(void)
{
    swap_depth--;
    if (swap_depth == 0)
    {
        if (std_swap_active)
        {
            charset_load(CHARSET_STD, backup_std);
            std_swap_active = 0;
        }
        charset_load(CHARSET_ALT, backup_alt_current);
    }
}
