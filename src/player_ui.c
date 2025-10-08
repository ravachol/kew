#include "player_ui.h"
#include "appstate.h"
#include "common_ui.h"
#include "directorytree.h"
#include "imgfunc.h"
#include <math.h>
#include "playerops.h"
#include "playlist.h"
#include "playlist_ui.h"
#include "search_ui.h"
#include "songloader.h"
#include "term.h"
#include "utils.h"
#include "visuals.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// clang-format off
static const char *LOGO[] = {" __\n",
                             "|  |--.-----.--.--.--.\n",
                             "|    <|  -__|  |  |  |\n",
                             "|__|__|_____|________|"};
// clang-format on

static int footerCol = 0;
static int footerRow = 0;
static const char LIBRARY_FILE[] = "kewlibrary";
static const int MAX_TERM_SIZE = 10000;
static const int scrollingInterval = 1;
static const int LOGO_WIDTH = 22;

static int minHeight = 0;
static int preferredWidth = 0;
static int preferredHeight = 0;
static int textWidth = 0;
static int indent = 0;
static int maxListSize = 0;
static int maxSearchListSize = 0;
static int numTopLevelSongs = 0;
static int startLibIter = 0;
static int startSearchIter = 0;
static int maxLibListSize = 0;
static int chosenRow = 0;             // The row that is chosen in playlist view
static int chosenLibRow = 0;          // The row that is chosen in library view
static int chosenSearchResultRow = 0; // The row that is chosen in search view
static int libIter = 0;
static int libSongIter = 0;
static int previousChosenLibRow = 0;
static int libCurrentDirSongCount = 0;
static PixelData footerColor = {120, 120, 120};
static FileSystemEntry *currentEntry = NULL;
static FileSystemEntry *lastEntry = NULL;
static FileSystemEntry *chosenDir = NULL;
static bool isSameNameAsLastTime = false;

int getFooterRow(void) { return footerRow; }

int getFooterCol(void) { return footerCol; }

int calcIdealImgSize(int *width, int *height, const int visualizerHeight,
                     const int metatagHeight)
{
        if (!width || !height)
                return -1;

        float aspectRatio = calcAspectRatio();

        if (!isfinite(aspectRatio) || aspectRatio <= 0.0f ||
            aspectRatio > 100.0f)
                aspectRatio = 1.0f; // fallback to square

        int term_w = 0, term_h = 0;
        getTermSize(&term_w, &term_h);

        if (term_w <= 0 || term_h <= 0 || term_w > MAX_TERM_SIZE ||
            term_h > MAX_TERM_SIZE)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        const int timeDisplayHeight = 1;
        const int heightMargin = 4;
        const int minHeight = visualizerHeight + metatagHeight +
                              timeDisplayHeight + heightMargin + 1;

        if (minHeight < 0 || minHeight > term_h)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        int availableHeight = term_h - minHeight;
        if (availableHeight <= 0)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        // Safe calculation using double
        double safeHeight = (double)availableHeight;
        double safeAspect = (double)aspectRatio;

        double tempWidth = safeHeight * safeAspect;

        // Clamp to INT_MAX and reasonable limits
        if (tempWidth < 1.0)
                tempWidth = 1.0;
        else if (tempWidth > INT_MAX)
                tempWidth = INT_MAX;

        int calcWidth = (int)ceil(tempWidth);
        int calcHeight = availableHeight;

        if (calcWidth > term_w)
        {
                calcWidth = term_w;
                if (calcWidth <= 0)
                {
                        *width = 1;
                        *height = 1;
                        return -1;
                }

                double tempHeight = (double)calcWidth / safeAspect;

                if (tempHeight < 1.0)
                        tempHeight = 1.0;
                else if (tempHeight > INT_MAX)
                        tempHeight = INT_MAX;

                calcHeight = (int)floor(tempHeight);
        }

        // Final clamping
        if (calcWidth < 1)
                calcWidth = 1;
        if (calcHeight < 2)
                calcHeight = 2;

        // Slight adjustment
        calcHeight -= 1;
        if (calcHeight < 1)
                calcHeight = 1;

        *width = calcWidth;
        *height = calcHeight;

        return 0;
}

void calcPreferredSize(UISettings *ui)
{
        minHeight = 2 + (ui->visualizerEnabled ? ui->visualizerHeight : 0);
        int metadataHeight = 4;
        calcIdealImgSize(&preferredWidth, &preferredHeight,
                         (ui->visualizerEnabled ? ui->visualizerHeight : 0),
                         metadataHeight);
}

void printHelp()
{
        fputs(" kew - A terminal music player.\n"
              "\n"
              " \033[1;4mUsage:\033[0m   kew path \"path to music library\"\n"
              "          (Saves the music library path. Use this the first "
              "time. Ie: kew path \"/home/joe/Music/\")\n"
              "          kew (no argument, opens library)\n"
              "          kew all (loads all your songs up to 10 000)\n"
              "          kew albums (plays all albums up to 2000 randomly one "
              "after the other)\n"
              "          kew <song name,directory or playlist words>\n"
              "          kew --help, -? or -h\n"
              "          kew --version or -v\n"
              "          kew dir <album name> (Sometimes it's necessary to "
              "specify it's a directory you want)\n"
              "          kew song <song name> \n"
              "          kew list <m3u list name> \n"
              "          kew theme <theme name> (sets a theme)\n"
              "          kew . (plays kew favorites.m3u file)\n"
              "          kew shuffle <dir name> (random and rand works too)\n"
              "          kew artistA:artistB (plays artistA and artistB "
              "shuffled)\n"
              "\n"
              " \033[1;4mExample:\033[0m kew moon\n"
              " (Plays the first song or directory it finds that has the word "
              "moon, ie moonlight sonata)\n"
              "\n"
              " kew returns the first directory or file whose name partially "
              "matches the string you provide.\n"
              "\n"
              " Use quotes when providing strings with single quotes in them "
              "(') or vice versa.\n"
              " Enter to select or replay a song.\n"
              " Switch tracks with ←, → or h, l keys.\n"
              " Volume is adjusted with + (or =) and -.\n"
              " Space, p or right mouse to play or pause.\n"
              " Shift+s to stop.\n"
              " F2 to show/hide playlist view.\n"
              " F3 to show/hide library view.\n"
              " F4 to show/hide track view.\n"
              " F5 to show/hide search view.\n"
              " F6 to show/hide key bindings view.\n"
              " You can also use the mouse to switch views.\n"
              " u to update the library.\n"
              " v to toggle the spectrum visualizer.\n"
              " i to cycle between colors from kewrc, theme,"
              " or from the track album cover \n"
              " b to toggle album covers drawn in ascii or as a normal image.\n"
              " r to repeat the current song after playing.\n"
              " s to shuffle the playlist.\n"
              " a to seek back.\n"
              " d to seek forward.\n"
              " x to save the current playlist to a .m3u in your music folder "
              "named after the first song.\n"
              " Tab to switch to next view.\n"
              " Shift+Tab to switch to previous view.\n"
              " Backspace to clear the playlist.\n"
              " Delete to remove a single playlist entry.\n"
              " gg to go to first song.\n"
              " number + G or Enter to go to specific song number in the "
              "playlist.\n"
              " G to go to last song.\n"
              " . to add currently playing song to \"kew favorites.m3u\" (run "
              "with \"kew .\").\n"
              " Esc or q to quit.\n"
              "\n",
              stdout);
}

static const char *getPlayerStatusIcon(void)
{
        if (isPaused())
                return "⏸";
        if (isStopped())
                return "■";
        return "▶";
}

static int printLogoArt(const UISettings *ui, int indent)
{
        if (ui->hideLogo)
        {
                clearLine();
                printf("\n");
                return 1;
        }

        size_t logoHeight = sizeof(LOGO) / sizeof(LOGO[0]);
        for (size_t i = 0; i < logoHeight; i++)
        {
                unsigned char defaultColor = ui->defaultColor;

                PixelData rowColor = {defaultColor, defaultColor, defaultColor};

                if (!(ui->color.r == defaultColor &&
                      ui->color.g == defaultColor &&
                      ui->color.b == defaultColor))
                {
                        rowColor = getGradientColor(ui->color, logoHeight - i,
                                                    logoHeight, 2, 0.8f);
                }

                applyColor(ui->colorMode, ui->theme.logo, rowColor);

                clearLine();
                printBlankSpaces(indent);
                printf("%s", LOGO[i]);
        }
        return 3; // lines used by logo
}

static void buildSongTitle(const SongData *songData, const UISettings *ui,
                           char *out, size_t outSize, int indent)
{
        if (!songData || !songData->metadata)
        {
                out[0] = '\0';
                return;
        }

        const char *icon = getPlayerStatusIcon();

        char prettyTitle[METADATA_MAX_SIZE] = {0};
        snprintf(prettyTitle, METADATA_MAX_SIZE, "%s",
                 songData->metadata->title);
        trim(prettyTitle, strlen(prettyTitle));

        if (ui->hideLogo && songData->metadata->artist[0] != '\0')
        {
                snprintf(out, outSize, "%*s%s %s - %s", indent, "", icon,
                         songData->metadata->artist, prettyTitle);
        }
        else if (ui->hideLogo)
        {
                snprintf(out, outSize, "%*s%s %s", indent, "", icon,
                         prettyTitle);
        }
        else
        {
                strncpy(out, prettyTitle, outSize - 1);
                out[outSize - 1] = '\0';
        }
}

int printLogo(SongData *songData, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        char title[MAXPATHLEN + 1];
        int logoWidth = ui->hideLogo ? 0 : LOGO_WIDTH;
        int maxWidth =
            term_w - indent - indent - (ui->hideLogo ? 2 : logoWidth + 4);

        int height = printLogoArt(ui, indent);

        buildSongTitle(songData, ui, title, sizeof(title), indent);

        applyColor(ui->colorMode, ui->theme.nowplaying, ui->color);

        if (title[0] != '\0')
        {
                char processed[MAXPATHLEN + 1] = {0};
                processName(title, processed, maxWidth, false, false);

                if (ui->hideLogo)
                        printBlankSpaces(indent);
                printf(" %s", processed);
        }

        printf("\n");
        clearLine();
        printf("\n");

        return height + 2;
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
        if (songdata != NULL && songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        printSquareBitmapCentered(
                            songdata->cover, songdata->coverWidth,
                            songdata->coverHeight, preferredHeight);
                }
                else
                {
                        printInAsciiCentered(songdata->coverArtPath,
                                             preferredHeight);
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
                        printSquareBitmap(row, col, songdata->cover,
                                          songdata->coverWidth,
                                          songdata->coverHeight, imgHeight);
                }
                else
                {
                        printInAscii(col, songdata->coverArtPath, imgHeight);
                }
        }
}

void printTitleWithDelay(int row, int col, const char *text, int delay,
                         int maxWidth)
{
        int max = strnlen(text, maxWidth);

        if (max == maxWidth) // For long names
                max -= 2;    // Accommodate for the cursor that we display after
                             // the name.

        for (int i = 0; i <= max && delay; i++)
        {
                printf("\033[%d;%dH", row, col);
                clearRestOfLine();

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
        clearRestOfLine();
        printf("%s", text);
        printf("\n");
        fflush(stdout);
}

void printBasicMetadata(int row, int col, int maxWidth,
                        TagSettings const *metadata, UISettings *ui)
{
        if (row < 1)
                row = 1;

        if (col < 1)
                col = 1;

        if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0)
        {
                applyColor(ui->colorMode, ui->theme.trackview_artist,
                           ui->color);
                printf("\033[%d;%dH", row + 1, col);
                clearRestOfLine();
                printf(" %.*s", maxWidth, metadata->artist);
        }

        if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0)
        {
                applyColor(ui->colorMode, ui->theme.trackview_album, ui->color);
                printf("\033[%d;%dH", row + 2, col);
                clearRestOfLine();
                printf(" %.*s", maxWidth, metadata->album);
        }

        if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0)
        {
                applyColor(ui->colorMode, ui->theme.trackview_year, ui->color);
                printf("\033[%d;%dH", row + 3, col);
                clearRestOfLine();
                int year = getYear(metadata->date);
                if (year == -1)
                        printf(" %s", metadata->date);
                else
                        printf(" %d", year);
        }

        PixelData pixel = increaseLuminosity(ui->color, 20);

        if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
        {
                unsigned char defaultColor = ui->defaultColor;

                pixel.r = defaultColor;
                pixel.g = defaultColor;
                pixel.b = defaultColor;
        }

        applyColor(ui->colorMode, ui->theme.trackview_title, pixel);

        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
        {
                // Clean up title before printing
                char prettyTitle[MAXPATHLEN + 1];
                prettyTitle[0] = '\0';

                processName(metadata->title, prettyTitle, maxWidth, false,
                            false);

                printTitleWithDelay(row, col + 1, prettyTitle, ui->titleDelay,
                                    maxWidth);
        }
}

int calcElapsedBars(double elapsedSeconds, double duration, int numProgressBars)
{
        if (elapsedSeconds == 0)
                return 0;

        return (int)((elapsedSeconds / duration) * numProgressBars);
}

void printProgress(double elapsed_seconds, double total_seconds,
                   ma_uint32 sampleRate, int avgBitRate)
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

        int progress_percentage =
            (int)((elapsed_seconds / total_seconds) * 100);
        int vol = getCurrentVolume();

        if (total_seconds >= 3600)
        {
                // Song is more than 1 hour long: use full HH:MM:SS format
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                       elapsed_hours, elapsed_minutes,
                       elapsed_seconds_remainder, total_hours, total_minutes,
                       total_seconds_remainder, progress_percentage, vol);
        }
        else
        {
                // Song is less than 1 hour: use M:SS format
                int elapsed_total_minutes = elapsed_seconds / 60;
                int elapsed_secs = (int)elapsed_seconds % 60;
                int total_total_minutes = total_seconds / 60;
                int total_secs = (int)total_seconds % 60;

                printf(" %d:%02d / %d:%02d (%d%%) Vol:%d%%",
                       elapsed_total_minutes, elapsed_secs, total_total_minutes,
                       total_secs, progress_percentage, vol);
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

void printTime(int row, int col, double elapsedSeconds, ma_uint32 sampleRate,
               int avgBitRate, AppState *state)
{
        applyColor(state->uiSettings.colorMode,
                   state->uiSettings.theme.trackview_time,
                   state->uiSettings.color);

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        printf("\033[%d;%dH", row, col);

        if (term_h > minHeight)
        {
                double duration = getCurrentSongDuration();
                double elapsed = elapsedSeconds;

                printProgress(elapsed, duration, sampleRate, avgBitRate);
                clearRestOfLine();
        }
}

int calcIndentNormal(void)
{
        int textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth)
                            ? ABSOLUTE_MIN_WIDTH
                            : preferredWidth;
        return getIndentation(textWidth - 1) - 1;
}

int calcIndentTrackView(TagSettings *metadata)
{
        if (metadata == NULL)
                return calcIndentNormal();

        int titleLength = strnlen(metadata->title, METADATA_MAX_LENGTH);
        int albumLength = strnlen(metadata->album, METADATA_MAX_LENGTH);
        int maxTextLength =
            (albumLength > titleLength) ? albumLength : titleLength;
        textWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth) ? ABSOLUTE_MIN_WIDTH
                                                          : preferredWidth;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int maxSize = term_w - 2;
        if (maxTextLength > 0 && maxTextLength < maxSize &&
            maxTextLength > textWidth)
                textWidth = maxTextLength;
        if (textWidth > maxSize)
                textWidth = maxSize;

        return getIndentation(textWidth - 1) - 1;
}

void calcIndent(AppState *state, SongData *songdata)
{
        if ((state->currentView == TRACK_VIEW && songdata == NULL) ||
            state->currentView != TRACK_VIEW)
        {
                indent = calcIndentNormal();
        }
        else
        {
                indent = calcIndentTrackView(songdata->metadata);
        }
}

int getIndent() { return indent; }

void printGlimmeringText(int row, int col, char *text, int textLength,
                         char *nerdFontText, PixelData color)
{
        int brightIndex = 0;
        PixelData vbright = increaseLuminosity(color, 120);
        PixelData bright = increaseLuminosity(color, 60);

        printf("\033[%d;%dH", row, col);

        clearRestOfLine();

        while (brightIndex < textLength)
        {
                for (int i = 0; i < textLength; i++)
                {
                        if (i == brightIndex)
                        {
                                setTextColorRGB(vbright.r, vbright.g,
                                                vbright.b);
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

void printErrorRow(int row, int col, UISettings *ui)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        printf("\033[%d;%dH", row, col);

        if (!hasPrintedErrorMessage() && hasErrorMessage())
        {
                applyColor(ui->colorMode, ui->theme.footer, footerColor);
                printf(" %s", getErrorMessage());
                markErrorMessageAsPrinted();
        }

        clearRestOfLine();
        fflush(stdout);
}

void formatWithShiftPlus(char *dest, size_t size, const char *src)
{
        if (isupper((unsigned char)src[0]))
        {
                snprintf(dest, size, "Shft+%s", src);
        }
        else
        {
                snprintf(dest, size, "%s", src);
        }
}

void printFooter(int row, int col, AppState *state, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        if (preferredWidth < 0 || preferredHeight < 0) // mini view
                return;

        footerRow = row;
        footerCol = col;

        printf("\033[%d;%dH", row, col);

        PixelData fColor;
        fColor.r = footerColor.r;
        fColor.g = footerColor.g;
        fColor.b = footerColor.b;

        applyColor(ui->colorMode, ui->theme.footer, fColor);

        if (ui->themeIsSet && ui->theme.footer.type == COLOR_TYPE_RGB)
        {
                fColor.r = ui->theme.footer.rgb.r;
                fColor.g = ui->theme.footer.rgb.g;
                fColor.b = ui->theme.footer.rgb.b;
        }

        char text[100];
#if defined(__ANDROID__) || defined(__APPLE__)
        char playlist[32], library[32], track[32], search[32], help[32];

        // Assume settings->showPlaylistAlt etc. are defined properly
        formatWithShiftPlus(playlist, sizeof(playlist),
                            settings->showPlaylistAlt);
        formatWithShiftPlus(library, sizeof(library), settings->showLibraryAlt);
        formatWithShiftPlus(track, sizeof(track), settings->showTrackAlt);
        formatWithShiftPlus(search, sizeof(search), settings->showSearchAlt);
        formatWithShiftPlus(help, sizeof(help), settings->showKeysAlt);

        snprintf(text, sizeof(text),
                 "%s Playlist|%s Library|%s Track|%s Search|%s Help", playlist,
                 library, track, search, help);

#else
        (void)settings;
        strcpy(text, state->uiSettings.LAST_ROW);
#endif

        char nerdFontText[100] = "";

        size_t maxLength = sizeof(nerdFontText);

        size_t currentLength = strnlen(nerdFontText, maxLength);

        if (term_w >= ABSOLUTE_MIN_WIDTH)
        {
                if (isPaused())
                {
                        char pauseText[] = " ⏸";
                        snprintf(nerdFontText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else if (isStopped())
                {
                        char pauseText[] = " ■";
                        snprintf(nerdFontText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else
                {
                        char pauseText[] = " ▶";
                        snprintf(nerdFontText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
        }

        if (isRepeatEnabled())
        {
                char repeatText[] = " \u27f3";
                snprintf(nerdFontText + currentLength,
                         maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }
        else if (isRepeatListEnabled())
        {
                char repeatText[] = " \u27f3L";
                snprintf(nerdFontText + currentLength,
                         maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }

        if (isShuffleEnabled())
        {
                char shuffleText[] = " \uf074";
                snprintf(nerdFontText + currentLength,
                         maxLength - currentLength, "%s", shuffleText);
                currentLength += strlen(shuffleText);
        }

        if (uis->isFastForwarding)
        {
                char forwardText[] = " \uf04e";
                snprintf(nerdFontText + currentLength,
                         maxLength - currentLength, "%s", forwardText);
                currentLength += strlen(forwardText);
        }

        if (uis->isRewinding)
        {
                char rewindText[] = " \uf04a";
                snprintf(nerdFontText + currentLength,
                         maxLength - currentLength, "%s", rewindText);
                currentLength += strlen(rewindText);
        }

        if (term_w < ABSOLUTE_MIN_WIDTH)
        {
#ifndef __ANDROID__
                if (term_w > (int)currentLength + indent)
                {
                        printf("%s", nerdFontText); // Print just the shuffle
                                                    // and replay settings
                }
#else
                // Always try to print the footer on Android because it will
                // most likely be too narrow. We use two rows for the footer on
                // Android.
                printBlankSpaces(indent);
                printf("%.*s", term_w * 2, text);
#endif
                return;
        }

        int textLength = strnlen(text, 100);
        int randomNumber = getRandomNumber(1, 808);

        if (randomNumber == 808 && !ui->hideGlimmeringText)
                printGlimmeringText(row, col, text, textLength, nerdFontText,
                                    footerColor);
        else
        {
                printf("%s", text);
                printf("%s", nerdFontText);
        }

        clearRestOfLine();
}

void calcAndPrintLastRowAndErrorRow(AppState *state, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);

#if defined(__ANDROID__)
        // Use two rows for the footer on Android. It makes everything
        // fit even with narrow terminal widths.
        if (hasErrorMessage())
                printErrorRow(term_h - 1, indent, &(state->uiSettings));
        else
                printFooter(term_h - 1, indent, &(state->uiSettings), settings);
#else
        printErrorRow(term_h - 1, indent, &(state->uiSettings));
        printFooter(term_h, indent, state, settings);
#endif
}

int printAbout(SongData *songdata, UISettings *ui)
{
        clearLine();
        int numRows = printLogo(songdata, ui);

        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printBlankSpaces(indent);
        printf(" kew version: ");
        applyColor(ui->colorMode, ui->theme.help, ui->color);
        printf("%s\n", ui->VERSION);
        clearLine();
        printf("\n");
        numRows += 2;

        return numRows;
}

int showKeyBindings(SongData *songdata, AppSettings *settings, AppState *state)
{
        int numPrintedRows = 0;
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxListSize = term_h - 4;

        clearScreen();

        UISettings *ui = &(state->uiSettings);

        numPrintedRows += printAbout(songdata, ui);

        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);

        printBlankSpaces(indent);
        printf(" Keybindings:\n\n");

        printBlankSpaces(indent);
        printf(" · Play/Pause: SPACE, %s or right click\n",
               settings->togglePause);

        printBlankSpaces(indent);
        printf(" · Enqueue/Dequeue: Enter\n");

        printBlankSpaces(indent);
        printf(" · Quit: Esc or %s\n", settings->quit);

        printBlankSpaces(indent);
        printf(" · Switch tracks: ← and → or %s and %s\n",
               settings->previousTrackAlt, settings->nextTrackAlt);

        printBlankSpaces(indent);
        printf(" · Volume: %s (or %s) and %s\n", settings->volumeUp,
               settings->volumeUpAlt, settings->volumeDown);

        printBlankSpaces(indent);
        printf(" · Clear List: Backspace\n");

        printBlankSpaces(indent);
        printf(" · Change View: TAB or ");

#if defined(__ANDROID__) || defined(__APPLE__)
        printf("%s, %s, %s, %s, %s", settings->showPlaylistAlt,
               settings->showLibraryAlt, settings->showTrackAlt,
               settings->showSearchAlt, settings->showKeysAlt);
#else
        printf("F2-F6");
#endif
        printf(" or click the footer\n");

        printBlankSpaces(indent);
        printf(
            " · Cycle Color Mode: %s (default theme, theme or cover colors)\n",
            settings->cycleColorsDerivedFrom);

        printBlankSpaces(indent);
        printf(" · Cycle Themes: %s\n", settings->cycleThemes);

        printBlankSpaces(indent);
        printf(" · Stop: Shift+s\n");

        printBlankSpaces(indent);
        printf(" · Update Library: %s\n", settings->updateLibrary);

        printBlankSpaces(indent);
        printf(" · Toggle Visualizer: %s\n", settings->toggleVisualizer);

        printBlankSpaces(indent);
        printf(" · Toggle ASCII Cover: %s\n", settings->toggleAscii);

        printBlankSpaces(indent);
        printf(" · Toggle Notifications: %s\n", settings->toggleNotifications);

        printBlankSpaces(indent);
        printf(" · Cycle Repeat: %s (repeat/repeat list/off)\n",
               settings->toggleRepeat);

        printBlankSpaces(indent);
        printf(" · Shuffle: %s\n", settings->toggleShuffle);

        printBlankSpaces(indent);
        printf(" · Seek: %s and %s\n", settings->seekBackward,
               settings->seekForward);

        printBlankSpaces(indent);
        printf(" · Export Playlist: %s (to Music folder, "
               "named after the first song)\n",
               settings->savePlaylist);

        printBlankSpaces(indent);
        printf(" · Add Song To 'kew favorites.m3u': %s (run with 'kew .')\n\n",
               settings->addToFavoritesPlaylist);

        printBlankSpaces(indent);
        printf(" Manual: See");
        applyColor(ui->colorMode, ui->theme.help, ui->color);
        printf(" README");
        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printf(" Or man kew\n\n");

        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printBlankSpaces(indent);
        printf(" Theme: ");

        if (ui->colorMode == COLOR_MODE_ALBUM)
        {
                applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf("Using ");
                applyColor(ui->colorMode, ui->theme.text, ui->color);
                printf("Colors ");
                applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf("From Track Covers");
        }
        else
        {
                applyColor(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_name);
        }

        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        if (ui->colorMode != COLOR_MODE_ALBUM)
        {
                printf(" Author: ");
                applyColor(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_author);
                numPrintedRows += 1;
        }
        printf("\n");
        numPrintedRows += 1;

        printf("\n");
        printBlankSpaces(indent);
        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(" Project URL:");
        applyColor(ui->colorMode, ui->theme.link, ui->color);
        printf(" https://codeberg.org/ravachol/kew\n");
        printBlankSpaces(indent);
        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(" Please Donate:");
        applyColor(ui->colorMode, ui->theme.link, ui->color);
        printf(" https://ko-fi.com/ravachol\n\n");
        applyColor(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printBlankSpaces(indent);
        printf(" Copyright © 2022-2025 Ravachol\n");

        printf("\n");

        numPrintedRows += 31;

        while (numPrintedRows < maxListSize)
        {
                printf("\n");
                numPrintedRows++;
        }

        calcAndPrintLastRowAndErrorRow(state, settings);

        numPrintedRows++;

        return numPrintedRows;
}

void toggleShowView(ViewState viewToShow, AppState *state)
{
        triggerRefresh();

        if (state->currentView == TRACK_VIEW)
                clearScreen();

        if (state->currentView == viewToShow)
        {
                state->currentView = TRACK_VIEW;
        }
        else
        {
                state->currentView = viewToShow;
        }
}

void switchToNextView(AppState *state)
{
        switch (state->currentView)
        {
        case PLAYLIST_VIEW:
                state->currentView = LIBRARY_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView =
                    (getCurrentSong() != NULL) ? TRACK_VIEW : SEARCH_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = SEARCH_VIEW;
                clearScreen();
                break;
        case SEARCH_VIEW:
                state->currentView = KEYBINDINGS_VIEW;
                break;
        case KEYBINDINGS_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        }

        triggerRefresh();
}

void switchToPreviousView(AppState *state)
{
        switch (state->currentView)
        {
        case PLAYLIST_VIEW:
                state->currentView = KEYBINDINGS_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = LIBRARY_VIEW;
                clearScreen();
                break;
        case SEARCH_VIEW:
                state->currentView =
                    (getCurrentSong() != NULL) ? TRACK_VIEW : LIBRARY_VIEW;
                break;
        case KEYBINDINGS_VIEW:
                state->currentView = SEARCH_VIEW;
                break;
        }

        triggerRefresh();
}

void showTrack(AppState *state)
{
        triggerRefresh();

        state->currentView = TRACK_VIEW;
}

void flipNextPage(AppState *state)
{
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (state->currentView == LIBRARY_VIEW)
        {
                chosenLibRow += maxLibListSize - 1;
                startLibIter += maxLibListSize - 1;
                triggerRefresh();
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                chosenRow += maxListSize - 1;
                chosenRow = (chosenRow >= unshuffledPlaylist->count)
                                ? unshuffledPlaylist->count - 1
                                : chosenRow;
                triggerRefresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow += maxSearchListSize - 1;
                chosenSearchResultRow =
                    (chosenSearchResultRow >= getSearchResultsCount())
                        ? getSearchResultsCount() - 1
                        : chosenSearchResultRow;
                startSearchIter += maxSearchListSize - 1;
                triggerRefresh();
        }
}

void flipPrevPage(AppState *state)
{
        if (state->currentView == LIBRARY_VIEW)
        {
                chosenLibRow -= maxLibListSize;
                startLibIter -= maxLibListSize;
                triggerRefresh();
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                chosenRow -= maxListSize;
                chosenRow = (chosenRow > 0) ? chosenRow : 0;
                triggerRefresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow -= maxSearchListSize;
                chosenSearchResultRow =
                    (chosenSearchResultRow > 0) ? chosenSearchResultRow : 0;
                startSearchIter -= maxSearchListSize;
                triggerRefresh();
        }
}

void scrollNext(AppState *state)
{
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (state->currentView == PLAYLIST_VIEW)
        {
                chosenRow++;
                chosenRow = (chosenRow >= unshuffledPlaylist->count)
                                ? unshuffledPlaylist->count - 1
                                : chosenRow;
                triggerRefresh();
        }
        else if (state->currentView == LIBRARY_VIEW)
        {
                previousChosenLibRow = chosenLibRow;
                chosenLibRow++;
                triggerRefresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow++;
                triggerRefresh();
        }
}

void scrollPrev(AppState *state)
{
        if (state->currentView == PLAYLIST_VIEW)
        {
                chosenRow--;
                chosenRow = (chosenRow > 0) ? chosenRow : 0;
                triggerRefresh();
        }
        else if (state->currentView == LIBRARY_VIEW)
        {
                previousChosenLibRow = chosenLibRow;
                chosenLibRow--;
                triggerRefresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosenSearchResultRow--;
                chosenSearchResultRow =
                    (chosenSearchResultRow > 0) ? chosenSearchResultRow : 0;
                triggerRefresh();
        }
}

int getRowWithinBounds(int row)
{
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (row >= unshuffledPlaylist->count)
        {
                row = unshuffledPlaylist->count - 1;
        }

        if (row < 0)
                row = 0;

        return row;
}

int printLogoAndAdjustments(SongData *songData, int termWidth, UISettings *ui,
                            int indentation, AppSettings *settings)
{
        int aboutRows = printLogo(songData, ui);

        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (termWidth > 52 && !ui->hideHelp)
        {
                printBlankSpaces(indentation);
                printf(" Use ↑/↓ or k/j to select. Enter=Accept. Backspace: "
                       "clear.\n");
                printBlankSpaces(indentation);
#ifndef __APPLE__
                printf(" PgUp/PgDn: scroll. Del: remove. %s/%s: move songs.\n",
                       settings->moveSongUp, settings->moveSongDown);
                clearLine();
                printf("\n");
#else
                printf(" Fn+↑/↓: scroll. Del: remove. %s/%s: move songs.\n",
                       settings->moveSongUp, settings->moveSongDown);
                clearLine();
                printf("\n");
#endif
                return aboutRows + 3;
        }
        return aboutRows;
}

void showSearch(SongData *songData, int *chosenRow, AppState *state,
                AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxSearchListSize = term_h - 3;

        UISettings *ui = &(state->uiSettings);

        gotoFirstLineFirstRow();

        int aboutRows = printLogo(songData, ui);
        maxSearchListSize -= aboutRows;

        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (term_w > indent + 38 && !ui->hideHelp)
        {
                clearLine();
                printBlankSpaces(indent);
                printf(" Use ↑/↓ to select. Enter=Enqueue. Alt+Enter=Play.\n");
                clearLine();
                printf("\n");
                maxSearchListSize -= 2;
        }

        displaySearch(maxSearchListSize, indent, chosenRow, startSearchIter,
                      ui);

        calcAndPrintLastRowAndErrorRow(state, settings);
}

void showPlaylist(SongData *songData, PlayList *list, int *chosenSong,
                  int *chosenNodeId, AppState *state, AppSettings *settings)
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        maxListSize = term_h - 3;

        UISettings *ui = &(state->uiSettings);

        int updateCounter = getUpdateCounter();

        if (getIsLongName() && isSameNameAsLastTime &&
            updateCounter % scrollingInterval != 0)
        {
                incrementUpdateCounter();
                triggerRefresh();
                return;
        }
        else
                cancelRefresh();

        gotoFirstLineFirstRow();

        int aboutRows =
            printLogoAndAdjustments(songData, term_w, ui, indent, settings);
        maxListSize -= aboutRows;

        applyColor(ui->colorMode, ui->theme.header, ui->color);

        if (maxListSize > 0)
        {
                clearLine();
                printBlankSpaces(indent);
                printf("   ─ PLAYLIST ─\n");
        }

        maxListSize -= 1;

        if (maxListSize > 0)
                displayPlaylist(list, maxListSize, indent, chosenSong,
                                chosenNodeId,
                                state->uiState.resetPlaylistDisplay, state);

        calcAndPrintLastRowAndErrorRow(state, settings);
}

void resetSearchResult(void) { chosenSearchResultRow = 0; }

void printProgressBar(int row, int col, AppSettings *settings, UISettings *ui,
                      int elapsedBars, int numProgressBars)
{
        PixelData color = ui->color;

        ProgressBar *progressBar = getProgressBar();

        progressBar->row = row;
        progressBar->col = col + 1;
        progressBar->length = numProgressBars;

        printf("\033[%d;%dH", row, col + 1);

        for (int i = 0; i < numProgressBars; i++)
        {
                if (i > elapsedBars)
                {
                        if (ui->colorMode == COLOR_MODE_ALBUM)
                        {
                                PixelData tmp = increaseLuminosity(color, 50);
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g,
                                       tmp.b);

                                applyColor(ui->colorMode,
                                           ui->theme.progress_empty, tmp);
                        }
                        else
                        {
                                applyColor(ui->colorMode,
                                           ui->theme.progress_empty, color);
                        }

                        if (i % 2 == 0)
                                printf(
                                    "%s",
                                    settings->progressBarApproachingEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarApproachingOddChar);

                        continue;
                }

                if (i < elapsedBars)
                {
                        applyColor(ui->colorMode, ui->theme.progress_filled,
                                   color);

                        if (i % 2 == 0)
                                printf("%s",
                                       settings->progressBarElapsedEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarElapsedOddChar);
                }
                else if (i == elapsedBars)
                {
                        applyColor(ui->colorMode, ui->theme.progress_elapsed,
                                   color);

                        if (i % 2 == 0)
                                printf("%s",
                                       settings->progressBarCurrentEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarCurrentOddChar);
                }
        }
        clearRestOfLine();
}

void printVisualizer(int row, int col, int visualizerWidth,
                     AppSettings *settings, double elapsedSeconds,
                     AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        int height = state->uiSettings.visualizerHeight;

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (row + height + 2 > term_h)
                height -= (row + height + 1 - term_h);

        if (height < 2)
                return;

        if (ui->visualizerEnabled)
        {
                uis->numProgressBars = (int)visualizerWidth / 2;
                double duration = getCurrentSongDuration();

                drawSpectrumVisualizer(row, col, height, state);

                int elapsedBars =
                    calcElapsedBars(elapsedSeconds, duration, visualizerWidth);

                printProgressBar(row + height - 1, col, settings, ui,
                                 elapsedBars, visualizerWidth - 1);
        }
}

FileSystemEntry *getCurrentLibEntry(void) { return currentEntry; }

FileSystemEntry *getChosenDir(void) { return chosenDir; }

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
        if (currentEntry && currentEntry->isDirectory)
                chosenDir = currentEntry;
}

void resetChosenDir(void) { chosenDir = NULL; }

void applyTreeItemColor(UISettings *ui, int depth, PixelData rowColor,
                        bool isEnqueued, bool isPlaying)
{
        if (depth <= 1)
        {
                applyColor(ui->colorMode, ui->theme.library_artist, rowColor);
        }
        else
        {
                applyColor(ui->colorMode, ui->theme.library_track,
                           ui->defaultColorRGB);
        }

        if (isEnqueued)
        {
                if (isPlaying)
                {
                        applyColor(ui->colorMode, ui->theme.library_playing,
                                   rowColor);
                }
                else
                {
                        applyColor(ui->colorMode, ui->theme.library_enqueued,
                                   rowColor);
                }
        }
}

int displayTree(FileSystemEntry *root, int depth, int maxListSize,
                int maxNameWidth, AppState *state)
{
        if (maxNameWidth < 0)
                maxNameWidth = 0;

        char dirName[maxNameWidth + 1];
        char filename[MAXPATHLEN + 1];
        bool foundChosen = false;
        int isPlaying = 0;
        int extraIndent = 0;

        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        Node *current = getCurrentSong();

        if (current != NULL &&
            (strcmp(current->song.filePath, root->fullPath) == 0))
        {
                isPlaying = 1;
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
        rowColor.r = ui->defaultColor;
        rowColor.g = ui->defaultColor;
        rowColor.b = ui->defaultColor;

        if (!(ui->color.r == ui->defaultColor && ui->color.g == ui->defaultColor &&
              ui->color.b == ui->defaultColor))
                rowColor = getGradientColor(ui->color, libIter - startLibIter,
                                            maxListSize, maxListSize / 2, 0.7f);

        if (!(root->isDirectory || (!root->isDirectory && depth == 1) ||
              (root->isDirectory && depth == 0) ||
              (chosenDir != NULL && uis->allowChooseSongs &&
               root->parent != NULL &&
               (strcmp(root->parent->fullPath, chosenDir->fullPath) == 0 ||
                strcmp(root->fullPath, chosenDir->fullPath) == 0))))
        {
                return foundChosen;
        }

        if (depth >= 0)
        {
                if (currentEntry != NULL && currentEntry != lastEntry &&
                    !currentEntry->isDirectory &&
                    currentEntry->parent != NULL &&
                    currentEntry->parent == chosenDir)
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

                        applyTreeItemColor(ui, depth, rowColor,
                                           root->isEnqueued, isPlaying);

                        clearLine();

                        if (depth >= 2)
                                printf("  ");

                        // If more than two levels deep add an extra
                        // indentation
                        extraIndent = (depth - 2 <= 0) ? 0 : depth - 2;

                        printBlankSpaces(indent + extraIndent);

                        if (chosenLibRow == libIter)
                        {
                                if (root->isEnqueued)
                                {
                                        printf("\x1b[7m * ");
                                }
                                else
                                {
                                        printf("  \x1b[7m ");
                                }

                                currentEntry = root;

                                if (uis->allowChooseSongs == true &&
                                    (chosenDir == NULL ||
                                     (currentEntry != NULL &&
                                      currentEntry->parent != NULL &&
                                      chosenDir != NULL &&
                                      !isContainedWithin(currentEntry,
                                                         chosenDir) &&
                                      strcmp(root->fullPath,
                                             chosenDir->fullPath) != 0)))
                                {
                                        uis->collapseView = true;
                                        triggerRefresh();

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
                                        snprintf(dirName,
                                                 maxNameWidth + 1 - extraIndent,
                                                 "%s", "─ MUSIC LIBRARY ─");
                                else
                                        snprintf(dirName,
                                                 maxNameWidth + 1 - extraIndent,
                                                 "%s", root->name);

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

                                isSameNameAsLastTime =
                                    (previousChosenLibRow == chosenLibRow);

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
                                        processNameScroll(root->name, filename,
                                                          maxNameWidth -
                                                              extraIndent,
                                                          isSameNameAsLastTime);
                                }
                                else
                                {
                                        processName(root->name, filename,
                                                    maxNameWidth - extraIndent,
                                                    true, true);
                                }

                                if (isPlaying)
                                {
                                        if (chosenLibRow == libIter)
                                        {
                                                printf("\x1b[7m");
                                        }
                                }

                                printf("└─ ");

                                // Playlist
                                if (pathEndsWith(root->fullPath, "m3u") ||
                                    pathEndsWith(root->fullPath, "m3u8"))
                                {
                                        printf("♫ ");
                                        maxNameWidth = maxNameWidth - 2;
                                }

                                if (isPlaying && chosenLibRow != libIter)
                                {
                                        printf("\e[4m");
                                }

                                printf("%s\n", filename);

                                libSongIter++;
                        }
                }

                libIter++;
        }

        FileSystemEntry *child = root->children;
        while (child != NULL)
        {
                if (displayTree(child, depth + 1, maxListSize, maxNameWidth,
                                state))
                        foundChosen = true;

                child = child->next;
        }

        return foundChosen;
}

char *getLibraryFilePath(void) { return getFilePath(LIBRARY_FILE); }

void showLibrary(SongData *songData, AppState *state, AppSettings *settings)
{
        // For scrolling names, update every nth time
        if (getIsLongName() && isSameNameAsLastTime &&
            getUpdateCounter() % scrollingInterval != 0)
        {
                triggerRefresh();
                return;
        }
        else
                cancelRefresh();

        gotoFirstLineFirstRow();

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
                                chosenLibRow -=
                                    state->uiState.numSongsAboveSubDir;
                                state->uiState.openedSubDir = false;
                                state->uiState.numSongsAboveSubDir = 0;
                                state->uiState.collapseView = false;
                        }
                }
                else
                {
                        if (state->uiState.openedSubDir)
                        {
                                chosenLibRow -=
                                    state->uiState.numSongsAboveSubDir;
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

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int totalHeight = term_h;
        maxLibListSize = totalHeight;
        int aboutSize = printLogo(songData, ui);
        int maxNameWidth = term_w - 10 - indent;
        maxLibListSize -= aboutSize + 2;

        applyColor(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (term_w > 67 && !ui->hideHelp)
        {
                maxLibListSize -= 3;
                clearLine();
                printBlankSpaces(indent);
                printf(" Use ↑/↓ or k/j to select. Enter=Enqueue/Dequeue. "
                       "Alt+Enter=Play.\n");
                clearLine();
                printBlankSpaces(indent);
#ifndef __APPLE__
                printf(" PgUp/PgDn: scroll. u: update, o: sort.\n");
                clearLine();
                printf("\n");
#else
                printf(" Fn+↑/↓: scroll. u: update, o: sort.\n");
                clearLine();
                printf("\n");
#endif
        }

        numTopLevelSongs = 0;

        FileSystemEntry *library = getLibrary();

        FileSystemEntry *tmp = library->children;

        while (tmp != NULL)
        {
                if (!tmp->isDirectory)
                        numTopLevelSongs++;

                tmp = tmp->next;
        }

        bool foundChosen = false;

        if (maxLibListSize <= 0)
                foundChosen = true;
        else
                foundChosen = displayTree(library, 0, maxLibListSize,
                                          maxNameWidth, state);

        if (!foundChosen)
        {
                chosenLibRow--;
                triggerRefresh();
        }

        for (int i = libIter - startLibIter; i < maxLibListSize; i++)
        {
                clearLine();
                printf("\n");
        }

        calcAndPrintLastRowAndErrorRow(state, settings);

        if (!foundChosen && refreshTriggered())
        {
                gotoFirstLineFirstRow();
                showLibrary(songData, state, settings);
        }
}

int calcVisualizerWidth()
{
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferredWidth)
                                  ? ABSOLUTE_MIN_WIDTH
                                  : preferredWidth;
        visualizerWidth =
            (visualizerWidth < textWidth && textWidth < term_w - 2)
                ? textWidth
                : visualizerWidth;
        visualizerWidth =
            (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
        visualizerWidth -= 1;

        return visualizerWidth;
}

void showTrackViewLandscape(int height, int width, float aspectRatio,
                            AppSettings *settings, SongData *songdata,
                            AppState *state, double elapsedSeconds)
{
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;
        int timeHeight = 1;

        if (songdata)
        {
                metadata = songdata->metadata;
        }

        int col = height * aspectRatio + 1;

        if (!state->uiSettings.coverEnabled ||
            (songdata && songdata->cover == NULL))
                col = 1;

        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        int visualizerWidth = term_w - col;

        int row = height - metadataHeight - timeHeight - state->uiSettings.visualizerHeight - 3;

        if (row < 1)
                row = 1;

        if (refreshTriggered())
        {
                printCover(height, songdata, &(state->uiSettings));
                if (height > metadataHeight)
                        printBasicMetadata(row, col, visualizerWidth - 1,
                                           metadata, &(state->uiSettings));

                cancelRefresh();
        }
        if (songdata)
        {
                ma_uint32 sampleRate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                getCurrentFormatAndSampleRate(&format, &sampleRate);
                if (height > metadataHeight + timeHeight)
                        printTime(row + 4, col, elapsedSeconds, sampleRate,
                                  avgBitRate, state);
        }

        if (row > 0)
                printVisualizer(row + metadataHeight + 2, col, visualizerWidth,
                                settings, elapsedSeconds, state);

        if (width - col > ABSOLUTE_MIN_WIDTH)
        {
                printErrorRow(row + metadataHeight + 2 +
                                  state->uiSettings.visualizerHeight,
                              col, &(state->uiSettings));
                printFooter(row + metadataHeight + 2 +
                                state->uiSettings.visualizerHeight + 1,
                            col,state, settings);
        }
}

void showTrackViewPortrait(int height, AppSettings *settings,
                           SongData *songdata, AppState *state,
                           double elapsedSeconds)
{
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;

        int row = height + 3;
        int col = indent;

        int visualizerWidth = calcVisualizerWidth();

        if (refreshTriggered())
        {
                if (songdata)
                {
                        metadata = songdata->metadata;
                }
                clearScreen();
                printCoverCentered(songdata, &(state->uiSettings));
                printBasicMetadata(row, col, visualizerWidth - 1, metadata,
                                   &(state->uiSettings));

                cancelRefresh();
        }
        if (songdata)
        {
                ma_uint32 sampleRate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                getCurrentFormatAndSampleRate(&format, &sampleRate);
                printTime(row + metadataHeight, col, elapsedSeconds, sampleRate,
                          avgBitRate, state);
        }

        printVisualizer(row + metadataHeight + 2, col, visualizerWidth,
                        settings, elapsedSeconds, state);

        calcAndPrintLastRowAndErrorRow(state, settings);
}

void showTrackView(int width, int height, AppSettings *settings,
                   SongData *songdata, AppState *state, double elapsedSeconds)
{
        float aspect = getAspectRatio();

        if (aspect == 0.0f)
                aspect = 1.0f;

        int correctedWidth = width / aspect;

        if (correctedWidth > height * 2)
        {
                showTrackViewLandscape(height, width, aspect, settings,
                                       songdata, state, elapsedSeconds);
        }
        else
        {
                showTrackViewPortrait(preferredHeight, settings, songdata,
                                      state, elapsedSeconds);
        }
}

int printPlayer(SongData *songdata, double elapsedSeconds,
                AppSettings *settings, AppState *state)
{
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (hasPrintedErrorMessage() && refreshTriggered())
                clearErrorMessage();

        if (!ui->uiEnabled)
        {
                return 0;
        }

        if (refreshTriggered())
        {
                hideCursor();

                if (songdata != NULL && songdata->metadata != NULL &&
                    !songdata->hasErrors && (songdata->hasErrors < 1))
                {
                        ui->color.r = songdata->red;
                        ui->color.g = songdata->green;
                        ui->color.b = songdata->blue;

                        if (ui->trackTitleAsWindowTitle)
                                setTerminalWindowTitle(
                                    songdata->metadata->title);
                }
                else
                {
                        if (state->currentView == TRACK_VIEW)
                        {
                                state->currentView = LIBRARY_VIEW;
                                clearScreen();
                        }

                        ui->color.r = ui->defaultColor;
                        ui->color.g = ui->defaultColor;
                        ui->color.b = ui->defaultColor;

                        if (ui->trackTitleAsWindowTitle)
                                setTerminalWindowTitle("kew");
                }

                calcPreferredSize(ui);
                calcIndent(state, songdata);
        }

        int term_w, term_h;
        getTermSize(&term_w, &term_h);

        if (state->currentView != PLAYLIST_VIEW)
                state->uiState.resetPlaylistDisplay = true;

        if (state->currentView == KEYBINDINGS_VIEW && refreshTriggered())
        {
                clearScreen();
                showKeyBindings(songdata, settings, state);
                saveCursorPosition();
                cancelRefresh();
                fflush(stdout);
        }
        else if (state->currentView == PLAYLIST_VIEW && refreshTriggered())
        {
                showPlaylist(songdata, unshuffledPlaylist, &chosenRow,
                             &(uis->chosenNodeId), state, settings);
                state->uiState.resetPlaylistDisplay = false;
                fflush(stdout);
        }
        else if (state->currentView == SEARCH_VIEW && refreshTriggered())
        {
                showSearch(songdata, &chosenSearchResultRow, state, settings);
                cancelRefresh();
                fflush(stdout);
        }
        else if (state->currentView == LIBRARY_VIEW && refreshTriggered())
        {
                showLibrary(songdata, state, settings);
                fflush(stdout);
        }
        else if (state->currentView == TRACK_VIEW)
        {
                showTrackView(term_w, term_h, settings, songdata, state,
                              elapsedSeconds);
                fflush(stdout);
        }

        return 0;
}

void showHelp(void) { printHelp(); }

void freeMainDirectoryTree(AppState *state)
{
        FileSystemEntry *library = getLibrary();

        if (library == NULL)
                return;

        char *filepath = getLibraryFilePath();

        if (state->uiSettings.cacheLibrary)
                freeAndWriteTree(library, filepath);
        else
                freeTree(library);

        free(filepath);
}

int getChosenRow(void) { return chosenRow; }

void setChosenRow(int row) { chosenRow = row; }
