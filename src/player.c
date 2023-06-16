#include "player.h"

const char VERSION[] = "1.0.0";

bool firstSong = true;
bool refresh = true;
bool visualizationEnabled = false;
bool coverEnabled = true;
bool coverBlocks = true;
bool drewCover = true;
bool drewVisualization = false;
int visualizationHeight = 6;
int asciiHeight = 250;
int asciiWidth = 500;
int minWidth = 36;
const char githubLatestVersionUrl[] = "https://api.github.com/repos/ravachol/cue/releases/latest";

int printPlayer(const char *songFilepath, const char *tagsFilePath, double elapsedSeconds, double songDurationSeconds, PlayList *playlist)
{
    int minHeight = 2 + (visualizationEnabled ? visualizationHeight : 0);
    int metadataHeight = 4;
    calcIdealImgSize(&asciiWidth, &asciiHeight, (visualizationEnabled ? visualizationHeight : 0), metadataHeight, firstSong);

    if (refresh)
    {
        int row, col;
        int coverRow = getFirstLineRow() - metadataHeight - (drewCover ? asciiHeight : 0) - (drewVisualization ? visualizationHeight : 0);
        clearRestOfScreen();
        if (coverEnabled)
        {
            displayAlbumArt(songFilepath, asciiHeight, asciiWidth, coverBlocks);
            drewCover = true;
            firstSong = false;
        }
        else
        {
            clearRestOfScreen();
            drewCover = false;
        }
        setDefaultTextColor();
        printBasicMetadata(tagsFilePath);
        refresh = false;
    }
    int term_w, term_h;
    getTermSize(&term_w, &term_h);
    if (term_h > minHeight && term_w > minWidth)
    {
        double totalDurationSeconds = playlist->totalDuration;
        printProgress(elapsedSeconds, songDurationSeconds, totalDurationSeconds);
    }
    if (visualizationEnabled)
    {
        drawEqualizer(visualizationHeight, asciiWidth);
        fflush(stdout);
        drewVisualization = true;
    }
    saveCursorPosition();
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
        // Parse the response data to extract the version information
        // Assuming the response is in JSON format
        // You may need to use a JSON library to parse the response, like Jansson or cJSON

        // Example parsing using Jansson
        // #include <jansson.h>
        // json_error_t error;
        // json_t* json = json_loads(responseData.content, 0, &error);
        // json_t* version = json_object_get(json, "tag_name");
        // const char* versionStr = json_string_value(version);

        // Example parsing using cJSON
        // #include <cjson/cJSON.h>
        // cJSON* json = cJSON_Parse(responseData.content);
        // cJSON* version = cJSON_GetObjectItemCaseSensitive(json, "tag_name");
        // const char* versionStr = version->valuestring;

        // After extracting the version string, you can split it into major, minor, and patch components
        // Here's an example of splitting a version string "v1.2.3" using sscanf:
        // sscanf(versionStr, "v%d.%d.%d", major, minor, patch);

        // Free the allocated memory
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