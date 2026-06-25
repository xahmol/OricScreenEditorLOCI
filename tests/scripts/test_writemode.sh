#!/usr/bin/env bash
# tests/scripts/test_writemode.sh
#
# Write mode smoke test (the `make test-writemode` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and verifies Write mode (src/write.c, entered via 'w'):
#   - typing printable characters plots their screencode and advances the
#     cursor right
#   - FUNCT+1/2/3 plot ink/paper/modifier-attribute at the cursor and
#     advance right (the *same* actions as Main mode's 'i'/'o'/'u', but on
#     different keys since 'i'/'o'/'u' are themselves printable characters
#     to type here -- see write.c's header comment)
#   - ESC exits to Main
#
# NOT exercised here (no automated coverage possible): the CTRL+letter
# toggles (CTRL+B/A/D blink/altchar/double, CTRL+Z/X/C/V ink/paper cycling,
# CTRL+R reverse-video) and DEL (backspace-style: move left, then clear --
# see write.c's header comment, a 2026-06-20 departure from V1's
# in-place-clear). Phosphoric's --type-keys has no CTRL-modifier escape
# (only Shift, via uppercase letters) and DEL (0x7F) is unmapped in its
# char_map (see CLAUDE.md "Phosphoric testing notes") -- both are covered
# by the manual Oricutron/Phosphoric walkthrough in the original plan for this mode
# instead, and by code review (the bit-packing/cycling logic is identical
# to the already-tested Main-mode ','/'.'/';'/'\''/'b'/'d'/'a' handlers in
# src/editor.c; DEL reuses the already-tested cursor_move_scroll()).
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). \FN = FUNCT+N. A \p1 MUST precede
#   every distinct key/combo action -- otherwise OSE's RELEASE_DEBOUNCE
#   blocks the next key indefinitely.
#
# Required env vars (set by `make test-writemode`):
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
echo "  OricScreenEditorLOCI -- Write mode test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: typing "hi" plots+advances, ESC exits to Main ------------
DUMP1="$OUT/capture_write_type.bin"
run_capture 12480000 '\p1w\p1h\p1i\p1\e' "$DUMP1"
echo ""
echo "w,h,i,ESC types 'hi' and returns to Main at col2"
check_found "row0 shows 'hi'" "hi" "$DUMP1"
check_found "back in Main mode at col2" "Main      XY 2, 0" "$DUMP1"

# --- Scenario 2: FUNCT+1 plots ink (0x07) and advances right --------------
DUMP2="$OUT/capture_write_f1.bin"
run_capture 12480000 '\p1w\p1\F1\p1\e' "$DUMP2"
echo ""
echo "w,FUNCT+1,ESC plots ink attr at col0, cursor advances to col1"
check_bytes "col0 = 0x07 (ink attr)" "0xBB80:1" "07" "$DUMP2"
check_found "cursor at col1" "Main      XY 1, 0" "$DUMP2"

# --- Scenario 3: FUNCT+2 plots paper (0x10) -------------------------------
DUMP3="$OUT/capture_write_f2.bin"
run_capture 12480000 '\p1w\p1\F2\p1\e' "$DUMP3"
echo ""
echo "w,FUNCT+2,ESC plots paper attr at col0"
check_bytes "col0 = 0x10 (paper attr)" "0xBB80:1" "10" "$DUMP3"

# --- Scenario 4: FUNCT+3 plots the modifier attribute (0x08) --------------
DUMP4="$OUT/capture_write_f3.bin"
run_capture 12480000 '\p1w\p1\F3\p1\e' "$DUMP4"
echo ""
echo "w,FUNCT+3,ESC plots modifier attr at col0"
check_bytes "col0 = 0x08 (modifier attr, Std/non-double/non-blink)" "0xBB80:1" "08" "$DUMP4"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
