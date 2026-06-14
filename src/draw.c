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

// Drawing functions

// Placed in:
// Overlay 1
#pragma code-name ("OVERLAY1");
#pragma rodata-name ("OVERLAY1");

// Functions

void plot_try()
{
    unsigned char key;

    strcpy(programmode, "Try");
    if (cfg.showbar)
    {
        printstatusbar();
    }
    cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode);
    key = getkey(ijk_present, 1);
    if (key == CH_SPACE)
    {
        screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, cfg.plotscreencode);
    }
    strcpy(programmode, "Main");
}

void writemode()
{
    // Write mode with screencodes

    unsigned char key, screencode;
    unsigned char rvs = 0;

    strcpy(programmode, "Write");

    do
    {
        if (cfg.showbar)
        {
            printstatusbar();
        }
        key = getkey(ijk_present, 0);

        switch (key)
        {
        // Cursor move
        case CH_CURS_LEFT:
        case CH_CURS_RIGHT:
        case CH_CURS_UP:
        case CH_CURS_DOWN:
            plotmove(key);
            break;

        // Toggle blink
        case CH_CTRL_B:
            cfg.plotblink = (cfg.plotblink == 0) ? 1 : 0;
            break;

        // Toggle alternate character set
        case CH_CTRL_A:
            cfg.plotaltchar = (cfg.plotaltchar == 0) ? 1 : 0;
            break;

        // Toggle double
        case CH_CTRL_D:
            cfg.plotdouble = (cfg.plotdouble == 0) ? 1 : 0;
            break;

        // Decrease ink
        case CH_CTRL_Z:
            if (cfg.plotink == 0)
            {
                cfg.plotink = 7;
            }
            else
            {
                cfg.plotink--;
            }
            break;

        // Increase ink
        case CH_CTRL_X:
            if (cfg.plotink == 7)
            {
                cfg.plotink = 0;
            }
            else
            {
                cfg.plotink++;
            }
            break;

        // Decrease paper
        case CH_CTRL_C:
            if (cfg.plotpaper == 0)
            {
                cfg.plotpaper = 7;
            }
            else
            {
                cfg.plotpaper--;
            }
            break;

        // Increase paper
        case CH_CTRL_V:
            if (cfg.plotpaper == 7)
            {
                cfg.plotpaper = 0;
            }
            else
            {
                cfg.plotpaper++;
            }
            break;

        // Plot present ink
        case CH_F1:
            screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, cfg.plotink);
            plotmove(CH_CURS_RIGHT);
            break;

        // Plot present paper
        case CH_F2:
            screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, 16 + cfg.plotpaper);
            plotmove(CH_CURS_RIGHT);
            break;

        // Plot present character modifier
        case CH_F3:
            screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, ORIC_CharAttribute(cfg.plotaltchar, cfg.plotdouble, cfg.plotblink));
            plotmove(CH_CURS_RIGHT);
            break;

        // Delete present screencode and attributes
        case CH_DEL:
            screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, CH_SPACE);
            cputcxy(cfg.screen_row, cfg.screen_col, CH_SPACE);
            break;

        // Toggle statusbar
        case CH_F6:
            togglestatusbar();
            break;

        case CH_F8:
            helpscreen_load(4);
            break;

        // Toggle RVS with the RVS ON and RVS OFF keys (control 9 and control 0)
        case CH_CTRL_R:
            rvs = (rvs == 1) ? 0 : 1;
            break;

        // Write printable character
        default:
            if (isprint(key))
            {
                screencode = key + (rvs * 128);
                screenmapplot(cfg.screen_row + cfg.yoffset, cfg.screen_col + cfg.xoffset, screencode);
                plotmove(CH_CURS_RIGHT);
            }
            break;
        }
    } while (key != CH_ESC && key != CH_STOP);
    strcpy(programmode, "Main");
}

void plotvisible(unsigned char row, unsigned char col, unsigned char setorrestore)
{
    // Plot or erase part of line or box if in visible viewport
    // Input: row, column, and flag setorrestore to plot new value (1) or restore old value (0)

    unsigned char val;

    if (row >= cfg.yoffset && row <= cfg.yoffset + 26 && col >= cfg.xoffset && col <= cfg.xoffset + 39)
    {
        if (setorrestore == 1)
        {
            cputcxy(col - cfg.xoffset, row - cfg.yoffset, cfg.plotscreencode + 128);
        }
        else
        {
            enable_overlay_ram();
            val = PEEK(screenmap_screenaddr(row, col, cfg.screenwidth));
            disable_overlay_ram();
            cputcxy(col - cfg.xoffset, row - cfg.yoffset, val);
        }
    }
}

void lineandbox(unsigned char draworselect)
{
    // Select line or box from upper left corner using cursor keys, ESC for cancel and ENTER for accept
    // Input: draworselect: Choose select mode (0) or draw mode (1)

    unsigned char key;
    unsigned char x, y, select_orgx, select_orgy;

    select_startx = cfg.screen_col + cfg.xoffset;
    select_starty = cfg.screen_row + cfg.yoffset;
    select_orgx = select_startx;
    select_orgy = select_starty;
    select_endx = select_startx;
    select_endy = select_starty;
    select_accept = 0;

    if (draworselect)
    {
        strcpy(programmode, "Line/Box");
    }
    cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);

    do
    {
        if (cfg.showbar)
        {
            printstatusbar();
        }
        key = getkey(ijk_present, 0);

        switch (key)
        {
        case CH_CURS_RIGHT:
            cursormove(0, 1, 0, 0);
            if (select_orgx > select_startx)
            {
                for (y = select_starty; y < select_endy + 1; y++)
                {
                    plotvisible(y, select_startx, 0);
                }
                select_startx = cfg.screen_col + cfg.xoffset;
            }
            else
            {
                select_endx = cfg.screen_col + cfg.xoffset;
                for (y = select_starty; y < select_endy + 1; y++)
                {
                    plotvisible(y, select_endx, 1);
                }
            }
            break;

        case CH_CURS_LEFT:
            cursormove(1, 0, 0, 0);
            if (select_endx > select_orgx)
            {
                for (y = select_starty; y < select_endy + 1; y++)
                {
                    plotvisible(y, select_endx, 0);
                }
                select_endx = cfg.screen_col + cfg.xoffset;
            }
            else
            {
                select_startx = cfg.screen_col + cfg.xoffset;
                for (y = select_starty; y < select_endy + 1; y++)
                {
                    plotvisible(y, select_startx, 1);
                }
            }
            break;

        case CH_CURS_UP:
            cursormove(0, 0, 1, 0);
            if (select_endy > select_orgy)
            {
                for (x = select_startx; x < select_endx + 1; x++)
                {
                    plotvisible(select_endy, x, 0);
                }
                select_endy = cfg.screen_row + cfg.yoffset;
            }
            else
            {
                select_starty = cfg.screen_row + cfg.yoffset;
                for (x = select_startx; x < select_endx + 1; x++)
                {
                    plotvisible(select_starty, x, 1);
                }
            }
            break;

        case CH_CURS_DOWN:
            cursormove(0, 0, 0, 1);
            if (select_orgy > select_starty)
            {
                for (x = select_startx; x < select_endx + 1; x++)
                {
                    plotvisible(select_starty, x, 0);
                }
                select_starty = cfg.screen_row + cfg.yoffset;
            }
            else
            {
                select_endy = cfg.screen_row + cfg.yoffset;
                for (x = select_startx; x < select_endx + 1; x++)
                {
                    plotvisible(select_endy, x, 1);
                }
            }
            break;

        // Toggle statusbar
        case CH_F6:
            togglestatusbar();
            break;

        case CH_F8:
            if (select_startx == select_endx && select_starty == select_endy)
                helpscreen_load(3);
            break;

        default:
            break;
        }
    } while (key != CH_ESC && key != CH_STOP && key != CH_ENTER);

    if (key == CH_ENTER)
    {
        select_width = select_endx - select_startx + 1;
        select_height = select_endy - select_starty + 1;
    }

    if (key == CH_ENTER && draworselect == 1)
    {
        enable_overlay_ram();
        for (y = select_starty; y < select_endy + 1; y++)
        {
            memset((void *)screenmap_screenaddr(y, select_startx, cfg.screenwidth), cfg.plotscreencode, select_width);
        }
        disable_overlay_ram();
        ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
        if (cfg.showbar)
        {
            initstatusbar();
        }
        cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);
    }
    else
    {
        ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
        if (cfg.showbar)
        {
            initstatusbar();
        }
        if (key == CH_ENTER)
        {
            select_accept = 1;
        }
        cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);
    }
    if (draworselect || key != CH_ESC || key != CH_STOP)
    {
        strcpy(programmode, "Main");
    }
}

void movemode()
{
    // Function to move the 80x25 viewport

    unsigned char key, y;
    unsigned char moved = 0;
    unsigned char val;

    strcpy(programmode, "Move");

    enable_overlay_ram();
    val = PEEK(screenmap_screenaddr(cfg.yoffset + cfg.screen_row, cfg.xoffset + cfg.screen_col, cfg.screenwidth));
    disable_overlay_ram();

    cputcxy(cfg.screen_col, cfg.screen_row, val);

    if (cfg.showbar)
    {
        hidestatusbar();
    }

    do
    {
        key = getkey(ijk_present, 0);

        switch (key)
        {
        case CH_CURS_RIGHT:
            ORIC_ScrollMove(0, 0, 40, 27, 2, 1);
            ORIC_VChar(0, 0, CH_SPACE, 27);
            moved = 1;
            break;

        case CH_CURS_LEFT:
            ORIC_ScrollMove(0, 0, 40, 27, 1, 1);
            ORIC_VChar(0, 39, CH_SPACE, 27);
            moved = 1;
            break;

        case CH_CURS_UP:
            ORIC_ScrollMove(0, 0, 40, 27, 8, 1);
            ORIC_HChar(24, 0, CH_SPACE, 40);
            moved = 1;
            break;

        case CH_CURS_DOWN:
            ORIC_ScrollMove(0, 0, 40, 27, 4, 1);
            ORIC_HChar(0, 0, CH_SPACE, 40);
            moved = 1;
            break;

        case CH_F8:
            helpscreen_load(3);
            break;

        default:
            break;
        }
    } while (key != CH_ENTER && key != CH_ESC && key != CH_STOP);

    if (moved == 1)
    {
        if (key == CH_ENTER)
        {
            for (y = 0; y < 25; y++)
            {
                enable_overlay_ram();
                memcpy((void *)screenmap_screenaddr(y + cfg.yoffset, cfg.xoffset, cfg.screenwidth), (void *)(SCREENMEMORY + (y * 40)), 40);
                disable_overlay_ram();
            }
        }
        ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
        if (cfg.showbar)
        {
            initstatusbar();
        }
    }

    cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);
    strcpy(programmode, "Main");
    if (cfg.showbar)
    {
        printstatusbar();
    }
}

void selectmode()
{
    // Function to select a screen area to delete, cut, copy or paint

    unsigned char key, movekey, y, ycount;

    strcpy(programmode, "Select");

    movekey = 0;
    lineandbox(0);
    if (select_accept == 0)
    {
        return;
    }

    strcpy(programmode, "x/c/d/ipm?");

    do
    {
        if (cfg.showbar)
        {
            printstatusbar();
        }
        key = getkey(ijk_present, 0);

        // Toggle statusbar
        if (key == CH_F6)
        {
            togglestatusbar();
        }

        if (key == CH_F8)
        {
            helpscreen_load(3);
        }

    } while (key != 'd' && key != 'x' && key != 'c' && key != 'p' && key != 'i' && key != 'm' && key != CH_ESC && key != CH_STOP);

    if (key != CH_ESC && key != CH_STOP)
    {
        if ((key == 'x' || key == 'c') && (select_width > 1024))
        {
            messagepopup("Selection too big.", 1);
            return;
        }

        if (key == 'x' || key == 'c')
        {
            if (key == 'x')
            {
                strcpy(programmode, "Cut");
            }
            else
            {
                strcpy(programmode, "Copy");
            }
            do
            {
                if (cfg.showbar)
                {
                    printstatusbar();
                }
                movekey = getkey(ijk_present, 0);

                switch (movekey)
                {
                // Cursor move
                case CH_CURS_LEFT:
                case CH_CURS_RIGHT:
                case CH_CURS_UP:
                case CH_CURS_DOWN:
                    plotmove(movekey);
                    break;

                case CH_F8:
                    helpscreen_load(3);
                    break;

                default:
                    break;
                }
            } while (movekey != CH_ESC && movekey != CH_STOP && movekey != CH_ENTER);

            if (movekey == CH_ENTER)
            {
                if ((cfg.screen_col + cfg.xoffset + select_width > cfg.screenwidth) || (cfg.screen_row + cfg.yoffset + select_height > cfg.screenheight))
                {
                    messagepopup("Selection does not fit.", 1);
                    return;
                }

                for (ycount = 0; ycount < select_height; ycount++)
                {
                    enable_overlay_ram();
                    y = (cfg.screen_row + cfg.yoffset >= select_starty) ? select_height - ycount - 1 : ycount;
                    memcpy((void *)SCREENMEMORY, (void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), select_width);
                    if (key == 'x')
                    {
                        memset((void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), CH_SPACE, select_width);
                    }
                    memcpy((void *)screenmap_screenaddr(cfg.screen_row + cfg.yoffset + y, cfg.screen_col + cfg.xoffset, cfg.screenwidth), (void *)SCREENMEMORY, select_width);
                    disable_overlay_ram();
                }
            }
        }

        if (key == 'd')
        {
            for (y = 0; y < select_height; y++)
            {
                enable_overlay_ram();
                memset((void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), CH_SPACE, select_width);
                disable_overlay_ram();
            }
        }

        if (key == 'i')
        {
            for (y = 0; y < select_height; y++)
            {
                enable_overlay_ram();
                memset((void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), cfg.plotink, select_width);
                disable_overlay_ram();
            }
        }

        if (key == 'p')
        {
            for (y = 0; y < select_height; y++)
            {
                enable_overlay_ram();
                memset((void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), 16 + cfg.plotpaper, select_width);
                disable_overlay_ram();
            }
        }

        if (key == 'm')
        {
            for (y = 0; y < select_height; y++)
            {
                enable_overlay_ram();
                memset((void *)screenmap_screenaddr(select_starty + y, select_startx, cfg.screenwidth), ORIC_CharAttribute(cfg.plotaltchar, cfg.plotdouble, cfg.plotblink), select_width);
                disable_overlay_ram();
            }
        }

        ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
        if (cfg.showbar)
        {
            initstatusbar();
        }
        cputcxy(cfg.screen_col, cfg.screen_row, cfg.plotscreencode + 128);
    }
    strcpy(programmode, "Main");
}