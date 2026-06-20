#!/usr/bin/env bash
# tests/scripts/test_select.sh
#
# Select mode smoke test (the `make test-select` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, enters Select mode via 's' from Main mode, grows a 3x3
# rectangle from the origin cell (0,0) using DOWN,DOWN,RIGHT,RIGHT, and
# verifies (src/select.c select_run()):
#   - ESC at the rect-growing stage cancels with no action prompt
#   - ENTER accepts the rect, then 'd'/'i'/'p'/'m' each fill it (space/
#     ink/paper/modifier-attribute)
#   - ESC at the action prompt leaves the canvas unchanged
#
# Cut/copy ('x'/'c') are exercised separately, in test_select_cutcopy.sh.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely.
#
# Required env vars (set by `make test-select`):
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
echo "  OricScreenEditorLOCI -- Select mode test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ESC at the rect stage cancels with no prompt --------------
DUMP1="$OUT/capture_select_cancel_rect.bin"
run_capture 16000000 '\p1s\p1\d\p1\d\p1\r\p1\r\p1\e' "$DUMP1"
echo ""
echo "s,DOWN,DOWN,RIGHT,RIGHT,ESC cancels at rect stage, no fill"
check_bytes "row0 cols0-2 still blank" "0xBB80:3" "20 20 20" "$DUMP1"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP1"

# --- Scenario 1b: 'd' clears a single-cell rect to CH_SPACE ----------------
DUMP1B="$OUT/capture_select_d.bin"
run_capture 13980000 '\p1 \p1s\p1\n\p1d' "$DUMP1B"
echo ""
echo "SPACE plots '@', s,ENTER (1x1 rect),d clears it back to space"
check_found "statusbar S field shows 20 (cleared)" "S20I7" "$DUMP1B"

# --- Scenario 2: 'i' fills the accepted rect with app.plotink (7) ----------
DUMP2="$OUT/capture_select_i.bin"
run_capture 18100000 '\p1s\p1\d\p1\d\p1\r\p1\r\p1\n\p1i' "$DUMP2"
echo ""
echo "s,DOWN,DOWN,RIGHT,RIGHT,ENTER,i fills the 3x3 rect with ink=7"
check_bytes "row0 cols0-2 = 07 07 07" "0xBB80:3" "07 07 07" "$DUMP2"
check_bytes "row1 cols0-2 = 07 07 07" "0xBBA8:3" "07 07 07" "$DUMP2"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP2"

# --- Scenario 3: 'p' fills the accepted rect with 16+app.plotpaper (16) ---
DUMP3="$OUT/capture_select_p.bin"
run_capture 18100000 '\p1s\p1\d\p1\d\p1\r\p1\r\p1\n\p1p' "$DUMP3"
echo ""
echo "s,DOWN,DOWN,RIGHT,RIGHT,ENTER,p fills the 3x3 rect with paper=0 (0x10)"
check_bytes "row0 cols0-2 = 10 10 10" "0xBB80:3" "10 10 10" "$DUMP3"

# --- Scenario 4: 'm' fills the accepted rect with the modifier attr -------
DUMP4="$OUT/capture_select_m.bin"
run_capture 18100000 '\p1s\p1\d\p1\d\p1\r\p1\r\p1\n\p1m' "$DUMP4"
echo ""
echo "s,DOWN,DOWN,RIGHT,RIGHT,ENTER,m fills the 3x3 rect with modifier 0x08"
check_bytes "row0 cols0-2 = 08 08 08" "0xBB80:3" "08 08 08" "$DUMP4"

# --- Scenario 5: ESC at the action prompt leaves the canvas unchanged -----
DUMP5="$OUT/capture_select_esc_prompt.bin"
run_capture 18100000 '\p1s\p1\d\p1\d\p1\r\p1\r\p1\n\p1\e' "$DUMP5"
echo ""
echo "s,DOWN,DOWN,RIGHT,RIGHT,ENTER,ESC leaves the rect unfilled"
check_bytes "row0 cols0-2 still blank" "0xBB80:3" "20 20 20" "$DUMP5"
check_found "back in Main mode" "Main      XY 2, 2" "$DUMP5"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
