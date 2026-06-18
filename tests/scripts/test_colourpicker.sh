#!/usr/bin/env bash
# tests/scripts/test_colourpicker.sh
#
# Phase 4c -- colour picker smoke test (the `make test-colourpicker` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, enters the colour picker via 'c' from main mode, and exercises
# the key bindings (src/colourpicker.c): title/feedback-line rendering,
# cursor wrap (LEFT/RIGHT cycle ink, UP/DOWN cycle paper, both wrap 0-7),
# ENTER commits app.plotink/app.plotpaper (statusbar I/P fields update), and
# ESC leaves them unchanged. Verified via $BB80 screen-text dumps
# (oric_screen.py --find) and raw screen-RAM byte dumps of the highlighted
# grid cell (oric_screen.py --bytes).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely. Cycle budget: 8,080,000 (boot+splash) +
#   N_pairs*1,100,000 + 300,000 margin.
#
# Grid layout (src/colourpicker.c): CP_GRID_X0=2, CP_CELL_STEP=4,
# CP_ROW_GRID0=1, popup at screen cols 2-37, rows 0-12. Cell (ink, paper) is
# 4 bytes at screen col (2 + CP_GRID_X0 + ink*CP_CELL_STEP), row
# (CP_ROW_GRID0 + paper): [ink-attr, paper-attr, swatch0, swatch1]. When
# highlighted, swatch0/1 = CH_INVSPACE(0xa0)/CH_SPACE(0x20); normal =
# CH_SPACE/CH_INVSPACE (0x20/0xa0).
#
# Required env vars (set by `make test-colourpicker`):
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
echo "  OricScreenEditorLOCI -- colour picker test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 'c' opens the popup on the default ink/paper (7/0) -------
# Cell (ink=7, paper=0): col = 2+2+7*4 = 32, row = 1 -> addr 0xBBC8.
DUMP1="$OUT/capture_cp_open.bin"
run_capture 9480000 '\p1c' "$DUMP1"
echo ""
echo "'c' opens colour picker on default ink=7/paper=0"
check_found "title shown" "Select ink and paper colour" "$DUMP1"
check_found "Ink: 7 feedback line"   "Ink:    7" "$DUMP1"
check_found "Paper: 0 feedback line" "Paper:  0" "$DUMP1"
check_found "Result: feedback line"  "Result:"   "$DUMP1"
check_bytes "cell (7,0) highlighted (ink-attr,paper-attr,inv,norm)" \
    "0xBBC8:4" "07 10 a0 20" "$DUMP1"

# --- Scenario 2: RIGHT wraps ink 7 -> 0 ------------------------------------
# Cell (ink=0, paper=0): col = 2+2+0*4 = 4, row = 1 -> addr 0xBBAC.
DUMP2="$OUT/capture_cp_right_wrap.bin"
run_capture 10580000 '\p1c\p1\r' "$DUMP2"
echo ""
echo "RIGHT wraps ink 7 -> 0"
check_found "Ink: 0 feedback line" "Ink:    0" "$DUMP2"
check_bytes "cell (0,0) now highlighted" "0xBBAC:4" "00 10 a0 20" "$DUMP2"
check_bytes "cell (7,0) back to normal" "0xBBC8:4" "07 10 20 a0" "$DUMP2"

# --- Scenario 3: UP wraps paper 0 -> 7 -------------------------------------
# Cell (ink=7, paper=7): col = 32, row = 1+7=8 -> addr 0xBCE0.
DUMP3="$OUT/capture_cp_up_wrap.bin"
run_capture 10580000 '\p1c\p1\u' "$DUMP3"
echo ""
echo "UP wraps paper 0 -> 7"
check_found "Paper: 7 feedback line" "Paper:  7" "$DUMP3"
check_bytes "cell (7,7) now highlighted" "0xBCE0:4" "07 17 a0 20" "$DUMP3"

# --- Scenario 4: ENTER commits the highlighted cell ------------------------
DUMP4="$OUT/capture_cp_enter.bin"
run_capture 11680000 '\p1c\p1\r\p1\n' "$DUMP4"
echo ""
echo "ENTER commits ink=0/paper=0 to app.plotink/plotpaper"
check_not_found "popup closed" "Select ink and paper colour" "$DUMP4"
check_found "statusbar shows I0P0" "Main      XY 0, 0C41A S20I0P0S" "$DUMP4"

# --- Scenario 5: ESC leaves plotink/plotpaper unchanged ---------------------
DUMP5="$OUT/capture_cp_esc.bin"
run_capture 11680000 '\p1c\p1\r\p1\e' "$DUMP5"
echo ""
echo "ESC leaves app.plotink/plotpaper unchanged (7/0)"
check_not_found "popup closed" "Select ink and paper colour" "$DUMP5"
check_found "statusbar shows I7P0" "Main      XY 0, 0C41A S20I7P0S" "$DUMP5"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
