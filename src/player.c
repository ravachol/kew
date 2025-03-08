#include "player.h"
#include "playerops.h"
/*

player.c

Functions related to printing the player to the screen.

*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef METADATA_MAX_SIZE
#define METADATA_MAX_SIZE 256
#endif

const char VERSION[] = "3.1.0";

#ifdef __APPLE__
const int ABSOLUTE_MIN_WIDTH = 80;
#else
const int ABSOLUTE_MIN_WIDTH = 68;
#endif

bool fastForwarding = false;
bool rewinding = false;

double pauseSeconds = 0.0;
double totalPauseSeconds = 0.0;
double seekAccumulatedSeconds = 0.0;

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

PixelData lastRowColor = {120, 120, 120};

const char LIBRARY_FILE[] = "kewlibrary";

FileSystemEntry *currentEntry = NULL;
FileSystemEntry *lastEntry = NULL;
FileSystemEntry *chosenDir = NULL;
FileSystemEntry *library = NULL;

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
        printf(" kew - a command-line music player.\n");
        printf("\n");
        printf(" \033[1;4mUsage:\033[0m   kew path \"path to music library\"\n");
        printf("          (Saves the music library path. Use this the first time. Ie: kew path \"/home/joe/Music/\")\n");
        printf("          kew (no argument, opens library)\n");
        printf("          kew all (loads all your songs up to 10 000)\n");
        printf("          kew albums (plays all albums up to 2000 randomly one after the other)");
        printf("          kew <song name,directory or playlist words>\n");
        printf("          kew --help, -? or -h\n");
        printf("          kew --version or -v\n");
        printf("          kew dir <album name> (Sometimes it's necessary to specify it's a directory you want)\n");
        printf("          kew song <song name> \n");
        printf("          kew list <m3u list name> \n");
        printf("          kew shuffle <dir name> (random and rand works too)\n");
        printf("          kew artistA:artistB (plays artistA and artistB shuffled)\n");
        printf("          kew . (plays kew.m3u file)\n");
        printf("\n");
        printf(" \033[1;4mExample:\033[0m kew moon\n");
        printf(" (Plays the first song or directory it finds that has the word moon, ie moonlight sonata)\n");
        printf("\n");
        printf(" kew returns the first directory or file whose name partially matches the string you provide.\n\n");
        printf(" Use quotes when providing strings with single quotes in them (') or vice versa.\n");
        printf(" Use ←, → or h, l to play the next or previous track in the playlist.\n");
        printf(" Use + (or =), - to adjust volume.\n");
        printf(" Use a, d to seek in a song.\n");
        printf(" Press space or p to pause.\n");
        printf(" Press u to update the library.\n");
        printf(" Press F2 to display playlist.\n");
        printf(" Press F3 to display music library.\n");
        printf(" Press F4 to display song info.\n");
        printf(" Press F5 to search.\n");
        printf(" Press F6 to display key bindings.\n");
        printf(" Press . to add the currently playing song to kew.m3u.\n");
        printf(" Press Esc to quit.\n");
        printf("\n");
}

int printLogo(SongData *songData, UISettings *ui)
{
        if (ui->useConfigColors)
                setTextColor(ui->mainColor);
        else
                setColor(ui);

        int height = 0;
        int logoWidth = 0;

        if (!ui->hideLogo)
        {
                printBlankSpaces(indent);
                printf(" __\n");
                printBlankSpaces(indent);
                printf("|  |--.-----.--.--.--.\n");
                printBlankSpaces(indent);
                printf("|    <|  -__|  |  |  |\n");
                printBlankSpaces(indent);
                printf("|__|__|_____|________|");

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

                if (ui->hideLogo && songData->metadata->artist[0] != '\0')
                {
                        printBlankSpaces(indent);
                        snprintf(title, MAXPATHLEN, "%s - %s",
                                 songData->metadata->artist, songData->metadata->title);
                }
                else
                {
                        if (ui->hideLogo)
                                printBlankSpaces(indent);
                        c_strcpy(title, songData->metadata->title, METADATA_MAX_SIZE - 1);
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

void printCover(SongData *songdata, UISettings *ui)
{
        clearScreen();

        if (songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        printSquareBitmapCentered(songdata->cover, songdata->coverWidth, songdata->coverHeight, preferredHeight);
                }
                else
                {
                        printInAscii(songdata->coverArtPath, preferredHeight);
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

void printTitleWithDelay(const char *text, int delay, int maxWidth)
{
        int length = strnlen(text, maxWidth);
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

                if (delay)
                        c_sleep(delay);
        }
        if (delay)
                c_sleep(delay * 20);

        printf("\r");
        printf("\033[K");
        printBlankSpaces(indent);
        printf("\033[1K %.*s", maxWidth, text);
        printf("\n");
        fflush(stdout);
}

void printBasicMetadata(TagSettings const *metadata, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int maxWidth = textWidth; // term_w - 3 - (indent * 2);
        printf("\n");

        if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0)
        {
                printBlankSpaces(indent);
                printf(" %.*s\n", maxWidth, metadata->artist);
        }
        else
        {
                printf("\n");
        }
        if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0)
        {
                printBlankSpaces(indent);
                printf(" %.*s\n", maxWidth, metadata->album);
        }
        else
        {
                printf("\n");
        }
        if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0)
        {
                printBlankSpaces(indent);
                int year = getYear(metadata->date);
                if (year == -1)
                        printf(" %s\n", metadata->date);
                else
                        printf(" %d\n", year);
        }
        else
        {
                printf("\n");
        }
        cursorJump(4);
        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
        {
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

                printTitleWithDelay(metadata->title, ui->titleDelay, maxWidth - 2);
        }
        else
        {
                printf("\n");
        }
        cursorJumpDown(3);
}

int calcElapsedBars(double elapsedSeconds, double duration, int numProgressBars)
{
        if (elapsedSeconds == 0)
                return 0;

        return (int)((elapsedSeconds / duration) * numProgressBars);
}

void printProgress(double elapsed_seconds, double total_seconds)
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
        int vol = getCurrentVolume();

        // Clear the current line
        printf("\r\033[K");

        printBlankSpaces(indent);

        printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
               elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
               total_hours, total_minutes, total_seconds_remainder,
               progress_percentage, vol);

        // Restore the cursor position
        printf("\033[u");
}

void printMetadata(TagSettings const *metadata, UISettings *ui)
{
        if (appState.currentView == LIBRARY_VIEW || appState.currentView == PLAYLIST_VIEW || appState.currentView == SEARCH_VIEW)
                return;

        if (ui->titleDelay)
                c_sleep(100);

        if (ui->useConfigColors)
                setDefaultTextColor();
        else
                setTextColorRGB(ui->color.r, ui->color.g, ui->color.b);
        printBasicMetadata(metadata, ui);
}

void printTime(double elapsedSeconds, AppState *state)
{
        if (appState.currentView == LIBRARY_VIEW || appState.currentView == PLAYLIST_VIEW || appState.currentView == SEARCH_VIEW)
                return;
        if (state->uiSettings.useConfigColors)
                setDefaultTextColor();
        else
                setTextColorRGB(state->uiSettings.color.r, state->uiSettings.color.g, state->uiSettings.color.b);
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        printBlankSpaces(indent);
        if (term_h > minHeight)
        {
                double duration = getCurrentSongDuration();
                printProgress(elapsedSeconds, duration);
        }
}

int calcIndentNormal(void)
{
        int textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
        return getIndentation(textWidth - 1) - 1;
}

int calcIndentSongView(SongData *songdata)
{
        if (songdata == NULL)
                return calcIndentNormal();

        int titleLength = strnlen(songdata->metadata->title, METADATA_MAX_LENGTH);
        int albumLength = strnlen(songdata->metadata->album, METADATA_MAX_LENGTH);
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
                indent = calcIndentSongView(songdata);
        }
}

void printGlimmeringText(char *text, int textLength, char *nerdFontText, PixelData color)
{
        int brightIndex = 0;
        PixelData vbright = increaseLuminosity(color, 120);
        PixelData bright = increaseLuminosity(color, 60);

        printBlankSpaces(calcIndentNormal());

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

void printLastRow(void)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        if (term_w < ABSOLUTE_MIN_WIDTH)
                return;

#ifndef __APPLE__
        // Move to lastRow
        printf("\033[%d;1H", term_h);
#endif

        setTextColorRGB(lastRowColor.r, lastRowColor.g, lastRowColor.b);

#ifdef __APPLE__
        char text[100] = " Sh+Z List|Sh+X Lib|Sh+C Track|Sh+V Search|Sh+B Help|Esc Quit";
#else
        char text[100] = " [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help|Esc Quit]";
#endif

        char nerdFontText[100] = "";

        printf("\r");

        size_t maxLength = sizeof(nerdFontText);

        size_t currentLength = strnlen(nerdFontText, maxLength);

        if (isPaused())
        {
                char pauseText[] = " \u23f8";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                currentLength += strnlen(pauseText, maxLength - currentLength);
        }
        else if (isStopped())
        {
                char pauseText[] = " \u23f9";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                currentLength += strnlen(pauseText, maxLength - currentLength);
        }
        else
        {
                char pauseText[] = " \u25b6";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", pauseText);
                currentLength += strnlen(pauseText, maxLength - currentLength);
        }

        if (isRepeatEnabled())
        {
                char repeatText[] = " \u27f3";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", repeatText);
                currentLength += strnlen(repeatText, maxLength - currentLength);
        }

        if (isShuffleEnabled())
        {
                char shuffleText[] = " \uf074";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", shuffleText);
                currentLength += strnlen(shuffleText, maxLength - currentLength);
        }

        if (fastForwarding)
        {
                char forwardText[] = " \uf04e";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", forwardText);
                currentLength += strnlen(forwardText, maxLength - currentLength);
        }

        if (rewinding)
        {
                char rewindText[] = " \uf04a";
                snprintf(nerdFontText + currentLength, maxLength - currentLength, "%s", rewindText);
                currentLength += strnlen(rewindText, maxLength - currentLength);
        }

        printf("\033[K"); // clear the line

        int textLength = strnlen(text, 100);
        int randomNumber = getRandomNumber(1, 808);

        if (randomNumber == 808)
                printGlimmeringText(text, textLength, nerdFontText, lastRowColor);
        else
        {
                printBlankSpaces(calcIndentNormal());
                printf("%s", text);
                printf("%s", nerdFontText);
        }
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
        maxListSize = term_h - 2;

        numPrintedRows += printAbout(songdata, ui);

        setDefaultTextColor();

        printBlankSpaces(indent);
        printf(" - Switch tracks with ←, → or %s, %s keys.\n", settings->previousTrackAlt, settings->nextTrackAlt);
        printBlankSpaces(indent);
        printf(" - Volume is adjusted with %s (or %s) and %s.\n", settings->volumeUp, settings->volumeUpAlt, settings->volumeDown);
        printBlankSpaces(indent);
        printf(" - Press F2 for Playlist View:\n");
        printBlankSpaces(indent);
        printf("     Use ↑, ↓ keys, %s, %s keys, or mouse scroll to scroll through the playlist.\n", settings->scrollUpAlt, settings->scrollDownAlt);
        printBlankSpaces(indent);
        printf("     Press Enter or middle click to play.\n");
        printBlankSpaces(indent);
        printf(" - Press F3 for Library View:\n");
        printBlankSpaces(indent);
        printf("     Use ↑, ↓ keys, %s, %s keys, or mouse scroll to scroll through the library.\n", settings->scrollUpAlt, settings->scrollDownAlt);
        printBlankSpaces(indent);
        printf("     Press Enter or middle click to add/remove songs to/from the playlist.\n");
        printBlankSpaces(indent);
        printf(" - Press F4 for Track View.\n");
        printBlankSpaces(indent);
        printf(" - Space, %s, or right click to toggle pause.\n", settings->togglePause);
        printBlankSpaces(indent);
        printf(" - %s toggle color derived from album or from profile.\n", settings->toggleColorsDerivedFrom);
        printBlankSpaces(indent);
        printf(" - %s to update the library.\n", settings->updateLibrary);
        printBlankSpaces(indent);
        printf(" - %s to show/hide the spectrum visualizer.\n", settings->toggleVisualizer);
        printBlankSpaces(indent);
        printf(" - %s to toggle album covers drawn in ascii.\n", settings->toggleAscii);
        printBlankSpaces(indent);
        printf(" - %s to repeat the current song.\n", settings->toggleRepeat);
        printBlankSpaces(indent);
        printf(" - %s to shuffle the playlist.\n", settings->toggleShuffle);
        printBlankSpaces(indent);
        printf(" - %s to seek backward.\n", settings->seekBackward);
        printBlankSpaces(indent);
        printf(" - %s to seek forward.\n", settings->seekForward);
        printBlankSpaces(indent);
        printf(" - %s to save the playlist to your music folder.\n", settings->savePlaylist);
        printBlankSpaces(indent);
        printf(" - %s to add current song to kew.m3u (run with \"kew .\").\n", settings->addToMainPlaylist);
        printBlankSpaces(indent);
        printf(" - Esc or %s to quit.\n", settings->quit);
        printf("\n");

        numPrintedRows += 22;

        while (numPrintedRows < maxListSize)
        {
                printf("\n");
                numPrintedRows++;
        }

        printLastRow();
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

void tabNext(void)
{
        if (appState.currentView == PLAYLIST_VIEW)
                appState.currentView = LIBRARY_VIEW;
        else if (appState.currentView == LIBRARY_VIEW)
        {
                if (currentSong != NULL)
                {
                        appState.currentView = TRACK_VIEW;
                }
                else
                {
                        appState.currentView = SEARCH_VIEW;
                }
        }
        else if (appState.currentView == TRACK_VIEW)
                appState.currentView = SEARCH_VIEW;
        else if (appState.currentView == SEARCH_VIEW)
                appState.currentView = KEYBINDINGS_VIEW;
        else if (appState.currentView == KEYBINDINGS_VIEW)
                appState.currentView = PLAYLIST_VIEW;

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
                printf(" Use ↑, ↓ or k, j to choose. Enter to accept.\n");
                printBlankSpaces(indentation);
                printf(" Pg Up and Pg Dn to scroll. Del to remove entry.\n\n");
                return aboutRows + 3;
        }
        return aboutRows;
}

void showSearch(SongData *songData, int *chosenRow, UISettings *ui)
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
                printf(" Use ↑, ↓ to choose. Enter to accept.\n\n");
                maxSearchListSize -= 2;
        }

        displaySearch(maxSearchListSize, indent, chosenRow, startSearchIter, ui);

        printf("\n");
        printLastRow();
}

void showPlaylist(SongData *songData, PlayList *list, int *chosenSong, int *chosenNodeId, AppState *state)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxListSize = term_h - 2;

        int aboutRows = printLogoAndAdjustments(songData, term_w, &(state->uiSettings), indent);
        maxListSize -= aboutRows;

        if (state->uiSettings.useConfigColors)
                setTextColor(state->uiSettings.artistColor);
        else
                setColor(&state->uiSettings);

        printBlankSpaces(indent);
        printf("   ─ PLAYLIST ─\n");
        maxListSize -= 1;

        displayPlaylist(list, maxListSize, indent, chosenSong, chosenNodeId, state->uiState.resetPlaylistDisplay, state);

        printf("\n");
        printLastRow();
}

void resetSearchResult(void)
{
        chosenSearchResultRow = 0;
}

void printElapsedBars(int elapsedBars, int numProgressBars)
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

void printVisualizer(double elapsedSeconds, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (ui->visualizerEnabled && appState.currentView == TRACK_VIEW)
        {
                printf("\n");

                int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH : preferredWidth;
                visualizerWidth = (visualizerWidth < textWidth && textWidth < term_w - 2) ? textWidth : visualizerWidth;
                visualizerWidth = (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
                visualizerWidth -= 1;
                uis->numProgressBars = (int)visualizerWidth / 2;
                double duration = getCurrentSongDuration();

#ifndef __APPLE__
                saveCursorPosition();
#endif
                drawSpectrumVisualizer(ui->visualizerHeight, visualizerWidth, ui->color, indent, ui->useConfigColors);
                printElapsedBars(calcElapsedBars(elapsedSeconds, duration, uis->numProgressBars), uis->numProgressBars);
                printLastRow();
#ifndef __APPLE__
                restoreCursorPosition();
                cursorJump(1);
#else
                int jumpAmount = ui->visualizerHeight + 2;
                cursorJump(jumpAmount);
#endif
        }
        else if (!ui->visualizerEnabled)
        {
                if (term_w >= ABSOLUTE_MIN_WIDTH)
                {
                        printf("\n");

                        saveCursorPosition();
                        printLastRow();
                        restoreCursorPosition();
                        cursorJump(1);
                }
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

void processName(const char *name, char *output, int maxWidth)
{
        char *lastDot = strrchr(name, '.');
        int copyLength;

        if (lastDot != NULL)
        {
                copyLength = lastDot - name;
                if (copyLength > maxWidth)
                {
                        copyLength = maxWidth;
                }
        }
        else
        {
                copyLength = maxWidth;
        }

        if (copyLength < 0)
                copyLength = 0;

        c_strcpy(output, name, copyLength + 1);

        output[copyLength] = '\0';
        removeUnneededChars(output, copyLength);
        trim(output, copyLength);
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

        if (root->isDirectory ||
            (!root->isDirectory && depth == 1) ||
            (root->isDirectory && depth == 0) ||
            (chosenDir != NULL && uis->allowChooseSongs && root->parent != NULL && (strcmp(root->parent->fullPath, chosenDir->fullPath) == 0 || strcmp(root->fullPath, chosenDir->fullPath) == 0)))
        {
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
                                                setColor(ui);
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
                                                        setColor(ui);

                                                printf("\x1b[7m * ");
                                        }
                                        else
                                        {
                                                printf("  \x1b[7m ");
                                        }

                                        currentEntry = root;

                                        if (uis->allowChooseSongs == true && (chosenDir == NULL ||
                                                                              (currentEntry != NULL && currentEntry->parent != NULL && chosenDir != NULL && (strcmp(currentEntry->parent->fullPath, chosenDir->fullPath) != 0) &&
                                                                               strcmp(root->fullPath, chosenDir->fullPath) != 0)))
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
                                                        setColorAndWeight(foundCurrent, ui);

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
                                        processName(root->name, filename, maxNameWidth - extraIndent);

                                        printf("└─ ");

                                        if (foundCurrent)
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
        }

        return foundChosen;
}

char *getLibraryFilePath(void)
{
        char *configdir = getConfigPath();
        char *filepath = NULL;

        if (configdir == NULL)
        {
                return NULL;
        }

        size_t configdir_length = strnlen(configdir, MAXPATHLEN);
        size_t library_file_length = strnlen(LIBRARY_FILE, sizeof(LIBRARY_FILE));

        size_t filepath_length = configdir_length + 1 + library_file_length + 1;

        if (filepath_length > MAXPATHLEN)
        {
                free(configdir);
                return NULL;
        }

        filepath = (char *)malloc(filepath_length);
        if (filepath == NULL)
        {
                free(configdir);
                return NULL;
        }

        snprintf(filepath, filepath_length, "%s/%s", configdir, LIBRARY_FILE);

        free(configdir);
        return filepath;
}

void showLibrary(SongData *songData, AppState *state)
{
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

        if (term_w > 60 && !ui->hideHelp)
        {
                maxLibListSize -= 3;
                printBlankSpaces(indent);
                printf(" Use ↑, ↓ or k, j to choose. Enter to enqueue/dequeue.\n");
                printBlankSpaces(indent);
                printf(" Pg Up and Pg Dn to scroll. Press u to update the library.\n\n");
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

        printf("\n");

        printLastRow();

        if (refresh)
        {
                printf("\033[1;1H");
                clearScreen();
                showLibrary(songData, state);
        }
}

int printPlayer(SongData *songdata, double elapsedSeconds, AppSettings *settings, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

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
                }
                else
                {
                        if (state->currentView != LIBRARY_VIEW && state->currentView != PLAYLIST_VIEW && state->currentView != SEARCH_VIEW && state->currentView != KEYBINDINGS_VIEW)
                        {
                                state->currentView = LIBRARY_VIEW;
                        }

                        ui->color.r = defaultColor;
                        ui->color.g = defaultColor;
                        ui->color.b = defaultColor;
                }

                calcPreferredSize(ui);
                calcIndent(songdata);
        }

        if (preferredWidth <= 0 || preferredHeight <= 0)
                return -1;

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
                clearScreen();
                showPlaylist(songdata, originalPlaylist, &chosenRow, &uis->chosenNodeId, state);
                state->uiState.resetPlaylistDisplay = false;
                refresh = false;
                fflush(stdout);
        }
        else if (state->currentView == SEARCH_VIEW && refresh)
        {
                clearScreen();
                showSearch(songdata, &chosenSearchResultRow, ui);
                refresh = false;
                fflush(stdout);
        }
        else if (state->currentView == LIBRARY_VIEW && refresh)
        {
                clearScreen();
                showLibrary(songdata, state);
                refresh = false;
                fflush(stdout);
        }
        else if (state->currentView == TRACK_VIEW && songdata != NULL)
        {
                if (refresh)
                {
                        clearScreen();
                        printf("\n");
                        printCover(songdata, ui);
                        printMetadata(songdata->metadata, ui);
                        refresh = false;
                }
                printTime(elapsedSeconds, state);
                printVisualizer(elapsedSeconds, state);
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
