#ifndef __MENU_H_
#define __MENU_H_

// Function prototypes
// Windows
void windowsave(unsigned char ypos, unsigned char height, unsigned char loadsyscharset);
void windowrestore(unsigned char restorealtcharset);
void windownew(unsigned char xpos, unsigned char ypos, unsigned char height, unsigned char width, unsigned char loadsyscharset);

// Menus
void menuplacebar();
unsigned char menupulldown(unsigned char xpos, unsigned char ypos, unsigned char menunumber, unsigned char escapable);
unsigned char menumain();
unsigned char areyousure(char *message, unsigned char syscharset);
void fileerrormessage(unsigned char syscharset);
void messagepopup(char *message, unsigned char syscharset);

// Variables

// Window data
struct WindowStruct
{
    unsigned int address;
    unsigned char ypos;
    unsigned char height;
};
extern struct WindowStruct Window[9];
extern unsigned int windowaddress;
extern unsigned char windownumber;

// Menu data
extern unsigned char menubaroptions;
extern unsigned char pulldownmenunumber;
extern char menubartitles[4][12];
extern unsigned char menubarcoords[4];
extern unsigned char pulldownmenuoptions[5];
extern char pulldownmenutitles[5][6][16];

#endif // __MENU_H_