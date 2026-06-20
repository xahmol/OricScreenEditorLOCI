#!/usr/bin/env bash
# tests/scripts/test_trymode.sh
#
# Try mode ('t' in Main mode, src/editor.c) regression test (the
# `make test-trymode` target). Try mode previews app.plotscreencode at
# the cursor by writing directly to screen RAM ($BB80), bypassing
# screenmap[] -- SPACE commits it via undo_snapshot()+canvas_put()
# (permanent, undoable), any other key cancels with no canvas change
# (canvas_blit() redraws the untouched real content).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action.
#
# Required env vars (set by `make test-trymode`):
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
echo "  OricScreenEditorLOCI -- Try mode regression test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 't' then SPACE commits '@' (0x40) at the cursor ----------
DUMP1="$OUT/capture_try_commit.bin"
run_capture 12500000 '\p1t\p1 ' "$DUMP1"
echo ""
echo "'t' then SPACE commits the previewed '@' at (0,0)"
check_bytes "cell (0,0) = c0 (committed '@', cursor-preview)" "0xBB80:1" "c0" "$DUMP1"
check_found "back in Main mode" "Main      XY 0, 0" "$DUMP1"

# --- Scenario 2: 't' then any other key (ESC) cancels, no change ----------
DUMP2="$OUT/capture_try_cancel.bin"
run_capture 12500000 '\p1t\p1\e' "$DUMP2"
echo ""
echo "'t' then ESC cancels -- canvas unchanged (still blank)"
check_bytes "cell (0,0) = c0 (cursor preview over blank, unchanged)" "0xBB80:1" "c0" "$DUMP2"

# --- Scenario 3: committed Try-mode plot is undoable with 'z' -------------
DUMP3="$OUT/capture_try_undo.bin"
run_capture 13500000 '\p1t\p1 \p1z' "$DUMP3"
echo ""
echo "'z' after a committed Try-mode plot undoes it"
check_bytes "cell (0,0) = c0 (undone back to blank)" "0xBB80:1" "c0" "$DUMP3"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
