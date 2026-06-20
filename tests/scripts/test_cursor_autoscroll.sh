#!/usr/bin/env bash
# tests/scripts/test_cursor_autoscroll.sh
#
# Phase 5a -- cursor auto-scroll smoke test (the `make test-cursor-autoscroll`
# target).
#
# Before Phase 5, OSE's main-mode cursor movement clamped to the 40x27
# viewport with no way to reach canvas cells beyond it -- a real gap, since
# Screen > Height/Width (Phase 2) already lets the canvas grow past the
# viewport. cursor_move_scroll() (src/canvas.c) fixes this: at a viewport
# edge, it scrolls app.xoffset/yoffset instead of refusing to move further,
# as long as the canvas extends further in that direction.
#
# This test: plots '@' at canvas row 1, resizes the canvas height to 28
# (Screen > Height, one row taller than the 27-row viewport), then presses
# DOWN 28 times from row 0. The first 26 presses move the cursor to the
# last viewport row (26); the 27th press has nowhere left to move the
# cursor, so it should scroll yoffset 0 -> 1 instead (the only room
# available, canvas_height - VIEWPORT_HEIGHT = 1); the 28th press should be
# a no-op (no further room). After scrolling, canvas row 1 (where '@' was
# plotted) becomes visible at screen row 0.
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \p1 = pause 1s (releases all keys). \fN = FUNCT+N. A \p1 MUST precede
#   every distinct key/combo action -- otherwise OSE's RELEASE_DEBOUNCE
#   blocks the next key indefinitely. cwin_textinput's cursor starts at the
#   end of "27"; \l\l moves it to idx=0, where typing '2','8' overwrites it
#   to "28".
#
# Required env vars (set by `make test-cursor-autoscroll`):
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
echo "  OricScreenEditorLOCI -- cursor auto-scroll test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# Plot 'A' at canvas row 1 (DOWN, SPACE, UP back to row0), resize height
# 27 -> 28 (FUNCT+1, ENTER opens Screen pulldown, DOWN selects Height:,
# ENTER opens its dialog, LEFT,LEFT to idx0, '2','8' overwrite "27"->"28",
# ENTER confirms, ESC closes the menu), then DOWN x28.
DOWNS28=""
for i in $(seq 1 28); do DOWNS28+='\p1\d'; done
KEYS="\\p1\\d\\p1 \\p1\\u\\p1\\f1\\p1\\n\\p1\\d\\p1\\n\\p1\\l\\p1\\l\\p12\\p18\\p1\\n\\p1\\e${DOWNS28}"

DUMP1="$OUT/capture_autoscroll_down.bin"
run_capture 65000000 "$KEYS" "$DUMP1"
echo ""
echo "Resize height 27->28, plot '@' at row1, DOWN x28 from row0"
check_bytes "row0 col0 now shows '@' (yoffset scrolled to 1)" "0xBB80:1" "40" "$DUMP1"
check_found "statusbar shows canvas-absolute row 27 (viewport row26 + yoffset1, XY 0,27)" "XY 0,27" "$DUMP1"

# Regression test for the statusbar XY coords bug (user-reported
# 2026-06-20): once the canvas is wider than the 40-col viewport and the
# view auto-scrolls right past column 40, the XY field used to freeze at
# the viewport-relative column (39) instead of continuing to count up
# with xoffset. Resize width 40 -> 41 (FUNCT+1, ENTER opens Screen
# pulldown, ENTER opens Width: dialog showing "40", LEFT,LEFT to idx0,
# '4','1' overwrite -> "41", ENTER confirms, ESC closes the menu), then
# RIGHT x41 from col0 -- the first 39 presses move the cursor to the
# last viewport column (39); the 40th has nowhere left to move, so it
# scrolls xoffset 0 -> 1 (canvas_width - VIEWPORT_WIDTH = 1); the 41st is
# a no-op (no further room).
RIGHTS41=""
for i in $(seq 1 41); do RIGHTS41+='\p1\r'; done
WKEYS="\\p1\\f1\\p1\\n\\p1\\n\\p1\\l\\p1\\l\\p14\\p11\\p1\\n\\p1\\e${RIGHTS41}"

DUMP2="$OUT/capture_autoscroll_right.bin"
run_capture 65000000 "$WKEYS" "$DUMP2"
echo ""
echo "Resize width 40->41, RIGHT x41 from col0"
check_found "statusbar shows canvas-absolute column 40 (viewport col39 + xoffset1, XY40,0)" "XY40, 0" "$DUMP2"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
