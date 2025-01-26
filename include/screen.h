#ifndef __SCREEN_H_
#define __SCREEN_H_

// Function prototypes

// Status bar functions
void printstatusbar();
void initstatusbar();
void hidestatusbar();
void togglestatusbar();

// Generic screen map routines
void screenmapplot(unsigned char row, unsigned char col, unsigned char screencode);
void cursormove(unsigned char left, unsigned char right, unsigned char up, unsigned char down);
void plotmove(unsigned char direction);

// Help screens
void helpscreen_load(unsigned char screennumber);

#endif // __SCREEN_H_