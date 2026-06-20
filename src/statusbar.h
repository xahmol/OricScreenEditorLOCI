#ifndef STATUSBAR_H
#define STATUSBAR_H

#include "charwin.h"

extern OricCharWin statusbar_win;

void statusbar_init(void);
void statusbar_draw(void);
void statusbar_show(uint8_t on);
void statusbar_set_override(const char *text);
void statusbar_clear_override(void);

#pragma compile("statusbar.c")

#endif // STATUSBAR_H
