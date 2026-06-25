# Boot, Help & Information

## Boot splash

`main()` loads `OSETSC.BIN` from LOCI at runtime into `SCREENMEMORY`, overlays
`MSG_SPLASH_PRESSKEY` at row 26, waits for a key. Loaded via
`loci_open`/`loci_read`/`loci_close` with `fd >= 0` guard. Missing file: skip
drawing, still wait for key. Path resolved via `homedir_join()`.

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

`help_show(screennumber)` (`src/help.c/h`): loads `OSEHS<n>.BIN` from LOCI
(1=Main, 2=Char editor, 3=Select/Move/Line-Box, 4=Write). Each is a raw 1120-byte
(40x28) screencode dump. Path via `homedir_join()`.

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
`make usb` distributes `OSETSC.BIN`/`OSEHS{1..4}.BIN` alongside the `.tap`s.

**Test impact:** `test_boot.sh` and `test_help_funct8.sh` use `--loci-flash "$SANDBOX"`
with `tests/fixtures/OSETSC.BIN`/`OSEHS{1..4}.BIN` as backing files. All other
tests use plain `--loci`; the graceful-skip means missing files don't break them.

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
