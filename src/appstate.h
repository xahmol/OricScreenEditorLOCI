#ifndef APPSTATE_H
#define APPSTATE_H

#include <stdint.h>

// Visible viewport: 40 columns x 27 rows (row 27 is the statusbar)
#define VIEWPORT_WIDTH   40
#define VIEWPORT_HEIGHT  27

// Canvas size for Phase 1 (fixed; resizable from Phase 2 onward)
#define CANVAS_WIDTH     VIEWPORT_WIDTH
#define CANVAS_HEIGHT    VIEWPORT_HEIGHT

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
    EditorMode mode;
    uint8_t    showstatusbar;
} AppState;

extern AppState app;

#endif // APPSTATE_H
