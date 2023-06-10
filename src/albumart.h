#ifndef ALBUMART_H
#define ALBUMART_H


extern int default_ascii_height;
extern int default_ascii_width;

extern int small_ascii_height;
extern int small_ascii_width;

extern int isAudioFile(const char* filename);

extern void addSlashIfNeeded(char* str);

extern int compareEntries(const struct dirent **a, const struct dirent **b) ;

extern char* findFirstPathWithAudioFile(const char* path);

extern char* findLargestImageFile(const char* directoryPath);


extern int displayAlbumArt(const char* directory, int asciiHeight, int asciiWidth);

#endif