#!/usr/bin/env bash
# tests/scripts/test_move.sh
#
# Phase 5d -- Move mode smoke test (the `make test-move` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and verifies Move mode (src/move.c, entered via 'm'): cursor
# keys nudge the *content* of the visible viewport by one cell (not the
# cursor itself), the trailing edge blanks, and both ENTER and ESC exit to
# Main with the shift already applied (no rollback -- see move.c's header
# comment for why that's a deliberate simplification vs V1).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely.
#
# Required env vars (set by `make test-move`):
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
echo "  OricScreenEditorLOCI -- Move mode test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: RIGHT,RIGHT shifts 'A' from col0 to col2, ESC exits -------
DUMP1="$OUT/capture_move_right_esc.bin"
run_capture 14980000 '\p1 \p1m\p1\r\p1\r\p1\e' "$DUMP1"
echo ""
echo "SPACE plots 'A' at (0,0), m,RIGHT,RIGHT,ESC shifts it to col2"
check_bytes "col0 blank, col1 blank, col2 'A'" "0xBB81:3" "20 41 20" "$DUMP1"
check_found "back in Main mode (no rollback on ESC)" "Main      XY 0, 0" "$DUMP1"

# --- Scenario 2: DOWN shifts 'A' from row0 to row1, ENTER exits -----------
DUMP2="$OUT/capture_move_down_enter.bin"
run_capture 12780000 '\p1 \p1m\p1\d\p1\n' "$DUMP2"
echo ""
echo "SPACE plots 'A' at (0,0), m,DOWN,ENTER shifts it to row1"
check_bytes "row1 col0 = 'A'" "0xBBA8:1" "41" "$DUMP2"
check_found "back in Main mode" "Main      XY 0, 0" "$DUMP2"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
