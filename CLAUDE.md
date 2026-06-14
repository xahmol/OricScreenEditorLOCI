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

**Status: Phase 2 of 9 complete** (canvas data model + minimal main mode +
menu bar/Screen menu + Phosphoric test harness; binary 9325 bytes). A previous
CC65-based attempt at this got stuck and is archived in the `nonworkingcc65`
branch (full history + uncommitted state). `main` was restarted from scratch
on the Oscar64 native/bare-metal build chain developed for
[locifilemanager-v2](https://github.com/xahmol/locifilemanager-v2)
(`/home/xahmol/git/locifilemanager-v2`), which the runtime and libraries below
were copied from verbatim. The remaining 7 phases (character editor,
palette/colour picker, select/move/line-box/write modes, LOCI file I/O, file
picker, overlay-RAM undo, IJK/help/polish) are tracked in the active plan —
see `~/.claude/plans/snappy-beaming-lynx.md` or ask Claude for status.

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
2-attr-column model.

**Feature-set reference:** `README.md` / `README.pdf` and `screenshots/`
describe V1's UI and feature set (main mode, character editor, palette mode,
select/move/line-box/write modes, file format) — this is the spec the rewrite
needs to (eventually) match and then extend. `assets/*.bin` (charset/title/
help screens, Petscii charset) are Oric-format binaries carried over from V1
and may be reusable as-is.

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
make              # build build/oseloci.tap
make run          # build + launch in Oricutron
make clean        # remove build artifacts
make docs         # regenerate README.pdf from README.md (requires pandoc)
make test         # full automated Phosphoric test suite (currently: test-boot)
make test-boot    # headless boot smoke test (splash + canvas/statusbar render)
make test-capture CYCLES=N TYPEKEYS='...'
                  # calibration helper: dumps tests/out/capture.bin + .png
```

Compiler flags: `-n -tf=bin -rt=include/oric_crt.c -i=include -i=src -O2 -dNOFLOAT`

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
                  FUNCT+1 opens the menu bar
  menu.c/h        Menu bar/pulldown/popup engine + main-RAM window-save stack
  menudata.c/h    OSE menu tables (Screen/File/Charset/Information) + Screen
                  pulldown dispatch (Width/Height/Clear/Fill)
  strings.h       User-visible message strings (MSG_* macros required by loci.c)
include/
  oric.h          Hardware constants (VIA, AY, screen, overlay RAM, ASTR_*/CH_*/A_* attrs)
  oric_crt.c      Custom Oscar64 runtime: startup, regions, math stubs
  crt_math.c      Math/float runtime routines for the custom runtime
  keyboard.h/c    Direct VIA/AY keyboard scanner — no ROM calls
  charwin.h/c     Character window / menu library
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
`src/strings.h` here is a minimal placeholder (just the two `MSG_*` macros
`loci.c`'s `get_locicfg()` needs) — locifilemanager-v2's full EN/FR
`strings.h`/`strings_en.h`/`strings_fr.h` localisation system was deliberately
**not** copied; revisit if/when localisation is needed.

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
  PHOSDIR fork** (`/home/xahmol/git/Phosphoric`, uncommitted as of Phase 2) —
  not yet upstreamed. It holds FUNCT (sentinel `0x84`) and digit N together.
  If `make test-menus`/`test-screenresize` start failing with "FUNCT does
  nothing", check whether PHOSDIR points at a checkout with this patch.
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
