#ifndef APPSTATE_H
#define APPSTATE_H

#include <stdint.h>

// Visible viewport: 40 columns x 28 rows (the Oric's true full screen
// height -- see SCREEN_ROWS, include/oric.h). Was 27 (inherited from V1's
// own bug; the screen is really 28 rows) with the statusbar permanently
// occupying a separate row 27 beyond the viewport -- changed 2026-06-21
// so the cursor can reach every real screen row. The statusbar now
// overlays the viewport's own last row (VIEWPORT_HEIGHT-1) and
// auto-hides while the cursor is on it -- see statusbar.c.
#define VIEWPORT_WIDTH   40
#define VIEWPORT_HEIGHT  28

// Canvas size: starts at the viewport size, resizable up to CANVAS_MAX_SIZE
// cells (Screen > Width/Height, canvas_resize()). screenmap[] (src/canvas.h)
// lives in Oric-side overlay RAM ($C000-$FFFF, OVERLAY_BASE/OVERLAY_SIZE in
// oric.h) rather than main RAM -- LOCI is required to run the program at
// all (see src/main.c's boot gate). 10240 (0x2800) is the canvas's share of
// the 16KB overlay region; the remaining 6144 (0x1800) bytes go to undo
// (src/undo.c's UNDO_REGION_SIZE) -- see CLAUDE.md "Memory Layout".
#define CANVAS_WIDTH     VIEWPORT_WIDTH
#define CANVAS_HEIGHT    VIEWPORT_HEIGHT
#define CANVAS_MAX_SIZE  10240

// Character editor favourites (0-9 select, SHIFT+0-9 store).
#define FAVOURITES_COUNT 10

// LOCI file-I/O base filename, persisted across File/Charset menu actions
// as the next prompt's default. LociDirent.d_name (include/loci.h) is 64
// bytes; 48 leaves comfortable room for the longest suffix ("CA.BIN" = 6)
// plus a subdirectory-prefixed name from src/filepicker.c's browser.
#define FILENAME_MAXLEN 48

// LOCI's boot-time current working directory (the directory oseloci.tap
// was actually loaded from, per loci_getcwd() -- src/homedir.c), captured
// once at startup. Used ONLY for the boot-time assets (splash/help
// screens, src/main.c/src/help.c) -- NOT for File/Charset menu actions
// any more (see app.filedir below), so a user browsing elsewhere with
// the file picker never affects where the next help screen loads from.
#define HOMEDIR_MAXLEN 64

// The LOCI directory File/Charset Load/Save actions resolve paths
// against (src/homedir.c's filedir_join()/filedir_join_suffix()) --
// distinct from app.homedir above. Always a full, drive-prefixed
// absolute path ("N:/sub/dir", matching locifilemanager-v2's own
// convention) once the file picker has navigated anywhere; defaults to
// app.homedir (or "/" if that's empty) the first time it's used this
// session. Updated by src/filepicker.c whenever the user navigates or
// confirms a directory, persisting across actions like app.filename
// already does -- this is what lets Load/Save remember the last
// browsed location instead of always resetting to the boot directory.
#define FILEDIR_MAXLEN 64

typedef enum {
    MODE_MAIN = 0,
    MODE_WRITE,
    MODE_SELECT,
    MODE_MOVE,
    MODE_LINEBOX
} EditorMode;

typedef struct {
    uint16_t   canvas_width;
    uint16_t   canvas_height;
    uint16_t   cursor_x;        // absolute canvas column
    uint16_t   cursor_y;        // absolute canvas row
    uint16_t   xoffset;         // viewport scroll offset (columns)
    uint16_t   yoffset;         // viewport scroll offset (rows)
    uint8_t    plotscreencode;  // character placed by SPACE
    uint8_t    plotaltchar;     // 0 = standard charset, 1 = alternate
    uint8_t    plotink;         // 0-7, ink colour to plot
    uint8_t    plotpaper;       // 0-7, paper colour to plot
    uint8_t    plotblink;       // 0/1, blink attribute to plot
    uint8_t    plotdouble;      // 0/1, double-height attribute to plot
    uint8_t    visualmap;       // 0/1, palette Alt-charset visual-charmap toggle
    uint8_t    favourites[FAVOURITES_COUNT]; // character-editor favourite screencodes
    EditorMode mode;
    uint8_t    showstatusbar;
    uint8_t    stdchanged;      // 0/1, CHARSET_STD edited this session (charsetedit.c)
    uint8_t    altchanged;      // 0/1, CHARSET_ALT edited this session (charsetedit.c)
    char       filename[FILENAME_MAXLEN + 1]; // LOCI base filename (src/fileio.c)
    char       homedir[HOMEDIR_MAXLEN + 1]; // LOCI boot directory (src/homedir.c)
    char       filedir[FILEDIR_MAXLEN + 1]; // LOCI Load/Save directory (src/filepicker.c)
} AppState;

extern AppState app;

#endif // APPSTATE_H
