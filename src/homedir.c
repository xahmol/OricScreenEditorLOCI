// homedir.c - LOCI boot-directory-relative path helper for
// OricScreenEditorLOCI (Oric Atmos, Oscar64)
//
// Every LOCI file path this codebase opens (the boot splash, the FUNCT+8
// help screens, and every File/Charset menu Save/Load action in
// src/fileio.c) used a bare relative filename, trusting LOCI's "current
// directory" was already correct at the time of the call. On real
// hardware this didn't hold for the title screen and help screens (found
// 2026-06-20): they loaded fine in Phosphoric (which apparently treats a
// freshly booted device's CWD as wherever the .tap was injected) but not
// on the user's real LOCI device. Ported from the archived `nonworkingcc65`
// branch's src/main.c (getcwd_loci()-based `homedir`, prefixed onto every
// asset path by hand) -- adapted here into a single shared helper instead
// of repeating the concatenation at each call site.

#include <string.h>
#include "appstate.h"
#include "loci.h"
#include "homedir.h"

/**
 * Capture LOCI's current working directory into app.homedir. Must be
 * called once, early in main(), after loci_present() succeeds.
 *
 * @return (none)
 */
void homedir_init(void)
{
    loci_getcwd(app.homedir, sizeof(app.homedir));
}

/**
 * Join app.homedir and name into out (see homedir.h for sizing).
 * app.homedir can legitimately be empty -- LOCI_OP_GETCWD errors out if
 * no drive is considered "mounted/booted from" (confirmed under
 * Phosphoric's --loci-flash test mode, which bypasses the mount system
 * entirely) -- in which case this is a no-op prefix and out == name,
 * exactly this codebase's pre-homedir behaviour (a bare relative path,
 * resolved by LOCI's own current directory at the time of the call).
 *
 * @param out  Destination buffer.
 * @param name Name (or relative sub-path) to resolve against app.homedir.
 * @return (none)
 */
void homedir_join(char *out, const char *name)
{
    uint8_t len;

    strcpy(out, app.homedir);
    len = (uint8_t)strlen(out);
    if (len > 0 && out[len - 1] != '/')
    {
        out[len] = '/';
        out[len + 1] = '\0';
    }
    strcat(out, name);
}

/**
 * Join app.homedir and app.filename + suffix into out in one step (see
 * homedir.h for sizing).
 *
 * @param out    Destination buffer.
 * @param suffix Suffix appended directly after app.filename.
 * @return (none)
 */
void homedir_join_suffix(char *out, const char *suffix)
{
    homedir_join(out, app.filename);
    strcat(out, suffix);
}
