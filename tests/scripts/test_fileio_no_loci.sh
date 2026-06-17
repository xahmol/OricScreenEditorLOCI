#!/usr/bin/env bash
# tests/scripts/test_fileio_no_loci.sh
#
# Phase 6 -- LOCI-absent path smoke test (the `make test-fileio-no-loci`
# target).
#
# Phosphoric/Oricutron cannot emulate a LOCI device, so none of src/
# fileio.c's actual file I/O (load/save bytes) can be exercised headless --
# that needs a real-hardware walkthrough (see the Phase 6 plan), the same
# constraint as the colour picker's hardware-rendering issue. What *is*
# testable headless is the LOCI-absent gate itself: loci_present() reads
# $0319 directly (include/loci.c) and correctly reports "absent" under
# Phosphoric/Oricutron, so every one of the 12 File/Charset menu items
# should hit loci_check_present() first and show the graceful
# MSG_LOCI_NOT_FOUND popup rather than attempting (and presumably hanging
# or erroring on) any LOCI call.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \fN = FUNCT+N, \pN = pause N sec (releases all keys). A \p1 MUST precede
#   every distinct key/combo action -- otherwise OSE's RELEASE_DEBOUNCE
#   blocks the next key indefinitely.
#
# Menu navigation: FUNCT+1 opens the bar (Screen highlighted); \r moves to
# File (1 right) or Charset (2 rights); \n opens that pulldown; \d (DOWN)
# moves down to the Nth item (N-1 downs); final \n selects/dispatches.
# pulldown_titles order (src/menudata.c): File = Save/Load Screen, Save/
# Load Project, Save/Load Combined (items 1-6); Charset = Load/Save
# Standard, Load/Save Alternate, Load/Save Combined (items 1-6).
#
# Required env vars (set by `make test-fileio-no-loci`):
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

run_capture() {
    local cycles="$1" typekeys="$2" dump="$3"
    "$PHOS" -r "$ATMOSROM" \
        -t "$SANDBOX/$TAPFILE" -f \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- LOCI-absent path test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

downs() {
    local n="$1" out=""
    for i in $(seq 1 "$n"); do out+='\p1\d'; done
    echo "$out"
}

# --- File menu: 6 items, bar = Screen -> 1x RIGHT -----------------------
FILE_LABELS=("Save screen" "Load screen" "Save project" "Load project" "Save combined" "Load combined")
for i in 0 1 2 3 4 5; do
    n=$((i+1))
    keys="\\p1\\f1\\p1\\r\\p1\\n$(downs $i)\\p1\\n"
    cycles=$((14280000 + i*1300000))
    dump="$OUT/capture_fileio_file_$n.bin"
    run_capture "$cycles" "$keys" "$dump"
    check_found "File > ${FILE_LABELS[$i]} shows LOCI-not-found" "No LOCI device detected" "$dump"
done

# --- Charset menu: 6 items, bar = Screen -> 2x RIGHT ---------------------
CHARSET_LABELS=("Load standard" "Load alternate" "Save standard" "Save alternate" "Load combined" "Save combined")
for i in 0 1 2 3 4 5; do
    n=$((i+1))
    keys="\\p1\\f1\\p1\\r\\p1\\r\\p1\\n$(downs $i)\\p1\\n"
    cycles=$((15580000 + i*1300000))
    dump="$OUT/capture_fileio_charset_$n.bin"
    run_capture "$cycles" "$keys" "$dump"
    check_found "Charset > ${CHARSET_LABELS[$i]} shows LOCI-not-found" "No LOCI device detected" "$dump"
done

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
