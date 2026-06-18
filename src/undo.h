#ifndef UNDO_H
#define UNDO_H

#include <stdint.h>

// Reset all undo/redo state. Call once from editor_run()'s init block.
void undo_init(void);

// Snapshot the rect [x,y .. x+w,y+h) from screenmap[] before a destructive
// edit overwrites it. No-op if the rect's bytes (w*h) wouldn't fit in the
// undo region at all (Screen > Clear/Fill, or a Select/Line-Box fill
// close to the full canvas size, on a large canvas -- see src/undo.c's
// file header comment) -- that action simply becomes non-undoable rather
// than corrupting memory.
void undo_snapshot(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Undo the most recent snapshot ('z' in Main mode). No-op if there is
// nothing to undo.
void undo_perform(void);

// Redo the most recently undone snapshot ('y' in Main mode). No-op if
// there is nothing to redo.
void redo_perform(void);

#pragma compile("undo.c")

#endif // UNDO_H
