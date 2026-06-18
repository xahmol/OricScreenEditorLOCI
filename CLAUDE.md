# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OricScreenEditorLOCI is a from-scratch **Oscar64** rewrite of
[OricScreenEditor](https://github.com/xahmol/OricScreenEditor) (V1, CC65,
local checkout at `/home/xahmol/git/OricScreenEditor`) — a character-set-aware
screen editor for the Oric Atmos — extended with support for the **LOCI mass
storage device**.

**Goal / planned additions over V1:**
- File browsing and file I/O via LOCI (load/save screens, charsets, etc. from
  LOCI storage instead of tape only)
- Overlay RAM ($C000-$FFFF, LOCI-backed) for extra working memory (e.g. window
  save/restore, undo buffers)
- Enhanced palette mode that also shows the inverse colour combinations for
  each ink/paper pair

**Status: all 9 phases complete.** Charset-swap infrastructure, a narrow
charset-editor popup, palette mode, the colour picker, Select/Move/Line-Box/
Write modes, the `G`/`I`/`O`/`U`/`R` main-mode keys, a cursor-auto-scroll fix
for oversized canvases, LOCI file I/O (File and Charset menus) with a real
XRAM-backed directory browser for every Load action, canvas-edit undo/redo
(`z`/`y`) backed by Oric-side overlay RAM, Select mode cut/copy, the
main-mode attribute-selection keys, a redesigned statusbar, IJK joystick
input (universal `key_read()`, every mode), FUNCT+8 help screens (Main/
Character editor/Select-Move-Line-Box/Write), an Information menu (3-page
Version/credits + QR popup, Exit via the RESET vector), and EN/FR
localisation (canvas data model + minimal main mode + menu bar/Screen menu
+ character editor + charset-swap mechanism + palette/colour-picker popups
+ Select/Move/Line-Box/Write modes + LOCI file I/O + file picker + undo/
redo + Select cut/copy + IJK/help/Information menu + Phosphoric test
harness; binary 31139 bytes EN / 31230 bytes FR).
A previous CC65-based attempt at this got stuck and is archived
in the `nonworkingcc65` branch (full history + uncommitted state). `main` was
restarted from scratch on the Oscar64 native/bare-metal build chain developed
for [locifilemanager-v2](https://github.com/xahmol/locifilemanager-v2)
(`/home/xahmol/git/locifilemanager-v2`), which the runtime and libraries below
were copied from verbatim.

**Other docs**: `ARCHITECTURE.md` (memory map, module layout, dependency
graph), `libmanual.md`/`libmanual_fr.md` (per-function API reference for
`oric.h`/`keyboard.h`/`charwin.h`/`ijk.h`/`loci.h`/`charset.h`/`menu.h`),
`README.md`/`README.pdf`/`README_fr.md` (user-facing feature documentation,
V1-derived).

### Canvas architecture (Phase 1 decision)

The 40x27 **canvas** (rows 0-26) is a custom flat `screenmap[]` buffer
(`src/canvas.c/h`), blitted to `$BB80` via raw pointer writes covering all 40
columns. It deliberately does **not** use `OricCharWin`/`OricViewport`/
`cwin_viewport_*`/`cwin_get_rect/put_rect`: `cwin_init` clamps `sx >= 2`, and
`cwin_clear`/`row_setattr`/`cwin_viewport_blit` always overwrite columns 0-1 of
every managed row with the window's fixed ink/paper attribute bytes on every
redraw. That's correct for bordered popup/menu/statusbar chrome, but
incompatible with OSE's canvas model, where the user can place arbitrary
serial-attribute bytes (0x00-0x1F) at **any** of the 40 columns (including 0-1)
as part of the screen design — charwin would clobber those on every blit.

`OricCharWin`/`cwin_*` (incl. `cwin_push/pop`, `cwin_textinput`, popups) remain
the right tool for Phase 2+ UI chrome (menus/popups) and for the **statusbar**
(row 27, `src/statusbar.c/h`), which genuinely benefits from the bordered
2-attr-column model. Colours: `A_FWBLACK`/`A_BGWHITE` (black-on-white)
throughout — matches V1's statusbar/editor-field convention and the
popup/menu chrome (locifilemanager-v2's `cwin_init(..., A_FWBLACK,
A_BGWHITE)` everywhere).

**Feature-set reference:** `README.md` / `README.pdf` and `screenshots/`
describe V1's UI and feature set (main mode, character editor, palette mode,
select/move/line-box/write modes, file format) — this is the spec the rewrite
needs to (eventually) match and then extend. `assets/*.bin` (charset/title/
help screens, Petscii charset) are Oric-format binaries carried over from V1
and may be reusable as-is.

### Character editor (Phase 3)

`src/charsetedit.c/h` — entered via `e` from main mode, edits the 6x8-pixel
glyph for `app.plotscreencode`/`app.plotaltchar`.

- **Layout: narrow sidebar popup (cols 27-39, rows 0-14), matching V1's
  sidebar intent**. V1 draws the char editor as a fixed inline panel in
  screen columns 27-39 (rows 0-11), leaving the user's canvas visible to the
  left while editing. Phase 1 of this rewrite made the canvas a *resizable,
  full-width* `screenmap[]` (see "Canvas architecture" above), and Phase 2
  introduced a popup-window/menu system (`cwin_init`/`menu_winsave`/
  `menu_winrestore`). Phase 3 initially presented the editor as a wide
  (38x16) full-screen popup; this was revised so canvas cols 0-26 stay
  visible (and live-updating) on every row while the popup is open, matching
  V1. The character editor reuses V1's **key bindings/behaviour** (per the
  Phase 3 plan). Colours (`A_FWBLACK`/`A_BGWHITE`) match V1's editor-field
  convention.
- **Strategy A (confirmed by the Phase 3a spike, kept as a permanent
  regression in `tests/scripts/test_charsetram_spike.sh`)**: both
  `CHARSET_STD` ($B400) and `CHARSET_ALT` ($B800) banks are edited
  **directly in live charset RAM** — no shadow buffers, no copy-in/copy-out.
  The ULA re-renders from charset RAM every frame, so the popup's live
  preview cell (`ce_draw_header`, `CE_PREVIEW_ATTR_X`/`CE_PREVIEW_CHAR_X`)
  and any on-canvas occurrences of the edited glyph update for free as soon
  as `ce_draw_grid()` redraws.
- **Charset-swap opt-out**: `charsetedit_run()` calls
  `menu_winsave(CE_WIN_SY, CE_WIN_WY, 0)` — the final `0` opts OUT of the
  charset-swap mechanism (see "Charset-swap mechanism" below), so glyph edits
  to `CHARSET_STD` stay visible live in the popup's preview/grid and the
  canvas while editing. Every other popup opts IN (`1`).
- **Address formula** (`charset_address()`, now in `include/charset.c/h` — a
  generic, non-OSE-specific Oric charset-addressing helper, see "Charset-swap
  mechanism" below): `base + screencode*8` where `base` = `CHARSET_STD`/
  `CHARSET_ALT` — **no** `-0x20` offset (full 128-glyph banks, codes
  0x00-0x7F). `CHARSETROM` ($FC78, used by the `s` restore-from-ROM command,
  std charset only, via `charset_rom_glyph()`) uses the **opposite**
  convention: `CHARSETROM + (screencode-0x20)*8`, since the ROM table only
  covers the 96 printable codes 0x20-0x7F. Both conventions are documented in
  `include/oric.h` and `include/charset.h`.
- **Popup layout** (13x15, screen cols 27-39 rows 0-14,
  `CE_WIN_SX=27/CE_WIN_SY=0/CE_WIN_WX=13/CE_WIN_WY=15`): row 0 = `Code:$xx` +
  live-preview attr/char (`CE_PREVIEW_ATTR_X`/`CE_PREVIEW_CHAR_X`); row 1 =
  `Set:Std`/`Set:Alt` (`CE_SET_Y`); rows 3-4 = favourite digit labels
  '0'-'9' (`CE_FAV_Y`) over `app.favourites[10]` screencodes
  (`CE_FAV_VALUE_Y`), both starting at `CE_FAV_X`; rows 6-13 = the 6x8 pixel
  grid (`CE_GRID_X`/`CE_GRID_Y`, one screen cell per pixel); row 14 = inline
  hex-row input (`CE_HEX_LABEL_X`/`CE_HEX_INPUT_X`, via `cwin_textinput`).
  The 4 lines of key-binding hints from the earlier 38x16 popup were dropped
  (no room in 13 cols) — key bindings move to the Phase 9 help system
  (`FUNCT+8`).
- **Pixel/cursor 4-state grid rendering** (`ce_draw_grid`): pixel=0,cursor=0
  -> `CH_SPACE` (0x20); pixel=1,cursor=0 -> `CE_PIXEL_CHAR` `'#'` (0x23);
  pixel=0,cursor=1 -> `CH_INVSPACE` (0xA0, = `CH_SPACE ^ 0x80`); pixel=1,
  cursor=1 -> `0xA3` (= `'#' ^ 0x80`). All 4 are visually distinct, resolving
  the ambiguity of naively XORing 0x80 onto `CH_SPACE`/`CH_INVSPACE` pairs.
- **Key bindings** (all implemented; see `README.md` "Character editor" for
  the full user-facing description): cursor keys move within the grid;
  `+`/`-`/`=` cycle the screencode (wrapping at the charset's valid range,
  std 0x20-0x7F / alt 0x20-0x6F via `ce_max_code()`); `0-9` recall
  `app.favourites[n]`, `SHIFT+0-9` store the current code into it; `SPACE`
  toggles the pixel under the cursor; `DEL` clears the glyph; `i` inverts
  (XOR 0x3F); `z` undoes the last destructive edit (single-level snapshot,
  `ce_undo[]`); `s` restores from `CHARSETROM` (std only); `c`/`v`
  copy/paste a glyph (`ce_copy[]`); `x`/`y` mirror horizontally/vertically;
  `u`/`d`/`l`/`r` scroll the glyph up/down/left/right (wrapping); `h` opens a
  2-digit hex-row input for the cursor's row; `a` toggles Std/Alt; `FUNCT+6`
  toggles the statusbar; `ESC` commits `ce_code`/`ce_altorstd` back to
  `app.plotscreencode`/`app.plotaltchar` and closes the popup
  (`menu_winsave`/`menu_winrestore`, main-RAM window stack — no residue).
- **Favourites default**: `editor_run()` initialises all 10 slots to `'!'`
  (0x21); `FAVOURITES_COUNT=10` in `src/appstate.h`.

### Main-mode attribute-selection keys and statusbar redesign (Phase 4a)

`src/appstate.h` gained five `AppState` fields: `plotink`/`plotpaper` (0-7,
default `A_FWWHITE`/`A_FWBLACK`), `plotblink`/`plotdouble` (0/1 toggles), and
`visualmap` (0/1, palette Alt-section toggle — see "Palette mode" below).
`editor_run()`'s main key switch (`src/editor.c`) gained, matching V1
`main.c`'s attribute-selection key table:

- `,`/`.`: decrease/increase `plotink` (wrap 0-7)
- `;`/`'`: decrease/increase `plotpaper` (wrap 0-7)
- `b`/`d`/`a`: toggle `plotblink`/`plotdouble`/`plotaltchar`
- `0`-`9`: `plotscreencode = favourites[n]`
- `)`/`!`/`@`/`#`/`$`/`%`/`^`/`&`/`*`/`(` (SHIFT+0-9): `favourites[n] =
  plotscreencode` — same key-code mapping as `charsetedit.c`'s favourite-store
- `p`/`c`: open the palette/colour-picker popups (below)

`G` (grab) and `I`/`O`/`U` (serial-attribute *plotting* into the canvas) are
deferred to Phase 5, alongside Write mode.

**Statusbar** (`src/statusbar.c`, `MSG_STATUSBAR_MAIN_FMT`) was redesigned to
match V1's `printstatusbar()` layout within the 36-content-column budget:
`Mode | XY x,y | C <code><glyph> | S <underlying-byte> | I <ink><swatch> | P
<paper><swatch> | A/S D/_ B/_`. The `S` field (`canvas_get()` at the cursor)
replaces the Phase 1-3 "canvas WxH" placeholder. The four 1-column ink/paper
swatches (`16+plotink`/`A_BGWHITE`/`16+plotpaper`/`A_BGWHITE`) are drawn via
separate `cwin_putat_char()` calls after `cwin_putat_printf()` — the literal
`"  "` gaps in `MSG_STATUSBAR_MAIN_FMT` are overwritten by these swatch
attribute bytes, which are `<0x20` and so render as nothing (not a visible
gap).

### Palette mode (Phase 4b)

`src/palette.c/h` — entered via `p` from main mode, a near-full-width popup
(`PAL_WIN_SX=2/SY=0/WX=38/WY=13`, screen cols 2-39 rows 0-12,
`menu_winsave(0, 13, 1)`) for selecting `app.plotscreencode`/
`app.plotaltchar`.

- **Layout** (13 rows x 16 cols, `PAL_GRID_X0=5`, `PAL_GRID_STEP=2`): row 0
  (`PAL_ROW_FAV`) = `"Fav:"` + 10 favourite glyphs; rows 1-6
  (`PAL_ROW_STD0=1`) = `"Std:"` + the full standard charset 0x20-0x7F (96
  glyphs, 16 per row); rows 7-12 (`PAL_ROW_ALT0=7`) = `"Alt:"` + the full
  alternate charset 0x20-0x7F, or — if `app.visualmap` is set — rows 7-11
  remapped via `visualchar[80]` (row 12 stays identity 0x70-0x7F). Each grid
  cell is the screencode byte itself, drawn under the row's `A_STD`/`A_ALT`
  attribute byte at `PAL_ATTR_X=4`.
- **`visualchar[80]`**: ported verbatim from V1 OricScreenEditor
  (`/home/xahmol/git/OricScreenEditor/src/main.c`), credited to jab/Artline
  Designs (Jaakko Luoto) — see README.md Credits. Covers screencodes
  0x20-0x6F; the final alternate row (0x70-0x7F) is always identity, matching
  V1.
- **Cursor**: `(rowsel, colsel)` highlighted via `^0x80` (same convention as
  `charsetedit`'s grid cursor). Initial position mirrors V1: derived from
  `app.plotscreencode` (`row = (code-0x20)/16 + 1`, `col = (code-0x20)%16`),
  always landing in the Std rows regardless of `app.plotaltchar`. OSE uses a
  13-row grid (V1 used 12); `pal_wrap()`'s post-check order was re-derived for
  13 rows to avoid an out-of-bounds `favourites[]` access when wrapping DOWN
  from the last Alt row into a favourites column > 9.
- **Keys**: cursor keys navigate with wrap across all three sections;
  `SPACE`/`ENTER` set `plotscreencode`/`plotaltchar` from the highlighted cell
  and close the popup; `0`-`9` store the highlighted cell's screencode into
  `favourites[n]` (redraws the Fav row); `v` toggles `app.visualmap`
  (redraws rows 7-12); `FUNCT+6` toggles the statusbar; `ESC` closes the popup
  unchanged.
- **Charset-swap**: opts IN (`menu_winsave(..., 1)`) — the Std/Alt grids
  render via the `A_STD`/`A_ALT` attribute bytes, not direct charset-RAM
  access, so there's no live-edit-preview requirement (unlike charsetedit).

**Noted follow-up (not blocking)**: V1 ships `assets/Petscii.css.bin` as the
default Alt charset, which is what makes `visualmap`'s reordering visually
meaningful. OSE doesn't load any default Alt-charset asset at startup (a
Phase-1 gap, not Phase-4-specific) — `visualmap` still works correctly (a pure
index permutation over whatever is in `CHARSET_ALT`), just less visually
useful until a default-charset-loading mechanism exists.

### Colour picker (Phase 4c)

`src/colourpicker.c/h` — entered via `c` from main mode, a **new OSE-LOCI
feature over V1** (see README.md "Planned feature additions over V1": "Enhanced
palette mode also showing inverse ink/paper colour combinations"). Popup
(`CP_WIN_SX=2/SY=0/WX=36/WY=13`, screen cols 2-37 rows 0-12,
`menu_winsave(0, 13, 1)`) for selecting `app.plotink`/`app.plotpaper` from an
8x8 ink x paper grid.

- **Layout**: row 0 = title (`MSG_COLOURPICKER_TITLE`); rows 1-8
  (`CP_ROW_GRID0=1`) = one row per paper value 0-7, each with 8 cells (one per
  ink value 0-7) at `CP_GRID_X0=2`, `CP_CELL_STEP=4` cols/cell: `[ink-attr
  byte, paper-attr byte (16+paper), normal swatch, inverse swatch]`. Rows
  10-12 are feedback lines: `"Ink:    N"` + swatch (`CP_ROW_INK`), `"Paper:
  N"` + swatch (`CP_ROW_PAPER`), `"Result:"` + ink/paper attrs + normal+inverse
  preview pair (`CP_ROW_RESULT`).
- **Cursor**: the highlighted cell swaps its two swatch chars
  (`CH_SPACE`<->`CH_INVSPACE`; normal = paper-colour swatch then ink-colour
  swatch, highlighted = reversed) — a 2-char analogue of charsetedit's `^0x80`
  cursor. Initial position = `(app.plotink, app.plotpaper)`.
- **Keys**: LEFT/RIGHT cycle ink (wrap 0-7), UP/DOWN cycle paper (wrap 0-7);
  `SPACE`/`ENTER` commit the highlighted cell to `plotink`/`plotpaper` and
  close the popup; `FUNCT+6` toggles the statusbar; `ESC` closes the popup
  unchanged.
- **Adapted from** V1's `colourpicker()`/`colorpicker_cursorplot()` (archived
  `nonworkingcc65:src/colorpicker.c`) — same 8x8 grid + Ink:/Paper:/Result:
  feedback concept; the border-drawing cursor is replaced with the simpler
  2-char swatch swap.
- **Charset-swap**: opts IN — swatch glyphs (`CH_SPACE`/`CH_INVSPACE`) are
  plain Std-charset chars, no live-edit-preview requirement.

**Known issue (parked 2026-06-17, not blocking)**: user-tested on real Oric
Atmos hardware and reports the colour picker's grid colours do not render
correctly (screenshot pending — not yet reproduced in any emulator: this
was tested before it was confirmed that Phosphoric's LOCI emulation
(alpha-quality, see "Phosphoric Testing Notes") could be used here; worth
retrying under Phosphoric before assuming a real-hardware-only ULA
timing quirk. Oricutron still cannot emulate LOCI at all, so it remains
unusable for this regardless). Suspected scope: this popup is
the first place in the codebase that changes ink *and* paper 8 times each
within a single 36-column row (16 attribute changes/row) — `cp_draw_grid()`'s
per-cell `[ink-attr, paper-attr, swatch0, swatch1]` write in
`src/colourpicker.c`. Everywhere else (statusbar swatches, palette) changes
colour at most twice per row. Deviations from V1's `colourpicker()` (4-col
cells with no leading reset byte, vs V1's 5-col `[A_BGWHITE, ink, paper+16,
'-', '-'+128]`; `CH_SPACE`/`CH_INVSPACE` swatches vs V1's `'-'`/`'-'+128`;
swap-based cursor vs V1's bordered `colorpicker_cursorplot()`) were reviewed
and should be colour-neutral under the documented Oric attribute model (ink
and paper are independent, left-to-right, V1's leading `A_BGWHITE` is
immediately overwritten by the following `paper+16` so has no effect on
final cell colour) — so this is suspected to be either a real bug in dense
per-row attribute writing, or a genuine real-hardware ULA timing quirk not
present in emulation. Not confirmed either way pending a real-hardware
screenshot. **Does not block Phase 5**: `app.plotink`/`plotpaper` can already
be set without this popup via the `,`/`.`/`;`/`'` cycling keys (Phase 4a,
`src/editor.c`), and Phase 5's write/line-box/select-move modes and the
deferred `G`/`I`/`O`/`U` serial-attribute-plotting keys write one attribute
byte at a time into `canvas.c`'s `screenmap[]` — a different, much simpler
code path than this widget's dense per-row grid, so they're not expected to
inherit this bug. Revisit once a real-hardware screenshot is available.

### Cursor auto-scroll fix (Phase 5)

Before Phase 5, `editor_run()`'s cursor-move keys clamped to the 40x27
viewport with no way to reach canvas cells beyond it — a real gap, since
Screen > Width/Height (Phase 2) already lets the canvas grow past the
viewport. `cursor_move_scroll(dx, dy)` (`src/canvas.c/h`) fixes this: at a
viewport edge, if the canvas extends further in that direction, it scrolls
`app.xoffset`/`yoffset` (and `canvas_blit()`s) instead of refusing to move;
otherwise it moves the cursor as before. `editor_run()`'s `KEY_UP/DOWN/
LEFT/RIGHT` cases, and the cursor movement inside Select/Line-Box's
rect-grower (`select.c`) and Write mode (`write.c`), all call this helper
rather than clamping `cursor_x`/`cursor_y` directly. Bundled into Phase 5
(confirmed with the user) rather than filed as a separate fix, since it's
the same area of code as the new modes and those modes' cursor movement
needed the same fix anyway. Tested by `tests/scripts/test_cursor_autoscroll.sh`.

Distinct from Move mode (below): auto-scroll changes `xoffset`/`yoffset` to
navigate a canvas larger than the viewport (the cursor stays on the same
canvas cell, the *view* moves); Move mode keeps `xoffset`/`yoffset` fixed
and rewrites cells within the current viewport (the view stays fixed, the
*content* moves).

### Shared rectangle-grower (Phase 5)

`src/select.c/h`'s `rect_select(draworselect)` is shared by Line/Box and
Select mode (below) — adapted from V1's `lineandbox()` (local reference at
`/home/xahmol/git/OricScreenEditor/src/main.c`). Grows a rectangle from the
cursor's starting position using cursor keys (with auto-scroll, see above);
ENTER accepts (populating file-scope statics `select_startx/starty/endx/
endy/width/height` — not `AppState` fields, since nothing outside
`select.c` reads them) and returns 1; ESC cancels and returns 0.

**Deliberate simplification vs V1**: V1 tracks which rectangle edge is
"trailing" the fixed origin and redraws only the single edge line that
moved each keypress. This port instead always recomputes the rectangle
fresh as `[min(origin,cursor) .. max(origin,cursor)]` on every keypress,
and redraws by XOR-toggling (`canvas_cell_invert()`) the *old* perimeter off
then the *new* perimeter on: cells in both perimeters are toggled twice
(net no-op, stay highlighted), cells only in the old one are toggled once
(un-highlighted), cells only in the new one are toggled once (highlighted).
This produces an identical result to V1's edge-tracking without porting its
four-way trailing-edge comparison logic. When `xoffset`/`yoffset` change
mid-grow (auto-scroll fired), `cursor_move_scroll()` already
`canvas_blit()`s, clearing all stale highlight bytes — so only the new
perimeter needs drawing in that case, not an erase pass first.

### Line/Box mode (Phase 5b)

`linebox_run()` (`src/select.c`), entered via `l` from Main mode. Calls
`rect_select(1)` (sets `app.mode = MODE_LINEBOX` for the statusbar's Mode
field); if accepted, fills the rectangle with `app.plotscreencode` via a
`canvas_put()` loop and `canvas_blit()`s. ESC at the rect stage leaves the
canvas unchanged.

### Select mode (Phase 5c)

`select_run()` (`src/select.c`), entered via `s` from Main mode. Calls
`rect_select(0)` (`app.mode = MODE_SELECT`); if cancelled, returns
immediately with no further prompt (matching V1). If accepted, prompts for
an action: `d` clears the rect to `CH_SPACE`, `i` fills it with
`app.plotink`, `p` fills it with `16+app.plotpaper`, `m` fills it with the
modifier-attribute byte for `app.plotaltchar`/`plotdouble`/`plotblink` (same
bit-packing as `oric.h`'s `A_STD`(8)/`A_ALT`(9)/.../`A_BLINK2HALT`(15): base
8, bit0=altchar, bit1=double, bit2=blink). `FUNCT+6` toggles the statusbar.
ESC at either the rect stage or the action prompt leaves the canvas
unchanged.

**Cut/copy (`x`/`c`)**: implemented in Phase 8 — see "Select mode cut/copy
(Phase 8)" below. Turned out not to need overlay RAM at all (the original
"deferred to Phase 8 because it needs overlay RAM" reasoning was based on
a wrong assumption about V1's mechanism, corrected during Phase 8's
research) — bundled into that phase anyway since it's the same "finish
the deferred editing features" work as undo.

### Move mode (Phase 5d)

`move_run()` (`src/move.c/h`), entered via `m` from Main mode. Cursor keys
shift the *visible viewport's content* (not the cursor, not `xoffset`/
`yoffset` — see "Cursor auto-scroll fix" above for the distinction) by one
row/col in `screenmap[]` directly: the leading edge fills with `CH_SPACE`,
the trailing edge's content is lost. `canvas_blit()` after each move shows
the result live. `FUNCT+6` toggles the statusbar. ENTER and ESC both exit
to Main — **equivalent here**, since every shift is already written
directly to `screenmap[]` (no separate scratch buffer the way V1's
hardware-scroll-then-bake-back approach has one) — so unlike V1, ESC does
**not** roll back shifts already applied during the session. This is a
deliberate simplification (V1's own ESC-cancel only worked by the accident
of not having baked anything back yet either, for the same single-buffer
reason).

### Write mode (Phase 5e)

`write_run()` (`src/write.c/h`), entered via `w` from Main mode. Direct port
of V1's free-typing key table: cursor keys move (with auto-scroll); `CTRL+B`/
`CTRL+A`/`CTRL+D` toggle blink/altchar/double; `CTRL+Z`/`CTRL+X` cycle ink
down/up; `CTRL+C`/`CTRL+V` cycle paper down/up; `FUNCT+1`/`FUNCT+2`/`FUNCT+3`
plot ink/paper/modifier-attribute at the cursor and advance right; `DEL`
clears the cell (no advance); `CTRL+R` toggles a local reverse-video flag;
any other printable key plots its screencode (`+0x80` if reverse-video is
on) and advances right. `FUNCT+6` toggles the statusbar. ESC exits to Main.

**Why `FUNCT+1/2/3`, not `i`/`o`/`u`**: Main mode's `i`/`o`/`u` keys
(below) plot ink/paper/modifier at the cursor, but Write mode can't reuse
those letters — they're themselves printable characters to type here. V1
uses `CH_F1/F2/F3` for the same three actions inside `writemode()`; this
port does the same. Easy to get wrong by copy-pasting Main mode's key
choices, so flagged explicitly.

`include/keyboard.h`'s Ctrl+letter decode is generic (`letter & 0x1F` in
`keyboard.c`, not a per-letter lookup table) — confirmed during
implementation that it works for any letter, not just the 4 that already
had named `KEY_CTRL_*` constants. Added `KEY_CTRL_B/D/R/V` alongside the
pre-existing `KEY_CTRL_A/C/X/Z` for readability at Write mode's call sites.

**Not covered by automated Phosphoric tests**: the `CTRL+letter` toggles and
`DEL`. Phosphoric's `--type-keys` has no CTRL-modifier escape (only Shift,
via uppercase letters) and `DEL` (0x7F) is unmapped in its `char_map` (see
"Phosphoric Testing Notes" below). Covered by the manual Oricutron/
Phosphoric walkthrough instead, and by code review (identical bit-packing/
cycling logic to the already-tested Main-mode `,`/`.`/`;`/`'`/`b`/`d`/`a`
handlers).

### `G`/`I`/`O`/`U`/`R` main-mode keys (Phase 5f)

`src/editor.c`, Main mode. `g` "grabs" the screencode/attribute under the
cursor into the matching `plot*` field: `>31` → `plotscreencode`; `<8` →
`plotink`; `>15` → `plotpaper` (minus 16); `8-15` → decodes
`plotaltchar`/`plotdouble`/`plotblink` from bits 0/1/2 of `grab-8` (same
bit positions as `oric.h`'s `A_STD`/`A_ALT`/.../`A_BLINK2HALT` constants).
**Bug fix vs V1**: V1 stores the raw 16-23 paper-attribute byte into
`plotpaper` (instead of subtracting 16) and `grab&2`/`grab&4` (values
0/2/0/4) into `plotdouble`/`plotblink` (instead of normalising to 0/1) —
both would violate this codebase's documented `plotpaper` (0-7) and
`plotdouble`/`plotblink` (0/1) invariants, so this port normalises instead
of copying the bug. `i`/`o`/`u` plot `plotink`/`16+plotpaper`/the
modifier-attribute byte at the cursor and move down one row (auto-scrolling
if needed). `r` toggles reverse-video on `plotscreencode` (XOR 0x80).

### LOCI file I/O (Phase 6)

`src/fileio.c/h` wires `include/loci.c/h` (copied verbatim from
locifilemanager-v2, see "Source Layout" — never `#include`d from any
`src/` file before this phase) into the File and Charset menus, which
previously showed "not yet implemented" for every item. Calling-convention
reference: locifilemanager-v2's `loci_present()`/`file_save()`/
`file_load()` usage (`src/dir.c` `config_save()`/`config_load()` there).

- **LOCI stays optional, checked per-action** — `loci_check_present()`
  wraps `loci_present()` with a graceful `MSG_LOCI_NOT_FOUND` popup, called
  first by every File/Charset action. Unlike locifilemanager-v2 (which
  requires LOCI to even boot), OSE keeps editing — and Phosphoric/Oricutron
  testing of everything except the file I/O itself — working with no LOCI
  device attached. `loci_present()` is a simple memory read (`*LOCI_
  SIGNATURE_ADDR == 'L'`, `include/loci.c`), so this gate is always safe to
  call regardless of whether a device is present.
- **File > Save/Load Screen**: `<name>.BIN`, a 6-byte `FileHeader` (magic +
  `canvas_width`/`height`) followed by `screenmap[]`. No charset data.
- **File > Save/Load Combined**: `<name>.BIN`, the same header followed by
  `CHARSET_STD`'s displayable glyph range (768 bytes) then `screenmap[]`.
  Both this and Screen write each piece directly from where it already
  lives (`screenmap[]`, charset RAM) via `loci_open()`+sequential
  `loci_write()`+`loci_close()` — **no staging buffer**, unlike V1, whose
  "combined" save works only because `CHARSET_STD` happens to sit directly
  before `SCREENMEMORY` in *V1's* CC65 memory map (letting it treat both as
  one contiguous span); OSE's `CHARSET_STD` ($B400) and `screenmap[]`
  (`$0580-$B1FF` somewhere) aren't adjacent, and `screenmap[]` is
  variable-size now where V1's was fixed.
- **File > Save/Load Project**: V1's literal 4-file scheme — `<name>PJ.BIN`
  (a `ProjectHeader` metadata struct: canvas size, cursor/viewport, plot*
  attributes, `stdchanged`/`altchanged`, via `file_save()`/`file_load()`'s
  single-contiguous-blob convenience wrapper), `<name>SC.BIN` (same shape
  as Screen), and `<name>CS.BIN`/`<name>CA.BIN` (768 raw bytes each,
  written only if that charset was edited this session, read only if
  present — a missing file leaves that bank untouched, matching V1).
- **`AppState` gains `stdchanged`/`altchanged`** (0/1, mirroring V1's
  `charsetchanged[2]`), set from `charsetedit.c`'s `ce_snapshot()` — the
  existing chokepoint called before every destructive glyph edit, based on
  `ce_altorstd` — so Project save knows which charset file(s) to write.
- **Charset menu Load/Save Standard/Alternate/Combined** (`altorstd`
  0/1/2, matching V1's `stdoralt` exactly): Std/Alt read/write 768 raw
  bytes directly from/to `CHARSET_STD`/`CHARSET_ALT`'s displayable range.
  **Combined save is identical to Save Std** (`CHARSET_STD`'s range is the
  only source there is to save); **combined load writes into both**
  `CHARSET_STD` and `CHARSET_ALT` (`charset_load()`, `include/charset.h`)
  — the closest available equivalent to V1's intent, since V1's ROM call
  to regenerate Alt from Std (`jsr $F816`) is a no-op on this runtime (see
  "Charset-swap mechanism" below).
- **Filenames are typed for Save, browsed for Load**: `fileio_get_filename()`
  (Save actions) is a popup reusing `resize_dialog()`'s `cwin_textinput`
  pattern, persisting `app.filename` (new `AppState` field) across calls
  as the next prompt's default. Load actions use the real directory
  browser added in Phase 7 (`filepicker_run()`, below) instead.
- **Headless coverage today is the LOCI-absent path only, but this can
  grow**: `tests/scripts/test_fileio_no_loci.sh` (confirming all 12
  File/Charset items correctly show `MSG_LOCI_NOT_FOUND` and return
  cleanly) is automated. The actual load/save byte traffic isn't covered
  yet, but — **correction from an earlier wrong assumption in this
  file** — Phosphoric *can* emulate a LOCI device (alpha-quality, see
  "Phosphoric Testing Notes"), so headless tests of real load/save
  traffic are possible and should be added; real hardware stays the
  authoritative check given the emulation's alpha status. Oricutron
  cannot emulate LOCI at all, so it remains unusable for this regardless.

### File picker (Phase 7)

`src/filepicker.c/h`'s `filepicker_run(title, filter)` replaces
`fileio_get_filename()` for every Load action (`fileio_load_screen/
combined/project()`, `fileio_load_charset()` for all three `altorstd`
values) with a real LOCI directory browser. Save actions are unaffected
(matches V1: `filepickerfromdir()` for Load, `chooseidandfilename()` for
Save — browsing to pick a name to overwrite isn't the same UX problem).

- **Adapted from locifilemanager-v2's directory engine**
  (`/home/xahmol/git/locifilemanager-v2/src/dir.c`, `struct DirElement`/
  `DirMeta` in `dir.h` — the primary reference for this phase, per
  explicit instruction), simplified for a single-pane, single-select,
  no-sort, dir-vs-file-only picker (no multi-type classification, no
  select bit).
- **The full directory listing is read into a linked list stored in
  XRAM** — RAM *on the LOCI device itself* (`include/loci.h`'s
  `xram_peek/poke/memcpy_to/memcpy_from`), a **completely different
  resource from the Oric-side bank-switched overlay RAM** ($C000-$FFFF)
  that Phase 8 is about. XRAM access needs no Oric memory banking at all,
  so this has **no Phase 8 dependency** — flagging this explicitly since
  an earlier draft of this phase mistakenly assumed XRAM-based storage
  required waiting for Phase 8's overlay-RAM work, which is wrong; the
  correction was made during implementation.
- `PickerMeta{next, prev, isdir, length}` (6 bytes) is written immediately
  before each (variable-length) name at its XRAM address — entries are
  packed tightly, not fixed-size, matching `DirElement`'s layout.
  `PICKER_DIRBASE = COPYBUF_XRAM_ADDR + COPYBUF_XRAM_SIZE` (`0x8800`,
  matching locifilemanager-v2's `DIR1BASE`), `PICKER_DIRSIZE = 0x0C00`
  (one pane's worth, since OSE only needs one list). Building the list
  stops early (silently truncating), if this budget would be exceeded —
  same guard locifilemanager-v2's `dir_read()` has.
- **Scrolling is O(1) per row**, not a directory re-walk: `picker_firstprint`/
  `picker_present` (XRAM addresses) + `picker_cursorrow` (visible-row
  index) follow `next`/`prev` pointers directly, exactly the bookkeeping
  locifilemanager-v2's `struct Directory` uses per pane.
- **Type filtering** (`PICKER_FILTER_PLAIN`/`PICKER_FILTER_PROJECT`):
  directories always match (so navigation works regardless of filter).
  `PLAIN` covers Load Screen, Load Combined and all three Charset Load
  actions — none of these are distinguishable from each other by filename
  alone (matches V1: a "combined" file and a "screen" file are both bare
  `<name>.BIN`, only the *content* differs), so they share one filter that
  excludes the four Project sub-file suffixes (`PJ/SC/CS/CA.BIN`) to avoid
  clutter/wrong-file-type mistakes. `PROJECT` (Load Project) matches only
  `*PJ.BIN`. On selection, the matched suffix is stripped before storing
  into `app.filename`, so `fileio.c`'s existing `sprintf(path,
  "%s<suffix>", app.filename)`-style composition (Phase 6, unchanged)
  keeps working without modification.
- **Full subdirectory navigation** (confirmed with the user — overrode the
  simpler flat-listing-only default I'd recommended): ENTER on a directory
  entry descends (`picker_path_descend()`, refusing rather than silently
  truncating if the resulting path wouldn't fit in `PICKER_PATH_MAXLEN=64`
  bytes); LEFT goes to the parent (`picker_path_ascend()`, no-op at the
  root). The path row always shows the current location. Selecting a file
  inside a subdirectory prepends that subdirectory to `app.filename` (a
  path relative to the LOCI root, e.g. `"DIR1/DIR2/name"`).
  `FILENAME_MAXLEN` grew from 24 to 48 (`src/appstate.h`) to leave room
  for this.
- **No new automated test coverage yet**: `filepicker_run()` is only
  reachable from a Load action that already requires
  `loci_check_present()` to pass first, so `test_fileio_no_loci.sh`'s
  existing 12 assertions (every Load/Save action's LOCI-absent path) are
  all that's automated today — the actual browsing isn't covered yet, but
  (per the Phase 6 correction above) Phosphoric's LOCI emulation makes
  this addable headlessly; not just a real-hardware-only walkthrough.

### Canvas undo/redo (Phase 8)

`src/undo.c/h`, `z`/`y` in Main mode. Genuinely new functionality — V1 has
no canvas-edit undo at all (only the character editor's single-glyph
undo, already ported as `ce_undo[]`). A **faithful port of
vdcscreeneditor-v2's `undo_new()`/`undo_performundo()`/
`undo_performredo()`** (`/home/xahmol/VDCScreenEditor2/src/main.c`, lines
739-905, `struct UndoStruct` in `include/defines.h`) — adapted for OSE's
single-plane canvas (one byte/cell, vs. VDC RAM's separate screen/
attribute planes) and **Oric-side overlay RAM** ($C000-$FFFF via
`enable_overlay_ram()`/`disable_overlay_ram()`, `include/loci.h`) instead
of bank-switched VDC RAM.

- **Don't confuse this with Phase 7's LOCI-device XRAM** — a completely
  different resource (RAM *on the LOCI device*, needing the MIA serial
  protocol) from this Oric-side, plain-pointer-addressable region. This
  distinction tripped up the first draft of this phase's plan; see the
  project memory `feedback_dont_oversimplify_ported_designs.md` for the
  full story of getting this (and the eviction policy below) corrected.
- **Ring design**: a fixed 40-slot array (matching `Undo[41]`'s 40 active
  slots), each slot independently storing its own dirty-rectangle
  snapshot (only the changed bounding box, not the full canvas) at its
  own byte offset in the 16KB region. The slot about to be overwritten
  *next* is invalidated immediately (a `valid` flag — not an overloaded
  `address==0` sentinel the way vdcscreeneditor-v2 does it, since 0 is a
  legitimate offset here) — this is what lets undo/redo always stop
  cleanly at the true edge of available history.
- **The byte bump-pointer only resets** (taking the slot index with it)
  when the next snapshot would exceed the 16KB budget — a rare event, not
  a per-snapshot check. Even then, slots elsewhere in the ring keep their
  data until a *later* cycle's growing writes physically reach them, so
  history degrades gradually across a wrap, not all at once. **This was
  the point of the correction above** — an earlier draft proposed
  discarding all history on overflow, which the user correctly rejected
  as worse *and* not actually simpler than the real mechanism.
- **Redo data is stored alongside the original snapshot** in the same
  slot (`width*height` bytes for the original + another `width*height`
  for the eventual redo copy, reserved when there's room) — `undo_perform()`
  saves the *current* live content into that reserved redo region before
  restoring the old content, exactly mirroring vdcscreeneditor-v2's
  save-before-restore ordering.
- **`undo_escapeundo()` has no OSE equivalent**: every call site here
  takes its snapshot only once an edit is actually about to be committed
  (after ENTER/confirmation, never speculatively before a cancellable
  step), so there's nothing to unwind on ESC.
- **Comprehensive coverage** (confirmed with the user): every destructive
  action calls `undo_snapshot()` first — Main-mode `SPACE`/`DEL`/`i`/`o`/
  `u`, Line/Box fill, Select's `d`/`i`/`p`/`m` fills and cut/copy, Move
  mode's per-keypress shift (whole-viewport snapshot, since a 1-cell shift
  changes every cell along that axis), Write mode's every plotted/cleared
  cell, and Screen menu Clear/Fill (whole-canvas snapshot). Dirty-rect
  snapshots are cheap (a few bytes for a 1-cell edit), so this fits the
  16KB budget comfortably even for a long Write session.
- **`undo_snapshot()` no-ops if `!loci_present()`** — undo is simply
  unavailable without a LOCI device (no popup, since this fires on every
  keystroke, not an explicit user action).
- **Bug fixed while retrofitting Main-mode `SPACE`/`DEL`**: they were
  calling `canvas_put()` with `cursor_x`/`cursor_y` only, missing the
  `+xoffset`/`+yoffset` that `i`/`o`/`u`/`g` already apply — meant they
  plotted at the wrong canvas cell once the viewport had auto-scrolled
  (Phase 5a). `undo_snapshot()` needed the corrected coordinates to
  snapshot the right cell, so both call sites were fixed together.
- **Not testable headless beyond the LOCI-absent path today** (this is
  the Oric-side overlay RAM, not LOCI-device XRAM, but the same
  `loci_present()` gate gets in the way) — `tests/scripts/
  test_undo_no_loci.sh` confirms `z`/`y` are graceful no-ops with no LOCI
  present. Unlike Phases 6-7's plain file-I/O/directory traffic, this
  specifically needs Phosphoric's LOCI emulation to *also* model
  `MICRODISCCFG`-driven overlay-RAM banking, not just the MIA/TAP/XRAM
  protocol — unconfirmed whether that's modelled; worth checking before
  assuming a real-hardware walkthrough is the only option (see the
  Phase 8 plan).

### Select mode cut/copy (Phase 8)

`select_run()` (`src/select.c`) gains `x`/`c`, completing V1's full
prompt key set (`d`/`i`/`p`/`m`/`x`/`c`). **Turned out not to need overlay
RAM at all**: V1's `selectmode()` cut/copy doesn't use a full-rectangle
scratch buffer — it copies **one row at a time** through `SCREENMEMORY`
(a single-row reuse of live screen RAM), choosing top-to-bottom vs.
bottom-to-top row order based on whether the destination is below or
above the source (handles overlap correctly). OSE's existing
`canvas_rowbuf[CANVAS_MAX_ROW]` (`src/canvas.c`, already used by
`canvas_resize()`'s row reflow for exactly the same overlap-safety
reason) serves the same role — exposed via `canvas.h` so `select.c` can
reuse it (both uses are mutually exclusive in time, so sharing the single
320-byte buffer is safe). `select_width` can never exceed
`CANVAS_MAX_ROW` since it's bounded by `app.canvas_width`, which
`canvas_resize()` already validates — no V1-style explicit
`select_width>1024` cap needed.

After picking `x`/`c`, a destination-picking sub-loop (cursor keys via
`cursor_move_scroll()`, `FUNCT+6` toggles the statusbar, `ENTER`
confirms, `ESC` cancels — same shape as `rect_select()`'s own loop, with
its own cursor highlight). On `ENTER`, validates the destination fits
within the canvas (`MSG_SELECT_NOFIT`, "Selection does not fit.", V1's
exact wording, if not); `undo_snapshot()`s the destination rect (and, for
cut, the source rect too — **two separate undo slots, so reverting a cut
needs two `z` presses** — a minor known wrinkle, not fixed) before
`select_paste()` runs the per-row copy.

Fully testable headless (`tests/scripts/test_select_cutcopy.sh`) — unlike
undo, this needs no LOCI/overlay RAM at all.

### IJK joystick input (Phase 9a)

`src/input.c/h` (new): `key_read()` is a faithful port of `nonworkingcc65`'s
`getkey(joyallowed=1, norepeat=1)` (V1 itself has no IJK code at all — the
IJK driver, `include/ijk.c/h`, is purely a LOCI-port addition, copied in at
the very start of this project but never `#include`d from `src/` until
this phase). Each call: if `ijk_present` (set once by `ijk_detect()`,
called from `main.c` right after `charwin_init()`), reads both sticks
(`ijk_read()`) and maps `FIRE`/`RIGHT`/`LEFT`/`DOWN`/`UP` bits (left stick
checked first, then right, last-match-wins) to
`KEY_ENTER`/`KEY_RIGHT`/`KEY_LEFT`/`KEY_DOWN`/`KEY_UP`; if a direction
fired, drains the stick to neutral before returning (so a single press
doesn't repeat). Falls back to `keyb_poll()` (`include/keyboard.h`) if the
joystick gave nothing this spin, or no joystick is present. Every
`cwin_getch()` call site across `src/*.c` (11 files: `main`, `editor`,
`select`, `move`, `write`, `charsetedit`, `palette`, `colourpicker`,
`filepicker`, `menu`, `menudata`) now goes through `key_read()` instead —
joystick input works everywhere keyboard input does. `include/charwin.c`'s
`cwin_getch()` itself is untouched (a generic library function with no
peripheral-specific knowledge).

### Help system (Phase 9b)

`src/help.c/h` (new): `help_show(screennumber)` shows one of 4 help
screens (1=Main, 2=Character editor, 3=Select/Move/Line-Box, 4=Write —
ported from V1's `helpscreen_load(screennumber)`), each a raw 1080-byte
(40x27) screencode dump **embedded at compile time**
(`assets/OSEforLOCI-Help{1..4}.bin`, Oscar64's `#embed`) rather than
tape-loaded at runtime like V1 — works even without a LOCI device.
`charsetswap_enter()`/`exit()` bracket the raw `$BB80` blit (same
direct-write approach `canvas_blit()` uses, for the same reason: a
pre-rendered image can use arbitrary attribute bytes in columns 0-1 that
`cwin_clear()`-based popups would clobber); `key_read()` waits for a key,
then `canvas_blit()` + `statusbar_draw()` restore the real canvas (no
manual screen-RAM save/restore needed, unlike V1, since OSE already has
the real content in `screenmap[]`/`AppState`).

Wired to `FUNCT+8` (`KEY_F8`): `editor.c` (Main, unconditional) →
`help_show(1)`; `charsetedit.c` → `help_show(2)` (also redraws the popup's
own chrome — `cwin_clear()` + `ce_draw_header/favourites/grid` — since
`help_show()`'s `canvas_blit()` would otherwise leave the popup's screen
area showing raw canvas content instead of the popup's white background);
`select.c`'s shared `rect_select()` → `help_show(3)`, **guarded to only
fire before the rect starts growing** (`curx==orgx && cury==orgy`, V1's
exact guard — ported, not redesigned, per
`feedback_dont_oversimplify_ported_designs`) since a live rect-highlight
redraw underneath would be visually awkward; `move.c` → `help_show(3)`
unconditionally (no rect-growing concern there); `write.c` →
`help_show(4)`. **Not** wired into `palette.c`/`colourpicker.c` — no V1
precedent (V1 has no help screen for Palette, and `README.md` previously
had an incorrect claim that it did — corrected in Phase 9d).

**Embedding gotcha** (see `oscar64manual.md`'s "Embedded Data" section for
the full writeup): `#embed` must be the only thing on its source line —
`byte data[] = { #embed "file.bin" };` all on one line mis-tokenizes the
embedded bytes. A second, size-budget gotcha: embedding the 4 help
screens raw (1080 bytes each) plus, in Phase 9c, the title image and QR
code pushed the `$0580-$B200` code/data/bss region too close to its
ceiling for `oscar64`'s heap section to be placed at all (`error 3034:
Cannot place heap section`) even though the total binary size was well
under the region's nominal size. Since `malloc`/`free` are fully stubbed
in this bare-metal runtime (`crt_math.c`'s `crt_malloc` always returns
NULL — no real heap is ever used), the fix was to drop `heap` from
`oric_crt.c`'s main region section list, **and** to switch all 5 raw
1080-byte screen dumps (4 help screens + the title image, Phase 9c) to
LZO compression (`#embed lzo "file.bin"`, `oscar.h`'s
`oscar_expand_lzo()` decompressing straight into `$BB80` screen RAM,
which sits outside the code/data/bss region entirely) — this both shrinks
the embedded data and avoids needing any decompression scratch buffer in
BSS.

**Prime target if more memory is ever needed**: these 5 embedded screens
(4 help screens + the title image, ~5.4KB raw, still a few KB even
LZO-compressed) are the single largest static-data cost in the binary
purely for the sake of LOCI-optionality (see "LOCI file I/O (Phase 6)"'s
"LOCI stays optional" point). If a future feature needs the freed
`$0580-$B200` headroom, converting these 5 assets to load from LOCI
storage at runtime (`loci_open`/`loci_read`, same mechanism as
`fileio.c`) instead of `#embed` is the first thing to consider — at the
cost of losing help/the Version-page title image when no LOCI device is
attached, and losing `test_help_funct8.sh`'s headless coverage unless
ported to run against Phosphoric's LOCI emulation (which can run these
tests too, just not yet wired up — see "Phosphoric Testing Notes").

### Information menu (Phase 9c)

`src/info.c/h` (new), wired into `menudata.c`'s Information pulldown
choices 41 (Version) / 42 (Exit) — the last two menu items that had been
permanent stubs since Phase 2.

- **`info_version_show()`**: a 3-page popup, adapted from
  `locifilemanager-v2`'s `versioninfo()` (same version/credits + QR-code
  concept, same vendored QR-generation library). Page 1 =
  `OSEforLOCI-Title.bin` (a full 40x27 image, unlike `locifilemanager-v2`'s
  smaller partial logo designed to share a screen with text) full-screen,
  raw-blit + LZO (same approach as the help screens, see above). Page 2 =
  programmatic version/credits text (`cwin_init`/`cwin_clear`, black
  background) — title, author, `VERSION_MAJOR`/`MINOR`/`PATCH` (the same
  macros the splash screen already uses), GitHub URL, copyright. Page 3 =
  a 25x25-module QR code linking to
  `https://github.com/xahmol/OricScreenEditorLOCI`
  (`CH_INVSPACE`/`CH_SPACE` per module), generated by `tools/gen_qr.js`
  (adapted from `locifilemanager-v2/tools/gen_qr.js` — same vendored
  `kazuhikoarase/qrcode-generator` library at
  `/usr/share/nodejs/qrcode-terminal/vendor/QRCode/`, just a different
  `TEXT` constant) and pasted into `info.c` as a static array — a one-time
  generation step, not a build dependency (no Node.js requirement added to
  the Makefile). Any key advances each page; `menu_winsave(0,
  VIEWPORT_HEIGHT+1, 1)`/`menu_winrestore()` (main-RAM, not
  `locifilemanager-v2`'s overlay-RAM `menu_popup_open`/`close()`) save/
  restore the whole screen around all 3 pages.
- **`info_exit()`**: V1 (CC65) just returns control to the OS-level tape
  loader; OSE's bare-metal Oscar64 runtime replaced that loader entirely at
  boot (see "Memory Layout" below), so there's nothing to "return" to. The
  safe, hardware-documented equivalent
  (`~/.claude/oric_atmos_reference.md`'s vector table) is a software reset
  through the RESET vector: `__asm { jmp ($fffc) }` — **not** a direct call
  to the cold-start handler at `$F88F`, which the reference explicitly
  warns must only be invoked via the CPU RESET pin. No confirmation is
  asked (matches `README.md`'s already-documented Exit behaviour); manually
  verified in Phosphoric (post-jump CPU state has interrupts re-enabled —
  the `I` flag clear, unlike this app's permanent boot-time `SEI` — and the
  PC deep in ROM, confirming the reset took effect). Not headlessly
  testable (a real reset is indistinguishable from a crash to the test
  harness from outside).

### Charset-swap mechanism

`src/charsetswap.c/h` + `include/charset.c/h` ensure popup chrome (menu bar,
pulldowns, Are-you-sure/message popups, the resize dialog) always renders
with the ROM-standard `CHARSET_STD` glyphs, even after the user has redefined
characters in the character editor -- without permanently discarding the
user's edits.

- **Generic primitives** (`include/charset.c/h`, no OSE-specific state, no
  `#pragma compile` chain back into `src/`): `charset_address()`,
  `charset_save()`/`charset_load()` (768-byte displayable-range copies
  between a charset bank and a buffer or `CHARSETROM`), `charset_rom_glyph()`,
  and the 7 glyph-bitmap transforms
  (`charset_glyph_invert/mirror_v/mirror_h/scroll_up/scroll_down/rotate_left/
  rotate_right`) used by the character editor's `i`/`x`/`y`/`u`/`d`/`l`/`r`
  commands.
- **Std-only** (Part 1 spike outcome): `jsr $F816` (`ROM_ALTCHARS`, used by V1
  to regenerate `CHARSET_ALT` from `CHARSET_STD`) is a no-op when called from
  Oscar64 in this runtime -- `CHARSET_ALT` is left untouched by the swap.
  Only `CHARSET_STD`'s 768-byte displayable range is backed up/restored.
  `CHARSET_ALT` keeps whatever the user has edited, even while popups are
  open; this is acceptable because popup chrome (menu bar, pulldowns,
  dialogs) only ever uses `CHARSET_STD` glyphs (`A_STD` attribute).
- **`charset_changed` gate**: `charsetswap_mark_changed()` is called once,
  from `ce_snapshot()` (the chokepoint already called before every
  destructive glyph edit in `charsetedit.c`). Until the user edits a glyph
  for the first time, `charsetswap_enter()`/`exit()` are no-ops -- popups on
  an unmodified charset render identically with or without the swap.
- **Depth-counting** (`swap_depth`): `charsetswap_enter()` only backs up
  `CHARSET_STD` on the *outermost* call (`swap_depth==0`);
  `charsetswap_exit()` only restores on the call that brings `swap_depth`
  back to 0. This lets nested popups (e.g. the Screen pulldown opening the
  resize dialog, which can open an Are-you-sure popup) share a single backup
  buffer safely.
- **`menu_winsave(ypos, height, swap_charset)`**: the new third parameter is
  stored in the pushed `MenuWinRecord` (see "Menu System" below);
  `menu_winrestore()` pairs `charsetswap_exit()` with a saved record's
  `charsetswap_enter()` automatically via the LIFO window stack, so callers
  cannot get enter/exit out of sync. **Opt-in convention**: every popup
  passes `1` except the character editor, which passes `0` (see "Charset-swap
  opt-out" above) so its live glyph edits remain visible while it is open.
  Current call sites: `menu_run()` (menu bar, `menudata.c`),
  `menu_pulldown()`, `menu_areyousure()`, `menu_messagepopup()` (all
  `src/menu.c`), `resize_dialog()` (`src/menudata.c`), `palette_run()`
  (`src/palette.c`) and `colourpicker_run()` (`src/colourpicker.c`) all pass
  `1`; `charsetedit_run()` (`src/charsetedit.c`) passes `0` — the only popup
  with the live-charset-RAM preview requirement.

## Code Style

Every C function definition (including `static` helpers) gets a
Javadoc-style `/** ... */` doc comment immediately above it, in the `.c`
file (not the `.h` declaration), following locifilemanager-v2's
`charwin.c`/`keyboard.c`/`loci.c` convention: a prose description, then
`@param name  Description` for each parameter and `@return Description`
(`@return (none)` for `void`). See `libmanual.md`/`libmanual_fr.md` for
worked examples drawn from `include/charset.c`. This is a cross-project
convention (see global memory `feedback_function_doc_comments.md`), not
specific to OSE.

## Compiler Toolchain

This is an **Oscar64** project. `oscar64manual.md` is in the project root —
consult it for compiler flags, pragmas, language extensions, and gotchas. The
Oscar64 binary is at `$OSCAR64_HOME/bin/oscar64` (Makefile defaults
`OSCAR64_HOME` to `~/oscar64`).

Target: 6502A (Oric Atmos), bare-metal. No VIC-II, SID, or C64 Kernal.

**Critical Oscar64 gotchas (all documented in oscar64manual.md):**
- `va_arg` is broken in native mode (`-n`) — do not use `<stdarg.h>` /
  `va_list` / `va_arg`.
- `#if MACRO` fails when macro is defined via `-d` flag (no value) — use
  `#ifdef MACRO`.
- `(type)struct.member` — Oscar64 applies cast before member access; use a
  temp variable.
- Macro that expands to a volatile read in a braces-free for-loop body causes
  a parse error; use a braced body with an explicit temp.
- Ternary `? ptr : 0` in a pointer-returning function — use `if`/`return`
  instead.

## Build and Run

```
make              # build build/oseloci.tap (English, LANG=EN default)
make LANG=FR      # build build/oseloci_fr.tap (French)
make all-langs    # build both build/oseloci.tap and build/oseloci_fr.tap
make run          # build + launch in Oricutron (LANG=FR for the French build)
make clean        # remove build artifacts (both languages)
make docs         # regenerate README.pdf from README.md (requires pandoc)
make test         # full automated Phosphoric test suite (test-boot, test-menus,
                  # test-screenresize, test-charsetram-spike, test-charsetedit,
                  # test-palette, test-colourpicker, test-cursor-autoscroll,
                  # test-linebox, test-select, test-move, test-writemode,
                  # test-fileio-no-loci, test-select-cutcopy,
                  # test-undo-no-loci, test-help-funct8) -- EN only, see
                  # "Localisation" below
make test-boot    # headless boot smoke test (splash + canvas/statusbar render)
make test-capture CYCLES=N TYPEKEYS='...'
                  # calibration helper: dumps tests/out/capture.bin + .png
```

Compiler flags: `-n -tf=bin -rt=include/oric_crt.c -i=include -i=src -O2 -dNOFLOAT $(LANGFLAG)`
(`LANGFLAG` is `-dLANG_FR` for `LANG=FR`, empty for the default `LANG=EN`)

Emulator: `$ORICUTRON_HOME/oricutron -ma --serial none --vsynchack off --turbotape on`
(Makefile defaults `ORICUTRON_HOME` to `~/oricutron`)

### Phosphoric test harness

`make test*` targets use a checkout of
[Phosphoric](https://github.com/benedictemarty/Phosphoric) (`oric1-emu` +
`roms/basic11b.rom`) to fast-load `oseloci.tap` headless and assert on
`$BB80` screen-RAM dumps via `tests/scripts/oric_screen.py`. Set `PHOSDIR` in
`.env` (gitignored, see `.env.example`). `tests/fixtures/` is checked in;
`tests/sandbox/` and `tests/out/` are gitignored scratch dirs regenerated by
`make sandbox-reset`/`make test*`.

**Phosphoric *can* emulate a LOCI device** (alpha-quality) — a fact this
file got wrong in several places until corrected by the user 2026-06-18;
don't re-introduce the "Phosphoric/Oricutron can't emulate LOCI" claim
elsewhere. **Oricutron cannot emulate LOCI at all.** This means LOCI-
dependent features (file I/O, the file picker, undo/redo's overlay RAM)
are not necessarily real-hardware-only for testing — Phosphoric should be
used to add headless coverage of the actual byte-level behaviour, not
just the LOCI-absent path, where practical. Real Oric Atmos hardware
remains the *authoritative* check given Phosphoric's LOCI emulation is
still alpha-quality — a Phosphoric pass doesn't retire the need for an
eventual real-hardware confirmation, but it's a much faster and more
repeatable first line of testing than jumping straight to hardware.

## Memory Layout (`include/oric_crt.c`)

Custom Oscar64 runtime, copied from locifilemanager-v2. Full details and
rationale are in the file's header comment; summary:

- `$0000-$00FF` — Zero page (Oscar64 internal registers)
- `$0100-$01FF` — 6502 hardware stack
- `$0200-$04FF` — Oric ROM system variables (do not use)
- `$0500-$057F` — Startup region (tape entry point -> `oric_startup`)
- `$0580-$B1FF` — Program code, data, BSS, heap (~42.4 KB)
- `$B200-$B3FF` — Oscar64 software stack (512 bytes)
- `$B400-$BBFF` — Character set RAM (left untouched — `$B400` is OSDK's
  documented ceiling for user code/data)
- `$BB80-$FFFF` — Screen RAM (`$BB80`) + ROM (`$C000`)
- `$C000-$FFFF` — Overlay RAM, **requires LOCI device**, not mapped as code

**IRQ convention:** interrupts are left disabled (`SEI`, no `CLI`) — the
keyboard scanner polls directly, no IRQ handler is installed. Any code that
must briefly enable IRQs (overlay RAM access via `MICRODISCCFG`, VIA Port A
access in `ijk.c`) **must** use `PHP; SEI ... PLP`, never `SEI ... CLI` — an
unconditional `CLI` would permanently re-enable IRQs and let the stock ROM IRQ
handler corrupt zero page / screen RAM.

## Source Layout

```
src/
  main.c          Application entry point: splash, canvas/statusbar init, editor_run()
  appstate.h      Global AppState struct (canvas size, cursor, viewport, mode, ...)
  canvas.c/h      Flat 40x27 screenmap[] buffer + raw $BB80 blit (bypasses charwin),
                  canvas_resize() (up to CANVAS_MAX_SIZE)
  statusbar.c/h   Row-27 statusbar (OricCharWin, Mode/XY/C/S/I/P + A/D/B flags,
                  see "Main-mode attribute-selection keys and statusbar
                  redesign (Phase 4a)")
  editor.c/h      Main-mode loop: cursor move (with auto-scroll, see
                  canvas.c's cursor_move_scroll()), +/- screencode select,
                  SPACE/DEL plot, ,/.;/' ink/paper cycling, b/d/a/r attribute
                  toggles, g grabs the screencode/attr under the cursor,
                  i/o/u plot ink/paper/modifier, z/y undo/redo,
                  0-9/SHIFT+0-9 favourites, FUNCT+1 opens the menu bar,
                  'e' opens the character editor, 'p'/'c' open the
                  palette/colour picker, 'l'/'s'/'m'/'w' open Line-Box/
                  Select/Move/Write mode (see "Phase 5"/"Canvas undo/redo
                  (Phase 8)" sections)
  modes.h/c       EditorMode -> MSG_MODE_* display-name lookup
                  (mode_name()), shared by statusbar.c and any mode file --
                  standalone, no dependency in either direction with
                  editor.c/select.c/move.c/write.c
  select.c/h      Shared rectangle-grower (rect_select()) + Line/Box mode
                  ('l', linebox_run()) + Select mode ('s', select_run(),
                  incl. x/c cut/copy) -- see "Shared rectangle-grower"/
                  "Line/Box mode"/"Select mode" (Phase 5)/"Select mode
                  cut/copy (Phase 8)"
  move.c/h        Move mode ('m'): nudges the visible canvas content by one
                  row/col (see "Move mode (Phase 5d)")
  write.c/h       Write mode ('w'): free-typing screencodes with CTRL-key
                  attribute toggles (see "Write mode (Phase 5e)")
  menu.c/h        Menu bar/pulldown/popup engine + main-RAM window-save stack
  menudata.c/h    OSE menu tables (Screen/File/Charset/Information) + Screen/
                  File/Charset pulldown dispatch (Width/Height/Clear/Fill,
                  Save/Load Screen/Project/Combined, Charset Load/Save
                  Std/Alt/Combined)
  fileio.c/h      LOCI file I/O backing the File/Charset menus: filename
                  prompt (Save), LOCI-presence gate, Screen/Combined/
                  Project save/load, Charset Std/Alt/Combined save/load
                  (see "LOCI file I/O (Phase 6)")
  filepicker.c/h  XRAM-backed LOCI directory browser, replaces the
                  filename prompt for every Load action (see "File picker
                  (Phase 7)")
  undo.c/h        Canvas-edit undo/redo ('z'/'y'), overlay-RAM-backed
                  dirty-rect ring buffer (see "Canvas undo/redo
                  (Phase 8)")
  input.c/h       key_read(): universal keyboard+IJK-joystick blocking
                  read, replaces cwin_getch() everywhere (see "IJK
                  joystick input (Phase 9a)")
  help.c/h        FUNCT+8 help screens (Main/Character editor/Select-
                  Move-Line-Box/Write), #embed'd + LZO-compressed (see
                  "Help system (Phase 9b)")
  info.c/h        Information menu actions: Version (3-page popup w/
                  title image + credits + QR code) and Exit (RESET
                  vector) (see "Information menu (Phase 9c)")
  charsetedit.c/h Character editor popup: edits the 6x8 glyph for
                  app.plotscreencode/app.plotaltchar directly in live charset
                  RAM (Strategy A -- see "Character editor (Phase 3)")
  charsetswap.c/h Charset-swap mechanism: backs up/restores CHARSET_STD around
                  popups so chrome renders with ROM glyphs (see "Charset-swap
                  mechanism")
  palette.c/h     Palette popup ('p'): Fav/Std/Alt charset grid + visualchar[]
                  visual-charmap toggle (see "Palette mode (Phase 4b)")
  colourpicker.c/h Colour picker popup ('c'): 8x8 ink x paper grid with
                  inverse-colour preview, NEW vs V1 (see "Colour picker
                  (Phase 4c)")
  strings.h       Localisation gateway: #include "strings_en.h" or
                  "strings_fr.h" depending on LANG_FR (see "Localisation")
  strings_en.h    All user-visible MSG_* strings, English (default)
  strings_fr.h    All user-visible MSG_* strings, French (LANG=FR), unaccented
include/
  oric.h          Hardware constants (VIA, AY, screen, overlay RAM, ASTR_*/CH_*/A_* attrs)
  oric_crt.c      Custom Oscar64 runtime: startup, regions, math stubs
  crt_math.c      Math/float runtime routines for the custom runtime
  keyboard.h/c    Direct VIA/AY keyboard scanner — no ROM calls
  charwin.h/c     Character window / menu library
  charset.h/c     Generic Oric charset-bank addressing/copy + glyph-bitmap
                   transform primitives (new to this project, not copied from
                   locifilemanager-v2; see "Charset-swap mechanism")
  loci.h/c        LOCI mass storage API (MIA/TAP/XRAM/file/dir/mount/overlay RAM)
  ijk.h/c         Raxiss IJK joystick driver (VIA Port A; independent of LOCI)
tools/
  mktap.py        Wraps .bin in Oric tape header
tests/
  scripts/        oric_screen.py (decode $BB80) + test_*.sh (Phosphoric harness)
  fixtures/       Checked-in files copied into tests/sandbox/ by sandbox-reset
build/
  oseloci.tap     Output tape image (gitignored)
```

All library files (`oric_crt.c`, `crt_math.c`, `oric.h`, `charwin.*`,
`keyboard.*`, `ijk.*`, `loci.*`) were copied verbatim from
locifilemanager-v2 — see that project's CLAUDE.md and `oscar64manual.md` for
their full API documentation (charwin window model, attribute macros, key
codes, MIA/TAP register layout, MIA helper calling conventions, etc.).
`src/strings.h`/`strings_en.h`/`strings_fr.h` follow locifilemanager-v2's
EN/FR localisation gateway pattern — see "Localisation" below.

## Localisation

OSE follows locifilemanager-v2's EN/FR localisation convention: every
user-visible string is a `MSG_*` macro, selected at compile time via a
gateway header.

- **`src/strings.h`** is the gateway: `#ifdef LANG_FR` includes
  `strings_fr.h`, else `strings_en.h`. Every source file that displays text
  (`main.c`, `statusbar.c`, `charsetedit.c`, `menudata.c`, plus `loci.c` when
  it's wired in) `#include "strings.h"` and references only `MSG_*` macros —
  no raw user-visible string literals in logic files.
- **`strings_en.h`/`strings_fr.h`** define the same set of `MSG_*` macros in
  each language. `strings_fr.h` uses **no accented characters** — the Oric
  ROM charset has no e-acute/e-grave/c-cedilla etc. (e.g. "Deplacer",
  "Effacer", "Oui"/"Non", "demarrer", "donnees").
- **Makefile**: `LANG ?= EN`; `LANG=FR` sets `LANGFLAG = -dLANG_FR` (appended
  to `CFLAGS`) and `LANGSUFFIX = _fr` (so outputs are `build/oseloci_fr.bin`/
  `.tap`, vs `build/oseloci.bin`/`.tap` for EN). `PROGNAME` (the tape header
  name `OSELOCI`) is identical for both languages. `make all-langs` builds
  both. All `test-*`/`sandbox-reset` targets are suffix-aware but their
  *assertions* (`oric_screen.py --find`) are written against the EN strings,
  so `make test` only exercises the EN build.
- **Key bindings are independent of `MSG_*`** — they're hardcoded chars in
  `switch (cwin_getch())` statements, untouched by language selection. Any
  future localisation work **must keep key bindings identical across
  languages**; only displayed labels translate.
- **Menu-bar layout constraint**: `menu_placebar()` (`src/menu.c`) lays out
  `menubar.titles[]` dynamically left-to-right with a 1-column gap on each
  side, and requires `sum(strlen(titles)) + 7 <= 39` (40-column row) or the
  last item gets clamped and can overlap the previous one. EN
  ("Screen"/"File"/"Charset"/"Information" = 6+4+7+11=28) has 4 chars of
  margin; FR's natural translations summed to 33 (1 over), so
  `MSG_MENU_BAR_CHARSET` is abbreviated `"Caract."` (7, from "Caracteres",
  10) to bring the FR total to 30. **Any new menu-bar title in either
  language must keep the 4-item sum <= 32**, or `menu_placebar()`'s clamp
  will visually merge the last two items.
- **Other length constraints** (`src/menu.h`): `MENUBAR_MAXLENGTH=12` (11
  visible chars + NUL) for `menubar.titles[]`; `PULLDOWN_MAXLENGTH=17` (16
  visible chars + NUL) for `pulldown_titles[][][]`. `pulldown_titles[0][0]`/
  `[0][1]` ("Width:"/"Height:" or "Largeur:"/"Hauteur:") are placeholders
  overwritten at runtime by `update_size_titles()` via
  `MSG_SCREEN_WIDTH_FMT`/`MSG_SCREEN_HEIGHT_FMT`.

## Oric Screen Model

- **40×28 character grid**, screen RAM at `$BB80`
- **Serial attributes:** byte with `(byte & 0x60) == 0` is an attribute
  (values 0x00-0x1F only). `0x20` (space) is a character, not an attribute.
  ULA resets to white-ink/black-paper at the start of each raster line, then
  applies attrs left-to-right.
- **Inverse video:** `ch | 0x80` inverts pixel rendering; `CH_INVSPACE`
  (0xA0) = solid ink-coloured block.

## Menu System (Phase 2)

`src/menu.c/h` is adapted from locifilemanager-v2's bar/pulldown/popup engine
(`/home/xahmol/git/locifilemanager-v2/src/menu.c/h`). Three layers: menu bar
(row 0, `MENUBAR_MAXOPTIONS=4`: Screen/File/Charset/Information) → pulldown
menus (`PULLDOWN_NUMBER=5`: 4 bar pulldowns + index `MENU_YESNO=4` for the
Yes/No confirm dialog, `PULLDOWN_MAXOPTIONS=6`) → popup dialogs.

- **`menu_main()` return encoding**: `menubarchoice*10 + menuoptionchoice`
  (1..49 = a real choice); `menubarchoice*10 + 99` (>= 99) = ESC at bar level.
  Callers loop `while (choice < 99)`.
- **`menu_pulldown()` special returns**: `MENU_CANCEL=0` (ESC, if
  `escapable=1`), `MENU_LEFT_ARROW=18`/`MENU_RIGHT_ARROW=19` (switch bar
  item), else 1..N = selected item (1-indexed).
- **Window save/restore is main-RAM, not overlay RAM**: `menu_winsave()`/
  `menu_winrestore()` use a static `menu_winbuf[MENU_WINBUF_SIZE=2048]` LIFO
  buffer (`MENU_WIN_DEPTH=9` nested saves max), unlike locifilemanager-v2's
  `cwin_push`/`cwin_pop` (overlay RAM). This makes menus close with no residue
  in plain Oricutron (no LOCI/`--loci-flash` needed) — correct for this
  project where overlay RAM is optional (Phase 8 undo only). Largest nested
  path so far (bar + Screen pulldown + resize popup + shrink-confirm popup +
  Yes/No pulldown) = 1080B, well within the 2048B budget.
- **`menu_winsave()`'s third parameter** (`swap_charset`, 0 or 1): opts the
  popup in or out of the charset-swap mechanism (see "Charset-swap
  mechanism" above). `menu_winrestore()` pairs the matching
  `charsetswap_enter()`/`exit()` automatically via the saved `MenuWinRecord`,
  so enter/exit can't drift out of sync. Every current call site passes `1`
  except `charsetedit_run()`, which passes `0`.
- **Canvas resize** (`canvas_resize()`, dispatched from `menudata.c`
  `resize_dialog()`): new size validated as
  `(newval >= minval) && (neww*newh <= CANVAS_MAX_SIZE=8192)`, where
  `minval = VIEWPORT_WIDTH` (40) for width, `VIEWPORT_HEIGHT` (27) for height
  — both equal the *default* canvas size, so a dimension can only be shrunk
  below its default after first being grown above it. Shrinking below the
  *current* (already-grown) size triggers `menu_areyousure()` (Yes applies,
  No leaves the size unchanged).

## Phosphoric Testing Notes

Beyond `tests/scripts/test_boot.sh`'s pattern (see that file), Phase 2 added
`test_menus.sh`/`test_screenresize.sh` which drive the menu via
`--type-keys`. Key things to know when writing further `--type-keys`-based
tests:

- **`\fN` (FUNCT+N, N=0-9)** is a Phosphoric escape **added locally to the
  PHOSDIR fork** (`/home/xahmol/git/Phosphoric`, committed as `a7fc243`,
  v1.16.85-alpha) — not yet upstreamed. It holds FUNCT (sentinel `0x84`) and
  digit N together. If `make test-menus`/`test-screenresize` start failing
  with "FUNCT does nothing", check whether PHOSDIR points at a checkout with
  this patch.
- **`+`/`=` keys were unmapped in Phosphoric's `char_map`** (`0x2B`/`0x3D`
  entries used matrix position `(7,7)`, which is unused in both Oric and OSE
  decode tables — should be `(7,6)`, the actual `=`/`+` key). Fixed locally in
  the PHOSDIR fork (`src/io/keyboard.c`, 2-line fix, Phase 3b) — without it,
  `\p1+`/`\p1-`/`\p1=` in `--type-keys` silently do nothing (the `-` key
  already worked, mapped correctly to `(3,3)`). If `+`/`-`/`=` stop affecting
  `ce_code` in `test-charsetedit`, check whether PHOSDIR has this patch.
- **`\p1` before every distinct key/combo action is mandatory.** OSE's
  `keyb_poll()` (`include/keyboard.c`) sets `release_count=RELEASE_DEBOUNCE
  (20)` when a key is first accepted, and only decrements it on a frame with
  no key pressed. Without a `\p1` (pause 1s, releases all keys) between
  consecutive `--type-keys` actions, `release_count` never reaches 0 and the
  next key is silently dropped.
- **Cycle-count formula**: `total_cycles ≈ 8,080,000 (boot + splash-dismiss)
  + N_pairs × 1,100,000 (each "\p1+action" pair) + 300,000 (render/dump
  margin)`, where `N_pairs` = number of distinct `\p1`-prefixed actions.
- **`oric_screen.py --find` searches *stripped* text**, where attribute
  bytes (0x00-0x1F, rendered as extra spaces in the unstripped `--row`
  view) are removed entirely rather than replaced with a space. A run of
  "word `<attr>` word" in `--row` output (shown as "word  word", two spaces)
  collapses to "word word" (one space) in `--find`'s haystack. Always derive
  `--find` needles from the *stripped* text (e.g. via a quick
  `load_grid()`/`row_text(stripped=True)` check), not from `--row` output.
- **`cwin_textinput`** (`include/charwin.c`) cursor starts at `idx=strlen`
  (end of "40"/"60"); typed chars *overwrite* at the cursor and advance it.
  `KEY_DEL` (0x7F) is unmapped in Phosphoric's `char_map`, so to change "40"
  to "60", navigate left twice (`\l\l` → idx=0) then type the new leading
  digit (overwrites '4').
- **`cwin_putat_printf`'s `%x`/`%02x` produce UPPERCASE hex digits**
  (`include/charwin.c`'s `_cwin_vformat()`: `'A'+d-10`), unlike Python's
  `f"{b:02x}"` (lowercase) used by `oric_screen.py --bytes`. When asserting
  on-screen hex text via `--find` (e.g. a statusbar `C3F?` readout), use
  UPPERCASE; when asserting raw screen-RAM bytes via `--bytes` (e.g. `"3f"`),
  use lowercase. These are two different rendering paths for the same value.
- **`oric_screen.py --bytes ADDR:LEN` dumps RAW bytes, unmasked** — unlike
  `--find`/`--row`'s `load_grid()`, which applies `byte & 0x7F` to strip
  attribute bytes and the inverse-video bit. A highlighted grid cell
  (screencode XOR 0x80, e.g. the cursor cell in `charsetedit`/`palette`/
  `colourpicker`) shows as `code|0x80` in `--bytes` but as the plain
  `code&0x7F` character in `--find`/`--row`.
