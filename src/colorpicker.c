/*
Oric Screen Editor - LOCI version
Screen editor for the Oric Atmos with a LOCI device
Written in 2025 by Xander Mol
Based on VDC Screen Editor for the C128 and the standard Oric Screen Editor

https://github.com/xahmol/OricScreenEditorLOCI
https://www.idreamtin8bits.com/

For information and documentation on the LOCI mass storage device:
-   LOCI User Manual
    https://github.com/sodiumlb/loci-hardware/wiki/LOCI-User-Manual
-   Sellers of my assembled LOCI device:
    - Raxiss: https://www.raxiss.com/article/id/38-LOCI

Code and resources from others used:
-   LOCI ROM by Sodiumlightbaby, 2024
    https://github.com/sodiumlb/loci-rom
-   CC65 cross compiler:
    https://cc65.github.io/

-   6502.org: Practical Memory Move Routines: Starting point for memory move routines
    http://6502.org/source/general/memory_move.html

-   DraBrowse source code for DOS Command and text input routine
    DraBrowse (db*) is a simple file browser.
    Originally created 2009 by Sascha Bader.
    Used version adapted by Dirk Jagdmann (doj)
    https://github.com/doj/dracopy

-   lib-ijk-egoist from oricOpenLibrary (for joystick support via Raxiss IJK interface)
    By Raxiss, (c) 2021
    https://github.com/iss000/oricOpenLibrary/tree/main/lib-ijk-egoist

-   Bart van Leeuwen and forum.defence-force.org: For inspiration and advice while coding.

-   jab / Artline Designs (Jaakko Luoto) for inspiration for Palette mode and PETSCII visual mode

-   Original windowing system code on Commodore 128 by unknown author.

-   Tested using real hardware Oric Atmos plus Cumana Reborn, and Oricutron for Windows and Linux

The code can be used freely as long as you retain
a notice describing original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

// Includes
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <time.h>
#include <peekpoke.h>
#include <ctype.h>
#include <atmos.h>
#include <errno.h>
#include "defines.h"
#include "osdklib.h"
#include "defines.h"
#include "oric_core.h"
#include "loci.h"
#include "generic.h"
#include "menu.h"
#include "screen.h"
#include "dir.h"
#include "file.h"
#include "colorpicker.h"

// Color picker functions

// Placed in:
// Overlay 2
#pragma code-name("OVERLAY2");
#pragma rodata-name("OVERLAY2");

void colourpicker()
// Select colour set from palette showing also the inverse colour pair
{
    unsigned char paper, ink, key;

    windowsave(2, 23, 1);
    ORIC_FillArea(2, 2, CH_SPACE, 38, 23);
    ORIC_VChar(2, 0, A_BGWHITE, 23);
    ORIC_VChar(2, 1, A_FWBLACK, 23);

    cputsxy(2, 3, "Select paper and ink colour");

    for (paper = 0; paper < 8; paper++)
    {
        for (ink = 0; ink < 8; ink++)
        {
            gotoxy(ink * 5, 5 + paper * 2);
            cprintf("%c%c%c%c%c", A_BGWHITE, ink, paper + 16, '-', '-' + 128);
        }
    }

    cputsxy(2, 21, "Cursor keys: Navigate");
    cputsxy(2, 22, "RETURN:      Select");
    cputsxy(2, 23, "ESC:         Cancel");

    paper = 0;
    ink = 0;

    do
    {
        // Prinr cursor position
        gotoxy(ink * 5, 5 + paper * 2);
        cprintf("%c%c%c%c%c", A_BGCYAN, ink, paper + 16, '*', '*' + 128);

        // Draw present ink color feedback
        gotoxy(25, 21);
        cprintf("Ink   : %u %c%c", ink, ink + 16, A_BGWHITE);
        gotoxy(25, 22);
        cprintf("Paper : %u %c%c", paper, paper + 16, A_BGWHITE);
        gotoxy(25, 23);
        cprintf("Result:%c%cNor%c%c%c", ink, paper + 16, 'I' + 128, 'n' + 128, 'v' + 128);

        // Get key
        key = getkey(ijk_present, 0);

        // Reset cursor position
        gotoxy(ink * 5, 5 + paper * 2);
        cprintf("%c%c%c%c%c", A_BGWHITE, ink, paper + 16, '-', '-' + 128);

        switch (key)
        {
        case CH_CURS_UP:
            if (paper > 0)
            {
                paper--;
            }
            else
            {
                paper = 7;
            }
            break;

        case CH_CURS_DOWN:
            if (paper < 7)
            {
                paper++;
            }
            else
            {
                paper = 0;
            }
            break;

        case CH_CURS_LEFT:
            if (ink > 0)
            {
                ink--;
            }
            else
            {
                ink = 7;
            }
            break;

        case CH_CURS_RIGHT:
            if (ink < 7)
            {
                ink++;
            }
            else
            {
                ink = 0;
            }
            break;

        default:
            break;
        }
    } while (key != CH_ESC && key != CH_ENTER);

    if (key == CH_ENTER)
    {
        plotpaper = paper;
        plotink = ink;
    }

    windowrestore(1);
}