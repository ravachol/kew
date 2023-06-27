#include <string.h>
#include "player.h"

const char VERSION[] = "0.9.4";
const char VERSION_DATE[] = "2023-06-28";

bool firstSong = true;
bool refresh = true;
bool equalizerEnabled = true;
bool equalizerBlocks = true;
bool coverEnabled = true;
bool coverBlocks = true;
bool metaDataEnabled = true;
bool timeEnabled = true;
bool drewCover = true;
bool printInfo = false;
bool showList = true;
int versionHeight = 11;
int equalizerHeight = 8;
int minWidth = 31;
int minHeight = 2;
int coverRow = 0;
int preferredWidth = 0;
int preferredHeight = 0;
int elapsed = 0;
int duration = 0;
char *path;
char *tagsPath;
double totalDurationSeconds = 0.0;
PixelData color = { 0, 0, 0 };
PixelData bgColor = { 50, 50, 50 };
TagSettings metadata = {};
char latestVersion[10] = "\0";
struct ResponseData
{
    char *content;
    size_t size;
};

const char githubLatestVersionUrl[] = "https://api.github.com/repos/ravachol/cue/releases/latest";

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
    minHeight = 2 + (equalizerEnabled ? equalizerHeight : 0);    
    calcIdealImgSize(&preferredWidth, &preferredHeight, (equalizerEnabled ? equalizerHeight : 0), calcMetadataHeight(), firstSong);
}

void printCover(SongData *songdata)
{
    clearRestOfScreen();
    if (songdata->cover != NULL && coverEnabled)
    {
        color.r = *songdata->red;
        color.g = *songdata->green;
        color.b = *songdata->blue;
        displayCover(songdata, preferredWidth, preferredHeight, !coverBlocks);
 
        drewCover = true;
        firstSong = false;
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
    else
        setTextColorRGB2(color.r, color.g, color.b);
}

void printBasicMetadata(TagSettings *metadata)
{
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
        printf(" %s\n", metadata->title); 
        
    }
    setColor();    
    if (strlen(metadata->artist) > 0)
        printf(" %s\n", metadata->artist);
    if (strlen(metadata->album) > 0)
        printf(" %s\n", metadata->album);
    if (strlen(metadata->date) > 0)
    {
        int year = getYear(metadata->date);
        if (year == -1)
            printf(" %s\n", metadata->date);
        else
            printf(" %d\n", year);
    }
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
    if (!metaDataEnabled || printInfo) return;
    usleep(100000);
    setColor();  
    printBasicMetadata(metadata);
}

void printTime()
{
    if (!timeEnabled || printInfo) return;
    setColor();    
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_h > minHeight && term_w > minWidth)
        printProgress(elapsed, duration, totalDurationSeconds);
}

void printLastRow()
{
    int minWidth = 30;
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_w < minWidth)
        return;
    setTextColorRGB2(bgColor.r, bgColor.g, bgColor.b);
    printf(" [F1 Info] [Q Quit] cue v%s", VERSION);
}

// Callback function to write response data
size_t WriteCallback(char *content, size_t size, size_t nmemb, void *userdata)
{
    size_t totalSize = size * nmemb;
    struct ResponseData *responseData = (struct ResponseData *)userdata;
    responseData->content = realloc(responseData->content, responseData->size + totalSize + 1);
    if (responseData->content == NULL)
    {
        printf("Failed to allocate memory for response data.\n");
        return 0;
    }
    memcpy(&(responseData->content[responseData->size]), content, totalSize);
    responseData->size += totalSize;
    responseData->content[responseData->size] = '\0';
    return totalSize;
}

// Function to get the latest version from GitHub
int fetchLatestVersion(int *major, int *minor, int *patch)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        printf("Failed to initialize cURL.\n");
        return -1;
    }

    // Set the request URL
    curl_easy_setopt(curl, CURLOPT_URL, githubLatestVersionUrl);

    // Set the write callback function
    struct ResponseData responseData;
    responseData.content = malloc(1); // Initial memory allocation
    responseData.size = 0;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        printf("Failed to perform cURL request: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(responseData.content);
        return -1;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Parse the version from the response
    char* versionStart = strstr(responseData.content, "tag_name");
    if (versionStart != NULL)
    {
        versionStart += strlen("tag_name\": \"");
        char* versionEnd = strchr(versionStart, '\"');
        if (versionEnd != NULL)
        {
            *versionEnd = '\0';  // Null-terminate the version string
            char* version = versionStart;
            // Assuming the version format is "vX.Y.Z", skip the 'v' character
            sscanf(version + 1, "%d.%d.%d", major, minor, patch);
        }
    }    

    curl_easy_cleanup(curl);

    if (response_code == 200)
    {
        free(responseData.content);

        return 0;
    }
    else
    {
        free(responseData.content);
        return -1;
    }
}

void showVersion(PixelData color, PixelData secondaryColor)
{
    int major, minor, patch;

    sprintf(latestVersion, VERSION);
    printf("\n");
    printVersion(VERSION, VERSION_DATE, color, secondaryColor);
}

void removeUnneededChars(char* str) {
    int i;
    for (i = 0; i < 3 && str[i] != '\0'; i++) {
        if (isdigit(str[i]) || str[i] == '-' || str[i] == ' ') {
            int j;
            for (j = i; str[j] != '\0'; j++) {
                str[j] = str[j + 1];
            }
            str[j] = '\0';
            i--;  // Decrement i to re-check the current index
        }
    }
}

void shortenString(char* str, int width) {
    if (strlen(str) > width) {
        str[width] = '\0';
    }
}

int showPlaylist(int maxHeight)
{
    Node *node = playlist.head;
    bool foundCurrentSong = false;
    bool startFromCurrent = false;
    int term_w, term_h;
    getTermSize(&term_w, &term_h);       
    int otherRows = 4;    
    int totalHeight = term_h;
    int maxListSize = totalHeight - versionHeight - otherRows;
    int numRows = 0;
    int numPrintedRows = 0;
    int foundAt = 0;
    if (node == NULL) return numRows;
    printf("\n");
    numRows++; 
    numPrintedRows++;

    PixelData textColor = increaseLuminosity(color, 100);
    setTextColorRGB2(textColor.r, textColor.g, textColor.b);      
    setTextColorRGB2(color.r, color.g, color.b);

    int numSongs = 0;
    for (int i = 0; i < playlist.count; i++)
    {
        if (!strcmp(node->song.filePath, currentSong->song.filePath) == 0)
        {
            foundAt = numSongs;
        }
        node = node->next;
        numSongs++;
        if (numSongs > maxListSize)
        {
            startFromCurrent = true;
            break;
        }
    }
    node = playlist.head;

    for (int i = 0; i < foundAt + maxListSize; i++)
    {
        if (node == NULL) break;
        char filePath[MAXPATHLEN];
        strcpy(filePath, node->song.filePath);        
        char *lastSlash = strrchr(filePath, '/');
        char *lastDot = strrchr(filePath, '.');
        printf("\r");
        setTextColorRGB2(color.r, color.g, color.b);
        if (lastSlash != NULL && lastDot != NULL && lastDot > lastSlash) {
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
            if (copiedString != NULL && (!startFromCurrent || foundCurrentSong))
            {
                if (numRows < 10)
                    printf(" ");
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

void printAbout()
{
    printf("\n\r\n");
    clearRestOfScreen();
    if (refresh && printInfo)
    {
        PixelData textColor = increaseLuminosity(color, 100);
        setTextColorRGB2(textColor.r, textColor.g, textColor.b);      
        printAsciiLogo();
        setTextColorRGB2(color.r, color.g, color.b);
        showVersion(textColor,color);        
        int emptyRows = (equalizerEnabled ? equalizerHeight : 0) + 
                        (metaDataEnabled ? calcMetadataHeight() : 0) + 
                        (timeEnabled ? 1 : 0) - 
                        versionHeight;
        fflush(stdout);
    }
}

void printEqualizer()
{   
    if (equalizerEnabled && !printInfo)
    {
        printf("\n");        
        int term_w, term_h;
        getTermSize(&term_w, &term_h);
        drawEqualizer(equalizerHeight, term_w, equalizerBlocks, color);            
        printLastRow();       
        int jumpAmount = equalizerHeight + 2;
        cursorJump(jumpAmount);
        saveCursorPosition();                   
    }  
}

int printPlayer(SongData *songdata, double elapsedSeconds, PlayList *playlist)
{    
    metadata = *songdata->metadata;
    hideCursor();    
    path = strdup(songdata->filePath);
    totalDurationSeconds = playlist->totalDuration;
    elapsed = elapsedSeconds;
    duration = *songdata->duration;

    calcPreferredSize();

    if (preferredWidth <= 0 || preferredHeight <= 0) return -1;

    if (printInfo)
    {
        if (refresh)
        {
            printAbout();  
            int height = showPlaylist(equalizerHeight + 4);
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
    }
    refresh = false;

    return 0; 
}

void showHelp()
{
    printHelp();
}