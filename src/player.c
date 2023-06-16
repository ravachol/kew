#include "player.h"

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