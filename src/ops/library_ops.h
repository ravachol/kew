/**
 * @file library_ops.h
 * @brief Music library management and scanning operations.
 *
 * Responsible for reading directories, and updating the in-memory library
 * representation used by playlists
 * and the UI browser.
 */

#ifndef LIBRARY_OPS_H
#define LIBRARY_OPS_H

#include "common/appstate.h"

void createLibrary(void);
void updateLibrary(char *path);
void askIfCacheLibrary(void);
void sortLibrary(void);
void markListAsEnqueued(FileSystemEntry *root, PlayList *playlist);
void enqueueSong(FileSystemEntry *child);
void dequeueSong(FileSystemEntry *child);
void dequeueChildren(FileSystemEntry *parent);
void enqueueChildren(FileSystemEntry *child, FileSystemEntry **firstEnqueuedEntry);
bool markAsDequeued(FileSystemEntry *root, char *path);
bool hasSongChildren(FileSystemEntry *entry);
bool hasDequeuedChildren(FileSystemEntry *parent);
bool isContainedWithin(FileSystemEntry *entry, FileSystemEntry *containingEntry);

#endif
