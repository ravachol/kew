#ifndef ALBUMART_H
#define ALBUMART_H

#include "../include/imgtotxt/write_ascii.h"
#include "../include/imgtotxt/options.h"
#include "chafafunc.h"

int displayCover(FIBITMAP *cover, const char *coverArtPath, int height, bool ansii);

#endif
