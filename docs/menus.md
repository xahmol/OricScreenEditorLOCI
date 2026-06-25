# Menu System & Popups

## Menu system

`src/menu.c/h` — 3 layers:
- **Menu bar** (row 0): `MENUBAR_MAXOPTIONS=4` (Screen/File/Charset/Information)
- **Pulldowns**: `PULLDOWN_NUMBER=5` (4 bar pulldowns + `MENU_YESNO=4` for Yes/No confirm)
  `PULLDOWN_MAXOPTIONS=8`, `PULLDOWN_MAXLENGTH=17` (16 visible chars + NUL)
- **Popup dialogs**: via `menu_winsave()`/`menu_winrestore()`

**`menu_main()` return encoding:** `menubarchoice*10 + menuoptionchoice` (1..49 =
a real choice); `menubarchoice*10 + 99` (>= 99) = ESC at bar level. Callers loop
`while (choice < 99)`.

**`menu_pulldown()` special returns:** `MENU_CANCEL=0` (ESC, `escapable=1`),
`MENU_LEFT_ARROW=18`/`MENU_RIGHT_ARROW=19` (switch bar item), else 1..N = item.

**Window save/restore is main-RAM, not overlay RAM:** `menu_winsave()`/
`menu_winrestore()` use a static `menu_winbuf[MENU_WINBUF_SIZE=2048]` LIFO
buffer (`MENU_WIN_DEPTH=9` nested saves max). Largest nested path so far
(bar + Screen pulldown + resize popup + shrink-confirm + Yes/No) = 1080 bytes.

**`menu_winsave(ypos, height, swap_charset)`:** third parameter opts the popup
in/out of the charset-swap mechanism. `menu_winrestore()` pairs
`charsetswap_enter()`/`exit()` automatically via the saved `MenuWinRecord`.
Every popup passes `1` except `charsetedit_run()` (passes `0`).

**`cwin_clear_full()` (`include/charwin.c/h`):** same as `cwin_clear()` but fills
from column 2 (not `w->sx`) through `w->sx+w->wx-1`. Use for all popups with
`sx > 2` that want a fully opaque box (resize_dialog, goto_dialog,
fileio_get_dimensions, fileio_get_filename, findreplace_run, write_hex_attr).
`charsetedit_run()`'s two `cwin_clear()` calls are untouched (sidebar popup wants
transparency left of its own `sx`).

**Pulldown row blanking:** `menu_pulldown()` blanks every covered row in full
(col0=ink, col1=endcolor, cols 2-39=spaces) right after `menu_winsave()`, before
any item is drawn. This prevents background canvas content from showing through,
and also ensures the Standard charset mode is set before pulldown text.

**`menu_wininit()`** (Are-you-sure/message popup background) also blanks cols 0
onward with the appropriate ink/paper pair + spaces.

**Trailing space after pulldown options:** `menu_draw_item()` pads to `width + 1`
(not `width`) — safe since the rightmost column at worst = 39.

**`menu_regpressure_scratch[100]`** — a real static buffer for the register-
pressure workaround in `menu_draw_item()`. **Do NOT use magic addresses like
`(uint8_t *)0xA000`** — this project's heap is degenerate (heap stubbed out,
`0xA000` falls inside live BSS). Using a literal address caused corruption of
`charsetswap.c`'s `backup_std[]` when BSS layout shifted.

## Canvas resize

`canvas_resize()` (`src/canvas.c`): validates `neww*newh <= CANVAS_MAX_SIZE` (10240)
and both dimensions non-zero. Shrinking below current size triggers
`menu_areyousure()` (Yes applies, No leaves unchanged). **No floor at
`VIEWPORT_WIDTH`/`HEIGHT`** — the interactive dialog accepts any size >= 1.

After resize in `resize_dialog()`, `canvas_goto()` re-clamps cursor/offsets into
the new (possibly smaller) extent.

**`canvas_resize_loaded()`** (`src/canvas.c/h`) — used only by
`fileio_load_project()`. Shares the core reflow logic but skips the
`VIEWPORT_WIDTH/HEIGHT` floor (file-header dimensions are never user-typed). Still
enforces `CANVAS_MAX_SIZE` ceiling and rejects zero dimensions. Blank-pads
`screenmap[]` rows `newh..VIEWPORT_HEIGHT-1` when `newh < VIEWPORT_HEIGHT` (so
`canvas_blit()` doesn't spill into overlay-RAM data past the real canvas).

**`canvas_blit()` column bound:** computes `validcols`/`validrows` from the real
canvas extent each call. Writes `CH_SPACE` for columns/rows beyond the canvas —
handles sub-viewport canvases correctly. `canvas_resize_loaded()`'s blank-pad is
belt-and-braces, not load-bearing.

## Screen > Width/Height shrink below viewport

The interactive resize dialog (`resize_dialog()`) accepts sizes below 40x28.
`canvas_blit()` handles display of narrower/shorter canvases via per-blit
bounds-checking. Sub-viewport cursor/Move-mode bounds are handled by
`cursor_move_scroll()` (bounds to `min(VIEWPORT, canvas)`) and `move_shift()`
(clamps shift rectangle to `canvas_width/height - 1`).

**Stale redraw after resize:** `resize_dialog()` tracks `resized` and, after
`menu_winrestore()` returns, calls `canvas_blit()`+`statusbar_draw()` again if
a resize happened. Without this, `menu_winrestore()` repaints the pre-resize
snapshot over the correct post-resize blit. This applies only to
`resize_dialog()` — `fileio_load_screen()`/`_combined()`/`_project()` call their
own helper popups to completion *before* `canvas_resize()`/`canvas_blit()` runs.

## Screen > Clear/Fill redraw timing

After `canvas_clear()`/`canvas_fill()`, both `case 13`/`case 14` in `menu_run()`
must call `canvas_blit()` + `statusbar_draw()` immediately. Without this,
`menu_pulldown()`'s own `menu_winrestore()` (called right after Clear/Fill is
chosen) repaints the pre-clear canvas back over the covered rows — the visual
update only appears after the whole menu bar is closed. This follows the same
pattern as `fileio.c`'s Load actions.

## Popup background bleed-through

Two separate fixes:

1. **`cwin_init()`-based popups with `sx > 2`:** `cwin_clear()` only fills from
   `w->sx` onward, leaving cols 2 to sx-1 with stale canvas content visible.
   Replaced with `cwin_clear_full()` at these call sites.

2. **Pulldowns and `menu_wininit()` popups:** `menu_pulldown()` now blanks every
   covered row in full right after `menu_winsave()`. `menu_wininit()` fills
   cols 0 onward instead of starting at cols 5/6.

## Secondary-option statusbar hints

`src/statusbar.c` has a module-static `mode_override` (`const char *`, default
`nullptr`) and `statusbar_set_override(text)`/`statusbar_clear_override()`.
`statusbar_draw()`'s Mode field reads `mode_override ? mode_override : mode_name(app.mode)`.

- **`linebox_run()`** sets `MSG_LINEBOX_MODE_HINT` (`"o:Box c:El"`) only after
  `rect_select(1)` returns 1, for the duration of the shape prompt.
- **`select_run()`** sets `MSG_SELECT_ACTION_HINT` (`"x/c/d/ipm?"`) before the
  action-key loop.

Override strings must fit `STATUSBAR_MODE_WIDTH` (10 chars).

`linebox_update_hint()` uppercases `'o'`/`'c'` in the hint string whenever the
corresponding toggle is on — assumes `'o'` is at index 0 and `'c'` at index 6
in both EN and FR strings (re-check if either string is restructured).
