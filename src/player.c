#include "player.h"

const char VERSION[] = "1.0.0";

bool firstSong = true;
bool refresh = true;
bool equalizerEnabled = true;
bool equalizerBlocks = true;
bool coverEnabled = true;
bool coverBlocks = true;
bool drewCover = true;
bool drewVisualization = false;
int equalizerHeight = 8;
int metadataHeight = 4;
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

const char githubLatestVersionUrl[] = "https://api.github.com/repos/ravachol/cue/releases/latest";

void calcPreferredSize()
{
    minHeight = 2 + (equalizerEnabled ? equalizerHeight : 0);    
    calcIdealImgSize(&preferredWidth, &preferredHeight, (equalizerEnabled ? equalizerHeight : 0), metadataHeight, firstSong);
}

void printCover()
{
    clearRestOfScreen();
    if (coverEnabled)
    {
        color.r = 0;
        color.g = 0;
        color.b = 0;        
        displayAlbumArt(path, preferredHeight, preferredWidth, coverBlocks, &color);
        drewCover = true;
        firstSong = false;
    }
    else
    {
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

void printMetadata()
{
    setColor();  
    TagSettings settings = printBasicMetadata(tagsPath);
    if (settings.title) setWindowTitle(settings.title);
}

void printTime()
{
    setColor();    
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_h > minHeight && term_w > minWidth)
        printProgress(elapsed, duration, totalDurationSeconds);
}

void printEqualizer()
{
    if (equalizerEnabled)
    {
        drawEqualizer(equalizerHeight, preferredWidth, equalizerBlocks, color);
        drewVisualization = true;
    }  
}

int printPlayer(const char *songFilepath, const char *tagsFilePath, double elapsedSeconds, double songDurationSeconds, PlayList *playlist)
{    
    path = strdup(songFilepath);
    tagsPath = strdup(tagsFilePath);
    totalDurationSeconds = playlist->totalDuration;
    elapsed = elapsedSeconds;
    duration = songDurationSeconds;

    calcPreferredSize();

    if (preferredWidth <= 0 || preferredHeight <= 0) return -1;

    if (refresh)
    {
        printCover();        
        printMetadata();
        refresh = false;
    }  
    printTime();
    printEqualizer();
    saveCursorPosition();

    return 0; 
}

// Struct to hold response data
struct ResponseData
{
    char *content;
    size_t size;
};

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

void showVersion()
{
    int major, minor, patch;
    char latestVersion[10];
    if (fetchLatestVersion(&major, &minor, &patch) >= 0)
    {
        sprintf(latestVersion, "%d.%d.%d", major, minor, patch);
    }
    else
    {
        sprintf(latestVersion, VERSION);
    }
    printVersion(VERSION, latestVersion);
}

void showHelp()
{
    printHelp();
}