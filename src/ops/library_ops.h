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

void create_library(bool set_enqueued_status);
void update_library(char *path, bool wait_until_complete);
void ask_if_cache_library(void);
void sort_library(void);
void reset_sort_library(void);
void mark_list_as_enqueued(FileSystemEntry *root, PlayList *playlist);
void enqueue_song(FileSystemEntry *child);
void dequeue_song(FileSystemEntry *child);
void dequeue_children(FileSystemEntry *parent);
void set_childrens_queued_status_on_parents(FileSystemEntry *parent, bool wanted_status);
int enqueue_children(FileSystemEntry *child, FileSystemEntry **first_enqueued_entry);
bool mark_as_dequeued(FileSystemEntry *root, char *path);
bool has_song_children(FileSystemEntry *entry);
bool has_dequeued_children(FileSystemEntry *parent);
bool is_contained_within(FileSystemEntry *entry, FileSystemEntry *containing_entry);
void update_library_if_changed_detected(bool wait_until_complete);
#endif
