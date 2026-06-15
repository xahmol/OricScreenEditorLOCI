#ifndef APPSTATE_H
#define APPSTATE_H

#include <stdint.h>

// Visible viewport: 40 columns x 27 rows (row 27 is the statusbar)
#define VIEWPORT_WIDTH   40
#define VIEWPORT_HEIGHT  27

// Canvas size: starts at the viewport size, resizable up to CANVAS_MAX_SIZE
// cells (Screen > Width/Height, canvas_resize()). 8192 is a documented,
// revisable budget -- about 2.5x V1's 6655-byte CC65 ceiling, leaving ~34KB
// of the ~42.4KB main-RAM region for the menu engine and later phases'
// static buffers.
#define CANVAS_WIDTH     VIEWPORT_WIDTH
#define CANVAS_HEIGHT    VIEWPORT_HEIGHT
#define CANVAS_MAX_SIZE  8192

// Character editor favourites (0-9 select, SHIFT+0-9 store).
#define FAVOURITES_COUNT 10

typedef enum {
    MODE_MAIN = 0
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
} AppState;

extern AppState app;

#endif // APPSTATE_H
