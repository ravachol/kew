/**
 * @file queue_ui.h
 * @brief Handles high level enqueue
 *
 */

 #include "common/appstate.h"

void determine_song_and_notify(void);
void reset_list_after_dequeuing_playing_song(void);
void view_enqueue(bool playImmediately);
void handleGoToSong(void);
void update_next_song_if_needed(void);
FileSystemEntry *enqueue(FileSystemEntry *entry);
FileSystemEntry *enqueue_songs(FileSystemEntry *entry,FileSystemEntry **chosen_dir);
FileSystemEntry *libraryEnqueue(PlayList *playlist);

