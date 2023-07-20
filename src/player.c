#include <string.h>
#include "player.h"

const char VERSION[] = "0.9.18";
const char VERSION_DATE[] = "2023-07-20";

volatile bool refresh = true;
bool visualizerEnabled = true;
bool coverEnabled = true;
bool coverAnsi = false;
bool metaDataEnabled = true;
bool timeEnabled = true;
bool drewCover = true;
bool printInfo = false;
bool showList = true;
int aboutHeight = 7;
int visualizerHeight = 8;
int minWidth = 37;
int minHeight = 2;
int coverRow = 0;
int preferredWidth = 0;
int preferredHeight = 0;
int elapsed = 0;
int duration = 0;
char *tagsPath;
double totalDurationSeconds = 0.0;
PixelData color = {0, 0, 0};
PixelData bgColor = {50, 50, 50};
TagSettings metadata = {}; 

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

void printCover(SongData *songdata)
{
    clearRestOfScreen();
    if (songdata->cover != NULL && coverEnabled)
    {
        color.r = *(songdata->red);
        color.g = *(songdata->green);
        color.b = *(songdata->blue);

        displayCover(songdata, preferredWidth, preferredHeight, coverAnsi);

        drewCover = true;
        if (color.r == 0 && color.g == 0 && color.b == 0)
        {
            color.r = 210;
            color.g = 210;
            color.b = 210;
        }
    }
    else
    {
        color.r = 210;
        color.g = 210;
        color.b = 210;
        clearRestOfScreen();
        drewCover = false;
    }
}

void setColor()
{
    if (color.r == 0 && color.g == 0 && color.b == 0)
        setDefaultTextColor();
    else if (color.r == 255 && color.g == 255 && color.b == 255)
    {
        color.r = 210;
        color.g = 210;
        color.b = 210;
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
        for (int j = 0; j < i; j++)
        {
            printf("%c", text[j]);
        }
        printf("█");
        fflush(stdout);
        usleep(delay * 1000); // Delay in milliseconds
    }
    usleep(delay * 50000);
    printf("\033[1K\r %.*s", maxWidth, text);
    printf("\n");
    fflush(stdout);
}

void printBasicMetadata(TagSettings *metadata)
{
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    int maxWidth = term_w - 3;
    printf("\n");
    setColor();
    int rows = 1;
    if (strlen(metadata->artist) > 0)
    {
        printf(" %.*s\n", maxWidth, metadata->artist);
        rows++;
    }
    if (strlen(metadata->album) > 0)
    {
        printf(" %.*s\n", maxWidth, metadata->album);
        rows++;
    }
    if (strlen(metadata->date) > 0)
    {
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
        PixelData pixel = increaseLuminosity(color, 100);
        if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
        {
            PixelData gray;
            gray.r = 210;
            gray.g = 210;
            gray.b = 210;
            setTextColorRGB2(gray.r, gray.g, gray.b);
        }
        else
        {
            setTextColorRGB2(pixel.r, pixel.g, pixel.b);
        }

        printWithDelay(metadata->title, 9, maxWidth);
    }
    fflush(stdout);
    cursorJumpDown(rows - 1);
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
    printf("\r");
    printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) T:%dh%02dm",
           elapsed_hours, elapsed_minutes, elapsed_seconds_remainder,
           total_hours, total_minutes, total_seconds_remainder,
           progress_percentage, total_playlist_hours, total_playlist_minutes);
    fflush(stdout);

    // Restore the cursor position
    printf("\033[u");
    fflush(stdout);
}

void printMetadata(TagSettings *metadata)
{
    if (!metaDataEnabled || printInfo)
        return;
    usleep(100000);
    setColor();
    printBasicMetadata(metadata);
}

void printTime()
{
    if (!timeEnabled || printInfo)
        return;
    setColor();
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_h > minHeight && term_w > minWidth)
        printProgress(elapsed, duration, totalDurationSeconds);
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

    while (brightIndex < textLength)
    {
        for (int i = 0; i < textLength; i++)
        {
            if (i == brightIndex)
            {
                setTextColorRGB2(vbright.r, vbright.g, vbright.b);
                printf("%c", text[i]);
            }
            else if (i == brightIndex - 1 || i == brightIndex + 1)
            {
                setTextColorRGB2(bright.r, bright.g, bright.b);
                printf("%c", text[i]);
            }
            else
            {
                setTextColorRGB2(color.r, color.g, color.b);
                printf("%c", text[i]);
            }

            fflush(stdout);
            usleep(30);
        }
        brightIndex++;
        printf("\r");
    }
}

void printLastRow()
{
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_w < minWidth)
        return;
    setTextColorRGB2(bgColor.r, bgColor.g, bgColor.b);

    char text[100] = " [F1 Playlist] [Q Quit] cue v%s";
    // Replace "%s" in the text with the actual version
    char *versionPtr = strstr(text, "%s");
    if (versionPtr != NULL) {
        strcpy(versionPtr, VERSION);
    }

    int randomNumber = getRandomNumber(1, 808);
    if (randomNumber == 808)
        printGlimmeringText(text, bgColor);
    else
        printf("%s", text);
}

void showVersion()
{
    printVersion(VERSION);
}

void printAbout()
{
    clearRestOfScreen();
    printf("\n\n\r");
    printAsciiLogo();
    printf("\n");
    showVersion();
    printf("\n");    
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

int showPlaylist()
{
    Node *node = playlist.head;
    Node *foundNode = playlist.head;
    bool foundCurrentSong = false;
    bool startFromCurrent = false;
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    int otherRows = 4;
    int totalHeight = term_h;
    int maxListSize = totalHeight - aboutHeight - otherRows;
    int numRows = 0;
    int numPrintedRows = 0;
    int foundAt = 0;
    if (node == NULL)
        return numRows;
    printf("\n");
    numRows++;
    numPrintedRows++;
    usleep(700000);
    PixelData textColor = increaseLuminosity(color, 100);
    setTextColorRGB2(textColor.r, textColor.g, textColor.b);
    printAbout();

    setTextColorRGB2(color.r, color.g, color.b);

    int numSongs = 0;
    for (int i = 0; i < playlist.count; i++)
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
        node = playlist.head;

    for (int i = (startFromCurrent ? foundAt : 0); i < (startFromCurrent ? foundAt : 0) + maxListSize; i++)
    {
        if (node == NULL)
            break;
        char filePath[MAXPATHLEN];
        strcpy(filePath, node->song.filePath);
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');
        printf("\r");
        setTextColorRGB2(color.r, color.g, color.b);
        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash)
        {
            char copiedString[256];
            strncpy(copiedString, lastSlash + 1, lastDot - lastSlash - 1);
            copiedString[lastDot - lastSlash - 1] = '\0';
            removeUnneededChars(copiedString);
            if (strcmp(filePath, currentSong->song.filePath) == 0)
            {
                setTextColorRGB2(textColor.r, textColor.g, textColor.b);
                foundCurrentSong = true;
            }
            shortenString(copiedString, term_w - 5);
            trim(copiedString);
            if (!startFromCurrent || foundCurrentSong)
            {
                if (i + 1 < 10)
                    printf(" ");
                if (startFromCurrent)
                    printf(" %d. %s\n", i + 1, copiedString);
                else
                    printf(" %d. %s\n", numRows, copiedString);
                numPrintedRows++;
                if (numPrintedRows > maxListSize)
                    break;
            }
            numRows++;

            setTextColorRGB2(color.r, color.g, color.b);
        }
        node = node->next;
    }
    printf("\n");
    printLastRow();
    numPrintedRows++;
    if (numRows > 1)
    {
        while (numPrintedRows <= maxListSize + 1)
        {
            printf("\n");
            numPrintedRows++;
        }
    }

    return numPrintedRows;
}

void printEqualizer()
{
    if (visualizerEnabled && !printInfo)
    {
        printf("\n");
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        drawSpectrumVisualizer(visualizerHeight, term_w, color);
        printLastRow();
        int jumpAmount = visualizerHeight + 2;
        cursorJump(jumpAmount);
        saveCursorPosition();
    }
}

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist)
{
    metadata = *songdata->metadata;
    hideCursor();
    totalDurationSeconds = playlist->totalDuration;
    elapsed = elapsedSeconds;
    duration = *songdata->duration;

    calcPreferredSize();

    if (preferredWidth <= 0 || preferredHeight <= 0)
        return -1;

    if (printInfo)
    {
        if (refresh)
        {
            int height = showPlaylist();
            int jumpAmount = height;
            cursorJump(jumpAmount);
            saveCursorPosition();
        }
    }
    else
    {
        if (refresh)
        {
            printf("\n");
            printCover(songdata);
            printMetadata(songdata->metadata);
        }
        printTime();
        printEqualizer();
        if (!visualizerEnabled)
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
    refresh = false;
    fflush(stdout);
    return 0;
}

void showHelp()
{
    printHelp();
}