# OricScreenEditorLOCI — Response to Code Quality Audit

## Finding 1 — Ignored File I/O Return Values ✅ FIXED

**Confirmed genuine bug.** All four call sites were verified against the `file_save`/`file_load`
signatures (`int16_t`, negative = error).

**`fileio_save_project()` — CS.BIN and CA.BIN saves (lines 507–516):**
Wrapped each `file_save()` call in a `< 0` check with `menu_messagepopup(MSG_FILE_INVALID_FORMAT)`
+ `return`. Matches the pattern already used for the PJ.BIN save in the same function. Without
this, a failed charset save left an incomplete project on disk with no user notification.

**`fileio_load_project()` — CS.BIN and CA.BIN loads (lines 640–651):**
Converted to an `if/else` pattern so `app.stdchanged`/`app.altchanged` are only set when the
load actually succeeds. Used `if/else` rather than early `return` because the canvas (SC.BIN) was
already loaded at that point — `charsetswap_mark_changed()`, `canvas_blit()`, and
`statusbar_draw()` must still run unconditionally after both charset attempts.
(`charsetswap_mark_changed()` has documented single-call semantics that cannot be split across
the two banks; see the comment at `fileio.c:630–638`.) Returning early would leave the project
loaded but not rendered on screen, and the charset-swap mechanism uninformed about a Std charset
that may have already loaded successfully.

**Diff (save path):**

```c
// Before
if (app.stdchanged)
{
    filedir_join_suffix(fullpath, "CS.BIN");
    file_save(fullpath, charsetswap_real_std(), charset_area_size(CHARSET_STD));
}
if (app.altchanged)
{
    filedir_join_suffix(fullpath, "CA.BIN");
    file_save(fullpath, charsetswap_real_alt(), charset_area_size(CHARSET_ALT));
}

// After
if (app.stdchanged)
{
    filedir_join_suffix(fullpath, "CS.BIN");
    if (file_save(fullpath, charsetswap_real_std(), charset_area_size(CHARSET_STD)) < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
}
if (app.altchanged)
{
    filedir_join_suffix(fullpath, "CA.BIN");
    if (file_save(fullpath, charsetswap_real_alt(), charset_area_size(CHARSET_ALT)) < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
}
```

**Diff (load path):**

```c
// Before
filedir_join_suffix(fullpath, "CS.BIN");
if (file_exists(fullpath))
{
    file_load(fullpath, (void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_STD));
    app.stdchanged = 1;
}
filedir_join_suffix(fullpath, "CA.BIN");
if (file_exists(fullpath))
{
    file_load(fullpath, (void *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_ALT));
    app.altchanged = 1;
}
charsetswap_mark_changed();

// After
filedir_join_suffix(fullpath, "CS.BIN");
if (file_exists(fullpath))
{
    if (file_load(fullpath, (void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_STD)) < 0)
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
    else
        app.stdchanged = 1;
}
filedir_join_suffix(fullpath, "CA.BIN");
if (file_exists(fullpath))
{
    if (file_load(fullpath, (void *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_ALT)) < 0)
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
    else
        app.altchanged = 1;
}
charsetswap_mark_changed();
```

All 200/200 automated tests continue to pass after the fix.

---

## Finding 2 — Menu Window Buffer Stack Potential Overflow ✅ ALREADY HANDLED

The audit stated "No bounds checking in `menu_winsave()`." This is incorrect.
`menu_winsave()` (`menu.c:129–130`) already contains two guards that fire before any write:

```c
if (menu_win_depth >= MENU_WIN_DEPTH) return;
if ((uint32_t)menu_win_ptr + len > MENU_WINBUF_SIZE) return;
```

The guards are silent (`void` return) by design — a failed save means the popup is not
saved/restored, which degrades gracefully rather than crashing. The header comment documents
the worst-case peak calculation (1160 bytes observed, 1400-byte limit, ~240 bytes margin).
No code change needed.

---

## Finding 3 — Hard-Coded Buffer Sizes / No Path Validation ❌ NOT FIXING

The `filedir_join()` / `filedir_join_suffix()` functions use `strcpy`/`strcat` without runtime
bounds checks. This is intentional for this platform:

- LOCI device paths are structured: single-character drive prefix (`0:/`, `1:/`, etc.), no
  deeply nested directories in practice.
- Buffer sizes are computed with documented arithmetic and explicit slack
  (`FILEIO_FULLPATH_MAXLEN = FILEDIR_MAXLEN + FILEIO_PATH_MAXLEN + 1 + 4`,
  with a `+4` slack comment in the source).
- Oscar64 targets a 6502 with ~44 KB of usable code/data space. Adding runtime length checks
  for every path construction call costs real binary bytes with no practical benefit on this
  hardware.
- `cwin_textinput` already enforces `FILENAME_MAXLEN` at the input layer, so user-typed names
  are bounded before they reach these functions.

---

## Finding 4 — Charset Swap Complexity Risk ❌ NOT FIXING

The complexity is real but the design is fully documented and working. The swap contract is
already covered with a detailed reference in `docs/charsetedit.md` (the "Charset-swap
mechanism" section), including:

- The depth-counting rationale for nested popups.
- Which popups opt in/out (`swap_charset` parameter) and why.
- The `charsetswap_real_std()`/`charsetswap_real_alt()` requirement for all Save actions.
- The single-call rule for `charsetswap_mark_changed()`.

The mechanism has been validated across multiple real-hardware sessions. Additional code-level
assertions would add dead weight on a 6502 with a fixed binary budget.

---

## Finding 5 — Undo Snapshot Silent Skip ❌ NOT FIXING

Already handled. `undo_snapshot()` shows `MSG_UNDO_TOOBIG` via `statusbar_set_override()` +
`statusbar_draw()` when the snapshot would exceed `UNDO_REGION_SIZE` (6144 bytes). The
behaviour is intentional and documented: Screen > Clear/Fill on a large canvas is explicitly
non-undoable because a full 10 KB canvas snapshot exceeds the 6 KB undo region. Making this
"more visible" would require a blocking modal popup for a common operation, which is worse UX.

---

## Finding 6 — LOCI Single Point of Failure ❌ NOT FIXING

By design. The canvas lives in LOCI-backed overlay RAM at `$C000–$E7FF`. Without LOCI, there
is no overlay RAM and no canvas — the program cannot function. The hard boot gate
(`loci_present()` in `main()`) is the correct response. The README and CLAUDE.md both document
this as a fundamental architectural constraint, not a limitation to work around.

---

## Finding 7 — Hex Digit Parsing Fallback to 0 ❌ NOT FIXING

`write_hex_digit()` silently returns 0 for invalid input. This function is only called from
`write_hex_attr()` (FUNCT+4 in Write mode), which prompts for a single hex nibble. The
preceding `cwin_textinput` call with `VINPUT_NUMS | VINPUT_ALPHA` already filters input to
`[0-9a-fA-F]` before `write_hex_digit()` sees it. Invalid input cannot reach the fallback
under normal operation. No fix warranted.

---

## Summary

| Finding | Action |
|---|---|
| 1 — Ignored file I/O return values | **Fixed** in `src/fileio.c` (4 call sites) |
| 2 — Menu buffer overflow | No action — bounds checks already present |
| 3 — Buffer sizes / path validation | No action — by design for this platform |
| 4 — Charset swap complexity | No action — fully documented in `docs/charsetedit.md` |
| 5 — Undo silent skip | No action — already shows user message |
| 6 — LOCI dependency | No action — architectural requirement |
| 7 — Hex digit fallback | No action — input pre-filtered upstream |
