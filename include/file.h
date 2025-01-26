#ifndef __FILE_H_
#define __FILE_H_

// Function prototypes
int filepickerfromdir(char* headertext);
int choosedirandfilename(char *headertext, unsigned char maxlen, unsigned char validation);
void loadscreenmap(unsigned char combined);
void savescreenmap(unsigned char combined);
void saveproject();
void loadproject();
void loadcharset(unsigned char stdoralt);
void savecharset(unsigned char stdoralt);

#endif // __FILE_H_