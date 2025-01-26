#ifndef __CHAREDIT_H_
#define __CHAREDIT_H_

// Function prototypes
// Placed in:
// Overlay 2

// Character editor functions
void showchareditfield();
unsigned int charaddress(unsigned char screencode, unsigned char stdoralt, unsigned char swap);
void showchareditgrid(unsigned int screencode, unsigned char stdoralt);
void chareditor();

#endif // __CHAREDIT_H_