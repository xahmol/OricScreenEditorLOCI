// filepicker.c - LOCI directory browser for OricScreenEditorLOCI (Oric
// Atmos, Oscar64)
//
// Replaces src/fileio.c's typed-filename prompt for Load actions with a
// real directory listing. Adapted from locifilemanager-v2's directory
// engine (local reference at /home/xahmol/git/locifilemanager-v2/src/
// dir.c, struct DirElement/DirMeta in dir.h) -- the primary reference for
// this phase per explicit instruction. The full directory listing is read
// into a linked list stored in XRAM (RAM *on the LOCI device itself*,
// include/loci.h's xram_peek/poke/memcpy_to/memcpy_from -- a completely
// different resource from the Oric-side bank-switched overlay RAM that
// Phase 8 is about, so this has no Phase 8 dependency), simplified for
// OSE's single-pane, single-select, no-sort, dir-vs-file-only use case
// (no multi-type classification, no select bit -- see PickerMeta below
// vs. locifilemanager-v2's DirMeta).

#include <string.h>
#include "oric.h"
#include "keyboard.h"
#include "charwin.h"
#include "appstate.h"
#include "menu.h"
#include "statusbar.h"
#include "strings.h"
#include "loci.h"
#include "input.h"
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

// XRAM layout for the directory linked list -- matches
// locifilemanager-v2's DIR1BASE/DIRSIZE (dir.h), but only one list (OSE's
// picker is single-pane).
#define PICKER_DIRBASE (COPYBUF_XRAM_ADDR + COPYBUF_XRAM_SIZE)  // 0x8800
#define PICKER_DIRSIZE 0x0C00U

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
 * already calls.
 *
 * @param w     Popup window.
 * @param title Title row text.
 * @param path  Current directory path (Phase 7c: always "/" for now).
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

#define PICKER_PATH_MAXLEN 64

/**
 * Append "/name" to base (or just "name" if base already ends in '/',
 * i.e. base is the root "/"), into dest. Refuses (returns 0) rather than
 * truncate silently if the result wouldn't fit in PICKER_PATH_MAXLEN.
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
 * Truncate path to its parent directory in place (no-op if path is
 * already the root "/").
 *
 * @param path Path to truncate, PICKER_PATH_MAXLEN bytes.
 * @return (none)
 */
static void picker_path_ascend(char *path)
{
    char *lastslash = strrchr(path, '/');
    if (!lastslash) return;
    if (lastslash == path) { path[1] = '\0'; return; }
    *lastslash = '\0';
}

/**
 * Rebuild the listing for path and redraw the popup. On failure (no
 * matching entries), the caller's previous picker_firstelement/etc. are
 * already gone -- the caller must reload a known-good path afterwards if
 * it wants to keep browsing rather than give up.
 *
 * @param w      Popup window.
 * @param title  Title row text.
 * @param path   Directory to list.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @return 1 if path had matching entries (now displayed), 0 if empty.
 */
static uint8_t picker_reload(OricCharWin *w, const char *title, const char *path, uint8_t filter)
{
    picker_build_list(path, filter);
    if (!picker_firstelement) return 0;

    picker_firstprint = picker_present = picker_firstelement;
    picker_cursorrow  = 0;

    picker_draw_header(w, title, path);
    picker_draw_list(w);
    return 1;
}

/**
 * Browse the LOCI device, starting at the root, for a file matching
 * filter. UP/DOWN scroll, ENTER descends into a directory entry or
 * selects a file, LEFT goes to the parent directory (no-op at the root),
 * ESC cancels. See filepicker.h for the full contract.
 *
 * @param title  Title row text.
 * @param filter PICKER_FILTER_PLAIN or PICKER_FILTER_PROJECT.
 * @return 1 if a file was selected (app.filename set, subdirectory-
 *         prefixed if applicable), 0 if cancelled or the root has no
 *         matching entries.
 */
uint8_t filepicker_run(const char *title, uint8_t filter)
{
    OricCharWin win;
    PickerMeta  meta;
    char        path[PICKER_PATH_MAXLEN];
    uint8_t     key;
    // app.homedir (the directory oseloci.tap was booted from, see
    // src/homedir.c) is empty when LOCI can't report a CWD (e.g. no
    // drive considered "mounted/booted from" -- confirmed happening
    // under Phosphoric's --loci-flash test mode); root falls back to
    // the LOCI device's true root "/" in that case, matching this
    // function's pre-homedir behaviour exactly.
    const char *root = app.homedir[0] ? app.homedir : "/";

    // Start browsing at root -- every path this codebase opens elsewhere
    // (src/fileio.c, src/main.c, src/help.c) is resolved relative to
    // app.homedir (or "/" in the empty-homedir fallback case, since
    // homedir_join() is then a no-op prefix too), so app.filename (set
    // below on selection) must be relative to the same base.
    strcpy(path, root);

    menu_winsave(PICKER_WIN_SY, PICKER_WIN_WY, 1);

    picker_build_list(path, filter);
    if (!picker_firstelement)
    {
        menu_winrestore();
        menu_messagepopup(MSG_FILE_NO_FILES);
        return 0;
    }

    picker_firstprint = picker_present = picker_firstelement;
    picker_cursorrow  = 0;

    cwin_init(&win, PICKER_WIN_SX, PICKER_WIN_SY, PICKER_WIN_WX, PICKER_WIN_WY, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);
    picker_draw_header(&win, title, path);
    picker_draw_list(&win);

    for (;;)
    {
        key = key_read();

        if (key == KEY_ESC)
        {
            menu_winrestore();
            return 0;
        }

        if (key == KEY_LEFT)
        {
            // Floored at root -- not necessarily the LOCI device's true
            // root if app.homedir is non-empty -- since every other LOCI
            // path in this codebase assumes app.filename never needs to
            // express a path *above* app.homedir (see homedir_join(),
            // src/homedir.c).
            if (strcmp(path, root) != 0)
            {
                picker_path_ascend(path);
                if (!picker_reload(&win, title, path, filter))
                {
                    // A directory we already successfully listed on the
                    // way down can't suddenly have zero matches (it has
                    // at least the subdirectory we just came from) -- but
                    // guard anyway rather than leave a blank popup.
                    menu_winrestore();
                    return 0;
                }
            }
            continue;
        }

        if (key == KEY_ENTER)
        {
            char        name[64];
            const char *suffix;
            uint8_t     suflen, baselen;

            xram_memcpy_from(&meta, picker_present, sizeof(meta));

            if (meta.isdir)
            {
                char newpath[PICKER_PATH_MAXLEN];

                xram_memcpy_from(name, (uint16_t)(picker_present + sizeof(meta)), meta.length);
                name[meta.length] = '\0';

                if (!picker_path_descend(newpath, path, name))
                {
                    menu_messagepopup(MSG_FILE_PATH_TOO_LONG);
                    picker_reload(&win, title, path, filter);
                    continue;
                }

                if (picker_reload(&win, title, newpath, filter))
                {
                    strcpy(path, newpath);
                }
                else
                {
                    menu_messagepopup(MSG_FILE_NO_FILES);
                    picker_reload(&win, title, path, filter);
                }
                continue;
            }

            xram_memcpy_from(name, (uint16_t)(picker_present + sizeof(meta)), meta.length);
            name[meta.length] = '\0';

            // Strip the matched suffix so the base name fileio.c's
            // sprintf(path, "%s<suffix>", app.filename)-style composition
            // expects (Phase 6, unchanged) is what's left, then prepend
            // the current subdirectory relative to root (if any -- sitting
            // at root itself contributes nothing) so app.filename
            // resolves back to this file regardless of where it was
            // found -- homedir_join() (src/homedir.c) is what turns this
            // back into a full LOCI path at the actual loci_open()/
            // file_save()/file_load() call site.
            suffix  = (filter == PICKER_FILTER_PROJECT) ? "PJ.BIN" : ".BIN";
            suflen  = (uint8_t)strlen(suffix);
            baselen = (uint8_t)(meta.length - suflen);
            if (baselen > 63) baselen = 63;
            name[baselen] = '\0';

            if (strcmp(path, root) == 0)
            {
                strncpy(app.filename, name, FILENAME_MAXLEN);
                app.filename[FILENAME_MAXLEN] = '\0';
            }
            else
            {
                // path is "<root>/DIR1/DIR2" -- drop the root prefix (and
                // any leading '/' left over) so the result is
                // "DIR1/DIR2/name", a relative-to-homedir path
                // homedir_join() resolves the same way it resolves any
                // other relative path used elsewhere in this codebase.
                const char *rel = path + strlen(root);
                if (*rel == '/') rel++;
                strncpy(app.filename, rel, FILENAME_MAXLEN);
                app.filename[FILENAME_MAXLEN] = '\0';
                strncat(app.filename, "/", FILENAME_MAXLEN - strlen(app.filename));
                strncat(app.filename, name, FILENAME_MAXLEN - strlen(app.filename));
            }

            menu_winrestore();
            return 1;
        }

        if (key == KEY_DOWN)
        {
            xram_memcpy_from(&meta, picker_present, sizeof(meta));
            if (!meta.next) continue;

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
        }
        else if (key == KEY_UP)
        {
            xram_memcpy_from(&meta, picker_present, sizeof(meta));
            if (!meta.prev) continue;

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
