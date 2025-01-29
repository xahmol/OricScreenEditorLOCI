#ifndef __FILE_H_
#define __FILE_H_

// Function prototypes
unsigned char filepickerfromdir(char *headertext, unsigned char filter);
unsigned char choosedirandfilename(char *headertext, unsigned char maxlen, unsigned char validation);
void loadscreenmap(unsigned char combined);
void savescreenmap(unsigned char combined);
void saveproject();
void loadproject();
void loadcharset(unsigned char stdoralt);
void savecharset(unsigned char stdoralt);

#endif // __FILE_H_