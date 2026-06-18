#!/usr/bin/env bash
# tests/scripts/test_undo_no_loci.sh
#
# Phase 8 -- undo/redo LOCI-absent path smoke test (the
# `make test-undo-no-loci` target).
#
# Phosphoric/Oricutron cannot emulate the Oric-side overlay RAM
# ($C000-$FFFF) undo.c needs, so the actual snapshot/restore mechanism
# can't be exercised headless -- that needs a real-hardware walkthrough
# (see the Phase 8 plan), the same constraint as Phases 6-7's LOCI-device
# features (note: that overlay RAM is a *different* resource from the
# LOCI-device XRAM the file picker uses, see CLAUDE.md "File picker
# (Phase 7)"). What *is* testable headless is that undo_snapshot() no-ops
# cleanly with no LOCI present (loci_present() correctly reports "absent"
# under Phosphoric/Oricutron, same mechanism test_fileio_no_loci.sh
# relies on) and that 'z'/'y' do nothing harmful when there is nothing
# recorded to undo/redo.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely.
#
# Required env vars (set by `make test-undo-no-loci`):
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

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- undo/redo LOCI-absent path test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: 'z' with nothing recorded is a silent no-op --------------
DUMP1="$OUT/capture_undo_z_empty.bin"
run_capture 10580000 '\p1z' "$DUMP1"
echo ""
echo "'z' with no prior edit does nothing"
check_bytes "canvas still blank (a0 = cursor-inverted blank)" "0xBB80:1" "a0" "$DUMP1"
check_found "still in Main mode" "Main      XY 0, 0" "$DUMP1"

# --- Scenario 2: plot, then z/y are no-ops without LOCI (canvas unchanged) -
DUMP2="$OUT/capture_undo_z_no_loci.bin"
run_capture 11680000 '\p1 \p1z\p1y' "$DUMP2"
echo ""
echo "SPACE plots 'A', then z,y do nothing (no LOCI -- undo unavailable)"
check_bytes "'A' still there (c1 = cursor-inverted 'A')" "0xBB80:1" "c1" "$DUMP2"
check_found "still in Main mode, no crash" "Main      XY 0, 0" "$DUMP2"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
