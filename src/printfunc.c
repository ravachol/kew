#include "printfunc.h"
#include "term.h"
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

void printVersion(const char *version, const char *latestVersion)
{
    printf(" $$$$$$$\\ $$\\   $$\\  $$$$$$\\\n");
    printf("$$  _____|$$ |  $$ |$$  __$$\\\n");
    printf("$$ /      $$ |  $$ |$$$$$$$$ |\n");
    printf("$$ |      $$ |  $$ |$$   ____|\n");
    printf("\\$$$$$$$\\ \\$$$$$$  |\\$$$$$$$\\\n");
    printf(" \\_______| \\______/  \\_______|\n");
    printf("cue version %s.\n", version);
    if (isVersionGreater("latestVersion", version))
        printf("There is a newer version (%s) available.\n", latestVersion);
    printf("Github Homepage: https://github.com/ravachol/cue\n");
    printf("Copyright ® Ravachol 2023.\n");
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

void printBasicMetadata(const char *file_path)
{
    int pair_count;
    KeyValuePair *pairs = readKeyValuePairs(file_path, &pair_count);
    if (pairs == NULL)
    {
        fprintf(stderr, "Error reading key-value pairs.\n");
        return;
    }

    TagSettings settings = construct_tag_settings(pairs, pair_count);

    if (strlen(settings.title) > 0)
        printf("%s\n", settings.title);
    if (strlen(settings.artist) > 0)
        printf("%s\n", settings.artist);
    if (strlen(settings.album) > 0)
        printf("%s\n", settings.album);
    if (strlen(settings.date) > 0)
    {
        int year = getYear(settings.date);
        if (year == -1)
            printf("%s\n", settings.date);
        else
            printf("%d\n", year);
    }
    freeKeyValuePairs(pairs, pair_count);
    fflush(stdout);
}

void printProgress(double elapsed_seconds, double total_seconds, double total_duration_seconds)
{
    int progressWidth = 31;
    int term_w, term_h;
    getTermSize(&term_w, &term_h);

    if (term_w < progressWidth)
        return;

    // Save the current cursor position
    printf("\033[s");

    int elapsed_hours = (int)(elapsed_seconds / 3600);
    int elapsed_minutes = (int)(((int)elapsed_seconds / 60) % 60);
    int elapsed_seconds_remainder = (int)elapsed_seconds % 60;

    int total_hours = (int)(total_seconds / 3600);
    int total_minutes = (int)(((int)total_seconds / 60) % 60);
    int total_seconds_remainder = (int)total_seconds % 60;

    int progress_percentage = (int)((elapsed_seconds / total_seconds) * 100);

    int total_playlist_hours = (int)(total_duration_seconds / 3600);
    int total_playlist_minutes = (int)(((int)total_duration_seconds / 60) % 60);

    // Clear the current line
    printf("\033[K");

    printf("%02d:%02d:%02d / %02d:%02d:%02d (%d%%) T:%dh%02dm",
           elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
           total_hours, total_minutes, total_seconds_remainder,
           progress_percentage, total_playlist_hours, total_playlist_minutes);
    fflush(stdout);

    // Restore the cursor position
    printf("\033[u");
    fflush(stdout);
}