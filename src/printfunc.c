#include "printfunc.h"

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
  printf("          cue random <dir name> (Shuffle and rand works too)\n");
  printf("\n");
  printf("Examples: cue moon (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
  printf("          play path \"/home/user/Music\"\n");
  printf("\n");
  printf("Use quotation marks when providing a path with blank spaces in it or a music file with single quotes (').\n");
  printf("Use arrow keys to play the next or previous track in the playlist. Press space to pause.\n");
  printf("Exit with Esc.\n");
  printf("\n");
  printf("To run it with colors displaying correctly, you need a terminal that can handle TrueColor.\n");
  printf("\n");
}

void printVersion(const char* version)
{
  printf(" $$$$$$$\\ $$\\   $$\\  $$$$$$\\\n");
  printf("$$  _____|$$ |  $$ |$$  __$$\\\n");
  printf("$$ /      $$ |  $$ |$$$$$$$$ |\n");
  printf("$$ |      $$ |  $$ |$$   ____|\n");
  printf("\\$$$$$$$\\ \\$$$$$$  |\\$$$$$$$\\\n");
  printf(" \\_______| \\______/  \\_______|\n");
  printf("cue version %s.\n", version);  
}

int getYear(const char* dateString) {
    int year;

    if (sscanf(dateString, "%d", &year) != 1) {
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

void printProgress(double elapsed_seconds, double total_seconds)
{
  // Save the current cursor position
  printf("\033[s");

  int elapsed_hours = (int)(elapsed_seconds / 3600);
  int elapsed_minutes = (int)(((int)elapsed_seconds / 60) % 60);
  int elapsed_seconds_remainder = (int)elapsed_seconds % 60;

  int total_hours = (int)(total_seconds / 3600);
  int total_minutes = (int)(((int)total_seconds / 60) % 60);
  int total_seconds_remainder = (int)total_seconds % 60;

  int progress_percentage = (int)((elapsed_seconds / total_seconds) * 100);

  // Clear the current line
  printf("\033[K");

  printf("%02d:%02d:%02d / %02d:%02d:%02d (%d%%)",
         elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
         total_hours, total_minutes, total_seconds_remainder,
         progress_percentage);
  fflush(stdout);

  // Restore the cursor position
  printf("\033[u");
  fflush(stdout);
}