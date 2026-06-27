#!/usr/bin/env python3
# tools/patch_title_screen.py
#
# Patches assets/OSETSC.BIN (the boot splash screen, a raw 40x28 Oric
# screen RAM dump) to fix the copyright year on row 24 and generates the
# French version OSETSF.BIN.
#
# The main title artwork (the large alt-charset "ORIC SCREEN EDITOR LOCI"
# wordmark across rows 2-23) is hand-drawn and identical for both languages
# -- only rows 24-25 carry the localised credits text.
#
# Row 24 layout (original, all 40 bytes):
#   bytes  0-11: colour-spectrum display (attribute bytes + coloured chars)
#   bytes 12-19: 8 spaces (padding between spectrum and credits text)
#   byte    20:  0x08 (A_STD, charset-mode reset)
#   byte    21:  0x03 (A_FWYELLOW)
#   bytes 22-39: 18-char credits text
#
# EN fix: "Written in 2022-26" (18 chars) → "Written in 2022-2026" (20 chars).
#   We need 2 extra bytes.  Trim 2 spaces from the padding (8→6 spaces) to
#   make room, shifting bytes 20-21 to 18-19 and the text to start at 20.
#
# FR:  "Ecrit en 2022-2026" (18 chars) — same length as the original EN text
#   so no padding change needed.  Also patches row 25 "by" → "par".
#
# Usage:  python3 tools/patch_title_screen.py
# Output: assets/OSETSC.BIN (patched in place), assets/OSETSF.BIN (new)
#
# The source binary (OSETSC.BIN) must already exist in assets/ -- this
# script does NOT regenerate the title artwork, only the credits rows.

import os
import sys
import shutil

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets")
EN_PATH = os.path.join(ASSETS, "OSETSC.BIN")
FR_PATH = os.path.join(ASSETS, "OSETSF.BIN")

COLS   = 40
ROW_24 = 24 * COLS   # byte offset of row 24
ROW_25 = 25 * COLS   # byte offset of row 25

# The first 12 bytes of rows 24 and 25 are the colour-spectrum display --
# attribute bytes plus coloured character codes.  They stay unchanged.
HEADER_LEN = 12

# Original layout after HEADER_LEN:
#   8 spaces + 0x08 + 0x03 + 18-char text
# = bytes [12..39], 28 bytes.
PAD_ORIG    = 8           # original padding space count
ATTR_BYTES  = bytes([0x08, 0x03])  # A_STD + A_FWYELLOW (charset + ink reset)
EXPECTED_ORIG_LEN = PAD_ORIG + len(ATTR_BYTES) + 18  # 28 bytes → cols 12..39


def make_tail(pad, text):
    """Build the 28-byte tail of a credits row (cols 12-39)."""
    assert len(text) + pad + 2 == 28, (
        f"tail must be 28 bytes: pad={pad} text={len(text)} chars"
    )
    return bytes([0x20] * pad) + ATTR_BYTES + text.encode("ascii")


def patch(data, offset, replacement):
    """Return a copy of data with replacement written at offset."""
    b = bytearray(data)
    b[offset : offset + len(replacement)] = replacement
    return bytes(b)


def main():
    if not os.path.isfile(EN_PATH):
        sys.exit(f"ERROR: {EN_PATH} not found -- run 'make usb' or copy it first")

    with open(EN_PATH, "rb") as f:
        original = f.read()

    if len(original) != COLS * 28:
        sys.exit(f"ERROR: unexpected size {len(original)} (expected {COLS * 28})")

    # --- Verify expected original tail of row 24 ----------------------------
    orig_tail_24 = original[ROW_24 + HEADER_LEN : ROW_24 + COLS]
    expected_24  = make_tail(PAD_ORIG, "Written in 2022-26")
    if orig_tail_24 != expected_24:
        # Could be that the file was already patched -- check.
        patched_24 = make_tail(6, "Written in 2022-2026")
        if orig_tail_24 == patched_24:
            print("OSETSC.BIN row 24 already patched (2022-2026) -- skipping EN patch")
            en_data = original
        else:
            print(f"WARNING: row 24 tail does not match expected original.")
            print(f"  got:      {orig_tail_24!r}")
            print(f"  expected: {expected_24!r}")
            print("Proceeding anyway (will overwrite row 24 content).")
            en_data = original
    else:
        # EN patch: shrink padding from 8 to 6 spaces; text grows from 18
        # to 20 chars ("2022-26" → "2022-2026").
        new_tail_24_en = make_tail(6, "Written in 2022-2026")
        en_data = patch(original, ROW_24 + HEADER_LEN, new_tail_24_en)
        print("OSETSC.BIN row 24 patched: 2022-26 -> 2022-2026")

    with open(EN_PATH, "wb") as f:
        f.write(en_data)
    print(f"Wrote {EN_PATH}")

    # --- Build FR version (from original, not from the EN-patched copy) ------
    # FR row 24: "Ecrit en 2022-2026" = 18 chars -- same length as the
    # original "Written in 2022-26", so padding stays at 8 spaces.
    # FR row 25: "    par Xander Mol" = 18 chars ("by" -> "par", 1 fewer
    # leading space to keep same total length).
    fr_data = original   # start from original to avoid double-patching

    new_tail_24_fr = make_tail(PAD_ORIG, "Ecrit en 2022-2026")
    fr_data = patch(fr_data, ROW_24 + HEADER_LEN, new_tail_24_fr)

    orig_tail_25 = fr_data[ROW_25 + HEADER_LEN : ROW_25 + COLS]
    expected_25  = make_tail(PAD_ORIG, "     by Xander Mol")
    if orig_tail_25 != expected_25:
        print(f"WARNING: row 25 tail does not match expected original.")
        print(f"  got:      {orig_tail_25!r}")
        print(f"  expected: {expected_25!r}")
        print("Proceeding with row 25 patch anyway.")

    # "    par Xander Mol" = 4 leading spaces + 14 chars = 18 chars total,
    # matching the original 18-char slot.
    new_tail_25_fr = make_tail(PAD_ORIG, "    par Xander Mol")
    fr_data = patch(fr_data, ROW_25 + HEADER_LEN, new_tail_25_fr)

    with open(FR_PATH, "wb") as f:
        f.write(fr_data)
    print(f"Wrote {FR_PATH}")


if __name__ == "__main__":
    main()
