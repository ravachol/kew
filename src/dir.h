#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#define MAX_FILENAME_LENGTH 256

enum SearchType {Any = 0, DirOnly = 1, FileOnly = 2}; 

int tryOpen(const char* path);

int isDirectory(const char* path);

// Function to traverse a directory tree and search for a given file or directory
int walker(const char *startPath, const char *searching, char *result,
           const char *allowedExtensions, enum SearchType searchType);