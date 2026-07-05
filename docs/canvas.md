# Canvas & Core Editing Modes

## Canvas architecture

The 40x28 **canvas** is a flat `screenmap[]` buffer (`src/canvas.c/h`), blitted
to `$BB80` via raw pointer writes. It deliberately does **not** use
`OricCharWin`/`OricViewport`/`cwin_viewport_*`: `cwin_init` clamps `sx >= 2`,
and `cwin_clear`/`row_setattr`/`cwin_viewport_blit` always overwrite columns 0-1
of every managed row with fixed ink/paper attribute bytes — incompatible with
OSE's canvas model, where the user can place arbitrary serial-attribute bytes
(0x00-0x1F) at **any** column including 0-1.

`OricCharWin`/`cwin_*` remain the right tool for menu/popup UI chrome and the
**statusbar** (row 27, `src/statusbar.c/h`). Colours: `A_FWBLACK`/`A_BGWHITE`
throughout.

`screenmap` is a pointer macro, not a real array:
`#define screenmap ((uint8_t *)CANVAS_REGION_BASE)` — the canvas lives in overlay
RAM at `$C000-$E7FF` (10240 bytes, `CANVAS_MAX_SIZE`). Every `screenmap[i]`
call site works unmodified. Canvas stride = `app.canvas_width`; rows are packed
tightly, so blitting or iterating must respect `canvas_width`, not
`VIEWPORT_WIDTH` (40). Canvas has a `CANVAS_MAX_SIZE=10240` ceiling enforced by
`canvas_resize()`.

## Cursor preview

`canvas_cursor_show(x, y)` writes `app.plotscreencode ^ 0x80` at viewport
position `(x, y)`. `canvas_cursor_hide(x, y)` restores
`canvas_get(x+xoffset, y+yoffset)` (the real content). This matches V1's exact
mechanism (`plotmove()`/`plotvisible()` in V1's `main.c`): V1 always writes
`plotscreencode+128` to show the cursor, never XORs existing content.

**Do not use a XOR/toggle approach** — it would show an inverted blank instead
of an inverted preview of `plotscreencode`.

Try mode (`t`) is different: writes `app.plotscreencode` (no `^0x80`) directly
to screen RAM — a plain non-inverted preview, matching V1's `plot_try()`.

Default `app.plotscreencode` at startup: `'@'` (0x40).

## Cursor auto-scroll

`cursor_move_scroll(dx, dy)` (`src/canvas.c/h`): at a viewport edge, if the
canvas extends further in that direction, scrolls `app.xoffset`/`yoffset` and
`canvas_blit()`s instead of refusing to move. Used by every cursor-key handler
in Main/Select/Line-Box/Write mode, and in `cursor_move_scroll()` itself.

Distinct from Move mode: auto-scroll changes `xoffset`/`yoffset` to navigate a
canvas larger than the viewport; Move mode keeps offsets fixed and rewrites cells
within the current viewport.

`cursor_move_scroll()` bounds DOWN/RIGHT to `min(VIEWPORT_HEIGHT/WIDTH,
canvas_height/width) - 1` — handles sub-viewport canvases correctly.

## Shared rectangle-grower

`rect_select(draworselect)` (`src/select.c/h`): grows a rectangle from the cursor's
starting position using cursor keys (with auto-scroll). ENTER accepts (sets file-scope
statics `select_startx/starty/endx/endy/width/height`) and returns 1; ESC cancels
and returns 0.

Redraw strategy: always recomputes `[min(origin,cursor)..max(origin,cursor)]`,
clears the old perimeter, draws the new one. When auto-scroll fires,
`canvas_blit()` clears all stale highlight bytes — only the new perimeter needs
drawing.

**`select_hollow` and `select_ellipse` flags are reset at the start of every
`rect_select()` call** but are *not* toggled during the grow loop — see Line/Box
mode below for where they are set.

## Line/Box mode

`linebox_run()` (`src/select.c`), entered via `l` from Main mode:
1. Calls `rect_select(1)` (shows plain "Line/Box" mode in statusbar throughout).
2. If accepted, enters a shape-prompt loop (sets `MODE_LINEBOX`,
   `MSG_LINEBOX_MODE_HINT`): `'o'` toggles `select_hollow`, `'c'` toggles
   `select_ellipse`, ENTER executes, ESC cancels with no canvas change.
3. On ENTER: 4-way dispatch on `(select_ellipse, select_hollow)` — filled box
   (default), hollow box, filled ellipse, hollow ellipse.

**Key gotchas:**
- `'o'`/`'c'` are **only** active in the shape-prompt loop *after* rect confirm,
  not during rect-grow. This matches Select mode's own sequencing.
- `linebox_update_hint()` uppercases `'o'`/`'c'` in the statusbar hint when the
  corresponding toggle is active (e.g. `"O:Box c:El"` when hollow is on).
- Ellipse uses an integer membership test `dx2²·ry2² + dy2²·rx2² ≤ rx2²·ry2²`
  with doubled coordinates (no float, required since build uses `-dNOFLOAT`).
  `int32_t` is sufficient; max cross-term bounded by `(rx2·ry2)² ≤ 10240²`.
- Character cells are 6x8 pixels — a square bounding box renders as a visually
  flattened ellipse. This is accepted, not corrected (consistent with Box mode).

## Select mode

`select_run()` (`src/select.c`), entered via `s` from Main mode:
1. Calls `rect_select(0)` — if cancelled, returns immediately (no action prompt).
2. If accepted, shows `MSG_SELECT_ACTION_HINT` (`"x/c/d/ipm?"`) in statusbar Mode
   field and prompts for action: `d` clear to CH_SPACE, `i` fill with plotink,
   `p` fill with 16+plotpaper, `m` fill with modifier-attribute byte,
   `x`/`c` cut/copy.

**Cut/copy (`x`/`c`):** copies one row at a time through `canvas_rowbuf[]`
(module-static 384-byte buffer in `canvas.c`, also used by `canvas_resize()` —
sharing is safe, uses are mutually exclusive). Row order is top-to-bottom for
copy, correctly handles overlap. After picking `x`/`c`, a destination cursor
loop: `cursor_move_scroll()`, FUNCT+6 toggles statusbar, ENTER confirms, ESC
cancels. On ENTER, validates fit (`MSG_SELECT_NOFIT` if not), then
`undo_snapshot()` the destination (and source for cut — **two separate undo
slots, so reverting a cut needs two `z` presses**).

## Canvas undo/redo

`src/undo.c/h`, `z`/`y` in Main mode. Ported from VDCScreenEditor2's undo
([VDCScreenEditor2/src/main.c](https://github.com/xahmol/VDCScreenEditor2/blob/main/src/main.c)), adapted for single-plane canvas
and Oric-side overlay RAM (`$E800-$FFFF`, 6144 bytes, `UNDO_REGION_SIZE`).

**Design:**
- 40-slot ring. Each slot stores a dirty-rectangle snapshot (only the changed
  bounding box) at its own byte offset in the undo region.
- A `valid` flag (not `address==0`) marks invalidated slots.
- Byte bump-pointer only resets when the next snapshot would exceed the region
  budget — history degrades gradually across a wrap, not all at once.
- **Screen > Clear/Fill is explicitly non-undoable on a large canvas.**
  `undo_snapshot()` checks `if (bytes > UNDO_REGION_SIZE) return;` rather than
  corrupting memory. This guard exists because Clear/Fill is the only operation
  that snapshots the entire canvas.
- Redo data stored alongside the original snapshot in the same slot.
- `undo_snapshot()` no-ops if `!loci_present()` — defensive, LOCI is always
  present by the time this runs.

**Every destructive action calls `undo_snapshot()` first:**
Main-mode SPACE/DEL/i/o/u, Line/Box fill, Select d/i/p/m fills and cut/copy,
Move mode per-keypress shift (whole-viewport, always 1120 bytes), Write mode
every plotted/cleared cell, Screen menu Clear/Fill (skipped if too large).

**Bug fixed alongside this:** SPACE/DEL were calling `canvas_put()` with
`cursor_x`/`cursor_y` only, missing `+xoffset`/`+yoffset` — meant they plotted
at the wrong canvas cell once the viewport had auto-scrolled. Fixed in both
`undo_snapshot()` and the `canvas_put()` calls.

**Overlay RAM is enabled once for the whole session** — do NOT add
`enable_overlay_ram()`/`disable_overlay_ram()` brackets inside undo functions.
The old brackets caused `disable_overlay_ram()` to turn off canvas access
between undo calls, corrupting every `screenmap[]` read/write in between.

## Move mode

`move_run()` (`src/move.c/h`), entered via `m`. Cursor keys shift the visible
viewport's content by one row/col in `screenmap[]` directly (leading edge fills
with CH_SPACE, trailing edge lost). ENTER and ESC both exit to Main — ESC does
**not** roll back shifts (no scratch buffer, changes are written directly).

Move mode bounds its shift rectangle's `x1`/`y1` to `canvas_width/height - 1`
to handle sub-viewport canvases correctly.

## Write mode

`write_run()` (`src/write.c/h`), entered via `w`. Key table (direct port of V1):
- Cursor keys: move with auto-scroll
- `CTRL+B`/`CTRL+A`/`CTRL+D`: toggle blink/altchar/double
- `CTRL+Z`/`CTRL+X`: cycle ink down/up
- `CTRL+C`/`CTRL+V`: cycle paper down/up
- `FUNCT+1`/`FUNCT+2`/`FUNCT+3`: plot ink/paper/modifier-attribute and advance right
- `FUNCT+4`: hex-direct attribute entry (`write_hex_attr()` — prompts 1=Ink/2=Paper/3=Mod, then a single hex digit 0-7)
- `DEL`: move left then clear that cell (backspace-style, no row-wrap) — **deliberate departure from V1** (V1 clears in-place)
- `CTRL+R`: toggle reverse-video
- Printable key: plot screencode (+0x80 if reverse-video) and advance right
- `FUNCT+6`: toggle statusbar; `ESC`: exit

**Why `FUNCT+1/2/3` not `i`/`o`/`u`:** those letters are printable in Write mode.
V1 uses `CH_F1/F2/F3` for the same actions inside `writemode()`.

`include/keyboard.h`'s Ctrl+letter decode is generic (`letter & 0x1F`) — works
for any letter. `KEY_CTRL_B/D/R/V` are defined alongside the pre-existing
`KEY_CTRL_A/C/X/Z`.

## G/I/O/U/R main-mode keys

`g` grabs the screencode/attribute under the cursor into `plot*` fields:
- `>31` → `plotscreencode`
- `<8` → `plotink`
- `>15` → `plotpaper` (minus 16, normalised to 0-7)
- `8-15` → decodes `plotaltchar`/`plotdouble`/`plotblink` from bits 0/1/2 of `grab-8`
  (normalised to 0/1 — **not** V1's raw values which violate this codebase's invariants)

`i`/`o`/`u`: plot `plotink`/`16+plotpaper`/modifier-attribute byte at cursor,
move down one row (auto-scrolling if needed).

`r`: toggles reverse-video on `plotscreencode` (XOR 0x80).

`modifier_attr_byte()` helper (`src/canvas.c/h`): packs
`8 | (plotaltchar?1:0) | (plotdouble?2:0) | (plotblink?4:0)`. Used by `u` here,
Select mode's `m` fill, and Write mode's `FUNCT+3`.

## Main-mode attribute-selection keys and statusbar

`AppState` fields: `plotink`/`plotpaper` (0-7), `plotblink`/`plotdouble`/`plotaltchar`
(0/1 toggles), `visualmap` (0/1 palette Alt-section toggle).

Main-mode key table:
- `,`/`.`: decrease/increase `plotink` (wrap 0-7)
- `;`/`'`: decrease/increase `plotpaper` (wrap 0-7)
- `b`/`d`/`a`: toggle `plotblink`/`plotdouble`/`plotaltchar`
- `0`-`9`: `plotscreencode = favourites[n]`
- `)` through `(` (SHIFT+0-9): `favourites[n] = plotscreencode`
- `p`/`c`: open palette/colour-picker popups

Statusbar (`src/statusbar.c`): `Mode | XY x,y | C <code><glyph> | S <underlying> |
I <ink><swatch> | P <paper><swatch> | A/S D/_ B/_`. XY field uses
`cursor_x + xoffset`/`cursor_y + yoffset` (absolute canvas position).

**VIEWPORT_HEIGHT = 28** (full screen). Statusbar overlays the viewport's own
last row (`STATUSBAR_ROW = VIEWPORT_HEIGHT - 1 = 27`). `statusbar_draw()`
auto-hides when `app.cursor_y == STATUSBAR_ROW` and `app.showstatusbar` is on:
calls `canvas_blit()` to show the real canvas row, tracked by `autohidden` static
so `canvas_blit()` only fires once per entry. Moving the cursor away restores
the statusbar on the next draw.

`statusbar_show(0)` (manual hide via FUNCT+6) calls `canvas_blit()` instead of
blanking — the row is real canvas, not a statusbar-only row.

## Try mode

`case 't'` in `editor.c`: writes `app.plotscreencode` to screen RAM (`$BB80`) at
the cursor — a pure preview bypassing `screenmap[]`. `key_read()` waits: SPACE
commits (`undo_snapshot()` + `canvas_put()`), anything else cancels
(`canvas_blit()` restores).

## Goto coordinates and Home

`canvas_goto(x, y)` (`src/canvas.c/h`): clamps to canvas extent, centers viewport,
sets `cursor_x/y`+`xoffset/yoffset`, `canvas_blit()`s. Used by:
- `goto_dialog()` (`src/menudata.c`, `j`): prompts for X then Y via `cwin_textinput`,
  pre-filled with current absolute position. ESC cancels.
- `h` in `editor.c`: `canvas_goto(0, 0)` — jump to origin, no popup.

## Unified Find/Replace

`findreplace_run()` (`src/findreplace.c/h`), entered via `f`:
1. Target: `1`=screencode / `2`=ink / `3`=paper. ESC cancels.
2. Find value: `cwin_textinput` hex/decimal field (pre-filled default). ESC cancels.
3. Replace value: same field shape.
   - ENTER → replace-all: `undo_snapshot(whole canvas)`, linear scan rewriting matches.
   - ESC → find-only: scan forward from after cursor, wrap around, jump to first match.

For ink/paper, candidate byte must pass `(b & 0x60) == 0` before checking range.

**Bug found here:** `cwin_textinput()`'s `VINPUT_ALPHA` flag does NOT include
digits despite its comment. Use `VINPUT_NUMS | VINPUT_ALPHA` for hex fields
(accepts 0-9 and a-f). `VINPUT_ALPHA` alone silently rejects decimal digits.
This also affected `src/charsetedit.c`'s `h` hex-row-input — fixed in both.

**`cwin_textinput` vwidth must be > maxlen** — if `vwidth == maxlen`, the viewport
scrolls the first typed digit out of view when a second digit is typed (display
bug; value is still accepted correctly). Use `vwidth = maxlen + 2` or more.
