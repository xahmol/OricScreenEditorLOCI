#ifndef __GENERIC_H_
#define __GENERIC_H_

// Variables
// Overlays
#define OVERLAYNUMBER       3           // Number of overlays
#define OVERLAYSIZE         0x2000      // Overlay size (align with config)
#define OVERLAYLOAD         0x9400      // Overlay load address (align with config=0xC000-OVERLAYSIZE)
extern unsigned char overlay_active;

// Function prototypes

// Generic functions
void wait(unsigned int wait_cycles);
void debugprint(char *text);

// Overlay functions
void loadoverlay(unsigned char overlay_select);

// Input functions
unsigned char getkey(unsigned char joyallowed, unsigned char norepeat);
signed int textInput(unsigned char xpos, unsigned char ypos, unsigned char width, char *str, unsigned char size, unsigned char validation);

// Screen functions
void cspaces(unsigned char number);
void cleararea(unsigned char xpos, unsigned char ypos, unsigned char height, unsigned char width);
void printcentered(char *text, unsigned char xpos, unsigned char ypos, unsigned char width);
void charset_swap(unsigned char sysorred);
unsigned int screenmap_screenaddr(unsigned char row, unsigned char col, unsigned int width);


#endif // __GENERIC_H_