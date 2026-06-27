# LOCI File I/O & File Picker

## LOCI file I/O overview

`src/fileio.c/h` — backs the File and Charset menus. LOCI is mandatory to boot
(canvas is in overlay RAM); `loci_check_present()` per-action popup is harmless
leftover defensiveness.

**File formats (final, V1-compatible):**
- **Screen** (`<name>.BIN`): bare raw dump of `screenmap[]`, no header. Load prompts
  for width and height (`fileio_get_dimensions()`, V1's exact wording).
- **Combined** (`<name>.BIN`): fixed memory-map layout — `CHARSET_STD` displayable
  range (768 bytes, $B500-$B7FF), then the full `CHARSET_ALT` region ($B800-$BB7F =
  896 bytes: 256-byte non-displayable prefix + 640-byte displayable range), then
  `screenmap[]` (40×height bytes). **Canvas must be 40×28 (2784 bytes total) or
  40×27 (2744 bytes total)**: Save rejects other dimensions, Load auto-detects height
  from `loci_lseek(SEEK_END)` — no width/height prompt. Sets both `stdchanged` and
  `altchanged` on load.
- **Project** (`<name>PJ.BIN` + `SC.BIN` + optional `CS.BIN`/`CA.BIN`): see below.
- **Charset**: Std = 768 raw bytes ($B500-$B7FF), Alt = 640 bytes ($B900-$BB7F,
  `CHARSET_ALT_GLYPH_AREA_SIZE`; Alt RAM ends at $BB80, codes 0x00-0x6F only).
  **Combined** = 1664 bytes, same layout as File > Combined without the screen:
  768 bytes Std + 256 bytes Alt non-displayable prefix ($B800-$B8FF) + 640 bytes
  Alt displayable. Saves and loads both banks.

**Save actions must read charset via `charsetswap_real_std()`/`charsetswap_real_alt()`
(returns a pointer, not a copy)** — otherwise menu-invoked Saves write the
chrome-swapped ROM glyphs instead of the user's real charset. See charset-swap
mechanism in charsetedit.md.

**AppState fields:** `stdchanged`/`altchanged` (0/1) — set by `ce_snapshot()`
(chokepoint for all destructive glyph edits). Project save writes `CS.BIN`/`CA.BIN`
only if the corresponding flag is set; project load reads them only if present.

## Project file format

**OSE-LOCI `ProjectHeader` (in `PJ.BIN`):** `uint16_t` fields for canvas
width/height, xoffset/yoffset, cursor position, and `plotscreencode`/ink/paper/
blink/double/altchar attributes, plus `stdchanged`/`altchanged` flags and a
`FILEIO_MAGIC` sentinel (`0x4F53`).

**V1 format importer:** `fileio_load_project()` reads `PJ.BIN`; if `proj.magic`
doesn't match `FILEIO_MAGIC`, falls back to `fileio_parse_v1_project()` which
parses V1's 19-byte struct. V1 bytes 0-1 (`charsetchanged[0]/[1]`, each 0 or 1)
read as a little-endian `uint16_t` can only be 0x0000/0x0001/0x0100/0x0101 —
never `0x4F53` — so the fallback is unambiguous.

**Project `SC.BIN`**: bare, no header — `proj.canvas_width/height` from `PJ.BIN`
sizes the read/write. `canvas_resize_loaded()` is called (not `canvas_resize()`)
to skip the VIEWPORT_WIDTH/HEIGHT floor — V1's default 27-row canvas must load
correctly.

**`CS.BIN`/`CA.BIN`**: raw charset dumps (768/640 bytes). Written only if
`stdchanged`/`altchanged`; read only if present. A missing file leaves that bank
untouched.

## File paths: homedir and filedir

`src/homedir.c/h`: `homedir_init()` captures `app.homedir` via `loci_getcwd()`
once at boot (before `enable_overlay_ram()`, in the same pre-overlay-RAM block as
the title-screen load). `homedir_join(out, name)` prefixes `app.homedir + "/"` onto
name. Empty `app.homedir` → no-op prefix (bare relative paths, same as pre-fix).

`app.filedir` (separate from `app.homedir`): the file picker's last-navigated
directory, persisted across File/Charset actions. `filedir_join()`/`filedir_join_suffix()`
resolve paths through it. Every `src/fileio.c` Load/Save call site uses `filedir_*`.

**Static-stack budget:** Oscar64 static stack = sum of all simultaneously-live
function locals along a call path. Large path buffers (`~120 bytes`) must be
**module-static, not function-local** — none of these functions are reentrant.
`fileio.c` has a single shared `fullpath` module-static buffer. `picker_path`/
`picker_before`/`picker_name`/`picker_newpath`/`picker_mkdir_name`/
`picker_mkdir_newpath` in `filepicker.c` are all module-static.

**`FILEIO_FULLPATH_MAXLEN`:** `HOMEDIR_MAXLEN(64) + 1 + FILENAME_MAXLEN(48) + 6 + NUL + 4`
(the `+4` is deliberate slack — comment explains the arithmetic).

## File picker

`src/filepicker.c/h`: `filepicker_run(title, filter)` for Load actions; Save
actions use `filepicker_run_save()` (tri-state: 0=cancelled, 1=existing file
selected, 2=new file).

**XRAM storage:** full directory listing in XRAM (RAM on the LOCI device, via
`xram_peek/poke/memcpy_to/from`). `PICKER_DIRBASE = 0x8800`, `PICKER_DIRSIZE = 0x0C00`.
`PickerMeta{next, prev, isdir, length}` (6 bytes) immediately before each
variable-length name entry.

**Scrolling O(1) per row:** `picker_firstprint`/`picker_present` (XRAM addresses)
+ `picker_cursorrow` follow `next`/`prev` pointers directly.

**Filtering:**
- `PICKER_FILTER_PROJECT`: Load Project only — matches `*PJ.BIN`, strips suffix
  before storing `app.filename`.
- `PICKER_FILTER_NONE`: all other Load actions — shows every file, stores
  verbatim filename. `fileio_strip_bin_suffix()` called after to restore bare
  base-name invariant (strips trailing `.BIN` case-insensitively).

**`app.filename` invariant:** must stay a bare base name (no extension) for Save's
prefill logic (`fileio_get_filename()` appends `.BIN` unconditionally). Loading via
`PICKER_FILTER_NONE` calls `fileio_strip_bin_suffix()` after storing. Save's
typed-name popup also calls `fileio_strip_bin_suffix()` after the popup confirms.

**Full navigation key set** (matches locifilemanager-v2's `dir.c`):
LEFT=ascend, RIGHT=descend-into-dir, `'t'`/`'b'`=top/bottom, `'d'`/`'p'`=pagedown/up,
`'\'`=drive root, `'.'`/`','`=cycle drives (skipping nonexistent via `locicfg.validdev[]`),
`'e'`=create-dir (mkdir + reload). No copy/rename/delete.

**Drive-prefix-aware path floor:** `picker_path_ascend()` detects `"N:/"` prefix
and floors at index 2 (not 0) — avoiding bare `"N:"` without trailing slash.

**Empty directory listings are not an error:** `picker_reload()` always succeeds
visually; an empty directory renders as a blank list. Navigation keys keep working.

**Unified Save-target picker (`PICKER_MODE_SAVE`):** a synthetic `"<new file>"`
entry pinned above the real listing. ENTER on it → returns 2 (caller prompts for
typed name). ENTER on a real file → `menu_areyousure()` overwrite confirm, on Yes
returns 1. ENTER/RIGHT on directory → descend. The sentinel is NOT a real XRAM
entry — tracked by `picker_on_newfile` flag. Visible list shrinks 1 row to make room.

**Default-cursor positioning** (`picker_apply_save_default()`): if `app.filename`
+ action suffix matches an entry in the starting directory, cursor defaults there
("save back to file I just opened"). Different directory → always defaults to sentinel.

**Oscar64 register-allocator bug:** when `picker_engine()` grew, its prologue
save-set shrank, under-protecting caller variables and corrupting screen output.
Fix: (1) extracted `picker_init_save_state()` and `picker_commit_file()` to
reduce pressure; (2) `picker_regpressure_scratch[]` — a **linker-allocated static
buffer** (not a magic address). `menu.c` has `menu_regpressure_scratch[100]`
for the same reason. **Never use magic addresses like `(uint8_t *)0xA000`** —
this falls inside live BSS; BSS layout changes with code size and corruption will
manifest unpredictably (experienced: debug string overwriting `backup_std[]`).

**`loci_readdir()` end-of-directory:** signals end with **empty `d_name`**, not
only a negative return value. Loop condition must check both:
`while ((de = loci_readdir(dir)) != 0 && de->d_name[0] != '\0')`.

**Styling:** cyan paper for unselected rows, yellow paper + `'-'` indicator for
selected. `PICKER_NAME_COLS=33`, final column writes fresh `A_BGWHITE` to stop
colour stripe at the row's right edge. 3-row key hint at bottom of popup.

**Drive validity:** `get_locicfg()` called in boot sequence (before
`enable_overlay_ram()`). `'.'`/`','` loop `while (!locicfg.validdev[drive])`.
`locicfg.validdev[0]` is always set, so the loop always terminates.

**Path row stale text:** `picker_draw_header()` pads the path to `PICKER_WIN_WX - 2`
columns using `picker_path_padded[]` (module-static) before writing.

**Subdirectory navigation:** storing a file inside a subdirectory prepends the
subdir to `app.filename` — but with the new `app.filedir` design, `app.filedir`
carries the directory and `app.filename` is just the base name. FILENAME_MAXLEN=48.

## Demo assets

`assets/PETSCII{PJ,SC,CS,CA}.BIN` — V1's original PETSCII demo project files.
Distributed by `make usb`. Load via File > Load Project (V1 importer handles
the 19-byte format). Canvas height = 27 (V1's default), handled by
`canvas_resize_loaded()`'s floor-free path.
