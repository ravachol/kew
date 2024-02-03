#include "player.h"
/*

player.c

Functions related to printing the player to the screen.

*/

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

AppState appState;

const char VERSION[] = "2.2.1";
const int LOGO_COLOR = 6;
const int ARTIST_COLOR = 6;
const int ENQUEUED_COLOR = 6;
const int ABSOLUTE_MIN_WIDTH = 64;
volatile bool refresh = true;
bool visualizerEnabled = true;
bool coverEnabled = true;
bool quitAfterStopping = false;
bool coverAnsi = false;
bool metaDataEnabled = true;
bool timeEnabled = true;
bool drewCover = true;
bool uiEnabled = true;
bool showList = true;
bool resetPlaylistDisplay = true;
bool useProfileColors = true;
bool fastForwarding = false;
bool rewinding = false;
bool nerdFontsEnabled = true;
int numProgressBars = 15;
int elapsedBars = 0;
int chosenRow = 0;
int chosenSong = 0;
int startIter = 0;
int aboutHeight = 8;
int visualizerHeight = 5;
int minWidth = ABSOLUTE_MIN_WIDTH;
int minHeight = 2;
int maxWidth = 0;
int coverRow = 0;
int preferredWidth = 0;
int preferredHeight = 0;
int elapsed = 0;
int textWidth = 0;
char *tagsPath;
double totalDurationSeconds = 0.0;
PixelData color = {125, 125, 125};
PixelData bgColor = {90, 90, 90};
TagSettings metadata = {};
double elapsedSeconds = 0.0;
double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;
double seekAccumulatedSeconds = 0.0;
double duration = 0.0;
int maxListSize = 0;
unsigned char defaultColor = 150;

int numDirectoryTreeEntries = 0;
int numTopLevelSongs = 0;
int startLibIter = 0;
int maxLibListSize = 0;
int chosenLibRow = 0;
bool allowChooseSongs = false;
FileSystemEntry *currentEntry = NULL;
FileSystemEntry *chosenDir = NULL;
int libIter = 0;
int libSongIter = 0;
int libTopLevelSongIter = 0;
int chosenNodeId = 0;

FileSystemEntry *library = NULL;

void setTextColorRGB2(int r, int g, int b)
{
        if (!useProfileColors)
                setTextColorRGB(r, g, b);
}

void setColor()
{
        if (useProfileColors)
        {
                setDefaultTextColor();
                return;
        }
        if (color.r == 125 && color.g == 125 && color.b == 125)
                setDefaultTextColor();
        else if (color.r >= 210 && color.g >= 210 && color.b >= 210)
        {
                color.r = defaultColor;
                color.g = defaultColor;
                color.b = defaultColor;
        }
        else
        {
                setTextColorRGB2(color.r, color.g, color.b);
        }
}

bool hasNerdFonts()
{
        bool nerdFonts = true;
        if (printf("\uf28b") < 0)
        {
                nerdFonts = false;
        }
        clearScreen();
        fflush(stdout);
        return nerdFonts;
}

int calcMetadataHeight()
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (metadata.title[0] != '\0')
        {
                size_t titleLength = strlen(metadata.title);
                int titleHeight = (int)ceil((float)titleLength / term_w);
                size_t artistLength = strlen(metadata.artist);
                int artistHeight = (int)ceil((float)artistLength / term_w);
                size_t albumLength = strlen(metadata.album);
                int albumHeight = (int)ceil((float)albumLength / term_w);
                int yearHeight = 1;

                return titleHeight + artistHeight + albumHeight + yearHeight;
        }
        else
        {
                return 4;
        }
}

void calcPreferredSize()
{
        minHeight = 2 + (visualizerEnabled ? visualizerHeight : 0);
        calcIdealImgSize(&preferredWidth, &preferredHeight, (visualizerEnabled ? visualizerHeight : 0), calcMetadataHeight());
}

void shortenString(char *str, size_t width)
{
        if (strlen(str) > width)
        {
                str[width] = '\0';
        }
}

void printHelp()
{
        printf("kew - a command-line music player.\n");
        printf("\n");
        printf("Usage:    kew path \"path to music library\"\n");
        printf("          (Saves the music library path. Use this the first time. Ie: kew path \"/home/joe/Music/\")\n");
        printf("          kew (no argument, loads all your songs up to 10 000)\n");
        printf("          kew <song name,directory or playlist words>\n");
        printf("          kew --help, -? or -h\n");
        printf("          kew --version or -v\n");
        printf("          kew dir <album name> (Sometimes it's necessary to specify it's a directory you want)\n");
        printf("          kew song <song name> \n");
        printf("          kew list <m3u list name> \n");
        printf("          kew shuffle <dir name> (random and rand works too)\n");
        printf("          kew artistA:artistB (plays artistA and artistB shuffled)");
        printf("\n");
        printf("Examples: kew moon (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
        printf("          play path \"/home/user/Music\"\n");
        printf("\n");
        printf("kew returns the first directory or file whose name matches the string you provide. ");
        printf("Use quotation marks when providing a path with blank spaces in it or if it's a music file that contains single quotes (').\n");
        printf("Use ←, → or h, l to play the next or previous track in the playlist.\n");
        printf("Use +, - to adjust volume.\n");
        printf("Press space or p to pause.\n");
        printf("Press F2 to display playlist.\n");
        printf("Press F3 to display key bindings.\n");
        printf("Press q to quit.\n");
        printf("\n");
        printf("To run it with colors displaying correctly, you need a terminal that can handle TrueColor.\n");
        printf("\n");
}

int printLogo(SongData *songData)
{
        if (useProfileColors)
                setTextColor(LOGO_COLOR);

        printBlankSpaces(indent);
        printf(" __\n");
        printBlankSpaces(indent);
        printf("|  |--.-----.--.--.--.\n");
        printBlankSpaces(indent);
        printf("|    <|  -__|  |  |  |\n");
        printBlankSpaces(indent);
        printf("|__|__|_____|________|");

        if (songData != NULL && songData->metadata != NULL)
        {
                int term_w, term_h;
                getTermSize(&term_w, &term_h);

                char *title = (char *)calloc(MAXPATHLEN, sizeof(char)); // Initialize to zero
                if (title == NULL)
                {
                        return -1;
                }
                title[0] = '\0';

                strcat(title, songData->metadata->title);
                shortenString(title, term_w - indent - indent - 27);

                printf(" %s", title);

                free(title);
        }
        printf("\n\n");

        int height = 6;
        return height;
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

void printCover(SongData *songdata)
{
        clearRestOfScreen();
        minWidth = ABSOLUTE_MIN_WIDTH + indent;
        if (songdata->cover != NULL && coverEnabled)
        {
                color.r = *(songdata->red);
                color.g = *(songdata->green);
                color.b = *(songdata->blue);

                displayCover(songdata, preferredWidth, preferredHeight, coverAnsi);

                if (color.r == 0 && color.g == 0 && color.b == 0)
                {
                        color.r = defaultColor;
                        color.g = defaultColor;
                        color.b = defaultColor;
                }
                drewCover = true;
        }
        else
        {
                color.r = defaultColor;
                color.g = defaultColor;
                color.b = defaultColor;
                clearRestOfScreen();
                for (int i = 0; i < preferredHeight - 1; i++)
                {
                        printf("\n");
                }
                drewCover = false;
        }
}

void printWithDelay(const char *text, int delay, int maxWidth)
{
        int length = strlen(text);
        int max = (maxWidth > length) ? length : maxWidth;
        for (int i = 0; i <= max; i++)
        {
                printf("\r ");
                printBlankSpaces(indent);
                for (int j = 0; j < i; j++)
                {
                        printf("%c", text[j]);
                }
                printf("█");
                fflush(stdout);
                c_sleep(delay);
        }
        c_sleep(delay * 20);
        printf("\r");
        printf("\033[K");
        printBlankSpaces(indent);
        printf("\033[1K %.*s", maxWidth, text);
        printf("\n");
        fflush(stdout);
}

void printBasicMetadata(TagSettings const *metadata)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxWidth = textWidth; // term_w - 3 - (indent * 2);
        printf("\n");
        setColor();
        int rows = 1;
        if (strlen(metadata->artist) > 0)
        {
                printBlankSpaces(indent);
                printf(" %.*s\n", maxWidth, metadata->artist);
                rows++;
        }
        if (strlen(metadata->album) > 0)
        {
                printBlankSpaces(indent);
                printf(" %.*s\n", maxWidth, metadata->album);
                rows++;
        }
        if (strlen(metadata->date) > 0)
        {
                printBlankSpaces(indent);
                int year = getYear(metadata->date);
                if (year == -1)
                        printf(" %s\n", metadata->date);
                else
                        printf(" %d\n", year);
                rows++;
        }
        cursorJump(rows);
        if (strlen(metadata->title) > 0)
        {
                PixelData pixel = increaseLuminosity(color, 20);

                if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
                {
                        PixelData gray;
                        gray.r = defaultColor;
                        gray.g = defaultColor;
                        gray.b = defaultColor;
                        printf("\033[1;38;2;%03u;%03u;%03um", gray.r, gray.g, gray.b);
                }
                else
                {
                        printf("\033[1;38;2;%03u;%03u;%03um", pixel.r, pixel.g, pixel.b);
                }

                if (useProfileColors)
                        printf("\e[1m\e[39m");

                printWithDelay(metadata->title, 9, maxWidth - 2);
        }
        cursorJumpDown(rows - 1);
}

int calcElapsedBars(double elapsedSeconds, double duration, int numProgressBars)
{
        if (elapsedSeconds == 0)
                return 0;

        return (int)((elapsedSeconds / duration) * numProgressBars);
}

void printProgress(double elapsed_seconds, double total_seconds, double total_duration_seconds, PlayList const *playlist)
{
        int progressWidth = 39;
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

        int vol = getCurrentVolume();

        // Clear the current line
        printf("\r\033[K");
        printBlankSpaces(indent);
        if (playlist->count <= MAX_COUNT_PLAYLIST_SONGS && total_duration_seconds > -1)
        {
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) T:%dh%02dm Vol:%d%%",
                       elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
                       total_hours, total_minutes, total_seconds_remainder,
                       progress_percentage, total_playlist_hours, total_playlist_minutes, vol);
        }
        else
        {

                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                       elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
                       total_hours, total_minutes, total_seconds_remainder,
                       progress_percentage, vol);
        }

        // Restore the cursor position
        printf("\033[u");
}
void printMetadata(TagSettings const *metadata)
{
        if (!metaDataEnabled || appState.currentView == LIBRARY_VIEW || appState.currentView == PLAYLIST_VIEW)
                return;
        c_sleep(100);
        setColor();
        printBasicMetadata(metadata);
}

void printTime(PlayList const *playlist)
{
        if (!timeEnabled || appState.currentView == LIBRARY_VIEW || appState.currentView == PLAYLIST_VIEW)
                return;
        setColor();
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        printBlankSpaces(indent);
        if (term_h > minHeight && term_w > minWidth)
                printProgress(elapsed, duration, totalDurationSeconds, playlist);
}

int getRandomNumber(int min, int max)
{
        return min + rand() % (max - min + 1);
}

void printGlimmeringText(char *text, char *nerdFontText, PixelData color)
{
        int textLength = strlen(text);
        int brightIndex = 0;
        PixelData vbright = increaseLuminosity(color, 120);
        PixelData bright = increaseLuminosity(color, 60);

        printBlankSpaces(indent);

        while (brightIndex < textLength)
        {
                for (int i = 0; i < textLength; i++)
                {
                        if (i == brightIndex)
                        {
                                setTextColorRGB(vbright.r, vbright.g, vbright.b);
                                printf("%c", text[i]);
                        }
                        else if (i == brightIndex - 1 || i == brightIndex + 1)
                        {
                                setTextColorRGB(bright.r, bright.g, bright.b);
                                printf("%c", text[i]);
                        }
                        else
                        {
                                setTextColorRGB(color.r, color.g, color.b);
                                printf("%c", text[i]);
                        }

                        fflush(stdout);
                        c_usleep(50);
                }
                printf("%s", nerdFontText);
                fflush(stdout);
                c_usleep(50);

                brightIndex++;
                printf("\r");
                printBlankSpaces(indent);
        }
}

void printLastRow()
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        if (term_w < minWidth)
                return;
        setTextColorRGB(bgColor.r, bgColor.g, bgColor.b);

        char text[100] = " [F2 Playlist] [F3 Library] [F4 Track] [F5 Keys] [Q Quit]";

        char nerdFontText[100] = "";

        printf("\r");

        if (nerdFontsEnabled)
        {
                if (isPaused())
                {
                        char pauseText[] = " \uf04c";
                        strcat(nerdFontText, pauseText);
                }

                if (isRepeatEnabled())
                {
                        char repeatText[] = " \uf01e";
                        strcat(nerdFontText, repeatText);
                }

                if (isShuffleEnabled())
                {
                        char shuffleText[] = " \uf074";
                        strcat(nerdFontText, shuffleText);
                }

                if (fastForwarding)
                {
                        char forwardText[] = " \uf04e";
                        strcat(nerdFontText, forwardText);
                }

                if (rewinding)
                {
                        char rewindText[] = " \uf04a";
                        strcat(nerdFontText, rewindText);
                }
        }
        else
        {
                if (isRepeatEnabled())
                {
                        char repeatText[] = " R";
                        strcat(text, repeatText);
                }

                if (isShuffleEnabled())
                {
                        char shuffleText[] = " S";
                        strcat(text, shuffleText);
                }
        }

        printf("\033[K"); // clear the line

        int randomNumber = getRandomNumber(1, 808);
        if (randomNumber == 808)
                printGlimmeringText(text, nerdFontText, bgColor);
        else
        {
                printBlankSpaces(indent);
                printf("%s", text);
                printf("%s", nerdFontText);
        }
}

int printAbout(SongData *songdata)
{
        clearRestOfScreen();
        printf("\r");
        int numRows = printLogo(songdata);
        printBlankSpaces(indent);
        printf(" Version: %s\n\n", VERSION);
        numRows += 3;

        return numRows;
}

void removeUnneededChars(char *str)
{
        if (strlen(str) < 6)
        {
                return;
        }

        int i;
        for (i = 0; i < 3 && str[i] != '\0' && str[i] != ' '; i++)
        {
                if (isdigit(str[i]) || str[i] == '-' || str[i] == ' ')
                {
                        int j;
                        for (j = i; str[j] != '\0'; j++)
                        {
                                str[j] = str[j + 1];
                        }
                        str[j] = '\0';
                        i--; // Decrement i to re-check the current index
                }
        }
        i = 0;
        while (str[i] != '\0')
        {
                if (str[i] == '-' || str[i] == '_')
                {
                        str[i] = ' ';
                }
                i++;
        }
}

int showKeyBindings(SongData *songdata)
{
        int numPrintedRows = 1;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        numPrintedRows += printAbout(songdata);

        setColor();

        printBlankSpaces(indent);
        printf(" - Use %s (or %s) and %s to adjust volume.\n", settings.volumeUp, settings.volumeUpAlt, settings.volumeDown);
        printBlankSpaces(indent);
        printf(" - Use ←, → or %s, %s keys to switch tracks.\n", settings.previousTrackAlt, settings.nextTrackAlt);
        printBlankSpaces(indent);
        printf(" - Use ↑, ↓  or %s, %s keys to scroll through playlist.\n", settings.scrollUpAlt, settings.scrollDownAlt);
        printBlankSpaces(indent);
        printf(" - Enter a number then Enter to switch song.\n");
        printBlankSpaces(indent);
        printf(" - Space (or %s) to toggle pause.\n", settings.togglePause);
        printBlankSpaces(indent);
        printf(" - %s toggle color derived from album or from profile.\n", settings.toggleColorsDerivedFrom);
        printBlankSpaces(indent);
        printf(" - %s to show/hide the spectrum visualizer.\n", settings.toggleVisualizer);
        printBlankSpaces(indent);
        printf(" - %s to show/hide album covers.\n", settings.toggleCovers);
        printBlankSpaces(indent);
        printf(" - %s to toggle album covers drawn in ascii.\n", settings.toggleAscii);
        printBlankSpaces(indent);
        printf(" - %s to repeat the current song.\n", settings.toggleRepeat);
        printBlankSpaces(indent);
        printf(" - %s to shuffle the playlist.\n", settings.toggleShuffle);
        printBlankSpaces(indent);
        printf(" - %s to seek backward.\n", settings.seekBackward);
        printBlankSpaces(indent);
        printf(" - %s to seek forward.\n", settings.seekForward);
        printBlankSpaces(indent);
        printf(" - %s to save the playlist to your music folder.\n", settings.savePlaylist);
        printBlankSpaces(indent);
        printf(" - %s to add current song to kew.m3u (run with \"kew .\").\n", settings.addToMainPlaylist);
        printBlankSpaces(indent);
        printf(" - %s to quit.\n", settings.quit);
        printf("\n");
        printLastRow();

        numPrintedRows += 19;

        return numPrintedRows;
}

void toggleShowPlaylist()
{
        refresh = true;

        if (appState.currentView == PLAYLIST_VIEW)
        {
                appState.currentView = SONG_VIEW;
        }
        else
        {
                appState.currentView = PLAYLIST_VIEW;
        }
}

void toggleShowLibrary()
{
        refresh = true;
        if (appState.currentView == LIBRARY_VIEW)
        {
                appState.currentView = SONG_VIEW;
        }
        else
        {
                appState.currentView = LIBRARY_VIEW;
        }
}

void showTrack()
{
        refresh = true;
        appState.currentView = SONG_VIEW;
}

void toggleShowKeyBindings()
{
        refresh = true;
        if (appState.currentView == KEYBINDINGS_VIEW)
        {
                appState.currentView = SONG_VIEW;
        }
        else
        {
                appState.currentView = KEYBINDINGS_VIEW;
        }
}

void flipNextPage()
{
        if (appState.currentView == LIBRARY_VIEW)
        {
                chosenLibRow += maxLibListSize - 1;
                startLibIter += maxLibListSize - 1;
                refresh = true;
        }
        else if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow += maxListSize - 1;
                refresh = true;
        }
}

void flipPrevPage()
{
        if (appState.currentView == LIBRARY_VIEW)
        {
                chosenLibRow -= maxLibListSize;
                startLibIter -= maxLibListSize;
                refresh = true;
        }
        else if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow -= maxListSize;
                refresh = true;
        }
}

void scrollNext()
{
        if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow++;
                refresh = true;
        }
        else if (appState.currentView == LIBRARY_VIEW)
        {
                chosenLibRow++;
                refresh = true;
        }
}

void scrollPrev()
{
        if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow--;
                refresh = true;
        }
        else if (appState.currentView == LIBRARY_VIEW)
        {
                chosenLibRow--;
                refresh = true;
        }
}

int getRowWithinBounds(int row)
{
        if (row >= originalPlaylist->count)
        {
                row = originalPlaylist->count - 1;
        }

        if (row < 0)
                row = 0;

        return row;
}

int showPlaylist(SongData *songData)
{
        Node *node = originalPlaylist->head;
        Node *foundNode = NULL;
        bool startFromCurrent = false;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int totalHeight = term_h;
        maxListSize = totalHeight;
        int numRows = 0;
        int numPrintedRows = 0;
        int foundAt = -1;

        maxListSize -= 1;
        numRows++;

        PixelData textColor = increaseLuminosity(color, 20);

        int aboutRows = printLogo(songData);
        maxListSize -= aboutRows;

        setColor();

        printBlankSpaces(indent);
        if (term_w > 52)
        {
                maxListSize -= 3;
                printf(" Use ↑, ↓ or k, j to choose. Enter to accept.\n");
                printBlankSpaces(indent);
                printf(" Pg Up and Pg Dn to scroll. Del to remove entry.\n\n");
        }

        int numSongs = 0;
        for (int i = 0; i < originalPlaylist->count; i++)
        {
                if (node == NULL)
                        break;

                if (currentSong != NULL && currentSong->id == node->id)
                {
                        foundAt = numSongs;
                        foundNode = node;
                }

                node = node->next;
                numSongs++;
                if (numSongs > maxListSize)
                {
                        startFromCurrent = true;
                        if (foundAt > -1)
                                break;
                }
        }

        if (startFromCurrent)
                node = foundNode;
        else
                node = originalPlaylist->head;

        if (chosenRow >= originalPlaylist->count)
        {
                chosenRow = originalPlaylist->count - 1;
        }

        chosenSong = chosenRow;
        chosenSong = (chosenSong < 0) ? 0 : chosenSong;

        if (chosenSong < startIter)
        {
                startIter = chosenSong;
        }

        if (chosenRow > startIter + maxListSize - round(maxListSize / 2))
        {
                startIter = chosenRow - maxListSize + round(maxListSize / 2);
        }

        if (startIter == 0 && chosenRow < 0)
        {
                chosenRow = 0;
        }

        if (resetPlaylistDisplay && !audioData.endOfListReached)
        {
                startIter = chosenRow = chosenSong = foundAt;
        }

        for (int i = foundAt; i > startIter; i--)
        {
                if (i > 0 && node->prev != NULL)
                        node = node->prev;
        }

        if (foundAt > -1)
        {
                for (int i = foundAt; i < startIter; i++)
                {
                        if (node->next != NULL)
                                node = node->next;
                }
        }

        for (int i = (startFromCurrent ? startIter : 0); i < (startFromCurrent ? startIter : 0) + maxListSize; i++)
        {
                if (node == NULL)
                        break;
                char filePath[MAXPATHLEN];
                c_strcpy(filePath, sizeof(filePath), node->song.filePath);
                char *lastSlash = strrchr(filePath, '/');
                char *lastDot = strrchr(filePath, '.');
                printf("\r");
                printBlankSpaces(indent);

                setColor();

                if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
                {
                        char copiedString[256];
                        strncpy(copiedString, lastSlash + 1, lastDot - lastSlash - 1);
                        copiedString[lastDot - lastSlash - 1] = '\0';
                        removeUnneededChars(copiedString);

                        if (i == chosenSong)
                        {
                                chosenNodeId = node->id;

                                if (useProfileColors)
                                        setTextColor(ENQUEUED_COLOR);
                                printf("\x1b[7m");
                        }

                        if (foundNode != NULL && foundNode->id == node->id)
                        {
                                if (useProfileColors)
                                {
                                        printf("\e[1m\e[39m");
                                }
                                else
                                        printf("\033[1;38;2;%03u;%03u;%03um", textColor.r, textColor.g, textColor.b);
                        }

                        shortenString(copiedString, term_w - 10 - indent);
                        trim(copiedString);

                        if (i + 1 < 10)
                                printf(" ");

                        if (startFromCurrent)
                        {
                                printf(" %d. %s \n", i + 1, copiedString);
                        }
                        else
                        {
                                printf(" %d. %s \n", numRows, copiedString);
                        }

                        numPrintedRows++;
                        numRows++;

                        setTextColorRGB2(color.r, color.g, color.b);
                        if (useProfileColors)
                                setDefaultTextColor();
                }
                node = node->next;
        }
        printf("\n");
        printLastRow();
        if (numRows > 1)
        {
                while (numPrintedRows < maxListSize)
                {
                        printf("\n");
                        numPrintedRows++;
                }
        }
        numPrintedRows += 2;
        numPrintedRows += aboutRows;
        if (term_w > 46)
        {
                numPrintedRows += 2;
        }
        resetPlaylistDisplay = false;
        return numPrintedRows;
}

void printElapsedBars(int elapsedBars)
{
        printBlankSpaces(indent);
        printf(" ");
        for (int i = 0; i < numProgressBars; i++)
        {
                if (i == 0)
                {
                        printf("■ ");
                }
                else if (i < elapsedBars)
                        printf("■ ");
                else
                {
                        printf("= ");
                }
        }
        printf("\n");
}

void printVisualizer()
{
        if (visualizerEnabled && appState.currentView == SONG_VIEW)
        {
                printf("\n");
                int term_w, term_h;
                getTermSize(&term_w, &term_h);
                int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
                visualizerWidth = (visualizerWidth < textWidth && textWidth < term_w - 2) ? textWidth : visualizerWidth;
                visualizerWidth = (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
                numProgressBars = (int)visualizerWidth / 2;

                drawSpectrumVisualizer(visualizerHeight, visualizerWidth, color);

                printElapsedBars(calcElapsedBars(elapsed, duration, numProgressBars));
                printLastRow();
                int jumpAmount = visualizerHeight + 2;
                cursorJump(jumpAmount);
                saveCursorPosition();
        }
        else if (!visualizerEnabled)
        {
                int term_w, term_h;
                getTermSize(&term_w, &term_h);
                if (term_w >= minWidth)
                {
                        printf("\n\n");
                        printLastRow();
                        cursorJump(2);
                }
        }
}

void calcIndent(SongData *songdata)
{
        if (songdata == NULL || appState.currentView != SONG_VIEW)
        {
                int textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
                indent = getCoverIndent(textWidth - 1) - 1;
                return;
        }

        int titleLength = strlen(songdata->metadata->title);
        int albumLength = strlen(songdata->metadata->album);
        int maxTextLength = (albumLength > titleLength) ? albumLength : titleLength;
        textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int maxSize = term_w - 2;
        if (titleLength > 0 && titleLength < maxSize && titleLength > textWidth)
                textWidth = titleLength;
        if (maxTextLength > 0 && maxTextLength < maxSize && maxTextLength > textWidth)
                textWidth = maxTextLength;
        if (textWidth > maxSize)
                textWidth = maxSize;

        indent = getCoverIndent(textWidth - 1) - 1;
}

FileSystemEntry *getCurrentLibEntry()
{
        return currentEntry;
}

FileSystemEntry *getLibrary()
{
        return library;
}

FileSystemEntry *getChosenDir()
{
        return chosenDir;
}

char *processName(const char *name, int maxWidth)
{
        char *lastDot = strrchr(name, '.');

        if (lastDot != NULL)
        {
                char copiedString[256];
                strncpy(copiedString, name, lastDot - name);
                copiedString[lastDot - name] = '\0';
                shortenString(copiedString, maxWidth);
                removeUnneededChars(copiedString);

                char *result = (char *)malloc(strlen(copiedString) + 1);
                if (result != NULL)
                {
                        strcpy(result, copiedString);
                        return result;
                }
        }
        return NULL;
}

void setCurrentAsChosenDir()
{
        if (currentEntry->isDirectory)
                chosenDir = currentEntry;
}

int displayTree(FileSystemEntry *root, int depth, int maxListSize, int maxNameWidth)
{
        if (libIter >= startLibIter + maxListSize)
        {
                return 0;
        }

        FileSystemEntry *tmp = root->parent == NULL ? NULL : root->parent->children;
        int numAudioChildren = 0;

        while (tmp != NULL)
        {
                if (!tmp->isDirectory)
                        numAudioChildren++;

                tmp = tmp->next;
        }

        if (chosenLibRow > startLibIter + maxListSize - round(maxListSize / 2))
        {
                startLibIter = chosenLibRow - maxListSize + round(maxListSize / 2) + 1;
        }

        if (allowChooseSongs)
        {
                if (chosenLibRow >= libIter + libSongIter && libSongIter != 0)
                {
                        if (chosenLibRow >= numDirectoryTreeEntries + numTopLevelSongs + numAudioChildren)
                        {
                                startLibIter = numDirectoryTreeEntries + numTopLevelSongs + numAudioChildren - maxListSize;
                                chosenLibRow = numDirectoryTreeEntries + numTopLevelSongs + numAudioChildren - 1;
                        }
                }
        }
        else
        {
                if (chosenLibRow >= numDirectoryTreeEntries + numTopLevelSongs)
                {
                        startLibIter = numDirectoryTreeEntries + numTopLevelSongs - maxListSize;
                        chosenLibRow = numDirectoryTreeEntries + numTopLevelSongs - 1;
                }
        }

        if (chosenLibRow < 0)
                startLibIter = chosenLibRow = libIter = 0;

        if (root->isDirectory ||
            (!root->isDirectory && depth == 1) ||
            (chosenDir != NULL && allowChooseSongs && root->parent != NULL && (strcmp(root->parent->fullPath, chosenDir->fullPath) == 0 || strcmp(root->fullPath, chosenDir->fullPath) == 0)))
        {
                if (depth > 0)
                {
                        if (libIter >= startLibIter)
                        {

                                if (depth == 1)
                                {
                                        if (useProfileColors)
                                                setTextColor(ARTIST_COLOR);
                                }
                                else
                                {
                                        setColor();
                                }

                                if (depth >= 2)
                                        printf("  ");

                                printBlankSpaces(indent);

                                if (chosenLibRow == libIter)
                                {
                                        if (root->isEnqueued)
                                        {
                                                if (useProfileColors)
                                                        setTextColor(ENQUEUED_COLOR);
                                                printf("\x1b[7m * ");
                                        }
                                        else
                                        {
                                                printf("  \x1b[7m ");
                                        }

                                        currentEntry = root;

                                        if (allowChooseSongs == true && (chosenDir == NULL ||
                                                                         (strcmp(currentEntry->parent->fullPath, chosenDir->fullPath) != 0 &&
                                                                          strcmp(root->fullPath, chosenDir->fullPath) != 0)))
                                        {
                                                chosenLibRow -= libSongIter;
                                                allowChooseSongs = false;
                                                chosenDir = NULL;
                                                refresh = true;
                                        }
                                }
                                else
                                {
                                        if (root->isEnqueued)
                                        {
                                                if (useProfileColors)
                                                        setTextColor(ENQUEUED_COLOR);
                                                printf(" * ");
                                        }
                                        else
                                                printf("   ");
                                }

                                if (root->isDirectory)
                                {
                                        shortenString(root->name, maxNameWidth);
                                        // if (chosenDir != NULL && strcmp(root->fullPath, chosenDir->fullPath) == 0)
                                        if (depth == 1)
                                                printf("%s \n", stringToUpper(root->name));
                                        else
                                                printf("%s \n", root->name);
                                }
                                else
                                {
                                        char *filename = processName(root->name, maxNameWidth);
                                        printf(" └─%s \n", filename);
                                        free(filename);
                                        libSongIter++;
                                }

                                setColor();
                        }

                        libIter++;
                }

                FileSystemEntry *child = root->children;
                while (child != NULL)
                {
                        if (displayTree(child, depth + 1, maxListSize, maxNameWidth) == -1)
                                return -1;
                        child = child->next;
                }
        }

        return 0;
}

void createLibrary()
{
        library = createDirectoryTree(settings.path, &numDirectoryTreeEntries);
}

void showLibrary(SongData *songData)
{
        libIter = 0;
        libSongIter = 0;
        startLibIter = 0;

        refresh = false;

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int totalHeight = term_h;
        maxLibListSize = totalHeight;
        setColor();
        int aboutSize = printLogo(songData);
        int maxNameWidth = term_w - 8 - indent;
        maxLibListSize -= aboutSize + 1;
        setColor();

        if (term_w > 60)
        {
                maxLibListSize -= 3;
                printBlankSpaces(indent);
                printf(" Use ↑, ↓ or k, j to choose. Enter to enqueue/dequeue.\n");
                printBlankSpaces(indent);
                printf(" Pg Up and Pg Dn to scroll.\n\n");
        }

        numTopLevelSongs = 0;

        displayTree(library, 0, maxLibListSize, maxNameWidth);

        printf("\n");

        printLastRow();

        if (refresh)
        {
                printf("\033[1;1H");
                clearScreen();
                showLibrary(songData);
        }
}

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist, bool isDeleted)
{
        if (!uiEnabled)
        {
                return 0;
        }

        hideCursor();
        setColor();

        if (songdata != NULL && !isDeleted && songdata->metadata != NULL && !songdata->hasErrors && (songdata->hasErrors < 1))
        {
                metadata = *songdata->metadata;
                totalDurationSeconds = playlist->totalDuration;
                elapsed = elapsedSeconds;
                duration = *songdata->duration;

                if (songdata->cover != NULL && coverEnabled)
                {
                        color.r = *songdata->red;
                        color.g = *songdata->green;
                        color.b = *songdata->blue;
                }
        }
        else
        {
                if (appState.currentView != LIBRARY_VIEW && appState.currentView != PLAYLIST_VIEW && appState.currentView != KEYBINDINGS_VIEW)
                {
                        appState.currentView = LIBRARY_VIEW;
                }

                color.r = defaultColor;
                color.g = defaultColor;
                color.b = defaultColor;
        }

        calcPreferredSize();
        calcIndent(songdata);

        if (preferredWidth <= 0 || preferredHeight <= 0)
                return -1;

        if (appState.currentView != PLAYLIST_VIEW)
                resetPlaylistDisplay = true;

        if (appState.currentView == KEYBINDINGS_VIEW && refresh)
        {
                clearScreen();
                showKeyBindings(songdata);
                saveCursorPosition();
                refresh = false;
        }
        else if (appState.currentView == PLAYLIST_VIEW && refresh)
        {
                clearScreen();
                int height = showPlaylist(songdata);
                cursorJump(height);
                saveCursorPosition();
                refresh = false;
        }
        else if (appState.currentView == LIBRARY_VIEW && refresh)
        {
                clearScreen();
                showLibrary(songdata);
                refresh = false;
        }
        else if (appState.currentView == SONG_VIEW && songdata != NULL)
        {
                if (refresh)
                {
                        clearScreen();
                        printf("\n");
                        printCover(songdata);
                        printMetadata(songdata->metadata);
                        refresh = false;
                }
                printTime(playlist);
                printVisualizer();
        }

        fflush(stdout);

        return 0;
}

void showHelp()
{
        printHelp();
}

void freeMainDirectoryTree()
{
        if (library != NULL)
                freeTree(library);
}
