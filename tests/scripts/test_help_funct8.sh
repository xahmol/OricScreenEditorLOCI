#!/usr/bin/env bash
# tests/scripts/test_help_funct8.sh
#
# Phase 9d -- FUNCT+8 help screens smoke test (the `make test-help-funct8`
# target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and verifies help_show() (src/help.c) wired via FUNCT+8.
#
# Rewritten after fixing include/keyboard.c's decode_funct[] table (see
# CLAUDE.md "FUNCT+digit keys now match real hardware exactly"): an
# earlier version of this script assumed a single `\f8` action both
# opens AND immediately dismisses the help screen within one matrix
# event. That assumption was actually a misdiagnosis -- under the old,
# buggy decode_funct[] table, FUNCT+(physical digit 8) decoded to
# nothing at all (a silent no-op), which looked like "shown then
# dismissed" only because no help text ever appeared and the statusbar
# never changed -- exactly what a genuine no-op also produces. Now that
# FUNCT+8 actually opens help (confirmed directly: a single `\f8` leaves
# help showing indefinitely, even at 30M+ cycles), every scenario below
# sends an explicit second key to dismiss it.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \fN = FUNCT+N, \pN = pause N sec (releases all keys). A \p1 MUST precede
#   every distinct key/combo action -- otherwise OSE's RELEASE_DEBOUNCE
#   blocks the next key indefinitely.
#
# Required env vars (set by `make test-help-funct8`):
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

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f --loci \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- FUNCT+8 help screens test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 0: FUNCT+8 alone genuinely opens help (stays open) -----------
# Regression test for the decode_funct[] fix itself: confirms FUNCT+8
# (the real physical digit-8 combo) opens help and it stays open with no
# further input, rather than silently doing nothing.
DUMP0="$OUT/capture_help_main_stays_open.bin"
run_capture 20000000 '\p1\f8' "$DUMP0"
echo ""
echo "FUNCT+8 alone opens help and it stays open (no auto-dismiss)"
check_found "help text shown" "Oric Screen Editor: Help" "$DUMP0"

# --- Scenario 1: Main mode FUNCT+8 then ESC round-trips cleanly ------------
DUMP1="$OUT/capture_help_main.bin"
run_capture 12000000 '\p1\f8\p1\e' "$DUMP1"
echo ""
echo "Main mode FUNCT+8 then ESC dismisses Help1.bin, no residue"
check_not_found "help text gone"   "Oric Screen Editor: Help" "$DUMP1"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP1"

# --- Scenario 2: Character editor FUNCT+8 then ESC, popup still open ------
DUMP2="$OUT/capture_help_charedit.bin"
run_capture 16000000 '\p1e\p1\f8\p1\e' "$DUMP2"
echo ""
echo "Character editor FUNCT+8 then ESC dismisses help, popup redrawn intact"
check_not_found "help text gone"     "Oric Screen Editor: Help" "$DUMP2"
check_found     "popup still open"   "Code:\$41"                "$DUMP2"
check_found     "grid redrawn"       "Set:Std"                  "$DUMP2"

# --- Scenario 3: a further ESC then closes the character editor as normal -
DUMP3="$OUT/capture_help_charedit_close.bin"
run_capture 18000000 '\p1e\p1\f8\p1\e\p1\e' "$DUMP3"
echo ""
echo "A further ESC after dismissing help closes the character editor"
check_not_found "popup gone"       "Code:\$"                  "$DUMP3"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP3"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
