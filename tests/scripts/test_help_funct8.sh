#!/usr/bin/env bash
# tests/scripts/test_help_funct8.sh
#
# Phase 9d -- FUNCT+8 help screens smoke test (the `make test-help-funct8`
# target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and verifies help_show() (src/help.c) wired via FUNCT+8.
#
# Phosphoric's `\fN` (FUNCT+N) type-keys escape sends the 2-key combo as a
# single matrix-scan event that key_read()'s second (inner, blocking) call
# inside help_show() also picks up as a fresh keypress -- so a single \f8
# both opens AND immediately dismisses the help screen, all within one
# `\p1\f8` action. This means the "still showing" intermediate state can't
# be captured reliably from a type-keys script; what IS reliably testable
# (and is real regression coverage) is that the round trip is clean: no
# crash, no leftover help-screen content, and the caller (Main mode or the
# character editor popup) is exactly as it was before FUNCT+8.
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

# --- Scenario 1: Main mode FUNCT+8 round-trips cleanly ----------------------
DUMP1="$OUT/capture_help_main.bin"
run_capture 9300000 '\p1\f8' "$DUMP1"
echo ""
echo "Main mode FUNCT+8 shows then dismisses Help1.bin, no residue"
check_not_found "help text gone"   "Oric Screen Editor: Help" "$DUMP1"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP1"

# --- Scenario 2: Character editor FUNCT+8 round-trips, popup still open ----
DUMP2="$OUT/capture_help_charedit.bin"
run_capture 10500000 '\p1e\p1\f8' "$DUMP2"
echo ""
echo "Character editor FUNCT+8 round-trips, popup redrawn intact"
check_not_found "help text gone"     "Oric Screen Editor: Help" "$DUMP2"
check_found     "popup still open"   "Code:\$41"                "$DUMP2"
check_found     "grid redrawn"       "Set:Std"                  "$DUMP2"

# --- Scenario 3: ESC then closes the character editor as normal ------------
DUMP3="$OUT/capture_help_charedit_close.bin"
run_capture 11600000 '\p1e\p1\f8\p1\e' "$DUMP3"
echo ""
echo "ESC after FUNCT+8 closes the character editor cleanly"
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
