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
#   - File > Save Screen: FileHeader (magic/width/height) + screenmap[]
#   - File > Load Screen: round-trips a saved file back onto a cleared
#     canvas (statusbar's S field confirms the restored screencode)
#   - File > Save Combined: FileHeader + 768-byte charset + screenmap[]
#   - File > Save Project: ProjectHeader fields (canvas size, cursor,
#     plotscreencode, stdchanged/altchanged) in "<name>PJ.BIN", plus
#     "<name>SC.BIN" matching the Screen format
#   - Charset > Save Standard: raw 768-byte charset dump
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
reset_flash
DUMP1="$OUT/capture_fileio_save_screen.bin"
run_capture 24000000 '\p1 \p1\f1\p1\r\p1\n\p1\n\p1T\p1\n' "$DUMP1"
echo ""
echo "File > Save Screen writes FileHeader + screenmap[]"
check_found "statusbar shows 'A' plotted" "Main      XY 0, 0C41A S41I7P0S" "$DUMP1"
check_file_bytes "t.BIN: size 1086 (6 + 40x27)" "$LOCIFLASH/t.BIN" \
    "len(data) == 1086"
check_file_bytes "t.BIN: FileHeader magic/width/height" "$LOCIFLASH/t.BIN" \
    "data[0:6] == bytes([0x53, 0x4F, 40, 0, 27, 0])"
check_file_bytes "t.BIN: 'A' (0x41) at screen offset 6" "$LOCIFLASH/t.BIN" \
    "data[6] == 0x41"

# --- Scenario 2: File > Load Screen round-trip ------------------------------
DUMP2="$OUT/capture_fileio_load_screen.bin"
run_capture 50000000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\n\p1T\p1\n\p1\e\p1\f1\p1\r\p1\n\p1\d\p1\n' "$DUMP2"
echo ""
echo "File > Load Screen restores a saved canvas after Screen > Clear"
check_found "statusbar shows 'A' restored" "Main      XY 0, 0C41A S41I7P0S" "$DUMP2"

# --- Scenario 3: File > Save Combined ----------------------------------------
reset_flash
DUMP3="$OUT/capture_fileio_save_combined.bin"
run_capture 30000000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\d\p1\d\p1\d\p1\d\p1\n\p1C\p1\n' "$DUMP3"
echo ""
echo "File > Save Combined writes FileHeader + 768B charset + screenmap[]"
check_found "statusbar shows 'A' plotted" "Main      XY 0, 0C41A S41I7P0S" "$DUMP3"
check_file_bytes "c.BIN: size 1854 (6 + 768 + 1080)" "$LOCIFLASH/c.BIN" \
    "len(data) == 1854"
check_file_bytes "c.BIN: 'A' at screen-section offset 774" "$LOCIFLASH/c.BIN" \
    "data[774] == 0x41"

# --- Scenario 4: File > Save Project -----------------------------------------
reset_flash
DUMP4="$OUT/capture_fileio_save_project.bin"
run_capture 28000000 \
    '\p1 \p1\f1\p1\r\p1\n\p1\d\p1\d\p1\n\p1P\p1\n' "$DUMP4"
echo ""
echo "File > Save Project writes <name>PJ.BIN + <name>SC.BIN"
check_found "statusbar shows 'A' plotted" "Main      XY 0, 0C41A S41I7P0S" "$DUMP4"
check_file_bytes "pPJ.BIN: size 22 (ProjectHeader)" "$LOCIFLASH/pPJ.BIN" \
    "len(data) == 22"
check_file_bytes "pPJ.BIN: canvas 40x27, cursor (0,0)" "$LOCIFLASH/pPJ.BIN" \
    "data[2:10] == bytes([40,0,27,0,0,0,0,0])"
check_file_bytes "pPJ.BIN: plotscreencode is 'A'" "$LOCIFLASH/pPJ.BIN" \
    "data[14] == 0x41"
check_file_bytes "pPJ.BIN: stdchanged/altchanged both 0" "$LOCIFLASH/pPJ.BIN" \
    "data[20] == 0 and data[21] == 0"
check_file_bytes "pSC.BIN: size 1086, 'A' at offset 6" "$LOCIFLASH/pSC.BIN" \
    "len(data) == 1086 and data[6] == 0x41"
[ -f "$LOCIFLASH/pCS.BIN" ] && { echo "  [FAIL] pCS.BIN should not exist (charset unedited)"; fail=$((fail+1)); } \
    || { echo "  [PASS] pCS.BIN correctly absent (charset unedited)"; pass=$((pass+1)); }

# --- Scenario 5: Charset > Save Standard -------------------------------------
reset_flash
DUMP5="$OUT/capture_fileio_save_charset.bin"
run_capture 22000000 \
    '\p1\f1\p1\r\p1\r\p1\n\p1\d\p1\d\p1\n\p1S\p1\n' "$DUMP5"
echo ""
echo "Charset > Save Standard writes a raw 768-byte charset dump"
check_file_bytes "s.BIN: size 768" "$LOCIFLASH/s.BIN" "len(data) == 768"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
