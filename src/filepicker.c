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
// picker_path_ascend()), '.'/',' switch drives outright (locifilemanager-
// v2's own next/prev-drive convention, "%u:/" reset-to-root, but without
// its locicfg.validdev[] availability check -- simpler, cycles 0-9
// unconditionally; an invalid/empty drive just shows "no files", same as
// browsing into any other empty directory). The result is stored in
// app.filedir (src/appstate.h), not app.homedir -- the file picker's own
// persisted location is deliberately kept separate from the boot-time
// splash/help asset directory.

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
#define PICKER_WIN_WX 36
#define PICKER_WIN_WY 15

#define PICKER_TITLE_Y   0
#define PICKER_PATH_Y    1
#define PICKER_LIST_Y0   2
#define PICKER_PAGE_ROWS 12
#define PICKER_NAME_COLS 34   // display width for a row (window is 36 wide)
#define PICKER_KEYS_Y    14   // key-overview row, below the list (PICKER_WIN_WY-1)

// Internal mode for picker_engine() -- not part of the public API
// (filepicker_run()/filepicker_browse_dir() below are the two public
// entry points, each fixing one of these).
#define PICKER_MODE_FILE 0   // ENTER on a file selects it (Load actions)
#define PICKER_MODE_DIR  1   // 's' confirms the current directory (Save actions);
                              // ENTER never selects a file, only descends dirs

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
 * Check whether a directory entry matches filter (PICKER_FILTER_PLAIN/
 * PROJECT). Directories always match, so navigation works regardless of
 * which file-type filter is active.
 *
 * @param de     Directory entry to check.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @return 1 if de should be shown, 0 otherwise.
 */
static uint8_t picker_entry_matches(const LociDirent *de, uint8_t filter)
{
    if (de->d_attrib & DIR_ATTR_DIR) return 1;

    if (filter == PICKER_FILTER_PROJECT)
        return picker_name_ends_with(de->d_name, "PJ.BIN");

    if (!picker_name_ends_with(de->d_name, ".BIN")) return 0;
    if (picker_name_ends_with(de->d_name, "PJ.BIN")) return 0;
    if (picker_name_ends_with(de->d_name, "SC.BIN")) return 0;
    if (picker_name_ends_with(de->d_name, "CS.BIN")) return 0;
    if (picker_name_ends_with(de->d_name, "CA.BIN")) return 0;
    return 1;
}

/**
 * Read path's directory into the XRAM linked list (picker_firstelement
 * onward), keeping only entries matching filter. Stops early (silently
 * truncating the listing), if the PICKER_DIRSIZE XRAM budget would be
 * exceeded -- same guard locifilemanager-v2's dir_read() has.
 *
 * @param path   Directory to list.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
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

/**
 * Draw the title and path rows, plus the key-overview row below the list
 * (MSG_FILE_PICKER_KEYS, user-requested 2026-06-20 -- the browser's
 * controls weren't discoverable without already knowing them from the
 * README). Redrawn on every reload (cheap, and never changes), not just
 * once at popup entry, since this is the function every redraw path
 * already calls. Still only documents the bare essentials (UpDn/Ent/Esc)
 * -- the fuller 2026-06-21 navigation key set (LEFT/RIGHT/top/bottom/
 * pgup/pgdn/root/drive-switch/mkdir) has no room left in the 36-column
 * budget, same precedent as LEFT itself not being mentioned before; the
 * README documents the full set.
 *
 * @param w     Popup window.
 * @param title Title row text.
 * @param path  Current directory path (a full, possibly drive-prefixed
 *              LOCI path -- see filepicker_run()'s header comment).
 * @return (none)
 */
static void picker_draw_header(OricCharWin *w, const char *title, const char *path)
{
    cwin_putat_string(w, 2, PICKER_TITLE_Y, title);
    cwin_putat_string(w, 2, PICKER_PATH_Y, path);
    cwin_putat_string(w, 0, PICKER_KEYS_Y, MSG_FILE_PICKER_KEYS);
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
 * indicator; the filename itself is unchanged, starting at column 2
 * (PICKER_NAME_COLS, unaffected -- columns 0/1 were already spare,
 * never part of that budget). Directory entries get a trailing '/'
 * marker. Rows beyond the end of the list are blanked (still coloured,
 * though the cursor can never actually land on one in practice).
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

    for (row = 0; row < PICKER_PAGE_ROWS; row++)
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

        selected = (row == picker_cursorrow);
        y = (uint8_t)(PICKER_LIST_Y0 + row);

        cwin_putat_char(w, 0, y, selected ? A_BGYELLOW : A_BGCYAN);
        cwin_putat_char(w, 1, y, selected ? '-' : CH_SPACE);
        cwin_putat_string(w, 2, y, line);
    }
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
 * dir_get_prev_drive()). No locicfg.validdev[] availability check (that
 * reference has one; this port deliberately doesn't, see this file's
 * header comment) -- cycling onto an invalid/empty drive just shows "no
 * files", the same as browsing into any other empty directory.
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
 * save destination, see filepicker_browse_dir(); it just renders as a
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
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @return (none)
 */
static void picker_reload(OricCharWin *w, const char *title, const char *path, uint8_t filter)
{
    picker_build_list(path, filter);
    picker_firstprint = picker_present = picker_firstelement;
    picker_cursorrow   = 0;

    picker_draw_header(w, title, path);
    picker_draw_list(w);
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

    if (picker_cursorrow + 1 < PICKER_PAGE_ROWS)
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
    for (i = 0; i < PICKER_PAGE_ROWS; i++)
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
    for (i = 0; i < PICKER_PAGE_ROWS; i++)
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
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
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
 * Shared navigation engine for both filepicker_run() (PICKER_MODE_FILE)
 * and filepicker_browse_dir() (PICKER_MODE_DIR) -- see this file's
 * header comment for the 2026-06-21 full-traversal redesign. Starts
 * browsing at app.filedir (filedir_init_default()'d if not already
 * set), not a fixed root.
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @param mode   PICKER_MODE_FILE or PICKER_MODE_DIR.
 * @return 1 on selection (app.filename set for FILE mode, app.filedir
 *         set for DIR mode), 0 if cancelled (ESC).
 */
static uint8_t picker_engine(const char *title, uint8_t filter, uint8_t mode)
{
    OricCharWin win;
    PickerMeta  meta;
    uint8_t     key;
    char       *path = picker_path;

    filedir_init_default();
    strncpy(path, app.filedir, PICKER_PATH_MAXLEN - 1);
    path[PICKER_PATH_MAXLEN - 1] = '\0';

    menu_winsave(PICKER_WIN_SY, PICKER_WIN_WY, 1);

    cwin_init(&win, PICKER_WIN_SX, PICKER_WIN_SY, PICKER_WIN_WX, PICKER_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);
    picker_reload(&win, title, path, filter);

    for (;;)
    {
        key = key_read();

        if (key == KEY_ESC)
        {
            menu_winrestore();
            return 0;
        }

        if (mode == PICKER_MODE_DIR && key == 's')
        {
            strncpy(app.filedir, path, FILEDIR_MAXLEN);
            app.filedir[FILEDIR_MAXLEN] = '\0';
            menu_winrestore();
            return 1;
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
            drive = (key == '.') ? (uint8_t)((drive + 1) % 10) : (uint8_t)((drive + 9) % 10);
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
            char       *name = picker_name;
            const char *suffix;
            uint8_t     suflen, baselen;

            // An empty listing (no entries at all in the current
            // directory -- a completely normal thing to navigate into,
            // see picker_reload()) leaves picker_present at 0: nothing
            // to act on, ENTER/RIGHT are simply no-ops here.
            if (!picker_present) continue;

            xram_memcpy_from(&meta, picker_present, sizeof(meta));

            if (meta.isdir)
            {
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
            // framing ("right to enter a dir"). Only ENTER can select a
            // file, and only in PICKER_MODE_FILE.
            if (key == KEY_RIGHT || mode == PICKER_MODE_DIR) continue;

            xram_memcpy_from(name, (uint16_t)(picker_present + sizeof(meta)), meta.length);
            name[meta.length] = '\0';

            // Strip the matched suffix so the base name fileio.c's
            // sprintf(path, "%s<suffix>", app.filename)-style composition
            // expects (see "LOCI file I/O" in CLAUDE.md, unchanged) is what's left.
            suffix  = (filter == PICKER_FILTER_PROJECT) ? "PJ.BIN" : ".BIN";
            suflen  = (uint8_t)strlen(suffix);
            baselen = (uint8_t)(meta.length - suflen);
            if (baselen > 63) baselen = 63;
            name[baselen] = '\0';

            strncpy(app.filename, name, FILENAME_MAXLEN);
            app.filename[FILENAME_MAXLEN] = '\0';
            strncpy(app.filedir, path, FILEDIR_MAXLEN);
            app.filedir[FILEDIR_MAXLEN] = '\0';

            menu_winrestore();
            return 1;
        }
        else if (key == KEY_DOWN)
        {
            if (!picker_step_down()) continue;
        }
        else if (key == KEY_UP)
        {
            if (!picker_step_up()) continue;
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
    }
}

/**
 * Browse the LOCI device, starting at app.filedir, for a file matching
 * filter. See filepicker.h and this file's header comment for the full
 * key set and contract.
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @return 1 if a file was selected (app.filename/app.filedir set), 0 if
 *         cancelled or the starting directory has no matching entries.
 */
uint8_t filepicker_run(const char *title, uint8_t filter)
{
    return picker_engine(title, filter, PICKER_MODE_FILE);
}

/**
 * Browse the LOCI device, starting at app.filedir, to choose a directory
 * to save into (added 2026-06-21) -- same navigation as filepicker_run(),
 * but 's' confirms the currently-browsed directory itself (app.filedir
 * set, app.filename untouched) instead of ENTER selecting a file (files
 * are still shown, for context, but not selectable in this mode).
 *
 * @param title  Title row text.
 * @return 1 if a directory was confirmed (app.filedir set), 0 if
 *         cancelled or the starting directory has no matching entries.
 */
uint8_t filepicker_browse_dir(const char *title)
{
    return picker_engine(title, PICKER_FILTER_PLAIN, PICKER_MODE_DIR);
}
