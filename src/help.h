#ifndef HELP_H
#define HELP_H

#include <stdint.h>

// Help screens (FUNCT+8), one per mode group, ported from V1's
// helpscreen_load(screennumber): 1=Main, 2=Character editor,
// 3=Select/Move/Line-Box (shared), 4=Write mode. Each screen is a raw
// 1080-byte (40x27) screencode dump loaded from LOCI at runtime as
// assets/OSEHS{1..4}.BIN (matching V1's own filenames).
void help_show(uint8_t screennumber);

#pragma compile("help.c")

#endif // HELP_H
