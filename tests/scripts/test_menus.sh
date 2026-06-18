#!/usr/bin/env bash
# tests/scripts/test_menus.sh
#
# Phase 2 -- menu bar smoke test (the `make test-menus` target).
#
# Fast-loads the freshly built oseloci.tap under Atmos BASIC 1.1 in
# Phosphoric, uses --type-keys to drive the menu bar via FUNCT+1, and
# decodes $BB80 screen-text dumps to assert:
#   - the menu bar opens and shows all 4 top-level items
#   - the Screen pulldown shows Width/Height/Clear/Fill
#   - ESC at pulldown level then bar level closes the menu with no residue
#   - Information > Version shows its 3 pages (title image, version/credits
#     text, QR code) and returns cleanly to the bar/Main mode
#   - the Fill dispatch actually fills the canvas
#
# --type-keys notes (see CLAUDE.md "Phosphoric testing notes"):
#   \fN = FUNCT+N, \pN = pause N sec (releases all keys). A \p1 MUST precede
#   every distinct key/combo action -- otherwise OSE's RELEASE_DEBOUNCE
#   blocks the next key indefinitely.
#
# Required env vars (set by `make test-menus`):
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
echo "  OricScreenEditorLOCI -- menu bar test"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

# --- Scenario 1: menu bar opens (FUNCT+1) ---------------------------------
DUMP1="$OUT/capture_menu_bar.bin"
run_capture 9300000 '\p1\f1' "$DUMP1"
echo ""
echo "Bar opens (FUNCT+1)"
check_found "bar shows all 4 items" "Screen File  Charset  Information" "$DUMP1"

# --- Scenario 2: Screen pulldown content -----------------------------------
DUMP2="$OUT/capture_menu_screen_pulldown.bin"
run_capture 10500000 '\p1\f1\p1\n' "$DUMP2"
echo ""
echo "Screen pulldown content (ENTER on bar)"
check_found "Width item shown"  "Width:   40" "$DUMP2"
check_found "Height item shown" "Height:  27" "$DUMP2"
check_found "Clear item shown"  "Clear"       "$DUMP2"
check_found "Fill item shown"   "Fill"        "$DUMP2"

# --- Scenario 3: ESC-ESC clean close ----------------------------------------
DUMP3="$OUT/capture_menu_esc_close.bin"
run_capture 12700000 '\p1\f1\p1\n\p1\e\p1\e' "$DUMP3"
echo ""
echo "ESC (pulldown) + ESC (bar) closes with no residue"
check_not_found "bar gone"       "Screen" "$DUMP3"
check_found     "statusbar intact" "Main      XY 0, 0C41A S20I7P0S" "$DUMP3"

# --- Scenario 4: Information > Version, page 1 (title image) ---------------
# (File's items were wired to real LOCI dispatch as of Phase 6 -- src/fileio.c
# -- and Information's Version/Exit were wired up in Phase 9c -- src/info.c
# -- so this scenario, which used to land on a permanent stub, now exercises
# the real 3-page Version popup.)
DUMP4="$OUT/capture_menu_version_p1.bin"
run_capture 15500000 '\p1\f1\p1\r\p1\r\p1\r\p1\n\p1\n' "$DUMP4"
echo ""
echo "Information > Version shows page 1 (title image, replaces the bar)"
check_not_found "bar gone (title image showing)" "Screen File  Charset  Information" "$DUMP4"

# --- Scenario 5: page 2 (version/credits text) ------------------------------
DUMP5="$OUT/capture_menu_version_p2.bin"
run_capture 16600000 '\p1\f1\p1\r\p1\r\p1\r\p1\n\p1\n\p1\n' "$DUMP5"
echo ""
echo "Page 2 shows version/credits text"
check_found "title line shown"     "OricScreenEditorLOCI"                "$DUMP5"
check_found "GitHub URL shown"     "github.com/xahmol/oricscreeneditorloci" "$DUMP5"

# --- Scenario 6: page 3 (QR code) + dismiss + clean close -------------------
DUMP6="$OUT/capture_menu_version_dismiss.bin"
run_capture 21000000 '\p1\f1\p1\r\p1\r\p1\r\p1\n\p1\n\p1\n\p1\n\p1\n\p1\e\p1\e' "$DUMP6"
echo ""
echo "Page 3 (QR) dismissed, ESC-ESC closes with no residue"
check_not_found "version text gone" "OricScreenEditorLOCI" "$DUMP6"
check_not_found "bar gone"          "Screen"               "$DUMP6"
check_found     "statusbar intact"  "Main      XY 0, 0C41A S20I7P0S" "$DUMP6"

# --- Scenario 7: Fill dispatch ----------------------------------------------
DUMP6="$OUT/capture_menu_fill.bin"
run_capture 16500000 '\p1\f1\p1\n\p1\d\p1\d\p1\d\p1\n\p1\e' "$DUMP6"
echo ""
echo "Screen > Fill fills the canvas with the current char"
check_found "row 0 filled with A"  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" "$DUMP6"
check_found "row 26 filled with A" "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" "$DUMP6"
check_found "statusbar intact"     "Main      XY 0, 0C41A S41I7P0S" "$DUMP6"

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
