#ifndef COLOURPICKER_H
#define COLOURPICKER_H

// Colour picker popup (entered via 'c' from main mode). New OSE-LOCI feature
// over V1 (see README.md "Planned feature additions over V1"): an 8x8
// ink x paper grid (one screen row per paper value, all 8 ink columns on
// it, drawn via direct screen-RAM writes -- see src/colourpicker.c's header
// comment for why) where each cell shows both the normal (paper-colour) and
// inverse (ink-colour) swatch for that combination, for selecting
// app.plotink/app.plotpaper. ESC leaves both unchanged.
void colourpicker_run(void);

#pragma compile("colourpicker.c")

#endif // COLOURPICKER_H
