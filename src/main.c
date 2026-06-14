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

AppState app;

int main(void)
{
    OricCharWin splash;

    charwin_init();

    cwin_init(&splash, 2, 0, 38, 28, A_FWWHITE, A_BGBLACK);
    cwin_clear(&splash);
    cwin_putat_string(&splash, 0, 0, "OricScreenEditor for LOCI");
    cwin_putat_printf(&splash, 0, 1, "Oscar64 build v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    cwin_putat_string(&splash, 0, 3, "Press any key to start");
    cwin_getch();

    canvas_init();
    statusbar_init();
    menu_init();
    menudata_init();
    editor_run();

    return 0;
}
