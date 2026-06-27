// menu.h - Menu system for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Adapted from locifilemanager-v2's src/menu.h/menu.c by Xander Mol (local
// reference at /home/xahmol/git/locifilemanager-v2/src/menu.h). Adapted: 4
// -item menu bar (Screen/File/Charset/Information), 5 pulldowns (incl.
// Yes/No), main-RAM window-save buffer (menu_winbuf[]) replacing overlay-RAM
// cwin_push/pop -- menus must close with no residue in plain Oricutron,
// without requiring LOCI/--loci-flash. Dropped vs. the original:
// menu_fileerrormessage/menu_confirm_file/menu_option_select/menu_popup_*/
// IJK input (not needed until later phases).
//
// Three-layer architecture:
//   menu bar (row 0) -> pulldown menus -> popup dialogs -> LIFO window stack
//
// Return encoding from menu_main():
//   menubarchoice * 10 + menuoptionchoice  (1..49 = a real choice)
//   menubarchoice * 10 + 99 (>= 99)        = ESC pressed at the bar level
// Callers should treat any return value >= 99 as "exit the menu".

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "oric.h"
#include "charsetswap.h"

// -------------------------------------------------------------------------
// Capacity constants
// -------------------------------------------------------------------------

#define MENUBAR_MAXOPTIONS   4
#define MENUBAR_MAXLENGTH   12   // char[] size for each bar title
#define PULLDOWN_NUMBER      5
#define PULLDOWN_MAXOPTIONS  8   // grew from 7 for Charset > Reset Alt->ROM
#define PULLDOWN_MAXLENGTH  17   // 16 visible chars + NUL

#define MENU_WIN_DEPTH       9   // maximum nested window saves

// Main-RAM window-save buffer. Actual worst-case peak: menu_run()'s row-0
// bar save (1*40=40B, always active) + info_version_show()'s full-screen save
// (28*40=1120B) = 1160B. Top-level pulldowns are restored by menu_pulldown()
// itself before menu_run() dispatches to any action, so they don't stack with
// inner popups. Picker nesting (picker_engine 17*40 + picker_make_dir 6*40 =
// 920B) and areyousure (6*40+Yes/No 2*40=320B) stay below 1160B. 1400B gives
// ~240B of margin above the known maximum, which is sufficient headroom.
#define MENU_WINBUF_SIZE  1400

// Special return codes from menu_pulldown()
#define MENU_CANCEL          0   // ESC pressed (when escapable=1)
#define MENU_LEFT_ARROW     18   // left arrow -> caller should open previous bar item
#define MENU_RIGHT_ARROW    19   // right arrow -> caller should open next bar item

// Pulldown index for Yes/No dialog
#define MENU_YESNO           4

// -------------------------------------------------------------------------
// Structs
// -------------------------------------------------------------------------

typedef struct {
    uint16_t  offset;       // offset into menu_winbuf[] where rows are saved
    uint8_t   ypos;         // first screen row saved
    uint8_t   height;       // number of rows saved (each row = SCREEN_COLS bytes)
    uint8_t   swap_charset; // 1 = charsetswap_enter() was called for this save
} MenuWinRecord;

typedef struct {
    char     titles[MENUBAR_MAXOPTIONS][MENUBAR_MAXLENGTH];
    uint8_t  xstart[MENUBAR_MAXOPTIONS];   // screen col of highlight attribute byte
    uint8_t  ypos;                          // screen row where bar is drawn
} MenuBar;

// -------------------------------------------------------------------------
// Exported data (defined in menudata.c)
// -------------------------------------------------------------------------

extern MenuBar menubar;
extern char    pulldown_options[PULLDOWN_NUMBER];
extern char    pulldown_titles[PULLDOWN_NUMBER][PULLDOWN_MAXOPTIONS][PULLDOWN_MAXLENGTH];

// -------------------------------------------------------------------------
// Functions
// -------------------------------------------------------------------------

// Initialise menu module: reset the window-save stack. Call once at startup.
void menu_init(void);

// Draw menu bar at row y; computes xstart[] positions from title lengths.
void menu_placebar(uint8_t y);

// Save rows ypos..ypos+height-1 to the main-RAM window-save stack (LIFO).
// No-op if the stack is full or the buffer is exhausted.
//
// swap_charset: 1 = also call charsetswap_enter() (opt in -- popup chrome
// renders with the ROM-standard charset); 0 = opt out (the character editor,
// so live glyph edits stay visible). menu_winrestore() calls
// charsetswap_exit() automatically iff this was 1.
void menu_winsave(uint8_t ypos, uint8_t height, uint8_t swap_charset);

// Restore the most recently saved rows from menu_winsave(). No-op if the
// stack is empty.
void menu_winrestore(void);

// Paint a white-paper popup background for rows ypos..ypos+height-1
// (col 5 = A_BGWHITE, col 6 = A_FWBLACK, cols 7-39 = space). Cols 0-4 retain
// existing content. Caller is responsible for menu_winsave()/winrestore().
void menu_wininit(uint8_t ypos, uint8_t height);

// Open a pulldown menu at (xpos, ypos) from pulldown_titles[menunumber].
//   escapable: 1 = ESC allowed, 0 = must choose
// Returns: 1..N (choice), MENU_CANCEL (0), MENU_LEFT_ARROW (18), MENU_RIGHT_ARROW (19).
uint8_t menu_pulldown(uint8_t xpos, uint8_t ypos,
                      uint8_t menunumber, uint8_t escapable);

// Run the menu bar navigation loop.
// Returns menubarchoice*10 + menuoptionchoice, or menubarchoice*10 + 99 (ESC).
uint8_t menu_main(void);

// Popup: show message + "Are you sure?" + Yes/No pulldown.
// Returns 1=Yes, 2=No.
uint8_t menu_areyousure(const char *message);

// Popup: show message + wait for keypress.
void menu_messagepopup(const char *message);

#pragma compile("menu.c")

#endif  // MENU_H
