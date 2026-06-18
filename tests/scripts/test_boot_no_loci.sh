#!/usr/bin/env bash
# tests/scripts/test_boot_no_loci.sh
#
# LOCI-required boot gate smoke test (the `make test-boot-no-loci`
# target).
#
# LOCI became mandatory when screenmap[] (the canvas, src/canvas.h)
# moved into Oric-side overlay RAM ($C000-$FFFF) -- there is no main-RAM
# fallback any more, so src/main.c hard-gates on loci_present() right
# after charwin_init(), before even the title splash. This replaces the
# *purpose* of the old test_fileio_no_loci.sh/test_undo_no_loci.sh
# (which tested individual menu/undo actions gracefully degrading with no
# LOCI present) -- those scenarios are no longer reachable, since Main
# mode itself is never reached without LOCI. What's testable now is the
# gate itself: no `--loci` flag -> MSG_LOCI_NOT_FOUND + MSG_PRESS_KEY_EXIT
# shown, any key exits cleanly, Main mode never renders.
#
# Required env vars (set by `make test-boot-no-loci`):
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

echo "==========================================================="
echo "  OricScreenEditorLOCI -- LOCI-required boot gate test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: no --loci -> gate message shown, no Main mode -------------
DUMP1="$OUT/capture_boot_no_loci.bin"
"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f \
    --headless -c 9000000 \
    --dump-ram-at 9000000:"$DUMP1" >/dev/null 2>&1
echo ""
echo "No LOCI device: gate message shown"
check_found "not-found message"  "No LOCI device detected" "$DUMP1"
check_found "press-key-exit msg" "Press any key to exit"   "$DUMP1"
check_not_found "Main mode never reached" "Main      XY" "$DUMP1"

# --- Scenario 2: a keypress exits cleanly (no crash/hang) ------------------
DUMP2="$OUT/capture_boot_no_loci_dismiss.bin"
"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f \
    --headless -c 10000000 \
    --type-keys "9000000: \p1 " \
    --dump-ram-at 10000000:"$DUMP2" >/dev/null 2>&1
echo ""
echo "A keypress exits cleanly"
check_not_found "Main mode never reached" "Main      XY" "$DUMP2"

# --- Scenario 3: with --loci, the gate does not fire ------------------------
DUMP3="$OUT/capture_boot_with_loci.bin"
"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f --loci \
    --headless -c 9000000 \
    --dump-ram-at 9000000:"$DUMP3" >/dev/null 2>&1
echo ""
echo "With --loci, the splash shows instead of the gate"
check_not_found "no gate message" "No LOCI device detected" "$DUMP3"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
