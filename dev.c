#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

void makeFilePath(char *dirPath, char *filePath, struct dirent *entry){
    if(dirPath[strnlen(dirPath, 256)-1] == '/'){
        sprintf(filePath, "%s%s", dirPath, entry->d_name);
    }else{
        sprintf(filePath, "%s/%s", dirPath, entry->d_name);
    }
}

void storeFilesAndDirs(char *dirPath){
    DIR *directory = opendir(dirPath);
    struct dirent *entry;
    struct stat fileStat;
    bool check = true;
    // Check if selected directory is empty //
    if(directory != NULL){

        // If it's not empty go through all the files in it and file paths //
        while ((entry = readdir(directory)) != NULL){
            char filePath[256];
            makeFilePath(dirPath, filePath, entry);

            // Using those file path check if the paths lead ot files or directories //
            if(stat(filePath, &fileStat) == 0){
                if(S_ISDIR(fileStat.st_mode)){
                    if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                        continue;
                    }else{
                        printf("Found a directory\n");
                    }
                }else{
                    printf("Found a file\n");
                }
            }
        }
        closedir(directory);
        printf("Empty directory\n");
    }else{
        printf("No directory found!!\n");
        fflush(stdout);
    }
}

void printElem(char *arr[], int len){
    char **ptr = arr;
    for(ptr; ptr < arr+len; ptr++){
        printf("%s\n", *ptr);
    }
}

int  main(void)
{   char dirPath[256] = "/home/ronit/Music/The Weeknd - Starboy";
    char *dirArr[] = {"gold", "silver", "diamond"};
    int len = sizeof(dirArr) / sizeof(dirArr[0]);
    printElem(dirArr, len);
    return 0;
}
