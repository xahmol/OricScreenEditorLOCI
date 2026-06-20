#ifndef HOMEDIR_H
#define HOMEDIR_H

// Captures LOCI's boot-time current working directory into app.homedir,
// once, at startup. The LOCI ROM's GETCWD call returns "the LOCI path of
// the assumed boot image" (sodiumlb/loci-rom's initcwd.s) -- i.e. the
// directory oseloci.tap was actually loaded from, however the user
// launched it on real hardware. Must be called once, early in main(),
// after the loci_present() boot gate passes and before any other LOCI
// file access (the splash screen load, in particular).
void homedir_init(void);

// Join app.homedir and name into out: app.homedir, then a '/' separator
// (inserted only if app.homedir doesn't already end with one and is
// non-empty), then name. out must be large enough to hold
// strlen(app.homedir) + 1 + strlen(name) + 1 bytes -- callers building a
// LOCI path from a fileio.c-style "name+suffix" buffer should size their
// destination as HOMEDIR_MAXLEN + that buffer's own size + 1.
//
// @param out  Destination buffer.
// @param name Name (or relative sub-path) to resolve against app.homedir.
// @return (none)
void homedir_join(char *out, const char *name);

// Join app.homedir and app.filename + suffix into out in one step (the
// fileio.c-style "%s<suffix>", app.filename pattern, e.g. suffix=".BIN"
// or suffix="PJ.BIN") -- avoids needing a separate small staging buffer
// for app.filename+suffix at each call site (which mattered: an earlier
// version of this fix needed two stack buffers per fileio.c function and
// blew Oscar64's static stack budget). out must be sized the same as for
// homedir_join().
//
// @param out    Destination buffer.
// @param suffix Suffix appended directly after app.filename.
// @return (none)
void homedir_join_suffix(char *out, const char *suffix);

#pragma compile("homedir.c")

#endif // HOMEDIR_H
