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
#include "charedit.h"
#include "palette.h"
#include "draw.h"
#include "menufunctions.h"

// Main menu functions apart from file ops

// Placed in:
// Overlay 1
#pragma code-name ("OVERLAY1");
#pragma rodata-name ("OVERLAY1");

// Functions

void resizewidth()
{
    // Function to resize screen canvas width

    unsigned int newwidth = screenwidth;
    unsigned int maxsize = MEMORYLIMIT - SCREENMAPBASE;
    unsigned char areyousure = 0;
    unsigned char sizechanged = 0;
    unsigned int y;
    char *ptrend;

    windownew(2, 5, 12, 38, 0);

    cputsxy(4, 6, "Resize canvas width");
    cputsxy(4, 8, "Enter new width:");

    sprintf(buffer, "%i", screenwidth);
    textInput(4, 9, 4, buffer, 4, 1);
    newwidth = (unsigned int)strtol(buffer, &ptrend, 10);

    if ((newwidth * screenheight) > maxsize || newwidth < 40)
    {
        cputsxy(4, 11, "New size unsupported. Press key.");
        getkey(ijk_present, 0);
    }
    else
    {
        if (newwidth < screenwidth)
        {
            cputsxy(4, 11, "Shrinking might delete data.");
            cputsxy(4, 12, "Are you sure?");
            areyousure = menupulldown(20, 13, 5, 0);
            if (areyousure == 1)
            {
                for (y = 0; y < screenheight; y++)
                {
                    enable_overlay_ram();
                    memcpy((void *)SCREENMEMORY, (void *)screenmap_screenaddr(y, 0, screenwidth), newwidth);
                    memcpy((void *)screenmap_screenaddr(y, 0, newwidth), (void *)SCREENMEMORY, newwidth);
                    disable_overlay_ram();
                }
                if (screen_col > newwidth - 1)
                {
                    screen_col = newwidth - 1;
                }
                sizechanged = 1;
            }
        }
        if (newwidth > screenwidth)
        {
            for (y = 0; y < screenheight; y++)
            {
                enable_overlay_ram();
                memcpy((void *)SCREENMEMORY, (void *)screenmap_screenaddr(screenheight - y - 1, 0, screenwidth), screenwidth);
                memcpy((void *)screenmap_screenaddr(screenheight - y - 1, 0, newwidth), (void *)SCREENMEMORY, screenwidth);
                memset((void *)screenmap_screenaddr(screenheight - y - 1, screenwidth, newwidth), CH_SPACE, newwidth - screenwidth);
                disable_overlay_ram();
            }
            sizechanged = 1;
        }
    }

    windowrestore(0);

    if (sizechanged == 1)
    {
        screenwidth = newwidth;
        screentotal = screenwidth * screenheight;
        xoffset = 0;
        ORIC_CopyViewPort(SCREENMAPBASE, screenwidth, xoffset, yoffset, 0, 0, 40, 27);
        sprintf(pulldownmenutitles[0][0], "Width:   %5i ", screenwidth);
        menuplacebar();
        if (showbar)
        {
            initstatusbar();
        }
        cputcxy(screen_col, screen_row, plotscreencode + 128);
    }
}

void resizeheight()
{
    // Function to resize screen camvas height

    unsigned int newheight = screenheight;
    unsigned int maxsize = MEMORYLIMIT - SCREENMAPBASE;
    unsigned char areyousure = 0;
    unsigned char sizechanged = 0;
    unsigned char y;
    char *ptrend;

    windownew(2, 5, 12, 38, 0);

    cputsxy(4, 6, "Resize canvas height");
    cputsxy(4, 8, "Enter new height:");

    sprintf(buffer, "%i", screenheight);
    textInput(4, 9, 4, buffer, 4, 1);
    newheight = (unsigned int)strtol(buffer, &ptrend, 10);

    if ((newheight * screenwidth) > maxsize || newheight < 27)
    {
        cputsxy(4, 11, "New size unsupported. press key.");
        getkey(ijk_present, 0);
    }
    else
    {
        if (newheight < screenheight)
        {
            cputsxy(4, 11, "Shrinking might delete data.");
            cputsxy(4, 12, "Are you sure?");
            areyousure = menupulldown(20, 13, 5, 0);
            if (areyousure == 1)
            {
                enable_overlay_ram();
                memcpy((void *)screenmap_screenaddr(0, 0, screenwidth), (void *)screenmap_screenaddr(0, 0, screenwidth), screenheight * screenwidth);
                disable_overlay_ram();
                if (screen_row > newheight - 1)
                {
                    screen_row = newheight - 1;
                }
                sizechanged = 1;
            }
        }
        if (newheight > screenheight)
        {
            enable_overlay_ram();
            for (y = 0; y < screenheight; y++)
            {
                memcpy((void *)screenmap_screenaddr(screenheight - y - 1, 0, screenwidth), (void *)screenmap_screenaddr(screenheight - y - 1, 0, screenwidth), screenwidth);
            }
            memset((void *)screenmap_screenaddr(screenheight, 0, screenwidth), CH_SPACE, (newheight - screenheight) * screenwidth);
            disable_overlay_ram();
            sizechanged = 1;
        }
    }

    windowrestore(0);

    if (sizechanged == 1)
    {
        screenheight = newheight;
        screentotal = screenwidth * screenheight;
        yoffset = 0;
        ORIC_CopyViewPort(SCREENMAPBASE, screenwidth, xoffset, yoffset, 0, 0, 40, 27);
        sprintf(pulldownmenutitles[0][1], "Height:  %5i ", screenheight);
        menuplacebar();
        if (showbar)
        {
            initstatusbar();
        }
        cputcxy(screen_col, screen_row, plotscreencode + 128);
    }
}

void versioninfo()
{
    windownew(2, 5, 15, 38, 1);
    cputsxy(4, 6, "Version information and credits");
    cputsxy(4, 8, "ORIC Screen Editor");
    cputsxy(4, 9, "Written in 2022 by Xander Mol");
    sprintf(buffer, "version: %s", version);
    cputsxy(4, 11, buffer);
    cputsxy(4, 13, "Source, docs and credits at:");
    cputsxy(4, 14, "github.com/xahmol/OricScreenEditor");
    cputsxy(4, 16, "(C) 2022, IDreamtIn8bits.com");
    cputsxy(4, 18, "Press a key to continue.");
    getkey(ijk_present, 1);
    windowrestore(0);
}