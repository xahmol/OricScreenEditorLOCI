#!/usr/bin/env bash
# tests/scripts/test_charsetram_spike.sh
#
# Charset-RAM edit-in-place regression (the `make
# test-charsetram-spike` / `make test` target).
#
# Confirms the core character-editor assumption: the live, ULA-rendered charset RAM
# at $B400-$BBFF (include/oric.h CHARSET_STD/CHARSET_ALT) can be edited
# directly by the running program -- no linker/heap/stack collision, and
# the new glyph data survives to the end of the run.
#
# Pressing 'e' from main mode opens the character editor popup
# (src/charsetedit.c) on the current plot screencode ('@' = 0x40,
# standard charset). 'i' XORs the 8 glyph bytes with 0x3F (inverts all 6
# pixel bits) directly in charset RAM and redraws the grid; ESC commits and
# closes the popup with no residue.
#
# Required env vars (set by `make test-charsetram-spike`):
#   PHOS      path to oric1-emu
#   ATMOSROM  path to roms/basic11b.rom
#   SANDBOX   tests/sandbox (freshly built .tap)
#   OUT       tests/out (scratch dir for RAM dumps)
#   TAPFILE   oseloci.tap

set -u
cd "$(dirname "$0")/../.." || exit 1

SCREEN=tests/scripts/oric_screen.py

# Calibrated cycle counts (see `make test-capture` for re-calibration).
DISMISS_AT=8000000
BEFORE_CYCLES=9000000
# AFTER: 3 \p1-prefixed actions (e, i, ESC) -- 8,080,000 + 3*1,100,000 + 300,000
AFTER_CYCLES=11680000

# charset_address(0x40, 0) = CHARSET_STD + 0x40*8 = 0xB600 (see src/charsetedit.c)
ADDR_LEN="0xB600:8"

BEFORE_DUMP="$OUT/capture_charset_before.bin"
AFTER_DUMP="$OUT/capture_charset_after.bin"

pass=0
fail=0

echo "==========================================================="
echo "  OricScreenEditorLOCI -- charset-RAM edit-in-place spike"
echo "==========================================================="

if [ ! -x "$PHOS" ]; then
    echo "  oric1-emu not found/executable at $PHOS -- skipping"
    exit 0
fi

"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f --loci \
    --headless -c $BEFORE_CYCLES \
    --type-keys "$DISMISS_AT: " \
    --dump-ram-at $BEFORE_CYCLES:"$BEFORE_DUMP" >/dev/null 2>&1

"$PHOS" -r "$ATMOSROM" \
    -t "$SANDBOX/$TAPFILE" -f --loci \
    --headless -c $AFTER_CYCLES \
    --type-keys "$DISMISS_AT: \p1e\p1i\p1\e" \
    --dump-ram-at $AFTER_CYCLES:"$AFTER_DUMP" >/dev/null 2>&1

if [ ! -f "$BEFORE_DUMP" ] || [ ! -f "$AFTER_DUMP" ]; then
    echo "  [FAIL] emulator did not produce expected RAM dumps"
    fail=$((fail+1))
else
    before=$(python3 "$SCREEN" "$BEFORE_DUMP" --bytes "$ADDR_LEN")
    after=$(python3 "$SCREEN" "$AFTER_DUMP" --bytes "$ADDR_LEN")

    echo ""
    echo "  '@' glyph before charsetedit:        $before"
    echo "  '@' glyph after  e,i,ESC charsetedit: $after"

    # Sanity: charset RAM is pre-populated by the ROM at boot (non-zero glyph).
    if [ "$before" != "00 00 00 00 00 00 00 00" ]; then
        echo "  [PASS] charset RAM is pre-populated with a non-empty glyph"
        pass=$((pass+1))
    else
        echo "  [FAIL] charset RAM glyph for 'A' is all-zero before edit"
        fail=$((fail+1))
    fi

    # Core assumption: after-bytes == before-bytes XOR 0x3F (every row),
    # i.e. the direct write to $B400-$BBFF persisted unchanged.
    expected=$(python3 -c "
before = '$before'.split()
print(' '.join(f'{int(b,16) ^ 0x3f:02x}' for b in before))
")
    if [ "$after" = "$expected" ]; then
        echo "  [PASS] direct charset-RAM edit persisted (after == before XOR 0x3F)"
        pass=$((pass+1))
    else
        echo "  [FAIL] charset-RAM edit did not persist as expected"
        echo "         expected: $expected"
        fail=$((fail+1))
    fi
fi

echo ""
echo "==========================================================="
echo "  Results: $pass passed, $fail failed"
echo "==========================================================="

if [ $fail -gt 0 ]; then
    exit 1
fi
exit 0
