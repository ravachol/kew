#include <string.h>
#include "player.h"

/*

player.c

Functions related to printing the player to the screen.

*/

#ifndef PIXELDATA_STRUCT
#define PIXELDATA_STRUCT
typedef struct
{
        unsigned char r;
        unsigned char g;
        unsigned char b;
} PixelData;
#endif

const char VERSION[] = "1.4.0";
const int LOGO_COLOR = 3;
const int VERSION_COLOR = 6;
const int ABSOLUTE_MIN_WIDTH = 38;
volatile bool refresh = true;
bool visualizerEnabled = true;
bool coverEnabled = true;
bool coverAnsi = false;
bool metaDataEnabled = true;
bool timeEnabled = true;
bool drewCover = true;
bool uiEnabled = true;
bool printInfo = false;
bool printKeyBindings = false;
bool showList = true;
bool resetPlaylistDisplay = true;
bool useProfileColors = true;
int numProgressBars = 15;
int elapsedBars = 0;
int chosenRow = 0;
int chosenSong = 0;
int startIter = 0;
int aboutHeight = 8;
int visualizerHeight = 8;
int minWidth = ABSOLUTE_MIN_WIDTH;
int minHeight = 2;
int maxWidth = 0;
int coverRow = 0;
int preferredWidth = 0;
int preferredHeight = 0;
int elapsed = 0;
int duration = 0;
int textWidth = 0;
char *tagsPath;
double totalDurationSeconds = 0.0;
PixelData color = {0, 0, 0};
PixelData bgColor = {90, 90, 90};
TagSettings metadata = {};

void setTextColorRGB2(int r, int g, int b)
{
        if (!useProfileColors)
                setTextColorRGB(r, g, b);
}

int calcMetadataHeight()
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        size_t titleLength = strlen(metadata.title);
        int titleHeight = (int)ceil((float)titleLength / term_w);
        size_t artistLength = strlen(metadata.artist);
        int artistHeight = (int)ceil((float)artistLength / term_w);
        size_t albumLength = strlen(metadata.album);
        int albumHeight = (int)ceil((float)albumLength / term_w);
        int yearHeight = 1;

        return titleHeight + artistHeight + albumHeight + yearHeight;
}

void calcPreferredSize()
{
        minHeight = 2 + (visualizerEnabled ? visualizerHeight : 0);
        calcIdealImgSize(&preferredWidth, &preferredHeight, (visualizerEnabled ? visualizerHeight : 0), calcMetadataHeight());
}

void printHelp()
{
        printf("cue - a command-line music player.\n");
        printf("\n");
        printf("Usage:    cue path \"path to music library\"\n");
        printf("          (Saves the music library path. Use this the first time. Ie: cue path \"/home/joe/Music/\")\n");
        printf("          cue (no argument, loads all your songs up to 10 000)\n");
        printf("          cue <song name,directory or playlist words>\n");
        printf("          cue --help, -? or -h\n");
        printf("          cue --version or -v\n");
        printf("          cue dir <album name> (Sometimes it's neccessary to specify it's a directory you want)\n");
        printf("          cue song <song name> \n");
        printf("          cue list <m3u list name> \n");
        printf("          cue shuffle <dir name> (random and rand works too)\n");
        printf("          cue artistA:artistB (plays artistA and artistB shuffled)");
        printf("\n");
        printf("Examples: cue moon (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
        printf("          play path \"/home/user/Music\"\n");
        printf("\n");
        printf("cue returns the first directory or file whose name matches the string you provide. ");
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

int printAsciiLogo()
{
        int minWidth = 31 + indent;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        if (term_w < minWidth)
                return 0;
        if (useProfileColors)
                setTextColor(LOGO_COLOR);
        printBlankSpaces(indent);
        printf("  $$$$$$$\\ $$\\   $$\\  $$$$$$\\\n");
        printBlankSpaces(indent);
        printf(" $$  _____|$$ |  $$ |$$  __$$\\\n");
        printBlankSpaces(indent);
        printf(" $$ /      $$ |  $$ |$$$$$$$$ |\n");
        printBlankSpaces(indent);
        printf(" $$ |      $$ |  $$ |$$   ____|\n");
        printBlankSpaces(indent);
        printf(" \\$$$$$$$\\ \\$$$$$$  |\\$$$$$$$\\\n");
        printBlankSpaces(indent);
        printf("  \\_______| \\______/  \\_______|\n");
        int printedRows = 6;
        return printedRows;
}

void printVersion(const char *version)
{
        if (useProfileColors)
                setTextColor(VERSION_COLOR);
        printBlankSpaces(indent);
        printf(" Version %s.\n", version);
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

                displayCover(songdata, preferredWidth, preferredHeight, coverAnsi) - 1;

                if (color.r == 0 && color.g == 0 && color.b == 0)
                {
                        color.r = 150;
                        color.g = 150;
                        color.b = 150;
                }
                drewCover = true;
        }
        else
        {
                color.r = 150;
                color.g = 150;
                color.b = 150;
                clearRestOfScreen();
                for (int i = 0; i < preferredHeight - 1; i++)
                {
                        printf("\n");
                }
                drewCover = false;
        }
}

void setColor()
{
        if (useProfileColors)
        {
                setDefaultTextColor();
                return;
        }
        if (color.r == 0 && color.g == 0 && color.b == 0)
                setDefaultTextColor();
        else if (color.r >= 210 && color.g >= 210 && color.b >= 210)
        {
                color.r = 150;
                color.g = 150;
                color.b = 150;
        }
        setTextColorRGB2(color.r, color.g, color.b);
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
                        gray.r = 150;
                        gray.g = 150;
                        gray.b = 150;
                        printf("\033[1;38;2;%03u;%03u;%03um", gray.r, gray.g, gray.b);
                }
                else
                {
                        printf("\033[1;38;2;%03u;%03u;%03um", pixel.r, pixel.g, pixel.b);
                }

                if (useProfileColors)
                        printf("\e[1m\e[39m");

                printWithDelay(metadata->title, 9, maxWidth - 2);

                // Alternative (no delay):
                // printf("\033[1K\r %.*s", maxWidth, metadata->title);
                // printf("\n");
        }
        cursorJumpDown(rows - 1);
}

void printProgress(double elapsed_seconds, double total_seconds, double total_duration_seconds, PlayList const *playlist)
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
        printf("\r");
        printBlankSpaces(indent);
        if (playlist->count <= MAX_COUNT_PLAYLIST_SONGS)
        {
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) T:%dh%02dm",
                       elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
                       total_hours, total_minutes, total_seconds_remainder,
                       progress_percentage, total_playlist_hours, total_playlist_minutes);
        }
        else
        {

                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%)",
                       elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
                       total_hours, total_minutes, total_seconds_remainder,
                       progress_percentage);
        }

        // Print out progress bar with 10 segments
        elapsedBars = (int)((elapsed_seconds / total_seconds) * numProgressBars);

        // Restore the cursor position
        printf("\033[u");
}
void printMetadata(TagSettings const *metadata)
{
        if (!metaDataEnabled || printInfo)
                return;
        c_sleep(100);
        setColor();
        printBasicMetadata(metadata);
}

void printTime(PlayList const *playlist)
{
        if (!timeEnabled || printInfo)
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

void printGlimmeringText(char *text, PixelData color)
{
        int textLength = strlen(text);
        int brightIndex = 0;

        PixelData vbright = increaseLuminosity(color, 160);
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

        char text[100] = " [F2 Playlist] [F3 Keys] [Q Quit]";

        if (repeatEnabled)
        {
                char repeatText[] = " R";
                strcat(text, repeatText);
        }

        if (shuffleEnabled)
        {
                char shuffleText[] = " S";
                strcat(text, shuffleText);
        }

        printf("\033[K"); // clear the line

        int randomNumber = getRandomNumber(1, 808);
        if (randomNumber == 808)
                printGlimmeringText(text, bgColor);
        else
        {
                printBlankSpaces(indent);
                printf("%s", text);
        }
}

void showVersion()
{
        printVersion(VERSION);
}

int printAbout()
{
        clearRestOfScreen();
        printf("\n\r");
        int numRows = 2;
        numRows += printAsciiLogo();
        printf("\n");
        showVersion();
        printf("\n");
        numRows += 3;

        return numRows;
}

void removeUnneededChars(char *str)
{
        int i;
        for (i = 0; i < 2 && str[i] != '\0' && str[i] != ' '; i++)
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

void shortenString(char *str, size_t width)
{
        if (strlen(str) > width)
        {
                str[width] = '\0';
        }
}

int showKeyBindings()
{
        PixelData textColor = increaseLuminosity(color, 20);
        int numPrintedRows = 1;
        if (useProfileColors)
                setDefaultTextColor();
        else
                setTextColorRGB2(textColor.r, textColor.g, textColor.b);
        numPrintedRows += printAbout();
        if (useProfileColors)
                setDefaultTextColor();
        else
                setTextColorRGB2(color.r, color.g, color.b);
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int indentation = indent;
        if (term_w < indent + 60 + indent)
                indentation = indent / 2;
        printBlankSpaces(indentation);
        printf(" - F2 to show/hide the playlist.\n");
        printBlankSpaces(indentation);
        printf(" - F3 to show/hide key bindings.\n");
        printBlankSpaces(indentation);
        printf(" - Use %s, %s to adjust volume.\n", settings.volumeUp, settings.volumeDown);
        printBlankSpaces(indentation);
        printf(" - Use ←, → or %s, %s keys to switch tracks.\n", settings.previousTrackAlt, settings.nextTrackAlt);
        printBlankSpaces(indentation);
        printf(" - Use ↑, ↓  or %s, %s keys to scroll through playlist.\n", settings.scrollUpAlt, settings.scrollDownAlt);
        printBlankSpaces(indentation);
        printf(" - Enter a Number then Enter, %s or %s to switch song.\n", settings.switchNumberedSongAlt, settings.switchNumberedSongAlt2);
        printBlankSpaces(indentation);
        printf(" - Space to toggle pause.\n");
        printBlankSpaces(indentation);
        printf(" - %s toggle color derived from album or from profile.\n", settings.toggleColorsDerivedFrom);
        printBlankSpaces(indentation);
        printf(" - %s to show/hide the spectrum visualizer.\n", settings.toggleVisualizer);
        printBlankSpaces(indentation);
        printf(" - %s to show/hide album covers.\n", settings.toggleCovers);
        printBlankSpaces(indentation);
        printf(" - %s to toggle album covers drawn in ascii.\n", settings.toggleAscii);
        printBlankSpaces(indentation);
        printf(" - %s to repeat the current song.\n", settings.toggleRepeat);
        printBlankSpaces(indentation);
        printf(" - %s to shuffle the playlist.\n", settings.toggleShuffle);
        printBlankSpaces(indentation);
        printf(" - %s to seek backward.\n", settings.seekBackward);
        printBlankSpaces(indentation);
        printf(" - %s to seek forward.\n", settings.seekForward);
        printBlankSpaces(indentation);
        printf(" - %s to save the playlist to your music folder.\n", settings.savePlaylist);
        printBlankSpaces(indentation);
        printf(" - %s to quit.\n", settings.quit);
        printf("\n");
        printLastRow();
        numPrintedRows += 15;
        return numPrintedRows;
}

void toggleShowPlaylist()
{
        refresh = true;
        printInfo = !printInfo;
        printKeyBindings = false;
}

void toggleShowKeyBindings()
{
        refresh = true;
        printKeyBindings = !printKeyBindings;
        printInfo = false;
}

void scrollNext()
{
        if (printInfo)
        {
                chosenRow++;
                refresh = true;
        }
}

void scrollPrev()
{
        if (printInfo)
        {
                chosenRow--;
                refresh = true;
        }
}

int showPlaylist()
{
        Node *node = originalPlaylist->head;
        Node *foundNode = originalPlaylist->head;
        bool foundCurrentSong = false;
        bool startFromCurrent = false;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int totalHeight = term_h;
        int maxListSize = totalHeight;
        int numRows = 0;
        int numPrintedRows = 0;
        int foundAt = 0;
        if (node == NULL)
                return numRows;

        maxListSize -= 1;
        numRows++;

        PixelData textColor = increaseLuminosity(color, 20);
        setTextColorRGB2(textColor.r, textColor.g, textColor.b);
        int aboutRows = printAbout();
        maxListSize -= aboutRows;

        setColor();
        setTextColorRGB2(color.r, color.g, color.b);
        printBlankSpaces(indent);
        if (term_w > 46)
        {
                maxListSize -= 2;
                printf(" Use ↑, ↓ or k, j to choose. Enter to accept.\n\n");
        }

        int numSongs = 0;
        for (int i = 0; i < originalPlaylist->count; i++)
        {
                if (strcmp(node->song.filePath, currentSong->song.filePath) == 0)
                {
                        foundAt = numSongs;
                        foundNode = node;
                }
                node = node->next;
                numSongs++;
                if (numSongs > maxListSize)
                {
                        startFromCurrent = true;
                        if (foundAt)
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

        if (chosenRow >= maxListSize - 1 && chosenRow > startIter + maxListSize - 1)
        {
                startIter = chosenSong - maxListSize + 1;
        }

        if (startIter == 0 && chosenRow < 0)
        {
                chosenRow = 0;
        }

        if (resetPlaylistDisplay)
        {
                startIter = chosenRow = chosenSong = foundAt;
        }
        for (int i = foundAt; i > startIter; i--)
        {
                if (i > 0 && node->prev != NULL)
                        node = node->prev;
        }

        for (int i = foundAt; i < startIter; i++)
        {
                if (node->next != NULL)
                        node = node->next;
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
                setTextColorRGB2(color.r, color.g, color.b);
                if (useProfileColors)
                        setDefaultTextColor();

                if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
                {
                        char copiedString[256];
                        strncpy(copiedString, lastSlash + 1, lastDot - lastSlash - 1);
                        copiedString[lastDot - lastSlash - 1] = '\0';
                        removeUnneededChars(copiedString);

                        if (i == chosenSong)
                        {
                                printf("\x1b[7m");
                        }

                        if (strcmp(filePath, currentSong->song.filePath) == 0)
                        {
                                if (useProfileColors)
                                        printf("\e[1m\e[39m");
                                else
                                        printf("\033[1;38;2;%03u;%03u;%03um", textColor.r, textColor.g, textColor.b);
                                foundCurrentSong = true;
                        }
                        shortenString(copiedString, term_w - 6 - indent);
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

void printElapsedBars()
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
        if (visualizerEnabled && !printInfo)
        {
                printf("\n");
                int term_w, term_h;
                getTermSize(&term_w, &term_h);
                int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
                visualizerWidth = (visualizerWidth < textWidth && textWidth < term_w - 2) ? textWidth : visualizerWidth;
                visualizerWidth = (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
                numProgressBars = (int)visualizerWidth / 2;
                drawSpectrumVisualizer(visualizerHeight, visualizerWidth, color);
                printElapsedBars();
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

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist)
{
        if (!uiEnabled || isPaused())
        {
                return 0;
        }

        metadata = *songdata->metadata;
        hideCursor();
        totalDurationSeconds = playlist->totalDuration;
        elapsed = elapsedSeconds;
        duration = *songdata->duration;

        calcPreferredSize();
        calcIndent(songdata);

        if (preferredWidth <= 0 || preferredHeight <= 0)
                return -1;

        if (!printInfo)
                resetPlaylistDisplay = true;

        if (printKeyBindings)
        {
                if (refresh)
                {
                        clearScreen();
                        showKeyBindings();
                        saveCursorPosition();
                }
        }
        else if (printInfo)
        {
                if (refresh)
                {
                        clearScreen();
                        int height = showPlaylist();
                        cursorJump(height);
                        saveCursorPosition();
                }
        }
        else
        {
                if (refresh)
                {
                        clearScreen();
                        printf("\n");
                        printCover(songdata);
                        printMetadata(songdata->metadata);
                }
                printTime(playlist);
                printVisualizer();
        }
        refresh = false;
        fflush(stdout);
        return 0;
}

void showHelp()
{
        printHelp();
}
