// fileio.c - LOCI file I/O for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Wires include/loci.c/h (copied verbatim from locifilemanager-v2, not
// previously #included from any src/ file) into OSE's File/Charset menus.
// Calling-convention reference: locifilemanager-v2's loci_present()
// startup gate and file_save()/file_load() single-blob convention
// (src/dir.c config_save()/config_load() there) -- adapted here to a
// per-action presence check (see loci_check_present()) rather than a
// startup-blocking one, since OSE must keep working with no LOCI device
// attached (editing, and Phosphoric/Oricutron testing of everything else).

#include <stdio.h>
#include <string.h>
#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "charset.h"
#include "menu.h"
#include "strings.h"
#include "loci.h"
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
    if (!fileio_get_filename(MSG_FILE_LOAD_SCREEN)) return;

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
    if (!fileio_get_filename(MSG_FILE_LOAD_COMBINED)) return;

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
    canvas_blit();
    statusbar_draw();
}
