extern int default_ascii_height;
extern int default_ascii_width;

extern int small_ascii_height;
extern int small_ascii_width;

int isAudioFile(const char* filename);

void addSlashIfNeeded(char* str);

int compareEntries(const struct dirent **a, const struct dirent **b);

char* findFirstPathWithAudioFile(const char* path);

char* findLargestImageFile(const char* directoryPath);


extern int displayAlbumArt(const char* directory, int asciiHeight, int asciiWidth);