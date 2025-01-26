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

// Character editor functions

// Placed in:
// Overlay 2
#pragma code-name ("OVERLAY2");
#pragma rodata-name ("OVERLAY2");

// Functions

void showchareditfield()
{
    // Function to draw char editor background field
    // Input: Flag for which charset is edited, standard (0) or alternate (1)

    windowsave(0, 12, 0);
    ORIC_FillArea(0, 30, CH_SPACE, 10, 12);
    ORIC_VChar(0, 27, A_STD, 12);
    ORIC_VChar(0, 28, A_BGWHITE, 12);
    ORIC_VChar(0, 29, A_FWBLACK, 12);
}

unsigned int charaddress(unsigned char screencode, unsigned char stdoralt, unsigned char swap)
{
    // Function to calculate address of character to edit
    // Input:   screencode to edit, flag for standard (0) or alternate (1) charset,
    //          flag for swap memory (1) or normal memort for standard charset

    unsigned int address;

    if (swap && !stdoralt)
    {
        address = CHARSET_SWAP;
    }
    else
    {
        address = (stdoralt == 0) ? CHARSET_STD : CHARSET_ALT;
    }
    address += (screencode - 32) * 8;
    return address;
}

void showchareditgrid(unsigned int screencode, unsigned char stdoralt)
{
    // Function to draw grid with present char to edit

    unsigned char x, y, char_byte;
    unsigned int address;

    address = charaddress(screencode, stdoralt, 0);

    gotoxy(30, 1);
    cprintf("Ch %2X %s", screencode, (stdoralt == 0) ? "Std" : "Alt");

    for (y = 0; y < 8; y++)
    {
        enable_overlay_ram();
        char_byte = PEEK(address + y);
        disable_overlay_ram();
        gotoxy(30, y + 3);
        cprintf("%2X", char_byte);
        for (x = 0; x < 6; x++)
        {
            if (char_byte & (1 << (5 - x)))
            {
                cputcxy(x + 33, y + 3, CH_SPACE);
            }
            else
            {
                cputcxy(x + 33, y + 3, CH_INVSPACE);
            }
        }
    }
}

void chareditor()
{
    unsigned char x, y, char_altorstd, char_screencode, key;
    unsigned char xpos = 0;
    unsigned char ypos = 0;
    unsigned char char_present[8];
    unsigned char char_copy[8];
    unsigned char char_undo[8];
    unsigned char char_buffer[8];
    unsigned int char_address;
    unsigned char charchanged = 0;
    unsigned char bitset;
    char *ptrend;

    char_altorstd = 0;
    char_screencode = plotscreencode;
    char_address = charaddress(char_screencode, char_altorstd, 0);
    charsetchanged[plotaltchar] = 1;
    strcpy(programmode, "Charedit");

    enable_overlay_ram();
    for (y = 0; y < 8; y++)
    {
        char_present[y] = PEEK(char_address + y);
        char_undo[y] = char_present[y];
    }
    disable_overlay_ram();

    showchareditfield();
    showchareditgrid(char_screencode, char_altorstd);
    do
    {
        bitset = (char_present[ypos] & (1 << (5 - xpos))) ? 0 : 1;
        cputcxy(xpos + 33, ypos + 3, '*' + bitset * 128);
        if (showbar)
        {
            printstatusbar();
        }
        key = getkey(ijk_present, 0);
        cputcxy(xpos + 33, ypos + 3, CH_SPACE + bitset * 128);

        switch (key)
        {
        // Movement
        case CH_CURS_RIGHT:
            if (xpos < 5)
            {
                xpos++;
            }
            break;

        case CH_CURS_LEFT:
            if (xpos > 0)
            {
                xpos--;
            }
            break;

        case CH_CURS_DOWN:
            if (ypos < 7)
            {
                ypos++;
            }
            break;

        case CH_CURS_UP:
            if (ypos > 0)
            {
                ypos--;
            }
            break;

        // Next or previous character
        case '+':
        case '-':
        case '=':
            if (key == '+' || key == '=')
            {
                char_screencode++;
                if (char_screencode > 127 || (char_altorstd && char_screencode > 111))
                {
                    char_screencode = 32;
                }
            }
            else
            {
                if (char_screencode > 32)
                {
                    char_screencode--;
                }
                else
                {
                    char_screencode = (char_altorstd) ? 111 : 127;
                }
            }
            charchanged = 1;
            break;

        // Toggle bit
        case CH_SPACE:
        case CH_ENTER:
            char_present[ypos] ^= 1 << (5 - xpos);
            enable_overlay_ram();
            POKE(char_address + ypos, char_present[ypos]);
            POKE(charaddress(char_screencode, char_altorstd, 1) + ypos, char_present[ypos]);
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Inverse
        case 'i':
            for (y = 0; y < 8; y++)
            {
                char_present[y] ^= 0x3F;
                enable_overlay_ram();
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
                disable_overlay_ram();
            }
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Delete
        case CH_DEL:
            for (y = 0; y < 8; y++)
            {
                char_present[y] = 0;
                enable_overlay_ram();
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
                disable_overlay_ram();
            }
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Undo
        case 'z':
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_undo[y];
                enable_overlay_ram();
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
                disable_overlay_ram();
            }
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Restore from system font
        case 's':
            if (!char_altorstd)
            {
                for (y = 0; y < 8; y++)
                {
                    char_present[y] = PEEK(CHARSETROM + y + (char_screencode - 32) * 8);
                    enable_overlay_ram();
                    POKE(char_address + y, char_present[y]);
                    POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
                    disable_overlay_ram();
                }
                showchareditgrid(char_screencode, char_altorstd);
            }
            break;

        // Copy
        case 'c':
            for (y = 0; y < 8; y++)
            {
                char_copy[y] = char_present[y];
            }
            break;

        // Paste
        case 'v':
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_copy[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Switch charset
        case 'a':
            char_altorstd = (char_altorstd == 0) ? 1 : 0;
            if (char_altorstd && char_screencode > 111)
            {
                char_screencode = 32;
            }
            charchanged = 1;
            break;

        // Mirror y axis
        case 'y':
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                POKE(char_address + y, char_present[7 - y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[7 - y]);
            }
            for (y = 0; y < 8; y++)
            {
                char_present[y] = PEEK(char_address + y);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Mirror x axis
        case 'x':
            for (y = 0; y < 8; y++)
            {
                for (x = 0; x < 6; x++)
                {
                    if (char_present[y] & (1 << (5 - x)))
                    {
                        char_buffer[y] |= (1 << x);
                    }
                    else
                    {
                        char_buffer[y] &= ~(1 << x);
                    }
                }
            }
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_buffer[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Scroll up
        case 'u':
            for (y = 1; y < 8; y++)
            {
                char_buffer[y - 1] = char_present[y];
            }
            char_buffer[7] = char_present[0];
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_buffer[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Scroll down
        case 'd':
            for (y = 1; y < 8; y++)
            {
                char_buffer[y] = char_present[y - 1];
            }
            char_buffer[0] = char_present[7];
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_buffer[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Scroll right
        case 'r':
            for (y = 0; y < 8; y++)
            {
                char_buffer[y] = char_present[y] >> 1;
                if (char_present[y] & 0x01)
                {
                    char_buffer[y] += 0x20;
                }
            }
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_buffer[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Scroll left
        case 'l':
            for (y = 0; y < 8; y++)
            {
                char_buffer[y] = char_present[y] << 1;
                if (char_present[y] & 0x20)
                {
                    char_buffer[y] += 0x01;
                }
                char_buffer[y] &= 0x3F;
            }
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = char_buffer[y];
                POKE(char_address + y, char_present[y]);
                POKE(charaddress(char_screencode, char_altorstd, 1) + y, char_present[y]);
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Hex edit
        case 'h':
            sprintf(buffer, "%2x", char_present[ypos]);
            textInput(30, ypos + 3, 2, buffer, 2, 3);
            char_present[ypos] = (unsigned char)strtol(buffer, &ptrend, 16);
            enable_overlay_ram();
            POKE(char_address + ypos, char_present[ypos]);
            POKE(charaddress(char_screencode, char_altorstd, 1) + ypos, char_present[ypos]);
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Toggle statusbar
        case CH_F6:
            togglestatusbar();
            break;

        // Help screen
        case CH_F8:
            windowrestore(0);
            helpscreen_load(2);
            if (charsetchanged[0] == 1)
            {
                charset_swap(1);
            }
            showchareditfield();
            showchareditgrid(char_screencode, char_altorstd);
            break;

        // Store to favourites with SHIFT+0-9
        case 33:
            favourites[1] = char_screencode;
            break;

        case 64:
            favourites[2] = char_screencode;
            break;

        case 35:
            favourites[3] = char_screencode;
            break;

        case 36:
            favourites[4] = char_screencode;
            break;

        case 37:
            favourites[5] = char_screencode;
            break;

        case 94:
            favourites[6] = char_screencode;
            break;

        case 38:
            favourites[7] = char_screencode;
            break;

        case 42:
            favourites[8] = char_screencode;
            break;

        case 40:
            favourites[9] = char_screencode;
            break;

        case 41:
            favourites[0] = char_screencode;
            break;

        default:
            // 0-9: Favourites select
            if (key > 47 && key < 58)
            {
                char_screencode = favourites[key - 48];
                charchanged = 1;
            }
            break;
        }

        if (charchanged)
        {
            charchanged = 0;
            char_address = charaddress(char_screencode, char_altorstd, 0);
            enable_overlay_ram();
            for (y = 0; y < 8; y++)
            {
                char_present[y] = PEEK(char_address + y);
                char_undo[y] = char_present[y];
            }
            disable_overlay_ram();
            showchareditgrid(char_screencode, char_altorstd);
        }
    } while (key != CH_ESC && key != CH_STOP);

    windowrestore(0);

    plotscreencode = char_screencode;
    plotaltchar = char_altorstd;
    cputcxy(screen_col, screen_row, plotscreencode + 128);
    strcpy(programmode, "Main");
}