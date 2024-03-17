#include <chafa.h>
#include <chafa-canvas-config.h>
#include <FreeImage.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

float calcAspectRatio();
void printImage(const char *image_path, int width, int height);
FIBITMAP *getBitmap(const char *image_path);
void printBitmap(FIBITMAP *bitmap, int width, int height);
void printBitmapCentered(FIBITMAP *bitmap, int width, int height);
void printSquareBitmapCentered(FIBITMAP *bitmap, int baseHeight);
int getCoverColor(FIBITMAP *bitmap, unsigned char *r, unsigned char *g, unsigned char *b);
