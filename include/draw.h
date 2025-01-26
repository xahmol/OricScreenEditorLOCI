#ifndef __DRAW_H_
#define __DRAW_H_

// Placed in:
// Overlay 1

// Function prototypes

// Drawing functions
void plot_try();
void writemode();
void plotvisible(unsigned char row, unsigned char col, unsigned char setorrestore);
void lineandbox(unsigned char draworselect);
void movemode();
void selectmode();

#endif // __DRAW_H_