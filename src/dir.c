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

// Placed in:
// Main
#pragma code-name("COE");
#pragma rodata-name("RODATA");

struct DirElement presentdirelement;
struct Directory presentdir;
DIR *dir;
struct dirent *file;
unsigned present;
unsigned previous;
unsigned next;
unsigned char sort;
unsigned char filter;

char *diraddress = (char *)DIRBASE;

char dir_entry_types[8][8] =
    {
        "    DIR",
        "    TAP",
        "    BIN",
        "Project",
        " Screen",
        "StdChar",
        "AltChar",
        "       "};

// Placed in:
// Overlay 3
#pragma code-name("OVERLAY3");
#pragma rodata-name("OVERLAY3");

// Functions
void dir_get_element(unsigned address)
// Get directory element from address
// Input: address = address of element
{
    unsigned workaddress = address;
    xram_memcpy_from(&presentdirelement.meta, workaddress, sizeof(presentdirelement.meta));
    workaddress += sizeof(presentdirelement.meta);
    xram_memcpy_from(presentdirelement.name, workaddress, presentdirelement.meta.length);
}

void dir_save_element(unsigned address)
// Save directory element to address
// Input: address = address of element
{
    unsigned workaddress = address;
    xram_memcpy_to(workaddress, &presentdirelement.meta, sizeof(presentdirelement.meta));
    workaddress += sizeof(presentdirelement.meta);
    xram_memcpy_to(workaddress, presentdirelement.name, presentdirelement.meta.length);
}

void dir_read(unsigned char filter)
// Read directory entries to memory from present path
// Input: dirnr = directory number
//        filter = filter for file type
{
    unsigned char presenttype;
    unsigned char datalength;
    unsigned char count = 0;
    unsigned char xpos = 3;
    unsigned char ypos = 5;
    unsigned element;
    struct DirElement bufferdir;
    unsigned workaddress;
    unsigned char inserted;

    ypos++;

    // Prepare progress bar
    gotoxy(2, ypos);
    cputc(A_ALT);
    cclear(37);

    previous = 0;

    // Reset dirdata
    presentdir.address = DIRBASE;
    present = presentdir.address;

    // Clear directory values
    presentdir.firstelement = 0;
    presentdir.firstprint = 0;
    presentdir.lastprint = 0;
    presentdir.position = 0;
    presentdir.present = 0;

    // Initialise reading dir
    dir = opendir(presentdir.path);
    if (!dir)
    {
        return;
    }

    // Loop while dir data is available or memory is full
    while (file = readdir(dir), file->d_name[0] != 0)
    {
        // Reset entry type
        presenttype = 0;

        // Get next dir entry
        datalength = strlen(file->d_name);

        sprintf(buffer, "Reading %s", file->d_name);
        debugprint(buffer);

        // print progress counter
        if ((count >> 2) > 36)
        {
            xpos = 3;
            count = 0;
            gotoxy(xpos, ypos);
            cclear(37);
        }
        else
        {
            gotoxy(xpos + (count >> 2), ypos);
            cputc(progressBar[count & 3]);
            ++count;
        }

        debugprint("dir_read 1");

        // Check if entry is a directory
        if (file->d_attrib & DIR_ATTR_DIR)
        {
            presenttype = 1;

            // Add / to dir name if room is left
            if (datalength < 64)
            {
                datalength++;
                strcat(file->d_name, "/");
            }
        }

        // Check if file is a matching image type
        if (!presenttype && datalength > 4)
        {

            // Check for filename extension for file type
            // First check for a dot four chars from the end
            if (file->d_name[datalength - 4] == '.')
            {
                // Check for TAP
                if ((file->d_name[datalength - 3] == 't' || file->d_name[datalength - 3] == 'T') &&
                    (file->d_name[datalength - 2] == 'a' || file->d_name[datalength - 2] == 'A') &&
                    (file->d_name[datalength - 1] == 'p' || file->d_name[datalength - 1] == 'P'))
                {
                    presenttype = 2;
                }

                // Check for BIN
                if ((file->d_name[datalength - 3] == 'b' || file->d_name[datalength - 3] == 'B') &&
                    (file->d_name[datalength - 2] == 'i' || file->d_name[datalength - 2] == 'I') &&
                    (file->d_name[datalength - 1] == 'n' || file->d_name[datalength - 1] == 'N'))
                {
                    presenttype = 3;
                }
            }
        }

        // Then check for OSE native file types if file type is BIN
        if (presenttype == 3 && datalength > 8 && file->d_name[datalength - 8] == '.')
        {
            // Check for Project
            if ((file->d_name[datalength - 7] == 'p' || file->d_name[datalength - 7] == 'P') &&
                (file->d_name[datalength - 6] == 'r' || file->d_name[datalength - 6] == 'R') &&
                (file->d_name[datalength - 5] == 'j' || file->d_name[datalength - 5] == 'J'))
            {
                presenttype = 4;
            }

            // Check for Screen
            if ((file->d_name[datalength - 7] == 's' || file->d_name[datalength - 7] == 'S') &&
                (file->d_name[datalength - 6] == 'c' || file->d_name[datalength - 6] == 'C') &&
                (file->d_name[datalength - 5] == 'r' || file->d_name[datalength - 5] == 'R'))
            {
                presenttype = 5;
            }

            // Check for Standard Charset
            if ((file->d_name[datalength - 7] == 'c' || file->d_name[datalength - 7] == 'C') &&
                (file->d_name[datalength - 6] == 's' || file->d_name[datalength - 6] == 'S') &&
                (file->d_name[datalength - 5] == 's' || file->d_name[datalength - 5] == 'S'))
            {
                presenttype = 6;
            }

            // Check for Alternate Charset
            if ((file->d_name[datalength - 7] == 'c' || file->d_name[datalength - 7] == 'C') &&
                (file->d_name[datalength - 6] == 's' || file->d_name[datalength - 6] == 'S') &&
                (file->d_name[datalength - 5] == 'a' || file->d_name[datalength - 5] == 'A'))
            {
                presenttype = 7;
            }
        }

        // Set file type to unknown if not set yet
        if (!presenttype)
        {
            presenttype = 8;
        }

        // Filter out hidden files
        if (file->d_name[0] == '.')
        {
            presenttype = 0;
        }

        // Filter out non-matching file types if filter is set
        if (filter && presenttype > 1)
        {
            if (presenttype != filter + 1)
            {
                presenttype = 0;
            }
        }

        sprintf(buffer,"FT:%u AD:%4x DL:%3u", presenttype, presentdir.address, datalength);
        debugprint(buffer);

        if (presenttype)
        {
            // Check if sufficient memory is left
            if (presentdir.address + datalength + sizeof(presentdirelement.meta) > presentdir.address + DIRSIZE - 1)
            {
                return;
            }

            // Set direntry pointers
            // Is this the first entry?
            if (!previous)
            {
                presentdir.firstelement = present;
                presentdir.firstprint = present;
                presentdirelement.meta.prev = 0;
                previous = present;
                presentdirelement.meta.next = 0;
            }
            // All next entries
            else
            {
                // Does the list need to be sorted?
                if (sort)
                {
                    inserted = 0;
                    // Find element to insert after
                    element = presentdir.firstelement;
                    do
                    {
                        workaddress = element;
                        xram_memcpy_from(&bufferdir.meta, workaddress, sizeof(bufferdir.meta));
                        workaddress += sizeof(bufferdir.meta);
                        xram_memcpy_from(bufferdir.name, workaddress, bufferdir.meta.length);

                        if (stricmp(bufferdir.name, file->d_name) > 0)
                        {
                            // Insert before the first one?
                            if (!bufferdir.meta.prev)
                            {
                                presentdirelement.meta.prev = 0;
                                presentdirelement.meta.next = element;
                                bufferdir.meta.prev = present;
                                presentdir.firstelement = present;
                                presentdir.firstprint = present;
                                xram_memcpy_to(element, &bufferdir.meta, sizeof(bufferdir.meta));
                            }
                            else
                            // Insert in between
                            {
                                presentdirelement.meta.prev = bufferdir.meta.prev;
                                presentdirelement.meta.next = element;
                                bufferdir.meta.prev = present;
                                xram_memcpy_to(element, &bufferdir.meta, sizeof(bufferdir.meta));
                                xram_memcpy_from(&bufferdir.meta, presentdirelement.meta.prev, sizeof(bufferdir.meta));
                                bufferdir.meta.next = present;
                                xram_memcpy_to(presentdirelement.meta.prev, &bufferdir.meta, sizeof(bufferdir.meta));
                            }
                            inserted = 1;
                            break;
                        }
                        element = bufferdir.meta.next;
                    } while (element);

                    // Insert at the end
                    if (!inserted)
                    {
                        presentdirelement.meta.prev = previous;              // Set prev in new entry
                        xram_memcpy_to(previous, &present, sizeof(present)); // Set next in previous entry
                        previous = present;
                        presentdirelement.meta.next = 0;
                    }
                }
                else
                {
                    presentdirelement.meta.prev = previous;              // Set prev in new entry
                    xram_memcpy_to(previous, &present, sizeof(present)); // Set next in previous entry
                    previous = present;
                    presentdirelement.meta.next = 0;
                }
            }

            presentdirelement.meta.select = 0;
            presentdirelement.meta.type = presenttype;
            datalength++;
            presentdirelement.meta.length = datalength;

            sprintf(buffer,"PR:%4x PV:%4x NX:%4x", present, previous, next);
            debugprint(buffer);

            // Set meta data
            xram_memcpy_to(presentdir.address, &presentdirelement.meta, sizeof(presentdirelement.meta));
            presentdir.address += sizeof(presentdirelement.meta);

            // Set filename
            xram_memcpy_to(presentdir.address, file->d_name, datalength);
            presentdir.address += datalength;

            // Update present pointer
            present = presentdir.address;
        }
    }
    closedir(dir);

    if (presentdir.firstelement)
    {
        present = presentdir.firstelement;
        dir_get_element(present);
    }
    else
    {
        present = 0;
    }

    dir_print_id_and_path();
}

void dir_print_id_and_path()
// Draw drive ID and path
// Input: dirnr = directory number
{
    unsigned char ypos = 5;

    debugprint("dir print id and path 1");
    gotoxy(2,4);
    cprintf("%u %.30s", presentdir.drive, presentdir.path);
    cgetc();

    // Print device name
    // First set color and clear area
    gotoxy(2, ypos);
    cclear(38);

    // Print device name
    cputs(get_loci_devname(presentdir.drive, 38));

    // Get drive ID
    gotoxy(2, ++ypos);
    cclear(38);
    gotoxy(2, ypos);
    strncpy(buffer, presentdir.path, 3);
    buffer[3] = 0;
    cputs(buffer);

    // Get rest of path and check if it fits else shorten
    cprintf("%.35s", presentdir.path + 3);
    debugprint("dir_print_id_and_path 2");
}

void dir_print_entry(unsigned char printpos)
// Print directory entry
// Input: present = pointer to directory element
//        dirnr = directory number
//        printpos = position to print
{
    unsigned char ypos = 7;
    unsigned char bg_color;

    // Set colors based on whether pane is active or not
    bg_color = (printpos == presentdir.position) ? A_BGYELLOW : A_BGCYAN;

    ypos += printpos;

    // Clear area
    gotoxy(2, ypos);
    cputc(bg_color);
    cclear(37);
    gotoxy(3, ypos);

    // Print '-'indicator if selected
    if (printpos == presentdir.position)
    {
        cputc('-');
    }
    else
    {
        cputc(' ');
    }

    // Print entry data
    cprintf("%-26.26s %s%c", presentdirelement.name, dir_entry_types[presentdirelement.meta.type - 1], A_BGWHITE);
}

void dir_draw(unsigned char readdir)
// Draw the dirlisting
// Input: dirnr = directory number
//        readdir = read directory, flag for 0 = just reprint same dir, 1 = read new dir
{
    unsigned char ypos = 5;
    unsigned char printpos = 0;
    unsigned element;

    // Clear area
    cleararea(2, ypos, 38, 17);

    // Print header
    dir_print_id_and_path();

    // Read directory contents
    if (readdir)
    {
        dir_read(filter);
    }

    presentdir.present = presentdir.firstprint;

    // Print no data if no valid entries in dir are found
    if (!presentdir.firstprint)
    {
        gotoxy(2, ypos + 2);
        cputc(A_FWRED);
        cputs("Empty directory.");
    }
    // Print entries until area is filled or last item is reached
    else
    {
        // Get direlement
        element = presentdir.firstprint;

        // Loop while area is not full and further direntries are still present
        do
        {
            // Print entry and increase printpos
            dir_get_element(element);
            dir_print_entry(printpos);
            presentdir.lastprint = element;
            if (printpos == presentdir.position)
            {
                presentdir.present = element;
            }
            printpos++;

            // Check if next dir entry is present, if no: break. If yes: update present pointer
            if (!presentdirelement.meta.next)
            {
                break;
            }
            else
            {
                element = presentdirelement.meta.next;
            }

        } while (printpos < PANE_HEIGHT);
        present = presentdir.present;
    }
}

void dir_get_next_drive()
// Get the next avaailable drive for pane
{
    unsigned char drive = presentdir.drive;
    unsigned char valid = 0;

    // Find first next valid drive
    do
    {
        drive++;
        if (drive > MAXDEV)
        {
            drive = 0;
        }
        if (locicfg.validdev[drive])
        {
            valid = 1;
        }
    } while (!valid);

    // Set drive
    presentdir.drive = drive;

    // Set root as path
    sprintf(buffer, "%1u:/", presentdir.drive);
    strncpy(presentdir.path, buffer, 4);

    // Draw new dir
    dir_draw(1);
}

void dir_get_prev_drive()
// Get the previous avaailable drive for pane
{
    unsigned char drive = presentdir.drive;
    unsigned char valid = 0;

    // Find first previous valid drive
    do
    {
        if (drive > 0)
        {
            drive--;
        }
        else
        {
            drive = MAXDEV;
        }

        if (locicfg.validdev[drive])
        {
            valid = 1;
        }
    } while (!valid);

    // Set drive
    presentdir.drive = drive;

    // Set root as path
    sprintf(buffer, "%1u:/", presentdir.drive);
    strncpy(presentdir.path, buffer, 4);

    // Draw new dir
    dir_draw(1);
}

void dir_go_down()
// Scroll down in dir of active pane
{
    // Are there dir entries? And is there a next entry?
    if (presentdir.firstelement && presentdirelement.meta.next)
    {
        present = presentdirelement.meta.next;
        dir_get_element(present);
        presentdir.present = present;
        presentdir.position++;

        // Check if next entry is on a next page. If yes, print next page
        if (presentdir.position > PANE_HEIGHT - 1)
        {
            presentdir.position = 0;
            presentdir.firstprint = present;
            dir_draw(0);
        }
        else
        {
            // Select next entry
            previous = presentdirelement.meta.prev;
            dir_get_element(previous);
            dir_print_entry(presentdir.position - 1);
            dir_get_element(present);
            dir_print_entry(presentdir.position);
        }
    }
}

void dir_go_up()
// Scroll up in dir of active pane
{
    unsigned char count;

    // Are there dir entries? And is there a previous entry?
    if (presentdir.firstelement && presentdirelement.meta.prev)
    {
        present = presentdirelement.meta.prev;
        dir_get_element(present);
        presentdir.present = present;

        // Check if previous entry is on a previous page. If yes, print previous page
        if (!presentdir.position)
        {
            presentdir.position = PANE_HEIGHT - 1;
            for (count = 0; count < 9; count++)
            {
                if (!presentdirelement.meta.prev)
                {
                    break;
                }
                present = presentdirelement.meta.prev;
                dir_get_element(present);
            }
            presentdir.firstprint = present;
            dir_draw(0);
        }
        else
        {
            // Select previous entry
            presentdir.position--;
            next = presentdirelement.meta.next;
            dir_get_element(next);
            dir_print_entry(presentdir.position + 1);
            dir_get_element(present);
            dir_print_entry(presentdir.position);
        }
    }
}

void dir_last_of_page()
// Go to last entry in present page
{
    unsigned element;
    unsigned char count;
    unsigned char position = 0;
    unsigned char oldpos = presentdir.position;

    // Are there dir entries?
    if (presentdir.firstelement)
    {
        // Find last of page
        element = presentdir.firstprint;
        dir_get_element(element);
        for (count = 0; count < PANE_HEIGHT - 1; count++)
        {
            if (!presentdirelement.meta.next)
            {
                break;
            }
            position++;
            element = presentdirelement.meta.next;
            dir_get_element(element);
        }

        // Set new variables and pint old and new entries in correct colour
        presentdir.position = position;
        dir_get_element(present);
        dir_print_entry(oldpos);
        presentdir.present = element;
        present = element;
        dir_get_element(present);
        dir_print_entry(position);
    }
}

void dir_pagedown()
// Page down in dir of active pane
{
    unsigned element;
    unsigned char count;

    // Are there dir entries?
    if (presentdir.firstelement)
    {
        element = presentdir.lastprint;
        dir_get_element(element);

        // Is there a next page?
        if (presentdirelement.meta.next)
        {
            dir_get_element(present);
            // Search how long next page is
            for (count = 0; count < PANE_HEIGHT; count++)
            {
                if (!presentdirelement.meta.next)
                {
                    break;
                }
                present = presentdirelement.meta.next;
                dir_get_element(present);
            }

            // Set new firstprint and present
            presentdir.firstprint = present;
            presentdir.present = present;
            presentdir.position = 0;

            // Print new page
            dir_draw(0);
        }
        else
        {
            // Go to boyyom
            dir_last_of_page();
        }
    }
}

void dir_pageup()
// Page up in dir of active pane
{
    unsigned char count;

    // Are there dir entries?
    if (presentdir.firstelement && presentdirelement.meta.prev)
    {
        // Search how long previous page is
        for (count = 0; count < PANE_HEIGHT; count++)
        {
            if (!presentdirelement.meta.prev)
            {
                break;
            }
            present = presentdirelement.meta.prev;
            dir_get_element(present);
        }

        // Set new firstprint and present
        presentdir.firstprint = present;
        presentdir.present = present;
        presentdir.position = 0;

        // Print new page
        dir_draw(0);
    }
}

void dir_top()
// Go to top of dir in active pane
{
    // Are there dir entries?
    if (presentdir.firstelement)
    {
        // Set present to first element and print new page
        present = presentdir.firstelement;
        dir_get_element(present);
        presentdir.present = present;
        presentdir.position = 0;
        presentdir.firstprint = present;
        dir_draw(0);
    }
}

void dir_bottom()
// Go to bottom of dir in active pane
{
    unsigned char count;

    // Are there dir entries? And is there a next dir entry
    if (presentdir.firstelement && presentdirelement.meta.next)
    {
        // Check if not already at bottom page
        present = presentdir.lastprint;
        dir_get_element(present);
        if (!presentdirelement.meta.next)
        {
            dir_last_of_page();
            return;
        }

        // Find last element
        present = presentdir.firstelement;
        do
        {
            dir_get_element(present);
            if (presentdirelement.meta.next)
            {
                present = presentdirelement.meta.next;
            }
            else
            {
                break;
            }
        } while (1);

        // Go back one page minus one element
        presentdir.present = present;
        presentdir.position = PANE_HEIGHT - 1;
        for (count = 0; count < PANE_HEIGHT - 1; count++)
        {
            present = presentdirelement.meta.prev;
            dir_get_element(present);
        }
        presentdir.firstprint = present;
        present = presentdir.lastprint;

        // Print new page
        dir_draw(0);
    }
}

void dir_gotoroot()
// Go to root of active pane
{
    // Set root as path
    sprintf(buffer, "%1u:/", presentdir.drive);
    strncpy(presentdir.path, buffer, 4);

    // Draw new dir
    dir_draw(1);
}

void dir_parentdir()
// Go to parent dir of active pane
{
    unsigned char length;
    unsigned char i;

    // Check if root dir
    length = strlen(presentdir.path);
    if (length > 3)
    {
        // Find last /
        for (i = length - 2; i > 2; i--)
        {
            if (presentdir.path[i] == '/')
            {
                break;
            }
        }

        // Set new path
        presentdir.path[i + 1] = 0;

        // Draw new dir
        dir_draw(1);
    }
}

void dir_togglesort()
// Toggle sort order of active pane
{
    sort = !sort;
    dir_draw(1);
}

unsigned char dir_browse(unsigned char filter)
// Browse directory
// Input: filter for which filetype to filter on
// Output:  pathname in patthbuffer
//          1 = no file selected
//          0 = file selected
{
    unsigned char done = 0;
    unsigned char key;

    // Draw directory
    dir_draw(1);

    // Browse loop
    do
    {
        present = presentdir.present;
        if (present)
        {
            dir_get_element(present);
        }

        key = getkey(ijk_present, 0);

        switch (key)
        {
        case CH_CURS_RIGHT:
        case 's':
            // Select dir if filter is on type dir and entry is a dir
            if (filter == 1 && presentdir.firstelement && presentdirelement.meta.type == 1)
            {
                strncpy(pathbuffer, presentdir.path, sizeof(pathbuffer));
                strncat(pathbuffer, presentdirelement.name, sizeof(pathbuffer) - strlen(pathbuffer));
                strncpy(presentdir.path, pathbuffer, sizeof(presentdir.path));
                done = 1;
            }
            break;

        case CH_ENTER:
            // Return: Go to dir if entry is a dir
            if (presentdir.firstelement && presentdirelement.meta.type == 1)
            {
                strncpy(pathbuffer, presentdir.path, sizeof(pathbuffer));
                strncat(pathbuffer, presentdirelement.name, sizeof(pathbuffer) - strlen(pathbuffer));
                strncpy(presentdir.path, pathbuffer, sizeof(presentdir.path));
                dir_draw(1);
            }
            else
            {
                // Return: select file
                strncpy(pathbuffer, presentdir.path, sizeof(pathbuffer));
                strncat(pathbuffer, presentdirelement.name, sizeof(pathbuffer) - strlen(pathbuffer));
                strncpy(presentdir.path, pathbuffer, sizeof(presentdir.path));
                done = 1;
            }
            break;

        case CH_ESC:
            // Escape: Cancel
            done = 2;
            break;

        case '.':
            // .: Next drive for active pane
            dir_get_next_drive();
            break;

        case ',':
            // ,: Previous drive for active pane
            dir_get_prev_drive();
            break;

        case '\\':
            // \: Go to root
            dir_gotoroot();
            break;

        case CH_CURS_LEFT:
            // Curs Left: Go to parent directory
            dir_parentdir();
            break;

        case CH_CURS_DOWN:
            // Curs Down: Scroll down
            dir_go_down();
            break;

        case CH_CURS_UP:
            // Curs Up: Scroll up
            dir_go_up();
            break;

        case 'd':
            // d: Page down
            dir_pagedown();
            break;

        case 'p':
            // p: Page up
            dir_pageup();
            break;

        case 't':
            // t: Go to top
            dir_top();
            break;

        case 'b':
            // b: Go to bottom
            dir_bottom();
            break;

        case 'o':
            // o: Toggle sort
            dir_togglesort();
            break;

        default:
            break;
        }
    } while (!done);

    // Return 1 on cancel, 0 on succes
    return (done == 2) ? 1 : 0;
}

// Placed in:
// Main
#pragma code-name("COE");
#pragma rodata-name("RODATA");
