// strings_fr.h - OricScreenEditorLOCI user-visible strings (French)
// All MSG_* macros for the application, in French. No accented characters --
// the Oric ROM charset has no e-acute, e-grave, c-cedilla etc. Selected via
// strings.h when LANG_FR is defined (make LANG=FR).
//
// IMPORTANT: key bindings are unchanged from the English build -- only the
// displayed labels below are translated.

#ifndef STRINGS_FR_H
#define STRINGS_FR_H

// ── LOCI library messages (include/loci.c, get_locicfg()) ────────────────
// Not yet wired into the build (see ARCHITECTURE.md "LOCI and IJK"), kept
// for when loci.c is included.
#define MSG_LOCI_NOT_FOUND  "Aucun LOCI detecte ou micrologiciel trop ancien."
#define MSG_PRESS_KEY_EXIT  "Appuyer sur une touche pour quitter."

// ── Splash screen (src/main.c) ────────────────────────────────────────────
#define MSG_SPLASH_TITLE      "OricScreenEditor pour LOCI"
#define MSG_SPLASH_BUILD_FMT  "Compilation Oscar64 v%d.%d.%d"
#define MSG_SPLASH_PRESSKEY   "Appuyer sur une touche pour demarrer"

// ── Menu bar titles (src/menudata.c, menubar.titles) ──────────────────────
// menu_placebar() lays out items dynamically with 1-col gaps and requires
// sum(strlen) + 7 <= 39 -- "Caracteres" (10) would push the EN-fitting total
// (33) one over that limit and collide with "Information". Abbreviated to
// "Caract." (7) to bring the total to 30, same layout shape as EN.
#define MSG_MENU_BAR_SCREEN   "Ecran"
#define MSG_MENU_BAR_FILE     "Fichier"
#define MSG_MENU_BAR_CHARSET  "Caract."
#define MSG_MENU_BAR_INFO     "Information"

// ── Pulldown 0 -- Screen / Ecran ───────────────────────────────────────────
// pulldown_titles[0][0]/[0][1] are rewritten at runtime from
// MSG_SCREEN_WIDTH_FMT/MSG_SCREEN_HEIGHT_FMT by update_size_titles().
#define MSG_SCREEN_WIDTH_FMT   "Largeur:%3u"
#define MSG_SCREEN_HEIGHT_FMT  "Hauteur:%3u"
#define MSG_SCREEN_CLEAR       "Effacer"
#define MSG_SCREEN_FILL        "Remplir"

// ── Pulldown 1 -- File / Fichier ─────────────────────────────────────────────
#define MSG_FILE_SAVE_SCREEN    "Sauver ecran"
#define MSG_FILE_LOAD_SCREEN    "Charger ecran"
#define MSG_FILE_SAVE_PROJECT   "Sauver projet"
#define MSG_FILE_LOAD_PROJECT   "Charger projet"
#define MSG_FILE_SAVE_COMBINED  "Sauver combine"
#define MSG_FILE_LOAD_COMBINED  "Charger combine"

// ── File I/O popup (src/fileio.c) ───────────────────────────────────────────
#define MSG_FILE_PROMPT_FILENAME  "Entrer nom de fichier:"
#define MSG_FILE_INVALID_FORMAT   "Fichier invalide ou incompatible."
#define MSG_FILE_NOT_FOUND        "Fichier non trouve."
#define MSG_FILE_NO_FILES         "Aucun fichier correspondant trouve."
#define MSG_FILE_PATH_TOO_LONG    "Chemin trop long."

// ── Pulldown 2 -- Charset / Caracteres ───────────────────────────────────────
#define MSG_CHARSET_LOAD_STD       "Charger standard"
#define MSG_CHARSET_LOAD_ALT       "Charger alterne"
#define MSG_CHARSET_SAVE_STD       "Sauver standard"
#define MSG_CHARSET_SAVE_ALT       "Sauver alterne"
#define MSG_CHARSET_LOAD_COMBINED  "Charger combine"
#define MSG_CHARSET_SAVE_COMBINED  "Sauver combine"

// ── Pulldown 3 -- Information ───────────────────────────────────────────────
#define MSG_INFO_VERSION  "Version/credits"
#define MSG_INFO_EXIT     "Fin du programme"

// ── Pulldown 4 -- Yes/No / Oui/Non ───────────────────────────────────────────
#define MSG_MENU_YN_YES  "Oui"
#define MSG_MENU_YN_NO   "Non"

// ── Menu popup helpers (src/menu.c) ─────────────────────────────────────────
#define MSG_MENU_AREYOUSURE  "Etes vous sur?"
#define MSG_MENU_PRESSAKEY   "Appuyer touche pour continuer"
#define MSG_MENU_NOTIMPL     "Pas encore implemente"
#define MSG_MENU_SHRINKWARN  "Reduire peut effacer donnees."

// ── Select mode cut/copy (src/select.c) ─────────────────────────────────────
#define MSG_SELECT_NOFIT  "Selection hors limites."

// ── Resize dialog (src/menudata.c, resize_dialog()) ──────────────────────────
#define MSG_RESIZE_TITLE_WIDTH    "Redimensionner largeur"
#define MSG_RESIZE_TITLE_HEIGHT   "Redimensionner hauteur"
#define MSG_RESIZE_PROMPT_WIDTH   "Entrer nouvelle largeur:"
#define MSG_RESIZE_PROMPT_HEIGHT  "Entrer nouvelle hauteur:"
#define MSG_RESIZE_INVALID        "Nouvelle taille invalide."

// ── Character editor (src/charsetedit.c) ──────────────────────────────────────
// "Jeu" (set) keeps Std/Alt identical to the English build -- only the
// "Code"/"Hex" labels below are already unaccented French/English cognates.
#define MSG_CE_CODE_FMT   "Code:$%02x"
#define MSG_CE_SET_STD    "Jeu:Std"
#define MSG_CE_SET_ALT    "Jeu:Alt"
#define MSG_CE_HEX_LABEL  "Hex:$"

// ── Palette popup (src/palette.c) ───────────────────────────────────────────
// Same abbreviations as English -- "Fav:"/"Std:"/"Alt:" are short enough and
// already unaccented; must stay 4 chars to preserve the 36-col grid layout.
#define MSG_PALETTE_FAV  "Fav:"
#define MSG_PALETTE_STD  "Std:"
#define MSG_PALETTE_ALT  "Alt:"

// ── Colour picker popup (src/colourpicker.c) ────────────────────────────────
// MSG_COLOURPICKER_INK_FMT/PAPER_FMT ("Encre:"/"Papier:") pad to 9 chars,
// same swatch-column convention as strings_en.h. MSG_COLOURPICKER_RESULT
// kept as "Result:" (7 chars) -- same precedent as MSG_PALETTE_FAV/STD/ALT:
// a literal "Resultat:" (9 chars) would collide with the fixed swatch
// columns at x=7..12.
#define MSG_COLOURPICKER_TITLE     "Choisir encre et papier"
#define MSG_COLOURPICKER_INK_FMT   "Encre:  %u"
#define MSG_COLOURPICKER_PAPER_FMT "Papier: %u"
#define MSG_COLOURPICKER_RESULT    "Result:"

// ── Status bar (src/statusbar.c) ────────────────────────────────────────────
// Mode name (cols 0-9) is written separately and blank-padded -- see
// strings_en.h for the full column breakdown.
#define MSG_STATUSBAR_MAIN_FMT  "XY%2u,%2uC%02x  S%02xI%u  P%u  %c%c%c"

// ── Mode names (src/editor.c mode_name(), statusbar Mode field) ────────────
// All <=10 chars (the field width), no accented characters (see CLAUDE.md
// "Localisation").
#define MSG_MODE_MAIN     "Princ."
#define MSG_MODE_WRITE    "Ecrire"
#define MSG_MODE_SELECT   "Select"
#define MSG_MODE_MOVE     "Deplacer"
#define MSG_MODE_LINEBOX  "Ligne/Bte"

#endif // STRINGS_FR_H
