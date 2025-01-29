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
#include "dir.h"
#include "file.h"

// Window data
struct WindowStruct Window[9];
unsigned int windowaddress = WINDOWBASEADDRESS;
unsigned char windownumber = 0;

// Menu data
unsigned char menubaroptions = 4;
unsigned char pulldownmenunumber = 8;
char menubartitles[4][12] = {"Screen", "File", "Charset", "Information"};
unsigned char menubarcoords[4] = {2, 9, 14, 22};
unsigned char pulldownmenuoptions[5] = {4, 6, 6, 2, 2};
char pulldownmenutitles[5][6][16] = {
    {"Width:      40 ",
     "Height:     28 ",
     "Clear          ",
     "Fill           "},
    {"Save screen    ",
     "Load screen    ",
     "Save project   ",
     "Load project   ",
     "Save combined  ",
     "Load combined  "},
    {"Load standard  ",
     "Load alternate ",
     "Save standard  ",
     "Save alternate ",
     "Load combined  ",
     "Save combined  "},
    {"Version/credits",
     "Exit program   "},
    {"Yes",
     "No "}};

// Functions for windowing and menu system

void windowsave(unsigned char ypos, unsigned char height, unsigned char loadsyscharset)
{
    /* Function to save a window
       Input:
       - ypos: startline of window
       - height: height of window
       - loadsyscharset: load syscharset if userdefined charset is loaded enabled (1) or not (0) */

    unsigned int baseaddress = SCREENMEMORY + (ypos * 40);
    int length = height * 40;

    Window[windownumber].address = windowaddress;
    Window[windownumber].ypos = ypos;
    Window[windownumber].height = height;

    // Copy screen
    enable_overlay_ram();
    memcpy((int *)windowaddress, (int *)baseaddress, length);
    disable_overlay_ram();
    windowaddress += length;

    windownumber++;

    // Load system charset if needed
    if (loadsyscharset == 1 && charsetchanged[0] == 1)
    {
        charset_swap(0);
    }
}

void windowrestore(unsigned char restorealtcharset)
{
    /* Function to restore a window
       Input: restorealtcharset: request to restore user defined charset if needed enabled (1) or not (0) */

    unsigned int baseaddress = SCREENMEMORY + (Window[--windownumber].ypos * 40);
    int length = Window[windownumber].height * 40;

    windowaddress = Window[windownumber].address;

    // Restore screen
    enable_overlay_ram();
    memcpy((int *)baseaddress, (int *)windowaddress, length);
    disable_overlay_ram();

    // Restore custom charset if needed
    if (restorealtcharset == 1 && charsetchanged[0] == 1)
    {
        charset_swap(1);
    }
}

void windownew(unsigned char xpos, unsigned char ypos, unsigned char height, unsigned char width, unsigned char loadsyscharset)
{
    /* Function to make menu border
       Input:
       - xpos: x-coordinate of left upper corner
       - ypos: y-coordinate of right upper corner
       - height: number of rows in window
       - width: window width in characters
        - loadsyscharset: load syscharset if userdefined charset is loaded enabled (1) or not (0) */

    windowsave(ypos, height, loadsyscharset);

    ORIC_FillArea(ypos, xpos, CH_SPACE, width, height);
    ORIC_VChar(ypos, xpos - 1, A_STD, height);
    ORIC_VChar(ypos, xpos, A_BGWHITE, height);
    ORIC_VChar(ypos, xpos + 1, A_FWBLACK, height);
}

void menuplacebar()
{
    /* Function to print menu bar */

    unsigned char x;

    gotoxy(0, 0);
    cputc(A_FWBLACK);
    cputc(A_BGGREEN);
    ORIC_FillArea(0, 2, CH_SPACE, 38, 1);
    for (x = 0; x < menubaroptions; x++)
    {
        cputsxy(menubarcoords[x], 0, menubartitles[x]);
    }
}

unsigned char menupulldown(unsigned char xpos, unsigned char ypos, unsigned char menunumber, unsigned char escapable)
{
    /* Function for pull down menu
       Input:
       - xpos = x-coordinate of upper left corner
       - ypos = y-coordinate of upper left corner
       - menunumber =
         number of the menu as defined in pulldownmenuoptions array
       - espacable: ability to escape with escape key enabled (1) or not (0)  */

    unsigned char x;
    unsigned char key;
    unsigned char exit = 0;
    unsigned char menuchoice = 1;
    unsigned char endcolor = (menunumber > menubaroptions) ? A_BGWHITE : A_BGBLACK;

    windowsave(ypos, pulldownmenuoptions[menunumber - 1], 0);
    for (x = 0; x < pulldownmenuoptions[menunumber - 1]; x++)
    {
        gotoxy(xpos - 1, ypos + x);
        cputc(A_STD);
        cputc(A_BGCYAN);
        cputc(A_FWBLACK);
        cprintf(" %s", pulldownmenutitles[menunumber - 1][x]);
        cputc(endcolor);
    }

    do
    {
        gotoxy(xpos - 1, ypos + menuchoice - 1);
        cputc(A_STD);
        cputc(A_BGYELLOW);
        cputc(A_FWBLACK);
        cprintf("-%s", pulldownmenutitles[menunumber - 1][menuchoice - 1]);
        cputc(endcolor);

        do
        {
            key = getkey(ijk_present, 1);
        } while (key != CH_ENTER && key != CH_CURS_LEFT && key != CH_CURS_RIGHT && key != CH_CURS_UP && key != CH_CURS_DOWN && key != CH_ESC && key != CH_STOP);

        switch (key)
        {
        case CH_ESC:
        case CH_STOP:
            if (escapable == 1)
            {
                exit = 1;
                menuchoice = 0;
            }
            break;

        case CH_ENTER:
            exit = 1;
            break;

        case CH_CURS_LEFT:
        case CH_CURS_RIGHT:
            if (menunumber < 5)
            {
                exit = 1;
                menuchoice = (key == CH_CURS_LEFT) ? 18 : 19;
                gotoxy(menubarcoords[menunumber - 1] - 1, 0);
                cputc(A_BGGREEN);
                cprintf("%s", menubartitles[menunumber - 1]);
            }
            break;

        case CH_CURS_DOWN:
        case CH_CURS_UP:
            gotoxy(xpos - 1, ypos + menuchoice - 1);
            cputc(A_STD);
            cputc(A_BGCYAN);
            cputc(A_FWBLACK);
            cprintf(" %s", pulldownmenutitles[menunumber - 1][menuchoice - 1]);
            cputc(endcolor);
            if (key == CH_CURS_UP)
            {
                menuchoice--;
                if (menuchoice < 1)
                {
                    menuchoice = pulldownmenuoptions[menunumber - 1];
                }
            }
            else
            {
                menuchoice++;
                if (menuchoice > pulldownmenuoptions[menunumber - 1])
                {
                    menuchoice = 1;
                }
            }
            break;

        default:
            break;
        }
    } while (exit == 0);
    windowrestore(0);
    return menuchoice;
}

unsigned char menumain()
{
    /* Function for main menu selection */

    unsigned char menubarchoice = 1;
    unsigned char menuoptionchoice = 0;
    unsigned char key;
    unsigned char xpos;

    menuplacebar();

    do
    {
        do
        {
            gotoxy(menubarcoords[menubarchoice - 1] - 1, 0);
            cputc(A_BGWHITE);
            cprintf("%s", menubartitles[menubarchoice - 1]);
            cputc(A_BGGREEN);

            do
            {
                key = getkey(ijk_present, 1);
            } while (key != CH_ENTER && key != CH_CURS_LEFT && key != CH_CURS_RIGHT && key != CH_ESC && key != CH_STOP);

            if (key == CH_CURS_LEFT || key == CH_CURS_RIGHT)
            {
                gotoxy(menubarcoords[menubarchoice - 1] - 1, 0);
                cputc(A_BGGREEN);
                cprintf("%s", menubartitles[menubarchoice - 1]);
            }

            if (key == CH_CURS_LEFT)
            {
                menubarchoice--;
                if (menubarchoice < 1)
                {
                    menubarchoice = menubaroptions;
                }
            }
            else if (key == CH_CURS_RIGHT)
            {
                menubarchoice++;
                if (menubarchoice > menubaroptions)
                {
                    menubarchoice = 1;
                }
            }
        } while (key != CH_ENTER && key != CH_ESC && key != CH_STOP);
        if (key != CH_ESC && key != CH_STOP)
        {
            xpos = menubarcoords[menubarchoice - 1] - 1;
            if (xpos + strlen(pulldownmenutitles[menubarchoice - 1][0]) > 38)
            {
                xpos = menubarcoords[menubarchoice - 1] + strlen(menubartitles[menubarchoice - 1]) - strlen(pulldownmenutitles[menubarchoice - 1][0]);
            }
            menuoptionchoice = menupulldown(xpos, 1, menubarchoice, 1);
            if (menuoptionchoice == 18)
            {
                menuoptionchoice = 0;
                menubarchoice--;
                if (menubarchoice < 1)
                {
                    menubarchoice = menubaroptions;
                }
            }
            if (menuoptionchoice == 19)
            {
                menuoptionchoice = 0;
                menubarchoice++;
                if (menubarchoice > menubaroptions)
                {
                    menubarchoice = 1;
                }
            }
        }
        else
        {
            menuoptionchoice = 99;
        }
    } while (menuoptionchoice == 0);

    return menubarchoice * 10 + menuoptionchoice;
}

unsigned char areyousure(char *message, unsigned char syscharset)
{
    /* Pull down menu to verify if player is sure */
    unsigned char choice;

    windownew(5, 8, 6, 35, syscharset);
    cputsxy(7, 9, message);
    cputsxy(7, 10, "Are you sure?");
    choice = menupulldown(20, 11, 5, 0);
    windowrestore(syscharset);
    return choice;
}

void fileerrormessage(unsigned char syscharset)
{
    /* Show message for file error encountered */

    windownew(5, 8, 6, 35, syscharset);
    cputsxy(7, 9, "File error!");

    gotoxy(7, 11);
    cprintf("Error# : %d", errno);
    gotoxy(7, 12);
    cprintf("Message: %.30s", strerror(errno));

    cputsxy(7, 13, "Press key.");
    getkey(ijk_present, 1);
    windowrestore(syscharset);
}

void messagepopup(char *message, unsigned char syscharset)
{
    // Show popup with a message

    windownew(5, 8, 6, 35, syscharset);
    cputsxy(7, 9, message);
    cputsxy(7, 11, "Press key.");
    getkey(ijk_present, 1);
    windowrestore(syscharset);
}