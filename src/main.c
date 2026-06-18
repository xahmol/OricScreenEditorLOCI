// main.c - OricScreenEditorLOCI entry point
//
// Phase 1: splash screen, canvas + statusbar init, main-mode editor loop.

#include "oric.h"
#include "charwin.h"
#include "appstate.h"
#include "canvas.h"
#include "statusbar.h"
#include "menu.h"
#include "menudata.h"
#include "editor.h"
#include "strings.h"
#include "ijk.h"
#include "input.h"

AppState app;

/**
 * Program entry point. Shows the splash screen and waits for a keypress,
 * then initialises the canvas, statusbar, and menu system before handing
 * control to the main-mode editor loop (editor_run(), never returns).
 *
 * @return 0 (never reached).
 */
int main(void)
{
    OricCharWin splash;

    charwin_init();
    ijk_detect();

    cwin_init(&splash, 2, 0, 38, 28, A_FWWHITE, A_BGBLACK);
    cwin_clear(&splash);
    cwin_putat_string(&splash, 0, 0, MSG_SPLASH_TITLE);
    cwin_putat_printf(&splash, 0, 1, MSG_SPLASH_BUILD_FMT, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    cwin_putat_string(&splash, 0, 3, MSG_SPLASH_PRESSKEY);
    key_read();

    canvas_init();
    statusbar_init();
    menu_init();
    menudata_init();
    editor_run();

    return 0;
}
