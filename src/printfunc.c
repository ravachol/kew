#include "printfunc.h"
#include "term.h"
#include <string.h>

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT
    typedef struct
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
    } PixelData;
#endif

void printHelp()
{
    printf("cue - a command-line music player.\n");
    printf("\n");
    printf("Usage:    cue path \"path to music library\"\n");
    printf("          (Saves the music library path. Use this the first time. Ie: cue path \"/home/joe/Music/\")\n");
    printf("          cue <song name,directory or playlist words>\n");
    printf("          cue --help, -? or -h\n");
    printf("          cue --version or -v\n");
    printf("          cue dir <album name> (Sometimes it's neccessary to specify it's a directory you want)\n");
    printf("          cue song <song name> \n");
    printf("          cue shuffle <dir name> (random and rand works too)\n");
    printf("          cue cure:depeche (plays the cure and depeche mode shuffled)");
    printf("\n");
    printf("Examples: cue moon (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
    printf("          play path \"/home/user/Music\"\n");
    printf("\n");
    printf("cue returns the first directory or file whose name matches the string you provide. ");
    printf("Use quotation marks when providing a path with blank spaces in it or if it's a music file that contains single quotes (').\n");
    printf("Use arrow keys to play the next or previous track in the playlist. Press space to pause.\n");
    printf("Press q to quit.\n");
    printf("\n");
    printf("To run it with colors displaying correctly, you need a terminal that can handle TrueColor.\n");
    printf("\n");
}

bool isVersionGreater(const char* versionA, const char* versionB) {
    // Split versionA into major, minor, and patch components
    int majorA, minorA, patchA;
    sscanf(versionA, "%d.%d.%d", &majorA, &minorA, &patchA);

    // Split versionB into major, minor, and patch components
    int majorB, minorB, patchB;
    sscanf(versionB, "%d.%d.%d", &majorB, &minorB, &patchB);

    // Compare the components
    if (majorA > majorB) {
        return true;
    } else if (majorA < majorB) {
        return false;
    }

    if (minorA > minorB) {
        return true;
    } else if (minorA < minorB) {
        return false;
    }

    if (patchA > patchB) {
        return true;
    } else if (patchA < patchB) {
        return false;
    }

    // If all components are equal, versions are not greater
    return false;
}

void printAsciiLogo()
{
    int minWidth = 31;
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_w < minWidth)
        return;

    printf("  $$$$$$$\\ $$\\   $$\\  $$$$$$\\\n");
    printf(" $$  _____|$$ |  $$ |$$  __$$\\\n");
    printf(" $$ /      $$ |  $$ |$$$$$$$$ |\n");
    printf(" $$ |      $$ |  $$ |$$   ____|\n");
    printf(" \\$$$$$$$\\ \\$$$$$$  |\\$$$$$$$\\\n");
    printf("  \\_______| \\______/  \\_______|\n");    
}

int getDayDifference(const char* date)
{
    struct tm tm_date;
    memset(&tm_date, 0, sizeof(struct tm)); // Initialize the structure with zeroes

    time_t time_now, time_date;

    time(&time_now);

    if (sscanf(date, "%d-%d-%d", &tm_date.tm_year, &tm_date.tm_mon, &tm_date.tm_mday) != 3)
    {
        return -1;
    }

    tm_date.tm_year -= 1900;
    tm_date.tm_mon -= 1;

    tm_date.tm_hour = 0;
    tm_date.tm_min = 0;
    tm_date.tm_sec = 0;

    time_date = mktime(&tm_date);

    double diff_seconds = difftime(time_now, time_date);

    int diff_days = diff_seconds / (24 * 60 * 60);

    return diff_days;
}


void printVersion(const char *version, const char *versionDate, PixelData color, PixelData secondaryColor)
{
    setTextColorRGB2(secondaryColor.r, secondaryColor.g, secondaryColor.b);
    printf(" cue version %s.\n", version);
    setTextColorRGB2(color.r, color.g, color.b); 
    int daysOld = getDayDifference(versionDate);
    printf(" This version is %d days old.\n", daysOld);
    setTextColorRGB2(secondaryColor.r, secondaryColor.g, secondaryColor.b);   
    printf(" Github Homepage:\n");
    setTextColorRGB2(color.r, color.g, color.b);
    printf(" https://github.com/ravachol/cue\n");
    setTextColorRGB2(secondaryColor.r, secondaryColor.g, secondaryColor.b);
}

int getYear(const char *dateString)
{
    int year;

    if (sscanf(dateString, "%d", &year) != 1)
    {
        return -1;
    }
    return year;
}