// filepicker.c - LOCI directory browser for OricScreenEditorLOCI (Oric
// Atmos, Oscar64)
//
// Replaces src/fileio.c's typed-filename prompt for Load actions with a
// real directory listing, and (added 2026-06-21) lets Save actions browse
// to and confirm a directory first too. Adapted from locifilemanager-v2's
// directory engine (local reference at /home/xahmol/git/locifilemanager-v2/
// src/dir.c, struct DirElement/DirMeta in dir.h) -- the primary reference
// for this, per explicit instruction (VDC Screen Editor 2's own file
// browser was also checked, but turned out to only navigate within a
// single disk image, no subdirectory tree at all, so it has nothing to add
// here). The full directory listing is read into a linked list stored in
// XRAM (RAM *on the LOCI device itself*, include/loci.h's xram_peek/poke/
// memcpy_to/memcpy_from -- a completely different resource from the
// Oric-side bank-switched overlay RAM that canvas undo/redo is about, so
// this has no dependency on that work), simplified for OSE's single-pane,
// single-select, no-sort, dir-vs-file-only use case (no multi-type
// classification, no select bit -- see PickerMeta below vs.
// locifilemanager-v2's DirMeta).
//
// Full filesystem traversal (2026-06-21, user-requested): originally
// floored at app.homedir (or "/" in the homedir-empty fallback case) --
// LEFT (ascend) could never go above that point. Now unbounded: LEFT
// ascends all the way to a drive's root ("N:/" or bare "/", see
// picker_path_ascend()), '.'/',' switch drives (locifilemanager-v2's own
// next/prev-drive convention, "%u:/" reset-to-root). The result is
// stored in app.filedir (src/appstate.h), not app.homedir -- the file
// picker's own persisted location is deliberately kept separate from the
// boot-time splash/help asset directory.
//
// Drive-switch now skips nonexistent drives (2026-06-22, user-reported):
// an earlier version of '.'/',' cycled all 10 drive numbers
// unconditionally, a deliberate simplification vs. locifilemanager-v2's
// own locicfg.validdev[] check -- the user found this confusing on real
// hardware (switching landed on drives that don't exist at all). Fixed
// by adopting locifilemanager-v2's exact loop (src/dir.c's
// dir_get_next_drive()/dir_get_prev_drive()): skip while
// !locicfg.validdev[drive]. locicfg is populated once by get_locicfg()
// (src/main.c, called in the same pre-enable_overlay_ram() boot block as
// homedir_init(), since it's itself a round of LOCI file ops -- see that
// call site's comment); drive 0 is always marked valid by get_locicfg()
// itself, so the skip-loop always terminates even if every other drive
// is absent.

#include <string.h>
#include <stdio.h>
#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "strings.h"
#include "loci.h"
#include "input.h"
#include "homedir.h"
#include "filepicker.h"

#define PICKER_WIN_SX  2
#define PICKER_WIN_SY  0
// 38, not 36: cwin_clear()/cwin_clear_full() only fill content through
// w->sx+w->wx-1 (include/charwin.c) -- with the old wx=36, that's
// absolute column 37, leaving columns 38-39 permanently untouched by
// either clear function (a structural gap, not new -- just much more
// visible once the title screen's bold stripe pattern was underneath
// it, user-reported 2026-06-22). Widening to 38 covers the full row
// (sx=2 .. sx+wx-1=39) with no other layout change needed -- every
// existing column position (PICKER_NAME_COLS's white-reset column,
// the key-hint rows) is well inside this width already, so this just
// adds blank padding at the right edge instead of leaving it bare.
#define PICKER_WIN_WX 38
#define PICKER_WIN_WY 17

#define PICKER_TITLE_Y   0
#define PICKER_PATH_Y    1
#define PICKER_LIST_Y0   2
#define PICKER_PAGE_ROWS 12
#define PICKER_NAME_COLS 33   // display width for a row (window is 36 wide:
                               // col0=paper attr, col1='-'/' ' indicator,
                               // cols2-34=name, col35=paper reset -- see
                               // picker_draw_list()'s doc comment)
#define PICKER_KEYS_Y1   14   // key-overview rows, below the list
#define PICKER_KEYS_Y2   15   // (PICKER_WIN_WY-3..PICKER_WIN_WY-1) -- grown
#define PICKER_KEYS_Y3   16   // from 1 to 3 rows to fit the full 2026-06-21
                               // navigation key set (was previously
                               // documented only in the README, see
                               // picker_draw_header()'s doc comment)

// Internal mode for picker_engine() -- not part of the public API
// (filepicker_run()/filepicker_run_save() below are the two public
// entry points, each fixing one of these).
#define PICKER_MODE_FILE 0   // ENTER on a file selects it (Load actions)
#define PICKER_MODE_SAVE 1   // ENTER on the pinned "<new file>" sentinel or
                              // an existing file picks a Save target (see
                              // picker_engine()'s doc comment for the
                              // tri-state return) -- replaces the old
                              // PICKER_MODE_DIR (2026-06-22 redesign,
                              // user-reported: "can never end as enter
                              // does not work" -- the old mode showed
                              // files for context only, never selectable,
                              // with the only way to finish being an
                              // undocumented 's' key)

// XRAM layout for the directory linked list -- matches
// locifilemanager-v2's DIR1BASE/DIRSIZE (dir.h), but only one list (OSE's
// picker is single-pane).
#define PICKER_DIRBASE (COPYBUF_XRAM_ADDR + COPYBUF_XRAM_SIZE)  // 0x8800
#define PICKER_DIRSIZE 0x0C00U

#define PICKER_PATH_MAXLEN 64

// Per-element metadata, written to XRAM immediately before the
// (variable-length) name -- elements are packed tightly, not fixed-size,
// matching locifilemanager-v2's DirElement layout. Trimmed from
// locifilemanager-v2's DirMeta: no `type` (just isdir) and no `select`
// (single-select picker).
typedef struct {
    uint16_t next;    // XRAM addr of next element, 0 = none
    uint16_t prev;    // XRAM addr of previous element, 0 = none
    uint8_t  isdir;   // 1 = directory entry, 0 = file
    uint8_t  length;  // name length
} PickerMeta;

static uint16_t picker_firstelement;  // XRAM addr of the list head, 0 = empty
static uint16_t picker_firstprint;    // XRAM addr of the row-0 entry
static uint16_t picker_present;       // XRAM addr of the highlighted entry
static uint8_t  picker_cursorrow;     // highlighted entry's visible row

// SAVE-mode state (2026-06-22 redesign) -- module-static rather than
// threaded through every helper's parameter list, matching this file's
// existing pattern (picker_firstelement etc. above). The "<new file>"
// sentinel is deliberately NOT a real XRAM/PickerMeta entry -- it's a
// fixed display row drawn by picker_draw_newfile_row(), tracked purely
// by picker_on_newfile, so none of the XRAM-list scroll/page helpers
// need to know it exists.
static uint8_t picker_mode;          // current picker_engine() mode
static uint8_t picker_on_newfile;    // 1 = cursor is on the sentinel row
static uint8_t picker_listrows;      // PICKER_PAGE_ROWS, or one less in
                                      // SAVE mode (the sentinel takes a
                                      // fixed row above the scrollable list)
static uint8_t picker_list_y0_eff;   // PICKER_LIST_Y0, or +1 in SAVE mode
static char    picker_save_match[64]; // app.filename + this action's
                                       // suffix, built once at SAVE-mode
                                       // entry -- the target basename
                                       // picker_apply_save_default() looks
                                       // for to default the cursor onto
                                       // the previously-used file

// Genuinely-reserved scratch buffer for picker_engine()'s register-
// pressure workaround (see that function's own comment) -- NOT a magic
// address. A real, linker-allocated static array, unlike the pre-
// existing src/menu.c precedent's literal `(uint8_t *)0xA000`: that
// address was documented as "unused scratch space in the heap region",
// which turned out to be wrong for this project's actual memory map
// (build/oseloci.map: `10000 - 0000 : HEAP, heap` -- an empty/degenerate
// heap, since malloc/free are fully stubbed; 0xA000 actually falls
// inside the program's own live BSS, `9438 - ac7c`). Confirmed the hard
// way (2026-06-22): reusing that same pattern at 0xA200/0xA300 for this
// function's own workaround silently corrupted real static data and
// hung the picker mid-session, with no compiler warning -- a fixed-size
// static array sidesteps the whole class of bug by construction. The
// pre-existing menu.c 0xA000 write happens to land on a byte that's
// apparently harmless in practice, but is the same kind of landmine and
// is flagged separately for follow-up, not fixed here (out of scope for
// this change).
// Sized generously above the dummy sprintf's own worst-case formatted
// length (title <=15 chars, path up to PICKER_PATH_MAXLEN-1=63 chars,
// the rest are 1-3 digit numbers + ~66 bytes of literal text -- about
// 160 bytes worst case) so that call can never overflow this buffer,
// the same way an out-of-bounds magic address could silently corrupt
// whatever followed it.
static uint8_t picker_regpressure_scratch[176];

// Module-static, not picker_engine() locals: picker_engine() already
// calls picker_make_dir() (its own static buffers, above) on the same
// call path, and Oscar64 sums every simultaneously-live function's
// locals along a call path when computing static stack usage -- stacking
// picker_engine()'s own large buffers (path/before/name/newpath, each up
// to 64 bytes) directly on top of that blew the budget (error 3034), the
// same failure class fileio.c's `fullpath` was already fixed for the
// same way. picker_engine() is never reentrant or recursive, so sharing
// static storage is safe; accessed via local pointer aliases inside
// picker_engine() so the rest of that function's logic reads unchanged.
static char picker_path[PICKER_PATH_MAXLEN];
static char picker_before[PICKER_PATH_MAXLEN];
static char picker_name[64];
static char picker_newpath[PICKER_PATH_MAXLEN];

/**
 * Check whether name ends with suffix (case-sensitive, matching how
 * fileio.c writes PJ.BIN/SC.BIN/CS.BIN/CA.BIN).
 *
 * @param name   NUL-terminated string to check.
 * @param suffix NUL-terminated suffix to look for.
 * @return 1 if name ends with suffix, 0 otherwise.
 */
static uint8_t picker_name_ends_with(const char *name, const char *suffix)
{
    uint8_t namelen = (uint8_t)strlen(name);
    uint8_t suflen  = (uint8_t)strlen(suffix);
    if (suflen > namelen) return 0;
    return strcmp(name + (namelen - suflen), suffix) == 0;
}

/**
 * Check whether a directory entry matches filter (PICKER_FILTER_PROJECT/
 * NONE). Directories always match, so navigation works regardless of
 * which file-type filter is active. PICKER_FILTER_NONE matches every
 * file unconditionally -- suffix-based filtering is reserved for the
 * Project family alone (the four-file PJ/SC/CS/CA.BIN scheme is
 * OSE-specific and self-consistent, so filtering to *PJ.BIN there is
 * still correct); every other Load/Save action needs to be able to
 * pick *any* file, including ones that don't follow OSE's own naming
 * convention at all (e.g. a raw binary dump saved by another tool, or
 * one missing the `.BIN` extension). The old PICKER_FILTER_PLAIN
 * (*.BIN-excluding-project-suffixes) branch was removed 2026-06-22
 * alongside filepicker_browse_dir(), its only caller.
 *
 * @param de     Directory entry to check.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return 1 if de should be shown, 0 otherwise.
 */
static uint8_t picker_entry_matches(const LociDirent *de, uint8_t filter)
{
    if (de->d_attrib & DIR_ATTR_DIR) return 1;

    if (filter == PICKER_FILTER_PROJECT)
        return picker_name_ends_with(de->d_name, "PJ.BIN");

    return 1;
}

/**
 * Read path's directory into the XRAM linked list (picker_firstelement
 * onward), keeping only entries matching filter. Stops early (silently
 * truncating the listing), if the PICKER_DIRSIZE XRAM budget would be
 * exceeded -- same guard locifilemanager-v2's dir_read() has.
 *
 * @param path   Directory to list.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return (none) -- result in picker_firstelement (0 if no matches).
 */
static void picker_build_list(const char *path, uint8_t filter)
{
    LociDir    *dir;
    LociDirent *de;
    PickerMeta  meta, prevmeta;
    uint16_t    address = PICKER_DIRBASE;
    uint16_t    previous = 0;

    picker_firstelement = 0;

    dir = loci_opendir(path);
    if (!dir) return;

    // loci_readdir() signals end-of-directory with an empty d_name, not
    // (only) a negative return -- locifilemanager-v2's own directory code
    // (dir.c, this loop's reference) already checks both; this loop was
    // missing the d_name check, causing an infinite loop once the real
    // entries were exhausted (confirmed: Phosphoric's --loci-flash keeps
    // returning non-error reads with an empty name past end-of-directory,
    // rather than ever returning a negative error code).
    while ((de = loci_readdir(dir)) != 0 && de->d_name[0] != '\0')
    {
        if (!picker_entry_matches(de, filter)) continue;

        meta.length = (uint8_t)strlen(de->d_name);
        if (meta.length == 0) continue;
        if ((uint32_t)address + sizeof(meta) + meta.length > (uint32_t)PICKER_DIRBASE + PICKER_DIRSIZE - 1)
            break;

        meta.isdir = (de->d_attrib & DIR_ATTR_DIR) ? 1 : 0;
        meta.next  = 0;
        meta.prev  = previous;

        if (previous)
        {
            xram_memcpy_from(&prevmeta, previous, sizeof(prevmeta));
            prevmeta.next = address;
            xram_memcpy_to(previous, &prevmeta, sizeof(prevmeta));
        }
        else
        {
            picker_firstelement = address;
        }

        xram_memcpy_to(address, &meta, sizeof(meta));
        xram_memcpy_to((uint16_t)(address + sizeof(meta)), de->d_name, meta.length);

        previous = address;
        address  = (uint16_t)(address + sizeof(meta) + meta.length);
    }

    loci_closedir(dir);
}

// PICKER_WIN_WX - 2 (the path field's window-relative start column) =
// the clipped width cwin_putat_string() actually writes for the path
// row -- module-static, not a picker_draw_header() local, same
// static-stack-budget reason as picker_path/picker_name etc. above.
static char picker_path_padded[PICKER_WIN_WX - 2 + 1];

/**
 * Draw the title and path rows, plus the 3-row key-overview block below
 * the list (MSG_FILE_PICKER_KEYS1/2/3, user-requested 2026-06-20, grown
 * to cover the full key set 2026-06-22 -- the browser's controls weren't
 * fully discoverable without already knowing them from the README:
 * the original single row only had room for UpDn/Ent/Esc, leaving the
 * 2026-06-21 navigation additions (LEFT/RIGHT/top/bottom/pgup/pgdn/root/
 * drive-switch/mkdir) undocumented on screen). Redrawn on every reload
 * (cheap, and never changes), not just once at popup entry, since this
 * is the function every redraw path already calls.
 *
 * The path row is space-padded to the field's full width before being
 * written (found and fixed 2026-06-22, user report) -- cwin_putat_string()
 * only overwrites as many columns as the string passed to it, so without
 * padding, navigating from a longer path to a shorter one left stale
 * trailing characters from the previous path visible past the new,
 * shorter one. The title row needs no such padding (its text never
 * changes after the popup opens).
 *
 * @param w     Popup window.
 * @param title Title row text.
 * @param path  Current directory path (a full, possibly drive-prefixed
 *              LOCI path -- see filepicker_run()'s header comment).
 * @return (none)
 */
static void picker_draw_header(OricCharWin *w, const char *title, const char *path)
{
    uint8_t i;

    picker_path_padded[PICKER_WIN_WX - 2] = '\0';
    strncpy(picker_path_padded, path, PICKER_WIN_WX - 2);
    for (i = (uint8_t)strlen(picker_path_padded); i < PICKER_WIN_WX - 2; i++)
        picker_path_padded[i] = ' ';
    picker_path_padded[PICKER_WIN_WX - 2] = '\0';

    cwin_putat_string(w, 2, PICKER_TITLE_Y, title);
    cwin_putat_string(w, 2, PICKER_PATH_Y, picker_path_padded);
    cwin_putat_string(w, 0, PICKER_KEYS_Y1, MSG_FILE_PICKER_KEYS1);
    cwin_putat_string(w, 0, PICKER_KEYS_Y2, MSG_FILE_PICKER_KEYS2);
    cwin_putat_string(w, 0, PICKER_KEYS_Y3, MSG_FILE_PICKER_KEYS3);
}

/**
 * Redraw all PICKER_PAGE_ROWS list rows starting from picker_firstprint,
 * following each element's `next` pointer, highlighting picker_cursorrow
 * to match this codebase's pulldown-menu convention (src/menu.c's
 * menu_draw_item(): cyan paper for unselected rows, yellow paper +
 * leading '-' for the selected one) instead of the previous whole-row
 * inverse-video (XOR 0x80) scheme -- user-requested 2026-06-20, also
 * matching the archived `nonworkingcc65` branch's own dir_print_entry()
 * (`A_BGYELLOW`/`A_BGCYAN` + a `'-'`/`' '` indicator at the column right
 * after the paper-attribute byte). Window-relative columns 0/1 (the
 * window's own base ink/paper attrs, set once by cwin_clear()'s
 * row_setattr() at absolute columns 0/1, already established the row's
 * ink before column 2 -- so only a fresh PAPER byte is needed here, no
 * separate ink byte, exactly matching nonworkingcc65's single-attribute-
 * byte approach) hold the per-row paper attribute and the '-'/' '
 * indicator; the filename itself starts at column 2 (PICKER_NAME_COLS
 * wide). Directory entries get a trailing '/' marker. Rows beyond the
 * end of the list are blanked (still coloured, though the cursor can
 * never actually land on one in practice).
 *
 * **Trailing white margin column (2026-06-22, user-requested)**: the
 * cyan/yellow paper attribute set at column 0 otherwise carries through
 * to the row's last screen column (the ULA applies an attribute byte
 * from the column it's written at onward until the next one) -- giving
 * every list row a coloured stripe running flush to the popup's right
 * edge, with no visual breathing room. PICKER_NAME_COLS shrank from 34
 * to 33 and the very last column (window-relative 35) is reset to
 * A_BGWHITE right after the name text, restoring the window's normal
 * background there for the rest of the row (the change is immediate --
 * no further attribute byte follows it on this row, so it simply stays
 * white through the right edge).
 *
 * @param w Popup window.
 * @return (none)
 */
static void picker_draw_list(OricCharWin *w)
{
    char       line[PICKER_NAME_COLS + 1];
    char       name[64];
    PickerMeta meta;
    uint16_t   addr = picker_firstprint;
    uint8_t    row, i, len, y;
    uint8_t    selected;

    for (row = 0; row < picker_listrows; row++)
    {
        if (addr)
        {
            xram_memcpy_from(&meta, addr, sizeof(meta));
            xram_memcpy_from(name, (uint16_t)(addr + sizeof(meta)), meta.length);
            name[meta.length] = '\0';

            len = (meta.length > PICKER_NAME_COLS) ? PICKER_NAME_COLS : meta.length;
            memcpy(line, name, len);
            if (meta.isdir && len < PICKER_NAME_COLS) line[len++] = '/';

            addr = meta.next;
        }
        else
        {
            len = 0;
        }

        for (i = len; i < PICKER_NAME_COLS; i++) line[i] = CH_SPACE;
        line[PICKER_NAME_COLS] = '\0';

        selected = (row == picker_cursorrow) && !picker_on_newfile;
        y = (uint8_t)(picker_list_y0_eff + row);

        cwin_putat_char(w, 0, y, selected ? A_BGYELLOW : A_BGCYAN);
        cwin_putat_char(w, 1, y, selected ? '-' : CH_SPACE);
        cwin_putat_string(w, 2, y, line);
        cwin_putat_char(w, 2 + PICKER_NAME_COLS, y, A_BGWHITE);
    }
}

/**
 * Draw the pinned "<new file>" sentinel row (SAVE mode only,
 * 2026-06-22), always at PICKER_LIST_Y0 -- one fixed display row above
 * the scrollable list, which is why picker_list_y0_eff/picker_listrows
 * shrink by 1 in this mode rather than the window growing. Same
 * yellow/cyan-paper + '-'/' '-indicator + trailing white-reset-column
 * convention as picker_draw_list()'s real rows, so it reads as part of
 * the same list rather than a separate UI element.
 *
 * @param w Popup window.
 * @return (none)
 */
static void picker_draw_newfile_row(OricCharWin *w)
{
    char    line[PICKER_NAME_COLS + 1];
    uint8_t i, len;

    len = (uint8_t)strlen(MSG_FILE_PICKER_NEWFILE);
    if (len > PICKER_NAME_COLS) len = PICKER_NAME_COLS;
    memcpy(line, MSG_FILE_PICKER_NEWFILE, len);
    for (i = len; i < PICKER_NAME_COLS; i++) line[i] = CH_SPACE;
    line[PICKER_NAME_COLS] = '\0';

    cwin_putat_char(w, 0, PICKER_LIST_Y0, picker_on_newfile ? A_BGYELLOW : A_BGCYAN);
    cwin_putat_char(w, 1, PICKER_LIST_Y0, picker_on_newfile ? '-' : CH_SPACE);
    cwin_putat_string(w, 2, PICKER_LIST_Y0, line);
    cwin_putat_char(w, 2 + PICKER_NAME_COLS, PICKER_LIST_Y0, A_BGWHITE);
}

/**
 * Append "/name" to base (or just "name" if base already ends in '/',
 * i.e. base is a drive or filesystem root), into dest. Refuses (returns
 * 0) rather than truncate silently if the result wouldn't fit in
 * PICKER_PATH_MAXLEN.
 *
 * @param dest Destination buffer, PICKER_PATH_MAXLEN bytes.
 * @param base Current path.
 * @param name Directory name to descend into.
 * @return 1 if dest was set, 0 if it would have overflowed.
 */
static uint8_t picker_path_descend(char *dest, const char *base, const char *name)
{
    uint8_t baselen    = (uint8_t)strlen(base);
    uint8_t needslash  = (baselen > 0 && base[baselen - 1] != '/') ? 1 : 0;
    uint8_t total      = (uint8_t)(baselen + needslash + strlen(name));

    if (total >= PICKER_PATH_MAXLEN) return 0;

    strcpy(dest, base);
    if (needslash) strcat(dest, "/");
    strcat(dest, name);
    return 1;
}

/**
 * Truncate path to its parent directory in place. No-op if path is
 * already at its floor -- a bare filesystem root ("/") or a drive's own
 * root ("N:/", where N is a single digit -- see picker_set_drive_root()).
 * Floor-aware as of 2026-06-21 (full filesystem traversal): previously
 * this only ever saw bare "/"-rooted paths (LEFT was floored at
 * app.homedir well before reaching here), so detecting just "is the
 * found slash at index 0" was sufficient; now that '.'/',' (drive
 * switch) can put a "N:/..." path in play, the floor slash sits at
 * index 2 instead, and naively truncating there would leave "N:"
 * (missing its trailing slash, an invalid/ambiguous path) rather than
 * "N:/".
 *
 * @param path Path to truncate, PICKER_PATH_MAXLEN bytes.
 * @return (none)
 */
static void picker_path_ascend(char *path)
{
    char   *lastslash = strrchr(path, '/');
    uint8_t floor_idx = 0;

    if (!lastslash) return;

    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':' && path[2] == '/')
        floor_idx = 2;

    if ((uint16_t)(lastslash - path) <= floor_idx)
    {
        path[floor_idx + 1] = '\0';
        return;
    }
    *lastslash = '\0';
}

/**
 * Reset path to the root of drive (locifilemanager-v2's own next/prev-
 * drive convention -- "N:/", src/dir.c's dir_get_next_drive()/
 * dir_get_prev_drive()). The caller (picker_engine()'s '.'/',' handling)
 * is responsible for only ever passing an already-validated drive number
 * (locicfg.validdev[drive] true) -- this function itself does no
 * validity check.
 *
 * @param path  Destination buffer, PICKER_PATH_MAXLEN bytes.
 * @param drive Drive number, 0-9.
 * @return (none)
 */
static void picker_set_drive_root(char *path, uint8_t drive)
{
    sprintf(path, "%u:/", drive);
}

/**
 * Parse path's leading drive number ("N:/..."), or 0 if path isn't
 * drive-prefixed (a bare "/..." path -- the common case, since most
 * sessions never press '.'/',' at all, see this file's header comment
 * for why bare paths stay the default rather than always being
 * normalized to a drive prefix).
 *
 * @param path Path to inspect.
 * @return Drive number 0-9.
 */
static uint8_t picker_current_drive(const char *path)
{
    if (path[0] >= '0' && path[0] <= '9' && path[1] == ':') return (uint8_t)(path[0] - '0');
    return 0;
}

/**
 * Rebuild the listing for path and redraw the popup -- always succeeds
 * visually, even if path turns out to have no matching entries (an empty
 * directory is a completely normal thing to navigate into or choose as a
 * save destination, see filepicker_run_save(); it just renders as a
 * blank list, picker_present left at 0/"no entry", and ENTER/UP/DOWN/etc
 * become no-ops there until the user navigates elsewhere). Earlier
 * versions of this function treated an empty result as failure and
 * expected the caller to fall back to a known-good path -- found wrong
 * 2026-06-21 while wiring up Save's directory browser: a freshly reset
 * LOCI flash root has no files yet, which made Save abort immediately,
 * before the user could even press 's' to confirm it. Load actions also
 * benefit: a Load action started in an empty directory can still
 * navigate elsewhere instead of hard-aborting on a directory with
 * nothing in it.
 *
 * @param w      Popup window.
 * @param title  Title row text.
 * @param path   Directory to list.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return (none)
 */
static void picker_reload(OricCharWin *w, const char *title, const char *path, uint8_t filter)
{
    picker_build_list(path, filter);
    picker_firstprint = picker_present = picker_firstelement;
    picker_cursorrow   = 0;
    // Every directory change defaults back onto the sentinel in SAVE
    // mode -- "previous file" context (picker_apply_save_default())
    // only ever applies once, to the starting directory, never re-shown
    // here on subsequent navigation.
    picker_on_newfile  = (picker_mode == PICKER_MODE_SAVE) ? 1 : 0;

    picker_draw_header(w, title, path);
    picker_draw_list(w);
    if (picker_mode == PICKER_MODE_SAVE) picker_draw_newfile_row(w);
}

/**
 * One-time-only default-cursor positioning for SAVE mode (2026-06-22,
 * user-requested: "default should be saving to the previously opened
 * file if applicable"). Called exactly once in picker_engine(), right
 * after its startup picker_reload() call -- not from inside
 * picker_reload() itself, so subsequent navigation to a different
 * directory does NOT re-apply this (there's no "previous file" context
 * anywhere but the starting directory). Scans the just-built listing
 * for picker_save_match (app.filename + this action's suffix, built by
 * the caller); if found, moves the cursor there instead of leaving it
 * on the sentinel, scrolling picker_firstprint if the match is beyond
 * the first visible page. No-op if picker_save_match is empty or
 * nothing matches -- the sentinel (already set by picker_reload())
 * stays the default.
 *
 * @param w Popup window (redrawn if a match was found).
 * @return (none)
 */
static void picker_apply_save_default(OricCharWin *w)
{
    uint16_t   addr = picker_firstelement;
    PickerMeta meta;
    char       name[64];
    uint8_t    row = 0;

    if (!picker_save_match[0]) return;

    while (addr)
    {
        xram_memcpy_from(&meta, addr, sizeof(meta));
        xram_memcpy_from(name, (uint16_t)(addr + sizeof(meta)), meta.length);
        name[meta.length] = '\0';

        if (!meta.isdir && strcmp(name, picker_save_match) == 0)
        {
            picker_present    = addr;
            picker_cursorrow  = row;
            picker_on_newfile = 0;
            picker_draw_list(w);
            picker_draw_newfile_row(w);
            return;
        }

        if (row + 1 < picker_listrows)
        {
            row++;
        }
        else
        {
            PickerMeta firstmeta;
            xram_memcpy_from(&firstmeta, picker_firstprint, sizeof(firstmeta));
            picker_firstprint = firstmeta.next;
        }
        addr = meta.next;
    }
}

/**
 * Move the highlighted entry to the next element, scrolling the page if
 * needed. Factored out of the former inline KEY_DOWN handler (2026-06-21)
 * so PAGEDOWN/BOTTOM (picker_pagedown()/picker_bottom() below) can reuse
 * the exact same per-step logic instead of duplicating it.
 *
 * @return 1 if moved, 0 if already at the last entry.
 */
static uint8_t picker_step_down(void)
{
    PickerMeta meta;

    if (!picker_present) return 0;

    xram_memcpy_from(&meta, picker_present, sizeof(meta));
    if (!meta.next) return 0;

    if (picker_cursorrow + 1 < picker_listrows)
    {
        picker_cursorrow++;
    }
    else
    {
        PickerMeta firstmeta;
        xram_memcpy_from(&firstmeta, picker_firstprint, sizeof(firstmeta));
        picker_firstprint = firstmeta.next;
    }
    picker_present = meta.next;
    return 1;
}

/**
 * Counterpart to picker_step_down() -- move to the previous element.
 *
 * @return 1 if moved, 0 if already at the first entry.
 */
static uint8_t picker_step_up(void)
{
    PickerMeta meta;

    if (!picker_present) return 0;

    xram_memcpy_from(&meta, picker_present, sizeof(meta));
    if (!meta.prev) return 0;

    if (picker_cursorrow > 0)
    {
        picker_cursorrow--;
    }
    else
    {
        PickerMeta firstmeta;
        xram_memcpy_from(&firstmeta, picker_firstprint, sizeof(firstmeta));
        picker_firstprint = firstmeta.prev;
    }
    picker_present = meta.prev;
    return 1;
}

/**
 * Jump to the first entry (locifilemanager-v2's 't'/dir_top()) -- O(1),
 * picker_firstelement is always known directly.
 *
 * @return (none)
 */
static void picker_top(void)
{
    if (!picker_firstelement) return;
    picker_firstprint = picker_present = picker_firstelement;
    picker_cursorrow  = 0;
}

/**
 * Jump to the last entry (locifilemanager-v2's 'b'/dir_bottom()) -- O(n),
 * no "last element" pointer is maintained, so this walks forward via
 * picker_step_down() until it can't any more (matching
 * locifilemanager-v2's own forward-walking approach).
 *
 * @return (none)
 */
static void picker_bottom(void)
{
    while (picker_step_down()) { }
}

/**
 * Move down a full page (locifilemanager-v2's 'd'/dir_pagedown()).
 *
 * @return (none)
 */
static void picker_pagedown(void)
{
    uint8_t i;
    for (i = 0; i < picker_listrows; i++)
        if (!picker_step_down()) break;
}

/**
 * Move up a full page (locifilemanager-v2's 'p'/dir_pageup()).
 *
 * @return (none)
 */
static void picker_pageup(void)
{
    uint8_t i;
    for (i = 0; i < picker_listrows; i++)
        if (!picker_step_up()) break;
}

/**
 * Prompt for a new subdirectory name and create it under path via loci_mkdir()
 * (locifilemanager-v2's 'e'/dir_newdir()), then reload path's listing so
 * the new directory shows up immediately. Silently does nothing if the
 * name is empty (ESC, or ENTER on an empty field) or the resulting path
 * would overflow PICKER_PATH_MAXLEN -- no error popup for either, since
 * both are the user simply backing out, not a real failure.
 *
 * @param w      Popup window.
 * @param title  Title row text.
 * @param path   Current directory (mkdir target's parent).
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return (none)
 */
// Module-static, not function locals: picker_make_dir() is called from
// inside picker_engine()'s own loop, on the same call path as
// picker_engine()'s own large buffers (path[]/name[]/newpath[] below) --
// stacking another ~100 bytes of locals on top blew Oscar64's static
// stack budget (error 3034), the same failure class fileio.c's `fullpath`
// was already fixed for the same way. Neither function is reentrant or
// recursive, so sharing static storage is safe.
static char picker_mkdir_name[32];
static char picker_mkdir_newpath[PICKER_PATH_MAXLEN];

static void picker_make_dir(OricCharWin *w, const char *title, const char *path, uint8_t filter)
{
    OricCharWin namewin;

    picker_mkdir_name[0] = '\0';

    menu_winsave(8, 6, 1);
    cwin_init(&namewin, 5, 8, 30, 6, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&namewin);
    cwin_putat_string(&namewin, 2, 1, MSG_FILE_PICKER_NEWDIR);

    if (cwin_textinput(&namewin, 2, 3, 24, picker_mkdir_name, 24, VINPUT_NUMS | VINPUT_ALPHA) >= 0 && picker_mkdir_name[0])
    {
        if (picker_path_descend(picker_mkdir_newpath, path, picker_mkdir_name))
            loci_mkdir(picker_mkdir_newpath);
    }

    menu_winrestore();

    picker_reload(w, title, path, filter);
}

/**
 * Shared navigation engine for filepicker_run() (PICKER_MODE_FILE) and
 * filepicker_run_save() (PICKER_MODE_SAVE) -- see this file's header
 * comment for the 2026-06-21 full-traversal redesign and
 * filepicker_run_save()'s doc comment for the 2026-06-22 SAVE-mode
 * redesign. Starts browsing at app.filedir (filedir_init_default()'d if
 * not already set), not a fixed root.
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @param mode   PICKER_MODE_FILE or PICKER_MODE_SAVE.
 * @return FILE mode: 1 on selection (app.filename/app.filedir set), 0 if
 *         cancelled (ESC). SAVE mode: 0 if cancelled; 1 if an existing
 *         file was picked and overwrite-confirmed (app.filename/
 *         app.filedir set); 2 if the "<new file>" sentinel was picked
 *         (app.filedir set, app.filename still needs a typed name).
 */
/**
 * Initialize all SAVE-mode module-static state for picker_engine() --
 * factored out of picker_engine() itself (2026-06-22) to keep that
 * function's own local-variable footprint small. Oscar64's -O2
 * whole-program register allocator computes each function's prologue
 * save/restore set from its own peak live-range pressure; the original
 * inline version of this setup (mode-dependent picker_listrows/
 * picker_list_y0_eff assignment plus the picker_save_match strncpy/
 * strcat block) pushed picker_engine()'s own pressure high enough to
 * under-count its save-set, corrupting unrelated screen RAM elsewhere
 * (src/menu.c's menu_draw_item(), see oscar64manual.md's "-O2
 * whole-program register allocator" entry) -- confirmed via a `-g` asm
 * diff against a build without this session's SAVE-mode additions.
 * Moving the logic into its own small function (rather than padding
 * picker_engine() with a dummy call, which only relocated the symptom
 * to a different corrupted register in testing) reduces picker_engine()'s
 * own pressure directly instead of fighting the allocator's heuristics.
 *
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @param mode   PICKER_MODE_FILE or PICKER_MODE_SAVE.
 * @return (none)
 */
static void picker_init_save_state(uint8_t filter, uint8_t mode)
{
    picker_mode        = mode;
    picker_listrows    = (mode == PICKER_MODE_SAVE) ? (PICKER_PAGE_ROWS - 1) : PICKER_PAGE_ROWS;
    picker_list_y0_eff = (mode == PICKER_MODE_SAVE) ? (PICKER_LIST_Y0 + 1) : PICKER_LIST_Y0;

    if (mode == PICKER_MODE_SAVE)
    {
        strncpy(picker_save_match, app.filename, sizeof(picker_save_match) - 7);
        picker_save_match[sizeof(picker_save_match) - 7] = '\0';
        strcat(picker_save_match, (filter == PICKER_FILTER_PROJECT) ? "PJ.BIN" : ".BIN");
    }
    else
    {
        picker_save_match[0] = '\0';
    }
}

/**
 * Commit picker_present (a real file, not a directory) as the picker's
 * result: extracts the name from XRAM, applies filter-specific suffix
 * stripping, and (SAVE mode only) the overwrite-confirm prompt before
 * assigning app.filename/app.filedir. Factored out of picker_engine()'s
 * ENTER-key handling (2026-06-22) to reduce that function's own local-
 * variable footprint -- the original inline version pushed
 * picker_engine()'s register pressure high enough that Oscar64's -O2
 * whole-program register allocator under-counted its prologue save/
 * restore set, corrupting unrelated screen RAM elsewhere (src/menu.c's
 * menu_draw_item(), confirmed via a `-g` asm diff -- see
 * oscar64manual.md's "-O2 whole-program register allocator" entry, and
 * picker_init_save_state()'s doc comment for the same fix applied to a
 * different part of this function).
 *
 * @param path   Current directory (set into app.filedir on commit).
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @param mode   PICKER_MODE_FILE or PICKER_MODE_SAVE.
 * @return 1 if committed (app.filename/app.filedir set), 0 if the user
 *         declined an overwrite confirmation (SAVE mode only -- FILE
 *         mode never returns 0 here).
 */
static uint8_t picker_commit_file(const char *path, uint8_t filter, uint8_t mode)
{
    char       *name = picker_name;
    const char *suffix;
    uint8_t     suflen, baselen;
    PickerMeta  meta;

    xram_memcpy_from(&meta, picker_present, sizeof(meta));
    xram_memcpy_from(name, (uint16_t)(picker_present + sizeof(meta)), meta.length);
    name[meta.length] = '\0';

    // PICKER_FILTER_PROJECT strips "PJ.BIN" so the base name fileio.c's
    // filedir_join_suffix(fullpath, "SC.BIN"/"CS.BIN"/"CA.BIN")-style
    // composition expects is what's left (see "LOCI file I/O" in
    // CLAUDE.md, unchanged). Every other filter (PICKER_FILTER_NONE --
    // see picker_entry_matches()'s doc comment) stores the selected
    // filename verbatim instead; in SAVE mode this still needs the same
    // trailing-".BIN" strip fileio_strip_bin_suffix() applies for Load,
    // so the eventual filedir_join_suffix(fullpath, ".BIN") doesn't
    // double it -- done inline here since app.filename isn't assigned
    // yet at this point.
    if (filter == PICKER_FILTER_PROJECT)
    {
        suffix  = "PJ.BIN";
        suflen  = (uint8_t)strlen(suffix);
        baselen = (uint8_t)(meta.length - suflen);
        if (baselen > 63) baselen = 63;
        name[baselen] = '\0';
    }
    else if (mode == PICKER_MODE_SAVE)
    {
        uint8_t len = (uint8_t)strlen(name);
        if (len >= 4 && strcmp(name + len - 4, ".BIN") == 0)
            name[len - 4] = '\0';
    }

    // SAVE mode: confirm before overwriting an existing file (2026-06-22,
    // user-requested, referencing locifilemanager-v2's menu_confirm_
    // file()/"File exists. Overwrite?" pattern). "No" leaves the picker
    // open (caller redraws) so the user is free to pick again.
    if (mode == PICKER_MODE_SAVE && menu_areyousure(MSG_FILE_OVERWRITE_Q) != 1)
        return 0;

    strncpy(app.filename, name, FILENAME_MAXLEN);
    app.filename[FILENAME_MAXLEN] = '\0';
    strncpy(app.filedir, path, FILEDIR_MAXLEN);
    app.filedir[FILEDIR_MAXLEN] = '\0';
    return 1;
}

static uint8_t picker_engine(const char *title, uint8_t filter, uint8_t mode)
{
    OricCharWin win;
    PickerMeta  meta;
    uint8_t     key = 0;
    char       *path = picker_path;

    filedir_init_default();
    strncpy(path, app.filedir, PICKER_PATH_MAXLEN - 1);
    path[PICKER_PATH_MAXLEN - 1] = '\0';

    picker_init_save_state(filter, mode);

    // WORKAROUND for an Oscar64 -O2 whole-program register-allocator bug
    // (see oscar64manual.md "-O2 whole-program register allocator:
    // caller-save set can be under-counted", and picker_regpressure_
    // scratch's own doc comment for why this uses a real static buffer
    // rather than a magic address). Confirmed via a `-g` asm diff:
    // picker_engine()'s own prologue save-set shrank once this function
    // grew with this session's SAVE-mode additions, under-protecting a
    // live caller variable and corrupting unrelated screen RAM elsewhere
    // (src/menu.c's menu_draw_item()). This dummy sprintf's register
    // pressure restores a large enough save set. Do not remove without
    // re-testing every Save action in the emulator (corruption/hangs can
    // reappear in a different place entirely, not just here).
    sprintf((char *)picker_regpressure_scratch, "picker_engine: title=%s, filter=%u, mode=%u, path=%s, key=%u, meta=%u, isdir=%u", title, filter, mode, path, key, meta.length, meta.isdir);

    menu_winsave(PICKER_WIN_SY, PICKER_WIN_WY, 1);

    cwin_init(&win, PICKER_WIN_SX, PICKER_WIN_SY, PICKER_WIN_WX, PICKER_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);
    picker_reload(&win, title, path, filter);
    if (mode == PICKER_MODE_SAVE) picker_apply_save_default(&win);

    for (;;)
    {
        key = key_read();

        if (key == KEY_ESC)
        {
            menu_winrestore();
            return 0;
        }

        if (key == 'e')
        {
            picker_make_dir(&win, title, path, filter);
            continue;
        }

        if (key == '\\')
        {
            picker_set_drive_root(path, picker_current_drive(path));
            picker_reload(&win, title, path, filter);
            continue;
        }

        if (key == '.' || key == ',')
        {
            uint8_t drive = picker_current_drive(path);
            do
            {
                drive = (key == '.') ? (uint8_t)((drive + 1) % 10) : (uint8_t)((drive + 9) % 10);
            } while (!locicfg.validdev[drive]);
            picker_set_drive_root(path, drive);
            picker_reload(&win, title, path, filter);
            continue;
        }

        if (key == KEY_LEFT)
        {
            picker_path_ascend(path);
            picker_reload(&win, title, path, filter);
            continue;
        }

        if (key == 't')
        {
            picker_top();
        }
        else if (key == 'b')
        {
            picker_bottom();
        }
        else if (key == 'd')
        {
            picker_pagedown();
        }
        else if (key == 'p')
        {
            picker_pageup();
        }
        else if (key == KEY_ENTER || key == KEY_RIGHT)
        {
            // SAVE mode's pinned "<new file>" sentinel -- persist the
            // browsed directory (parity with the old 's' behavior) and
            // tell the caller a typed name is still needed. Checked
            // before the empty-listing guard below since the sentinel
            // is always present/selectable even in an empty directory.
            if (mode == PICKER_MODE_SAVE && picker_on_newfile)
            {
                strncpy(app.filedir, path, FILEDIR_MAXLEN);
                app.filedir[FILEDIR_MAXLEN] = '\0';
                menu_winrestore();
                return 2;
            }

            // An empty listing (no entries at all in the current
            // directory -- a completely normal thing to navigate into,
            // see picker_reload()) leaves picker_present at 0: nothing
            // to act on, ENTER/RIGHT are simply no-ops here.
            if (!picker_present) continue;

            xram_memcpy_from(&meta, picker_present, sizeof(meta));

            if (meta.isdir)
            {
                char *name    = picker_name;
                char *newpath = picker_newpath;

                xram_memcpy_from(name, (uint16_t)(picker_present + sizeof(meta)), meta.length);
                name[meta.length] = '\0';

                if (!picker_path_descend(newpath, path, name))
                {
                    menu_messagepopup(MSG_FILE_PATH_TOO_LONG);
                    picker_reload(&win, title, path, filter);
                    continue;
                }

                strcpy(path, newpath);
                picker_reload(&win, title, path, filter);
                continue;
            }

            // RIGHT only ever descends into directories (handled above);
            // on a file entry it's a no-op, matching the user's own
            // framing ("right to enter a dir"). Only ENTER can select/
            // overwrite a file.
            if (key == KEY_RIGHT) continue;

            if (!picker_commit_file(path, filter, mode))
            {
                picker_reload(&win, title, path, filter);
                continue;
            }

            menu_winrestore();
            return 1;
        }
        else if (key == KEY_DOWN)
        {
            if (mode == PICKER_MODE_SAVE && picker_on_newfile)
            {
                if (picker_firstelement) picker_on_newfile = 0;
                else continue;
            }
            else if (!picker_step_down())
            {
                continue;
            }
        }
        else if (key == KEY_UP)
        {
            if (mode == PICKER_MODE_SAVE && !picker_on_newfile)
            {
                if (!picker_step_up()) picker_on_newfile = 1;
            }
            else if (mode != PICKER_MODE_SAVE)
            {
                if (!picker_step_up()) continue;
            }
            else
            {
                continue;  // already on the sentinel, UP is a no-op
            }
        }
        else if (key == KEY_F6)
        {
            statusbar_show((uint8_t)!app.showstatusbar);
        }
        else
        {
            continue;
        }

        picker_draw_list(&win);
        if (mode == PICKER_MODE_SAVE) picker_draw_newfile_row(&win);
    }
}

/**
 * Browse the LOCI device, starting at app.filedir, for a file matching
 * filter. See filepicker.h and this file's header comment for the full
 * key set and contract.
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return 1 if a file was selected (app.filename/app.filedir set), 0 if
 *         cancelled or the starting directory has no matching entries.
 */
uint8_t filepicker_run(const char *title, uint8_t filter)
{
    return picker_engine(title, filter, PICKER_MODE_FILE);
}

/**
 * Pick a Save target, starting at app.filedir -- same navigation as
 * filepicker_run(), but see filepicker.h's doc comment for the full
 * tri-state contract (2026-06-22 redesign, replacing the old
 * filepicker_browse_dir()/'s'-confirms-directory-only flow).
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PROJECT or PICKER_FILTER_NONE.
 * @return 0 cancelled; 1 existing file overwrite-confirmed; 2 "<new
 *         file>" picked (caller must still prompt for a name).
 */
uint8_t filepicker_run_save(const char *title, uint8_t filter)
{
    return picker_engine(title, filter, PICKER_MODE_SAVE);
}
