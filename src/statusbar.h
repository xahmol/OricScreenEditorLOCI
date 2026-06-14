#ifndef STATUSBAR_H
#define STATUSBAR_H

#include "charwin.h"

extern OricCharWin statusbar_win;

void statusbar_init(void);
void statusbar_draw(void);
void statusbar_show(uint8_t on);

#pragma compile("statusbar.c")

#endif // STATUSBAR_H
