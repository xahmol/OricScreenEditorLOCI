#!/usr/bin/env bash
# tests/scripts/test_linebox.sh
#
# Phase 5b -- Line/Box mode smoke test (the `make test-linebox` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, enters Line/Box mode via 'l' from Main mode, grows a 3x3
# rectangle from the origin cell (0,0) using DOWN,DOWN,RIGHT,RIGHT, and
# verifies (src/select.c rect_select()/linebox_run()):
#   - ENTER fills the rectangle with app.plotscreencode and returns to Main
#   - ESC cancels, leaving the canvas unchanged
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely.
#
# Required env vars (set by `make test-linebox`):
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
echo "  OricScreenEditorLOCI -- Line/Box mode test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ENTER fills the 3x3 rect with plotscreencode 'A' ----------
DUMP1="$OUT/capture_linebox_enter.bin"
run_capture 14980000 '\p1l\p1\d\p1\d\p1\r\p1\r\p1\n' "$DUMP1"
echo ""
echo "l,DOWN,DOWN,RIGHT,RIGHT,ENTER fills a 3x3 rect with '@'"
check_bytes "row0 cols0-2 = @@@" "0xBB80:3" "40 40 40" "$DUMP1"
check_bytes "row1 cols0-2 = @@@" "0xBBA8:3" "40 40 40" "$DUMP1"
check_bytes "row2 cols0-1 = @@"  "0xBBD0:2" "40 40" "$DUMP1"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP1"

# --- Scenario 2: ESC cancels, canvas unchanged ------------------------------
DUMP2="$OUT/capture_linebox_esc.bin"
run_capture 14980000 '\p1l\p1\d\p1\d\p1\r\p1\r\p1\e' "$DUMP2"
echo ""
echo "l,DOWN,DOWN,RIGHT,RIGHT,ESC leaves canvas unchanged"
check_bytes "row0 cols0-2 still blank" "0xBB80:3" "20 20 20" "$DUMP2"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP2"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
