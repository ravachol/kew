#include "albumart.h"

/*

albumart.c

 Facade for displaying the album cover either using chafa or as ascii art
 
*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

int displayCover(FIBITMAP *cover, const char *coverArtPath, int height, bool ansii)
{
        int width = height * 2;

        if (!ansii)
        {
                printBitmapCentered(cover, width, height);
        }
        else
        {
                output_ascii(coverArtPath, height, width);
        }
        printf("\n");

        return 0;
}
