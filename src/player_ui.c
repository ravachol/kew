#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "imgfunc.h"
#include "directorytree.h"
#include "playlist.h"
#include "playlist_ui.h"
#include "search_ui.h"
#include "songloader.h"
#include "sound.h"
#include "term.h"
#include "utils.h"
#include "visuals.h"
#include "common_ui.h"
#include "player_ui.h"
#include "playerops.h"
/*

player_ui.c

 Functions related to printing the player to the screen.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef METADATA_MAX_SIZE
#define METADATA_MAX_SIZE 256
#endif

#ifdef __APPLE__
const int ABSOLUTE_MIN_WIDTH = 80;
#else
const int ABSOLUTE_MIN_WIDTH = 65;
#endif

bool fastForwarding = false;
bool rewinding = false;

int minHeight = 0;
int elapsedBars = 0;
int preferredWidth = 0;
int preferredHeight = 0;
int textWidth = 0;
int indent = 0;
int maxListSize = 0;
int maxSearchListSize = 0;
int numTopLevelSongs = 0;
int startLibIter = 0;
int startSearchIter = 0;
int maxLibListSize = 0;
int chosenRow = 0;             // The row that is chosen in playlist view
int chosenLibRow = 0;          // The row that is chosen in library view
int chosenSearchResultRow = 0; // The row that is chosen in search view
int libIter = 0;
int libSongIter = 0;
int libTopLevelSongIter = 0;
int previousChosenLibRow = 0;
int libCurrentDirSongCount = 0;
int lastRowRow = 0;
int lastRowCol = 0;
int progressBarRow = 0;
int progressBarCol = 0;
int progressBarLength = 0;
int draggedProgressBarCol = 0;
double draggedPositionSeconds = 0.0;
bool draggingProgressBar = false;
int miniVisualizerRow = 0;

PixelData lastRowColor = {120, 120, 120};

const char LIBRARY_FILE[] = "kewlibrary";

FileSystemEntry *currentEntry = NULL;
FileSystemEntry *lastEntry = NULL;
FileSystemEntry *chosenDir = NULL;
FileSystemEntry *library = NULL;

const char *LOGO[] = {
    " __\n",
    "|  |--.-----.--.--.--.\n",
    "|    <|  -__|  |  |  |\n",
    "|__|__|_____|________|"};

int calcIdealImgSize(int *width, int *height, const int visualizerHeight, const int metatagHeight)
{
        float aspectRatio = calcAspectRatio();

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        const int timeDisplayHeight = 1;
        const int heightMargin = 4;
        const int minHeight = visualizerHeight + metatagHeight + timeDisplayHeight + heightMargin + 1;
        const int minBorderWidth = 0;

        *height = term_h - minHeight;
        *width = (int)ceil((double)(*height) * aspectRatio);

        if (*width > term_w)
        {
                *width = term_w - minBorderWidth;
                *height = (int)floor((double)(*width) / aspectRatio);
        }

        if (*width > INT_MAX)
                *width = INT_MAX;
        if (*height > INT_MAX)
                *height = INT_MAX;

        *height -= 1;

        return 0;
}

void calcPreferredSize(UISettings *ui)
{
        minHeight = 2 + (ui->visualizerEnabled ? ui->visualizerHeight : 0);
        int metadataHeight = 4;
        calcIdealImgSize(&preferredWidth, &preferredHeight, (ui->visualizerEnabled ? ui->visualizerHeight : 0), metadataHeight);
}

void printHelp()
{
        printf(" kew - A terminal music player.\n"
               "\n"
               " \033[1;4mUsage:\033[0m   kew path \"path to music library\"\n"
               "          (Saves the music library path. Use this the first time. Ie: kew path \"/home/joe/Music/\")\n"
               "          kew (no argument, opens library)\n"
               "          kew all (loads all your songs up to 10 000)\n"
               "          kew albums (plays all albums up to 2000 randomly one after the other)"
               "          kew <song name,directory or playlist words>\n"
               "          kew --help, -? or -h\n"
               "          kew --version or -v\n"
               "          kew dir <album name> (Sometimes it's necessary to specify it's a directory you want)\n"
               "          kew song <song name> \n"
               "          kew list <m3u list name> \n"
               "          kew shuffle <dir name> (random and rand works too)\n"
               "          kew artistA:artistB (plays artistA and artistB shuffled)\n"
               "\n"
               " \033[1;4mExample:\033[0m kew moon\n"
               " (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n"
               "\n"
               " kew returns the first directory or file whose name partially matches the string you provide.\n"
               "\n"
               " Use quotes when providing strings with single quotes in them (') or vice versa.\n"
               " Enter to select or replay a song.\n"
               " Switch tracks with ←, → or h, l keys.\n"
               " Volume is adjusted with + (or =) and -.\n"
               " Space, p or right mouse to play or pause.\n"
               " Shift+s to stop.\n"
               " F2 to show/hide playlist view.\n"
               " F3 to show/hide library view.\n"
               " F4 to show/hide track view.\n"
               " F5 to show/hide search view.\n"
               " F6 to show/hide show/hide key bindings view.\n"
               " You can also use the mouse to switch views.\n"
               " u to update the library.\n"
               " v to toggle the spectrum visualizer.\n"
               " i to switch between using your regular color scheme or colors derived from the track cover.\n"
               " b to toggle album covers drawn in ascii or as a normal image.\n"
               " r to repeat the current song after playing.\n"
               " s to shuffle the playlist.\n"
               " a to seek back.\n"
               " d to seek forward.\n"
               " x to save the current playlist to a .m3u in your music folder named after the first song.\n"
               " Tab to switch to next view.\n"
               " Shift+Tab to switch to previous view.\n"
               " Backspace to clear the playlist.\n"
               " Delete to remove a single playlist entry.\n"
               " gg go to first song.\n"
               " number + G or Enter to go to specific song number in the playlist.\n"
               " G to go to last song.\n"
               " Esc or q to quit.\n"
               "\n");
}

int printLogo(SongData *songData, UISettings *ui)
{
        int height = 0;
        int logoWidth = 0;
        size_t logoHeight = sizeof(LOGO) / sizeof(LOGO[0]);

        PixelData rowColor;
        rowColor.r = defaultColor;
        rowColor.g = defaultColor;
        rowColor.b = defaultColor;

        if (!ui->hideLogo)
        {
                for (size_t i = 0; i < logoHeight; i++)
                {
                        if (!(ui->color.r == defaultColor && ui->color.g == defaultColor && ui->color.b == defaultColor))
                                rowColor = getGradientColor(ui->color, logoHeight - i, logoHeight, 2, 0.8f);

                        if (ui->useConfigColors)
                                setTextColor(ui->mainColor);
                        else
                                setColorAndWeight(false, rowColor, ui->useConfigColors);
                        printBlankSpaces(indent);
                        printf("%s", LOGO[i]);
                }

                logoWidth = 22;
                height += 3;
        }
        else
        {
                printf("\n");
                height += 1;
        }

        if (songData != NULL && songData->metadata != NULL)
        {
                int term_w, term_h;
                getTermSize(&term_w, &term_h);

                char title[MAXPATHLEN] = {0};

                int titleLength = strnlen(songData->metadata->title, METADATA_MAX_SIZE);
                char prettyTitle[titleLength + 1];

                strncpy(prettyTitle, songData->metadata->title, titleLength);
                prettyTitle[titleLength] = '\0';

                trim(prettyTitle, titleLength);

                if (ui->hideLogo && songData->metadata->artist[0] != '\0')
                {
                        printBlankSpaces(indent);
                        snprintf(title, MAXPATHLEN, "%s - %s",
                                 songData->metadata->artist, prettyTitle);
                }
                else
                {
                        if (ui->hideLogo)
                                printBlankSpaces(indent);
                        c_strcpy(title, prettyTitle, METADATA_MAX_SIZE - 1);
                        title[MAXPATHLEN - 1] = '\0';
                }

                shortenString(title, term_w - indent - indent - logoWidth - 4);

                if (ui->useConfigColors)
                        setTextColor(ui->titleColor);

                printf(" %s\n\n", title);
                height += 2;
        }
        else
        {
                printf("\n\n");
                height += 2;
        }

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

void printCoverCentered(SongData *songdata, UISettings *ui)
{
        clearScreen();

        if (songdata != NULL && songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        printSquareBitmapCentered(songdata->cover, songdata->coverWidth, songdata->coverHeight, preferredHeight);
                }
                else
                {
                        printInAsciiCentered(songdata->coverArtPath, preferredHeight);
                }
        }
        else
        {
                for (int i = 0; i <= preferredHeight; ++i)
                {
                        printf("\n");
                }
        }

        printf("\n\n");
}

void printCover(int height, SongData *songdata, UISettings *ui)
{
        int row = 2;
        int col = 2;
        int imgHeight = height - 2;

        clearScreen();

        if (songdata != NULL && songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        printSquareBitmap(row, col, songdata->cover, songdata->coverWidth, songdata->coverHeight, imgHeight);
                }
                else
                {
                        printInAscii(col, songdata->coverArtPath, imgHeight);
                }
        }
}

void printTitleWithDelay(int row, int col, const char *text, int delay, int maxWidth)
{
        int max = strnlen(text, maxWidth);

        if (max == maxWidth) // For long names
                max -= 2;    // Accommodate for the cursor that we display after the name.

        for (int i = 0; i <= max && delay; i++)
        {
                printf("\033[%d;%dH", row, col);
                printf("\033[K");

                for (int j = 0; j < i; j++)
                {
                        printf("%c", text[j]);
                }
                printf("█");
                fflush(stdout);

                c_sleep(delay);
        }
        if (delay)
                c_sleep(delay * 20);

        printf("\033[%d;%dH", row, col);
        printf("\033[K");
        printf("%.*s", maxWidth, text);
        printf("\n");
        fflush(stdout);
}

void printBasicMetadata(int row, int col, int maxWidth, TagSettings const *metadata, UISettings *ui)
{
        if (ui->useConfigColors)
                setDefaultTextColor();
        else
                setTextColorRGB(ui->color.r, ui->color.g, ui->color.b);

        if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0)
        {
                printf("\033[%d;%dH", row + 1, col);
                printf(" %.*s", maxWidth, metadata->artist);
        }

        if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0)
        {
                printf("\033[%d;%dH", row + 2, col);
                printf(" %.*s", maxWidth, metadata->album);
        }

        if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0)
        {
                printf("\033[%d;%dH", row + 3, col);
                int year = getYear(metadata->date);
                if (year == -1)
                        printf(" %s", metadata->date);
                else
                        printf(" %d", year);
        }

        PixelData pixel = increaseLuminosity(ui->color, 20);

        if (ui->useConfigColors)
        {
                setDefaultTextColor();
        }
        else if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
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

        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
        {
                // Clean up title before printing
                int titleLength = strnlen(metadata->title, maxWidth);
                char prettyTitle[titleLength + 1];

                c_strcpy(prettyTitle, metadata->title, titleLength + 1);

                trim(prettyTitle, titleLength);

                printTitleWithDelay(row, col + 1, prettyTitle, ui->titleDelay, maxWidth);
        }
}

int calcElapsedBars(double elapsedSeconds, double duration, int numProgressBars)
{
        if (elapsedSeconds == 0)
                return 0;

        return (int)((elapsedSeconds / duration) * numProgressBars);
}

void printProgress(double elapsed_seconds, double total_seconds, ma_uint32 sampleRate, int avgBitRate)
{
        int progressWidth = 39;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (term_w < progressWidth)
                return;

        int elapsed_hours = (int)(elapsed_seconds / 3600);
        int elapsed_minutes = (int)(((int)elapsed_seconds / 60) % 60);
        int elapsed_seconds_remainder = (int)elapsed_seconds % 60;

        int total_hours = (int)(total_seconds / 3600);
        int total_minutes = (int)(((int)total_seconds / 60) % 60);
        int total_seconds_remainder = (int)total_seconds % 60;

        int progress_percentage = (int)((elapsed_seconds / total_seconds) * 100);
        int vol = getCurrentVolume();

        if (total_seconds >= 3600)
        {
                // Song is more than 1 hour long: use full HH:MM:SS format
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                       elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
                       total_hours, total_minutes, total_seconds_remainder,
                       progress_percentage, vol);
        }
        else
        {
                // Song is less than 1 hour: use M:SS format
                int elapsed_total_minutes = elapsed_seconds / 60;
                int elapsed_secs = (int)elapsed_seconds % 60;
                int total_total_minutes = total_seconds / 60;
                int total_secs = (int)total_seconds % 60;

                printf(" %d:%02d / %d:%02d (%d%%) Vol:%d%%",
                       elapsed_total_minutes, elapsed_secs,
                       total_total_minutes, total_secs,
                       progress_percentage, vol);
        }

        double rate = ((float)sampleRate) / 1000;

        if (term_w > progressWidth + 10)
        {
                if (rate == (int)rate)
                        printf(" %dkHz", (int)rate);
                else
                        printf(" %.1fkHz", rate);
        }

        if (term_w > progressWidth + 19)
        {
                if (avgBitRate > 0)
                        printf(" %dkb/s ", avgBitRate);
        }
}

void printTime(int row, int col, double elapsedSeconds, ma_uint32 sampleRate, int avgBitRate, AppState *state)
{
        if (state->uiSettings.useConfigColors)
                setDefaultTextColor();
        else
                setTextColorRGB(state->uiSettings.color.r, state->uiSettings.color.g, state->uiSettings.color.b);
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        printf("\033[%d;%dH", row, col);
        if (term_h > minHeight)
        {
                double duration = getCurrentSongDuration();
                double elapsed = elapsedSeconds;

                printProgress(elapsed, duration, sampleRate, avgBitRate);
        }
}

int calcIndentNormal(void)
{
        int textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
        return getIndentation(textWidth - 1) - 1;
}

int calcIndentTrackView(TagSettings *metadata)
{
        if (metadata == NULL)
                return calcIndentNormal();

        int titleLength = strnlen(metadata->title, METADATA_MAX_LENGTH);
        int albumLength = strnlen(metadata->album, METADATA_MAX_LENGTH);
        int maxTextLength = (albumLength > titleLength) ? albumLength : titleLength;
        textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int maxSize = term_w - 2;
        if (maxTextLength > 0 && maxTextLength < maxSize && maxTextLength > textWidth)
                textWidth = maxTextLength;
        if (textWidth > maxSize)
                textWidth = maxSize;

        return getIndentation(textWidth - 1) - 1;
}

void calcIndent(SongData *songdata)
{
        if ((appState.currentView == TRACK_VIEW && songdata == NULL) || appState.currentView != TRACK_VIEW)
        {
                indent = calcIndentNormal();
        }
        else
        {
                indent = calcIndentTrackView(songdata->metadata);
        }
}

int getIndent()
{
        return indent;
}

void printGlimmeringText(int row, int col, char *text, int textLength, char *nerdFontText, PixelData color)
{
        int brightIndex = 0;
        PixelData vbright = increaseLuminosity(color, 120);
        PixelData bright = increaseLuminosity(color, 60);

        printf("\033[%d;%dH", row, col);

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
                printf("\033[%d;%dH", row, col);
        }
}

void printErrorRow(int row, int col)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        printf("\033[%d;%dH", row, col);

        if (!hasPrintedError && hasErrorMessage())
        {
                setTextColorRGB(lastRowColor.r, lastRowColor.g, lastRowColor.b);
                printf(" %s", getErrorMessage());
                hasPrintedError = true;
                fflush(stdout);
        }
        else
        {
                printf("\033[K"); // Clear the rest of the line
        }
}

void formatWithShiftPlus(char *dest, size_t size, const char *src)
{
        if (isupper((unsigned char)src[0]))
        {
                snprintf(dest, size, "Shift+%s", src);
        }
        else
        {
                snprintf(dest, size, "%s", src);
        }
}

void printLastRow(int row, int col, UISettings *ui, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (preferredWidth < 0 || preferredHeight < 0) // mini view
                return;

        lastRowRow = row;
        lastRowCol = col;

        printf("\033[%d;%dH", row, col);

        setTextColorRGB(lastRowColor.r, lastRowColor.g, lastRowColor.b);

        char text[100];
#ifdef __APPLE__
        char playlist[8], library[8], track[8], search[8], help[8];

        // Assume settings->showPlaylistAlt etc. are defined properly
        formatWithShiftPlus(playlist, sizeof(playlist), settings->showPlaylistAlt);
        formatWithShiftPlus(library, sizeof(library), settings->showLibraryAlt);
        formatWithShiftPlus(track, sizeof(track), settings->showTrackAlt);
        formatWithShiftPlus(search, sizeof(search), settings->showSearchAlt);
        formatWithShiftPlus(help, sizeof(help), settings->showKeysAlt);

        snprintf(text, sizeof(text), "%s Playlist|%s Library|%s Track|%s Search|%s Help",
                 playlist, library, track, search, help);

#else
        (void)settings;
        strcpy(text, LAST_ROW);
#endif

        char nerdFontText[100] = "";

        size_t maxLength = sizeof(nerdFontText);

        size_t currentLength = strnlen(nerdFontText, maxLength);

        if (term_w >= ABSOLUTE_MIN_WIDTH)
        {
                if (isPaused())
                {
                        char pauseText[] = " ⏸";
                        snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else if (isStopped())
                {
                        char pauseText[] = " ■";
                        snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else
                {
                        char pauseText[] = " ▶";
                        snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
        }

        if (isRepeatEnabled())
        {
                char repeatText[] = " \u27f3";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }
        else if (isRepeatListEnabled())
        {
                char repeatText[] = " \u27f3L";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }

        if (isShuffleEnabled())
        {
                char shuffleText[] = " \uf074";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", shuffleText);
                currentLength += strlen(shuffleText);
        }

        if (fastForwarding)
        {
                char forwardText[] = " \uf04e";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", forwardText);
                currentLength += strlen(forwardText);
        }

        if (rewinding)
        {
                char rewindText[] = " \uf04a";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", rewindText);
                currentLength += strlen(rewindText);
        }

        printf("\033[K"); // Clear the line

        if (term_w < ABSOLUTE_MIN_WIDTH)
        {
                if (term_w > (int)currentLength + indent)
                {
                        printf("%s", nerdFontText); // Print just the shuffle and replay settings
                }
                return;
        }

        int textLength = strnlen(text, 100);
        int randomNumber = getRandomNumber(1, 808);

        if (randomNumber == 808 && !ui->hideGlimmeringText)
                printGlimmeringText(row, col, text, textLength, nerdFontText, lastRowColor);
        else
        {
                printf("%s", text);
                printf("%s", nerdFontText);
        }
}

void calcAndPrintLastRowAndErrorRow(UISettings *ui, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        printErrorRow(term_h - 1, indent);
        printLastRow(term_h, indent, ui, settings);
}

int printAbout(SongData *songdata, UISettings *ui)
{
        clearScreen();
        int numRows = printLogo(songdata, ui);
        setDefaultTextColor();
        printBlankSpaces(indent);
        printf(" kew version: %s\n\n", VERSION);
        numRows += 2;

        return numRows;
}

int showKeyBindings(SongData *songdata, AppSettings *settings, UISettings *ui)
{
        int numPrintedRows = 0;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxListSize = term_h - 4;

        numPrintedRows += printAbout(songdata, ui);

        setDefaultTextColor();

        printBlankSpaces(indent);
        printf(" - Switch tracks with ←, → or %s, %s keys.\n", settings->previousTrackAlt, settings->nextTrackAlt);
        printBlankSpaces(indent);
        printf(" - Volume is adjusted with %s (or %s) and %s.\n", settings->volumeUp, settings->volumeUpAlt, settings->volumeDown);
        printBlankSpaces(indent);
        printf(" - Press F2 for Playlist View:\n");
        printBlankSpaces(indent);
        printf("   Use ↑, ↓ keys, %s, %s keys, or mouse scroll to scroll.\n", settings->scrollUpAlt, settings->scrollDownAlt);
        printBlankSpaces(indent);
        printf("   Press Enter or middle click to play.\n");
        printBlankSpaces(indent);
        printf("   Press Backspace to clear the list or Delete to remove an entry.\n");
        printBlankSpaces(indent);
        printf(" - Press F3 for Library View:\n");
        printBlankSpaces(indent);
        printf("   Use ↑, ↓ keys, %s, %s keys, or mouse scroll to scroll.\n", settings->scrollUpAlt, settings->scrollDownAlt);
        printBlankSpaces(indent);
        printf("   Press Enter or middle click to add/remove songs.\n");
        printBlankSpaces(indent);
        printf(" - Press F4 for Track View.\n");
        printBlankSpaces(indent);
        printf(" - Space, %s, or right click to play or pause.\n", settings->togglePause);
        printBlankSpaces(indent);
        printf(" - Shift+s to stop.\n");
        printBlankSpaces(indent);
        printf(" - You can also use the mouse to switch views.\n");
        printBlankSpaces(indent);
        printf(" - %s toggle color derived from album or from profile.\n", settings->toggleColorsDerivedFrom);
        printBlankSpaces(indent);
        printf(" - %s to update the library.\n", settings->updateLibrary);
        printBlankSpaces(indent);
        printf(" - %s to show/hide the spectrum visualizer.\n", settings->toggleVisualizer);
        printBlankSpaces(indent);
        printf(" - %s to toggle album covers drawn in ascii.\n", settings->toggleAscii);
        printBlankSpaces(indent);
        printf(" - %s to repeat the current song after playing.\n", settings->toggleRepeat);
        printBlankSpaces(indent);
        printf(" - %s to shuffle the playlist.\n", settings->toggleShuffle);
        printBlankSpaces(indent);
        printf(" - %s to seek backward.\n", settings->seekBackward);
        printBlankSpaces(indent);
        printf(" - %s to seek forward.\n", settings->seekForward);
        printBlankSpaces(indent);
        printf(" - %s to save the playlist to your music folder,\n", settings->savePlaylist);
        printBlankSpaces(indent);
        printf("   in an .m3u file named after the first song in the playlist.\n");
        printBlankSpaces(indent);
        printf(" - Esc or %s to quit.\n\n", settings->quit);
        printBlankSpaces(indent);
        printf(" Project URL: https://github.com/ravachol/kew\n");
        printBlankSpaces(indent);
        printf(" Copyright © 2022-2025 Ravachol.\n");
        printf("\n");

        numPrintedRows += 27;

        while (numPrintedRows < maxListSize)
        {
                printf("\n");
                numPrintedRows++;
        }

        calcAndPrintLastRowAndErrorRow(ui, settings);

        numPrintedRows++;

        return numPrintedRows;
}

void toggleShowView(ViewState viewToShow)
{
        refresh = true;

        if (appState.currentView == viewToShow)
        {
                appState.currentView = TRACK_VIEW;
        }
        else
        {
                appState.currentView = viewToShow;
        }
}

void switchToNextView(void)
{
        switch (appState.currentView)
        {
        case PLAYLIST_VIEW:
                appState.currentView = LIBRARY_VIEW;
                break;
        case LIBRARY_VIEW:
                appState.currentView = (currentSong != NULL) ? TRACK_VIEW : SEARCH_VIEW;
                break;
        case TRACK_VIEW:
                appState.currentView = SEARCH_VIEW;
                break;
        case SEARCH_VIEW:
                appState.currentView = KEYBINDINGS_VIEW;
                break;
        case KEYBINDINGS_VIEW:
                appState.currentView = PLAYLIST_VIEW;
                break;
        }
        refresh = true;
}

void switchToPreviousView(void)
{
        switch (appState.currentView)
        {
        case PLAYLIST_VIEW:
                appState.currentView = KEYBINDINGS_VIEW;
                break;
        case LIBRARY_VIEW:
                appState.currentView = PLAYLIST_VIEW;
                break;
        case TRACK_VIEW:
                appState.currentView = LIBRARY_VIEW;
                break;
        case SEARCH_VIEW:
                appState.currentView = (currentSong != NULL) ? TRACK_VIEW : LIBRARY_VIEW;
                break;
        case KEYBINDINGS_VIEW:
                appState.currentView = SEARCH_VIEW;
                break;
        }
        refresh = true;
}

void showTrack(void)
{
        refresh = true;
        appState.currentView = TRACK_VIEW;
}

void flipNextPage(void)
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
                chosenRow = (chosenRow >= originalPlaylist->count) ? originalPlaylist->count - 1 : chosenRow;
                refresh = true;
        }
        else if (appState.currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow += maxSearchListSize - 1;
                chosenSearchResultRow = (chosenSearchResultRow >= getSearchResultsCount()) ? getSearchResultsCount() - 1 : chosenSearchResultRow;
                startSearchIter += maxSearchListSize - 1;
                refresh = true;
        }
}

void flipPrevPage(void)
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
                chosenRow = (chosenRow > 0) ? chosenRow : 0;
                refresh = true;
        }
        else if (appState.currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow -= maxSearchListSize;
                chosenSearchResultRow = (chosenSearchResultRow > 0) ? chosenSearchResultRow : 0;
                startSearchIter -= maxSearchListSize;
                refresh = true;
        }
}

void scrollNext(void)
{
        if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow++;
                chosenRow = (chosenRow >= originalPlaylist->count) ? originalPlaylist->count - 1 : chosenRow;
                refresh = true;
        }
        else if (appState.currentView == LIBRARY_VIEW)
        {
                previousChosenLibRow = chosenLibRow;
                chosenLibRow++;
                refresh = true;
        }
        else if (appState.currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow++;
                refresh = true;
        }
}

void scrollPrev(void)
{
        if (appState.currentView == PLAYLIST_VIEW)
        {
                chosenRow--;
                chosenRow = (chosenRow > 0) ? chosenRow : 0;
                refresh = true;
        }
        else if (appState.currentView == LIBRARY_VIEW)
        {
                previousChosenLibRow = chosenLibRow;
                chosenLibRow--;
                refresh = true;
        }
        else if (appState.currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow--;
                chosenSearchResultRow = (chosenSearchResultRow > 0) ? chosenSearchResultRow : 0;
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

int printLogoAndAdjustments(SongData *songData, int termWidth, UISettings *ui, int indentation)
{
        int aboutRows = printLogo(songData, ui);
        if (termWidth > 52 && !ui->hideHelp)
        {
                setDefaultTextColor();
                printBlankSpaces(indentation);
                printf(" Use ↑, ↓ or k, j to choose. Enter=Accept.\n");
                printBlankSpaces(indentation);
#ifndef __APPLE__
                printf(" Pg Up and Pg Dn to scroll. Del to remove an entry.\n");
#else
                printf(" Fn+Arrow Up and Fn+Arrow Down to scroll. Del to remove an entry.\n");
#endif
                printBlankSpaces(indentation);
                printf(" Backspace to clear. Use t, g to move the songs.\n\n");
                return aboutRows + 4;
        }
        return aboutRows;
}

void showSearch(SongData *songData, int *chosenRow, UISettings *ui, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxSearchListSize = term_h - 3;

        int aboutRows = printLogo(songData, ui);
        maxSearchListSize -= aboutRows;

        setDefaultTextColor();

        if (term_w > indent + 38 && !ui->hideHelp)
        {
                printBlankSpaces(indent);
                printf(" Use ↑, ↓ to choose. Enter=Enqueue. Alt+Enter=Play.\n\n");
                maxSearchListSize -= 2;
        }

        displaySearch(maxSearchListSize, indent, chosenRow, startSearchIter, ui);

        calcAndPrintLastRowAndErrorRow(ui, settings);
}

void showPlaylist(SongData *songData, PlayList *list, int *chosenSong, int *chosenNodeId, AppState *state, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxListSize = term_h - 3;

        // Setup scrolling names
        if (getIsLongName() && isSameNameAsLastTime && updateCounter % scrollingInterval != 0)
        {
                updateCounter++;
                refresh = true;
                return;
        }
        else
                refresh = false;

        clearScreen();

        int aboutRows = printLogoAndAdjustments(songData, term_w, &(state->uiSettings), indent);
        maxListSize -= aboutRows;

        if (state->uiSettings.useConfigColors)
                setTextColor(state->uiSettings.artistColor);
        else
                setColor(&(state->uiSettings));

        printBlankSpaces(indent);
        printf("   ─ PLAYLIST ─\n");
        maxListSize -= 1;

        displayPlaylist(list, maxListSize, indent, chosenSong, chosenNodeId, state->uiState.resetPlaylistDisplay, state);

        calcAndPrintLastRowAndErrorRow(&(state->uiSettings), settings);
}

void resetSearchResult(void)
{
        chosenSearchResultRow = 0;
}

void printProgressBar(int row, int col, AppSettings *settings, UISettings *ui, int elapsedBars, int numProgressBars)
{
        PixelData color = ui->color;
        bool useConfigColors = ui->useConfigColors;

        progressBarRow = row;
        progressBarCol = col + 1;
        progressBarLength = numProgressBars;

        printf("\033[%d;%dH", row, col + 1);

        for (int i = 0; i < numProgressBars; i++)
        {
                if (i > elapsedBars)
                {
                        if (!useConfigColors)
                        {
                                PixelData tmp = increaseLuminosity(color, 50);
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g, tmp.b);
                        }
                        else
                        {
                                setTextColorRGB(lastRowColor.r, lastRowColor.g, lastRowColor.b);
                        }

                        if (i % 2 == 0)
                                printf("%s", settings->progressBarApproachingEvenChar);
                        else
                                printf("%s", settings->progressBarApproachingOddChar);

                        continue;
                }

                if (!useConfigColors)
                {
                        printf("\033[38;2;%d;%d;%dm", color.r, color.g, color.b);
                }
                else
                {
                        setDefaultTextColor();
                }

                if (i < elapsedBars)
                {
                        if (i % 2 == 0)
                                printf("%s", settings->progressBarElapsedEvenChar);
                        else
                                printf("%s", settings->progressBarElapsedOddChar);
                }
                else if (i == elapsedBars)
                {
                        if (i % 2 == 0)
                                printf("%s", settings->progressBarCurrentEvenChar);
                        else
                                printf("%s", settings->progressBarCurrentOddChar);
                }
        }
}

void printVisualizer(int row, int col, int visualizerWidth, AppSettings *settings, double elapsedSeconds, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        if (ui->visualizerEnabled)
        {
                uis->numProgressBars = (int)visualizerWidth / 2;
                double duration = getCurrentSongDuration();

                drawSpectrumVisualizer(row, col, state);

                int height = state->uiSettings.visualizerHeight;
                int elapsedBars = calcElapsedBars(elapsedSeconds, duration, visualizerWidth);

                printProgressBar(row + height - 1, col, settings, ui, elapsedBars, visualizerWidth - 1);
        }
}

FileSystemEntry *getCurrentLibEntry(void)
{
        return currentEntry;
}

FileSystemEntry *getLibrary(void)
{
        return library;
}

FileSystemEntry *getChosenDir(void)
{
        return chosenDir;
}

void setChosenDir(FileSystemEntry *entry)
{
        if (entry == NULL)
        {
                return;
        }

        if (entry->isDirectory)
        {
                currentEntry = chosenDir = entry;
        }
}

void setCurrentAsChosenDir(void)
{
        if (currentEntry->isDirectory)
                chosenDir = currentEntry;
}

void resetChosenDir(void)
{
        chosenDir = NULL;
}

int displayTree(FileSystemEntry *root, int depth, int maxListSize, int maxNameWidth, AppState *state)
{
        if (maxNameWidth < 0)
                maxNameWidth = 0;

        char dirName[maxNameWidth + 1];
        char filename[maxNameWidth + 1];
        bool foundChosen = false;
        int foundCurrent = 0;
        int extraIndent = 0;

        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        if (currentSong != NULL && (strcmp(currentSong->song.filePath, root->fullPath) == 0))
        {
                foundCurrent = 1;
        }

        if (startLibIter < 0)
                startLibIter = 0;

        if (libIter >= startLibIter + maxListSize)
        {
                return false;
        }

        int threshold = startLibIter + (maxListSize + 1) / 2;
        if (chosenLibRow > threshold)
        {
                startLibIter = chosenLibRow - maxListSize / 2 + 1;
        }

        if (chosenLibRow < 0)
                startLibIter = chosenLibRow = libIter = 0;

        if (root == NULL)
                return false;

        PixelData rowColor;
        rowColor.r = defaultColor;
        rowColor.g = defaultColor;
        rowColor.b = defaultColor;

        if (!(ui->color.r == defaultColor && ui->color.g == defaultColor && ui->color.b == defaultColor))
                rowColor = getGradientColor(ui->color, libIter - startLibIter, maxListSize, maxListSize / 2, 0.7f);

        if (!(root->isDirectory ||
              (!root->isDirectory && depth == 1) ||
              (root->isDirectory && depth == 0) ||
              (chosenDir != NULL && uis->allowChooseSongs && root->parent != NULL && (strcmp(root->parent->fullPath, chosenDir->fullPath) == 0 || strcmp(root->fullPath, chosenDir->fullPath) == 0))))
        {
                return foundChosen;
        }

        if (depth >= 0)
        {
                if (currentEntry != NULL && currentEntry != lastEntry && !currentEntry->isDirectory && currentEntry->parent != NULL && currentEntry->parent == chosenDir)
                {
                        FileSystemEntry *tmpc = currentEntry->parent->children;

                        libCurrentDirSongCount = 0;

                        while (tmpc != NULL)
                        {
                                if (!tmpc->isDirectory)
                                        libCurrentDirSongCount++;
                                tmpc = tmpc->next;
                        }

                        lastEntry = currentEntry;
                }

                if (libIter >= startLibIter)
                {

                        if (depth <= 1)
                        {
                                if (ui->useConfigColors)
                                        setTextColor(ui->artistColor);
                                else
                                        setColorAndWeight(0, rowColor, ui->useConfigColors);
                        }
                        else
                        {
                                setDefaultTextColor();
                        }

                        if (depth >= 2)
                                printf("  ");

                        // If more than two levels deep add an extra indentation
                        extraIndent = (depth - 2 <= 0) ? 0 : depth - 2;

                        printBlankSpaces(indent + extraIndent);

                        if (chosenLibRow == libIter)
                        {
                                if (root->isEnqueued)
                                {
                                        if (ui->useConfigColors)
                                                setTextColor(ui->enqueuedColor);
                                        else
                                                setColorAndWeight(0, rowColor, ui->useConfigColors);

                                        printf("\x1b[7m * ");
                                }
                                else
                                {
                                        printf("  \x1b[7m ");
                                }

                                currentEntry = root;

                                if (uis->allowChooseSongs == true && (chosenDir == NULL ||
                                                                      (currentEntry != NULL && currentEntry->parent != NULL && chosenDir != NULL && !isContainedWithin(currentEntry, chosenDir) && strcmp(root->fullPath, chosenDir->fullPath) != 0)))
                                {
                                        uis->collapseView = true;
                                        refresh = true;

                                        if (!uis->openedSubDir)
                                        {

                                                uis->allowChooseSongs = false;
                                                chosenDir = NULL;
                                        }
                                }

                                foundChosen = true;
                        }
                        else
                        {
                                if (root->isEnqueued)
                                {
                                        if (ui->useConfigColors)
                                                printf("\033[%d;3%dm", foundCurrent, ui->enqueuedColor);
                                        else
                                                setColorAndWeight(foundCurrent, rowColor, ui->useConfigColors);

                                        printf(" * ");
                                }
                                else
                                {
                                        printf("   ");
                                }
                        }

                        if (maxNameWidth < extraIndent)
                                maxNameWidth = extraIndent;

                        if (root->isDirectory)
                        {
                                dirName[0] = '\0';

                                if (strcmp(root->name, "root") == 0)
                                        snprintf(dirName, maxNameWidth + 1 - extraIndent, "%s", "─ MUSIC LIBRARY ─");
                                else
                                        snprintf(dirName, maxNameWidth + 1 - extraIndent, "%s", root->name);

                                char *upperDirName = stringToUpper(dirName);

                                if (depth == 1)
                                        printf("%s \n", upperDirName);
                                else
                                {
                                        printf("%s \n", dirName);
                                }
                                free(upperDirName);
                        }
                        else
                        {
                                filename[0] = '\0';

                                isSameNameAsLastTime = (previousChosenLibRow == chosenLibRow);

                                if (foundChosen)
                                {
                                        previousChosenLibRow = chosenLibRow;
                                }

                                if (!isSameNameAsLastTime)
                                {
                                        resetNameScroll();
                                }

                                if (foundChosen)
                                {
                                        processNameScroll(root->name, filename, maxNameWidth - extraIndent, isSameNameAsLastTime);
                                }
                                else
                                {
                                        processName(root->name, filename, maxNameWidth - extraIndent);
                                }

                                printf("└─ ");

                                if (foundCurrent && chosenLibRow != libIter)
                                {
                                        printf("\e[4m\e[1m");
                                }

                                printf("%s\n", filename);

                                libSongIter++;
                        }

                        setColor(ui);
                }

                libIter++;
        }

        FileSystemEntry *child = root->children;
        while (child != NULL)
        {
                if (displayTree(child, depth + 1, maxListSize, maxNameWidth, state))
                        foundChosen = true;

                child = child->next;
        }

        return foundChosen;
}

char *getLibraryFilePath(void)
{
        return getFilePath(LIBRARY_FILE);
}

void showLibrary(SongData *songData, AppState *state, AppSettings *settings)
{
        // For scrolling names, update every nth time
        if (getIsLongName() && isSameNameAsLastTime && updateCounter % scrollingInterval != 0)
        {
                refresh = true;
                return;
        }
        else
                refresh = false;

        clearScreen();

        if (state->uiState.collapseView)
        {
                if (previousChosenLibRow < chosenLibRow)
                {
                        if (!state->uiState.openedSubDir)
                        {
                                chosenLibRow -= libCurrentDirSongCount;
                                libCurrentDirSongCount = 0;
                        }
                        else
                        {
                                chosenLibRow -= state->uiState.numSongsAboveSubDir;
                                state->uiState.openedSubDir = false;
                                state->uiState.numSongsAboveSubDir = 0;
                                state->uiState.collapseView = false;
                        }
                }
                else
                {
                        if (state->uiState.openedSubDir)
                        {
                                chosenLibRow -= state->uiState.numSongsAboveSubDir;
                        }
                        libCurrentDirSongCount = 0;
                        state->uiState.openedSubDir = false;
                        state->uiState.numSongsAboveSubDir = 0;
                }
                state->uiState.collapseView = false;
        }

        UISettings *ui = &(state->uiSettings);

        libIter = 0;
        libSongIter = 0;
        startLibIter = 0;

        refresh = false;

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int totalHeight = term_h;
        maxLibListSize = totalHeight;
        setColor(ui);
        int aboutSize = printLogo(songData, ui);
        int maxNameWidth = term_w - 10 - indent;
        maxLibListSize -= aboutSize + 2;

        setDefaultTextColor();

        if (term_w > 67 && !ui->hideHelp)
        {
                maxLibListSize -= 3;
                printBlankSpaces(indent);
                printf(" Use ↑, ↓ or k, j to choose. Enter=Enqueue/Dequeue. Alt+Enter=Play.\n");
                printBlankSpaces(indent);
#ifndef __APPLE__
                printf(" Pg Up and Pg Dn to scroll. Press u to update, o to sort.\n\n");
#else
                printf(" Fn+Arrow Up and Fn+Arrow Down to scroll. u to update, o to sort.\n\n");
#endif
        }

        numTopLevelSongs = 0;

        FileSystemEntry *tmp = library->children;

        while (tmp != NULL)
        {
                if (!tmp->isDirectory)
                        numTopLevelSongs++;

                tmp = tmp->next;
        }

        bool foundChosen = displayTree(library, 0, maxLibListSize, maxNameWidth, state);

        if (!foundChosen)
        {
                chosenLibRow--;
                refresh = true;
        }

        for (int i = libIter - startLibIter; i < maxLibListSize; i++)
        {
                printf("\n");
        }

        calcAndPrintLastRowAndErrorRow(ui, settings);

        if (!foundChosen && refresh)
        {
                printf("\033[1;1H");
                clearScreen();
                showLibrary(songData, state, settings);
        }
}

int calcVisualizerWidth()
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
        visualizerWidth = (visualizerWidth < textWidth && textWidth < term_w - 2) ? textWidth : visualizerWidth;
        visualizerWidth = (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
        visualizerWidth -= 1;

        return visualizerWidth;
}

void showTrackViewMini(AppSettings *settings, SongData *songdata, AppState *state, double elapsedSeconds)
{
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int col = indent;

        if (refresh)
        {
                miniVisualizerRow = 1;

                printf("\n");
                miniVisualizerRow++;

                clearScreen();

                if (currentSong == NULL)
                {
                        int term_w, term_h;
                        getTermSize(&term_w, &term_h);

                        if (term_w > 21 && term_h > 4)
                        {
                                for (size_t i = 0; i < sizeof(LOGO) / sizeof(LOGO[0]); i++)
                                {
                                        printBlankSpaces(indent);
                                        printf("%s", LOGO[i]);
                                }
                                printf("\n");
                                printBlankSpaces(indent);
                                printf(" kew version: %s", VERSION);
                                miniVisualizerRow++;
                        }
                        return;
                }

                if (songdata)
                {
                        metadata = songdata->metadata;
                }

                calcIndentTrackView(metadata);

                if (state->uiSettings.useConfigColors)
                        setDefaultTextColor();
                else
                        setTextColorRGB(state->uiSettings.color.r, state->uiSettings.color.g, state->uiSettings.color.b);

                if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0 && strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
                {
                        printBlankSpaces(indent);

                        int text_len = textWidth - 3;
                        char combined[text_len + 1];
                        snprintf(combined, sizeof(combined), "%s - %s", metadata->artist, metadata->title);

                        printf("%.*s\n", text_len, combined);
                        miniVisualizerRow++;
                }
                else if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
                {
                        printBlankSpaces(indent);
                        printf("%.*s\n", textWidth, metadata->title);
                        miniVisualizerRow++;
                }

                refresh = false;
        }

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        bool doPrintTime = term_h > (state->uiSettings.visualizerHeight + 3);
        bool doPrintVis = term_h > (state->uiSettings.visualizerHeight + 2);

        if (songdata && doPrintTime)
        {
                ma_uint32 sampleRate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                getCurrentFormatAndSampleRate(&format, &sampleRate);
                printTime(miniVisualizerRow - 1, col, elapsedSeconds, sampleRate, avgBitRate, state);
        }
        if (doPrintVis)
        {
                int visualizerWidth = calcVisualizerWidth();
                printVisualizer(miniVisualizerRow, col, visualizerWidth, settings, elapsedSeconds, state);
        }
}

void showTrackViewLandscape(int height, int width, float aspectRatio, AppSettings *settings, SongData *songdata, AppState *state, double elapsedSeconds)
{
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;
        int timeHeight = 1;

        int col = height * aspectRatio;

        if (!state->uiSettings.coverEnabled)
                col = 1;

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int visualizerWidth = term_w - col;

        int row = height - metadataHeight - timeHeight - state->uiSettings.visualizerHeight - 3;

        if (refresh)
        {
                if (songdata)
                {
                        metadata = songdata->metadata;
                }

                printCover(height, songdata, &(state->uiSettings));
                if (height > metadataHeight)
                        printBasicMetadata(row, col, visualizerWidth-1, metadata, &(state->uiSettings));

                refresh = false;
        }
        if (songdata)
        {
                ma_uint32 sampleRate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                getCurrentFormatAndSampleRate(&format, &sampleRate);
                if (height > metadataHeight + timeHeight)
                        printTime(row + 4, col, elapsedSeconds, sampleRate, avgBitRate, state);
        }

        if (row > 0)
                printVisualizer(row + metadataHeight + 2, col, visualizerWidth, settings, elapsedSeconds, state);

        if (width - col > ABSOLUTE_MIN_WIDTH)
        {
                printErrorRow(row + metadataHeight + 2 + state->uiSettings.visualizerHeight, col);
                printLastRow(row + metadataHeight + 2 + state->uiSettings.visualizerHeight + 1, col, &(state->uiSettings), settings);
        }
}

void showTrackViewPortrait(int height, AppSettings *settings, SongData *songdata, AppState *state, double elapsedSeconds)
{
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;

        int row = height + 3;
        int col = indent;

        int visualizerWidth = calcVisualizerWidth();

        if (refresh)
        {
                if (songdata)
                {
                        metadata = songdata->metadata;
                }

                printCoverCentered(songdata, &(state->uiSettings));
                printBasicMetadata(row, col, visualizerWidth-1, metadata, &(state->uiSettings));

                refresh = false;
        }
        if (songdata)
        {
                ma_uint32 sampleRate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                getCurrentFormatAndSampleRate(&format, &sampleRate);
                printTime(row + metadataHeight, col, elapsedSeconds, sampleRate, avgBitRate, state);
        }

        printVisualizer(row + metadataHeight + 2, col, visualizerWidth, settings, elapsedSeconds, state);

        calcAndPrintLastRowAndErrorRow(&(state)->uiSettings, settings);
}

void showTrackView(int width, int height, AppSettings *settings, SongData *songdata, AppState *state, double elapsedSeconds)
{
        float aspect = getAspectRatio();

        if (aspect == 0.0f)
                aspect = 1.0f;

        int correctedWidth = width / aspect;

        if (correctedWidth > height * 2)
        {
                showTrackViewLandscape(height, width, aspect, settings, songdata, state, elapsedSeconds);
        }
        else
        {
                showTrackViewPortrait(preferredHeight, settings, songdata, state, elapsedSeconds);
        }
}

int printPlayer(SongData *songdata, double elapsedSeconds, AppSettings *settings, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        if (hasPrintedError && refresh)
                clearErrorMessage();

        if (!ui->uiEnabled)
        {
                return 0;
        }

        if (refresh)
        {
                hideCursor();
                setColor(ui);

                if (songdata != NULL && songdata->metadata != NULL && !songdata->hasErrors && (songdata->hasErrors < 1))
                {
                        ui->color.r = songdata->red;
                        ui->color.g = songdata->green;
                        ui->color.b = songdata->blue;

                        if (ui->trackTitleAsWindowTitle)
                                setTerminalWindowTitle(songdata->metadata->title);
                }
                else
                {
                        if (state->currentView == TRACK_VIEW)
                        {
                                state->currentView = LIBRARY_VIEW;
                        }

                        ui->color.r = defaultColor;
                        ui->color.g = defaultColor;
                        ui->color.b = defaultColor;

                        if (ui->trackTitleAsWindowTitle)
                                setTerminalWindowTitle("kew");
                }

                calcPreferredSize(ui);
                calcIndent(songdata);
        }

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        state->uiState.miniMode = false;

        if ((term_w <= 10 || term_h <= 8) || (preferredHeight <= 0 || preferredWidth <= 0))
        {
                state->uiState.miniMode = true;
                showTrackViewMini(settings, songdata, state, elapsedSeconds);
                fflush(stdout);
                return 0;
        }
        if (state->currentView != PLAYLIST_VIEW)
                state->uiState.resetPlaylistDisplay = true;

        if (state->currentView == KEYBINDINGS_VIEW && refresh)
        {
                clearScreen();
                showKeyBindings(songdata, settings, ui);
                saveCursorPosition();
                refresh = false;
                fflush(stdout);
        }
        else if (state->currentView == PLAYLIST_VIEW && refresh)
        {
                showPlaylist(songdata, originalPlaylist, &chosenRow, &(uis->chosenNodeId), state, settings);
                state->uiState.resetPlaylistDisplay = false;
                fflush(stdout);
        }
        else if (state->currentView == SEARCH_VIEW && refresh)
        {
                clearScreen();
                showSearch(songdata, &chosenSearchResultRow, ui, settings);
                refresh = false;
                fflush(stdout);
        }
        else if (state->currentView == LIBRARY_VIEW && refresh)
        {
                showLibrary(songdata, state, settings);
                fflush(stdout);
        }
        else if (state->currentView == TRACK_VIEW)
        {
                showTrackView(term_w, term_h, settings, songdata, state, elapsedSeconds);
                fflush(stdout);
        }

        return 0;
}

void showHelp(void)
{
        printHelp();
}

void freeMainDirectoryTree(AppState *state)
{
        if (library == NULL)
                return;

        char *filepath = getLibraryFilePath();

        if (state->uiSettings.cacheLibrary)
                freeAndWriteTree(library, filepath);
        else
                freeTree(library);

        free(filepath);
}

int getChosenRow(void)
{
        return chosenRow;
}

void setChosenRow(int row)
{
        chosenRow = row;
}
