# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OricScreenEditorLOCI is a from-scratch **Oscar64** rewrite of
[OricScreenEditor](https://github.com/xahmol/OricScreenEditor) (V1, CC65,
local checkout at `/home/xahmol/git/OricScreenEditor`) — a character-set-aware
screen editor for the Oric Atmos — extended with support for the **LOCI mass
storage device**.

**LOCI is required to run the program at all.** The canvas (`screenmap[]`) lives
in LOCI-backed overlay RAM at `$C000-$E7FF`. `main()` hard-gates on `loci_present()`
before anything else. `make run` (Oricutron) can only show the LOCI-absent gate;
use Phosphoric `--loci` or real hardware for everything past it.

**Feature set (complete):** Canvas-edit undo/redo, LOCI file I/O (File+Charset
menus) with XRAM-backed directory browser, charset-swap mechanism, narrow charset
editor popup, palette mode, colour picker, Select/Move/Line-Box/Write modes,
G/I/O/U/R attribute keys, cursor auto-scroll for oversized canvases, canvas undo/
redo backed by overlay RAM, Select cut/copy, attribute-selection keys, statusbar
(auto-hides at last row), IJK joystick input (universal `key_read()`), FUNCT+8
help screens (4 topic screens), Information menu (Version/credits/QR + Exit via
RESET vector), EN/FR localisation, Try mode, Goto/Home, hollow-box + ellipse/circle
toggle in Line/Box mode, Charset > Reset Std->ROM / Reset Alt->ROM, unified
Find/Replace, hex-direct attribute entry in Write mode (FUNCT+4).

**Other docs:** `ARCHITECTURE.md`, `libmanual.md`/`libmanual_fr.md`, `README.md`/
`README.pdf`/`README_fr.md`, `oscar64manual.md` (Oscar64 compiler reference).
V1 reference: `/home/xahmol/git/OricScreenEditor/src/main.c`.

**V1 as literal reference:** When porting or adapting V1 behavior, **read its
actual code** before designing — do not substitute an assumed-simpler
approximation. See `feedback_dont_oversimplify_ported_designs.md` in project memory.

## Detailed Feature Documentation

@docs/canvas.md
@docs/charsetedit.md
@docs/menus.md
@docs/fileio.md
@docs/boot.md

## Code Style

Every C function definition (including `static` helpers) gets a Javadoc-style
`/** ... */` doc comment immediately above it, in the `.c` file (not the `.h`
declaration): a prose description, then `@param name  Description` for each
parameter and `@return Description` (`@return (none)` for `void`). See
`libmanual.md` for worked examples. This is a cross-project convention.

## Compiler Toolchain

This is an **Oscar64** project. `oscar64manual.md` is in the project root —
consult it for compiler flags, pragmas, language extensions, and gotchas. The
Oscar64 binary is at `$OSCAR64_HOME/bin/oscar64` (Makefile defaults to `~/oscar64`).

Target: 6502A (Oric Atmos), bare-metal. No VIC-II, SID, or C64 Kernal.

**Critical Oscar64 gotchas:**
- `va_arg` is broken in native mode (`-n`) — do not use `<stdarg.h>`/`va_list`/`va_arg`.
- `#if MACRO` fails when macro is defined via `-d` flag (no value) — use `#ifdef MACRO`.
- `(type)struct.member` — Oscar64 applies cast before member access; use a temp variable.
- Macro that expands to a volatile read in a braces-free for-loop body causes a parse
  error; use a braced body with an explicit temp.
- Ternary `? ptr : 0` in a pointer-returning function — use `if`/`return` instead.
- **Static stack usage** is the sum of all simultaneously-live function locals along
  a call path (not a runtime stack). Large buffers (~64-120 bytes) that appear in
  deep call paths must be **module-static, not function-local** (`error 3034: Static
  stack usage exceeds stack segment` otherwise).
- **`-O2` whole-program register allocator:** caller-save set can be under-counted
  when a function grows. Symptom: garbage appears on screen despite not touching
  display code. Fix: extract helpers to reduce function size, and use a
  linker-allocated static scratch buffer (never a magic address) for the residual
  pressure. See `oscar64manual.md` "Register allocator" section.

## Build and Run

```
make              # build build/oseloci.tap (English, LANG=EN default)
make LANG=FR      # build build/oseloci_fr.tap (French)
make all-langs    # build both
make run          # build + launch in Oricutron (shows LOCI-absent gate only)
make usb          # build both languages + kbtest/, copy to USBPATH (set in .env)
make clean        # remove build artifacts (both languages)
make docs         # regenerate README.pdf from README.md (requires pandoc)
make test         # full automated Phosphoric test suite -- EN only
                  # Targets: test-boot, test-menus, test-screenresize,
                  # test-charsetram-spike, test-charsetedit, test-palette,
                  # test-colourpicker, test-cursor-autoscroll, test-linebox,
                  # test-select, test-move, test-writemode, test-boot-no-loci,
                  # test-select-cutcopy, test-undo-overflow, test-help-funct8,
                  # test-fileio-traffic, test-findreplace, test-write-hexattr,
                  # test-trymode, test-goto, test-hollowbox, test-ellipse
                  # All pass --loci except test-boot-no-loci (tests absent path)
                  # and test-fileio-traffic (uses --loci-flash DIR).
                  # Current total: 200/200.
make test-boot    # headless boot smoke test
make test-capture CYCLES=N TYPEKEYS='...'
                  # calibration helper: dumps tests/out/capture.bin + .png
```

Compiler flags: `-n -tf=bin -rt=include/oric_crt.c -i=include -i=src -O2 -dNOFLOAT $(LANGFLAG)`

Emulator: `$ORICUTRON_HOME/oricutron -ma --serial none --vsynchack off --turbotape on`

### Phosphoric test harness

`make test*` uses [Phosphoric](https://github.com/benedictemarty/Phosphoric)
(`oric1-emu` + `roms/basic11b.rom`) to fast-load `oseloci.tap` headless. Set
`PHOSDIR` in `.env`. `tests/fixtures/` is checked in; `tests/sandbox/` and
`tests/out/` are gitignored scratch dirs regenerated by `make sandbox-reset`/`make test*`.

**Phosphoric CAN emulate a LOCI device** (alpha-quality). **Oricutron cannot.** Use
Phosphoric for headless coverage of actual byte-level behaviour, not just the
LOCI-absent path. Real Oric Atmos hardware remains the authoritative check.

`--loci-flash DIR` points LOCI's file ops at a real host filesystem directory —
lets test scripts assert on actual bytes a Save action writes.

## Memory Layout (`include/oric_crt.c`)

- `$0000-$00FF` — Zero page (Oscar64 internal registers)
- `$0100-$01FF` — 6502 hardware stack
- `$0200-$04FF` — Oric ROM system variables (do not use)
- `$0500-$057F` — Startup region (tape entry point → `oric_startup`)
- `$0580-$B1FF` — Program code, data, BSS (~44.1 KB; `heap` section dropped — `malloc`/`free` are fully stubbed)
- `$B200-$B3FF` — Oscar64 software stack (512 bytes)
- `$B400-$BBFF` — Character set RAM (`CHARSET_STD`=$B400, `CHARSET_ALT`=$B800-$BB7F)
- `$BB80-$BFFF` — Screen RAM (40×28 = 1120 bytes)
- `$C000-$E7FF` — Overlay RAM: **canvas** (10240 bytes, `CANVAS_REGION_BASE`/`CANVAS_MAX_SIZE`)
- `$E800-$FFFF` — Overlay RAM: **undo ring** (6144 bytes, `UNDO_REGION_BASE`/`UNDO_REGION_SIZE`)
- `$C000-$FFFF` — ROM when overlay RAM is disabled

**Overlay RAM is enabled once for the whole session** (`enable_overlay_ram()` called
right after the LOCI boot gate in `main()`). Do NOT add `enable_overlay_ram()`/
`disable_overlay_ram()` brackets inside undo/canvas functions.

**Two places that need to borrow ROM briefly:**
1. `charsetswap_enter()`'s `charset_load(CHARSET_STD, CHARSETROM)` — CHARSETROM is
   at $FC78 (inside $C000-$FFFF). Bracket with `disable_overlay_ram()`/`enable_overlay_ram()`.
2. `charsetedit.c`'s `'s'` key (`charset_rom_glyph()`) — same bracket.
3. `info_exit()`'s `jmp ($fffc)` — call `disable_overlay_ram()` immediately before.

**IRQ convention:** interrupts disabled (`SEI`, no `CLI`). VIA Port A access in
`ijk.c` uses `PHP; SEI ... PLP`, never unconditional `CLI` (would permanently
re-enable IRQs and let ROM handler corrupt zero page/screen RAM).

**`$CHARSET_ALT` RAM boundary:** `$B800-$BB7F` = 896 bytes. Only codes 0x00-0x6F
× 8 bytes = 640 bytes are valid for charset file I/O (`CHARSET_ALT_GLYPH_AREA_SIZE`).
Codes 0x70-0x7F exist but their glyph RAM overlaps screen RAM — the character
editor caps Alt at `ce_max_code()` = 0x6F.

## Source Layout

```
src/
  main.c          Entry point: LOCI gate, enable_overlay_ram(), homedir/locicfg init,
                  boot snapshot, title splash, canvas/statusbar init, editor_run()
  appstate.h      Global AppState struct
  canvas.c/h      screenmap[] (overlay RAM pointer macro) + raw $BB80 blit,
                  cursor_move_scroll(), canvas_resize(), canvas_resize_loaded(),
                  canvas_goto(), canvas_rowbuf[]
  statusbar.c/h   Row-27 statusbar (OricCharWin), auto-hide at STATUSBAR_ROW,
                  statusbar_set/clear_override() for secondary-option hints
  editor.c/h      Main-mode loop: all key bindings, cursor move, Try/Goto/Home/Find
  modes.h/c       EditorMode → MSG_MODE_* display-name lookup (mode_name())
  select.c/h      rect_select() + linebox_run() + select_run() + ellipse helpers
  move.c/h        Move mode
  write.c/h       Write mode + write_hex_attr()
  menu.c/h        Menu bar/pulldown/popup engine + main-RAM window-save stack
                  menu_regpressure_scratch[] — linker-allocated, not a magic address
  menudata.c/h    OSE menu tables + pulldown dispatch + resize_dialog() + goto_dialog()
  findreplace.c/h Unified Find/Replace
  fileio.c/h      LOCI file I/O (Screen/Combined/Project/Charset save/load)
                  fileio_parse_v1_project(), fileio_strip_bin_suffix()
                  module-static fullpath buffer
  filepicker.c/h  XRAM-backed directory browser, picker_engine(), PICKER_MODE_SAVE
                  picker_regpressure_scratch[] — linker-allocated, not a magic address
  homedir.c/h     homedir_init(), homedir_join(), filedir_join(), filedir_join_suffix()
  undo.c/h        Canvas-edit undo/redo, overlay-RAM ring buffer
  input.c/h       key_read() — keyboard + IJK joystick, replaces cwin_getch() everywhere
  help.c/h        FUNCT+8 help screens (OSEHS1-4.BIN, runtime LOCI loads)
  info.c/h        Information menu: Version popup + Exit (RESET vector)
  charsetedit.c/h Character editor sidebar popup (live charset RAM, Strategy A)
  charsetswap.c/h Charset-swap mechanism (both banks), charsetswap_real_std/alt()
  palette.c/h     Palette popup ('p')
  colourpicker.c/h Colour picker popup ('c'), direct screen-RAM grid writes
  strings.h       Localisation gateway (#include strings_en.h or strings_fr.h)
  strings_en.h    All MSG_* strings, English
  strings_fr.h    All MSG_* strings, French (no accented characters)
include/
  oric.h          Hardware constants (VIA, AY, screen, overlay RAM, ASTR_*/CH_*/A_*)
  oric_crt.c      Custom Oscar64 runtime
  crt_math.c      Math/float runtime
  keyboard.h/c    Direct VIA/AY keyboard scanner (no ROM calls), decode_funct[]
  charwin.h/c     Character window/menu library; cwin_clear_full() added by this project
  charset.h/c     Oric charset addressing/copy + glyph transforms; charset_area_size()
  loci.h/c        LOCI mass storage API
  ijk.h/c         Raxiss IJK joystick driver
tools/
  mktap.py        Wraps .bin in Oric tape header
tests/
  scripts/        oric_screen.py + test_*.sh (Phosphoric harness)
  fixtures/       Checked-in assets copied into tests/sandbox/ by sandbox-reset
kbtest/           Standalone keyboard matrix diagnostic tool (separate build)
build/
  oseloci.tap     Output tape image (gitignored)
```

Library files (`oric_crt.c`, `crt_math.c`, `oric.h`, `charwin.*`, `keyboard.*`,
`ijk.*`, `loci.*`) copied verbatim from locifilemanager-v2.

## Localisation

Every user-visible string is a `MSG_*` macro selected at compile time via `src/strings.h`.
`strings_fr.h` uses **no accented characters** (Oric ROM charset has none).

`LANG ?= EN`; `LANG=FR` sets `-dLANG_FR` and output suffix `_fr`. `make all-langs`
builds both. `make test` runs EN build only.

**Key bindings are independent of `MSG_*`** — hardcoded chars in switch statements,
identical across languages.

**Menu-bar layout constraint:** `sum(strlen(titles)) + 7 <= 39`. EN total = 28 (4 slack);
FR uses `"Caract."` (abbrev of Caracteres) to stay at 30. Any new bar title must keep
the 4-item sum <= 32.

`MENUBAR_MAXLENGTH=12` (11 visible + NUL); `PULLDOWN_MAXLENGTH=17` (16 visible + NUL).

## Oric Screen Model

- **40×28 character grid**, screen RAM at `$BB80`
- **Serial attributes:** byte with `(byte & 0x60) == 0` is an attribute (0x00-0x1F
  only). `0x20` (space) is a character, not an attribute. ULA resets to white-ink/
  black-paper at the start of each raster line, then applies attrs left-to-right.
  **Charset-mode latch (`A_STD`/`A_ALT`, values 8/9) does NOT reset per-line** —
  it persists across rows until the next charset-mode attribute byte. Always write
  `A_STD` before popup text that must render in the standard charset.
- **Inverse video:** `ch | 0x80` inverts pixel rendering; `CH_INVSPACE` (0xA0) =
  solid ink-coloured block.
- **Attribute byte order in column 0-1:** the Oric ULA reads attributes strictly
  left-to-right. `cwin_clear()`/`row_setattr()` writes `[ink, paper]` at cols 0-1
  (this codebase's convention). V1's own colour picker writes `[paper, ink]` — the
  colour picker popup uses `cp_draw_border()` to override to V1's order.

## Phosphoric Testing Notes

- **`\FN` (FUNCT+N, N=0-9)** is upstream Phosphoric's own escape (Sprint 59,
  v1.20.0-alpha+). Use this, not `\fN`. If FUNCT keys do nothing, check PHOSDIR
  is current upstream `benedictemarty/Phosphoric` main and `--type-keys` has no
  `loci-hid:` prefix.
- **`\p1` before every distinct key/combo action is mandatory.** `keyb_poll()`'s
  `release_count=RELEASE_DEBOUNCE(20)` must drain to 0 between actions.
- **Cycle-count formula:** `≈ 8,080,000 (boot+splash) + N_pairs × 1,100,000 + 300,000`
  where `N_pairs` = number of distinct `\p1`-prefixed actions.
- **`oric_screen.py --find` searches stripped text** — attribute bytes (0x00-0x1F)
  are removed entirely (not replaced with a space). "word `<attr>` word" → "word word".
- **`cwin_textinput`** cursor starts at `idx=strlen` (end of buffer). Typed chars
  overwrite at cursor and advance. `KEY_DEL` (0x7F) is unmapped in Phosphoric's
  `char_map`. To change "40" to "60": `\l\l` (idx=0) then type new leading digit.
- **`cwin_putat_printf`'s `%x`/`%02x` produce UPPERCASE hex digits** (`'A'+d-10`).
  `--find` needles → UPPERCASE; `--bytes` raw dump → lowercase.
- **`oric_screen.py --bytes ADDR:LEN` dumps RAW bytes, unmasked** — highlighted
  grid cells show as `code|0x80`, not `code&0x7F`.
- **`+`/`=` keys in Phosphoric's `char_map`: correct mapping is `(7,7)` (`U(7,7)`/
  `S(7,7)`) — do NOT change to `(7,6)`.**  Phosphoric's `char_entry_t{col,row}` uses
  opposite axis labelling from this project's `keyb_matrix[row]` (bit=col).
  Anchor: `RETURN` = Phosphoric `(7,5)` = our row7/col5.
- **`cwin_textinput` `VINPUT_ALPHA` does NOT include digits** — use
  `VINPUT_NUMS | VINPUT_ALPHA` for hex fields.
- **`vwidth` must be > `maxlen`** in `cwin_textinput` calls — `vwidth == maxlen`
  causes a display-only viewport-scroll bug (typed digits appear to vanish).
