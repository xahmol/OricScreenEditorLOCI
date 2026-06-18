#!/usr/bin/env bash
# tests/scripts/test_write_hexattr.sh
#
# Write mode hex-direct attribute entry (FUNCT+5, src/write.c
# write_hex_attr()) regression test (the `make test-write-hexattr`
# target).
#
# Picked FUNCT+5 over FUNCT+4 deliberately: include/keyboard.c's
# decode_funct[] (V1's own physical FUNCT-row mapping) binds KEY_F4 to
# FUNCT+R, not FUNCT+digit-4, so it cannot be driven by Phosphoric's
# --type-keys \fN escape (FUNCT+digit only). KEY_F5 is a genuine
# FUNCT+5 and is fully testable headless. See src/write.c's file header
# comment for the same note.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action. \f5 = FUNCT+5. The hex-value field is pre-filled
#   "0"; \l moves the cwin_textinput cursor to idx=0 so the typed digit
#   overwrites it (same convention as resize_dialog()/goto_dialog()).
#
# Required env vars (set by `make test-write-hexattr`):
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
echo "  OricScreenEditorLOCI -- Write hex-attribute entry test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: ESC at the target prompt cancels with no change ----------
DUMP1="$OUT/capture_writehex_cancel.bin"
run_capture 18000000 '\p1w\p1\f5\p1\e\p1\e' "$DUMP1"
echo ""
echo "ESC at the target prompt cancels, no plot, back in Main"
check_bytes "cell (0,0) unchanged (a0 = cursor-inverted blank)" "0xBB80:1" "a0" "$DUMP1"
check_found "back in Main mode at col0" "Main      XY 0, 0" "$DUMP1"

# --- Scenario 2: target=Ink, value=5 -> attribute byte 0x05 ----------------
DUMP2="$OUT/capture_writehex_ink.bin"
run_capture 23500000 '\p1w\p1\f5\p11\p1\l\p15\p1\n\p1\e' "$DUMP2"
echo ""
echo "Target=Ink, value=5 plots attribute 0x05, cursor advances to col1"
check_bytes "cell (0,0) = 05" "0xBB80:1" "05" "$DUMP2"
check_found "cursor at col1" "Main      XY 1, 0" "$DUMP2"

# --- Scenario 3: target=Paper, value=3 -> attribute byte 0x13 --------------
DUMP3="$OUT/capture_writehex_paper.bin"
run_capture 23500000 '\p1w\p1\f5\p12\p1\l\p13\p1\n\p1\e' "$DUMP3"
echo ""
echo "Target=Paper, value=3 plots attribute 0x13"
check_bytes "cell (0,0) = 13" "0xBB80:1" "13" "$DUMP3"

# --- Scenario 4: target=Modifier, value=5 (altchar+blink) -> 0x0D ----------
DUMP4="$OUT/capture_writehex_mod.bin"
run_capture 23500000 '\p1w\p1\f5\p13\p1\l\p15\p1\n\p1\e' "$DUMP4"
echo ""
echo "Target=Modifier, value=5 (altchar+blink bits) plots attribute 0x0D"
check_bytes "cell (0,0) = 0d" "0xBB80:1" "0d" "$DUMP4"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
