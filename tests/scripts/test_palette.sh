#!/usr/bin/env bash
# tests/scripts/test_palette.sh
#
# Palette smoke test (the `make test-palette` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, enters the palette popup via 'p' from main mode, and exercises
# the key bindings (src/palette.c): Fav:/Std:/Alt: labels and grid contents,
# the V1-derived initial cursor position, cursor wrap, SPACE selects a
# screencode into app.plotscreencode (statusbar C: updates), '0'-'9' stores
# the highlighted screencode into a favourite, 'v' toggles the visualchar[]
# remap for the Alt section, and ESC leaves plotscreencode unchanged.
# Verified via $BB80 screen-text dumps (oric_screen.py --find) and raw
# screen-RAM byte dumps of individual grid cells (oric_screen.py --bytes).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely. Cycle budget: 8,080,000 (boot+splash) +
#   N_pairs*1,100,000 + 300,000 margin.
#
# Grid layout (src/palette.c): popup at screen cols 2-39, rows 0-12.
# PAL_GRID_X0=5, PAL_GRID_STEP=2 -- grid cell (rowsel, colsel) is 1 byte
# (the screencode, XOR 0x80 when highlighted -- --bytes dumps raw bytes, so
# a highlighted cell shows code|0x80) at screen col (2 + 5 + colsel*2), row
# rowsel. Default app.plotscreencode=0x40 ('@') gives initial cursor
# (rowsel=3, colsel=0) -- code = 0x20 + 0 + (3-1)*16 = 0x40, highlighted ->
# 0xc0, addr 0xBBFF. Fav row (rowsel=0) cell 0 -> addr 0xBB87. Alt row 0
# (rowsel=7) cell 0 -> addr 0xBC9F.
#
# Required env vars (set by `make test-palette`):
#   PHOS      path to oric1-emu
#   ATMOSROM  path to roms/basic11b.rom
#   SANDBOX   tests/sandbox (freshly built .tap)
#   OUT       tests/out (scratch dir for RAM dumps)
#   TAPFILE   oseloci.tap

set -u
cd "$(dirname "$0")/../.." || exit 1

SCREEN=tests/scripts/oric_screen.py

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

check_not_found() {
    local label="$1" needle="$2" dump="$3"
    if python3 "$SCREEN" "$dump" --find "$needle" >/dev/null 2>&1; then
        echo "  [FAIL] $label -- '$needle' found (should not be present)"
        fail=$((fail+1))
    else
        echo "  [PASS] $label"
        pass=$((pass+1))
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

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f --loci \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- palette test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 'p' opens; Fav/Std/Alt labels + initial cursor -----------
DUMP1="$OUT/capture_pal_open.bin"
run_capture 9480000 '\p1p' "$DUMP1"
echo ""
echo "'p' opens palette; Fav/Std/Alt labels and initial cursor on '@'"
check_found "Fav: label shown" "Fav:" "$DUMP1"
check_found "Std: label shown" "Std:" "$DUMP1"
check_found "Alt: label shown" "Alt:" "$DUMP1"
check_bytes "Fav[0] default favourite '!' (0x21)" "0xBB87:1" "21" "$DUMP1"
check_bytes "initial cursor on '@' (0x40) highlighted -> 0xc0" "0xBBFF:1" "c0" "$DUMP1"
check_bytes "Alt row0 col0 identity 0x20 (visualmap off)" "0xBC9F:1" "20" "$DUMP1"

# --- Scenario 2: LEFT, LEFT wraps from ('@' = row3,col0) to row2,col14 ----
# row3,col0 LEFT wraps immediately (colsel already 0) to row2,col15 = '?'
# (0x3f); a second LEFT just decrements within row2 to col14 = '>' (0x3e).
DUMP2="$OUT/capture_pal_left_wrap.bin"
run_capture 11680000 '\p1p\p1\l\p1\l' "$DUMP2"
echo ""
echo "LEFT,LEFT cursor-wraps row3,col0 -> row2,col15 -> row2,col14"
check_bytes "cursor now on '>' (0x3e) highlighted -> 0xbe at row2,col14" "0xBBF3:1" "be" "$DUMP2"
check_bytes "old position '@' no longer highlighted (back to 0x40)" "0xBBFF:1" "40" "$DUMP2"

# --- Scenario 3: SPACE selects the highlighted cell into plotscreencode ---
DUMP3="$OUT/capture_pal_space.bin"
run_capture 12780000 '\p1p\p1\l\p1\l\p1 ' "$DUMP3"
echo ""
echo "SPACE selects '>' (0x3e) into app.plotscreencode"
check_not_found "popup closed" "Fav:" "$DUMP3"
check_found "statusbar shows C3E> S20I7P0S" "Main      XY 0, 0C3E> S20I7P0S" "$DUMP3"

# --- Scenario 4: '0' stores the highlighted cell into favourites[0] ------
DUMP4="$OUT/capture_pal_store_fav.bin"
run_capture 12780000 '\p1p\p1\l\p1\l\p10' "$DUMP4"
echo ""
echo "'0' stores highlighted '>' (0x3e) into favourites[0]"
check_bytes "Fav[0] now 0x3e" "0xBB87:1" "3e" "$DUMP4"

# --- Scenario 5: 'v' toggles visualmap, remapping the Alt section --------
DUMP5="$OUT/capture_pal_visualmap.bin"
run_capture 10580000 '\p1p\p1v' "$DUMP5"
echo ""
echo "'v' remaps Alt row0 col0 via visualchar[0] (0x20 -> 0x37)"
check_bytes "Alt row0 col0 now visualchar[0]=0x37" "0xBC9F:1" "37" "$DUMP5"

# --- Scenario 6: ESC leaves app.plotscreencode unchanged ------------------
DUMP6="$OUT/capture_pal_esc.bin"
run_capture 12780000 '\p1p\p1\l\p1\l\p1\e' "$DUMP6"
echo ""
echo "ESC closes popup leaving app.plotscreencode unchanged (0x40)"
check_not_found "popup closed" "Fav:" "$DUMP6"
check_found "statusbar shows C40@ S20I7P0S" "Main      XY 0, 0C40@ S20I7P0S" "$DUMP6"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
