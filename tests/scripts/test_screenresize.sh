#!/usr/bin/env bash
# tests/scripts/test_screenresize.sh
#
# Phase 2 -- Screen > Width resize test (the `make test-screenresize` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, drives the Screen > Width resize dialog via --type-keys, and
# decodes the statusbar row of $BB80 dumps to assert:
#   - growing the canvas (40 -> 60) applies immediately, no confirmation
#   - shrinking back below the previous size (60 -> 40) shows a Yes/No
#     confirmation; selecting Yes applies the shrink
#   - selecting No on the same confirmation leaves the size unchanged
#
# Note: height is NOT separately exercised here. VIEWPORT_HEIGHT (27) equals
# the default canvas height, so resize_dialog()'s `minval` for height is 27
# -- any height *decrease* from the default is < minval and therefore
# invalid, meaning the shrink-confirm path can never be reached for height
# without first growing it. Width exercises the exact same shared
# resize_dialog()/menu_areyousure() code path, so it is sufficient.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \pN = pause N sec (releases all keys). A \p1 MUST precede every distinct
#   key/combo action -- otherwise OSE's RELEASE_DEBOUNCE blocks the next key.
#   cwin_textinput's cursor starts at the end of "40"/"60" (idx=2); \l\l
#   moves it to idx=0, where typing a digit overwrites the leading digit.
#
# Required env vars (set by `make test-screenresize`):
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
        -t "$SANDBOX/$TAPFILE" -f \
        --headless -c "$cycles" \
        --type-keys "8000000: $typekeys" \
        --dump-ram-at "$cycles:$dump" >/dev/null 2>&1
}

echo "==========================================================="
echo "  OricScreenEditorLOCI -- Screen > Width resize test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: grow 40 -> 60 (no confirmation needed) ---------------------
# FUNCT+1, ENTER (Screen pulldown), ENTER (Width dialog, shows "40"),
# left, left (cursor to idx 0), '6' (overwrite -> "60"), ENTER (confirm), ESC.
DUMP1="$OUT/capture_resize_grow.bin"
run_capture 17200000 '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\e' "$DUMP1"
echo ""
echo "Width 40 -> 60 (grow, applies immediately)"
check_not_found "bar gone" "Screen File  Charset  Information" "$DUMP1"

# Reopen the Screen pulldown (FUNCT+1, ENTER) to confirm canvas_width was
# actually applied -- the statusbar no longer shows canvas size (Phase 4
# repurposed the "S" field for the screencode under the cursor), so
# pulldown_titles[0][0] ("Width:  NN", via update_size_titles()) is now the
# only on-screen signal for the resized width.
DUMP1W="$OUT/capture_resize_grow_width.bin"
run_capture 19400000 '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\e\p1\f1\p1\n' "$DUMP1W"
check_found "Width pulldown shows 60" "Width:   60" "$DUMP1W"

# --- Scenario 2: grow 40 -> 60, then shrink 60 -> 40, confirm Yes -----------
# Repeats scenario 1, then reopens Screen pulldown, Width dialog (now "60"),
# left, left, '4' (overwrite -> "40"), ENTER (confirm, triggers shrink-confirm
# popup since 40 < 60), ENTER (selects "Yes", default highlight), ESC.
DUMP2="$OUT/capture_resize_shrink_yes.bin"
run_capture 25500000 \
    '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\n\p1\n\p1\l\p1\l\p14\p1\n\p1\n\p1\e' \
    "$DUMP2"
echo ""
echo "Width 60 -> 40 (shrink, confirm Yes -> applies)"
check_not_found "bar gone" "Screen File  Charset  Information" "$DUMP2"
check_not_found "confirm popup gone" "Yes" "$DUMP2"

# Reopen the Screen pulldown to confirm canvas_width is back to 40.
DUMP2W="$OUT/capture_resize_shrink_yes_width.bin"
run_capture 27800000 \
    '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\n\p1\n\p1\l\p1\l\p14\p1\n\p1\n\p1\e\p1\f1\p1\n' \
    "$DUMP2W"
check_found "Width pulldown shows 40" "Width:   40" "$DUMP2W"

# --- Scenario 3: grow 40 -> 60, then shrink 60 -> 40, confirm No ------------
# Same as scenario 2, but DOWN before the final ENTER selects "No" instead
# of the default-highlighted "Yes" -- the shrink must NOT be applied.
DUMP3="$OUT/capture_resize_shrink_no.bin"
run_capture 26000000 \
    '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\n\p1\n\p1\l\p1\l\p14\p1\n\p1\d\p1\n\p1\e' \
    "$DUMP3"
echo ""
echo "Width 60 -> 40 (shrink, confirm No -> size unchanged)"
check_not_found "bar gone" "Screen File  Charset  Information" "$DUMP3"
check_not_found "confirm popup gone" "Yes" "$DUMP3"

# Reopen the Screen pulldown to confirm canvas_width is still 60 (shrink
# was declined).
DUMP3W="$OUT/capture_resize_shrink_no_width.bin"
run_capture 28200000 \
    '\p1\f1\p1\n\p1\n\p1\l\p1\l\p16\p1\n\p1\n\p1\n\p1\l\p1\l\p14\p1\n\p1\d\p1\n\p1\e\p1\f1\p1\n' \
    "$DUMP3W"
check_found "Width pulldown shows 60" "Width:   60" "$DUMP3W"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
