/**
 * @file library_ops.[h]
 * @brief Music library management and scanning operations.
 *
 * Responsible for reading directories, and updating the in-memory library
 * representation used by playlists
 * and the UI browser.
 */

#ifndef LIBRARY_OPS_H
#define LIBRARY_OPS_H

#include "common/appstate.h"

void createLibrary(AppSettings *settings, AppState *state, char *libFilepath);
void updateLibrary(AppState *state, char *path);
void askIfCacheLibrary(UISettings *ui);
void sortLibrary(void);
void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist);
bool markAsDequeued(FileSystemEntry *root, char *path);
void enqueueSong(FileSystemEntry *child);
void dequeueSong(FileSystemEntry *child, AppState *state);
void dequeueChildren(FileSystemEntry *parent, AppState *state);
void enqueueChildren(FileSystemEntry *child, FileSystemEntry **firstEnqueuedEntry);
bool hasSongChildren(FileSystemEntry *entry);
bool hasDequeuedChildren(FileSystemEntry *parent);
bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry);
FileSystemEntry *enqueueSongs(FileSystemEntry *entry, AppState *state, FileSystemEntry **chosenDir);

#endif
