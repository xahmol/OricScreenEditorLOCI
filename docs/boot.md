# Boot, Help & Information

## Boot splash

`main()` loads the title screen from LOCI at runtime into `SCREENMEMORY`, overlays
`MSG_SPLASH_PRESSKEY` at row 27 (the true last row), waits for a key. Loaded via
`loci_open`/`loci_read`/`loci_close` with `fd >= 0` guard. Missing file: skip
drawing, still wait for key. Path resolved via `homedir_join(path, MSG_TITLE_SCREEN_FILE)`.

**Language-specific title files** (selected at compile time via `MSG_TITLE_SCREEN_FILE`):
- EN build: `OSETSC.BIN` — English mosaic wordmark "ORIC / SCREEN / EDITOR / FOR / LOCI"
- FR build: `OSETSF.BIN` — French mosaic wordmark "ORIC / EDITEUR / ECRANS / POUR / LOCI"

Both files are raw 40×28 = 1120-byte Oric screen RAM dumps. Rows 0-20 contain the
Alt-charset mosaic letter art; rows 21-23 are decorative border fill; rows 24-25 carry
language-specific credits text; rows 26-27 are blank (row 27 = press-key overlay).

`idi8b_logo[]` (520-byte logo artwork) and QR code in `src/info.c` stay
`#embed`'d as plain static arrays — not runtime loads.

## IJK joystick input

`src/input.c/h`: `key_read()` replaces every `cwin_getch()` call site across all
`src/*.c` (11 files). If `ijk_present` (set once by `ijk_detect()` at startup),
reads both sticks (`ijk_read()`) and maps FIRE/RIGHT/LEFT/DOWN/UP to
KEY_ENTER/KEY_RIGHT/KEY_LEFT/KEY_DOWN/KEY_UP; drains stick to neutral before
returning (prevents repeat). Falls back to `keyb_poll()` if no joystick signal.

`include/charwin.c`'s `cwin_getch()` is untouched — `key_read()` wraps it.

## Help system

`help_show(screennumber)` (`src/help.c/h`): loads `MSG_HELP_SCREEN_PREFIX "<n>.BIN"`
from LOCI (1=Main, 2=Char editor, 3=Select/Move/Line-Box, 4=Write). Each is a raw
1120-byte (40×28) screen RAM dump. Path via `homedir_join()`.

**Language-specific help files** (selected at compile time via `MSG_HELP_SCREEN_PREFIX`):
- EN build: prefix `"OSEHS"` → `OSEHS1.BIN`–`OSEHS4.BIN`
- FR build: prefix `"OSEHF"` → `OSEHF1.BIN`–`OSEHF4.BIN`

`charsetswap_enter()`/`exit()` bracket the raw `$BB80` blit — the pre-rendered
image can use arbitrary attribute bytes in columns 0-1 that `cwin_clear()`-based
popups would clobber. After the key press, `canvas_blit()` + `statusbar_draw()`
restore the real canvas.

Wired to `FUNCT+8`:
- `editor.c` (Main): → `help_show(1)` unconditionally
- `charsetedit.c`: → `help_show(2)`, then redraws popup chrome
- `select.c`'s `rect_select()`: → `help_show(3)`, **guarded** to only fire
  before rect starts growing (`curx==orgx && cury==orgy`) — V1's exact guard
- `move.c`: → `help_show(3)` unconditionally
- `write.c`: → `help_show(4)`
- **Not** wired into `palette.c`/`colourpicker.c` — no V1 precedent

**Why runtime LOCI loads (not #embed):** LOCI is mandatory to boot, so there's
no "no LOCI" case these screens need to survive. Runtime loads make the title
image and help screens editable in OSE itself (they're 1120-byte screen dumps).
`make usb` distributes both language variants of title + help screens alongside
the `.tap`s (see `tools/patch_title_screen.py`, `tools/gen_help_screens.py`).

**Test impact:** `test_boot.sh` and `test_help_funct8.sh` use `--loci-flash "$SANDBOX"`
with `tests/fixtures/OSETSC.BIN`/`OSEHS{1..4}.BIN` as backing files. All other
tests use plain `--loci`; the graceful-skip means missing files don't break them.
Tests use the EN build only — FR assets are not covered by the Phosphoric suite.

## Information menu

`src/info.c/h`, Information pulldown choices 41 (Version) / 42 (Exit).

**`info_version_show()`:** 2-page popup matching `locifilemanager-v2`'s layout.
Page 1: `idi8b_logo[]` artwork (40x13, rows 0-12) + version/credits text on rows
13-27. Page 2: QR code (25x25 modules, `CH_INVSPACE`/`CH_SPACE` per module).
`menu_winsave(0, VIEWPORT_HEIGHT, 1)` / `menu_winrestore()` save/restore the whole
screen. `VIEWPORT_HEIGHT=28` (no `+1` — viewport spans the full screen).

**`info_exit()`:** `jmp ($fffc)` — software reset through the RESET vector.
**Not** a direct call to $F88F (requires physical RESET pin). Must call
`disable_overlay_ram()` immediately before the jump — otherwise reads canvas/undo
overlay data instead of the real ROM vector.

**`idi8b_logo[]` Alt-charset dependency:** the logo embeds `A_ALT` attribute bytes.
Needs CHARSET_ALT to hold the boot-time mosaic glyphs. Fixed by:
`charsetswap_capture_boot_alt()` called as **literally the first statement in
`main()`** — captures `CHARSET_ALT` into `backup_alt_boot[]` before anything can
modify it. `charsetswap_enter()`/`exit()` (via `menu_winsave(..., 1)`) then restore
from this snapshot. CHARSETROM is NOT a valid source for Alt (the Oric ROM generates
the mosaic font algorithmically via `jsr $F816`, it does not store a static copy).

## FUNCT+digit keys — real hardware mapping

`decode_funct[]` in `include/keyboard.c`: each entry at a digit's matrix position
holds the corresponding `KEY_F*` value. Specifically:
- `KEY_F1`-`KEY_F9` at matrix positions for digits 1-9
- `KEY_F10` at digit 0

`KEY_F1`-`KEY_F10` constants (`include/keyboard.h`) = `0xB1`-`0xB9` + `0xB0`
(digit's normal ASCII | 0x80) — same as CC65's `~/cc65/include/atmos.h`.

**Do NOT place F4/F8/F10 at matrix positions for letters (R/U/I/O etc.)** — the
old decode_funct[] table had this bug, causing FUNCT+8 (help) and FUNCT+4 (hex
attribute) to silently decode as nothing.

## Standalone keyboard matrix test (`kbtest/`)

`kbtest/src/main.c` + `kbtest/Makefile` — minimal program for diagnosing keyboard
matrix issues on real hardware. Isolates `include/keyboard.c`'s `keyb_scan()`/
`keyb_decode()` with no LOCI, no overlay RAM, no IJK, no charwin/menu, no canvas.
Builds ~1.7KB. `make run` in `kbtest/` launches in Oricutron.

Distributed via `make usb` (root Makefile's `usb` target has `kbtest-build`
prerequisite). Not part of `make`/`make LANG=FR`/`make test`/`make run`.

## Title and help screen asset generators

All screen-dump assets in `assets/` are generated by Python scripts in `tools/`
and checked in — the built binaries, not the scripts, are what `make usb` ships.
Run the relevant script by hand whenever content needs regenerating.

### `tools/patch_title_screen.py`

Maintains `assets/OSETSC.BIN` (EN) and generates `assets/OSETSF.BIN` (FR):

```
python3 tools/patch_title_screen.py
```

- Patches `OSETSC.BIN` row 24 (copyright year field) in place.
- Generates `OSETSF.BIN` from the EN original by:
  1. Calling `build_fr_title_art()` (see below) → overwrites rows 0-20.
  2. Patching rows 24-25 with FR credits text.
  3. Rows 21-23 and 26-27 are copied verbatim from the EN source.

### `tools/gen_title_text.py`

Imported by `patch_title_screen.py`; also runnable standalone for inspection.
Contains the mosaic letter table and `build_fr_title_art(src_bin) → bytes`.

**Oric Alt-charset mosaic encoding** (how the big letters are built):
- Glyph code N (0x20–0x7F): `N − 0x20` = 6-bit value encoding a 2-wide × 3-tall
  pixel sub-grid — bit0=TL, bit1=TR, bit2=ML, bit3=MR, bit4=BL, bit5=BR.
- Each big letter occupies **3 glyph-columns × 3 glyph-rows** (6px wide × 9px tall).

**Row payload scheme** (matches the EN original exactly):
- 2-colour words (first letter different colour): `[c1][L0][c2][L1][sp][L2]…[sp][LN-1]`
  — the second colour attr byte `c2` acts as the single-column gap between L0 and L1.
- 1-colour words: `[c1][L0][sp][L1][sp]…[sp][LN-1]` — plain `0x20` gap between all pairs.
- Lead colour (`c1`) and trail colour (the closing attr after the last glyph) are cloned
  from the corresponding EN source row via backward scan for the last attr byte (< 0x20).

**Letter table** (`LETTERS` dict): glyphs for O, R, I, C, S, E, N, D, T, F, L are
extracted from `OSETSC.BIN`. Three letters hand-designed in the same encoding:
- **A**: O's top arc + full-width crossbar at the base of the mid section + open sides
  at the bottom. Mid row hand-edited to `55 50 5a` for better legibility.
- **U**: straight sides (top + mid) + O's bottom arc.
- **P**: R's top bowl and bowl-close mid + left leg only at the bottom row.

**FR layout** (rows 0-20 of `OSETSF.BIN`):

| rows  | content        | source          |
|-------|----------------|-----------------|
| 0-4   | border + ORIC  | verbatim from EN |
| 5     | separator      | verbatim from EN |
| 6-8   | EDITEUR        | generated (replaces SCREEN) |
| 9     | separator      | verbatim from EN |
| 10-12 | ECRANS         | generated (replaces EDITOR) |
| 13    | separator      | verbatim from EN |
| 14-16 | POUR           | generated (replaces FOR) |
| 17    | separator      | verbatim from EN |
| 18-20 | LOCI           | verbatim from EN |

### `tools/gen_help_screens.py`

Generates all 8 help-screen BINs:

```
python3 tools/gen_help_screens.py
```

Outputs `assets/OSEHS{1-4}.BIN` (EN) and `assets/OSEHF{1-4}.BIN` (FR). Screen
format matches the runtime loader in `help_show()`: raw 40×28 = 1120 bytes, each
byte a serial attribute (0x00–0x1F) or character code (0x20–0x7F). The FR screens
use the same layout as the EN screens with translated text.

### `tools/gen_demo_screen.py` and `tools/gen_logo_screen.py`

One-off generators for the bundled demo project files. Not run as part of any
`make` target — run by hand if the demo content needs regenerating. Both write
multi-file project sets (`*PJ.BIN`, `*SC.BIN`, `*CS.BIN`, `*CA.BIN`) loadable
via File > Load Project inside OSE itself.

- `gen_demo_screen.py` → `assets/OSEDEMO*.BIN` — colour-swatch + pixel-art sun.
- `gen_logo_screen.py` → `assets/OSELOGO*.BIN` — Oric company logo in custom glyphs.

### `tools/gen_qr.js`

One-off Node.js generator for the QR code embedded in `src/info.c` as a
`#embed`'d byte array. Run by hand if the QR target URL changes.

## The `=`/`+` key (resolved)

`=`/`+` is at real Oric Atmos matrix row 7, **column 7** (confirmed by OSDK ART20
reference, OSDK Keyboard-FullMatrix demo, and LOCI ROM source `sodiumlb/loci-rom`).
The old `decode_normal[]`/`decode_shifted[]` had it at **column 6** — an
otherwise-unused position, so no other key was affected.

**Phosphoric `char_map` mapping is `(7,7)` (`U(7,7)`/`S(7,7)`) — do NOT change
this to `(7,6)`.** Note: Phosphoric's `char_entry_t{col,row}` has the opposite
axis labelling from this project's `keyb_matrix[row]` (bit=col). Anchor key
confirmation: `RETURN` = Phosphoric `(7,5)` vs. our row7/col5.

Both `include/keyboard.c` and `locifilemanager-v2`'s `include/keyboard.c` were
fixed. Main mode's `+` key: `case '=':` added alongside `case '+':` (was missing
the unshifted form).
