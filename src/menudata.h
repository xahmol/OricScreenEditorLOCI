// menudata.h - OricScreenEditorLOCI menu tables, dialogs and dispatch
//
// Owns the data tables required by src/menu.h (menubar, pulldown_options,
// pulldown_titles) and the app-specific menu_run() entry point. Keeping this
// separate from menu.c lets menu.c stay a generic, app-agnostic menu engine.

#ifndef MENUDATA_H
#define MENUDATA_H

// Initialise menu tables (sets the Screen > Width/Height titles from the
// current canvas size). Call once at startup, after canvas_init().
void menudata_init(void);

// Rewrite the Screen > Width:/Height: pulldown titles from the current
// canvas size. Call after any canvas_resize() outside resize_dialog()
// (e.g. src/fileio.c's Project/Screen/Combined load).
void update_size_titles(void);

// Goto coordinates popup ('j' in Main mode): prompts for X then Y,
// jumps the cursor + viewport there via canvas_goto() on confirm.
void goto_dialog(void);

// Open the menu bar (row 0) and run it until ESC is pressed at the bar
// level. Dispatches Screen menu items (Width/Height/Clear/Fill); File/
// Charset/Information items show a "not yet implemented" popup. Restores
// row 0 and redraws the canvas + statusbar on exit.
void menu_run(void);

#pragma compile("menudata.c")

#endif  // MENUDATA_H
