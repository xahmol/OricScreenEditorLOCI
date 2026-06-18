#ifndef INFO_H
#define INFO_H

// Information menu actions (src/menudata.c choices 41/42):
// info_version_show() -- 3-page Version/credits popup (title image,
// version/credits text, QR code linking to the project's GitHub page).
// info_exit() -- software reset through the RESET vector, the bare-metal
// equivalent of V1's "Exit" (no confirmation, matches README's documented
// behaviour).
void info_version_show(void);
void info_exit(void);

#pragma compile("info.c")

#endif // INFO_H
