#!/usr/bin/env bash
# tests/scripts/test_findreplace.sh
#
# Unified Find/Replace ('f' in Main mode, src/findreplace.c) regression
# test (the `make test-findreplace` target).
#
# Covers: target selection (1=Code), find-only (ESC at the replace
# step jumps the cursor to the next match, canvas unchanged),
# replace-all (ENTER at the replace step rewrites every match and is
# undoable via 'z'), and ink-attribute recolor (target 2=Ink).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action. Both find/replace fields are pre-filled with a
#   non-empty default ("00" for code, "0" for ink/paper); \l\l moves the
#   cwin_textinput cursor to idx=0 so typed digits overwrite from the
#   start (same convention as resize_dialog()/goto_dialog()).
#
# Required env vars (set by `make test-findreplace`):
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
echo "  OricScreenEditorLOCI -- Find/Replace regression test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ESC at the target prompt cancels cleanly ------------------
DUMP1="$OUT/capture_fr_cancel.bin"
run_capture 11400000 '\p1f\p1\e' "$DUMP1"
echo ""
echo "ESC at the target prompt cancels with no residue"
check_found "back in Main, popup closed" "Main      XY 0, 0C41A S20I7P0S" "$DUMP1"

# --- Scenario 2: find-only jumps the cursor to the next match -------------
# Plot 'A' (screencode $41) at (0,0), move right 5 cols, then Find/Replace:
# target=Code, find=41 (hex), ESC at the replace step (find-only) -- cursor
# should jump back to (0,0) where the 'A' is.
DUMP2="$OUT/capture_fr_findonly.bin"
run_capture 25000000 \
    '\p1 \p1\r\p1\r\p1\r\p1\r\p1\r\p1f\p11\p1\l\p1\l\p14\p11\p1\n\p1\e' \
    "$DUMP2"
echo ""
echo "Find-only: cursor jumps from (5,0) back to (0,0) where 'A' is"
check_found "cursor at (0,0)" "Main      XY 0, 0C41A S41I7P0S" "$DUMP2"

# --- Scenario 3: replace-all rewrites every match --------------------------
# Plot 'A' at (0,0), Find/Replace: target=Code, find=41, replace=42 ('B'),
# ENTER confirms -- the cell becomes 'B'.
DUMP3="$OUT/capture_fr_replaceall.bin"
run_capture 24500000 \
    '\p1 \p1f\p11\p1\l\p1\l\p14\p11\p1\n\p1\l\p1\l\p14\p12\p1\n' \
    "$DUMP3"
echo ""
echo "Replace-all: 'A' (\$41) becomes 'B' (\$42) at (0,0)"
check_found "statusbar shows S42" "Main      XY 0, 0C41A S42I7P0S" "$DUMP3"

# --- Scenario 4: 'z' undoes the replace-all ---------------------------------
DUMP4="$OUT/capture_fr_undo.bin"
run_capture 25700000 \
    '\p1 \p1f\p11\p1\l\p1\l\p14\p11\p1\n\p1\l\p1\l\p14\p12\p1\n\p1z' \
    "$DUMP4"
echo ""
echo "'z' after replace-all restores the original 'A' (\$41)"
check_found "statusbar shows S41" "Main      XY 0, 0C41A S41I7P0S" "$DUMP4"

# --- Scenario 5: ink-attribute recolor (target 2=Ink) -----------------------
# 'i' plots the default plotink (7) at (0,0) and moves down a row. Find/
# Replace: target=Ink, find=7, replace=3 -- the ink attribute byte at (0,0)
# becomes $03.
DUMP5="$OUT/capture_fr_ink.bin"
run_capture 20000000 \
    '\p1i\p1f\p12\p1\l\p17\p1\n\p1\l\p13\p1\n' \
    "$DUMP5"
echo ""
echo "Ink recolor: attribute byte at (0,0) becomes \$03"
check_bytes "screen RAM (0,0) = 03" "0xBB80:1" "03" "$DUMP5"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
