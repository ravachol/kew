#include <stdio.h>
#include "metadata.h"

void printHelp()
{
  printf("play - a command-line music player.\n");
  printf("\n");
  printf("Usage:    play path \"path to music library\"\n");
  printf("          (Saves the music library path. Use this the first time. Ie: play path \"/home/joe/Music/\")\n");
  printf("          play <song name,directory or playlist words>\n");
  printf("          play --help, -? or -h\n");
  printf("          play --version or -v\n");
  printf("          play dir <album name> (Sometimes it's neccessary to specify it's a directory you want)\n");
  printf("          play song <song name> \n");  
  printf("\n");
  printf("Examples: play moon (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
  printf("          play path \"/home/user/Music\"\n");
  printf("\n");
  printf("Use quotation marks when providing a path with blank spaces in it or a music file with single quotes (').\n");
  printf("Use arrow keys to play the next or previous track in the playlist. Press space to pause.\n");
  printf("Exit with Esc.\n");
  printf("\n");
  printf("To run it with colors displaying correctly, you need a terminal that can handle TrueColor. For instance Konsole:\n");
  printf("macOS: Konsole, https://ports.macports.org/port/konsole/\n");
  printf("Linux: st (suckless), https://github.com/Shourai/st, or KDE Konsole\n");
  printf("\n");
}

void printVersion(const char* version)
{
  printf("::::::::::. :::      :::.  .-:.     ::-.\n");
  printf(" `;;;```.;;;;;;      ;;`;;  ';;.   ;;;;'\n");
  printf("  `]]nnn]]' [[[     ,[[ '[[,  '[[,[[['  \n");
  printf("   $$$\"\"    $$'    c$$$cc$$$c   c$$\" \n");
  printf("   888o    o88oo,.__888   888,,8P\"`    \n");
  printf("   YMMMb   \"\"\"\"YUMMMYMM   \"\"`mM\" \n");  
  printf("Play version %s.\n", version);  
}

void printBasicMetadata(const char *file_path)
{
  int pair_count;
  KeyValuePair *pairs = read_key_value_pairs(file_path, &pair_count);
  if (pairs == NULL)
  {
    fprintf(stderr, "Error reading key-value pairs.\n");
    return;
  }

  TagSettings settings = construct_tag_settings(pairs, pair_count);

  if (strlen(settings.title) > 0)
    printf("Title: %s\n", settings.title);
  if (strlen(settings.artist) > 0)
    printf("Artist: %s\n", settings.artist);
  if (strlen(settings.album) > 0)
    printf("Album: %s\n", settings.album);
  if (strlen(settings.date) > 0)
    printf("Date: %s\n", settings.date);
  free_key_value_pairs(pairs, pair_count);
}

void printProgress(double elapsed_seconds, double total_seconds, int min_cursor_line)
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

  // Move the cursor to the target row
  printf("\033[%d;1H", min_cursor_line);

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