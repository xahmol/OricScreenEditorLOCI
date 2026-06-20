#!/usr/bin/env bash
# tests/scripts/test_ellipse.sh
#
# Ellipse/circle toggle in Line/Box mode ('c' during rect-grow,
# src/select.c rect_select()/linebox_run()) regression test (the `make
# test-ellipse` target).
#
# Grows a 5x5 bounding box from (0,0) (DOWN x4, RIGHT x4) -- small
# enough to verify the ellipse_inside() membership test by hand against
# the doubled-coordinate inequality dx2^2*ry2^2 + dy2^2*rx2^2 <=
# rx2^2*ry2^2. For this 5x5 box (rx2=ry2=4), the filled set is the
# 13-cell diamond:
#   row0: . . A . .
#   row1: . A A A .
#   row2: A A A A A
#   row3: . A A A .
#   row4: . . A . .
# and the hollow outline is just the 8 cells (2,0) (1,1) (3,1) (0,2)
# (4,2) (1,3) (3,3) (2,4).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action.
#
# Required env vars (set by `make test-ellipse`):
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

GROW='\p1l\p1\d\p1\d\p1\d\p1\d\p1\r\p1\r\p1\r\p1\r'

echo "==========================================================="
echo "  OricScreenEditorLOCI -- ellipse/circle toggle test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 'c' then ENTER fills the 5x5 box as a diamond ellipse -----
DUMP1="$OUT/capture_ellipse_fill.bin"
run_capture 21000000 "$GROW"'\p1c\p1\n' "$DUMP1"
echo ""
echo "5x5 grow, c, ENTER plots a filled ellipse (diamond pattern)"
check_bytes "row0 = . . @ . ." "0xBB80:5" "20 20 40 20 20" "$DUMP1"
check_bytes "row1 = . @ @ @ ." "0xBBA8:5" "20 40 40 40 20" "$DUMP1"
check_bytes "row2 = @ @ @ @ @" "0xBBD0:5" "40 40 40 40 40" "$DUMP1"
check_bytes "row3 = . @ @ @ ." "0xBBF8:5" "20 40 40 40 20" "$DUMP1"
check_bytes "row4 col4 = c0 (cursor preview, outside ellipse)" "0xBC24:1" "c0" "$DUMP1"
check_found "back in Main mode at (4,4)" "Main      XY 4, 4" "$DUMP1"

# --- Scenario 2: 'c' then 'o' then ENTER plots only the 8-cell outline ----
DUMP2="$OUT/capture_ellipse_hollow.bin"
run_capture 22500000 "$GROW"'\p1c\p1o\p1\n' "$DUMP2"
echo ""
echo "5x5 grow, c, o, ENTER plots only the ellipse outline (hollow)"
check_bytes "row0 = . . @ . ." "0xBB80:5" "20 20 40 20 20" "$DUMP2"
check_bytes "row1 = . @ . @ ." "0xBBA8:5" "20 40 20 40 20" "$DUMP2"
check_bytes "row2 = @ . . . @" "0xBBD0:5" "40 20 20 20 40" "$DUMP2"
check_bytes "row3 = . @ . @ ." "0xBBF8:5" "20 40 20 40 20" "$DUMP2"

# --- Scenario 3: 'c' pressed twice toggles back to a filled box -----------
DUMP3="$OUT/capture_ellipse_toggleback.bin"
run_capture 23600000 "$GROW"'\p1c\p1c\p1\n' "$DUMP3"
echo ""
echo "'c' pressed twice toggles back to filled box (default, unchanged)"
check_bytes "row0 = @@@@@ (filled box, no ellipse)" "0xBB80:5" "40 40 40 40 40" "$DUMP3"
check_bytes "row2 = @@@@@" "0xBBD0:5" "40 40 40 40 40" "$DUMP3"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
