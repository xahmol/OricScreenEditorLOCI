#!/usr/bin/env bash
# tests/scripts/test_hollowbox.sh
#
# Hollow-box toggle in Line/Box mode ('o' at the post-confirm shape
# prompt, src/select.c rect_select()/linebox_run()) regression test (the
# `make test-hollowbox` target).
#
# Grows the same 3x3 rectangle from (0,0) as test_linebox.sh, confirms it
# with ENTER, then presses 'o' once at the shape prompt to select a
# hollow (border-only) box before the final ENTER. Default (no 'o'
# press) behaviour is covered by test_linebox.sh and is unaffected by
# this feature. 'o' moved from "live during rect-grow" to "only at the
# post-confirm shape prompt" 2026-06-20 -- see "Line/Box secondary-hint
# timing" in CLAUDE.md.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action.
#
# Required env vars (set by `make test-hollowbox`):
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

echo "==========================================================="
echo "  OricScreenEditorLOCI -- hollow-box toggle test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ENTER (confirm rect), o, ENTER (shape) plots a hollow border
DUMP1="$OUT/capture_hollowbox_enter.bin"
run_capture 17300000 '\p1l\p1\d\p1\d\p1\r\p1\r\p1\n\p1o\p1\n' "$DUMP1"
echo ""
echo "l,DOWN,DOWN,RIGHT,RIGHT,ENTER,o,ENTER plots a hollow 3x3 border"
check_bytes "row0 cols0-2 = @@@ (top border)"    "0xBB80:3" "40 40 40" "$DUMP1"
check_bytes "row1 cols0-2 = @,blank,@ (sides)"   "0xBBA8:3" "40 20 40" "$DUMP1"
check_bytes "row2 cols0-2 = @,@,cursor-@ (bottom border, cursor on last cell)" "0xBBD0:3" "40 40 c0" "$DUMP1"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP1"

# --- Scenario 2: toggling 'o' twice (at the shape prompt) returns to the --
# default filled box
DUMP2="$OUT/capture_hollowbox_toggleback.bin"
run_capture 18400000 '\p1l\p1\d\p1\d\p1\r\p1\r\p1\n\p1o\p1o\p1\n' "$DUMP2"
echo ""
echo "'o' pressed twice toggles back to filled (default) behaviour"
check_bytes "row1 cols0-2 = @@@ (filled, no hole)" "0xBBA8:3" "40 40 40" "$DUMP2"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
