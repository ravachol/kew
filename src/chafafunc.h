#include <chafa.h>
#include <chafa-canvas-config.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

float calcAspectRatio();
void printImage(const char *image_path, int width, int height);
unsigned char *getBitmap(const char *image_path, int *width, int *height);
void printSquareBitmapCentered(unsigned char *pixels, int width, int height, int baseHeight);
int getCoverColor(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b);
