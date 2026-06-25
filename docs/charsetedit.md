# Character & Charset Editing

## Character editor

`src/charsetedit.c/h` — entered via `e` from main mode. **Narrow sidebar popup**
(cols 26-39, rows 0-14) — canvas cols 0-25 stay visible and live-updating while
editing. `CE_WIN_SX=26`, `CE_WIN_WX=14`, `CE_WIN_WY=15`.

**Strategy A:** both `CHARSET_STD` ($B400) and `CHARSET_ALT` ($B800) are edited
**directly in live charset RAM** — no shadow buffers. The ULA re-renders from
charset RAM every frame, so on-canvas occurrences of the edited glyph update for
free. This is the reason `charsetedit_run()` opts OUT of the charset-swap
mechanism (`menu_winsave(..., 0)`).

**Popup layout** (window-relative, with `CE_CONTENT_X0=3` reserving cols 0-2):
- Col 0: `A_BGWHITE` (paper reset, written every row by `ce_clear()`)
- Col 1: ink attribute
- Col 2: `A_STD` (charset-mode reset — prevents Alt-charset content on the canvas
  from carrying into the popup's text)
- Col 3+: content (`CE_CONTENT_X0=3`)
- Row 0: `Code:$xx` + live-preview attr/char
- Row 1: `Set:Std`/`Set:Alt`
- Rows 3-4: favourite digit labels + screencodes
- **Rows 6-13: 6x8 pixel grid** — with hex value column immediately left of grid
- Row grid: `CE_HEX_VAL_X = CE_CONTENT_X0` (window-relative cols 2-3 within the
  grid rows, i.e. absolute cols 28-29), `CE_GRID_X` at relative col 5

**`ce_clear()` must be used** (not `cwin_clear()`) — it writes ink/paper at the
window's own first two relative columns (absolute 27/28) and `A_STD` at the
third, instead of literal screen columns 0/1. Without this, the Oric ULA's
left-to-right attribute model bleeds the popup's white paper across columns 0-26.

**Per-row hex column:** `ce_draw_grid()` draws `"%02x"` (lowercase — `%X` is
silently ignored by `cwin_putat_printf`'s parser) of each row's byte at
`(CE_HEX_VAL_X, CE_GRID_Y+y)` on every redraw.

**`h` key — inline hex edit:** reads the cursor row's current byte, formats it
into a 2-char buffer via a hex lookup table (no `<stdio.h>`), calls
`cwin_textinput()` at the row's own on-screen position with `vwidth=4` (must be
> `maxlen=2` to avoid viewport-scroll display bug).

**Address formula** (`charset_address()`, `include/charset.h`): `base + screencode*8`
where `base = CHARSET_STD`/`CHARSET_ALT` — **no** `-0x20` offset (full 128-glyph
banks, codes 0x00-0x7F). `CHARSETROM` ($FC78) uses `CHARSETROM + (screencode-0x20)*8`
(only covers 0x20-0x7F printable codes).

**Key bindings:** cursor keys move within grid; `+`/`-`/`=` cycle screencode
(std: 0x20-0x7F, alt: 0x20-0x6F via `ce_max_code()`); `SPACE` toggles pixel;
`DEL` clears glyph; `i` inverts (XOR 0x3F); `z` undoes (single-level `ce_undo[]`);
`s` restores from CHARSETROM (std only); `c`/`v` copy/paste glyph; `x`/`y` mirror
horizontal/vertical; `u`/`d`/`l`/`r` scroll glyph (wrapping); `h` hex-edit row;
`a` toggles Std/Alt; `0`-`9` recall favourites; `SHIFT+0-9` store to favourites;
`FUNCT+6` toggles statusbar; `FUNCT+8` shows help screen 2 (then redraws popup
chrome via `cwin_clear()` + `ce_draw_header/favourites/grid`); `ESC` commits
`ce_code`/`ce_altorstd` back to `app.plotscreencode`/`app.plotaltchar` and closes.

**4-state grid rendering:** pixel=0,cursor=0 → `CH_SPACE`; pixel=1,cursor=0 →
`'#'`; pixel=0,cursor=1 → `CH_INVSPACE`; pixel=1,cursor=1 → `0xA3`.

**`ce_snapshot()` is the chokepoint** for all destructive glyph edits — it calls
`charsetswap_mark_changed()` and sets `app.stdchanged`/`altchanged` based on
`ce_altorstd`.

**CHARSETROM read needs overlay-RAM bracket:** `charset_rom_glyph()` reads from
$FC78 which is inside $C000-$FFFF (overlay RAM region). The `'s'` key must call
`disable_overlay_ram()` before reading, `enable_overlay_ram()` after.

## Charset-swap mechanism

`src/charsetswap.c/h` ensures popup chrome always renders with safe glyphs in
**both** `CHARSET_STD` and `CHARSET_ALT`, even after the user redefined characters.

**Final design:**
- `charsetswap_enter()`: backs up both banks on the outermost call (`swap_depth==0`).
  Std: copies live CHARSET_STD content into `backup_std[]`, then overwrites live
  CHARSET_STD with CHARSETROM (safe ROM glyphs). Alt: copies live CHARSET_ALT
  into `backup_alt_current[]`, then restores from `backup_alt_boot[]`.
  Increments `swap_depth`.
- `charsetswap_exit()`: decrements `swap_depth`. On reaching 0, restores both banks
  from their backups (guided by `std_swap_active` flag for Std).
- `charsetswap_mark_changed()`: sets `charset_changed` + `std_swap_active`. If
  `swap_depth > 0` (a swap is currently active), immediately re-captures the
  just-overwritten live content as the new backup and re-applies safe chrome —
  so a Load-action that runs mid-session correctly updates the backup.
- `charsetswap_capture_boot_alt()`: called as **literally the first statement in
  `main()`** — captures `CHARSET_ALT` into `backup_alt_boot[]` before anything
  can modify it. This is the only correct source for Alt restoration (the Oric
  ROM generates the alternate/mosaic font algorithmically at boot via `jsr $F816`,
  it does NOT copy a static table — so `CHARSETROM` is not a valid source for Alt).
- Depth-counting: nested popups (e.g. Screen pulldown → resize dialog → Are-you-sure)
  share a single pair of backup buffers safely.

**`menu_winsave(ypos, height, swap_charset)`**: third param opts popup in/out.
`menu_winrestore()` pairs `charsetswap_exit()` automatically via the saved
`MenuWinRecord`. **Every popup passes `1` except `charsetedit_run()` (passes `0`).**

**`charsetswap_real_std()`/`charsetswap_real_alt()`**: return `const uint8_t *`
to the genuine current charset content, regardless of whether a swap is active.
Returns `backup_std`/`backup_alt_current` if a swap has that bank overwritten,
or the live bank address otherwise. **All Save actions must read through these**
(not direct bank addresses) — otherwise menu-invoked Saves write chrome glyphs
instead of the user's real charset.

**`charset_changed` gate (Std only):** until `charsetswap_mark_changed()` is
called, `charsetswap_enter()`/`exit()` skip the CHARSET_STD half — popups on
an unmodified Std charset render identically with or without the swap. The
CHARSET_ALT half always runs unconditionally.

**Call `charsetswap_mark_changed()` once per multi-bank action, not once per
bank.** `fileio_load_project()` previously called it twice (after Std, after Alt)
— the second call re-captured the already-swapped-in chrome content into
`backup_std`, clobbering the correct backup. Fixed to one call after both
loads complete.

**CHARSETROM read needs overlay-RAM bracket:** `charsetswap_enter()`'s
`charset_load(CHARSET_STD, CHARSETROM)` must bracket the read with
`disable_overlay_ram()` ... `enable_overlay_ram()` since $FC78 is inside the
overlay-RAM region.

**`charset_area_size(base)` (`include/charset.h`):** returns
`CHARSET_ALT_GLYPH_AREA_SIZE` (640) for `CHARSET_ALT`, 768 otherwise. Alt only
has 896 bytes of RAM before screen RAM begins at $BB80 (`$B800–$BB7F`), so its
displayable range is 640 bytes (codes 0x00-0x6F × 8 bytes = 640). The character
editor's `ce_max_code()` already caps Alt at 0x6F. All file I/O for Alt charset
must use 640, not 768.

## Palette mode

`src/palette.c/h` — entered via `p`. Popup `PAL_WIN_SX=2/SY=0/WX=38/WY=13`,
`menu_winsave(0, 13, 1)`. Selects `app.plotscreencode`/`app.plotaltchar`.

Layout (13 rows x 16 cols): row 0 = Fav row (10 favourites); rows 1-6 = Std charset
0x20-0x7F (96 glyphs, 16 per row); rows 7-12 = Alt charset 0x20-0x7F or
`visualchar[80]` reordering when `app.visualmap` is set.

`visualchar[80]`: ported verbatim from V1 (credited to jab/Artline Designs,
Jaakko Luoto). Covers screencodes 0x20-0x6F; row 0x70-0x7F is always identity.
`visualchar[]`'s reordering is designed around the Oric's native ROM-resident
alternate/mosaic charset, NOT a PETSCII asset.

Cursor highlighted via `^0x80`. Initial position derived from `app.plotscreencode`.
`pal_wrap()`'s post-check order re-derived for 13 rows (V1 used 12) to avoid
out-of-bounds `favourites[]` access when wrapping DOWN from the last Alt row.

Keys: cursor keys navigate with wrap; `SPACE`/`ENTER` set plotscreencode/plotaltchar
and close; `0`-`9` store to favourites; `v` toggles `app.visualmap`; `FUNCT+6`
toggles statusbar; `ESC` closes unchanged.

Charset-swap: opts IN (`menu_winsave(..., 1)`) — renders via A_STD/A_ALT
attribute bytes, no live-edit-preview requirement.

## Charset > Reset Std->ROM / Reset Alt->ROM

`charset_reset_std()` (`src/menudata.c`, Charset-pulldown choice 37): after
`menu_areyousure()` confirmation, calls `charset_load(CHARSET_STD, CHARSETROM)`
and sets `app.stdchanged = 1`. **Must bracket with `disable_overlay_ram()` ...
`enable_overlay_ram()`** — CHARSETROM ($FC78) is inside the overlay-RAM region.

`charset_reset_alt()` (`src/menudata.c`, choice 38): after confirmation, calls
`charsetswap_reset_alt_from_boot()` — restores from `backup_alt_boot[]`
(the boot-time snapshot). **NOT** `charset_load(CHARSET_ALT, CHARSETROM)` — the
CHARSETROM bytes are not valid for Alt (Alt is generated algorithmically, see
charset-swap mechanism above). No overlay-RAM bracket needed (boot snapshot is
in main RAM).

`PULLDOWN_MAXOPTIONS` is currently 8 to accommodate both reset items.

## Colour picker

`src/colourpicker.c/h` — entered via `c`. Popup `CP_WIN_SX=2/SY=0/WX=36/WY=12`,
`menu_winsave(0, 12, 1)`. Selects `app.plotink`/`app.plotpaper`.

**Layout:** 8 rows × 8 columns (one row per paper value, one column per ink value).
Each cell is 5 bytes: `[A_BGWHITE reset, ink-attr byte, paper-attr byte (16+paper),
normal swatch, inverse swatch]`. 8 cells × 5 bytes = 40 columns exactly — no
end-of-row reset needed.

**The grid is drawn via direct screen-RAM writes** (`cp_screen_put()`, same
technique as `canvas_blit()`) — NOT through the popup's `OricCharWin`. This
bypasses the window's reserved border columns so the ink=0 cell's leading
`A_BGWHITE` reset byte lands at literal screen column 0, matching V1's
`colorpicker.c` approach exactly.

`cp_draw_border()` overrides columns 0-1 for **every** row of the popup (not
just grid rows) to paper-then-ink order (V1's convention, which is opposite to
this codebase's usual `cwin_clear()` convention of ink-then-paper). Called right
after `cwin_clear()`.

Cell byte order: `[A_BGWHITE, ink, paper+16, swatch0, swatch1]`. The leading
`A_BGWHITE` reset is essential — without it, the previous row's paper colour
bleeds into the current cell.

**Swatch glyphs:** `CP_STAR`/`CP_INVSTAR` (`'*'`/`'*'|0x80`) for normal cells
(both ink foreground and paper background visible). **Highlighted cell** uses
`CP_DASH`/`CP_INVDASH` (`'-'`/`'-'|0x80`) instead — glyph change makes selection
visible, not just swap-order change.

Cursor initial position: `(app.plotink, app.plotpaper)`. Rows 9-11 show Ink/Paper/
Result feedback. Keys: LEFT/RIGHT cycle ink (wrap 0-7), UP/DOWN cycle paper (wrap
0-7); `SPACE`/`ENTER` commit; `FUNCT+6` toggles statusbar; `ESC` closes unchanged.

**Real-hardware confirmation still pending** for the round-3 grid rewrite — two
previous "confirmed via Phosphoric" rounds didn't hold up on real hardware. A
Phosphoric pass alone doesn't close this out.
