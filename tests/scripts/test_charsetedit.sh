#!/usr/bin/env bash
# tests/scripts/test_charsetedit.sh
#
# Phase 3b -- character editor smoke test (the `make test-charsetedit`
# target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, enters the character editor via 'e' from main mode, and
# exercises a representative subset of the README key bindings
# (src/charsetedit.c), asserting via $BB80 screen-text dumps and raw
# charset-RAM byte dumps ($B608 = charset_address(0x41, 0), the 'A' glyph
# in the standard charset).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely. Cycle budget: 8,080,000 (boot+splash) +
#   N_pairs*1,100,000 + 300,000 margin.
#
# Required env vars (set by `make test-charsetedit`):
#   PHOS      path to oric1-emu
#   ATMOSROM  path to roms/basic11b.rom
#   SANDBOX   tests/sandbox (freshly built .tap)
#   OUT       tests/out (scratch dir for RAM dumps)
#   TAPFILE   oseloci.tap

set -u
cd "$(dirname "$0")/../.." || exit 1

SCREEN=tests/scripts/oric_screen.py

# charset_address(0x41, 0) = CHARSET_STD + 0x41*8 = 0xB608 (see src/charsetedit.c)
ADDR_LEN="0xB608:8"
ORIG_BYTES="08 14 22 22 3e 22 22 00"

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
    local label="$1" expected="$2" dump="$3"
    local actual
    actual=$(python3 "$SCREEN" "$dump" --bytes "$ADDR_LEN")
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
        -t "$SANDBOX/$TAPFILE" -f \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- character editor test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 'e' opens the popup on the current plot char ('A') -------
DUMP1="$OUT/capture_ce_open.bin"
run_capture 9480000 '\p1e' "$DUMP1"
echo ""
echo "'e' opens character editor on 'A' (\$41, Std)"
check_found "header shows Code:\$41 Set:Std" "Character editor   Code:\$41  Set:Std" "$DUMP1"
check_found "favourites label shown"         "Favourites:"                          "$DUMP1"
check_found "grid shows 'A' crossbar"         "#####"                                "$DUMP1"

# --- Scenario 2: SPACE toggles the pixel under the cursor (0,0) -----------
DUMP2="$OUT/capture_ce_space.bin"
run_capture 11680000 '\p1e\p1 \p1\e' "$DUMP2"
echo ""
echo "SPACE toggles pixel at cursor (0,0), ESC commits"
check_bytes "row0 0x08 -> 0x28 (bit5 set)" "28 14 22 22 3e 22 22 00" "$DUMP2"
check_not_found "popup closed cleanly" "Character editor" "$DUMP2"

# --- Scenario 3: i (invert) then z (undo) round-trips to original --------
DUMP3="$OUT/capture_ce_invert_undo.bin"
run_capture 12780000 '\p1e\p1i\p1z\p1\e' "$DUMP3"
echo ""
echo "i (invert) then z (undo) restores the original glyph"
check_bytes "glyph unchanged after invert+undo" "$ORIG_BYTES" "$DUMP3"

# --- Scenario 4: y (mirror vertical) reverses the row order --------------
DUMP4="$OUT/capture_ce_mirror_y.bin"
run_capture 11680000 '\p1e\p1y\p1\e' "$DUMP4"
echo ""
echo "y mirrors the glyph vertically (row order reversed)"
check_bytes "rows reversed" "00 22 22 3e 22 22 14 08" "$DUMP4"

# --- Scenario 5: x (mirror horizontal) bit-reverses each row -------------
DUMP5="$OUT/capture_ce_mirror_x.bin"
run_capture 11680000 '\p1e\p1x\p1\e' "$DUMP5"
echo ""
echo "x mirrors the glyph horizontally (bits reversed per row)"
check_bytes "bits reversed" "04 0a 11 11 1f 11 11 00" "$DUMP5"

# --- Scenario 6: +/-/=  cycle the screencode, @ stores a favourite -------
# +: $41 -> $42 ('B'); @ (SHIFT+2) stores $42 into favourites[2]
# (initially '!' = $21); -: $42 -> $41; 2 selects favourites[2] -> $42
DUMP6="$OUT/capture_ce_favourites.bin"
run_capture 14980000 '\p1e\p1+\p1@\p1-\p12\p1\e' "$DUMP6"
echo ""
echo "+/-  cycle the screencode; @ stores, 2 recalls a favourite"
check_found "ESC commits recalled code to plotscreencode (\$42)" \
    'Main  X:00 Y:00  C:$42  S:40x27' "$DUMP6"

# --- Scenario 7: a toggles Std/Alt charset ---------------------------------
DUMP7="$OUT/capture_ce_altset.bin"
run_capture 10580000 '\p1e\p1a' "$DUMP7"
echo ""
echo "a toggles the active charset bank (Std -> Alt)"
check_found "header shows Set:Alt" "Character editor   Code:\$41  Set:Alt" "$DUMP7"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
