#ifndef FINDREPLACE_H
#define FINDREPLACE_H

// Unified Find/Replace, entered via 'f' from Main mode. Prompts for a
// target (screencode/ink/paper), a value to find, then a replacement
// value: ESC at the replacement step means "find next occurrence only"
// (jumps the cursor there, canvas unchanged); ENTER means "replace
// every occurrence" (whole-canvas undo_snapshot() first, same as
// Screen > Clear/Fill -- inherits that same graceful skip if the
// canvas is too big for the undo region). New functionality, no V1
// precedent.
void findreplace_run(void);

#pragma compile("findreplace.c")

#endif // FINDREPLACE_H
