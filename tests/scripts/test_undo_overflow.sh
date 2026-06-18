#!/usr/bin/env bash
# tests/scripts/test_undo_overflow.sh
#
# Undo-overflow regression test (the `make test-undo-overflow` target).
#
# Before the canvas moved into overlay RAM (CANVAS_MAX_SIZE=10240),
# CANVAS_MAX_SIZE was always < UNDO_REGION_SIZE (16384), so a single
# undo_snapshot() call could never exceed the undo region -- the
# wrap-to-start logic in src/undo.c's undo_snapshot() was never
# exercised with bytes > UNDO_REGION_SIZE. Once undo's region shrank to
# 6144 bytes (the other half of $C000-$FFFF, canvas gets the first
# 10240), a Screen > Clear/Fill on a canvas bigger than 6144 cells can
# exceed it -- and the old wrap logic would write past $FFFF, wrapping a
# 16-bit pointer into zero page. Fixed with an explicit
# `if (bytes > UNDO_REGION_SIZE) return;` guard in undo_snapshot() (see
# that function and its file header comment) -- this is the regression
# test for that fix: confirms the oversized snapshot is skipped
# gracefully (no crash, no corruption) and that normal undo on smaller
# edits keeps working on the same oversized canvas.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). A \p1 MUST precede every distinct
#   key/combo action. cwin_textinput's cursor starts at the end of "27"
#   (idx=2); \l\l moves it to idx=0, where typing digits overwrites/
#   extends the field ("27" -> "200").
#
# Required env vars (set by `make test-undo-overflow`):
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
echo "  OricScreenEditorLOCI -- undo-overflow regression test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

RESIZE='\p1\f1\p1\n\p1\d\p1\n\p1\l\p1\l\p12\p10\p10\p1\n\p1\e'

# --- Scenario 1: grow canvas height to 200 (40x200=8000 > 6144) ------------
DUMP1="$OUT/capture_undo_overflow_resize.bin"
run_capture 21600000 "$RESIZE" "$DUMP1"
echo ""
echo "Screen > Height resized to 200 (canvas now 8000 cells)"
check_not_found "bar gone" "Screen File  Charset  Information" "$DUMP1"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP1"

# --- Scenario 2: Screen > Clear on the oversized canvas, no crash ----------
DUMP2="$OUT/capture_undo_overflow_clear.bin"
run_capture 29600000 "$RESIZE"'\p1\f1\p1\n\p1\d\p1\d\p1\n\p1\e' "$DUMP2"
echo ""
echo "Screen > Clear on the oversized canvas completes cleanly"
check_not_found "bar gone" "Screen File  Charset  Information" "$DUMP2"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP2"

# --- Scenario 3: 'z' after the oversized Clear is a graceful no-op --------
DUMP3="$OUT/capture_undo_overflow_z.bin"
run_capture 30700000 "$RESIZE"'\p1\f1\p1\n\p1\d\p1\d\p1\n\p1\e\p1z' "$DUMP3"
echo ""
echo "'z' after the oversized Clear -- no crash, no corruption"
check_not_found "bar gone"        "Screen File  Charset  Information" "$DUMP3"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP3"

# --- Scenario 4: normal undo still works on the same oversized canvas -----
DUMP4="$OUT/capture_undo_overflow_normal.bin"
run_capture 32900000 "$RESIZE"'\p1\f1\p1\n\p1\d\p1\d\p1\n\p1\e\p1 \p1z' "$DUMP4"
echo ""
echo "SPACE plots 'A' then z undoes it -- small edits stay undoable"
check_bytes "cell (0,0) undone (a0 = cursor-inverted blank)" "0xBB80:1" "a0" "$DUMP4"
check_found "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP4"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
