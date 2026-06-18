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
#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "charset.h"
#include "menu.h"
#include "menudata.h"
#include "strings.h"
#include "loci.h"
#include "filepicker.h"
#include "fileio.h"

// Magic number identifying an OSE screen/combined file (FileHeader.magic) --
// a sanity check on load, not a real version negotiation.
#define FILEIO_MAGIC 0x4F53U

// app.filename + the longest suffix ("CA.BIN") + NUL.
#define FILEIO_PATH_MAXLEN (FILENAME_MAXLEN + 7)

// Header shared by "Screen" and "Combined" files (screenmap[] dimensions),
// and reused as the leading fields of Project's SC.BIN (see fileio_save_
// project()/fileio_load_project(), Part 3).
typedef struct {
    uint16_t magic;
    uint16_t width;
    uint16_t height;
} FileHeader;

/**
 * Validate a FileHeader read from a "Screen"/"Combined"/Project-SC.BIN
 * file: magic must match, and width/height must pass the same bounds
 * canvas_resize() enforces.
 *
 * @param hdr Header to validate.
 * @return 1 if valid, 0 otherwise.
 */
static uint8_t fileio_header_valid(const FileHeader *hdr)
{
    if (hdr->magic != FILEIO_MAGIC) return 0;
    if (hdr->width < VIEWPORT_WIDTH || hdr->height < VIEWPORT_HEIGHT) return 0;
    if ((uint32_t)hdr->width * hdr->height > CANVAS_MAX_SIZE) return 0;
    return 1;
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
 * Prompt for a LOCI base filename in a small popup: title (the calling
 * action's MSG_FILE_.../MSG_CHARSET_... label) on row 1, MSG_FILE_PROMPT_
 * FILENAME on row 3, then a cwin_textinput field pre-filled with
 * app.filename (persisting the last-used name as the next prompt's
 * default, matching V1's UX).
 *
 * @param title Title line (the calling action's label).
 * @return 1 if ENTER accepted (app.filename updated), 0 if ESC cancelled
 *         (app.filename unchanged).
 */
uint8_t fileio_get_filename(const char *title)
{
    OricCharWin win;
    uint8_t result;

    menu_winsave(8, 7, 1);
    cwin_init(&win, 5, 8, 30, 7, A_FWBLACK, A_BGWHITE);
    cwin_clear(&win);

    cwin_putat_string(&win, 2, 1, title);
    cwin_putat_string(&win, 2, 3, MSG_FILE_PROMPT_FILENAME);

    result = (cwin_textinput(&win, 2, 4, 24, app.filename, FILENAME_MAXLEN, VINPUT_ALPHA) >= 0) ? 1 : 0;

    menu_winrestore();
    return result;
}

/**
 * Save the canvas (no charset data) to app.filename+".BIN": a FileHeader
 * (magic, app.canvas_width/height) followed by screenmap[]
 * (canvas_width*canvas_height bytes), via loci_open()+loci_write()+
 * loci_close() -- screenmap[] is written directly from the array, no
 * staging buffer needed.
 *
 * @return (none)
 */
void fileio_save_screen(void)
{
    char       path[FILEIO_PATH_MAXLEN];
    FileHeader hdr;
    int16_t    fd;
    uint16_t   len;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_SCREEN)) return;

    sprintf(path, "%s.BIN", app.filename);
    hdr.magic  = FILEIO_MAGIC;
    hdr.width  = app.canvas_width;
    hdr.height = app.canvas_height;
    len        = (uint16_t)(app.canvas_width * app.canvas_height);

    fd = loci_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, &hdr, sizeof(hdr));
    loci_write(fd, screenmap, len);
    loci_close(fd);
}

/**
 * Load a canvas-only file saved by fileio_save_screen(): reads and
 * validates the FileHeader (fileio_header_valid()), applies the size via
 * canvas_resize(), then reads screenmap[] directly. Shows
 * MSG_FILE_INVALID_FORMAT and aborts (canvas unchanged) if the header is
 * invalid or the file can't be opened.
 *
 * @return (none)
 */
void fileio_load_screen(void)
{
    char       path[FILEIO_PATH_MAXLEN];
    FileHeader hdr;
    int16_t    fd;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_SCREEN, PICKER_FILTER_PLAIN)) return;

    sprintf(path, "%s.BIN", app.filename);

    fd = loci_open(path, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, &hdr, sizeof(hdr));
    if (!fileio_header_valid(&hdr) || !canvas_resize(hdr.width, hdr.height))
    {
        loci_close(fd);
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_read(fd, screenmap, (uint16_t)(hdr.width * hdr.height));
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
 * app.filename+".BIN": a FileHeader followed by CHARSET_GLYPH_AREA_SIZE
 * (768) bytes written directly from charset RAM (CHARSET_STD +
 * CHARSET_GLYPH_AREA_OFFSET, no copy-to-buffer step), then screenmap[].
 *
 * @return (none)
 */
void fileio_save_combined(void)
{
    char       path[FILEIO_PATH_MAXLEN];
    FileHeader hdr;
    int16_t    fd;
    uint16_t   len;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_COMBINED)) return;

    sprintf(path, "%s.BIN", app.filename);
    hdr.magic  = FILEIO_MAGIC;
    hdr.width  = app.canvas_width;
    hdr.height = app.canvas_height;
    len        = (uint16_t)(app.canvas_width * app.canvas_height);

    fd = loci_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, &hdr, sizeof(hdr));
    loci_write(fd, (const void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    loci_write(fd, screenmap, len);
    loci_close(fd);
}

/**
 * Load a file saved by fileio_save_combined(): validates the header,
 * resizes the canvas, reads CHARSET_GLYPH_AREA_SIZE bytes directly into
 * CHARSET_STD's displayable range, then reads screenmap[]. Sets
 * app.stdchanged=1 since CHARSET_STD was just replaced.
 *
 * @return (none)
 */
void fileio_load_combined(void)
{
    char       path[FILEIO_PATH_MAXLEN];
    FileHeader hdr;
    int16_t    fd;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_COMBINED, PICKER_FILTER_PLAIN)) return;

    sprintf(path, "%s.BIN", app.filename);

    fd = loci_open(path, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, &hdr, sizeof(hdr));
    if (!fileio_header_valid(&hdr) || !canvas_resize(hdr.width, hdr.height))
    {
        loci_close(fd);
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_read(fd, (void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    loci_read(fd, screenmap, (uint16_t)(hdr.width * hdr.height));
    loci_close(fd);

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
 * "<name>SC.BIN" (FileHeader + screenmap[], same shape as
 * fileio_save_screen()), and -- only if that charset was edited this
 * session (app.stdchanged/altchanged, set from charsetedit.c's
 * ce_snapshot()) -- "<name>CS.BIN"/"<name>CA.BIN" (768 raw bytes each,
 * direct from CHARSET_STD/CHARSET_ALT's displayable range).
 *
 * @return (none)
 */
void fileio_save_project(void)
{
    char          path[FILEIO_PATH_MAXLEN];
    ProjectHeader proj;
    FileHeader    hdr;
    int16_t       fd;

    if (!loci_check_present()) return;
    if (!fileio_get_filename(MSG_FILE_SAVE_PROJECT)) return;

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

    sprintf(path, "%sPJ.BIN", app.filename);
    if (file_save(path, &proj, sizeof(proj)) < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }

    hdr.magic  = FILEIO_MAGIC;
    hdr.width  = app.canvas_width;
    hdr.height = app.canvas_height;
    sprintf(path, "%sSC.BIN", app.filename);
    fd = loci_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_write(fd, &hdr, sizeof(hdr));
    loci_write(fd, screenmap, (uint16_t)(app.canvas_width * app.canvas_height));
    loci_close(fd);

    if (app.stdchanged)
    {
        sprintf(path, "%sCS.BIN", app.filename);
        file_save(path, (const void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    }
    if (app.altchanged)
    {
        sprintf(path, "%sCA.BIN", app.filename);
        file_save(path, (const void *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
    }
}

/**
 * Load a project saved by fileio_save_project(). Reads and validates
 * "<name>PJ.BIN" first (missing/invalid -> error popup, abort before
 * touching anything else); applies canvas size/cursor/viewport/plot*
 * fields; reads "<name>SC.BIN" (validated independently, same as
 * fileio_load_screen()); then, if present, loads "<name>CS.BIN"/
 * "<name>CA.BIN" into CHARSET_STD/CHARSET_ALT (missing file leaves that
 * bank untouched, matching V1).
 *
 * @return (none)
 */
void fileio_load_project(void)
{
    char          path[FILEIO_PATH_MAXLEN];
    ProjectHeader proj;
    FileHeader    hdr;
    int16_t       fd;

    if (!loci_check_present()) return;
    if (!filepicker_run(MSG_FILE_LOAD_PROJECT, PICKER_FILTER_PROJECT)) return;

    sprintf(path, "%sPJ.BIN", app.filename);
    if (file_load(path, &proj, sizeof(proj)) < 0 || proj.magic != FILEIO_MAGIC)
    {
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }

    sprintf(path, "%sSC.BIN", app.filename);
    fd = loci_open(path, O_RDONLY);
    if (fd < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }
    loci_read(fd, &hdr, sizeof(hdr));
    if (!fileio_header_valid(&hdr) || !canvas_resize(hdr.width, hdr.height))
    {
        loci_close(fd);
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
        return;
    }
    loci_read(fd, screenmap, (uint16_t)(hdr.width * hdr.height));
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

    sprintf(path, "%sCS.BIN", app.filename);
    if (file_exists(path))
    {
        file_load(path, (void *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
        app.stdchanged = 1;
    }
    sprintf(path, "%sCA.BIN", app.filename);
    if (file_exists(path))
    {
        file_load(path, (void *)(CHARSET_ALT + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE);
        app.altchanged = 1;
    }

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
 * Std/Alt load 768 raw bytes directly into that bank's displayable range.
 * Combined loads into CHARSET_STD then copies the result into CHARSET_ALT
 * too (charset_load(), include/charset.h) -- see fileio.h's header comment
 * for why. Sets app.stdchanged/altchanged for whichever bank(s) changed.
 *
 * @param altorstd 0=std, 1=alt, 2=combined.
 * @return (none)
 */
void fileio_load_charset(uint8_t altorstd)
{
    char     path[FILEIO_PATH_MAXLEN];
    uint16_t base;
    const char *title = fileio_charset_title(altorstd, 0, &base);

    if (!loci_check_present()) return;
    if (!filepicker_run(title, PICKER_FILTER_PLAIN)) return;

    sprintf(path, "%s.BIN", app.filename);
    if (file_load(path, (void *)(base + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE) < 0)
    {
        menu_messagepopup(MSG_FILE_NOT_FOUND);
        return;
    }

    if (altorstd == 2)
    {
        charset_load(CHARSET_ALT, (const uint8_t *)(CHARSET_STD + CHARSET_GLYPH_AREA_OFFSET));
        app.stdchanged = 1;
        app.altchanged = 1;
    }
    else if (altorstd == 1)
    {
        app.altchanged = 1;
    }
    else
    {
        app.stdchanged = 1;
    }
}

/**
 * Charset menu Save Standard/Alternate/Combined ('altorstd' 0/1/2).
 * Std/Alt save 768 raw bytes directly from that bank's displayable range.
 * Combined is identical to Save Std -- CHARSET_STD's range is the only
 * source there is to save (see fileio.h's header comment).
 *
 * @param altorstd 0=std, 1=alt, 2=combined.
 * @return (none)
 */
void fileio_save_charset(uint8_t altorstd)
{
    char     path[FILEIO_PATH_MAXLEN];
    uint16_t base;
    const char *title = fileio_charset_title(altorstd, 1, &base);

    if (!loci_check_present()) return;
    if (!fileio_get_filename(title)) return;

    sprintf(path, "%s.BIN", app.filename);
    if (file_save(path, (const void *)(base + CHARSET_GLYPH_AREA_OFFSET), CHARSET_GLYPH_AREA_SIZE) < 0)
        menu_messagepopup(MSG_FILE_INVALID_FORMAT);
}
