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
#include "palette.h"

// Character palette functions

// Placed in:
// Overlay 2
#pragma code-name ("OVERLAY2");
#pragma rodata-name ("OVERLAY2");

void palette_draw()
{
    // Draw window for character palette
    unsigned char x, y;
    unsigned char counter = 0;

    windowsave(0, 25, 0);
    ORIC_FillArea(0, 3, CH_SPACE, 37, 25);
    ORIC_VChar(0, 0, A_STD, 25);
    cputcxy(7, 2, A_ALT);
    ORIC_VChar(15, 7, A_ALT, 10);
    ORIC_VChar(0, 1, A_BGWHITE, 25);
    ORIC_VChar(0, 2, A_FWBLACK, 25);
    cputsxy(3, 1, "Fav:");
    cputsxy(3, 3, "Std:");
    cputsxy(3, 15, "Alt:");

    // Set coordinate of present char if no visual map
    cfg.rowsel = cfg.palettechar / 16 + 1;
    cfg.colsel = cfg.palettechar % 16;

    // cfg.favourites palette
    for (x = 0; x < 10; x++)
    {
        cputcxy(8 + x * 2, 1, cfg.favourites[x]);
        cputcxy(8 + x * 2, 2, cfg.favourites[x]);
    }

    // Full charsets
    for (y = 0; y < 6; y++)
    {
        for (x = 0; x < 16; x++)
        {
            cputcxy(8 + x * 2, 3 + y * 2, counter + 32);
            if (y < 5)
            {
                cputcxy(8 + x * 2, 15 + y * 2, (cfg.visualmap) ? visualchar[counter] : counter + 32);
            }
            counter++;
        }
    }
}

unsigned char palette_returnscreencode()
{
    // Get screencode from palette position
    unsigned char palpos;

    if (cfg.rowsel == 0)
    {
        palpos = cfg.favourites[cfg.colsel];
    }
    if (cfg.rowsel > 0 && cfg.rowsel < 7)
    {
        palpos = 32 + cfg.colsel + (cfg.rowsel - 1) * 16;
    }
    if (cfg.rowsel > 6)
    {
        if (cfg.visualmap)
        {
            palpos = visualchar[cfg.colsel + (cfg.rowsel - 7) * 16];
        }
        else
        {
            palpos = 32 + cfg.colsel + (cfg.rowsel - 7) * 16;
        }
    }

    return palpos;
}

void palette()
{
    // Show character set palette

    unsigned char key;

    cfg.palettechar = cfg.plotscreencode - 32;

    strcpy(programmode, "Palette");

    palette_draw();

    // Get key loop
    do
    {
        cputcxy(8 + cfg.colsel * 2, 1 + cfg.rowsel * 2, palette_returnscreencode() + 128);
        if (cfg.showbar)
        {
            printstatusbar();
        }
        key = getkey(ijk_present, 0);

        switch (key)
        {
        // Cursor movemeny
        case CH_CURS_RIGHT:
        case CH_CURS_LEFT:
        case CH_CURS_DOWN:
        case CH_CURS_UP:
            cputcxy(8 + cfg.colsel * 2, 1 + cfg.rowsel * 2, palette_returnscreencode());
            if (key == CH_CURS_RIGHT)
            {
                cfg.colsel++;
            }
            if (key == CH_CURS_LEFT)
            {
                if (cfg.colsel > 0)
                {
                    cfg.colsel--;
                }
                else
                {
                    cfg.colsel = 15;
                    if (cfg.rowsel > 0)
                    {
                        cfg.rowsel--;
                        if (cfg.rowsel == 0)
                        {
                            cfg.colsel = 9;
                        }
                    }
                    else
                    {
                        cfg.rowsel = 11;
                    }
                }
            }
            if (key == CH_CURS_DOWN)
            {
                cfg.rowsel++;
            }
            if (key == CH_CURS_UP)
            {
                if (cfg.rowsel > 0)
                {
                    cfg.rowsel--;
                }
                else
                {
                    cfg.rowsel = 11;
                }
            }
            if (cfg.colsel > 9 && cfg.rowsel == 0)
            {
                cfg.colsel = 0;
                cfg.rowsel = 1;
            }
            if (cfg.colsel > 15)
            {
                cfg.colsel = 0;
                cfg.rowsel++;
            }
            if (cfg.rowsel > 11)
            {
                cfg.rowsel = 0;
            }
            break;

        // Select character
        case CH_SPACE:
        case CH_ENTER:
            cfg.plotscreencode = palette_returnscreencode();
            ;
            key = CH_ESC;
            break;

        // Toggle visual charset map
        case 'v':
            windowrestore(0);
            cfg.visualmap = (cfg.visualmap) ? 0 : 1;
            palette_draw();
            break;

        // Toggle statusbar
        case CH_F6:
            togglestatusbar();
            break;

        // Help screen
        case CH_F8:
            windowrestore(0);
            helpscreen_load(2);
            palette_draw();
            break;

        default:
            // Store in cfg.favourites
            if (key > 47 && key < 58 && cfg.rowsel > 0)
            {
                cfg.palettechar = palette_returnscreencode() - 32;
                cfg.favourites[key - 48] = cfg.palettechar + 32;
                cputcxy(8 + (key - 48) * 2, 1, cfg.favourites[key - 48]);
                cputcxy(8 + (key - 48) * 2, 2, cfg.favourites[key - 48]);
            }
            break;
        }
    } while (key != CH_ESC && key != CH_STOP);

    windowrestore(0);
    cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);
    strcpy(programmode, "Main");
}