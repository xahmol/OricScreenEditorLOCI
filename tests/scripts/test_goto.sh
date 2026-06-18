#!/usr/bin/env bash
# tests/scripts/test_goto.sh
#
# Goto coordinates ('j') and jump-to-origin ('h') in Main mode
# regression test (the `make test-goto` target). Both go through
# canvas_goto() (src/canvas.h/c): clamps to the canvas extent, centers
# the viewport on the target (clamped at canvas edges), sets cursor_x/y
# + xoffset/yoffset, re-blits.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action. goto_dialog()'s X/Y fields are pre-filled with the
#   cursor's current position; \l\l moves the cwin_textinput cursor to
#   idx=0 so typed digits overwrite from the start (resize_dialog()'s
#   convention).
#
# Required env vars (set by `make test-goto`):
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

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f --loci \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- Goto/Home regression test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ESC at the X field cancels with no change -----------------
DUMP1="$OUT/capture_goto_cancel.bin"
run_capture 12500000 '\p1j\p1\e' "$DUMP1"
echo ""
echo "ESC at the X field cancels, cursor stays at (0,0)"
check_found "back in Main at (0,0)" "Main      XY 0, 0" "$DUMP1"

# --- Scenario 2: type X=5, Y=3, confirm both -- cursor jumps there --------
DUMP2="$OUT/capture_goto_jump.bin"
run_capture 21000000 '\p1j\p1\l\p1\l\p15\p1\n\p1\l\p1\l\p13\p1\n' "$DUMP2"
echo ""
echo "Goto X=5 Y=3 jumps the cursor there"
check_found "cursor at (5,3)" "Main      XY 5, 3" "$DUMP2"

# --- Scenario 3: 'h' jumps straight back to the origin --------------------
DUMP3="$OUT/capture_goto_home.bin"
run_capture 22500000 '\p1j\p1\l\p1\l\p15\p1\n\p1\l\p1\l\p13\p1\n\p1h' "$DUMP3"
echo ""
echo "'h' after a Goto jumps straight back to (0,0)"
check_found "cursor back at (0,0)" "Main      XY 0, 0" "$DUMP3"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
