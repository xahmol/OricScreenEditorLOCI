#!/usr/bin/env python3
# tools/gen_logo_screen.py
#
# One-off generator for assets/OSELOGO{PJ,SC,CS,CA}.BIN -- a showcase
# screen recreating the official Oric company logo (the "ORIC" wordmark
# with the red diagonal slash through the O and a red baseline) entirely
# out of hand-drawn Oric character glyphs -- no hires/bitmap mode, just
# the 40x27 text screen and two redefined charset banks.
#
# Why two charset banks: the logo has almost no symmetry to dedupe
# against (unlike assets/OSEDEMO.BIN's circle), so at a resolution fine
# enough to stay recognisable (26x11 character cells) it needs >100
# unique glyph tiles -- more than CHARSET_STD's ~96-minus-reserved-text
# codes can hold alone. CHARSET_ALT (src/charsetswap.c's docs: codes
# 0x20-0x6F only, see CHARSET_ALT_GLYPH_AREA_SIZE) supplies the rest.
# Mixing banks costs one extra screen column per A_STD/A_ALT switch
# (src/oric.h serial attributes), so tiles are assigned to a SINGLE
# bank per *row* (never split a row across both banks) to avoid
# expensive mid-row switching -- see the "per-row bank assignment"
# comment below. Each row also gets at most one ink colour (white/red)
# switch column where the diagonal slash enters/exits that row.
#
# Output format: File > Load Project's 4-file scheme (src/fileio.c
# fileio_save_project()/fileio_load_project()) -- "<name>PJ.BIN"
# (ProjectHeader), "<name>SC.BIN" (bare 1120-byte 40x28 screen dump),
# "<name>CS.BIN" (768-byte CHARSET_STD range), "<name>CA.BIN" (640-byte
# CHARSET_ALT range). Unlike assets/OSEDEMO.BIN's single-file Combined
# format, Project is the only format that can load into *both* charset
# banks at once.
#
# Not part of the build -- run by hand whenever the demo needs
# regenerating. Requires Pillow (pip install pillow) and a local copy
# of the source logo image (not redistributed in this repo -- it's
# Oric's own company logo, fetched from Wikimedia Commons for this
# one-off conversion, not checked in raw) plus a real Atmos ROM image
# for CHARSET_STD's text glyphs.
#
# Usage: python3 tools/gen_logo_screen.py LOGO.png [ROM.rom]
#        (ROM defaults to $PHOSDIR/roms/basic11b.rom, then
#        ~/oricutron/roms/basic11b.rom, if omitted)

import os
import sys

try:
    from PIL import Image
    import numpy as np
except ImportError:
    sys.exit("This script needs Pillow and numpy: pip install pillow numpy")

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets")
OUT_BASE = "OSELOGO"

# ---- Oric serial attribute values (oric.h) ----
A_FWRED = 1
A_FWWHITE = 7
A_BGBLACK = 16
A_STD = 8
A_ALT = 9
CH_SPACE = 0x20

COLS_SCR, ROWS_SCR = 40, 28

# Logo cell-grid resolution -- see the header comment above for how this
# was chosen (max per-row colour transitions stays low enough to fit
# the 36-column content budget even after charset-bank-switch overhead).
LOGO_COLS, LOGO_ROWS = 26, 11

TITLE = "OFFICIAL ORIC LOGOTYPE - 1983"
FOOTER1 = "RECREATED IN ORICSCREENEDITORLOCI"
FOOTER2 = "SOURCE: WIKIMEDIA COMMONS"
FOOTER3 = "XANDER MOL - 2026"


def find_rom(argv):
    if len(argv) > 2:
        return argv[2]
    phosdir = os.environ.get("PHOSDIR")
    candidates = []
    if phosdir:
        candidates.append(os.path.join(phosdir, "roms", "basic11b.rom"))
    candidates.append(os.path.expanduser("~/oricutron/roms/basic11b.rom"))
    for c in candidates:
        if os.path.isfile(c):
            return c
    sys.exit("basic11b.rom not found -- pass its path as argv[2]")


def load_rom_font(rom_path):
    with open(rom_path, "rb") as f:
        rom = f.read()
    font = bytearray(rom[0xFC78 - 0xC000 : 0xFC78 - 0xC000 + 768])
    assert len(font) == 768
    return font


def glyph_off(code):
    return (code - 0x20) * 8


def set_glyph(font, code, rows8, offset0=0x20):
    off = (code - offset0) * 8
    for i, b in enumerate(rows8):
        font[off + i] = b


def classify_logo(image_path, cols, rows):
    """Downscale the source logo to a cols x rows character-cell grid.
    Returns cellinfo[row][col] = None (blank) or (bits8-tuple, colour)
    where colour is A_FWWHITE or A_FWRED -- the cell's *dominant* colour
    (a single glyph cell can only show one ink colour, so where the
    slash and a letter overlap within one cell, majority wins)."""
    img = Image.open(image_path).convert("RGB")
    arr = np.array(img).astype(int)
    black, white, red = np.array([0, 0, 0]), np.array([255, 255, 255]), np.array([228, 28, 28])

    def nearest(arr, colours):
        dists = np.stack([((arr - c) ** 2).sum(axis=2) for c in colours], axis=-1)
        return np.argmin(dists, axis=-1)

    idx = nearest(arr, [black, white, red])
    white_mask = (idx == 1).astype(np.uint8) * 255
    red_mask = (idx == 2).astype(np.uint8) * 255

    w, h = cols * 6, rows * 8
    wm = np.array(Image.fromarray(white_mask).resize((w, h), Image.BOX)) >= 128
    rm = np.array(Image.fromarray(red_mask).resize((w, h), Image.BOX)) >= 128

    cellinfo = [[None] * cols for _ in range(rows)]
    for ty in range(rows):
        for tx in range(cols):
            wcount = wm[ty * 8 : ty * 8 + 8, tx * 6 : tx * 6 + 6].sum()
            rcount = rm[ty * 8 : ty * 8 + 8, tx * 6 : tx * 6 + 6].sum()
            if wcount == 0 and rcount == 0:
                continue
            colour = A_FWRED if rcount > wcount else A_FWWHITE
            lit = wm[ty * 8 : ty * 8 + 8, tx * 6 : tx * 6 + 6] | rm[ty * 8 : ty * 8 + 8, tx * 6 : tx * 6 + 6]
            bits = tuple(
                sum(1 << (5 - px) for px in range(6) if lit[py, px]) for py in range(8)
            )
            if any(bits):
                cellinfo[ty][tx] = (bits, colour)
    return cellinfo


def assign_codes(cellinfo, cols, rows, std_font):
    """Per-row bank assignment: every cell in a given screen row uses
    glyphs from a single charset bank, chosen as whichever bank still
    has room for that row's not-yet-allocated tile bitmaps (preferring
    STD). This guarantees zero A_STD/A_ALT switches *within* a row --
    only a single switch (if any) right before the row's first tile.
    A bitmap already coded in one bank gets a second, separate code in
    the other bank if a later row needs it there (cheap: this logo has
    little tile reuse across rows anyway, see the header comment)."""
    reserved = set(ord(c) for t in (TITLE, FOOTER1, FOOTER2, FOOTER3) for c in t)
    reserved.add(0x20)
    std_spare = [c for c in range(0x21, 0x7F) if c not in reserved]
    alt_spare = list(range(0x21, 0x70))  # CHARSET_ALT_GLYPH_AREA_SIZE cap (0x20-0x6F)

    code_in_bank = {}
    std_idx = alt_idx = 0
    row_bank = {}
    alt_font = bytearray(640)

    for ty in range(rows):
        bitmaps_this_row = []
        for tx in range(cols):
            info = cellinfo[ty][tx]
            if info is None:
                continue
            bits = info[0]
            if bits not in bitmaps_this_row:
                bitmaps_this_row.append(bits)

        need_std = sum(1 for b in bitmaps_this_row if (b, "STD") not in code_in_bank)
        need_alt = sum(1 for b in bitmaps_this_row if (b, "ALT") not in code_in_bank)
        rem_std = len(std_spare) - std_idx
        rem_alt = len(alt_spare) - alt_idx
        if need_std <= rem_std:
            bank = "STD"
        elif need_alt <= rem_alt:
            bank = "ALT"
        else:
            sys.exit(f"row {ty}: not enough spare glyph codes in either bank "
                      f"(need_std={need_std} rem_std={rem_std}, need_alt={need_alt} rem_alt={rem_alt})")
        row_bank[ty] = bank

        for bits in bitmaps_this_row:
            if (bits, bank) in code_in_bank:
                continue
            if bank == "STD":
                code = std_spare[std_idx]
                std_idx += 1
                set_glyph(std_font, code, bits)
            else:
                code = alt_spare[alt_idx]
                alt_idx += 1
                set_glyph(alt_font, code, bits, offset0=0x20)
            code_in_bank[(bits, bank)] = code

    print(f"STD spare used: {std_idx}/{len(std_spare)}  ALT spare used: {alt_idx}/{len(alt_spare)}")
    return code_in_bank, row_bank, alt_font


def build_border_tiles(std_font):
    TL, TR, BL, BR, HOR, VER = 0x21, 0x22, 0x23, 0x24, 0x25, 0x26
    set_glyph(std_font, TL, [0, 0, 0, 0x0F, 0x08, 0x08, 0x08, 0x08])
    set_glyph(std_font, TR, [0, 0, 0, 0x38, 0x08, 0x08, 0x08, 0x08])
    set_glyph(std_font, BL, [0x08, 0x08, 0x08, 0x0F, 0, 0, 0, 0])
    set_glyph(std_font, BR, [0x08, 0x08, 0x08, 0x38, 0, 0, 0, 0])
    set_glyph(std_font, HOR, [0, 0, 0, 0x3F, 0, 0, 0, 0])
    set_glyph(std_font, VER, [0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08])
    return TL, TR, BL, BR, HOR, VER


def build_screen(border_codes, cellinfo, code_in_bank, row_bank, cols, rows):
    TL, TR, BL, BR, HOR, VER = border_codes
    screen = bytearray([CH_SPACE] * (COLS_SCR * ROWS_SCR))

    def put(col, row, byte):
        screen[row * COLS_SCR + col] = byte

    def put_row_attr(row, ink, paper):
        put(0, row, ink)
        put(1, row, paper)

    def put_text(row, startcol, text, ink, paper):
        put_row_attr(row, ink, paper)
        for i, ch in enumerate(text):
            put(startcol + i, row, ord(ch))

    def center_start(text_len, content_start=2, content_width=38):
        return content_start + (content_width - text_len) // 2

    put_row_attr(0, A_FWWHITE, A_BGBLACK)
    put(2, 0, TL)
    for c in range(3, 39):
        put(c, 0, HOR)
    put(39, 0, TR)

    # Bottom border at row 27 (the screen's true last row, 28 rows total
    # 0-27) -- was row 26 when ROWS_SCR was still 27 (matching OSE's own
    # VIEWPORT_HEIGHT before its 27->28 fix, see src/appstate.h); bumped
    # together with ROWS_SCR above so this demo project keeps loading
    # after canvas_resize()'s minimum height grew to 28.
    put_row_attr(27, A_FWWHITE, A_BGBLACK)
    put(2, 27, BL)
    for c in range(3, 39):
        put(c, 27, HOR)
    put(39, 27, BR)

    for row in range(1, 27):
        put(2, row, VER)
        put(39, row, VER)
        put_row_attr(row, A_FWWHITE, A_BGBLACK)

    put_text(2, center_start(len(TITLE)), TITLE, A_FWWHITE, A_BGBLACK)

    def row_width(ty, bank):
        # Must also budget for the trailing ink/bank reset added below the
        # main tile loop -- found 2026-06-21 (real OSE rendering, user
        # report): a row that ends with non-default ink/bank active
        # leaves that state active all the way through the right-hand
        # border glyph (attribute bytes persist left-to-right for the
        # rest of the scanline, the ULA only resets at the START of each
        # new row, not at end-of-content), corrupting the border's
        # appearance for that one row. Without this, max_width/logo_
        # start_col could come out too small to leave room for the reset
        # bytes the draw loop below now always appends.
        cur_ink, cur_bank = A_FWWHITE, "STD"
        width = 0
        for tx in range(cols):
            info = cellinfo[ty][tx]
            if info is None:
                width += 1
                continue
            _, colour = info
            if colour != cur_ink:
                width += 1
                cur_ink = colour
            if bank != cur_bank:
                width += 1
                cur_bank = bank
            width += 1
        if cur_ink != A_FWWHITE:
            width += 1
        if cur_bank != "STD":
            width += 1
        return width

    max_width = max(row_width(ty, row_bank[ty]) for ty in range(rows))
    # Centered start would be 3 + (36-cols)//2, but per-row attribute-byte
    # overhead (colour/bank switches) can push some rows wider than the
    # logical tile count -- clamp so even the widest row stays clear of
    # the right-hand border glyph at column 39.
    logo_start_col = min(3 + (36 - cols) // 2, 39 - max_width)
    assert logo_start_col >= 3, f"logo too wide for the frame: max_width={max_width}"
    logo_start_row = 7
    for ty in range(rows):
        row = logo_start_row + ty
        bank = row_bank[ty]
        # Row already starts (ULA hardware reset) at ink=white/std at
        # column 0 -- but col0/1 here are our own ink/paper reset bytes
        # (A_FWWHITE/A_BGBLACK, set above), so "current" state entering
        # the content area is genuinely (white, STD).
        cur_ink, cur_bank = A_FWWHITE, "STD"
        col = logo_start_col
        for tx in range(cols):
            info = cellinfo[ty][tx]
            if info is None:
                put(col, row, CH_SPACE)
                col += 1
                continue
            bits, colour = info
            code = code_in_bank[(bits, bank)]
            if colour != cur_ink:
                put(col, row, colour)
                col += 1
                cur_ink = colour
            if bank != cur_bank:
                put(col, row, A_ALT if bank == "ALT" else A_STD)
                col += 1
                cur_bank = bank
            put(col, row, code)
            col += 1
        # Reset ink/bank back to default before the row ends -- see
        # row_width()'s matching comment above for why this is needed
        # (otherwise the right-hand border glyph at column 39 inherits
        # whatever ink/bank was last active in this row's content).
        if cur_ink != A_FWWHITE:
            put(col, row, A_FWWHITE)
            col += 1
        if cur_bank != "STD":
            put(col, row, A_STD)
            col += 1
        assert col <= 39, f"row {ty} overflowed: ended at column {col}"

    put_text(21, center_start(len(FOOTER1)), FOOTER1, A_FWWHITE, A_BGBLACK)
    put_text(22, center_start(len(FOOTER2)), FOOTER2, A_FWWHITE, A_BGBLACK)
    put_text(23, center_start(len(FOOTER3)), FOOTER3, A_FWWHITE, A_BGBLACK)

    assert len(screen) == COLS_SCR * ROWS_SCR
    return screen


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__ or "usage: gen_logo_screen.py LOGO.png [ROM.rom]")
    logo_path = sys.argv[1]
    rom_path = find_rom(sys.argv)

    std_font = load_rom_font(rom_path)
    border_codes = build_border_tiles(std_font)

    cellinfo = classify_logo(logo_path, LOGO_COLS, LOGO_ROWS)
    code_in_bank, row_bank, alt_font = assign_codes(cellinfo, LOGO_COLS, LOGO_ROWS, std_font)
    screen = build_screen(border_codes, cellinfo, code_in_bank, row_bank, LOGO_COLS, LOGO_ROWS)

    # ProjectHeader (src/fileio.c) -- 7 uint16_t then 8 uint8_t fields,
    # no padding (confirmed by that file's own "size 22" test assertion).
    import struct
    FILEIO_MAGIC = 0x4F53
    proj = struct.pack(
        "<7H8B",
        FILEIO_MAGIC, COLS_SCR, ROWS_SCR,  # magic, canvas_width, canvas_height
        0, 0,    # cursor_x, cursor_y
        0, 0,    # xoffset, yoffset
        0x40,    # plotscreencode ('@', this codebase's default)
        0,       # plotaltchar
        A_FWWHITE, 0,  # plotink, plotpaper
        0, 0,    # plotblink, plotdouble
        1, 1,    # stdchanged, altchanged
    )
    assert len(proj) == 22

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, OUT_BASE + "PJ.BIN"), "wb") as f:
        f.write(proj)
    with open(os.path.join(OUT_DIR, OUT_BASE + "SC.BIN"), "wb") as f:
        f.write(screen)
    with open(os.path.join(OUT_DIR, OUT_BASE + "CS.BIN"), "wb") as f:
        f.write(std_font)
    with open(os.path.join(OUT_DIR, OUT_BASE + "CA.BIN"), "wb") as f:
        f.write(alt_font)

    print(f"Wrote {OUT_BASE}PJ/SC/CS/CA.BIN to {OUT_DIR}")


if __name__ == "__main__":
    main()
