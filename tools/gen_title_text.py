#!/usr/bin/env python3
# tools/gen_title_text.py
#
# Builds the French mosaic title art (rows 0-20) for OSETSF.BIN.
# Called from patch_title_screen.py; can also be run standalone for inspection.
#
# Oric Alt-charset mosaic encoding: glyph code N (0x20-0x7F),
# N-0x20 = 6-bit value: bit0=TL, bit1=TR, bit2=ML, bit3=MR, bit4=BL, bit5=BR.
# Each big letter = 3 glyph-columns x 3 glyph-rows (6px wide x 9px tall).
# Letters separated by 1-column space (0x20 = empty mosaic cell).
#
# French title layout (rows 0-20):
#   rows  0-1:  top border  (fill 0x53/0x2c, unchanged from EN)
#   rows  2-4:  ORIC        (unchanged)
#   row   5:    separator
#   rows  6-8:  EDITEUR     (replaces SCREEN)
#   row   9:    separator
#   rows 10-12: ECRANS      (replaces EDITOR)
#   row  13:    separator
#   rows 14-16: POUR        (replaces FOR)
#   row  17:    separator
#   rows 18-20: LOCI        (unchanged)
#
# Glyph data extracted from OSETSC.BIN hex dump (2026-06-27).
# Color scheme per word block cloned from EN source rows.

COLS = 40

# Each letter: tuple of (top_glyphs, mid_glyphs, bot_glyphs), each 3 bytes.
LETTERS = {
    # Extracted from ORIC (EN rows 2-4)
    'O': (b'\x36\x23\x49', b'\x35\x20\x4a', b'\x45\x50\x3a'),
    'R': (b'\x37\x23\x49', b'\x55\x50\x3a', b'\x35\x22\x44'),
    'I': (b'\x23\x4b\x23', b'\x20\x4a\x20', b'\x50\x5a\x50'),
    'C': (b'\x36\x23\x23', b'\x35\x20\x20', b'\x45\x50\x50'),
    # Extracted from SCREEN (EN rows 6-8)
    'S': (b'\x36\x23\x23', b'\x45\x50\x30', b'\x50\x50\x3a'),
    'E': (b'\x36\x23\x23', b'\x55\x50\x20', b'\x45\x50\x50'),
    'N': (b'\x55\x20\x4a', b'\x35\x45\x4a', b'\x35\x20\x4b'),
    # Extracted from EDITOR (EN rows 10-12)
    'D': (b'\x37\x23\x49', b'\x35\x20\x4a', b'\x55\x50\x3a'),
    'T': (b'\x23\x4b\x23', b'\x20\x4a\x20', b'\x20\x4a\x20'),
    # Extracted from FOR (EN rows 14-16)
    'F': (b'\x37\x23\x23', b'\x55\x50\x20', b'\x35\x20\x20'),
    # Extracted from LOCI (EN rows 18-20)
    'L': (b'\x35\x20\x20', b'\x35\x20\x20', b'\x55\x50\x50'),
    # Hand-designed: U (O sides + O bottom arc)
    'U': (b'\x35\x20\x4a', b'\x35\x20\x4a', b'\x45\x50\x3a'),
    # Hand-designed: A (O top arc + crossbar at base of mid section + open sides at bottom)
    'A': (b'\x36\x23\x49', b'\x55\x50\x5a', b'\x35\x20\x4a'),
    # Hand-designed: P (R top bowl + bowl-close mid + left leg only at bottom)
    'P': (b'\x37\x23\x49', b'\x55\x50\x3a', b'\x35\x20\x20'),
}


def _find_trail_color(row_bytes):
    """Scan backward to find the last attribute byte (0x00-0x1F) in the row."""
    for i in range(COLS - 1, 1, -1):
        if row_bytes[i] < 0x20:
            return row_bytes[i]
    return 0x07  # fallback: white ink


def _build_row(word, fill_char, row_idx, lead, first_color, rest_color, trail):
    """
    Build one 40-byte title-art row matching the EN payload scheme:
      [first_color][L0] [rest_color][L1] [sp][L2] [sp]...[sp][LN-1]

    When first_color == rest_color, rest_color acts as an explicit sep between
    L0 and L1 (same as a 0x20 sep). When they differ, rest_color is the color
    change attr that also serves as the single-column gap between L0 and L1.
    All subsequent inter-letter gaps are plain 0x20.

    word:        list of letter characters
    fill_char:   0x53 (top/bot glyph rows) or 0x2c (mid glyph row)
    row_idx:     0=top, 1=mid, 2=bot
    lead:        color attr byte after A_ALT (fills + overall context colour)
    first_color: color attr before letter 0
    rest_color:  color attr between letter 0 and letter 1 (= colour for L1..LN-1)
    trail:       color attr byte after the last letter glyph
    """
    payload = bytearray()
    payload.append(first_color)
    payload += LETTERS[word[0]][row_idx]
    for i in range(1, len(word)):
        if i == 1 and rest_color != first_color:
            payload.append(rest_color)   # colour change doubles as the 1-col gap
        else:
            payload.append(0x20)         # explicit 1-col gap (inherited colour)
        payload += LETTERS[word[i]][row_idx]

    inner = COLS - 2 - 1 - len(payload)   # 2=A_ALT+lead, 1=trail
    fill_lead  = (inner + 1) // 2
    fill_trail = inner - fill_lead

    row = bytes([0x09, lead])
    row += bytes([fill_char] * fill_lead)
    row += bytes(payload)
    row += bytes([trail])
    row += bytes([fill_char] * fill_trail)
    assert len(row) == COLS, f"row length {len(row)} for {''.join(word)}"
    return row


def _word_rows(word, first_color, rest_color, src_bin, top_src, mid_src, bot_src):
    """
    Build 3 rows (top, mid, bot) for a word, cloning lead/trail from EN source rows.

    first_color: colour attr for letter 0
    rest_color:  colour attr between letter 0 and letter 1 (doubles as 1-col gap)
    """
    def src_row(n):
        return src_bin[n*COLS:(n+1)*COLS]

    rows = b''
    for fill_char, src_idx, row_idx in [(0x53, top_src, 0),
                                         (0x2c, mid_src, 1),
                                         (0x53, bot_src, 2)]:
        r = src_row(src_idx)
        rows += _build_row(word, fill_char, row_idx,
                           r[1],
                           first_color, rest_color,
                           _find_trail_color(r))
    return rows


def build_fr_title_art(src_bin):
    """
    Build 840 bytes (rows 0-20) of the French title screen.
    Rows not containing changed text are copied verbatim from src_bin.

    src_bin: bytes, must be 40*28=1120 bytes (full EN title screen).
    Returns: bytes, 840 bytes (rows 0-20).
    """
    assert len(src_bin) == COLS * 28

    def row(n):
        return src_bin[n*COLS:(n+1)*COLS]

    out = bytearray()

    # rows 0-4: top border + ORIC (unchanged)
    out += src_bin[0 : 5*COLS]

    # row 5: separator (unchanged)
    out += row(5)

    # rows 6-8: EDITEUR (replaces SCREEN)
    # EN colour scheme: first letter red (0x01), rest_color white (0x07) acts as gap
    out += _word_rows(list('EDITEUR'), 0x01, 0x07, src_bin, 6, 7, 8)

    # row 9: separator (unchanged)
    out += row(9)

    # rows 10-12: ECRANS (replaces EDITOR)
    out += _word_rows(list('ECRANS'), 0x01, 0x07, src_bin, 10, 11, 12)

    # row 13: separator (unchanged)
    out += row(13)

    # rows 14-16: POUR (replaces FOR)
    # EN colour scheme: single colour (0x07), plain 0x20 sep between all letters
    out += _word_rows(list('POUR'), 0x07, 0x07, src_bin, 14, 15, 16)

    # row 17: separator (unchanged)
    out += row(17)

    # rows 18-20: LOCI (unchanged)
    out += src_bin[18*COLS : 21*COLS]

    assert len(out) == 21 * COLS
    return bytes(out)


if __name__ == "__main__":
    import os, sys
    assets = os.path.join(os.path.dirname(__file__), "..", "assets")
    en_path = os.path.join(assets, "OSETSC.BIN")
    if not os.path.isfile(en_path):
        sys.exit(f"ERROR: {en_path} not found")
    with open(en_path, "rb") as f:
        src = f.read()
    art = build_fr_title_art(src)
    print(f"Generated {len(art)} bytes of FR title art (rows 0-20).")
    for name, start, end in [
        ("rows 6-8  (EDITEUR)", 6, 9),
        ("rows 10-12 (ECRANS)", 10, 13),
        ("rows 14-16 (POUR)",  14, 17),
    ]:
        for r in range(start, end):
            print(f"  row{r:2d}: {art[r*40:(r+1)*40].hex()}")
