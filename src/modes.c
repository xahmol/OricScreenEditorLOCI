// modes.c - Editor mode name lookup for OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Small standalone module (no editor.c/select.c/etc dependency in either
// direction) so any mode-implementing file and statusbar.c can both use it
// without a circular #pragma compile chain.

#include "strings.h"
#include "modes.h"

/**
 * Look up the display name for an editor mode, for the statusbar's Mode
 * field (MSG_STATUSBAR_MAIN_FMT's leading %-10s).
 *
 * @param mode Editor mode to name.
 * @return The mode's MSG_MODE_* string (English or French depending on
 *         build), or MSG_MODE_MAIN if mode is not recognised.
 */
const char *mode_name(EditorMode mode)
{
    switch (mode)
    {
    case MODE_WRITE:   return MSG_MODE_WRITE;
    case MODE_SELECT:  return MSG_MODE_SELECT;
    case MODE_MOVE:    return MSG_MODE_MOVE;
    case MODE_LINEBOX: return MSG_MODE_LINEBOX;
    case MODE_TRY:     return MSG_MODE_TRY;
    case MODE_MAIN:
    default:           return MSG_MODE_MAIN;
    }
}
