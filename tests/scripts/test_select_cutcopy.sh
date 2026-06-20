#!/usr/bin/env bash
# tests/scripts/test_select_cutcopy.sh
#
# Phase 8c -- Select mode cut/copy smoke test (the `make test-select-cutcopy`
# target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric and verifies select_run()'s 'x'/'c' actions (src/select.c):
# plots a marker character, selects it (1x1 rect), then copies or cuts it
# three columns to the right, checking:
#   - copy leaves the source intact and places a copy at the destination
#   - cut blanks the source and places the moved content at the destination
#   - a destination that doesn't fit shows MSG_SELECT_NOFIT and leaves the
#     canvas unchanged
#   - ESC at the destination-picking step cancels cleanly, canvas unchanged
#
# This is fully testable headless (unlike most of Phase 6-8's LOCI/overlay-
# RAM features): cut/copy only needs canvas_rowbuf[] (main RAM), no LOCI
# device at all -- see CLAUDE.md "Select mode cut/copy (Phase 8)".
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next
#   key indefinitely.
#
# Required env vars (set by `make test-select-cutcopy`):
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
echo "  OricScreenEditorLOCI -- Select mode cut/copy test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: copy '@' from col0 to col3, source unchanged ------------
DUMP1="$OUT/capture_cutcopy_copy.bin"
run_capture 17200000 '\p1 \p1s\p1\n\p1c\p1\r\p1\r\p1\r\p1\n' "$DUMP1"
echo ""
echo "SPACE plots '@', s,ENTER,c,RIGHTx3,ENTER copies it to col3"
check_bytes "col0 still '@' (40), col3 now '@' under cursor (c0)" "0xBB80:4" "40 20 20 c0" "$DUMP1"

# --- Scenario 2: cut '@' from col0 to col3, source blanked ----------------
DUMP2="$OUT/capture_cutcopy_cut.bin"
run_capture 17200000 '\p1 \p1s\p1\n\p1x\p1\r\p1\r\p1\r\p1\n' "$DUMP2"
echo ""
echo "SPACE plots '@', s,ENTER,x,RIGHTx3,ENTER cuts it to col3"
check_bytes "col0 now blank (20), col3 has the moved '@' under cursor (c0)" "0xBB80:4" "20 20 20 c0" "$DUMP2"

# --- Scenario 3: destination doesn't fit -----------------------------------
DOWNS38=""
for i in $(seq 1 38); do DOWNS38+='\p1\r'; done
DUMP3="$OUT/capture_cutcopy_nofit.bin"
run_capture 60000000 "\\p1s\\p1\\d\\p1\\d\\p1\\r\\p1\\r\\p1\\n\\p1c${DOWNS38}\\p1\\n" "$DUMP3"
echo ""
echo "3x3 select,copy, move destination 38 cols right -- doesn't fit"
check_found "MSG_SELECT_NOFIT shown" "Selection does not fit" "$DUMP3"

# --- Scenario 4: ESC at destination pick cancels cleanly --------------------
DUMP4="$OUT/capture_cutcopy_esc.bin"
run_capture 17200000 '\p1 \p1s\p1\n\p1c\p1\r\p1\r\p1\r\p1\e' "$DUMP4"
echo ""
echo "SPACE plots '@', s,ENTER,c,RIGHTx3,ESC cancels"
check_bytes "col0 still '@' (40), col3 still blank under cursor (c0)" "0xBB80:4" "40 20 20 c0" "$DUMP4"
check_found "back in Main mode" "Main      XY 3, 0" "$DUMP4"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
