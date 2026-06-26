#!/usr/bin/env python3
# tools/gen_demo_screen.py
#
# One-off generator for assets/OSEDEMO.BIN -- a custom-charset showcase
# screen demonstrating what's possible on Oric hardware by hand-drawing
# 6x8 glyphs instead of relying on the stock ROM font: a pixel-art sun
# (a filled circle, computed from the circle equation and sliced into
# unique 6x8 tiles -- true curves are otherwise impossible on a 6x8
# monospace character grid) inside a hand-drawn box-frame border (the
# ROM ASCII font has no line-drawing characters at all), plus ink/paper
# serial-attribute colour swatch bars.
#
# Not part of the build -- run by hand whenever the demo needs
# regenerating, same convention as tools/gen_qr.js (see CLAUDE.md
# "Information menu"): the *output* (assets/OSEDEMO.BIN) is what's
# checked in and distributed via `make usb`, not this script's
# invocation.
#
# Output format: writes BOTH of OSE's multi-file formats from the same
# artwork -- File > Load Combined's bare layout (src/fileio.c
# fileio_save_combined()/fileio_load_combined()) as assets/OSEDEMO.BIN
# (CHARSET_GLYPH_AREA_SIZE=768 bytes of CHARSET_STD's displayable glyph
# range, codes 0x20-0x7F, immediately followed by a bare 40x28-row
# screenmap[] dump, no header -- see src/fileio.h), AND File > Load
# Project's 4-file scheme (fileio_save_project()/fileio_load_project())
# as assets/OSEDEMOPJ/SC/CS.BIN (added 2026-06-21, user-requested --
# OSEDEMO.BIN had no Project-format counterpart, unlike
# assets/OSELOGOPJ/SC/CS/CA.BIN). No OSEDEMOCA.BIN, since this demo
# never touches CHARSET_ALT at all (every glyph is std-only, see the
# comment below) -- matching fileio_load_project()'s own "a missing
# CA.BIN file leaves that bank untouched" convention exactly.
#
# Codes 0x20 (space), 0x30-0x39 (digits), 0x41-0x5A (A-Z), and '-' '.'
# '/' ':' are left as genuine ROM glyphs (copied from a real Atmos ROM
# image) so every on-screen text label stays legible; only the
# remaining ~54 "spare" codes (symbols + lowercase a-z, none of which
# appear in this screen's text) are repurposed for the border (6 tiles)
# and circle (computed at run time, 29 unique tiles for a 9x9-cell
# circle) glyphs.
#
# Usage: python3 tools/gen_demo_screen.py /path/to/basic11b.rom
#        (defaults to $PHOSDIR/roms/basic11b.rom, then
#        ~/oricutron/roms/basic11b.rom, if no argument is given)

import os
import sys

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets")
OUT_PATH = os.path.join(OUT_DIR, "OSEDEMO.BIN")
OUT_BASE = "OSEDEMO"

# ---- Oric serial attribute values (oric.h) ----
A_FWBLACK   = 0
A_FWRED     = 1
A_FWGREEN   = 2
A_FWYELLOW  = 3
A_FWBLUE    = 4
A_FWMAGENTA = 5
A_FWCYAN    = 6
A_FWWHITE   = 7
A_BGBLACK   = 16
CH_SPACE    = 0x20
CH_INVSPACE = 0xA0

# 3-letter abbreviations for ink/paper colour labels, index = Oric colour number
COLOUR_NAMES = ["BLK", "RED", "GRE", "YEL", "BLU", "MAG", "CYA", "WHI"]

COLS, ROWS = 40, 28


def find_rom(argv):
    if len(argv) > 1:
        return argv[1]
    phosdir = os.environ.get("PHOSDIR")
    candidates = []
    if phosdir:
        candidates.append(os.path.join(phosdir, "roms", "basic11b.rom"))
    candidates.append(os.path.expanduser("~/oricutron/roms/basic11b.rom"))
    for c in candidates:
        if os.path.isfile(c):
            return c
    sys.exit("basic11b.rom not found -- pass its path as argv[1]")


def load_rom_font(rom_path):
    # CHARSETROM ($FC78, oric.h) is the ROM's own copy of the displayable
    # glyph range, mapped at $C000-$FFFF in a 16KB Atmos ROM image.
    with open(rom_path, "rb") as f:
        rom = f.read()
    font = bytearray(rom[0xFC78 - 0xC000 : 0xFC78 - 0xC000 + 768])
    assert len(font) == 768
    return font


def glyph_off(code):
    return (code - 0x20) * 8


def set_glyph(font, code, rows8):
    off = glyph_off(code)
    for i, b in enumerate(rows8):
        font[off + i] = b


def build_border_tiles(font):
    """Single-pixel box-frame tiles: a vertical line at column 2 of each
    6x8 cell met by a horizontal line at row 3, forming right-angle
    corners. Returns the 6 assigned codes (TL, TR, BL, BR, HOR, VER)."""
    TL, TR, BL, BR, HOR, VER = 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
    set_glyph(font, TL, [0, 0, 0, 0x0F, 0x08, 0x08, 0x08, 0x08])
    set_glyph(font, TR, [0, 0, 0, 0x38, 0x08, 0x08, 0x08, 0x08])
    set_glyph(font, BL, [0x08, 0x08, 0x08, 0x0F, 0, 0, 0, 0])
    set_glyph(font, BR, [0x08, 0x08, 0x08, 0x38, 0, 0, 0, 0])
    set_glyph(font, HOR, [0, 0, 0, 0x3F, 0, 0, 0, 0])
    set_glyph(font, VER, [0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08])
    return TL, TR, BL, BR, HOR, VER


def build_circle_tiles(font, ccols=9, crows=9):
    """Filled circle, computed from the circle equation over the full
    pixel grid (ccols*6 x crows*8) then sliced into 6x8 character-cell
    tiles. Identical tiles dedupe (the circle's symmetry means most
    edge tiles repeat); an all-blank tile maps to CH_SPACE instead of
    consuming a glyph code. Returns cellmap[row][col] -> screencode."""
    w, h = ccols * 6, crows * 8
    cx, cy = (w - 1) / 2.0, (h - 1) / 2.0
    r = min(w, h) / 2.0 - 0.5
    grid = [
        [1 if (x - cx) ** 2 + (y - cy) ** 2 <= r * r else 0 for x in range(w)]
        for y in range(h)
    ]

    # Spare codes not used by any text label on this screen: symbols
    # outside '-' '.' '/' ':' , plus lowercase a-z.
    pool = []
    for lo, hi in [(0x27, 0x2D), (0x3B, 0x41), (0x5B, 0x61), (0x61, 0x7B)]:
        pool.extend(range(lo, hi))

    tile_code = {}
    cellmap = [[None] * ccols for _ in range(crows)]
    for ty in range(crows):
        for tx in range(ccols):
            rowsbytes = []
            for py in range(8):
                bits = 0
                for px in range(6):
                    if grid[ty * 8 + py][tx * 6 + px]:
                        bits |= 1 << (5 - px)
                rowsbytes.append(bits)
            if all(b == 0 for b in rowsbytes):
                cellmap[ty][tx] = CH_SPACE
                continue
            key = tuple(rowsbytes)
            if key not in tile_code:
                code = pool[len(tile_code)]
                tile_code[key] = code
                set_glyph(font, code, rowsbytes)
            cellmap[ty][tx] = tile_code[key]
    return cellmap, ccols, crows


def build_screen(border_codes, circle_cellmap, ccols, crows):
    TL, TR, BL, BR, HOR, VER = border_codes
    screen = bytearray([CH_SPACE] * (COLS * ROWS))

    def put(col, row, byte):
        screen[row * COLS + col] = byte

    def put_row_attr(row, ink, paper):
        put(0, row, ink)
        put(1, row, paper)

    def put_text(row, startcol, text, ink, paper):
        put_row_attr(row, ink, paper)
        for i, ch in enumerate(text):
            put(startcol + i, row, ord(ch))

    def center_start(text_len, content_start=2, content_width=38):
        return content_start + (content_width - text_len) // 2

    # Top/bottom border.
    put_row_attr(0, A_FWCYAN, A_BGBLACK)
    put(2, 0, TL)
    for c in range(3, 39):
        put(c, 0, HOR)
    put(39, 0, TR)

    # Bottom border at row 27 (the screen's true last row, 28 rows total
    # 0-27) -- was row 26 when ROWS was still 27 (matching OSE's own
    # VIEWPORT_HEIGHT before its 27->28 fix, see src/appstate.h); bumped
    # together with ROWS above so this demo keeps loading after
    # canvas_resize()'s minimum height grew to 28.
    put_row_attr(27, A_FWCYAN, A_BGBLACK)
    put(2, 27, BL)
    for c in range(3, 39):
        put(c, 27, HOR)
    put(39, 27, BR)

    # Side borders for every interior row.
    for row in range(1, 27):
        put(2, row, VER)
        put(39, row, VER)

    put_text(1, center_start(23), "ORIC SCREEN EDITOR LOCI", A_FWWHITE, A_BGBLACK)
    put_text(2, center_start(20), "CUSTOM CHARSET DEMO", A_FWYELLOW, A_BGBLACK)
    put_row_attr(3, A_FWCYAN, A_BGBLACK)
    put_text(4, center_start(31), "EVERY GLYPH BELOW IS HAND-DRAWN", A_FWWHITE, A_BGBLACK)
    put_row_attr(5, A_FWCYAN, A_BGBLACK)

    put_row_attr(6, A_FWWHITE, A_BGBLACK)
    for c in range(3, 39):
        put(c, 6, CH_INVSPACE)

    circ_start_col = 3 + (36 - ccols) // 2
    circ_start_row = 7
    for ty in range(crows):
        row = circ_start_row + ty
        put_row_attr(row, A_FWYELLOW, A_FWBLUE + 16)
        for tx in range(ccols):
            put(circ_start_col + tx, row, circle_cellmap[ty][tx])

    put_row_attr(16, A_FWCYAN, A_BGBLACK)

    # Ink colour labels -- row 17 for label, row 18 for 3-letter blocks.
    # Block layout (row 18, left-aligned at col 3):
    #   8 blocks × [ink_attr + "XYZ"] = 8 × 4 = 32 bytes, cols 3-34.
    # Order: 1,2,...,7,0 so black (invisible on black paper) is trailing.
    # col 35: A_FWWHITE to restore ink before the right border at col 39.
    put_text(17, 3, "INK COLOURS:", A_FWWHITE, A_BGBLACK)
    put_row_attr(18, A_FWWHITE, A_BGBLACK)
    ink_order = [1, 2, 3, 4, 5, 6, 7, 0]
    for i, n in enumerate(ink_order):
        base = 3 + i * 4
        put(base,     18, n)                          # ink attribute
        for j, ch in enumerate(COLOUR_NAMES[n]):
            put(base + 1 + j, 18, ord(ch))            # 3-letter name
    put(35, 18, A_FWWHITE)                            # restore ink for right border

    # Paper colour labels -- row 19 for label, row 20 for 3-letter blocks.
    # Block layout (row 20, left-aligned starting at col 4):
    #   col 3: ink=black (so all abbreviations are printed in black ink)
    #   8 blocks × [paper_attr + "XYZ"] = 8 × 4 = 32 bytes, cols 4-35.
    # Order: 1,2,...,7,0 so black paper (invisible text on black) is trailing.
    # cols 36-37: restore ink=white + paper=black before the right border.
    put_text(19, 3, "PAPER COLOURS:", A_FWWHITE, A_BGBLACK)
    put_row_attr(20, A_FWWHITE, A_BGBLACK)
    put(3, 20, A_FWBLACK)                             # black ink for all blocks
    pap_order = [1, 2, 3, 4, 5, 6, 7, 0]
    for i, n in enumerate(pap_order):
        base = 4 + i * 4
        put(base,     20, A_BGBLACK + n)              # paper attribute
        for j, ch in enumerate(COLOUR_NAMES[n]):
            put(base + 1 + j, 20, ord(ch))            # 3-letter name
    put(36, 20, A_FWWHITE)                            # restore ink
    put(37, 20, A_BGBLACK)                            # restore paper

    put_row_attr(21, A_FWCYAN, A_BGBLACK)
    put_text(22, center_start(31), "MADE WITH ORICSCREENEDITORLOCI", A_FWWHITE, A_BGBLACK)
    put_text(23, center_start(17), "XANDER MOL - 2026", A_FWCYAN, A_BGBLACK)
    for row in range(24, 27):
        put_row_attr(row, A_FWCYAN, A_BGBLACK)

    assert len(screen) == COLS * ROWS
    return screen


def main():
    rom_path = find_rom(sys.argv)
    font = load_rom_font(rom_path)
    border_codes = build_border_tiles(font)
    circle_cellmap, ccols, crows = build_circle_tiles(font)
    screen = build_screen(border_codes, circle_cellmap, ccols, crows)

    with open(OUT_PATH, "wb") as f:
        f.write(font)
        f.write(screen)
    print(f"Wrote {OUT_PATH} ({len(font) + len(screen)} bytes)")

    # Project format (src/fileio.c's ProjectHeader -- 7 uint16_t then
    # 8 uint8_t fields, no padding, same layout tools/gen_logo_screen.py
    # uses) -- no OSEDEMOCA.BIN, see the header comment above.
    import struct
    FILEIO_MAGIC = 0x4F53
    proj = struct.pack(
        "<7H8B",
        FILEIO_MAGIC, COLS, ROWS,  # magic, canvas_width, canvas_height
        0, 0,    # cursor_x, cursor_y
        0, 0,    # xoffset, yoffset
        0x40,    # plotscreencode ('@', this codebase's default)
        0,       # plotaltchar
        A_FWWHITE, 0,  # plotink, plotpaper
        0, 0,    # plotblink, plotdouble
        1, 0,    # stdchanged=1 (custom Std charset), altchanged=0 (Alt untouched)
    )
    assert len(proj) == 22

    with open(os.path.join(OUT_DIR, OUT_BASE + "PJ.BIN"), "wb") as f:
        f.write(proj)
    with open(os.path.join(OUT_DIR, OUT_BASE + "SC.BIN"), "wb") as f:
        f.write(screen)
    with open(os.path.join(OUT_DIR, OUT_BASE + "CS.BIN"), "wb") as f:
        f.write(font)
    print(f"Wrote {OUT_BASE}PJ/SC/CS.BIN to {OUT_DIR}")


if __name__ == "__main__":
    main()
