#ifndef PALETTE_H
#define PALETTE_H

// Palette popup (entered via 'p' from main mode). Near-full-width popup
// showing the favourites row, the full standard charset (0x20-0x7F) and the
// full alternate charset (0x20-0x7F, optionally remapped via visualchar[]),
// for selecting app.plotscreencode/app.plotaltchar. ESC leaves both
// unchanged.
void palette_run(void);

#pragma compile("palette.c")

#endif // PALETTE_H
