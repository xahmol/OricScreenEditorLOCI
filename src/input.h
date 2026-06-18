#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

// Universal blocking key/joystick read, replacing cwin_getch() everywhere
// in src/*.c. If an IJK joystick is present (ijk_detect(), called once
// from main.c), polls it first each iteration and maps a direction/fire
// to the matching cursor/ENTER key code; otherwise (or if absent) falls
// back to a non-blocking keyboard poll. See src/input.c for the full
// contract -- a port of nonworkingcc65's getkey(joyallowed=1, norepeat=1).
uint8_t key_read(void);

#pragma compile("input.c")

#endif // INPUT_H
