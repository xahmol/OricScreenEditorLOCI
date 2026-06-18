// strings_en.h - OricScreenEditorLOCI user-visible strings (English)
// All MSG_* macros for the application, in English. Selected via strings.h
// when LANG_FR is NOT defined (the default build).

#ifndef STRINGS_EN_H
#define STRINGS_EN_H

// ── LOCI library messages (include/loci.c, get_locicfg()) ────────────────
// Not yet wired into the build (see ARCHITECTURE.md "LOCI and IJK"), kept
// for when loci.c is included.
#define MSG_LOCI_NOT_FOUND  "No LOCI device detected or firmware too old."
#define MSG_PRESS_KEY_EXIT  "Press any key to exit."

// ── Splash screen (src/main.c) ────────────────────────────────────────────
#define MSG_SPLASH_TITLE      "OricScreenEditor for LOCI"
#define MSG_SPLASH_BUILD_FMT  "Oscar64 build v%d.%d.%d"
#define MSG_SPLASH_PRESSKEY   "Press any key to start"

// ── Menu bar titles (src/menudata.c, menubar.titles) ──────────────────────
#define MSG_MENU_BAR_SCREEN   "Screen"
#define MSG_MENU_BAR_FILE     "File"
#define MSG_MENU_BAR_CHARSET  "Charset"
#define MSG_MENU_BAR_INFO     "Information"

// ── Pulldown 0 -- Screen ───────────────────────────────────────────────────
// pulldown_titles[0][0]/[0][1] are rewritten at runtime from
// MSG_SCREEN_WIDTH_FMT/MSG_SCREEN_HEIGHT_FMT by update_size_titles().
#define MSG_SCREEN_WIDTH_FMT   "Width:  %3u"
#define MSG_SCREEN_HEIGHT_FMT  "Height: %3u"
#define MSG_SCREEN_CLEAR       "Clear"
#define MSG_SCREEN_FILL        "Fill"

// ── Pulldown 1 -- File ──────────────────────────────────────────────────────
#define MSG_FILE_SAVE_SCREEN    "Save screen"
#define MSG_FILE_LOAD_SCREEN    "Load screen"
#define MSG_FILE_SAVE_PROJECT   "Save project"
#define MSG_FILE_LOAD_PROJECT   "Load project"
#define MSG_FILE_SAVE_COMBINED  "Save combined"
#define MSG_FILE_LOAD_COMBINED  "Load combined"

// ── File I/O popup (src/fileio.c) ───────────────────────────────────────────
#define MSG_FILE_PROMPT_FILENAME  "Enter filename:"
#define MSG_FILE_INVALID_FORMAT   "Invalid or incompatible file."
#define MSG_FILE_NOT_FOUND        "File not found."
#define MSG_FILE_NO_FILES         "No matching files found."
#define MSG_FILE_PATH_TOO_LONG    "Path too long."

// ── Pulldown 2 -- Charset ───────────────────────────────────────────────────
#define MSG_CHARSET_LOAD_STD       "Load standard"
#define MSG_CHARSET_LOAD_ALT       "Load alternate"
#define MSG_CHARSET_SAVE_STD       "Save standard"
#define MSG_CHARSET_SAVE_ALT       "Save alternate"
#define MSG_CHARSET_LOAD_COMBINED  "Load combined"
#define MSG_CHARSET_SAVE_COMBINED  "Save combined"

// ── Pulldown 3 -- Information ───────────────────────────────────────────────
#define MSG_INFO_VERSION  "Version/credits"
#define MSG_INFO_EXIT     "Exit program"

// ── Pulldown 4 -- Yes/No ─────────────────────────────────────────────────────
#define MSG_MENU_YN_YES  "Yes"
#define MSG_MENU_YN_NO   "No"

// ── Menu popup helpers (src/menu.c) ─────────────────────────────────────────
#define MSG_MENU_AREYOUSURE  "Are you sure?"
#define MSG_MENU_PRESSAKEY   "Press a key to continue"
#define MSG_MENU_NOTIMPL     "Not yet implemented"
#define MSG_MENU_SHRINKWARN  "Shrinking might delete data."

// ── Select mode cut/copy (src/select.c) ─────────────────────────────────────
#define MSG_SELECT_NOFIT  "Selection does not fit."

// ── Resize dialog (src/menudata.c, resize_dialog()) ──────────────────────────
#define MSG_RESIZE_TITLE_WIDTH    "Resize canvas width"
#define MSG_RESIZE_TITLE_HEIGHT   "Resize canvas height"
#define MSG_RESIZE_PROMPT_WIDTH   "Enter new width:"
#define MSG_RESIZE_PROMPT_HEIGHT  "Enter new height:"
#define MSG_RESIZE_INVALID        "New size unsupported."

// ── Character editor (src/charsetedit.c) ──────────────────────────────────────
#define MSG_CE_CODE_FMT   "Code:$%02x"
#define MSG_CE_SET_STD    "Set:Std"
#define MSG_CE_SET_ALT    "Set:Alt"
#define MSG_CE_HEX_LABEL  "Hex:$"

// ── Palette popup (src/palette.c) ───────────────────────────────────────────
// Fixed 4-char labels (PAL_LABEL_X..PAL_LABEL_X+3) -- must stay 4 chars in
// both languages to preserve the 36-col grid layout.
#define MSG_PALETTE_FAV  "Fav:"
#define MSG_PALETTE_STD  "Std:"
#define MSG_PALETTE_ALT  "Alt:"

// ── Colour picker popup (src/colourpicker.c) ────────────────────────────────
// MSG_COLOURPICKER_INK_FMT/PAPER_FMT pad to 9 chars so the swatch (drawn
// separately at CP_SWATCH_X=9) lands in the same column for both lines.
// MSG_COLOURPICKER_RESULT is kept identical in EN/FR (7 chars, "Result:" is
// already an unaccented French cognate) -- a literal translation
// ("Resultat:", 9 chars) would collide with the fixed swatch columns at
// x=7..12, same precedent as MSG_PALETTE_FAV/STD/ALT above.
#define MSG_COLOURPICKER_TITLE     "Select ink and paper colour"
#define MSG_COLOURPICKER_INK_FMT   "Ink:    %u"
#define MSG_COLOURPICKER_PAPER_FMT "Paper:  %u"
#define MSG_COLOURPICKER_RESULT    "Result:"

// ── Status bar (src/statusbar.c) ────────────────────────────────────────────
// 36-col layout: mode name (cols 0-9, written separately and blank-padded --
// _cwin_vformat's %s has no width/left-pad support, see modes.h/mode_name()),
// "XY"+cursor x,y, "C"+plotscreencode hex (+glyph at col 20), "S"+screencode
// under cursor, "I"+plotink (+swatch), "P"+plotpaper (+swatch), then
// altchar/double/blink flag chars.
#define MSG_STATUSBAR_MAIN_FMT  "XY%2u,%2uC%02x  S%02xI%u  P%u  %c%c%c"

// ── Mode names (src/editor.c mode_name(), statusbar Mode field) ────────────
#define MSG_MODE_MAIN     "Main"
#define MSG_MODE_WRITE    "Write"
#define MSG_MODE_SELECT   "Select"
#define MSG_MODE_MOVE     "Move"
#define MSG_MODE_LINEBOX  "Line/Box"

#endif // STRINGS_EN_H
