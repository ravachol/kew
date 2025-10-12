#ifndef LIBRARY_OPS_H
#define LIBRARY_OPS_H

#include "appstate.h"

void createLibrary(AppSettings *settings, AppState *state);
void updateLibrary(AppState *state, char *path);
void updateLibraryIfChangedDetected(AppState *state);
void askIfCacheLibrary(UISettings *ui);
void sortLibrary(void);
void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist);
bool markAsDequeued(FileSystemEntry *root, char *path);
void dequeueAllExceptPlayingSong(AppState *state);

#endif
