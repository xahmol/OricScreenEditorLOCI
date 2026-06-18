# ARCHITECTURE.md

Technical architecture reference for **OricScreenEditorLOCI** (OSE) — a
character-set-aware screen editor for the Oric Atmos, built with the Oscar64
C compiler, extended with support for the LOCI mass-storage device. This
document describes the memory map, the main data structures, the module
layout, and the application's runtime control flow. For compiler gotchas,
day-to-day build/test commands and phase-by-phase design rationale, see
`CLAUDE.md` and `oscar64manual.md`. For full per-function API documentation
of the project's libraries (`oric.h`, `keyboard.h`, `charwin.h`, `ijk.h`,
`loci.h`, `charset.h`, `menu.h`), see `libmanual.md` (English) /
`libmanual_fr.md` (French).

OSE is a from-scratch Oscar64 rewrite of
[OricScreenEditor](https://github.com/xahmol/OricScreenEditor) (V1, CC65,
local checkout at `/home/xahmol/git/OricScreenEditor`), restarted on the
Oscar64 native/bare-metal build chain shared with
[locifilemanager-v2](https://github.com/xahmol/locifilemanager-v2)
(`/home/xahmol/git/locifilemanager-v2`), which this document's structure is
adapted from. **All 9 phases of the rewrite are complete.**

---

## 1. Technical Setup

### 1.1 Target platform

- **CPU:** 6502A @ 1 MHz (Oric Atmos)
- **No ROM calls** for application logic — bare-metal. No VIC-II/SID/Kernal
  (C64 concepts, do not apply here). Screen, keyboard and (eventually) LOCI
  storage are driven directly via memory-mapped registers. The one exception
  is `CHARSETROM` (`$FC78`), a ROM data table read (not called) by the
  character editor's "restore from ROM" command — see §2.1.
- **No interrupts.** Startup executes `SEI` and never `CLI`s. The keyboard
  scanner is pure-poll, so no IRQ handler is installed. Any code that must
  briefly enable interrupts (overlay-RAM access, IJK VIA Port A access — both
  currently dormant, see §2.4) uses `PHP`/`SEI`/`PLP`, never `SEI`/`CLI` —
  see §2.5.

### 1.2 Toolchain

- **Compiler:** [Oscar64](https://github.com/drmortalwombat/oscar64), native
  mode (`-n`), binary target (`-tf=bin`).
- **Custom runtime:** `include/oric_crt.c` (passed via `-rt=`) — defines
  memory regions, the startup routine, and pulls in `crt_math.c` for
  integer/float runtime helpers. Copied verbatim from locifilemanager-v2.
- **Flags:** `-n -tf=bin -rt=include/oric_crt.c -i=include -i=src -O2
  -dNOFLOAT`, plus `-dVERSION_MAJOR/MINOR/PATCH`.
- **Tape wrapping:** `tools/mktap.py` wraps the Oscar64 `.bin` output in an
  Oric tape header to produce `.tap` files.
- **Emulator:** Oricutron (`make run`) — LOCI features (once wired up) are
  **not** emulated by Oricutron and will require real hardware.
- **Headless test emulator:** [Phosphoric](https://github.com/benedictemarty/Phosphoric)
  (`oric1-emu`) — fast-loads `oseloci.tap` under Atmos BASIC 1.1 and asserts
  on `$BB80` screen-RAM dumps via `--type-keys` + `--dump-ram-at`. This is
  what `make test*` runs against (§7). Unlike Oricutron, **Phosphoric can
  emulate a LOCI device** (alpha-quality) — so LOCI-dependent features
  are not necessarily real-hardware-only for testing; real hardware
  stays the authoritative check given Phosphoric's LOCI emulation's
  alpha status.

### 1.3 Build artifacts

| Target | Output |
|---|---|
| `make` (`LANG=EN`, default) | `build/oseloci.tap` |
| `make LANG=FR` | `build/oseloci_fr.tap` |
| `make all-langs` | both of the above |
| `make run` | builds, then launches `build/oseloci$(LANGSUFFIX).tap` in Oricutron |
| `make docs` | regenerates `README.pdf` from `README.md` (requires pandoc) |

EN/FR localisation is implemented (`src/strings.h`/`strings_en.h`/
`strings_fr.h`, `-dLANG_FR`/`LANGSUFFIX=_fr`), matching locifilemanager-v2's
EN/FR convention — see `CLAUDE.md` "Localisation". `make test*` assertions
are EN-only.

---

## 2. Memory Map

OSE uses the same custom Oscar64 runtime (`include/oric_crt.c`) as
locifilemanager-v2, copied verbatim.

### 2.1 Main CPU address space ($0000–$FFFF)

| Range | Size | Contents |
|---|---|---|
| `$0000–$00FF` | 256 B | Zero page — Oscar64 internal register file |
| `$0100–$01FF` | 256 B | 6502 hardware stack (`txs #$FF` at startup) |
| `$0200–$02FF` | — | Oric ROM system variables (reserved, not used) |
| `$0300–$030F` | 16 B | **VIA 6522** registers (§2.2) — used by `include/keyboard.c` |
| `$0310–$04FF` | — | Remaining Oric ROM system variables, incl. `MICRODISCCFG` (`$0314`, Oric-side overlay-RAM enable, now wired in -- Phase 8, `src/undo.c`) and the LOCI TAP/MIA register block, incl. `$0319` (`LOCI_SIGNATURE_ADDR`, `loci_present()`'s detection byte) -- `include/loci.c/h`'s file-I/O API (`src/fileio.c`, Phase 6), LOCI-device XRAM API (`src/filepicker.c`, Phase 7 -- a different resource from the Oric-side overlay RAM, see §2.4), and Oric-side overlay-RAM API (`src/undo.c`, Phase 8) are all now compiled into OSE |
| `$0500–$057F` | 128 B | **Startup region** — tape entry point jumps into `oric_startup` |
| `$0580–$B1FF` | ~42.4 KB | **Program region** — code, data, BSS, heap |
| `$B200–$B3FF` | 512 B | **Software stack** (`#pragma stacksize(0x0200)`) |
| `$B400–$B7FF` | 1 KB | `CHARSET_STD` — standard charset bank, codes `$00`-`$7F` (live, ULA-rendered; edited directly by the character editor, "Strategy A") |
| `$B800–$BB7F` | 896 B | `CHARSET_ALT` — alternate charset bank, codes `$20`-`$6F` editable (`CHAREDIT_ALT_COUNT=80`, vs. std's 96) — codes `$70`-`$7F` of the nominal 128-glyph bank would land on `$BB80`-`$BBFF`, which is screen RAM, so the alt charset's usable range is deliberately 16 codes shorter than std's |
| `$BB80–$BFFF` | 1120 B | **Text screen RAM** (`TEXTVRAM`) — 40 cols × 28 rows |
| `$C000–$FFFF` | 16 KB | Atmos ROM (normal); **overlay RAM** when LOCI enables it via `MICRODISCCFG` — not yet used by OSE (planned for Phase 8 undo buffers) |
| `$FC78–$FF77` | 768 B | `CHARSETROM` — ROM's resident standard-charset table, codes `$20`-`$7F`, addressed as `CHARSETROM+(code-$20)*8` (opposite convention from `CHARSET_STD`/`CHARSET_ALT`'s `base+code*8`, which has no `-$20` offset) |

> **Hard ceiling:** `$B400` is OSDK's documented start of charset RAM and the
> ceiling for the program/data/stack region. Code, data, BSS, heap and the
> software stack must all stay below `$B400`.

### 2.2 VIA 6522 ($0300–$030F)

```c
typedef volatile struct {
    uint8_t prb;   // $0300  Port B: row select, tape/kbd-sense, printer strobe,
                   //        cassette motor, AY BC1/BDIR
    uint8_t pra;   // $0301  Port A (with handshake): AY data bus / printer data
    uint8_t ddrb;  // $0302  DDR B (ROM sets $F7)
    uint8_t ddra;  // $0303  DDR A
    uint8_t t1lo, t1hi, t1llo, t1lhi;  // $0304-$0307  Timer 1
    uint8_t t2lo, t2hi;                // $0308-$0309  Timer 2
    uint8_t sr;    // $030A  Shift register
    uint8_t acr;   // $030B  Auxiliary control register
    uint8_t pcr;   // $030C  Peripheral control register (CB2 = AY column drive)
    uint8_t ifr;   // $030D  Interrupt flag register
    uint8_t ier;   // $030E  Interrupt enable register
    uint8_t pra2;  // $030F  Port A without handshake (AY data bus writes)
} VIA_t;
#define VIA (*((VIA_t *)0x0300))
```

`include/keyboard.c` drives the AY-3-8912 keyboard matrix scan directly
through `VIA.prb`/`VIA.pra2`/`VIA.pcr` — no ROM calls. See `include/oric.h`
for the full AY register-write sequence.

### 2.3 Charset RAM and the character editor

`CHARSET_STD`/`CHARSET_ALT` are full 128-glyph tables (`addr = base +
code*8`, codes `$00`-`$7F`); the displayable range (codes `$20`-`$7F`, 96
glyphs = 768 bytes) lives at `base+$100..base+$3FF` and is what `CHARSETROM`
mirrors for the standard set. `include/charset.h/c` provides generic
addressing/copy primitives (`charset_address/save/load/rom_glyph`) and
in-place glyph-bitmap transforms (`invert/mirror_v/mirror_h/scroll_up/
scroll_down/rotate_left/rotate_right`) over this layout — see §4.5 and §6.4.

### 2.4 LOCI and IJK

`include/loci.h/c` (LOCI mass-storage MIA/TAP/XRAM/file/dir/mount/overlay-RAM
API) and `include/ijk.h/c` (Raxiss IJK joystick driver, VIA Port A) were
copied verbatim from locifilemanager-v2 as part of the Oscar64 scaffold
restart. **`loci.h/c` is now fully wired in**: `loci_present()`/
`loci_open/close/read/write()`/`file_save()`/`file_load()` (Phase 6,
`src/fileio.c/h` — see §6.14) back the File/Charset menus; `xram_peek/
poke/memcpy_to/memcpy_from()` (Phase 7, `src/filepicker.c/h` — see §6.15)
back the LOCI directory browser; and `enable_overlay_ram()`/
`disable_overlay_ram()` (Phase 8, `src/undo.c/h` — see §6.16) back canvas
undo/redo. **Important distinction**: XRAM is RAM *on the LOCI device
itself* — a completely different resource from the Oric-side
bank-switched overlay RAM ($C000-$FFFF) `enable_overlay_ram()`/
`disable_overlay_ram()` control. XRAM access needs no Oric memory
banking, so Phase 7 had no dependency on Phase 8 wiring up overlay RAM —
an earlier draft of the Phase 7 plan conflated the two; corrected during
implementation, and the same distinction came up again (and was corrected
again) while planning Phase 8's undo eviction policy — see CLAUDE.md's
"Canvas undo/redo (Phase 8)" section. `ijk.h/c` is now wired in too
(Phase 9a, `src/input.c/h` — see §6.17): `ijk_detect()`/`ijk_read()` back
`key_read()`, the universal keyboard+joystick blocking read that replaced
`cwin_getch()` everywhere.

### 2.5 Interrupt policy

Startup (`oric_startup` in `oric_crt.c`) executes `SEI` and never `CLI`s —
the keyboard scanner is pure-poll (`keyb_scan`/`keyb_poll`), so no IRQ
handler is installed and the ROM's IRQ chain at `$0245/$0246` is never
entered. **Convention:** any code that must transiently enable interrupts
(overlay-RAM access via `MICRODISCCFG`, or VIA Port A access in `ijk.c`)
brackets it with `PHP`/`SEI` ... `PLP`, never `SEI`/`CLI` — an unconditional
`CLI` would permanently re-enable IRQs (since no handler exists) and let the
stock ROM IRQ handler corrupt zero page / screen RAM on every frame
thereafter. Exercised by both `src/undo.c` (overlay-RAM access) and
`include/ijk.c` (VIA Port A access via `key_read()`, Phase 9a).

### 2.6 Screen model

- **40×28 character grid**, screen RAM at `$BB80` (1120 bytes). Rows 0-26 are
  the user's **canvas** (resizable, see §4.3); row 27 is the **statusbar**.
- A screen byte with `(byte & 0x60) == 0` (values `$00`-`$1F`) is a **serial
  attribute** (changes ink/paper/charset for the rest of the raster line,
  occupies one screen column as a paper-coloured box); `$20` (space) is a
  normal character. The ULA resets to white-ink/black-paper at the start of
  each raster line.
- Ink attrs `$00`-`$07` (`A_FW*`), paper attrs `$10`-`$17` (`A_BG*`), charset
  mode attrs `$08`-`$0F` (`A_STD`, `A_ALT`, `A_STD2H`, …).
- Inverse video: `ch | 0x80`. `CH_INVSPACE` (`$A0`) = solid ink-coloured
  block (canvas cursor).

---

## 3. Source Layout

```
src/
  main.c          Application entry point: splash, canvas/statusbar init, editor_run()
  appstate.h      Global AppState struct (canvas size, cursor, viewport, mode, ...)
  canvas.c/h      Flat 40x27 screenmap[] buffer + raw $BB80 blit (bypasses charwin),
                  canvas_resize() (up to CANVAS_MAX_SIZE)
  statusbar.c/h   Row-27 statusbar (OricCharWin, Mode/XY/C/S/I/P readout +
                  A/D/B attribute flags, see §6.2/§6.7/§6.8)
  editor.c/h      Main-mode loop: cursor move (auto-scroll via
                  canvas.c's cursor_move_scroll(), §6.2), +/- screencode
                  select, SPACE/DEL plot, ,/.;/' ink/paper cycling, b/d/a/r
                  attribute toggles, g grabs the screencode/attr under the
                  cursor, i/o/u plot ink/paper/modifier, 0-9/SHIFT+0-9
                  favourites, FUNCT+1 opens the menu bar, 'e' opens the
                  character editor, 'p'/'c' open the palette/colour picker,
                  'l'/'s'/'m'/'w' open Line-Box/Select/Move/Write mode
                  (§6.9-§6.13), z/y undo/redo (§6.16), FUNCT+8 shows help
                  (§6.18)
  modes.h/c       EditorMode -> MSG_MODE_* display-name lookup
                  (mode_name()), shared by statusbar.c and any mode file
  select.c/h      Shared rectangle-grower (rect_select()) + Line/Box mode
                  ('l') + Select mode ('s', incl. x/c cut/copy)
                  (§6.9/§6.10/§6.11/§6.17)
  move.c/h        Move mode ('m'): nudges visible canvas content (§6.12)
  write.c/h       Write mode ('w'): free-typing screencodes (§6.13)
  fileio.c/h      LOCI file I/O backing the File/Charset menus (§6.14)
  filepicker.c/h  XRAM-backed LOCI directory browser for every Load
                  action (§6.15)
  undo.c/h        Canvas-edit undo/redo ('z'/'y'), overlay-RAM-backed
                  dirty-rect ring buffer (§6.16)
  input.c/h       key_read(): universal keyboard+IJK-joystick blocking
                  read, replaces cwin_getch() everywhere (§6.17)
  help.c/h        FUNCT+8 help screens, #embed'd + LZO-compressed (§6.18)
  info.c/h        Information menu actions: Version (3-page popup) and
                  Exit (RESET vector) (§6.19)
  menu.c/h        Menu bar/pulldown/popup engine + main-RAM window-save stack
  menudata.c/h    OSE menu tables (Screen/File/Charset/Information) + Screen/
                  File/Charset/Information pulldown dispatch
  charsetedit.c/h Character editor popup: edits the 6x8 glyph for
                  app.plotscreencode/app.plotaltchar directly in live charset RAM
  charsetswap.c/h Charset-swap mechanism: backs up/restores CHARSET_STD around
                  popups so chrome renders with ROM glyphs
  palette.c/h     Palette popup ('p'): Fav/Std/Alt charset grid + visualchar[]
                  visual-charmap toggle (§6.7)
  colourpicker.c/h Colour picker popup ('c'): 8x8 ink x paper grid with
                  inverse-colour preview, new vs V1 (§6.8)
  strings.h       Localisation gateway (LANG_FR -> strings_fr.h, else strings_en.h)
  strings_en.h    User-visible MSG_* strings, English
  strings_fr.h    User-visible MSG_* strings, French (unaccented)

include/
  oric.h          Hardware constants (VIA, AY, screen, overlay RAM, ASTR_*/CH_*/A_* attrs)
  oric_crt.c      Custom Oscar64 runtime: startup, regions, math stubs
  crt_math.c      Math/float runtime routines for the custom runtime
  keyboard.h/c    Direct VIA/AY keyboard scanner -- no ROM calls
  charwin.h/c     Character window / menu library (windows, viewports, text input)
  charset.h/c     Generic Oric charset-bank addressing/copy + glyph-bitmap transform
                  primitives (new to this project, not copied from locifilemanager-v2)
  loci.h/c        LOCI mass-storage API -- fully wired in: file I/O via
                  src/fileio.c (§6.14), LOCI-device XRAM via
                  src/filepicker.c (§6.15), Oric-side overlay RAM via
                  src/undo.c (§6.16) -- see §2.4
  ijk.h/c         Raxiss IJK joystick driver -- wired in via src/input.c
                  (§2.4, §6.17)

tools/
  mktap.py        Wraps Oscar64 .bin output in an Oric tape header

tests/
  scripts/        oric_screen.py (decode $BB80) + test_*.sh (Phosphoric harness)
  fixtures/       Checked-in files copied into tests/sandbox/ by sandbox-reset
  sandbox/        Working copy of fixtures + built .tap (regenerated per run)
  out/            RAM dumps / screenshots from test runs

build/
  oseloci.tap     Output tape image (gitignored)
```

All library files (`oric_crt.c`, `crt_math.c`, `oric.h`, `charwin.*`,
`keyboard.*`, `ijk.*`, `loci.*`) were copied verbatim from locifilemanager-v2.
`include/charset.h/c` is new to this project.

---

## 4. Main Data Structures

### 4.1 `OricCharWin` and `OricViewport` (`include/charwin.h`)

```c
typedef struct {
    uint8_t sx, sy;   // start col (>=2), start row
    uint8_t wx, wy;   // width, height in characters
    uint8_t cx, cy;   // cursor position within window (0-based)
    uint8_t ink;      // A_FW* ink colour
    uint8_t paper;    // A_BG* paper colour
} OricCharWin;
```

Window convention: screen columns 0-1 of every managed row hold the
INK/PAPER attribute bytes; content occupies columns `sx..sx+wx-1`.
`charwin_init()` precomputes a row-address lookup table used by all `cwin_*`
functions. Used for all menu/popup chrome and the row-27 statusbar — **not**
for the canvas, which bypasses charwin entirely (§4.3).

```c
typedef struct {
    uint8_t     *sourcebase;   // flat source character map (row-major)
    uint16_t     sourcewidth;  // bytes per row in source (>= win->wx)
    uint16_t     sourceheight; // total rows in source
    uint16_t     viewx, viewy; // current scroll offset
    OricCharWin *win;          // target display window
} OricViewport;
```

Not currently used by OSE (no scrollable text view yet); carried over from
locifilemanager-v2 for future use.

### 4.2 `AppState` (`src/appstate.h`)

```c
#define VIEWPORT_WIDTH   40
#define VIEWPORT_HEIGHT  27
#define CANVAS_WIDTH     VIEWPORT_WIDTH
#define CANVAS_HEIGHT    VIEWPORT_HEIGHT
#define CANVAS_MAX_SIZE  8192
#define FAVOURITES_COUNT 10
#define FILENAME_MAXLEN  48

typedef enum {
    MODE_MAIN = 0,
    MODE_WRITE,
    MODE_SELECT,
    MODE_MOVE,
    MODE_LINEBOX
} EditorMode;

typedef struct {
    uint16_t   canvas_width;
    uint16_t   canvas_height;
    uint16_t   cursor_x;        // absolute canvas column
    uint16_t   cursor_y;        // absolute canvas row
    uint16_t   xoffset;         // viewport scroll offset (columns)
    uint16_t   yoffset;         // viewport scroll offset (rows)
    uint8_t    plotscreencode;  // character placed by SPACE
    uint8_t    plotaltchar;     // 0 = standard charset, 1 = alternate
    uint8_t    plotink;         // 0-7, ink colour to plot
    uint8_t    plotpaper;       // 0-7, paper colour to plot
    uint8_t    plotblink;       // 0/1, blink attribute to plot
    uint8_t    plotdouble;      // 0/1, double-height attribute to plot
    uint8_t    visualmap;       // 0/1, palette Alt-charset visual-charmap toggle
    uint8_t    favourites[FAVOURITES_COUNT]; // character-editor favourites
    EditorMode mode;
    uint8_t    showstatusbar;
    uint8_t    stdchanged;      // 0/1, CHARSET_STD edited this session
    uint8_t    altchanged;      // 0/1, CHARSET_ALT edited this session
    char       filename[FILENAME_MAXLEN + 1]; // LOCI base filename
} AppState;

extern AppState app;
```

`CANVAS_MAX_SIZE=8192` is a documented, revisable budget (~2.5x V1's
6655-byte CC65 ceiling), leaving headroom in the ~42.4 KB program region for
the menu engine and later phases' static buffers. `EditorMode` (Phase 5)
gained `MODE_WRITE`/`MODE_SELECT`/`MODE_MOVE`/`MODE_LINEBOX` alongside
`MODE_MAIN`, looked up via `mode_name()` (`src/modes.h/c`) for the
statusbar's Mode field. Select/Line-Box's own rectangle bounds
(`select_startx`/`endx`/`starty`/`endy`/`width`/`height`) are *not*
`AppState` fields -- they're file-scope statics in `select.c`, since
nothing outside that file reads them. The Phase 4a fields (`plotink`/
`plotpaper`/`plotblink`/`plotdouble`/`visualmap`) default to
`A_FWWHITE`/`A_FWBLACK`/`0`/`0`/`0` (set in `editor_run()`'s init block) and
are read/written by the main-mode attribute-selection keys, the statusbar,
the palette/colour-picker popups, and the Phase 5 modes (§6.2, §6.7-§6.12).

### 4.3 Canvas (`src/canvas.h`)

```c
extern uint8_t screenmap[CANVAS_MAX_SIZE];

#define CANVAS_MAX_ROW 320
extern uint8_t canvas_rowbuf[CANVAS_MAX_ROW];

void canvas_init(void);
void canvas_clear(void);
void canvas_fill(uint8_t value);
uint8_t canvas_get(uint16_t x, uint16_t y);
void canvas_put(uint16_t x, uint16_t y, uint8_t value);
void canvas_blit(void);
void canvas_cell_invert(uint16_t x, uint16_t y);
uint8_t canvas_resize(uint16_t neww, uint16_t newh);
void cursor_move_scroll(int8_t dx, int8_t dy);
```

`screenmap[]` is a flat, row-major buffer holding raw screen bytes (both
characters and serial-attribute bytes `$00`-`$1F`) for the whole canvas,
which can be larger than the 40x27 viewport. `canvas_blit()` writes the
visible `VIEWPORT_WIDTH x VIEWPORT_HEIGHT` slice (from `app.xoffset`/
`app.yoffset`) to `$BB80` via raw pointer writes covering all 40 columns —
deliberately bypassing `OricCharWin`/`cwin_viewport_*`, which would clobber
attribute bytes the user places in columns 0-1 (§4.1).
`canvas_resize(neww, newh)` validates `(new >= viewport min) && (neww*newh <=
CANVAS_MAX_SIZE)` before reallocating the logical size (the physical array is
always `CANVAS_MAX_SIZE` bytes).

`canvas_rowbuf[]` (widest possible row) is `canvas_resize()`'s row-reflow
scratch buffer, made non-`static` in Phase 8 so `select.c`'s cut/copy
(§6.17) can reuse it for the same overlap-safety purpose -- the two uses
are mutually exclusive in time, so sharing one 320-byte buffer is safe.

`cursor_move_scroll(dx, dy)` (Phase 5) moves `app.cursor_x`/`cursor_y` by one
cell, except at a viewport edge on a canvas larger than the viewport, where
it scrolls `app.xoffset`/`yoffset` (and `canvas_blit()`s) instead — fixing a
pre-Phase-5 gap where cells beyond the 40x27 viewport on a resized canvas
were unreachable. Used by `editor_run()`'s cursor keys, Select/Line-Box's
rect-grower (`select.c`), and Write mode (`write.c`).

### 4.4 Menu system (`src/menu.h`)

```c
#define MENUBAR_MAXOPTIONS   4
#define MENUBAR_MAXLENGTH   12
#define PULLDOWN_NUMBER      5   // 4 bar pulldowns + MENU_YESNO=4
#define PULLDOWN_MAXOPTIONS  6
#define PULLDOWN_MAXLENGTH  17
#define MENU_WIN_DEPTH       9   // max nested window saves
#define MENU_WINBUF_SIZE  2048   // main-RAM LIFO buffer

typedef struct {
    uint16_t  offset;       // offset into menu_winbuf[] where rows are saved
    uint8_t   ypos;         // first screen row saved
    uint8_t   height;       // number of rows saved (each row = SCREEN_COLS bytes)
    uint8_t   swap_charset; // 1 = charsetswap_enter() was called for this save
} MenuWinRecord;

typedef struct {
    char     titles[MENUBAR_MAXOPTIONS][MENUBAR_MAXLENGTH];
    uint8_t  xstart[MENUBAR_MAXOPTIONS];   // screen col of highlight attribute byte
    uint8_t  ypos;                          // screen row where bar is drawn
} MenuBar;

extern MenuBar menubar;
extern char    pulldown_options[PULLDOWN_NUMBER];
extern char    pulldown_titles[PULLDOWN_NUMBER][PULLDOWN_MAXOPTIONS][PULLDOWN_MAXLENGTH];
```

`PULLDOWN_NUMBER = 5`: index 0 = Screen, 1 = File, 2 = Charset, 3 =
Information (the menu-bar pulldowns), index `MENU_YESNO = 4` = the shared
Yes/No confirm dialog. `menubar`/`pulldown_options`/`pulldown_titles` are
defined in `src/menudata.c`; `pulldown_titles[0][0]`/`[0][1]` ("Width:
NN"/"Height: NN") are rewritten by `update_size_titles()` whenever the canvas
size changes.

Unlike locifilemanager-v2 (which saves popup-covered rows to **overlay RAM**
via `cwin_push`/`cwin_pop`, `MenuWinRecord.overlay_addr`), OSE's
`menu_winsave`/`menu_winrestore` use a **static main-RAM buffer**
(`menu_winbuf[MENU_WINBUF_SIZE]`, LIFO via `MenuWinRecord.offset`) — see §6.3.
The `swap_charset` field is OSE-specific (§4.5/§6.5).

### 4.5 Charset-swap mechanism (`src/charsetswap.h`, `include/charset.h`)

No new structs — a small piece of module-private state in `charsetswap.c`:

```c
static uint8_t swap_depth;       // nesting depth of opt-in menu_winsave() calls
static uint8_t charset_changed;  // set once the user edits any glyph
static uint8_t backup_std[CHARSET_GLYPH_AREA_SIZE]; // 768B CHARSET_STD backup
```

plus the generic, non-OSE-specific primitives in `include/charset.h/c`:

```c
uint16_t charset_address(uint8_t screencode, uint8_t altorstd);
void charset_save(uint16_t base, uint8_t *dest);
void charset_load(uint16_t base, const uint8_t *src);
const uint8_t *charset_rom_glyph(uint8_t screencode);

void charset_glyph_invert(volatile uint8_t *glyph);
void charset_glyph_mirror_v(volatile uint8_t *glyph);
void charset_glyph_mirror_h(volatile uint8_t *glyph);
void charset_glyph_scroll_up(volatile uint8_t *glyph);
void charset_glyph_scroll_down(volatile uint8_t *glyph);
void charset_glyph_rotate_left(volatile uint8_t *glyph);
void charset_glyph_rotate_right(volatile uint8_t *glyph);
```

See §6.5 for how these compose into the swap mechanism, and `libmanual.md`
for full per-function documentation.

---

## 5. Module Dependency Overview

```
main.c
 ├─ canvas.h     (screenmap[], canvas_blit/resize)
 │   └─ appstate.h (AppState, canvas/grid size constants)
 ├─ statusbar.h  (row-27 OricCharWin statusbar)
 ├─ menu.h       (menu bar, pulldowns, popups, main-RAM window stack)
 │   └─ charsetswap.h (charsetswap_mark_changed/enter/exit)
 │       └─ charset.h (charset_address/save/load/rom_glyph, glyph-bitmap ops)
 ├─ menudata.h   (Screen/File/Charset/Information tables + Screen/File/
 │                Charset/Information dispatch)
 │   └─ menu.h, canvas.h, statusbar.h, strings.h, fileio.h, info.h
 ├─ editor.h     (main-mode loop)
 │   ├─ charsetedit.h (character editor popup)
 │   │   └─ charset.h (glyph addressing + bitmap transforms)
 │   │   └─ help.h (FUNCT+8, §6.18)
 │   ├─ palette.h     (palette popup: Fav/Std/Alt grid, visualchar[])
 │   ├─ colourpicker.h (colour picker popup: 8x8 ink x paper grid)
 │   ├─ select.h      (rect_select(), Line/Box + Select mode, cut/copy)
 │   │   └─ help.h (FUNCT+8, guarded -- §6.18)
 │   ├─ move.h         (Move mode: content nudge)
 │   │   └─ help.h (FUNCT+8, §6.18)
 │   ├─ write.h        (Write mode: free-typing screencodes)
 │   │   └─ help.h (FUNCT+8, §6.18)
 │   ├─ undo.h          (undo/redo, §6.16)
 │   │    └─ loci.h (enable_overlay_ram/disable_overlay_ram -- Oric-side
 │   │               overlay RAM, not LOCI-device XRAM, see §2.4)
 │   ├─ input.h         (key_read(), §6.17)
 │   │    └─ ijk.h (ijk_detect/ijk_read -- VIA Port A joystick)
 │   └─ help.h          (FUNCT+8 in Main mode, §6.18)
 └─ strings.h    (EN/FR localisation gateway -> strings_en.h/strings_fr.h, §3)

select.c, move.c, write.c, menudata.c
 └─ undo.h (undo_snapshot() retrofitted into every destructive edit, §6.16)

main.c, editor.c, select.c, move.c, write.c, charsetedit.c, palette.c,
colourpicker.c, filepicker.c, menu.c, menudata.c
 └─ input.h (key_read() replaces cwin_getch() everywhere, §6.17)

menudata.c
 └─ fileio.h  (LOCI file I/O: filename prompt (Save), presence gate,
               Screen/Combined/Project/Charset save/load, §6.14)
      └─ loci.h (LOCI mass-storage API -- file I/O portion now wired in)
      └─ canvas.h, charset.h, menu.h, menudata.h, statusbar.h
      └─ filepicker.h (LOCI directory browser for Load actions, §6.15)
           └─ loci.h (xram_peek/poke/memcpy_to/memcpy_from -- LOCI-device
                      XRAM, not Oric-side overlay RAM, see §2.4)
           └─ menu.h, statusbar.h
 └─ info.h    (Information menu: Version 3-page popup, Exit via RESET
               vector, §6.19)
      └─ menu.h, appstate.h, input.h

statusbar.c, select.c, move.c, write.c
 └─ modes.h  (mode_name(): EditorMode -> MSG_MODE_* lookup, standalone)

main.c, canvas.c, statusbar.c, menu.c, menudata.c, editor.c, charsetedit.c,
charsetswap.c, select.c, move.c, write.c, fileio.c, filepicker.c, help.c,
info.c
 └─ charwin.h  (windows, cursor, text input)
      └─ keyboard.h (raw key scan/decode)
 └─ oric.h     (hardware register layout, screen/attr/charset-bank constants)

help.c, info.c
 └─ oscar.h (Oscar64 stdlib: oscar_expand_lzo(), decompresses #embed lzo
            data straight into $BB80 screen RAM)
```

`charwin.h`, `keyboard.h`, `menu.h`, `menudata.h`, `canvas.h`, `statusbar.h`,
`editor.h`, `charsetedit.h`, `charsetswap.h` and `charset.h` each carry a
trailing `#pragma compile("xxx.c")`, so including the header is sufficient to
pull the implementation into the build (Oscar64 whole-program compilation).
`charset.h` is `#include`d from both `charsetswap.h` and `charsetedit.h`;
its include guard makes the repeated `#pragma compile("charset.c")` a no-op
the second time.

---

## 6. Application Flow & Logic

### 6.1 Startup sequence

1. Tape loader jumps to the **startup region** (`$0500`), which contains
   `oric_startup` (`include/oric_crt.c`).
2. `oric_startup`: `SEI` → clear BSS → clear the Oscar64 zero-page register
   file → set the software stack pointer (top of `$B200-$B3FF`) → set the
   6502 hardware stack pointer to `$FF` → `JSR main` → on return, spin
   forever (bare metal, no OS to return to).
3. `main()` (`src/main.c`):
   - `charwin_init()` — build the row-address lookup table.
   - Splash screen: `cwin_init(&splash, 2, 0, 38, 28, A_FWWHITE, A_BGBLACK)`,
     `cwin_clear`, three lines of text (title, build version, "Press any key
     to start"), `cwin_getch()` to wait.
   - `canvas_init()` — allocate/clear `screenmap[]` at the default 40x27 size.
   - `statusbar_init()` — set up the row-27 `OricCharWin`.
   - `menu_init()` — reset the main-RAM window-save stack
     (`menu_winbuf`/`menu_win_stack`).
   - `menudata_init()` — set the Screen > Width/Height pulldown titles from
     the current canvas size.
   - `editor_run()` — enter the main-mode loop (never returns).

### 6.2 Main loop (`editor_run`, `src/editor.c`)

```c
app.cursor_x = app.cursor_y = 0;
app.xoffset = app.yoffset = 0;
app.plotscreencode = 'A'; app.plotaltchar = 0;
app.plotink = A_FWWHITE; app.plotpaper = A_FWBLACK;
app.plotblink = 0; app.plotdouble = 0; app.visualmap = 0;
app.favourites[0..9] = '!';
app.mode = MODE_MAIN; app.showstatusbar = 1;

canvas_blit(); statusbar_draw(); canvas_cell_invert(cursor); // show cursor

for (;;) {
    c = cwin_getch();
    canvas_cell_invert(cursor);              // hide cursor
    switch (c) {
        case KEY_UP/DOWN/LEFT/RIGHT: cursor_move_scroll(dx, dy)  (Phase 5, §4.3)
        case KEY_SPACE: canvas_put(cursor, app.plotscreencode); canvas_blit();
        case KEY_DEL:   canvas_put(cursor, CH_SPACE); canvas_blit();
        case '+'/'-':   cycle app.plotscreencode in [0x20, 0x7E], wrapping
        case ','/'.':   cycle app.plotink 0-7, wrapping        (Phase 4a)
        case ';'/'\'':  cycle app.plotpaper 0-7, wrapping       (Phase 4a)
        case 'b'/'d'/'a': toggle plotblink/plotdouble/plotaltchar (Phase 4a)
        case 'r':       app.plotscreencode ^= 0x80               (Phase 5f)
        case 'g':       grab screencode/attr under cursor into plot* (Phase 5f)
        case 'i'/'o'/'u': plot ink/paper/modifier at cursor, move down (Phase 5f)
        case '0'-'9':   app.plotscreencode = app.favourites[c-'0'] (Phase 4a)
        case SHIFT+0-9: app.favourites[n] = app.plotscreencode  (Phase 4a)
        case KEY_F6:    statusbar_show(!app.showstatusbar);
        case KEY_F1:    menu_run();          // open the menu bar
        case 'e':       charsetedit_run();   // open the character editor
        case 'p':       palette_run();       // open the palette popup (Phase 4b)
        case 'c':       colourpicker_run();  // open the colour picker (Phase 4c)
        case 'l':       linebox_run();       // Line/Box mode (Phase 5b, §6.9)
        case 's':       select_run();        // Select mode (Phase 5c, §6.10)
        case 'm':       move_run();          // Move mode (Phase 5d, §6.11)
        case 'w':       write_run();         // Write mode (Phase 5e, §6.12)
        case 'z':       undo_perform();      // undo last canvas edit (Phase 8, §6.16)
        case 'y':       redo_perform();      // redo last undone edit (Phase 8, §6.16)
    }
    statusbar_draw();
    canvas_cell_invert(cursor);              // show cursor at new pos
}
```

### 6.3 Menu system (three-layer + main-RAM window stack)

1. **Menu bar** (row 0, `menu_run()` in `menudata.c`): `menu_main()` runs the
   bar navigation loop — left/right move between the 4 bar items
   (Screen/File/Charset/Information), ENTER/down opens that item's
   **pulldown**. Returns `menubarchoice*10 + menuoptionchoice`, or `>= 99` =
   ESC at bar level.
2. **Pulldowns**: `menu_pulldown(xpos, ypos, menunumber, escapable)` draws
   `pulldown_titles[menunumber]` (`pulldown_options[menunumber]` entries) and
   handles up/down/ENTER/ESC and left/right (`MENU_LEFT_ARROW`/
   `MENU_RIGHT_ARROW` = 18/19, to switch to the adjacent bar item). Returns
   `1..N`, `MENU_CANCEL` (0), or 18/19.
3. **Popups**: `menu_areyousure()`, `menu_messagepopup()`, and
   `resize_dialog()` (`menudata.c`, Screen > Width/Height) each call
   `menu_winsave(ypos, height, swap_charset)` / `menu_winrestore()`.

**Window stack** (`menu_winsave`/`menu_winrestore`, §4.4): each save copies
`height` rows of `SCREEN_COLS` (40) bytes from `$BB80+ypos*40` into
`menu_winbuf[]` at the current bump-allocator offset, and pushes a
`MenuWinRecord{offset, ypos, height, swap_charset}` onto
`menu_win_stack[MENU_WIN_DEPTH]`. `menu_winrestore()` pops the record, copies
the bytes back, and — if `swap_charset` was 1 — calls `charsetswap_exit()`
(§6.5). All main-RAM, no overlay RAM, so popups close with no residue in
plain Oricutron.

Call sites and their `swap_charset` argument: `menu_run()` (menu bar, `1`),
`menu_pulldown()` (`1`), `menu_areyousure()` (`1`), `menu_messagepopup()`
(`1`), `resize_dialog()` (`1`), `palette_run()` (`1`, §6.7),
`colourpicker_run()` (`1`, §6.8); `charsetedit_run()` passes `0` (§6.4/§6.5).

Return-code convention from `menu_run()`'s `switch`: `menubarchoice*10 +
menuoptionchoice` — e.g. `11`/`12` = Screen pulldown (bar item 1), options 1
(Width)/2 (Height); `13`/`14` = Clear/Fill. File/Charset/Information items
currently show `MSG_MENU_NOTIMPL` (any return `< 99` not matched by a Screen
case).

### 6.4 Character editor (`src/charsetedit.c`, entered via `e`)

Edits the 6x8-pixel glyph (`CHARSET_GLYPH_BYTES=8` rows x
`CHARSET_GLYPH_WIDTH=6` bits) for `app.plotscreencode`/`app.plotaltchar`
**directly in live charset RAM** ("Strategy A" — no shadow buffers; the ULA
re-renders from charset RAM every frame, so edits are visible immediately in
the popup's preview and on the canvas).

Layout: a narrow **13x15 sidebar popup** at screen cols 27-39, rows 0-14
(`menu_winsave(CE_WIN_SY, CE_WIN_WY, 0)` — opts OUT of the charset-swap, §6.5,
so live edits stay visible), leaving the user's canvas (cols 0-26) visible to
the left on every row — matching V1's sidebar intent. Content: row 0 =
`Code:$xx` + live preview cell; row 1 = `Set:Std`/`Set:Alt`; rows 3-4 =
favourites (`'0'-'9'` digits over `app.favourites[10]` screencodes); rows
6-13 = the 6x8 pixel grid (one screen cell per pixel, 4-state rendering for
pixel x cursor); row 14 = a 2-digit hex-row input.

Key bindings (full description in `README.md` "Character editor"): cursor
keys move within the grid; `+`/`-`/`=` cycle the screencode (wrapping at
`ce_max_code()`, std `$20`-`$7F` / alt `$20`-`$6F`); `0-9`/`SHIFT+0-9`
recall/store favourites; `SPACE` toggles the pixel under the cursor; `DEL`
clears the glyph; `i` inverts (`charset_glyph_invert`); `z` undoes the last
destructive edit (`ce_undo[]`); `s` restores from `CHARSETROM` (std only,
`charset_rom_glyph`); `c`/`v` copy/paste (`ce_copy[]`); `x`/`y` mirror
horizontal/vertical; `u`/`d`/`l`/`r` scroll/rotate (all via `include/charset.h`
glyph-bitmap ops); `h` opens the hex-row input; `a` toggles Std/Alt;
`FUNCT+6` toggles the statusbar; `ESC` commits `ce_code`/`ce_altorstd` back to
`app.plotscreencode`/`app.plotaltchar` and closes the popup.

Every destructive edit goes through `ce_snapshot()`, which both populates
`ce_undo[]` and calls `charsetswap_mark_changed()` (§6.5) — the single
chokepoint for both features.

### 6.5 Charset-swap mechanism (`src/charsetswap.c`, `include/charset.h`)

Ensures popup chrome (menu bar, pulldowns, Are-you-sure/message/resize
popups) always renders with ROM-standard `CHARSET_STD` glyphs, even after the
user has redefined characters in the character editor, without permanently
discarding those edits:

- **`charsetswap_mark_changed()`** — called once, from `ce_snapshot()`. Until
  the user edits a glyph for the first time, the swap is a no-op
  (`charset_changed == 0`).
- **`charsetswap_enter()`** (opt-in via `menu_winsave(..., 1)`) — on the
  outermost call (`swap_depth == 0`, gated on `charset_changed`):
  `charset_save(CHARSET_STD, backup_std)` then `charset_load(CHARSET_STD,
  CHARSETROM)`. Then `swap_depth++`.
- **`charsetswap_exit()`** (called by `menu_winrestore()` iff the matching
  save had `swap_charset == 1`) — `swap_depth--`; if now `0` and
  `charset_changed`: `charset_load(CHARSET_STD, backup_std)` restores the
  user's edits.
- **Std-only**: a Phase-3a-style spike confirmed `jsr $F816` (ROM_ALTCHARS,
  used by V1 to regenerate `CHARSET_ALT` from `CHARSET_STD`) is a no-op from
  Oscar64 in this runtime, so `CHARSET_ALT` is left untouched by the swap —
  acceptable because popup chrome only uses `CHARSET_STD` (`A_STD` attribute).
- **Depth-counting** lets nested popups (e.g. Screen pulldown → resize popup
  → shrink-confirm Are-you-sure popup) share the single `backup_std[]` buffer
  safely — only the outermost enter/exit pair touches it.

### 6.6 Canvas resize (`resize_dialog()`, `src/menudata.c`)

Screen > Width/Height open a popup (`menu_winsave(5, 12, 1)`,
`cwin_textinput` for a numeric field). A new size is valid iff `newval >=
minval` (`minval = VIEWPORT_WIDTH`/`VIEWPORT_HEIGHT`, i.e. the *default*
size — a dimension can only shrink below its default after first being grown
above it) and `neww*newh <= CANVAS_MAX_SIZE` (8192). Shrinking below the
*current* size triggers `menu_areyousure(MSG_MENU_SHRINKWARN)`; "No" leaves
the size unchanged. On success, `canvas_resize()` is applied and
`update_size_titles()` rewrites the pulldown labels.

### 6.7 Palette mode (`src/palette.c`, entered via `p`)

Near-full-width popup (`PAL_WIN_SX=2/SY=0/WX=38/WY=13`, screen cols 2-39 rows
0-12, `menu_winsave(0, 13, 1)`) for selecting `app.plotscreencode`/
`app.plotaltchar`.

Layout (13 rows x 16 cols, `PAL_GRID_X0=5`, `PAL_GRID_STEP=2`): row 0
(`PAL_ROW_FAV`) = `"Fav:"` + 10 favourite glyphs (`app.favourites[]`); rows
1-6 (`PAL_ROW_STD0=1`) = `"Std:"` + the full standard charset 0x20-0x7F (96
glyphs, 16/row); rows 7-12 (`PAL_ROW_ALT0=7`) = `"Alt:"` + the full alternate
charset 0x20-0x7F, or — if `app.visualmap` — rows 7-11 remapped via
`visualchar[80]` (row 12 stays identity 0x70-0x7F). Each grid cell is the
screencode byte, drawn under the row's `A_STD`/`A_ALT` attribute byte at
`PAL_ATTR_X=4`. `visualchar[80]` is ported verbatim from V1 OricScreenEditor
(`/home/xahmol/git/OricScreenEditor/src/main.c`), credited to jab/Artline
Designs.

Cursor `(rowsel, colsel)` is highlighted via `^0x80` (same convention as
charsetedit's grid cursor). Initial position mirrors V1: derived from
`app.plotscreencode` (`row = (code-0x20)/16 + 1`, `col = (code-0x20)%16`),
always landing in the Std rows. `pal_wrap()` applies cursor-wrap post-checks
(row overflow, then favourites-row column limit, then general column limit —
order matters to avoid an out-of-bounds `favourites[]` access).

Key bindings: cursor keys navigate with wrap across all three sections;
`SPACE`/`ENTER` set `plotscreencode`/`plotaltchar` from the highlighted cell
and close the popup (`menu_winrestore()`); `0`-`9` store the highlighted
cell's screencode into `favourites[n]` (redraws the Fav row); `v` toggles
`app.visualmap` (redraws rows 7-12); `FUNCT+6` toggles the statusbar; `ESC`
closes the popup leaving `plotscreencode`/`plotaltchar` unchanged.

Opts IN to the charset-swap (`menu_winsave(..., 1)`) — the Std/Alt grids
render via `A_STD`/`A_ALT` attribute bytes, not direct charset-RAM access, so
there's no live-edit-preview requirement.

### 6.8 Colour picker (`src/colourpicker.c`, entered via `c`)

**New OSE-LOCI feature over V1** (see README.md "Planned feature additions
over V1"). Popup (`CP_WIN_SX=2/SY=0/WX=36/WY=13`, screen cols 2-37 rows 0-12,
`menu_winsave(0, 13, 1)`) for selecting `app.plotink`/`app.plotpaper` from an
8x8 ink x paper grid.

Layout: row 0 = title (`MSG_COLOURPICKER_TITLE`); rows 1-8 (`CP_ROW_GRID0=1`)
= one row per paper value 0-7, 8 cells (ink 0-7) at `CP_GRID_X0=2`,
`CP_CELL_STEP=4` cols/cell: `[ink-attr byte, paper-attr byte (16+paper),
normal swatch, inverse swatch]`. Rows 10-12 are feedback lines: `Ink:` +
swatch (`CP_ROW_INK`), `Paper:` + swatch (`CP_ROW_PAPER`), `Result:` +
ink/paper attrs + normal+inverse preview pair (`CP_ROW_RESULT`).

Cursor highlight swaps a cell's two swatch chars (`CH_SPACE`<->`CH_INVSPACE`;
normal = paper-colour swatch then ink-colour swatch, highlighted = reversed)
— a 2-char analogue of charsetedit's `^0x80` cursor. Initial position =
`(app.plotink, app.plotpaper)`.

Key bindings: LEFT/RIGHT cycle ink (wrap 0-7), UP/DOWN cycle paper (wrap 0-7);
`SPACE`/`ENTER` commit the highlighted cell to `plotink`/`plotpaper` and close
the popup; `FUNCT+6` toggles the statusbar; `ESC` closes the popup leaving
`plotink`/`plotpaper` unchanged.

Adapted from V1's `colourpicker()`/`colorpicker_cursorplot()` (archived
`nonworkingcc65:src/colorpicker.c`) — same 8x8 grid + Ink:/Paper:/Result:
feedback concept; the border-drawing cursor is replaced with the simpler
2-char swatch swap. Opts IN to the charset-swap — swatch glyphs
(`CH_SPACE`/`CH_INVSPACE`) are plain Std-charset chars.

### 6.9 Shared rectangle-grower (`rect_select()`, `src/select.c`)

Used by both Line/Box and Select mode (§6.10/§6.11 below). Adapted
from V1's `lineandbox()`. Grows a rectangle from the cursor's starting
position via cursor keys (using `cursor_move_scroll()`, §4.3/§6.2, so it
auto-scrolls on oversized canvases too); ENTER accepts (populating
file-scope statics `select_startx`/`starty`/`endx`/`endy`/`width`/`height`
in `select.c`, not `AppState` fields) and returns 1; ESC cancels and
returns 0.

**Simplified vs V1**: rather than tracking which rectangle edge is
"trailing" the origin and redrawing only the moved edge each keypress, this
port always recomputes the rectangle fresh as
`[min(origin,cursor)..max(origin,cursor)]`, and redraws by XOR-toggling
(`canvas_cell_invert()`) the old perimeter off then the new perimeter on —
cells in both are toggled twice (no-op, stay highlighted), cells only in
the old one are un-highlighted, cells only in the new one are highlighted.
Produces the same visual result without porting V1's four-way trailing-edge
comparison. When the viewport auto-scrolls mid-grow, `canvas_blit()` (called
by `cursor_move_scroll()`) already clears the stale highlight, so only the
new perimeter needs drawing that step.

### 6.10 Line/Box mode (`linebox_run()`, `src/select.c`, entered via `l`)

Calls `rect_select(1)` (`app.mode = MODE_LINEBOX`); if accepted, fills the
rectangle with `app.plotscreencode` via a `canvas_put()` loop, then
`canvas_blit()`s. ESC at the rect stage leaves the canvas unchanged.

### 6.11 Select mode (`select_run()`, `src/select.c`, entered via `s`)

Calls `rect_select(0)` (`app.mode = MODE_SELECT`); if cancelled, returns
immediately with no further prompt (matches V1). If accepted, prompts for
an action: `d` clears to `CH_SPACE`, `i` fills with `app.plotink`, `p` fills
with `16+app.plotpaper`, `m` fills with the modifier-attribute byte for
`app.plotaltchar`/`plotdouble`/`plotblink` (same bit-packing as `A_STD`(8)/
`A_ALT`(9)/.../`A_BLINK2HALT`(15) in `oric.h`: base 8, bit0=altchar,
bit1=double, bit2=blink). `FUNCT+6` toggles the statusbar. ESC at either
stage leaves the canvas unchanged.

V1's cut/copy (`x`/`c`) are **deferred to Phase 8** (overlay-RAM
clipboard) — V1 uses a screen-RAM scratch buffer OSE has no safe equivalent
for yet.

### 6.12 Move mode (`move_run()`, `src/move.c`, entered via `m`)

Cursor keys shift the *visible viewport's content* (not the cursor, not
`app.xoffset`/`yoffset` — distinct from the cursor-auto-scroll fix, §4.3) by
one row/col directly in `screenmap[]`: leading edge fills with `CH_SPACE`,
trailing edge's content is lost. `canvas_blit()` after each move.
`FUNCT+6` toggles the statusbar. ENTER and ESC both exit to Main —
equivalent here, since every shift is already written directly to
`screenmap[]` (V1's hardware-scroll-then-bake-back approach has a separate
scratch buffer to roll back from on ESC; this port doesn't, so ESC does
*not* roll back shifts already applied this session — a deliberate
simplification, not a bug).

### 6.13 Write mode (`write_run()`, `src/write.c`, entered via `w`)

Direct port of V1's free-typing key table: cursor keys move (with
auto-scroll); `CTRL+B`/`CTRL+A`/`CTRL+D` toggle blink/altchar/double;
`CTRL+Z`/`CTRL+X` cycle ink down/up; `CTRL+C`/`CTRL+V` cycle paper down/up;
`FUNCT+1`/`FUNCT+2`/`FUNCT+3` plot ink/paper/modifier-attribute at the
cursor and advance right (`FUNCT+N`, *not* `i`/`o`/`u` — those letters are
themselves printable characters to type here, see CLAUDE.md's Write mode
section); `DEL` clears the cell; `CTRL+R` toggles a local reverse-video
flag; any other printable key plots its screencode (`+0x80` if
reverse-video) and advances right. `FUNCT+6` toggles the statusbar. ESC
exits to Main.

### 6.14 LOCI file I/O (`src/fileio.c`, File/Charset menus, Phase 6)

`menu_run()`'s `switch` (§6.2) dispatches all 12 File (choices 21-26) and
Charset (31-36) items here, replacing the `default:`-case "not yet
implemented" popup those used to fall through to (Information, 41-42,
still falls through — out of Phase 6's scope).

Every entry point calls `loci_check_present()` first (`include/loci.h`'s
`loci_present()`, a plain `*LOCI_SIGNATURE_ADDR == 'L'` read — always safe
to call) and aborts with a graceful popup if absent, then
`fileio_get_filename()` (a `cwin_textinput` popup, same pattern as
`resize_dialog()`, §6.6) to get/confirm `app.filename`.

- **Save/Load Screen**: `<name>.BIN` = `FileHeader` (magic + width/height)
  + `screenmap[]`. **Save/Load Combined**: same header + `CHARSET_STD`'s
  displayable range (768B) + `screenmap[]`. Both write/read each piece
  directly from where it lives via `loci_open`+`loci_write`/`read`+
  `loci_close` — no staging buffer (unlike V1, whose combined save relies
  on `CHARSET_STD` and `SCREENMEMORY` being memory-adjacent in *V1's* map,
  which doesn't hold for OSE).
- **Save/Load Project**: V1's literal 4-file scheme — `<name>PJ.BIN`
  (`ProjectHeader` metadata, via `file_save`/`file_load`'s single-blob
  convenience wrapper), `<name>SC.BIN` (same shape as Screen),
  `<name>CS.BIN`/`<name>CA.BIN` (768 raw bytes, written only if
  `app.stdchanged`/`altchanged`, read only if present).
- `AppState` gained `stdchanged`/`altchanged`, set in `charsetedit.c`'s
  `ce_snapshot()` (§6.4) based on `ce_altorstd`.
- **Charset menu** Load/Save Standard/Alternate/Combined: Std/Alt are 768
  raw bytes direct to/from `CHARSET_STD`/`CHARSET_ALT`'s displayable
  range; Combined save = Save Std; Combined load writes into *both* banks
  (`charset_load()`, §4.5) since the ROM call V1 used to regenerate Alt
  from Std is a no-op on this runtime (§6.5).
- Headless coverage today is the LOCI-absent gate only (`test_fileio_no_
  loci.sh`, §7); actual byte I/O isn't tested yet, but Phosphoric's LOCI
  emulation (alpha-quality) makes that addable headlessly rather than
  needing real hardware for every check — see §7's note.

### 6.15 File picker (`src/filepicker.c`, every Load action, Phase 7)

`filepicker_run(title, filter)` replaces `fileio_get_filename()` for
every Load action with a real LOCI directory browser. Save actions are
unaffected. Adapted from locifilemanager-v2's directory engine
(`/home/xahmol/git/locifilemanager-v2/src/dir.c`, `struct DirElement`/
`DirMeta` in `dir.h` — the primary reference for this phase, per explicit
instruction), simplified for a single-pane, single-select, no-sort,
dir-vs-file-only picker.

**The full directory listing is read into a linked list stored in
XRAM** — RAM *on the LOCI device itself* (`include/loci.h`'s `xram_peek/
poke/memcpy_to/memcpy_from`), not the Oric-side overlay RAM §2.4 covers
(Phase 8) — so this has **no Phase 8 dependency** (a correction made
during implementation; an earlier draft of this phase's plan conflated
the two resources). `PickerMeta{next, prev, isdir, length}` (6 bytes) is
written immediately before each variable-length name at its XRAM
address — entries are packed tightly, not fixed-size, matching
`DirElement`'s layout. `PICKER_DIRBASE = COPYBUF_XRAM_ADDR +
COPYBUF_XRAM_SIZE` (`0x8800`, matching locifilemanager-v2's `DIR1BASE`),
`PICKER_DIRSIZE = 0x0C00` (one pane's worth). Building the list stops
early (silently truncating) if this budget would be exceeded, same guard
`dir_read()` has.

Scrolling is **O(1) per row**, not a directory re-walk: `picker_firstprint`/
`picker_present` (XRAM addresses) + `picker_cursorrow` (visible-row index)
follow `next`/`prev` pointers directly — the same bookkeeping
locifilemanager-v2's `struct Directory` uses per pane.

**Type filtering** (`PICKER_FILTER_PLAIN`/`PICKER_FILTER_PROJECT`):
directories always match. `PLAIN` covers Load Screen/Combined and all
three Charset Load actions (none distinguishable from each other by
filename alone — matches V1), excluding the four Project sub-file
suffixes (`PJ/SC/CS/CA.BIN`); `PROJECT` (Load Project) matches only
`*PJ.BIN`. The matched suffix is stripped on selection before storing
into `app.filename`, so `fileio.c`'s existing suffix-appending
composition keeps working unmodified.

**Full subdirectory navigation** (confirmed with the user, overriding the
simpler flat-listing-only default recommended): ENTER on a directory
entry descends (`picker_path_descend()`, refusing rather than truncating
if the result wouldn't fit in `PICKER_PATH_MAXLEN=64`); LEFT goes to the
parent (`picker_path_ascend()`, no-op at the root). Selecting a file in a
subdirectory prepends that subdirectory to `app.filename` (relative to
the LOCI root, e.g. `"DIR1/DIR2/name"`). `FILENAME_MAXLEN` grew from 24 to
48 (`src/appstate.h`) to leave room for this.

No new automated test coverage yet: `filepicker_run()` is only reachable
from a Load action that already requires `loci_check_present()` to pass
first, so `test_fileio_no_loci.sh`'s existing assertions are what's
automated today. The actual directory browsing isn't covered yet, but
(see §7) Phosphoric's LOCI emulation should make that addable headlessly.

### 6.16 Canvas undo/redo (`src/undo.c`, `z`/`y` in Main mode, Phase 8)

Genuinely new functionality — V1 has no canvas-edit undo at all. A
faithful port of vdcscreeneditor-v2's `undo_new()`/`undo_performundo()`/
`undo_performredo()` (`/home/xahmol/VDCScreenEditor2/src/main.c`, lines
739-905), adapted for OSE's single-plane canvas and **Oric-side overlay
RAM** ($C000-$FFFF, `enable_overlay_ram()`/`disable_overlay_ram()`) — a
different resource from §6.15's LOCI-device XRAM (see §2.4; this
distinction was the subject of a planning correction, twice, during
Phase 7 and again during Phase 8).

A fixed 40-slot ring, each slot independently storing its own
dirty-rectangle snapshot at its own byte offset in the 16KB region. The
slot about to be overwritten *next* is invalidated immediately (a
`valid` flag), so undo/redo always stops cleanly at the true edge of
history. The byte bump-pointer only resets (taking the slot index with
it) when the next snapshot would exceed the 16KB budget — a rare event —
and even then, slots elsewhere in the ring keep their data until a later
cycle's writes physically reach them, so history degrades gradually
across a wrap rather than vanishing all at once. Redo data is reserved
and stored alongside the original snapshot in the same slot;
`undo_perform()` saves the *current* content into that reserved region
before restoring the old content, mirroring vdcscreeneditor-v2's
save-before-restore order.

`undo_snapshot(x, y, w, h)` is called immediately before every
destructive edit, retrofitted across `editor.c` (Main-mode `SPACE`/`DEL`/
`i`/`o`/`u`), `select.c` (Line/Box and Select fills, cut/copy), `move.c`
(per-shift, whole-viewport), `write.c` (every plotted/cleared cell), and
`menudata.c` (Screen Clear/Fill, whole-canvas). No-ops if
`!loci_present()` — undo is simply unavailable without a LOCI device.
`undo_escapeundo()` (vdcscreeneditor-v2's mid-operation-cancel cleanup)
has no OSE equivalent: every call site here snapshots only once an edit
is actually about to commit, never speculatively before a cancellable
step.

Not testable headless beyond the LOCI-absent path (`test_undo_no_loci.sh`)
today — see §7's note: Phosphoric's LOCI emulation might cover this too,
but only if it also models `MICRODISCCFG`-driven overlay-RAM banking
(unconfirmed), not just the file/XRAM protocol.

### 6.17 Select mode cut/copy (`src/select.c`, `x`/`c`, Phase 8)

Completes V1's full Select prompt key set (`d`/`i`/`p`/`m`/`x`/`c`).
Turned out not to need overlay RAM: V1's `selectmode()` cut/copy copies
**one row at a time** through a single-row scratch buffer
(`SCREENMEMORY`, a reuse of live screen RAM there), choosing row order by
whether the destination is above or below the source. OSE's
`canvas_rowbuf[]` (§4.3, already used by `canvas_resize()` for the same
overlap-safety reason) serves the same role, exposed via `canvas.h`.
`select_width` can never exceed `CANVAS_MAX_ROW` since `canvas_resize()`
already bounds `app.canvas_width` — no V1-style explicit size cap needed.

A destination-picking sub-loop (cursor keys, `ENTER` confirms, `ESC`
cancels, its own cursor highlight) follows picking `x`/`c`; `MSG_SELECT_
NOFIT` if the destination doesn't fit. `select_paste()` then runs the
per-row copy (`memcpy` source row into `canvas_rowbuf`, `memset` to blank
if cutting, `memcpy` into the destination row), in the direction that
avoids self-overlap corruption. Cutting takes two `undo_snapshot()` calls
(source + destination), so reverting a cut needs two `z` presses — a
minor known wrinkle.

Fully testable headless (`test_select_cutcopy.sh`) — no LOCI/overlay RAM
involved at all.

### 6.18 IJK joystick input (`src/input.c`, Phase 9a)

`key_read()` is the single blocking-read function every mode now calls
instead of `cwin_getch()`. Each call: if `ijk_present` (set once by
`ijk_detect()` from `main.c`), reads both sticks (`ijk_read()`) and maps
`FIRE`/`RIGHT`/`LEFT`/`DOWN`/`UP` bits to `KEY_ENTER`/cursor keys
(left stick first, then right, last-match-wins); a fired direction is
drained to neutral before returning (no repeat from a single press).
Falls back to `keyb_poll()` if the joystick gave nothing this spin, or
none is present. Ported from `nonworkingcc65`'s `getkey(joyallowed=1,
norepeat=1)` — V1 itself has no joystick code at all.

### 6.19 Help screens (`src/help.c`, `FUNCT+8`, Phase 9b)

`help_show(screennumber)` (1=Main, 2=Character editor, 3=Select/Move/
Line-Box, 4=Write) blits one of 4 `#embed`'d, LZO-compressed 1080-byte
screen dumps (`assets/OSEforLOCI-Help{1..4}.bin`) straight into `$BB80`
via `oscar_expand_lzo()`, bracketed by `charsetswap_enter()`/`exit()` (so
the dump's own text renders with ROM glyphs regardless of user charset
edits) and a `key_read()` wait; `canvas_blit()` + `statusbar_draw()`
restore the real canvas afterward. Ported from V1's
`helpscreen_load(screennumber)`, but compile-time-embedded instead of
tape-loaded — works with no LOCI device. Wired into `editor.c` (Main,
unconditional), `charsetedit.c` (also redraws the popup's own chrome
afterward), `select.c`'s shared `rect_select()` (guarded to only fire
before the rect starts growing, V1's exact guard), `move.c`
(unconditional), `write.c`. Not wired into the palette or colour-picker
popups (no V1 precedent).

### 6.20 Information menu (`src/info.c`, Phase 9c)

`info_version_show()`: a 3-page popup (`menu_winsave(0, VIEWPORT_
HEIGHT+1, 1)`/`menu_winrestore()` around all 3) — page 1 is
`OSEforLOCI-Title.bin` full-screen (same `#embed lzo` +
`oscar_expand_lzo()` approach as §6.19's help screens), page 2 is
programmatic version/credits text, page 3 is a 25x25-module QR code
(`tools/gen_qr.js`, adapted from `locifilemanager-v2`'s same-purpose
script) linking to the project's GitHub page. `info_exit()` resets the
machine via `__asm { jmp ($fffc) }` — an indirect jump through the RESET
vector, the bare-metal equivalent of V1's Exit (which just returned to
the CC65 program's OS-level loader; OSE's runtime has no such loader to
return to). Both wired into `menudata.c`'s Information pulldown (choices
41/42), the last two menu items that had been permanent stubs since
Phase 2.

---

## 7. Testing Infrastructure

Automated headless testing runs `build/oseloci.tap` under **Phosphoric**
(`oric1-emu`), which fast-loads it under Atmos BASIC 1.1 and supports
`--type-keys` (auto-typer) + `--dump-ram-at` (RAM dump for `$BB80`
screen-content assertions via `tests/scripts/oric_screen.py`).

```
make test                  # full suite (all 16 targets below)
make test-boot              # headless boot smoke test: splash + canvas/statusbar render
make test-menus              # menu bar/pulldown/Screen-menu regression + Information > Version
make test-screenresize        # Screen > Width/Height resize + shrink-confirm
make test-charsetram-spike     # Strategy A charset-RAM edit regression (Phase 3a)
make test-charsetedit            # character editor: grid, favourites, transforms, Std/Alt
make test-palette                 # palette popup: Fav/Std/Alt grid, cursor wrap, select/store/visualmap
make test-colourpicker              # colour picker: 8x8 grid, cursor wrap, select/ESC
make test-cursor-autoscroll          # viewport auto-scroll on an oversized canvas (Phase 5a)
make test-linebox                     # Line/Box mode: rect-grow, ENTER fills, ESC cancels (Phase 5b)
make test-select                       # Select mode: rect-grow, d/i/p/m fill actions, ESC paths (Phase 5c)
make test-move                          # Move mode: content nudge, ENTER/ESC both keep the shift (Phase 5d)
make test-writemode                      # Write mode: typing+advance, FUNCT+1/2/3 (Phase 5e)
make test-fileio-no-loci                  # all 12 File/Charset items show the LOCI-absent popup (Phase 6)
make test-select-cutcopy                   # Select x/c: copy/cut/no-fit/ESC (Phase 8c)
make test-undo-no-loci                      # z/y are graceful no-ops with no LOCI (Phase 8)
make test-help-funct8                       # FUNCT+8 round-trips cleanly, Main + Character editor (Phase 9b)
make test-capture CYCLES=N TYPEKEYS='...'  # calibration helper for new scripts
```

- Each `make test-*` target runs `sandbox-reset` first: wipes
  `tests/sandbox/`, copies `tests/fixtures/` into it, and copies the
  freshly-built `.tap` in.
- `tests/scripts/oric_screen.py` decodes the 40x28 `$BB80` text screen from a
  `--dump-ram-at` dump, providing `--find`/`--row`/`--bytes` assertions used
  by the shell scripts in `tests/scripts/test_*.sh`.
- Current totals: 5+16+8+2+12+14+14+2+6+10+4+6+12+5+4+7 = **127/127**
  (`test-boot` + `test-menus` + `test-screenresize` +
  `test-charsetram-spike` + `test-charsetedit` + `test-palette` +
  `test-colourpicker` + `test-cursor-autoscroll` + `test-linebox` +
  `test-select` + `test-move` + `test-writemode` + `test-fileio-no-loci` +
  `test-select-cutcopy` + `test-undo-no-loci` + `test-help-funct8`).
- Write mode's `CTRL+letter` toggles and `DEL` have no automated coverage:
  Phosphoric's `--type-keys` has no CTRL-modifier escape and `DEL` (0x7F) is
  unmapped in its `char_map` — covered by manual walkthrough + code review
  instead (see CLAUDE.md's Write mode section).
- LOCI's actual file I/O (the byte-level load/save traffic, not just the
  presence gate), the file picker's directory browsing (§6.15), and
  canvas undo/redo's actual snapshot/restore (§6.16) have no automated
  coverage *yet* — but, **correction**: this is not because emulation is
  impossible. Phosphoric *can* emulate a LOCI device (alpha-quality);
  only Oricutron cannot emulate one at all. The `*-no-loci` targets today
  only confirm the graceful absent-path, but headless tests of the real
  load/save/browsing traffic should be addable via Phosphoric — that work
  just hasn't been done yet. Undo/redo is the partial exception: it needs
  Phosphoric to also model `MICRODISCCFG`-driven overlay-RAM banking, not
  just the MIA/TAP/XRAM protocol, which is unconfirmed. Real hardware
  remains the authoritative check given Phosphoric's LOCI emulation is
  alpha-quality (see the Phase 6/7/8 plans), same caveat as the colour
  picker's hardware-rendering issue. Select mode's cut/copy (§6.17) needs
  neither LOCI nor overlay RAM and is already fully covered headless.
- IJK joystick input (§6.18) cannot be exercised by Phosphoric/Oricutron's
  `--type-keys` (no joystick simulation) — verified manually in Oricutron
  with a configured joystick instead. The help screens' and Version
  popup's exact *visual* content (§6.19/§6.20) likewise needs a manual
  look; what `test-help-funct8` checks instead is that `FUNCT+8` is wired
  correctly and round-trips cleanly (see that script's header comment for
  why the "still showing" intermediate state can't be captured from a
  type-keys script — Phosphoric's `\fN` escape delivers the 2-key FUNCT
  combo as a single matrix event that the *second*, inner `key_read()`
  call inside `help_show()` also consumes, so a lone `\f8` both opens and
  immediately dismisses help within one action). `info_exit()`'s RESET
  jump (§6.20) is not headlessly testable at all — a real reset is
  indistinguishable from a crash to the test harness from outside; it was
  confirmed manually instead (post-jump CPU state has the `I` flag clear
  and the PC deep in ROM, see CLAUDE.md's "Information menu (Phase 9c)").

---

## 8. Key Constraints (carry into any new feature work)

- **512-byte software stack** (`$B200-$B3FF`) shared by the whole call chain
  → avoid deep C-level recursion.
- **`$B400` ceiling** — code/data/BSS/stack must not extend into charset RAM
  (`$B400+`). The character editor and charset-swap mechanism intentionally
  *write into* `$B400-$BBFF` (that's the point), but program code/data never
  lives there.
- **No floats** (`-dNOFLOAT`) — `cwin_printf`/`cwin_putat_printf` support
  only `%d/%u/%x/%s/%c/%%`.
- **All user-visible strings must be `MSG_*` macros** defined identically in
  `src/strings_en.h` and `src/strings_fr.h` (selected via `src/strings.h` /
  `-dLANG_FR`, see §1.3 and `CLAUDE.md` "Localisation") — no raw user-visible
  string literals in logic files. FR strings must be unaccented (Oric ROM
  charset). Key bindings are independent of `MSG_*` and must stay identical
  across languages. New menu-bar titles must keep the 4-item
  `sum(strlen) <= 32` constraint (`menu_placebar()`, `src/menu.c`).
- **IRQs stay disabled for the whole program** — never use unconditional
  `SEI`/`CLI`; use `PHP`/`SEI`/`PLP` for any future code that touches overlay
  RAM or VIA Port A (§2.5).
- **`#embed` must be the only thing on its source line**, and large
  `#embed`s eat into the `heap` section's already-tight remaining budget
  before `code`/`data`/`bss` — prefer `#embed lzo`/`#embed rle` +
  `oscar_expand_lzo()`/`oscar_expand_rle()` (decompressing straight into
  hardware screen RAM, outside the `$0580-$B200` region) over raw embeds
  for anything screen-sized (see `oscar64manual.md`'s "Embedded Data"
  section and CLAUDE.md's "Help system (Phase 9b)").
- **Canvas vs. charwin** (§4.1/§4.3) — never use `cwin_*`/`OricCharWin` for
  the canvas; it bypasses the column-0/1 attribute convention that all other
  `cwin_*` windows rely on.
- **V1 UI parity** — any user-visible deviation from V1 OricScreenEditor's
  design (layout, key bindings, colours, wording, flow) should be flagged to
  the user with a rationale *before* implementing it, not after.
