#!/usr/bin/env python3
# tools/gen_help_screens.py
#
# Generates all 8 help-screen binary files (1120 bytes each, raw 40x28 Oric
# screen RAM dumps) loaded at runtime by help_show() in src/help.c:
#   assets/OSEHS{1-4}.BIN  -- English help screens
#   assets/OSEHF{1-4}.BIN  -- French  help screens (LANG=FR build)
#
# Screen format: 40 columns x 28 rows, each byte is a serial-attribute
# (0x00-0x1F) or a ROM character code (0x20-0x7F). The Oric ULA resets
# ink to white and paper to black at the start of each raster row and
# then applies attribute bytes left-to-right through the row.
#
# Row types used here:
#   TITLE    row 0:  [0x00 A_FWBLACK, 0x12 A_BGGREEN] + 38-char text
#   HEADER   rows:   [0x02 A_FWGREEN, 0x10 A_BGBLACK] + 38-char text
#   BLANK_G  rows:   [0x02 A_FWGREEN, 0x10 A_BGBLACK] + 38 spaces
#   BLANK_C  rows:   [0x06 A_FWCYAN,  0x10 A_BGBLACK] + 38 spaces
#   CONTENT  rows:   [0x06 A_FWCYAN,  0x10 A_BGBLACK] +
#                    key(11 chars) + ':' + [0x03 A_FWYELLOW] + desc(25)
#   BLANK_W  rows:   [0x06 A_FWCYAN,  0x07 A_FWWHITE] +
#                    12 spaces + [0x03] + 25 spaces
#                    (separator above the PRESS row, matches V1 style)
#   CONTENT_W rows:  [0x06 A_FWCYAN,  0x07 A_FWWHITE] +
#                    key(11) + ':' + [0x03] + desc(25)
#                    (bottom-section content rows, same visual as regular)
#   PRESS    row:    [0x06 A_FWCYAN,  0x07 A_FWWHITE] + 38-char text
#   LAST     row:    40 x 0x20 (all spaces -- trailing padding)
#
# Changes vs. original V1 help screens:
#   OSEHS2 / OSEHF2 -- added "A : Toggle Std / Alt charset" (was missing
#                       from the char-edit section); removed the blank row
#                       that previously separated char-edit from palette so
#                       the screen stays at 28 rows. Fixed typo "visaual"
#                       -> "visual" in the palette section.
#
# Not part of the regular build -- run by hand when help content changes:
#   python3 tools/gen_help_screens.py
#
# Output files are checked into assets/ and distributed via `make usb`.

import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets")
COLS = 40
ROWS = 28

# Oric serial attribute values (include/oric.h)
A_FWBLACK  = 0x00
A_FWGREEN  = 0x02
A_FWYELLOW = 0x03
A_FWCYAN   = 0x06
A_FWWHITE  = 0x07
A_BGBLACK  = 0x10
A_BGGREEN  = 0x12

CH_SPACE = 0x20


# ---- Row builders ----------------------------------------------------------

def _enc(text, length):
    s = text.ljust(length)[:length]
    return bytes(ord(c) for c in s)


def row_title(text):
    return bytes([A_FWBLACK, A_BGGREEN]) + _enc(text, 38)


def row_header(text):
    return bytes([A_FWGREEN, A_BGBLACK]) + _enc(text, 38)


def row_blank_g():
    return bytes([A_FWGREEN, A_BGBLACK]) + bytes([CH_SPACE] * 38)


def row_blank_c():
    return bytes([A_FWCYAN, A_BGBLACK]) + bytes([CH_SPACE] * 38)


def row_content(key, desc, white=False):
    """
    key:  key-field text, max 11 chars (colon appended automatically)
    desc: description text, max 25 chars
    white: if True use A_FWWHITE as second byte (for bottom-section rows)
    """
    assert len(key)  <= 11, f"key too long ({len(key)}): {key!r}"
    assert len(desc) <= 25, f"desc too long ({len(desc)}): {desc!r}"
    b1 = A_FWWHITE if white else A_BGBLACK
    return (bytes([A_FWCYAN, b1])
            + _enc(key, 11) + bytes([ord(':')])
            + bytes([A_FWYELLOW])
            + _enc(desc, 25))


def row_blank_w():
    """Blank separator row before the PRESS row (V1 style, white ink)."""
    return (bytes([A_FWCYAN, A_FWWHITE])
            + bytes([CH_SPACE] * 12)
            + bytes([A_FWYELLOW])
            + bytes([CH_SPACE] * 25))


def row_press(text):
    return bytes([A_FWCYAN, A_FWWHITE]) + _enc(text, 38)


def row_last():
    return bytes([CH_SPACE] * COLS)


def build_screen(rows):
    assert len(rows) == ROWS, f"expected {ROWS} rows, got {len(rows)}"
    data = b"".join(rows)
    assert len(data) == ROWS * COLS
    return data


# ---- Screen content --------------------------------------------------------
# EN: exactly reproduces the original V1 text where unchanged.
# FR: key names stay in English (they are keyboard labels); only the
#     descriptions are translated.  No accented characters -- the Oric ROM
#     charset has none (same constraint as src/strings_fr.h).


# Screen 1 -- Main mode
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

def screen_main(fr=False):
    title_text = "Oric Screen Editor: Aide" if fr else "Oric Screen Editor: Help"
    press_text = "Appuyer sur une touche." if fr else "Press a key to continue."

    if fr:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Mode principal"),
            row_blank_c(),
            row_content("Cursor keys", "Deplacer curseur"),
            row_content("-     / =  ", "Car. prec. / suivant"),
            row_content(",     / .  ", "Encre prec. / suivante"),
            row_content(";     / '  ", "Papier prec. / suivant"),
            row_content("DEL   / T  ", "Effacer / Essayer"),
            row_content("SPACE / G  ", "Tracer / Saisir car."),
            row_content("B     / A  ", "Bascule clignt / alt jeu"),
            row_content("R     / D  ", "Bascule inverse / double"),
            row_content("I     / O  ", "Tracer encre / papier"),
            row_content("U          ", "Modif. attribut"),
            row_content("J     / H  ", "Coords / Accueil"),
            row_content("F          ", "Chercher/Remplacer"),
            row_content("Z     / Y  ", "Annuler / Refaire"),
            row_content("E     / W  ", "Editeur car. / Ecriture"),
            row_content("L     / S  ", "Ligne-boite / Selection"),
            row_content("M     / P  ", "Deplacement / Palette"),
            row_content("C          ", "Choix de couleur"),
            row_content("0-9/SHFT0-9", "Sel. / stocke fav. car."),
            row_content("FUNCT+1    ", "Menu principal"),
            row_content("FUNCT+6    ", "Masquer barre etat"),
            row_content("FUNCT+8    ", "Cette aide"),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    else:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Main mode"),
            row_blank_c(),
            row_content("Cursor keys", "Move cursor"),
            row_content("-     / =  ", "Previous / next character"),
            row_content(",     / .  ", "Previous / next ink"),
            row_content(";     / '  ", "Previous / next paper"),
            row_content("DEL   / T  ", "Delete / Try character"),
            row_content("SPACE / G  ", "Plot / Grab character"),
            row_content("B     / A  ", "Toggle blink / alt charst"),
            row_content("R     / D  ", "Toggle reverse / double"),
            row_content("I     / O  ", "Plot ink / paper"),
            row_content("U          ", "Plot character modifier"),
            row_content("J     / H  ", "Goto coords / Home"),
            row_content("F          ", "Find/Replace"),
            row_content("Z     / Y  ", "Undo / Redo"),
            row_content("E     / W  ", "Character edit / Write"),
            row_content("L     / S  ", "Line and box / Select"),
            row_content("M     / P  ", "Move / Palette mode"),
            row_content("C          ", "Colour picker mode"),
            row_content("0-9/SHFT0-9", "Select / store fav char"),
            row_content("FUNCT+1    ", "Go to main menu"),
            row_content("FUNCT+6    ", "Toggle status bar"),
            row_content("FUNCT+8    ", "Show this help"),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    return build_screen(rows)


# Screen 2 -- Character edit / Palette / Colour picker
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Change vs original V1 help screens:
#   - Added "A : Toggle Std / Alt charset" after "S : Restore from system font"
#     (new in OSE-LOCI, was missing from help screen).
#   - Removed the blank row that previously separated char-edit from palette
#     (reclaimed to fit the new A entry at exactly 28 rows).
#   - Fixed typo "visaual" -> "visual" in the Palette section.

def screen_charedit(fr=False):
    title_text = "Oric Screen Editor: Aide" if fr else "Oric Screen Editor: Help"
    press_text = "Appuyer sur une touche." if fr else "Press a key to continue."

    if fr:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Mode editeur car."),
            row_content("-     / =  ", "Car. prec. / suivant"),
            row_content("DEL   / Z  ", "Effacer car. / Annuler"),
            row_content("SPACE      ", "Basculer pixel"),
            row_content("S          ", "Restaur. depuis ROM"),
            row_content("A          ", "Basculer Std / Alt"),      # new
            row_content("I          ", "Inverser car."),
            row_content("C     / V  ", "Copier / Coller"),
            row_content("X     / Y  ", "Miroir en X / Y"),
            row_content("L/R/U/D    ", "Scroll G/D/H/B"),
            row_content("H          ", "Editer valeur hex"),
            row_header("Mode palette:"),
            row_content("SPACE/ENTER", "Choisir caractere"),
            row_content("V          ", "Basculer carte visuelle"),
            row_blank_g(),
            row_header("Les 2 modes:"),
            row_content("Cursor keys", "Deplacer curseur"),
            row_content("0-9        ", "Selectionner car. fav."),
            row_content("ESC        ", "Quitter mode"),
            row_content("SHIFT+0-9  ", "Stocker car. fav 0-9"),
            row_content("FUNCT+1    ", "Menu principal"),
            row_content("FUNCT+6    ", "Masquer barre etat"),
            row_content("FUNCT+8    ", "Cette aide"),
            row_header("Mode choix couleur:"),
            row_content("Cursor keys", "Choix encre/papier,SPC:OK", white=True),
            row_press(press_text),
        ]
    else:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Character edit mode"),
            row_content("-     / =  ", "Previous / next character"),
            row_content("DEL   / Z  ", "Delete character / Undo"),
            row_content("SPACE      ", "Toggle character bit"),
            row_content("S          ", "Restore from system font"),
            row_content("A          ", "Toggle Std / Alt charset"),  # new
            row_content("I          ", "Inverse character"),
            row_content("C     / V  ", "Copy / Paste"),
            row_content("X     / Y  ", "Mirror in X / Y axis"),
            row_content("L/R/U/D    ", "Scroll Left/Right/Up/Down"),
            row_content("H          ", "Edit hex value"),
            row_header("Palette mode:"),
            row_content("SPACE/ENTER", "Select character"),
            row_content("V          ", "Toggle visual char map"),   # typo fixed
            row_blank_g(),
            row_header("Both modes:"),
            row_content("Cursor keys", "Move cursor"),
            row_content("0-9        ", "Select favourite char"),
            row_content("ESC        ", "Leave mode"),
            row_content("SHIFT+0-9  ", "Store char in fav 0-9"),
            row_content("FUNCT+1    ", "Go to main menu"),
            row_content("FUNCT+6    ", "Toggle status bar"),
            row_content("FUNCT+8    ", "Show this help"),
            row_header("Colour picker mode:"),
            row_content("Cursor keys", "Cycle ink/paper, SPACE:OK", white=True),
            row_press(press_text),
        ]
    return build_screen(rows)


# Screen 3 -- Line/Box, Select, Move
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

def screen_linebox(fr=False):
    title_text = "Oric Screen Editor: Aide" if fr else "Oric Screen Editor: Help"
    press_text = "Appuyer sur une touche." if fr else "Press a key to continue."

    if fr:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Mode ligne et boite:"),
            row_content("Cursor keys", "Agrandir / Retrecir"),
            row_content("RETURN     ", "Accepter"),
            row_content("O     / C  ", "Creux / Ellipse"),
            row_header("Mode selection:"),
            row_content("Cursor keys", "Agrandir / Retrecir"),
            row_content("RETURN     ", "Accepter"),
            row_content("X     / C  ", "Couper / Copier"),
            row_content("I     / P  ", "Peindre encre / papier"),
            row_content("D     / M  ", "Suppr. / Peindre modif."),
            row_blank_c(),
            row_header("Mode deplacement:"),
            row_content("Cursor keys", "Deplacer le contenu"),
            row_content("RETURN     ", "Accepter"),
            row_blank_c(),
            row_header("Les 2 modes:"),
            row_content("ESC        ", "Quitter mode"),
            row_content("FUNCT+6    ", "Masquer barre etat"),
            row_content("FUNCT+8    ", "Cette aide"),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    else:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Line and box mode:"),
            row_content("Cursor keys", "Expand / Shrink"),
            row_content("RETURN     ", "Accept"),
            row_content("O     / C  ", "Toggle hollow box/ellipse"),
            row_header("Select mode:"),
            row_content("Cursor keys", "Expand / Shrink"),
            row_content("RETURN     ", "Accept"),
            row_content("X     / C  ", "Cut / Copy"),
            row_content("I     / P  ", "Paint ink / Paint paper"),
            row_content("D     / M  ", "Delete / Paint char mod"),
            row_blank_c(),
            row_header("Move mode:"),
            row_content("Cursor keys", "Move in direction"),
            row_content("RETURN     ", "Accept"),
            row_blank_c(),
            row_header("Both modes:"),
            row_content("ESC        ", "Leave mode"),
            row_content("FUNCT+6    ", "Toggle status bar"),
            row_content("FUNCT+8    ", "Show this help"),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    return build_screen(rows)


# Screen 4 -- Write mode
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

def screen_write(fr=False):
    title_text = "Oric Screen Editor: Aide" if fr else "Oric Screen Editor: Help"
    press_text = "Appuyer sur une touche." if fr else "Press a key to continue."

    if fr:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Mode ecriture:"),
            row_content("Cursor keys", "Deplacer curseur"),
            row_content("DEL        ", "Effacer (retour arriere)"),
            row_content("CTRL+B     ", "Bascule clignotement"),
            row_content("CTRL+A     ", "Bascule alt jeu car."),
            row_content("CTRL+D     ", "Bascule double"),
            row_content("CTRL+Z/X   ", "Dec. / incr. encre"),
            row_content("CTRL+C/V   ", "Dec. / incr. papier"),
            row_content("CTRL+R     ", "Bascule inverse"),
            row_content("FUNCT+1    ", "Tracer encre presente"),
            row_content("FUNCT+2    ", "Tracer papier present"),
            row_content("FUNCT+3    ", "Tracer modif. car."),
            row_content("FUNCT+4    ", "Attr. hex. enc/pap/mod"),
            row_content("FUNCT+6    ", "Masquer barre etat"),
            row_content("FUNCT+8    ", "Cette aide"),
            row_content("ESC        ", "Quitter mode"),
            row_content("Other keys ", "Tracer caractere"),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    else:
        rows = [
            row_title(title_text),
            row_blank_g(),
            row_header("Write mode:"),
            row_content("Cursor keys", "Move cursor"),
            row_content("DEL        ", "Delete character"),
            row_content("CTRL+B     ", "Toggle blink"),
            row_content("CTRL+A     ", "Toggle alt charset"),
            row_content("CTRL+D     ", "Toggle double"),
            row_content("CTRL+Z/X   ", "Decrease/increase ink"),
            row_content("CTRL+C/V   ", "Decrease/increase paper"),
            row_content("CTRL+R     ", "Toggle reverse"),
            row_content("FUNCT+1    ", "Plot present ink"),
            row_content("FUNCT+2    ", "Plot present paper"),
            row_content("FUNCT+3    ", "Plot present char modify"),
            row_content("FUNCT+4    ", "Hex-direct ink/paper/mod"),
            row_content("FUNCT+6    ", "Toggle status bar"),
            row_content("FUNCT+8    ", "Show this help"),
            row_content("ESC        ", "Leave this mode"),
            row_content("Other keys ", "Plot character"),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_c(),
            row_blank_w(),
            row_press(press_text),
            row_last(),
        ]
    return build_screen(rows)


# ---- Main ------------------------------------------------------------------

def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    screens = [
        ("OSEHS1.BIN", screen_main(fr=False)),
        ("OSEHS2.BIN", screen_charedit(fr=False)),
        ("OSEHS3.BIN", screen_linebox(fr=False)),
        ("OSEHS4.BIN", screen_write(fr=False)),
        ("OSEHF1.BIN", screen_main(fr=True)),
        ("OSEHF2.BIN", screen_charedit(fr=True)),
        ("OSEHF3.BIN", screen_linebox(fr=True)),
        ("OSEHF4.BIN", screen_write(fr=True)),
    ]

    for filename, data in screens:
        path = os.path.join(OUT_DIR, filename)
        with open(path, "wb") as f:
            f.write(data)
        print(f"Wrote {filename}  ({len(data)} bytes)")


if __name__ == "__main__":
    main()
