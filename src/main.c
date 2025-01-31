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
#include "colorpicker.h"
#include "draw.h"
#include "menufunctions.h"

// Global variables
unsigned char charsetchanged[2];
unsigned char appexit;
char filename[8];
char programmode[11];
unsigned char showbar;

unsigned char screen_col;
unsigned char screen_row;
unsigned int xoffset;
unsigned int yoffset;
unsigned int screenwidth;
unsigned int screenheight;
unsigned int screentotal;
unsigned char plotscreencode;
unsigned char plotink;
unsigned char plotpaper;
unsigned char plotblink;
unsigned char plotdouble;
unsigned char plotaltchar;
unsigned int select_startx, select_starty, select_endx, select_endy, select_width, select_height, select_accept;
unsigned char rowsel = 0;
unsigned char colsel = 0;
unsigned char palettechar;
unsigned char visualmap = 0;
unsigned char favourites[10] = {33, 33, 33, 33, 33, 33, 33, 33, 33, 33};

char buffer[81];
char version[23];
char pathbuffer[256];
char homedir[256];

// Visual charset data
unsigned char visualchar[80] =
    {
        0x37, 0x4B, 0x33, 0x43, 0x3F, 0x4F, 0x27, 0x2B, 0x3C, 0x4C, 0x40, 0x30, 0x48, 0x34, 0x24, 0x28,
        0x55, 0x5A, 0x51, 0x52, 0x5D, 0x5E, 0x54, 0x58, 0x2D, 0x2E, 0x22, 0x21, 0x2A, 0x25, 0x53, 0x5F,
        0x36, 0x49, 0x26, 0x29, 0x56, 0x59, 0x57, 0x5B, 0x3D, 0x4E, 0x32, 0x41, 0x50, 0x5C, 0x2C, 0x2F,
        0x45, 0x3A, 0x44, 0x38, 0x47, 0x3B, 0x46, 0x39, 0x3E, 0x4D, 0x31, 0x42, 0x23, 0x35, 0x4A, 0x20,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F};

void mainmenuloop()
{
    // Function for main menu selection loop

    unsigned char menuchoice;

    windowsave(0, 1, 1);

    do
    {
        menuchoice = menumain();

        switch (menuchoice)
        {
        case 11:
            loadoverlay(1);
            resizewidth();
            break;

        case 12:
            loadoverlay(1);
            resizeheight();
            break;

        case 13:
        case 14:
            ORIC_ScreenmapFill(SCREENMAPBASE, screenwidth, screenheight, plotink, plotpaper, (menuchoice == 13) ? CH_SPACE : plotscreencode);
            windowrestore(0);
            ORIC_CopyViewPort(SCREENMAPBASE, screenwidth, xoffset, yoffset, 0, 0, 40, 27);
            windowsave(0, 1, 0);
            menuplacebar();
            if (showbar)
            {
                initstatusbar();
            }
            break;

        case 21:
            savescreenmap(0);
            break;

        case 22:
            loadscreenmap(0);
            break;

        case 23:
            saveproject();
            break;

        case 24:
            loadproject();
            break;

        case 25:
            savescreenmap(1);
            break;

        case 26:
            loadscreenmap(1);
            break;

        case 31:
            loadcharset(0);
            break;

        case 32:
            loadcharset(1);
            break;

        case 33:
            savecharset(0);
            break;

        case 34:
            savecharset(1);
            break;

        case 35:
            loadcharset(2);
            break;

        case 36:
            savecharset(2);
            break;

        case 41:
            loadoverlay(1);
            versioninfo();
            break;

        case 42:
            appexit = 1;
            menuchoice = 99;
            break;

        default:
            break;
        }
    } while (menuchoice < 99);

    windowrestore(1);
}

// Main routine
void main()
{
    // Main application initialization, loop and exit

    unsigned char key, grab;
    int error;

    // Reset startvalues global variables
    charsetchanged[0] = 0;
    charsetchanged[1] = 0;
    appexit = 0;
    screen_col = 0;
    screen_row = 0;
    xoffset = 0;
    yoffset = 0;
    screenwidth = 40;
    screenheight = 27;
    screentotal = screenwidth * screenheight;
    plotscreencode = 33;
    plotink = A_FWWHITE;
    plotpaper = A_FWBLACK;
    plotblink = 0;
    plotdouble = 0;
    plotaltchar = 0;
    sort = 0;
    filter = 0;

    sprintf(pulldownmenutitles[0][0], "Width:   %5i ", screenwidth);
    sprintf(pulldownmenutitles[0][1], "Height:  %5i ", screenheight);

    // Set version number in string variable
    sprintf(version,
            "v%u.%u.%u - %c%c%c%c%c%c%c%c-%c%c%c%c",
            VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
            BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3, BUILD_MONTH_CH0, BUILD_MONTH_CH1, BUILD_DAY_CH0, BUILD_DAY_CH1, BUILD_HOUR_CH0, BUILD_HOUR_CH1, BUILD_MIN_CH0, BUILD_MIN_CH1);

    // Initialise screen and set standard charsets
    ORIC_Init();
    ORIC_RestoreStandardCharset();
    ORIC_RestoreAlternateCharset();

    // Initialise LOCI and joystick
    get_locicfg();
    ijk_detect();

    // Get home directory
    getcwd_loci(homedir, 255);
    if (strlen(homedir) > 230)
    {
        cprintf("Path to OSE too long. Press key.");
        getkey(ijk_present, 1);
        ORIC_Exit();
    }

    // Set homedir as default dir for browser
    strncpy(presentdir.path, homedir, 256);
    presentdir.drive = homedir[0] - '0';

    // Print loading message
    printcentered("Loading titlescreen.", 10, 26, 20);

    // Load and show title screen
    strncpy(pathbuffer, homedir, 255);
    strncat(pathbuffer, "OSEforLOCI-Title.bin", 255);
    error = file_load(pathbuffer, (void *)SCREENMEMORY, 1120);
    if (error < 1)
    {
        fileerrormessage(0);
    }

    // Load overlay 1 as default, as that is the one with most draw functions
    loadoverlay(1);

    // Clear screen map in bank 1 with spaces in text color white
    ORIC_ScreenmapFill(SCREENMAPBASE, screenwidth, screenheight, A_FWWHITE, A_FWBLACK, CH_SPACE);

    // Initialise swap charset with system charset
    enable_overlay_ram();
    memcpy((void *)CHARSET_SWAP, (void *)CHARSET_STD, 768);
    disable_overlay_ram();

    // Wait for key press to start application
    printcentered("Press key.", 10, 26, 20);

    getkey(ijk_present, 1);

    // Clear viewport of titlescreen
    clrscr();

    // Main program loop
    cputcxy(screen_col, screen_row, plotscreencode + 128);
    strcpy(programmode, "Main");
    showbar = 1;

    initstatusbar();

    do
    {
        if (showbar)
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

        // Increase screencode
        case '=':
            if (plotscreencode == 127)
            {
                plotscreencode = 32;
            }
            else
            {
                plotscreencode++;
            }
            cputcxy(screen_col, screen_row, plotscreencode + 128);
            break;

        // Decrease screencode
        case '-':
            if (plotscreencode == 32)
            {
                plotscreencode = 127;
            }
            else
            {
                plotscreencode--;
            }
            cputcxy(screen_col, screen_row, plotscreencode + 128);
            break;

        // Decrease ink
        case ',':
            if (plotink == 0)
            {
                plotink = 7;
            }
            else
            {
                plotink--;
            }
            break;

        // Increase ink
        case '.':
            if (plotink == 7)
            {
                plotink = 0;
            }
            else
            {
                plotink++;
            }
            break;

        // Decrease paper
        case ';':
            if (plotpaper == 0)
            {
                plotpaper = 7;
            }
            else
            {
                plotpaper--;
            }
            break;

        // Increase paper
        case 39:
            if (plotpaper == 7)
            {
                plotpaper = 0;
            }
            else
            {
                plotpaper++;
            }
            break;

        // Color picker
        case 'c':
            loadoverlay(2);
            colourpicker();
            break;

        // Toggle blink
        case 'b':
            plotblink = (plotblink == 0) ? 1 : 0;
            break;

        // Toggle alternate character set
        case 'a':
            plotaltchar = (plotaltchar == 0) ? 1 : 0;
            break;

        // Toggle double
        case 'd':
            plotdouble = (plotdouble == 0) ? 1 : 0;
            break;

        // Character edit mode
        case 'e':
            loadoverlay(2);
            chareditor();
            break;

        // Palette for character selection
        case 'p':
            loadoverlay(2);
            palette();
            break;

        // Grab underlying character and attributes
        case 'g':
            enable_overlay_ram();
            grab = PEEK(screenmap_screenaddr(screen_row + yoffset, screen_col + xoffset, screenwidth));
            disable_overlay_ram();
            if (grab > 31)
            {
                plotscreencode = grab;
                cputcxy(screen_col, screen_row, plotscreencode + 128);
            }
            else
            {
                if (grab < 8)
                {
                    plotink = grab;
                }
                if (grab > 15)
                {
                    plotpaper = grab;
                }
                if (grab > 7 && grab < 16)
                {
                    grab -= 8;
                    plotaltchar = grab & 1;
                    plotdouble = grab & 2;
                    plotblink = grab & 4;
                }
            }
            break;

        // Write mode: type in screencodes
        case 'w':
            loadoverlay(1);
            writemode();
            break;

        // Line and box mode
        case 'l':
            loadoverlay(1);
            lineandbox(1);
            break;

        // Move mode
        case 'm':
            loadoverlay(1);
            movemode();
            break;

        // Select mode
        case 's':
            loadoverlay(1);
            selectmode();
            break;

        // Try
        case 't':
            loadoverlay(1);
            plot_try();
            break;

        // Increase/decrease plot screencode by 128 (toggle 'RVS ON' and 'RVS OFF')
        case 'r':
            plotscreencode += 128; // Will increase 128 if <128 and decrease by 128 if >128 by overflow
            cputcxy(screen_col, screen_row, plotscreencode + 128);
            break;

        // Plot present screencode
        case CH_SPACE:
        case CH_ENTER:
            screenmapplot(screen_row + yoffset, screen_col + xoffset, plotscreencode);
            break;

        // Plot present ink
        case 'i':
            screenmapplot(screen_row + yoffset, screen_col + xoffset, plotink);
            plotmove(CH_CURS_DOWN);
            break;

        // Plot present paper
        case 'o':
            screenmapplot(screen_row + yoffset, screen_col + xoffset, 16 + plotpaper);
            plotmove(CH_CURS_DOWN);
            break;

        // Plot present character modifier
        case 'u':
            screenmapplot(screen_row + yoffset, screen_col + xoffset, ORIC_CharAttribute(plotaltchar, plotdouble, plotblink));
            plotmove(CH_CURS_DOWN);
            break;

        // Delete present screencode and attributes
        case CH_DEL:
            screenmapplot(screen_row + yoffset, screen_col + xoffset, CH_SPACE);
            break;

        // Store to favourites with SHIFT+0-9
        case 33:
            favourites[1] = plotscreencode;
            break;

        case 64:
            favourites[2] = plotscreencode;
            break;

        case 35:
            favourites[3] = plotscreencode;
            break;

        case 36:
            favourites[4] = plotscreencode;
            break;

        case 37:
            favourites[5] = plotscreencode;
            break;

        case 94:
            favourites[6] = plotscreencode;
            break;

        case 38:
            favourites[7] = plotscreencode;
            break;

        case 42:
            favourites[8] = plotscreencode;
            break;

        case 40:
            favourites[9] = plotscreencode;
            break;

        case 41:
            favourites[0] = plotscreencode;
            break;

        // Go to menu
        case CH_F1:
            mainmenuloop();
            cputcxy(screen_col, screen_row, plotscreencode);
            break;

        // Toggle statusbar
        case CH_F6:
            togglestatusbar();
            break;

        // Help screen
        case CH_F8:
            helpscreen_load(1);
            break;

        default:
            // 0-9: Favourites select
            if (key > 47 && key < 58)
            {
                plotscreencode = favourites[key - 48];
                cputcxy(screen_col, screen_row, plotscreencode + 128);
            }
            break;
        }
    } while (appexit == 0);

    textcolor(COLOR_YELLOW);
    ORIC_Exit();
}
