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

// Status bar functions
void printstatusbar()
{
    unsigned char val;

    if (screen_row == 26)
    {
        return;
    }

    sprintf(buffer, "%-10s", programmode);
    cputsxy(2, 26, buffer);
    if (screenwidth > 99 || screenheight > 99)
    {
        sprintf(buffer, "%2x,%2x", screen_col + xoffset, screen_row + yoffset);
    }
    else
    {
        sprintf(buffer, "%2u,%2u", screen_col + xoffset, screen_row + yoffset);
    }
    cputsxy(14, 26, buffer);
    sprintf(buffer, "%2x", plotscreencode);
    cputsxy(21, 26, buffer);
    cputcxy(23, 26, plotscreencode);
    enable_overlay_ram();
    val = PEEK(screenmap_screenaddr(screen_row + yoffset, screen_col + xoffset, screenwidth));
    disable_overlay_ram();
    sprintf(buffer, "%2x", val);
    cputsxy(26, 26, buffer);
    sprintf(buffer, "%1u", plotink);
    cputsxy(30, 26, buffer);
    cputc(16 + plotink);
    cputc(A_BGWHITE);
    sprintf(buffer, "%1u", plotpaper);
    cputsxy(34, 26, buffer);
    cputc(16 + plotpaper);
    cputc(A_BGWHITE);
    if (plotaltchar)
    {
        cputsxy(37, 26, "A");
    }
    else
    {
        cputsxy(37, 26, "S");
    }
    if (plotdouble)
    {
        cputsxy(38, 26, "D");
    }
    else
    {
        cputsxy(38, 26, " ");
    }
    if (plotblink)
    {
        cputsxy(39, 26, "B");
    }
    else
    {
        cputsxy(39, 26, " ");
    }
}

void initstatusbar()
{
    if (screen_row == 26)
    {
        return;
    }

    gotoxy(0, 26);
    cputc(A_FWBLACK);
    cputc(A_BGWHITE);
    ORIC_FillArea(26, 2, CH_SPACE, 38, 1);
    cputsxy(12, 26, "XY");
    cputsxy(20, 26, "C");
    cputsxy(25, 26, "S");
    cputsxy(29, 26, "I");
    cputsxy(33, 26, "P");
    printstatusbar();
}

void hidestatusbar()
{
    ORIC_CopyViewPort(SCREENMAPBASE, screenwidth, xoffset, yoffset + 26, 0, 26, 40, 1);
}

void togglestatusbar()
{
    if (screen_row == 26)
    {
        return;
    }

    if (showbar)
    {
        showbar = 0;
        hidestatusbar();
    }
    else
    {
        showbar = 1;
        initstatusbar();
    }
}

// Generic screen map routines
void screenmapplot(unsigned char row, unsigned char col, unsigned char screencode)
{
    // Function to plot a screencode
    // Input: row and column, screencode to plot

    enable_overlay_ram();
    POKE(screenmap_screenaddr(row, col, screenwidth), screencode);
    disable_overlay_ram();
}

void cursormove(unsigned char left, unsigned char right, unsigned char up, unsigned char down)
{
    // Move cursor and scroll screen if needed
    // Input: flags to enable (1) or disable (0) move in the different directions

    if (left == 1)
    {
        if (screen_col == 0)
        {
            if (xoffset > 0)
            {
                ORIC_ScrollCopy(SCREENMAPBASE, screenwidth, xoffset--, yoffset, 0, 0, 40, 27, 2);
                initstatusbar();
            }
        }
        else
        {
            screen_col--;
        }
    }
    if (right == 1)
    {
        if (screen_col == 39)
        {
            if (xoffset + screen_col < screenwidth - 1)
            {
                ORIC_ScrollCopy(SCREENMAPBASE, screenwidth, xoffset++, yoffset, 0, 0, 40, 27, 1);
                initstatusbar();
            }
        }
        else
        {
            screen_col++;
        }
    }
    if (up == 1)
    {
        if (screen_row == 0)
        {
            if (yoffset > 0)
            {
                ORIC_ScrollCopy(SCREENMAPBASE, screenwidth, xoffset, yoffset--, 0, 0, 40, 27, 4);
                initstatusbar();
            }
        }
        else
        {
            screen_row--;
            if (showbar && screen_row == 25)
            {
                initstatusbar();
            }
        }
    }
    if (down == 1)
    {
        if (screen_row == 25)
        {
            hidestatusbar();
        }
        if (screen_row == 26)
        {
            if (yoffset + screen_row < screenheight - 1)
            {
                ORIC_ScrollCopy(SCREENMAPBASE, screenwidth, xoffset, yoffset++, 0, 0, 40, 27, 8);
                initstatusbar();
            }
        }
        else
        {
            screen_row++;
        }
    }
}

void plotmove(unsigned char direction)
{
    // Drive cursor move
    // Input: ASCII code of cursor key pressed

    unsigned char val;

    enable_overlay_ram();
    val = PEEK(screenmap_screenaddr(yoffset + screen_row, xoffset + screen_col, screenwidth));
    disable_overlay_ram();

    cputcxy(screen_col, screen_row, val);

    switch (direction)
    {
    case CH_CURS_LEFT:
        cursormove(1, 0, 0, 0);
        break;

    case CH_CURS_RIGHT:
        cursormove(0, 1, 0, 0);
        break;

    case CH_CURS_UP:
        cursormove(0, 0, 1, 0);
        break;

    case CH_CURS_DOWN:
        cursormove(0, 0, 0, 1);
        break;

    default:
        break;
    }

    cputcxy(screen_col, screen_row, plotscreencode + 128);
}

// Help screens
void helpscreen_load(unsigned char screennumber)
{
    // Function to show selected help screen
    // Input: screennumber: 1-Main mode, 2-Character editor, 3-SelectMoveLinebox, 4-Write/colorwrite mode

    int error;

    // Load system charset if needed
    if (charsetchanged[0] == 1)
    {
        charset_swap(0);
    }

    // Load selected help screen
    sprintf(buffer, "OSEforLOCI-Help%u.bin", screennumber);
    strncpy(pathbuffer, homedir, 255);
    strncat(pathbuffer, buffer, 255);
    error = file_load(pathbuffer, (void *)SCREENMEMORY, 1120);
    if (error < 1)
    {
        fileerrormessage(1);
    }

    getkey(ijk_present, 1);

    // Restore screen
    ORIC_CopyViewPort(SCREENMAPBASE, screenwidth, xoffset, yoffset, 0, 0, 40, 27);
    if (showbar)
    {
        initstatusbar();
    }
    if (screennumber != 2)
    {
        cputcxy(screen_col, screen_row, plotscreencode);
    }

    // Restore custom charset if needed
    if (charsetchanged[0] == 1)
    {
        charset_swap(1);
    }
}