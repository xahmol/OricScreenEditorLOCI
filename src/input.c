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
 * Each spin: if an IJK joystick is present, reads both sticks
 * (ijk_read()) and maps the first matching direction/fire bit (checked
 * FIRE/RIGHT/LEFT/DOWN/UP on the left stick then the right, last match
 * wins -- mirrors nonworkingcc65's exact check order) to
 * KEY_ENTER/KEY_RIGHT/KEY_LEFT/KEY_DOWN/KEY_UP; if a direction fired,
 * drains the stick to neutral before returning so a single press can't
 * repeat. Falls back to a non-blocking keyboard poll (keyb_poll(),
 * include/keyboard.h) if the joystick gave nothing this spin (or no
 * joystick is present at all).
 *
 * @return The key code read (keyboard key code, or the joystick's
 *         mapped equivalent).
 */
uint8_t key_read(void)
{
    uint8_t key;

    do
    {
        key = 0;

        if (ijk_present)
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
            }
        }

        if (!key) key = keyb_poll();
    } while (!key);

    return key;
}
