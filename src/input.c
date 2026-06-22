// input.c - Universal key/joystick input for OricScreenEditorLOCI (Oric
// Atmos, Oscar64)
//
// Adapted from nonworkingcc65's getkey(joyallowed, norepeat)
// (local reference, git show nonworkingcc65:src/generic.c) -- always
// called here with joyallowed=1/norepeat=1 (every OSE call site wants
// exactly that), so this port drops those two parameters. V1 itself has
// no IJK/joystick code at all -- the IJK driver (include/ijk.c/h,
// Raxiss's libsrc/ijk-driver.s) is purely a LOCI-port addition, already
// copied into this project's include/ but never #include'd from src/
// before this phase.

#include "keyboard.h"
#include "charwin.h"
#include "ijk.h"
#include "input.h"

/**
 * Block until a key is pressed (keyboard or IJK joystick) and return it.
 * Each spin: polls the keyboard first (keyb_poll(), include/keyboard.h);
 * only if that comes back empty AND an IJK joystick is present does it
 * read the sticks (ijk_read()) and map the first matching direction/fire
 * bit (checked FIRE/RIGHT/LEFT/DOWN/UP on the left stick then the right,
 * last match wins -- mirrors nonworkingcc65's exact check order) to
 * KEY_ENTER/KEY_RIGHT/KEY_LEFT/KEY_DOWN/KEY_UP. If a direction fired,
 * drains the stick to neutral, then re-syncs the keyboard scanner with a
 * burst of keyb_scan() calls before returning, so a single joystick press
 * can't repeat and the next keyboard poll starts from a clean state.
 *
 * Keyboard-before-joystick (not the reverse): ijk_read() (include/ijk.c)
 * shares VIA Port A with the keyboard scanner (include/keyboard.c's
 * keyb_scan()). This order matches locifilemanager-v2's own fm_getkey().
 * (Was investigated as a candidate fix for '=' never registering on
 * real hardware and ruled out -- the actual cause was a decode-table
 * transcription error, see memory equals_plus_key_not_recognized. Kept
 * anyway since it matches the proven reference and has no downside.)
 *
 * @return The key code read (keyboard key code, or the joystick's
 *         mapped equivalent).
 */
uint8_t key_read(void)
{
    uint8_t key;

    do
    {
        key = keyb_poll();

        if (!key && ijk_present)
        {
            ijk_read();

            if (ijk_ljoy & IJK_JOY_FIRE)  key = KEY_ENTER;
            if (ijk_ljoy & IJK_JOY_RIGHT) key = KEY_RIGHT;
            if (ijk_ljoy & IJK_JOY_LEFT)  key = KEY_LEFT;
            if (ijk_ljoy & IJK_JOY_DOWN)  key = KEY_DOWN;
            if (ijk_ljoy & IJK_JOY_UP)    key = KEY_UP;
            if (ijk_rjoy & IJK_JOY_FIRE)  key = KEY_ENTER;
            if (ijk_rjoy & IJK_JOY_RIGHT) key = KEY_RIGHT;
            if (ijk_rjoy & IJK_JOY_LEFT)  key = KEY_LEFT;
            if (ijk_rjoy & IJK_JOY_DOWN)  key = KEY_DOWN;
            if (ijk_rjoy & IJK_JOY_UP)    key = KEY_UP;

            if (key)
            {
                do { ijk_read(); } while (ijk_ljoy || ijk_rjoy);
                for (uint16_t d = 0; d < 1000; d++) keyb_scan();
            }
        }
    } while (!key);

    return key;
}
