// fileio.c - LOCI file I/O for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Wires include/loci.c/h (copied verbatim from locifilemanager-v2, not
// previously #included from any src/ file) into OSE's File/Charset menus.
// Calling-convention reference: locifilemanager-v2's loci_present()
// startup gate and file_save()/file_load() single-blob convention
// (src/dir.c config_save()/config_load() there) -- adapted here, at the
// time this file was written, to a per-action presence check
// (loci_check_present()) rather than a startup-blocking one, since LOCI
// was still optional then. LOCI is now required to boot at all (the
// canvas itself lives in overlay RAM -- see src/main.c, CLAUDE.md
// "Canvas storage is overlay RAM, LOCI is required"), so this gate can
// never actually fail in practice any more; left in place as harmless
// defensiveness rather than removed, since it costs nothing. Byte-level
// I/O correctness is exercised by tests/scripts/test_fileio_traffic.sh
// (Phosphoric's --loci-flash gives direct host-filesystem access to
// what gets written).

#include <stdio.h>
#include <string.h>
#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "charset.h"
#include "charsetswap.h"
#include "menu.h"
#include "menudata.h"
#include "strings.h"
#include "loci.h"
#include "filepicker.h"
#include "homedir.h"
#include "fileio.h"

// Magic number identifying an OSE Project file (ProjectHeader.magic) --
// a sanity check on load, and the signal fileio_load_project() uses to
// tell OSE's own ProjectHeader apart from V1's original 19-byte layout
// (see fileio_parse_v1_project()). Screen/Combined/Project's SC.BIN
// files carry no header/magic at all -- by design, see fileio.h.
#define FILEIO_MAGIC 0x4F53U

// app.filename + the longest suffix ("CA.BIN") + NUL.
#define FILEIO_PATH_MAXLEN (FILENAME_MAXLEN + 7)

// FILEIO_PATH_MAXLEN's result, filedir_join()-ed onto app.filedir
// (2026-06-21: every File/Charset action now resolves against
// app.filedir, the file picker's last-navigated/confirmed directory --
// see homedir.h -- not app.homedir, which stays boot-assets-only), plus
// a separator -- the buffer every actual loci_open()/file_save()/
// file_load()/file_exists() call site uses. Module-static, not a
// per-function local: at ~120 bytes, a local of this size in every one
// of fileio.c's File/Charset menu action functions blew Oscar64's
// static stack budget (error 3034) -- none of these functions are
// reentrant or recursive (each runs to completion, synchronously, from
// one key/menu choice), so a single shared buffer is safe and costs
// nothing on the stack.
//
// Exact worst-case content length filedir_join_suffix() can write:
// FILEDIR_MAXLEN (64, app.filedir's longest content) + 1 (the '/'
// separator filedir_join() inserts) + FILENAME_MAXLEN (48, app.filename's
// longest content) + 6 (the longest suffix, "PJ.BIN"/"SC.BIN"/etc) + 1
// (NUL) = 120 -- which is exactly what `FILEDIR_MAXLEN +
// FILEIO_PATH_MAXLEN + 1` below evaluates to (FILEIO_PATH_MAXLEN already
// bundles FILENAME_MAXLEN + the longest suffix + NUL). That means this
// buffer fits its worst case with *zero* spare bytes -- correct as of
// 2026-06-20, but fragile: if FILEDIR_MAXLEN/FILENAME_MAXLEN/any suffix
// string ever grows without re-deriving this formula by hand, it would
// silently start overflowing. The "+ 4" below is deliberate slack
// against exactly that, not part of the real worst-case math.
#define FILEIO_FULLPATH_MAXLEN (FILEDIR_MAXLEN + FILEIO_PATH_MAXLEN + 1 + 4)
static char fullpath[FILEIO_FULLPATH_MAXLEN];

/**
 * Strip a trailing ".BIN" (case-insensitive) from app.filename in place,
 * if present. PICKER_FILTER_NONE (see filepicker.h) stores the selected
 * filename verbatim, extension included, so that arbitrary/non-OSE-
 * convention files can be loaded -- but app.filename is documented
 * (appstate.h) as a bare base name, and fileio_get_filename()'s Save
 * popup pre-fills its typed-filename field directly from app.filename's
 * current content, then unconditionally appends ".BIN" again
 * (filedir_join_suffix(fullpath, ".BIN")) on confirm. Without this
 * normalisation, loading "FOO.BIN" left app.filename holding "FOO.BIN"
 * verbatim, so the very next Save action's default text was "FOO.BIN"
 * and confirming it unedited wrote "FOO.BIN.BIN" instead of overwriting
 * "FOO.BIN" -- found 2026-06-22 (user report: "save screen and save
 * project are broken" right after loading/editing a *.BIN file). Called
 * after every PICKER_FILTER_NONE Load action's filepicker_run() returns,
 * restoring the bare-base-name invariant for the common case; a loaded
 * file with a different extension (or none) keeps its full name in
 * app.filename, since there's nothing canonical to strip -- the
 * accepted cost for that corner case is an extra ".BIN" appended if the
 * very next action is an unedited Save, same as it would have been
 * before PICKER_FILTER_NONE existed for any non-".BIN" name.
 *
 * Also called from fileio_get_filename()'s typed-name popup (2026-06-22,
 * second round, user-confirmed real-hardware repro) -- a user can type
 * (or a stale pre-fill can still hold) a name that already ends in
 * ".bin"/".Bin"/".BIN" in any case; without stripping it there too, the
 * same doubled-suffix bug reappears, this time without even an
 * overwrite-confirm to flag it (the doubled name never matches any
 * existing file). Case-insensitive (the user can type any case; the
 * filesystem itself normalises the base name but keeps ".BIN"
 * uppercase) -- a plain strcmp() would miss "foo.bin".
 *
 * @return (none)
 */
static void fileio_strip_bin_suffix(void)
{
    uint8_t len = (uint8_t)strlen(app.filename);
    uint8_t i;

    if (len < 4) return;

    for (i = 0; i < 4; i++)
    {
        char c = app.filename[len - 4 + i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c != ".BIN"[i]) return;
    }
    app.filename[len - 4] = '\0';
}

/**
 * Parse a NUL-terminated decimal digit string into a uint16_t. Stops at
 * the first non-digit. Returns 0 for an empty string. (A local copy of
 * the same tiny helper several other files in this codebase already
 * have their own copy of, e.g. menudata.c's parse_uint().)
 *
 * @param s NUL-terminated digit string.
 * @return Parsed value.
 */
static uint16_t fileio_parse_uint(const char *s)
{
    uint16_t v = 0;
    while (*s >= '0' && *s <= '9')
    {
        v = (uint16_t)(v * 10 + (uint16_t)(*s - '0'));
        s++;
    }
    return v;
}

/**
 * Prompt for screen width then height in a small popup (V1's exact
 * "Enter screen width:"/"Enter screen height:" wording, same as
 * V1's loadscreenmap()) -- used by fileio_load_screen()/
 * fileio_load_combined() before reading a bare, metadata-free screen
 * dump (see fileio.h for why these files carry no embedded size, by
 * design). Both fields are pre-filled with the current app.canvas_width/
 * height (matching V1's pre-fill). Does not itself validate the
 * entered size -- the caller passes it straight to canvas_resize() and
 * handles a 0 return from that.
 *
 * @param title Title line (the calling action's label).
 * @param outw  Out: entered width, only set if 1 is returned.
 * @param outh  Out: entered height, only set if 1 is returned.
 * @return 1 if both fields were entered (ENTER), 0 if ESC cancelled
 *         either one.
 */
static uint8_t fileio_get_dimensions(const char *title, uint16_t *outw, uint16_t *outh)
{
    OricCharWin win;
    char        bufw[6], bufh[6];
    uint8_t     ok = 0;

    menu_winsave(5, 12, 1);
    cwin_init(&win, 5, 5, 35, 12, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&win);

    cwin_putat_string(&win, 2, 1, title);
    cwin_putat_string(&win, 2, 3, MSG_FILEIO_PROMPT_WIDTH);
    sprintf(bufw, "%u", app.canvas_width);

    if (cwin_textinput(&win, 2, 4, 8, bufw, 5, VINPUT_NUMS) >= 0)
    {
        cwin_putat_string(&win, 2, 6, MSG_FILEIO_PROMPT_HEIGHT);
        sprintf(bufh, "%u", app.canvas_height);

        if (cwin_textinput(&win, 2, 7, 8, bufh, 5, VINPUT_NUMS) >= 0)
        {
            *outw = fileio_parse_uint(bufw);
            *outh = fileio_parse_uint(bufh);
            ok = 1;
        }
    }

    menu_winrestore();
    return ok;
}

/**
 * Check whether a LOCI device is present, showing MSG_LOCI_NOT_FOUND via
 * menu_messagepopup() if not. Called as the first step of every File/
 * Charset menu action, so editing and every other OSE feature keep
 * working with no LOCI device attached.
 *
 * @return 1 if a LOCI device is present, 0 if absent.
 */
uint8_t loci_check_present(void)
{
    if (loci_present()) return 1;
    menu_messagepopup(MSG_LOCI_NOT_FOUND);
    return 0;
}

/**
 * Pick a LOCI base filename to save to: one unified picker
 * (filepicker_run_save(), src/filepicker.c, 2026-06-22 redesign --
 * replaces the old two-stage "browse for a directory with 's', then
 * type a name in a separate popup, always silently overwriting"
 * flow) lets the user navigate directories, pick an existing file as
 * an overwrite target (confirmed via menu_areyousure()), or pick the
 * picker's pinned "<new file>" entry to type a brand new name instead.
 *
 * filepicker_run_save() returns a tri-state: 0 = cancelled entirely;
 * 1 = an existing file was picked and overwrite-confirmed --
 * app.filename/app.filedir are already set, nothing more to do here;
 * 2 = the "<new file>" entry was picked -- app.filedir is set, but
 * app.filename still needs a typed name, via the same small
 * cwin_textinput() popup this function always used to show
 * unconditionally (title row 1, MSG_FILE_PROMPT_FILENAME row 3,
 * pre-filled with app.filename's current value).
 *
 * @param title  Title line (the calling action's label).
 * @param filter PICKER_FILTER_PROJECT (Save Project only) or
 *               PICKER_FILTER_NONE (every other Save action).
 * @return 1 if a target is ready (app.filename/app.filedir set), 0 if
 *         cancelled at any step.
 */
uint8_t fileio_get_filename(const char *title, uint8_t filter)
{
    OricCharWin win;
    uint8_t     picked;
    uint8_t     result;

    picked = filepicker_run_save(title, filter);
    if (picked == 0) return 0;
    if (picked == 1) return 1;

    menu_winsave(8, 7, 1);
    cwin_init(&win, 5, 8, 30, 7, A_FWBLACK, A_BGWHITE);
    cwin_clear_full(&win);

    cwin_putat_string(&win, 2, 1, title);
    cwin_putat_string(&win, 2, 3, MSG_FILE_PROMPT_FILENAME);

    result = (cwin_textinput(&win, 2, 4, 24, app.filename, FILENAME_MAXLEN, VINPUT_ALPHA) >= 0) ? 1 : 0;
    if (result) fileio_strip_bin_suffix();

    menu_winrestore();
    return result;
}

/**
 * Save the canvas (no charset data) to app.filename+".BIN": a bare raw
 * dump of screenmap[] (canvas_width*canvas_height bytes), via
 * loci_open()+loci_write()+loci_close() -- no header, by design, see
 * fileio.h.
 *
 * @return (none)
 */
void fileio_save_screen(void)
{
    int16_t fd;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_SCREEN, PICKER_FILTER_NONE)) return;

    filedir_join_suffix(fullpath, ".BIN");

    fd = loci_open(fullpath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, screenmap, (uint16_t)(app.canvas_width * app.canvas_height));
    loci_close(fd);
}

/**
 * Load a canvas-only file saved by fileio_save_screen() (or any bare raw
 * screen dump from elsewhere -- see fileio.h): prompts for width/height
 * (fileio_get_dimensions(), V1's exact UX, since there is no embedded
 * size to auto-detect), applies it via canvas_resize(), then reads the
 * raw bytes directly. Shows MSG_RESIZE_INVALID and aborts (canvas
 * unchanged) if the entered size is out of bounds, MSG_FILE_NOT_FOUND if
 * the file can't be opened.
 *
 * @return (none)
 */
void fileio_load_screen(void)
{
    int16_t  fd;
    uint16_t neww, newh;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_SCREEN, PICKER_FILTER_NONE)) return;
    if (!fileio_get_dimensions(MSG_FILE_LOAD_SCREEN, &neww, &newh)) return;
    if (!canvas_resize(neww, newh))
    {
        menu_messagepopup(MSG_RESIZE_INVALID);
        return;
    }

    filedir_join(fullpath, app.filename);
    fileio_strip_bin_suffix();

    fd = loci_open(fullpath, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, screenmap, (uint16_t)(neww * newh));
    loci_close(fd);

    app.cursor_x = 0;
    app.cursor_y = 0;
    app.xoffset  = 0;
    app.yoffset  = 0;
    update_size_titles();
    canvas_blit();
    statusbar_draw();
}

/**
 * Save the canvas together with CHARSET_STD's displayable glyph range to
 * app.filename+".BIN": CHARSET_GLYPH_AREA_SIZE (768) bytes written
 * directly from charset RAM (CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET, no
 * copy-to-buffer step), immediately followed by screenmap[] -- no
 * header, by design, see fileio.h.
 *
 * @return (none)
 */
void fileio_save_combined(void)
{
    int16_t fd;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_COMBINED, PICKER_FILTER_NONE)) return;

    filedir_join_suffix(fullpath, ".BIN");

    fd = loci_open(fullpath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, (const void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    loci_write(fd, screenmap, (uint16_t)(app.canvas_width * app.canvas_height));
    loci_close(fd);
}

/**
 * Load a file saved by fileio_save_combined(): prompts for width/height
 * (fileio_get_dimensions(), same as fileio_load_screen()), resizes the
 * canvas, reads CHARSET_GLYPH_AREA_SIZE bytes directly into CHARSET_STD's
 * displayable range, then reads the raw screen bytes. Sets
 * app.stdchanged=1 since CHARSET_STD was just replaced.
 *
 * @return (none)
 */
void fileio_load_combined(void)
{
    int16_t  fd;
    uint16_t neww, newh;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_COMBINED, PICKER_FILTER_NONE)) return;
    if (!fileio_get_dimensions(MSG_FILE_LOAD_COMBINED, &neww, &newh)) return;
    if (!canvas_resize(neww, newh))
    {
        menu_messagepopup(MSG_RESIZE_INVALID);
        return;
    }

    filedir_join(fullpath, app.filename);
    fileio_strip_bin_suffix();

    fd = loci_open(fullpath, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, (void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    loci_read(fd, screenmap, (uint16_t)(neww * newh));
    loci_close(fd);

    // charsetswap_mark_changed() (not just app.stdchanged) -- this load
    // bypasses the character editor's own ce_snapshot() chokepoint, the
    // only other call site for charsetswap_mark_changed(), so without
    // this the general popup-chrome Std-charset-swap mechanism would
    // never realise CHARSET_STD just changed and would leave menus/
    // dialogs unprotected (found 2026-06-21, user report).
    charsetswap_mark_changed();
    app.stdchanged = 1;
    app.cursor_x = 0;
    app.cursor_y = 0;
    app.xoffset  = 0;
    app.yoffset  = 0;
    update_size_titles();
    canvas_blit();
    statusbar_draw();
}

// Project metadata file ("<name>PJ.BIN"), adapted from V1's 19-byte
// projbuffer -- trimmed to fields OSE actually has (no separate
// screen_col/screen_row distinct from cursor_x/cursor_y, no separate
// screentotal). Written/read as one contiguous struct via file_save()/
// file_load() (the convenience wrapper fits here, unlike the multi-part
// Screen/Combined formats).
typedef struct {
    uint16_t magic;
    uint16_t canvas_width, canvas_height;
    uint16_t cursor_x, cursor_y;
    uint16_t xoffset, yoffset;
    uint8_t  plotscreencode, plotaltchar, plotink, plotpaper;
    uint8_t  plotblink, plotdouble;
    uint8_t  stdchanged, altchanged;
} ProjectHeader;

/**
 * Save the current project to 2-4 LOCI files sharing app.filename as a
 * base name: "<name>PJ.BIN" (ProjectHeader, via file_save()),
 * "<name>SC.BIN" (bare screenmap[] dump, no header -- see fileio.h),
 * and -- only if that charset was edited this session
 * (app.stdchanged/altchanged, set from charsetedit.c's ce_snapshot()) --
 * "<name>CS.BIN"/"<name>CA.BIN" (raw charset dumps, charset_area_size()
 * bytes each, direct from CHARSET_STD/CHARSET_ALT's displayable range).
 *
 * @return (none)
 */
void fileio_save_project(void)
{
    ProjectHeader proj;
    int16_t       fd;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_PROJECT, PICKER_FILTER_PROJECT)) return;

    proj.magic          = FILEIO_MAGIC;
    proj.canvas_width    = app.canvas_width;
    proj.canvas_height   = app.canvas_height;
    proj.cursor_x        = app.cursor_x;
    proj.cursor_y        = app.cursor_y;
    proj.xoffset         = app.xoffset;
    proj.yoffset         = app.yoffset;
    proj.plotscreencode  = app.plotscreencode;
    proj.plotaltchar     = app.plotaltchar;
    proj.plotink         = app.plotink;
    proj.plotpaper       = app.plotpaper;
    proj.plotblink       = app.plotblink;
    proj.plotdouble      = app.plotdouble;
    proj.stdchanged      = app.stdchanged;
    proj.altchanged      = app.altchanged;

    filedir_join_suffix(fullpath, "PJ.BIN");
    if (file_save(fullpath, &proj, sizeof(proj)) < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }

    filedir_join_suffix(fullpath, "SC.BIN");
    fd = loci_open(fullpath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, screenmap, (uint16_t)(app.canvas_width * app.canvas_height));
    loci_close(fd);

    if (app.stdchanged)
    {
        filedir_join_suffix(fullpath, "CS.BIN");
        file_save(fullpath, (const void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_STD));
    }
    if (app.altchanged)
    {
        filedir_join_suffix(fullpath, "CA.BIN");
        file_save(fullpath, (const void *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(CHARSET_ALT));
    }
}

/**
 * Parse a buffer containing V1's original 19-byte PJ.BIN layout
 * (/home/xahmol/git/OricScreenEditor/src/main.c saveproject()) into the
 * current ProjectHeader fields, so Load Project transparently accepts
 * V1 projects too -- see fileio_load_project(). V1 byte offsets:
 *   0  charsetchanged[0] -> stdchanged
 *   1  charsetchanged[1] -> altchanged
 *   2  screen_col        -> cursor_x (same concept, direct copy)
 *   3  screen_row        -> cursor_y
 *   4-5 screenwidth (hi,lo)  -> canvas_width
 *   6-7 screenheight (hi,lo) -> canvas_height
 *   8-9 screentotal (hi,lo)  -- dropped, redundant (= width*height)
 *   10 plotink, 11 plotpaper, 12 plotblink, 13 plotdouble,
 *   14 plotaltchar, 15 plotscreencode
 *   16 -- a dead byte, never assigned in V1's own saveproject(), skipped
 *   17 xoffset (1 byte, zero-extended -- V1 itself truncates a 16-bit
 *      runtime value to this single byte on save; nothing to recover
 *      beyond whatever V1's own file already preserved)
 *   18 yoffset (1 byte, zero-extended)
 * proj->magic is deliberately left unset by this function -- the caller
 * already knows it's parsing the V1 fallback path.
 *
 * @param raw  19-byte buffer holding V1's original PJ.BIN layout.
 * @param proj Out: every ProjectHeader field except magic.
 * @return (none)
 */
static void fileio_parse_v1_project(const uint8_t *raw, ProjectHeader *proj)
{
    proj->stdchanged     = raw[0];
    proj->altchanged     = raw[1];
    proj->cursor_x       = raw[2];
    proj->cursor_y       = raw[3];
    proj->canvas_width   = (uint16_t)(raw[4] * 256 + raw[5]);
    proj->canvas_height  = (uint16_t)(raw[6] * 256 + raw[7]);
    proj->plotink        = raw[10];
    proj->plotpaper      = raw[11];
    proj->plotblink      = raw[12];
    proj->plotdouble     = raw[13];
    proj->plotaltchar    = raw[14];
    proj->plotscreencode = raw[15];
    proj->xoffset        = raw[17];
    proj->yoffset        = raw[18];
}

/**
 * Load a project saved by fileio_save_project(), or one of V1's original
 * 4-file projects (fileio_parse_v1_project()): reads "<name>PJ.BIN"
 * first (missing -> error popup, abort before touching anything else);
 * if its magic field doesn't match FILEIO_MAGIC, re-reads the same file
 * as V1's 19-byte layout instead (V1 has no magic number, but its first
 * 2 bytes -- charsetchanged[0]/[1], each 0 or 1 -- can never collide
 * with FILEIO_MAGIC, so this fallback is unambiguous). Applies canvas
 * size/cursor/viewport/plot* fields; reads "<name>SC.BIN" as a bare raw
 * dump (sized from the just-parsed canvas_width/height -- no header of
 * its own, matching V1's loadproject() exactly, see fileio.h); then, if
 * present, loads "<name>CS.BIN"/"<name>CA.BIN" into CHARSET_STD/
 * CHARSET_ALT (missing file leaves that bank untouched, matching V1).
 *
 * @return (none)
 */
void fileio_load_project(void)
{
    ProjectHeader proj;
    int16_t       fd;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_PROJECT, PICKER_FILTER_PROJECT)) return;

    filedir_join_suffix(fullpath, "PJ.BIN");
    if (file_load(fullpath, &proj, sizeof(proj)) < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    if (proj.magic != FILEIO_MAGIC)
    {
        uint8_t v1buf[19];
        if (file_load(fullpath, v1buf, sizeof(v1buf)) < (int16_t)sizeof(v1buf))
        {
            menu_messagepopup(MSG_FILE_INVALID_FORMAT);
            return;
        }
        fileio_parse_v1_project(v1buf, &proj);
    }

    if (!canvas_resize_loaded(proj.canvas_width, proj.canvas_height))
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }

    filedir_join_suffix(fullpath, "SC.BIN");
    fd = loci_open(fullpath, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, screenmap, (uint16_t)(proj.canvas_width * proj.canvas_height));
    loci_close(fd);

    app.cursor_x       = proj.cursor_x;
    app.cursor_y       = proj.cursor_y;
    app.xoffset        = proj.xoffset;
    app.yoffset        = proj.yoffset;
    app.plotscreencode = proj.plotscreencode;
    app.plotaltchar    = proj.plotaltchar;
    app.plotink        = proj.plotink;
    app.plotpaper      = proj.plotpaper;
    app.plotblink      = proj.plotblink;
    app.plotdouble     = proj.plotdouble;

    // charsetswap_mark_changed() is called only ONCE, after both possible
    // loads below, not once per bank -- found 2026-06-21: calling it
    // after the CS.BIN load and again after the CA.BIN load made the
    // second call re-capture CHARSET_STD's *currently swapped-in safe*
    // content (charsetswap_mark_changed() had already re-applied
    // CHARSETROM during the first call, if a swap session was active)
    // as the new "restore on exit" backup, discarding the correctly-
    // captured loaded-Std backup from the first call. A single call
    // covering both banks at once avoids this entirely.
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

    update_size_titles();
    canvas_blit();
    statusbar_draw();
}

/**
 * Look up the Charset menu title and charset-RAM base address for
 * altorstd (0=std, 1=alt, 2=combined -- combined uses CHARSET_STD as the
 * base for both load and save, see fileio_load_charset()/
 * fileio_save_charset()).
 *
 * @param altorstd   0/1/2, see above.
 * @param save       1 for a Save title, 0 for a Load title.
 * @param base       Out: charset-RAM bank base address.
 * @return The matching MSG_CHARSET_* title.
 */
static const char *fileio_charset_title(uint8_t altorstd, uint8_t save, uint16_t *base)
{
    if (altorstd == 1)
    {
        *base = CHARSET_ALT;
        return save ? MSG_CHARSET_SAVE_ALT : MSG_CHARSET_LOAD_ALT;
    }
    if (altorstd == 2)
    {
        *base = CHARSET_STD;
        return save ? MSG_CHARSET_SAVE_COMBINED : MSG_CHARSET_LOAD_COMBINED;
    }
    *base = CHARSET_STD;
    return save ? MSG_CHARSET_SAVE_STD : MSG_CHARSET_LOAD_STD;
}

/**
 * Charset menu Load Standard/Alternate/Combined ('altorstd' 0/1/2).
 * Std loads 768 raw bytes, Alt loads 640 (charset_area_size(), see
 * CHARSET_ALT_GLYPH_AREA_SIZE) directly into that bank's displayable
 * range. Combined loads into CHARSET_STD then copies the result into
 * CHARSET_ALT too (charset_load(), include/charset.h, 640 bytes -- see
 * fileio.h's header comment for why Combined uses CHARSET_STD as its
 * source either way). Sets app.stdchanged/altchanged for whichever
 * bank(s) changed.
 *
 * @param altorstd 0=std, 1=alt, 2=combined.
 * @return (none)
 */
void fileio_load_charset(uint8_t altorstd)
{
    uint16_t base;
    const char *title = fileio_charset_title(altorstd, 0, &base);

    if (!loci_check_present()) return;
    if (!filepicker_run(title, PICKER_FILTER_NONE)) return;

    filedir_join(fullpath, app.filename);
    fileio_strip_bin_suffix();
    if (file_load(fullpath, (void *)(base + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(base)) < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }

    if (altorstd == 2)
    {
        charset_load(CHARSET_ALT, (const uint8_t *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET));
        charsetswap_mark_changed();
        app.stdchanged = 1;
        app.altchanged = 1;
    }
    else if (altorstd == 1)
    {
        charsetswap_mark_changed();
        app.altchanged = 1;
    }
    else
    {
        charsetswap_mark_changed();
        app.stdchanged = 1;
    }
}

/**
 * Charset menu Save Standard/Alternate/Combined ('altorstd' 0/1/2).
 * Std saves 768 raw bytes, Alt saves 640 (charset_area_size(), see
 * CHARSET_ALT_GLYPH_AREA_SIZE) directly from that bank's displayable
 * range. Combined is identical to Save Std -- CHARSET_STD's range is the
 * only source there is to save (see fileio.h's header comment).
 *
 * @param altorstd 0=std, 1=alt, 2=combined.
 * @return (none)
 */
void fileio_save_charset(uint8_t altorstd)
{
    uint16_t base;
    const char *title = fileio_charset_title(altorstd, 1, &base);

    if (!loci_check_present()) return;
    if (!fileio_get_filename(title, PICKER_FILTER_NONE)) return;

    filedir_join_suffix(fullpath, ".BIN");
    if (file_save(fullpath, (const void *)(base + CHARSET_GLYPH_AREA_OFFSET), charset_area_size(base)) < 0)
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
}
