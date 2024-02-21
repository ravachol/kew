#ifndef ALBUMART_H
#define ALBUMART_H

#include "chafafunc.h"
#include "../include/imgtotxt/write_ascii.h"
#include "../include/imgtotxt/options.h"

int displayCover(FIBITMAP *cover, const char *coverArtPath, int height, bool ansii);

#endif
