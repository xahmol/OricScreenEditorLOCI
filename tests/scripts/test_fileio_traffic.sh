#!/usr/bin/env bash
# tests/scripts/test_fileio_traffic.sh
#
# LOCI file I/O byte-traffic test (the `make test-fileio-traffic` target).
#
# Unlike every other test script, this one uses Phosphoric's
# `--loci-flash DIR` (a real host filesystem directory LOCI file ops
# read/write through) instead of plain `--loci` -- giving direct,
# byte-level access to verify what fileio.c (src/fileio.c) actually
# writes to LOCI storage, not just on-screen behaviour. Covers:
#   - File > Save Screen: a bare raw screenmap[] dump, no header -- by
#     design, matches V1 exactly (see fileio.h)
#   - File > Load Screen: prompts for width/height (no embedded size to
#     auto-detect), round-trips a saved file back onto a cleared canvas
#     (statusbar's S field confirms the restored screencode)
#   - File > Save Combined: 768-byte charset immediately followed by
#     screenmap[], no header
#   - File > Save Project: ProjectHeader fields (canvas size, cursor,
#     plotscreencode, stdchanged/altchanged) in "<name>PJ.BIN", plus a
#     bare "<name>SC.BIN" (size known from PJ.BIN, no header of its own)
#   - Charset > Save Standard: raw 768-byte charset dump
#   - Charset > Save Alternate: raw 640-byte charset dump (not 768 -- see
#     CHARSET_ALT_GLYPH_AREA_SIZE, include/charset.h)
#
# Filenames are typed as single characters (Phosphoric's `--type-keys`
# needs a `\p1` pause between every distinct keypress, so longer names
# cost more cycles for no extra coverage) -- LOCI/FAT lowercases them on
# the host filesystem (e.g. typed "T" -> "t.BIN" on disk).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action. Menu bar item counts (src/menudata.c pulldown_
#   titles[1]/[2]): File = Save screen(0)/Load screen(1)/Save project(2)/
#   Load project(3)/Save combined(4)/Load combined(5); Charset = Load
#   standard(0)/Load alternate(1)/Save standard(2)/Save alternate(3)/
#   Load combined(4)/Save combined(5).
#
# Required env vars (set by `make test-fileio-traffic`):
#   PHOS      path to oric1-emu
#   ATMOSROM  path to roms/basic11b.rom
#   SANDBOX   tests/sandbox (freshly built .tap)
#   OUT       tests/out (scratch dir for RAM dumps + the LOCI flash root)
#   TAPFILE   oseloci.tap

set -u
cd "$(dirname "$0")/../.." || exit 1

SCREEN=tests/scripts/oric_screen.py
LOCIFLASH="$OUT/lociflash"

pass=0
fail=0

check_found() {
    local label="$1" needle="$2" dump="$3"
    if python3 "$SCREEN" "$dump" --find "$needle" >/dev/null 2>&1; then
        echo "  [PASS] $label"
        pass=$((pass+1))
    else
        echo "  [FAIL] $label -- '$needle' not found"
        fail=$((fail+1))
    fi
}

check_bytes() {
    local label="$1" addr_len="$2" expected="$3" dump="$4"
    local actual
    actual=$(python3 "$SCREEN" "$dump" --bytes "$addr_len")
    if [ "$actual" = "$expected" ]; then
        echo "  [PASS] $label"
        pass=$((pass+1))
    else
        echo "  [FAIL] $label -- expected '$expected', got '$actual'"
        fail=$((fail+1))
    fi
}

check_file_bytes() {
    # check_file_bytes LABEL FILE PYTHON_EXPR
    # PYTHON_EXPR receives the file's bytes as `data` and must be a bool
    # expression.
    local label="$1" file="$2" expr="$3"
    if [ ! -f "$file" ]; then
        echo "  [FAIL] $label -- $file does not exist"
        fail=$((fail+1))
        return
    fi
    if python3 -c "
data = open('$file', 'rb').read()
import sys
sys.exit(0 if ($expr) else 1)
"; then
        echo "  [PASS] $label"
        pass=$((pass+1))
    else
        echo "  [FAIL] $label -- $file failed: $expr"
        fail=$((fail+1))
    fi
}

reset_flash() {
    rm -rf "$LOCIFLASH"
    mkdir -p "$LOCIFLASH"
}

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f --loci-flash "$LOCIFLASH" \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- LOCI file I/O byte-traffic test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: File > Save Screen -----------------------------------------
# Bare raw dump, no header -- matches V1 exactly (see fileio.h: a saved
# screen is meant to be a portable, tool-agnostic raw dump, loadable from
# any source, not just OSE's own saves).
#
# Every Save action goes through the unified Save-target picker
# (filepicker_run_save(), src/fileio.c's fileio_get_filename() --
# 2026-06-22 redesign, replacing the old 's'-confirms-directory-then-
# type-a-name flow): with the flash root empty, the cursor defaults to
# the pinned "<new file>" sentinel, so the extra `\p1\n` (in the same
# position the old `\p1s` occupied) confirms it and opens the typed-name
# popup -- same keystroke count as before, just ENTER instead of 's'.
reset_flash
DUMP1="$OUT/capture_fileio_save_screen.bin"
run_capture 25200000 '\p1 \p1\f1\p1\r\p1\n\p1\n\p1\n\p1T\p1\n' "$DUMP1"
echo ""
echo "File > Save Screen writes a bare screenmap[] dump (no header)"
check_found "statusbar shows '@' plotted" "Main      XY 0, 0C40@ S40I7P0S" "$DUMP1"
check_file_bytes "t.BIN: size 1120 (40x28, no header)" "$LOCIFLASH/t.BIN" \
    "len(data) == 1120"
check_file_bytes "t.BIN: '@' (0x40) at screen offset 0" "$LOCIFLASH/t.BIN" \
    "data[0] == 0x40"

# --- Scenario 2: File > Load Screen round-trip ------------------------------
# Save '@', ESC, Screen > Clear (confirm cleared, not just stale content --
# the original version of this scenario had no Clear step and so could
# never actually distinguish "load worked" from "save never got
# overwritten"), then File > Load Screen: filepicker selects t.BIN,
# then ENTER/ENTER accepts the pre-filled width/height defaults
# (fileio_get_dimensions(), see fileio.c).
DUMP2="$OUT/capture_fileio_load_screen.bin"
run_capture 36100000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\n\p1\n\p1T\p1\n\p1\e\p1\f1\p1\n\p1\d\p1\d\p1\n\p1\e\p1\f1\p1\r\p1\n\p1\d\p1\n\p1\n\p1\n\p1\n' "$DUMP2"
echo ""
echo "File > Load Screen restores a saved canvas after Screen > Clear"
check_found "statusbar shows '@' restored" "Main      XY 0, 0C40@ S40I7P0S" "$DUMP2"

# --- Scenario 3: File > Save Combined ----------------------------------------
reset_flash
DUMP3="$OUT/capture_fileio_save_combined.bin"
run_capture 31100000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\d\p1\d\p1\d\p1\d\p1\n\p1\n\p1C\p1\n' "$DUMP3"
echo ""
echo "File > Save Combined writes 768B charset + screenmap[] (no header)"
check_found "statusbar shows '@' plotted" "Main      XY 0, 0C40@ S40I7P0S" "$DUMP3"
check_file_bytes "c.BIN: size 1888 (768 + 1120, no header)" "$LOCIFLASH/c.BIN" \
    "len(data) == 1888"
check_file_bytes "c.BIN: '@' at screen-section offset 768" "$LOCIFLASH/c.BIN" \
    "data[768] == 0x40"

# --- Scenario 4: File > Save Project -----------------------------------------
reset_flash
DUMP4="$OUT/capture_fileio_save_project.bin"
run_capture 29100000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\d\p1\d\p1\n\p1\n\p1P\p1\n' "$DUMP4"
echo ""
echo "File > Save Project writes <name>PJ.BIN + <name>SC.BIN"
check_found "statusbar shows '@' plotted" "Main      XY 0, 0C40@ S40I7P0S" "$DUMP4"
check_file_bytes "pPJ.BIN: size 22 (ProjectHeader)" "$LOCIFLASH/pPJ.BIN" \
    "len(data) == 22"
check_file_bytes "pPJ.BIN: canvas 40x28, cursor (0,0)" "$LOCIFLASH/pPJ.BIN" \
    "data[2:10] == bytes([40,0,28,0,0,0,0,0])"
check_file_bytes "pPJ.BIN: plotscreencode is '@'" "$LOCIFLASH/pPJ.BIN" \
    "data[14] == 0x40"
check_file_bytes "pPJ.BIN: stdchanged/altchanged both 0" "$LOCIFLASH/pPJ.BIN" \
    "data[20] == 0 and data[21] == 0"
check_file_bytes "pSC.BIN: size 1120, '@' at offset 0 (no header)" "$LOCIFLASH/pSC.BIN" \
    "len(data) == 1120 and data[0] == 0x40"
[ -f "$LOCIFLASH/pCS.BIN" ] && { echo "  [FAIL] pCS.BIN should not exist (charset unedited)"; fail=$((fail+1)); } \
    || { echo "  [PASS] pCS.BIN correctly absent (charset unedited)"; pass=$((pass+1)); }

# --- Scenario 5: Charset > Save Standard -------------------------------------
reset_flash
DUMP5="$OUT/capture_fileio_save_charset.bin"
run_capture 23100000 \
    '\p1\f1\p1\r\p1\r\p1\n\p1\d\p1\d\p1\n\p1\n\p1S\p1\n' "$DUMP5"
echo ""
echo "Charset > Save Standard writes a raw 768-byte charset dump"
check_file_bytes "s.BIN: size 768" "$LOCIFLASH/s.BIN" "len(data) == 768"

# --- Scenario 6: Charset > Save Alternate -- 640 bytes, not 768 -------------
# CHARSET_ALT only has 896 real bytes of RAM before screen RAM begins at
# $BB80 (see CHARSET_ALT_GLYPH_AREA_SIZE, include/charset.h) -- codes
# 0x70-0x7F have no independent storage there. Regression test for the
# bug where charset_save()/fileio_save_charset() copied the full 768-byte
# Std-sized range for Alt too, reading 128 bytes of live screen RAM as if
# it were charset data.
reset_flash
DUMP6="$OUT/capture_fileio_save_charset_alt.bin"
run_capture 24100000 \
    '\p1\f1\p1\r\p1\r\p1\n\p1\d\p1\d\p1\d\p1\n\p1\n\p1A\p1\n' "$DUMP6"
echo ""
echo "Charset > Save Alternate writes exactly 640 bytes (not 768)"
check_file_bytes "a.BIN: size 640" "$LOCIFLASH/a.BIN" "len(data) == 640"

# --- Scenario 7: Load Project accepts V1's original 19-byte PJ.BIN ---------
# fileio_load_project() transparently falls back to V1's 19-byte layout
# (fileio_parse_v1_project(), src/fileio.c) when the magic field doesn't
# match. Writes a synthetic V1-format "V1PJ.BIN" (cursor (5,3), plotink=3,
# plotpaper=2, plotscreencode='B') + bare "V1SC.BIN" ('C' at offset 0) and
# confirms every field translates correctly.
reset_flash
python3 -c "
data = bytes([
    0, 0,        # stdchanged, altchanged
    5, 3,        # cursor_x, cursor_y
    0, 40,       # width=40
    0, 28,       # height=28
    0x04, 0x38,  # screentotal (ignored)
    3, 2, 0, 0, 0, # plotink, plotpaper, plotblink, plotdouble, plotaltchar
    0x42,        # plotscreencode = 'B'
    0xFF,        # dead byte
    0, 0,        # xoffset, yoffset
])
open('$LOCIFLASH/V1PJ.BIN', 'wb').write(data)
scr = bytearray(b' ' * 1120)
scr[0] = 0x43  # 'C' at (0,0)
open('$LOCIFLASH/V1SC.BIN', 'wb').write(bytes(scr))
"
DUMP7="$OUT/capture_fileio_load_v1project.bin"
run_capture 21000000 \
    '\p1\f1\p1\r\p1\n\p1\d\p1\d\p1\d\p1\n\p1\n\p1\e' "$DUMP7"
echo ""
echo "Load Project accepts V1's original 19-byte PJ.BIN layout"
check_found "cursor/plot fields translated (XY 5,3 C42B I3 P2)" "XY 5, 3C42B S20I3P2" "$DUMP7"
check_bytes "screen content loaded ('C'=0x43 at (0,0))" "0xBB80:1" "43" "$DUMP7"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
