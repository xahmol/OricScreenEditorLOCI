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

**Status: Phase 3 of 9 complete**, plus charset-swap infrastructure, a narrow
charset-editor popup, and EN/FR localisation (Phase 4 prep; canvas data model
+ minimal main mode + menu bar/Screen menu + character editor + charset-swap
mechanism + Phosphoric test harness; binary 11382 bytes EN / 11435 bytes FR).
A previous CC65-based attempt at this got stuck and is archived
in the `nonworkingcc65` branch (full history + uncommitted state). `main` was
restarted from scratch on the Oscar64 native/bare-metal build chain developed
for [locifilemanager-v2](https://github.com/xahmol/locifilemanager-v2)
(`/home/xahmol/git/locifilemanager-v2`), which the runtime and libraries below
were copied from verbatim. The remaining 6 phases (palette/colour picker,
select/move/line-box/write modes, LOCI file I/O, file picker, overlay-RAM
undo, IJK/help/polish) are tracked in the active plan — see
`~/.claude/plans/snappy-beaming-lynx.md` or ask Claude for status.

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
  `src/menu.c`), and `resize_dialog()` (`src/menudata.c`) all pass `1`;
  `charsetedit_run()` (`src/charsetedit.c`) passes `0`. Future popups (Phase
  4 palette/colour picker etc.) should pass `1` unless they have the same
  live-charset-RAM preview requirement as the character editor.

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
                  # test-screenresize, test-charsetram-spike, test-charsetedit)
                  # -- EN only, see "Localisation" below
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
  statusbar.c/h   Row-27 statusbar (OricCharWin, Mode/X,Y/C/S readout)
  editor.c/h      Main-mode loop: cursor move, +/- screencode select, SPACE/DEL plot,
                  FUNCT+1 opens the menu bar, 'e' opens the character editor
  menu.c/h        Menu bar/pulldown/popup engine + main-RAM window-save stack
  menudata.c/h    OSE menu tables (Screen/File/Charset/Information) + Screen
                  pulldown dispatch (Width/Height/Clear/Fill)
  charsetedit.c/h Character editor popup: edits the 6x8 glyph for
                  app.plotscreencode/app.plotaltchar directly in live charset
                  RAM (Strategy A -- see "Character editor (Phase 3)")
  charsetswap.c/h Charset-swap mechanism: backs up/restores CHARSET_STD around
                  popups so chrome renders with ROM glyphs (see "Charset-swap
                  mechanism")
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
