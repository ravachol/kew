/**
 * @file queue_ops.[h]
 * @brief Handles high level enqueue
 *
 */

 #include "common/appstate.h"

void init(void);
void determineSongAndNotify(void);
void resetListAfterDequeuingPlayingSong(void);
void enqueueHandler(bool playImmediately);
void enqueueAndPlay(void);
void handleGoToSong(void);
void updateNextSongIfNeeded(void);
FileSystemEntry *enqueue(FileSystemEntry *entry);
FileSystemEntry *libraryEnqueue(PlayList *playlist);

