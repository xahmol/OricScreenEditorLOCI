// main.c - OricScreenEditorLOCI entry point
//
// Minimal smoke test for the Oscar64 build-chain scaffold: initialises
// charwin, clears the screen, prints a startup banner and version, and
// reports LOCI presence. No editor functionality yet.

#include "oric.h"
#include "charwin.h"
#include "loci.h"
#include "strings.h"

int main(void)
{
    OricCharWin scr;

    charwin_init();
    cwin_init(&scr, 2, 0, 38, 28, A_FWWHITE, A_BGBLACK);
    cwin_clear(&scr);

    cwin_putat_string(&scr, 0, 0, "OricScreenEditor for LOCI");
    cwin_putat_printf(&scr, 0, 1, "Oscar64 scaffold v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    if (loci_present())
    {
        cwin_putat_string(&scr, 0, 3, "LOCI device detected.");
    }
    else
    {
        cwin_putat_string(&scr, 0, 3, MSG_LOCI_NOT_FOUND);
    }

    cwin_putat_string(&scr, 0, 5, MSG_PRESS_KEY_EXIT);
    cwin_getch();

    return 0;
}
