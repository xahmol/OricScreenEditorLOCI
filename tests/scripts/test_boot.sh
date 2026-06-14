#!/usr/bin/env bash
# tests/scripts/test_boot.sh
#
# Phase 1 -- boot smoke test (the `make test-boot` / `make test` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and decodes the $BB80 screen-text dump to assert:
#   - the splash screen renders (title + version) before any key is pressed
#   - after a key is pressed, the splash is dismissed and the blank canvas
#     + statusbar render correctly
#
# Required env vars (set by `make test-boot`):
#   PHOS      path to oric1-emu
#   ATMOSROM  path to roms/basic11b.rom
#   SANDBOX   tests/sandbox (freshly built .tap)
#   OUT       tests/out (scratch dir for RAM dumps)
#   TAPFILE   oseloci.tap

set -u
cd "$(dirname "$0")/../.." || exit 1

SCREEN=tests/scripts/oric_screen.py

# Calibrated cycle counts (see `make test-capture` for re-calibration).
SPLASH_CYCLES=8000000
DISMISS_AT=8000000
MAIN_CYCLES=9000000

SPLASH_DUMP="$OUT/capture_splash.bin"
MAIN_DUMP="$OUT/capture_main.bin"

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

echo "==========================================================="
echo "  OricScreenEditorLOCI -- boot smoke test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f \
    --headless -c $SPLASH_CYCLES \
    --dump-ram-at $SPLASH_CYCLES:"$SPLASH_DUMP" >/dev/null 2>&1

"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f \
    --headless -c $MAIN_CYCLES \
    --type-keys "$DISMISS_AT: " \
    --dump-ram-at $MAIN_CYCLES:"$MAIN_DUMP" >/dev/null 2>&1

if [ ! -f "$SPLASH_DUMP" ] || [ ! -f "$MAIN_DUMP" ]; then
    echo "  [FAIL] emulator did not produce expected RAM dumps"
    fail=$((fail+1))
else
    echo ""
    echo "Splash screen"
    check_found "title renders"   "OricScreenEditor for LOCI" "$SPLASH_DUMP"
    check_found "version renders" "Oscar64 build v0.1.0"      "$SPLASH_DUMP"
    check_found "prompt renders"  "Press any key to start"    "$SPLASH_DUMP"

    echo ""
    echo "Main mode (after dismiss)"
    check_not_found "splash dismissed"     "OricScreenEditor for LOCI" "$MAIN_DUMP"
    check_found     "statusbar renders"    "Main  X:00 Y:00  C:\$41  S:40x27" "$MAIN_DUMP"
fi

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
