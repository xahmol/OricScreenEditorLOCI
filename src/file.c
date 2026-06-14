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

// File operation menu functions

unsigned char filepickerfromdir(char *headertext, unsigned char filter)
// Pick a file or a directory with the directory browser
{
    unsigned char error, ypos, offs, restlen;

    // Create window and print header text
    windowsave(2, 24, 1);
    ORIC_FillArea(2, 2, CH_SPACE, 38, 24);
    ORIC_VChar(2, 0, A_BGWHITE, 24);
    ORIC_VChar(2, 1, A_FWBLACK, 24);

    
    cputsxy(2, 3, headertext);
    debugprint("filepickerfromdir 1");

    // Start dir browser and select file or dir
    loadoverlay(3);
    error = dir_browse(filter);

    // Clear window and print selected path
    cleararea(2, 5, 21, 38);
    offs = 0;
    ypos = 5;
    restlen = strlen(pathbuffer);
    cputsxy(2, ypos++, "Selected path:");
    while (restlen > 36)
    {
        gotoxy(2, ypos++);
        cprintf(".%.36s", pathbuffer + offs);
        offs += 36;
        restlen -= 36;
    }

    // Return 0 on file selected, else 1
    return error;
}

unsigned char choosedirandfilename(char *headertext, unsigned char maxlen, unsigned char validation)
{
    // Function to present dialogue to enter device id and filename
    // Input: Headertext to print, maximum length of filename input string

    int valid = 0;
    unsigned char len = strlen(filename);

    if (len > maxlen)
    {
        filename[maxlen] = 0;
    }

    windownew(2, 5, 12, 38, 0);
    cputsxy(4, 6, headertext);

    cputsxy(4, 8, "Choose filename:            ");
    valid = textInput(4, 9, 35, filename, maxlen, validation);
    return valid;
}

void loadscreenmap(unsigned char combined)
{
    // Function to load screenmap

    unsigned int newwidth, newheight, newtotal;
    unsigned int maxsize = MEMORYLIMIT - SCREENMAPBASE;
    char *ptrend;
    unsigned char error;
    int address = (combined) ? CHARSET_STD : SCREENMAPBASE;
    int charlen = (combined) ? 1920 : 0;

    error = filepickerfromdir("Load screen", 0);
    // Return on cancel / no file selected
    if (error)
    {
        windowrestore(0);
        return;
    }

    if (!combined)
    {
        cputsxy(2, 14, "Enter screen width:");
        sprintf(buffer, "%i", cfg.screenwidth);
        textInput(25, 14, 4, buffer, 4, 1);
        newwidth = (unsigned int)strtol(buffer, &ptrend, 10);

        cputsxy(2, 15, "Enter screen height:");
        sprintf(buffer, "%i", cfg.screenheight);
        textInput(25, 15, 4, buffer, 4, 1);
        newheight = (unsigned int)strtol(buffer, &ptrend, 10);
    }
    else
    {
        newwidth = 40;
        newheight = SCREENHEIGHT;
    }   

    newtotal = newwidth * newheight;

    if (newtotal > maxsize || newwidth < 40 || newheight < 27)
    {
        cputsxy(2, 16, "New size unsupported. Press key.");
        getkey(ijk_present, 1);
        windowrestore(0);
        return;
    }

    windowrestore(0);
    windowrestore(0);

    // Load screen data
    enable_overlay_ram();
    error = file_load(pathbuffer, (void *)address, newtotal + charlen);
    disable_overlay_ram();
    if (error < 1)
    {
        fileerrormessage(0);
        return;
    }

    cfg.screenwidth = newwidth;
    cfg.screenheight = newheight;
    cfg.screentotal = newtotal;

    if (combined)
    {
        enable_overlay_ram();
        memcpy((void *)SCREENMAPBASE, (void *)SCREENMEMORY, newtotal);
        memcpy((void *)CHARSET_SWAP, (void *)CHARSET_STD, 768);
        disable_overlay_ram();
        cfg.charsetchanged[0] = 1;
        cfg.charsetchanged[1] = 1;
        charset_swap(0);
    }

    ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
    windowsave(0, 1, 0);
    menuplacebar();
    if (cfg.showbar)
    {
        initstatusbar();
    }
}

void savescreenmap(unsigned char combined)
{
    // Function to save screenmap
    int escapeflag;
    // int rc;
    int len = cfg.screenwidth * cfg.screenheight;
    int address = (combined) ? CHARSET_STD : SCREENMAPBASE;

    escapeflag = choosedirandfilename("Save screen", 9, 3);

    windowrestore(0);

    if (escapeflag == -1)
    {
        return;
    }

    if (combined)
    {
        windowrestore(0);
        ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
        charset_swap(1);
        len += 1664;
    }

    sprintf(buffer, "SAVEO\"%s.BIN\",A#%4X,E#%4X", filename, address, address + len - 1);
    // basic(buffer);
    // sprintf(buffer,"%s.BIN",filename);
    // rc = savefile(buffer,(void*)address,len);

    if (combined)
    {
        charset_swap(0);
        windowsave(0, 1, 0);
        menuplacebar();
        if (cfg.showbar)
        {
            initstatusbar();
        }
    }

    // if(rc) { fileerrormessage(rc,0); }
}

void saveproject()
{
    // Function to save project (screen, charsets and metadata)
    // int rc;
    // int len = 0;

    unsigned char error;

    error = choosedirandfilename("Save project", 7, 3);

    windowrestore(0);

    if (error)
    {
        return;
    }

    //sprintf(buffer, "SAVEO\"%sPJ.BIN\",A#%4X,E#%4X", filename, projbuffer, projbuffer + 18);
    // basic(buffer);
    // sprintf(buffer,"%spj.BIN",filename);
    // rc = savefile(buffer,(void*)projbuffer,19);

    // if(rc) { fileerrormessage(rc,0); }

    // Store screen data
    //sprintf(buffer, "SAVEO\"%sSC.BIN\",A#%4X,E#%4X", filename, SCREENMAPBASE, SCREENMAPBASE + (cfg.screenwidth * cfg.screenheight) - 1);
    // basic(buffer);
    // sprintf(buffer,"%ssc.BIN",filename);
    // rc = savefile(buffer,(void*)SCREENMAPBASE,cfg.screenwidth*cfg.screenheight);
    // if(rc) { fileerrormessage(rc,0); }

    // Store standard charset
    if (cfg.charsetchanged[0] == 1)
    {
        //sprintf(buffer, "SAVEO\"%sCS.BIN\",A#%4X,E#%4X", filename, CHARSET_SWAP, CHARSET_SWAP + 767);
        // basic(buffer);
        // sprintf(buffer,"%scs.BIN",filename);
        // rc = savefile(buffer,(void*)CHARSET_SWAP,768);
        // if(rc) { fileerrormessage(rc,0); }
    }

    // Store alternate charset
    if (cfg.charsetchanged[1] == 1)
    {
        //sprintf(buffer, "SAVEO\"%sCA.BIN\",A#%4X,E#%4X", filename, CHARSET_ALT, CHARSET_ALT + 639);
        // basic(buffer);
        // sprintf(buffer,"%sca.BIN",filename);
        // rc = savefile(buffer,(void*)CHARSET_ALT,640);
        // if(rc) { fileerrormessage(rc,0); }
    }
}

void loadproject()
{
    // Function to load project (screen, charsets and metadata)
    // int rc;
    // int len = 0;
    unsigned char projbuffer[19];
    unsigned char error;

    error = filepickerfromdir("Load project", 4);

    windowrestore(0);

    // Return if no file selected
    if (error)
    {
        return;
    }

    // Load project variables
    error = file_load(pathbuffer, projbuffer, 19);
    if (error < 1)
    {
        fileerrormessage(0);
        return;
    }

    cfg.charsetchanged[0] = projbuffer[0];
    cfg.charsetchanged[1] = projbuffer[1];
    cfg.screen_col = projbuffer[2];
    cfg.screen_row = projbuffer[3];
    cfg.screenwidth = projbuffer[4] * 256 + projbuffer[5];
    sprintf(pulldownmenutitles[0][0], "Width:   %5i ", cfg.screenwidth);
    cfg.screenheight = projbuffer[6] * 256 + projbuffer[7];
    sprintf(pulldownmenutitles[0][1], "Height:  %5i ", cfg.screenheight);
    cfg.screentotal = projbuffer[8] * 256 + projbuffer[9];
    cfg.plotink = projbuffer[10];
    cfg.plotpaper = projbuffer[11];
    cfg.plotblink = projbuffer[12];
    cfg.plotdouble = projbuffer[13];
    cfg.plotaltchar = projbuffer[14];
    cfg.plotscreencode = projbuffer[15];
    cfg.xoffset = projbuffer[17];
    cfg.yoffset = projbuffer[18];

    // Load screen
    sprintf(buffer, "!LOAD\"%sSC.BIN\",A#%4X", filename, SCREENMAPBASE);
    // basic(buffer);
    // sprintf(buffer,"%ssc.BIN",filename);
    // rc=loadfile(buffer,(void*)SCREENMAPBASE,&len);
    // if(len)
    //{
    windowrestore(0);
    ORIC_CopyViewPort(SCREENMAPBASE, cfg.screenwidth, cfg.xoffset, cfg.yoffset, 0, 0, 40, 27);
    windowsave(0, 1, 0);
    menuplacebar();
    if (cfg.showbar)
    {
        initstatusbar();
    }
    //}

    // Load standard charset
    if (cfg.charsetchanged[0] == 1)
    {
        sprintf(buffer, "!LOAD\"%sCS.BIN\",A#%4X", filename, CHARSET_SWAP);
        // basic(buffer);
        // sprintf(buffer,"%scs.BIN",filename);
        // rc=loadfile(buffer,(void*)CHARSET_SWAP,&len);
        // if(!len) { cfg.charsetchanged[0]=0; }
    }

    // Load alternate charset
    if (cfg.charsetchanged[1] == 1)
    {
        sprintf(buffer, "!LOAD\"%sCA.BIN\",A#%4X", filename, CHARSET_ALT);
        // basic(buffer);
        // sprintf(buffer,"%sca.BIN",filename);
        // rc=loadfile(buffer,(void*)CHARSET_ALT,&len);
        // if(!len) { cfg.charsetchanged[1]=0; }
    }
}

void loadcharset(unsigned char stdoralt)
{
    // Function to load charset
    // Input: stdoralt: standard charset (0), alternate charset (1) or combined (2)

    unsigned int charsetaddress;
    int escapeflag;
    // int rc;
    // int len = 0;

    escapeflag = filepickerfromdir("Load character set", 0);

    windowrestore(0);

    if (escapeflag == -1)
    {
        return;
    }

    if (stdoralt == 0)
    {
        charsetaddress = CHARSET_SWAP;
    }
    if (stdoralt == 1)
    {
        charsetaddress = CHARSET_ALT;
    }
    if (stdoralt == 2)
    {
        charsetaddress = CHARSET_STD;
    }

    sprintf(buffer, "!LOAD\"%s.BIN\",A#%4X", filename, charsetaddress);
    // basic(buffer);
    // sprintf(buffer,"%s.*",filename);
    // rc = loadfile(buffer,(void*)charsetaddress,&len);

    // if(len)
    //{
    if (stdoralt < 2)
    {
        cfg.charsetchanged[stdoralt] = 1;
    }
    else
    {
        cfg.charsetchanged[0] = 1;
        cfg.charsetchanged[1] = 1;
        enable_overlay_ram();
        memcpy((void *)CHARSET_SWAP, (void *)CHARSET_STD, 768);
        disable_overlay_ram();
        charset_swap(0);
    }
    //}
}

void savecharset(unsigned char stdoralt)
{
    // Function to save charset
    // Input: stdoralt: standard charset (0), alternate charset (1) or combined (2)

    unsigned int charsetaddress;
    int escapeflag;
    // int rc;
    int len;

    escapeflag = choosedirandfilename("Save character set", 9, 3);

    windowrestore(0);

    if (escapeflag == -1)
    {
        return;
    }

    if (stdoralt == 0)
    {
        charsetaddress = CHARSET_SWAP;
        len = 768;
    }
    if (stdoralt == 1)
    {
        charsetaddress = CHARSET_ALT;
        len = 640;
    }
    if (stdoralt == 2)
    {
        charsetaddress = CHARSET_STD;
        charset_swap(1);
        len = 1664;
    }

    sprintf(buffer, "SAVEO\"%s.BIN\",A#%4X,E#%4X", filename, charsetaddress, charsetaddress + len - 1);
    // basic(buffer);
    // sprintf(buffer,"%s.BIN",filename);
    // rc = savefile(buffer,(void*)charsetaddress,len);
    // if(rc) { fileerrormessage(rc,0); }
    if (stdoralt == 2)
    {
        charset_swap(0);
    }
}