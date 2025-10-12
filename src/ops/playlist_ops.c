/**
 * @file playlist_ops.[c]
 * @brief Playlist management operations.
 *
 * Implements functionality for adding, removing, reordering,
 * shuffling, and retrieving songs within playlists.
 * Coordinates with the playback system for next/previous transitions.
 */

#include "playlist_ops.h"

#include "playback_ops.h"
#include "playback_clock.h"
#include "playback_system.h"
#include "trackmanager.h"
#include "library_ops.h"

#include "common/appstate.h"

#include "sound/soundcommon.h"

#include "data/songloader.h"

#include "sys/systemintegration.h"

#include "utils/utils.h"

Node *chooseNextSong(void)
{

        Node *current = getCurrentSong();

        Node *nextSong = getNextSong();

        if (nextSong != NULL)
                return nextSong;
        else if (current != NULL && current->next != NULL)
        {
                return current->next;
        }
        else
        {
                return NULL;
        }
}

Node *findSelectedEntryById(PlayList *playlist, int id)
{
        Node *node = playlist->head;

        if (node == NULL || id < 0)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (node != NULL && node->id == id)
                {
                        found = true;
                        break;
                }
                else if (node == NULL)
                {
                        return NULL;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

Node *findSelectedEntry(PlayList *playlist, int row)
{
        Node *node = playlist->head;

        if (node == NULL)
                return NULL;

        bool found = false;

        for (int i = 0; i < playlist->count; i++)
        {
                if (i == row)
                {
                        found = true;
                        break;
                }
                node = node->next;
        }

        if (found)
        {
                return node;
        }

        return NULL;
}

void removeCurrentlyPlayingSong(AppState *state)
{
        Node *current = getCurrentSong();
        UIState *uis = &(state->uiState);
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        if (current != NULL)
        {
                stopPlayback(state);
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
                clearCurrentTrack();
                clearCurrentSong();
        }

        uis->loadedNextSong = false;
        audioData->restart = true;
        audioData->endOfListReached = true;

        if (current != NULL)
        {
                ps->lastPlayedId = current->id;
                setSongToStartFrom(getListNext(current));
        }
        uis->waitingForNext = true;
        current = NULL;
}

void rebuildNextSong(Node *song, AppState *state)
{
        if (song == NULL)
                return;

        UIState *uis = &(state->uiState);
        PlaybackState *ps = getPlaybackState();

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = false;

        ps->songLoading = true;

        loadSong(song, &ps->loadingdata, uis);

        int maxNumTries = 50;
        int numtries = 0;

        while (ps->songLoading && !uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }
        ps->songLoading = false;
}

void handleRemove(AppState *state, int chosenRow)
{
        UIState *uis = &(state->uiState);
        PlayList *playlist = getPlaylist();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();

        if (state->currentView == PLAYLIST_VIEW)
        {

                bool rebuild = false;

                Node *node = findSelectedEntry(unshuffledPlaylist, chosenRow);

                if (node == NULL)
                {
                        return;
                }

                Node *current = getCurrentSong();
                Node *song = chooseNextSong();
                int id = node->id;
                int currentId = (current != NULL) ? current->id : -1;

                if (currentId == node->id)
                {
                        removeCurrentlyPlayingSong(state);
                }
                else
                {
                        if (getSongToStartFrom() != NULL)
                        {
                                setSongToStartFrom(getListNext(node));
                        }
                }

                pthread_mutex_lock(&(playlist->mutex));

                if (node != NULL && song != NULL && current != NULL)
                {
                        if (strcmp(song->song.filePath, node->song.filePath) ==
                                0 ||
                            (current != NULL && current->next != NULL &&
                             id == current->next->id))
                                rebuild = true;
                }

                if (node != NULL)
                        markAsDequeued(getLibrary(), node->song.filePath);

                Node *node2 = findSelectedEntryById(playlist, id);

                if (node != NULL)
                        deleteFromList(unshuffledPlaylist, node);

                if (node2 != NULL)
                        deleteFromList(playlist, node2);

                if (isShuffleEnabled())
                        rebuild = true;

                current = findSelectedEntryById(playlist, currentId);

                if (rebuild && current != NULL)
                {
                        node = NULL;
                        setNextSong(NULL);
                        reshufflePlaylist();

                        setTryNextSong(current->next);
                        PlaybackState *ps = getPlaybackState();
                        ps->nextSongNeedsRebuilding = false;
                        setNextSong(NULL);
                        setNextSong(getListNext(current));
                        rebuildNextSong(getNextSong(), state);
                        uis->loadedNextSong = true;
                }

                pthread_mutex_unlock(&(playlist->mutex));
        }
        else
        {
                return;
        }

        triggerRefresh();
}

void addToFavoritesPlaylist(void)
{
        Node *current = getCurrentSong();
        PlayList *favoritesPlaylist = getFavoritesPlaylist();

        if (current == NULL)
                return;

        int id = current->id;

        Node *node = NULL;

        if (findSelectedEntryById(favoritesPlaylist, id) !=
            NULL) // Song is already in list
                return;

        createNode(&node, current->song.filePath, id);
        addToList(favoritesPlaylist, node);
}

// Go through the display playlist and the shuffle playlist to remove all songs
// except the current one. If no active song (if stopped rather than paused for
// example) entire playlist will be removed
void dequeueAllExceptPlayingSong(AppState *state)
{
        bool clearAll = false;
        int currentID = -1;

        Node *current = getCurrentSong();
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();

        // Do we need to clear the entire playlist?
        if (current == NULL)
        {
                clearAll = true;
        }
        else
        {
                currentID = current->id;
        }

        int nextInPlaylistID;
        pthread_mutex_lock(&(playlist->mutex));
        Node *songToBeRemoved;
        Node *nextInPlaylist = unshuffledPlaylist->head;

        while (nextInPlaylist != NULL)
        {
                nextInPlaylistID = nextInPlaylist->id;

                if (clearAll || nextInPlaylistID != currentID)
                {
                        songToBeRemoved = nextInPlaylist;

                        nextInPlaylist = nextInPlaylist->next;

                        int id = songToBeRemoved->id;

                        // Update Library
                        if (songToBeRemoved != NULL)
                                markAsDequeued(getLibrary(),
                                               songToBeRemoved->song.filePath);

                        // Remove from Display playlist
                        if (songToBeRemoved != NULL)
                                deleteFromList(unshuffledPlaylist,
                                               songToBeRemoved);

                        // Remove from Shuffle playlist
                        Node *node2 = findSelectedEntryById(playlist, id);
                        if (node2 != NULL)
                                deleteFromList(playlist, node2);
                }
                else
                {
                        nextInPlaylist = nextInPlaylist->next;
                }
        }
        pthread_mutex_unlock(&(playlist->mutex));

        PlaybackState *ps = getPlaybackState();
        ps->nextSongNeedsRebuilding = true;
        setNextSong(NULL);

        // Only refresh the screen if it makes sense to do so
        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW)
        {
                triggerRefresh();
        }
}

Node *getSongByNumber(PlayList *playlist, int songNumber)
{
        Node *song = playlist->head;

        if (!song)
                return getCurrentSong();

        if (songNumber <= 0)
        {
                return song;
        }

        int count = 1;

        while (song->next != NULL && count != songNumber)
        {
                song = getListNext(song);
                count++;
        }

        return song;
}

void setCurrentSongToNext(void)
{
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        if (current != NULL)
                ps->lastPlayedId = current->id;

        setCurrentSong(chooseNextSong());
}

void setCurrentSongToPrev(void)
{
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        if (current != NULL && current->prev != NULL)
        {
                ps->lastPlayedId = current->id;
                setCurrentSong(current->prev);
        }
}

void silentSwitchToNext(bool loadSong, AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        ps->skipping = true;

        setNextSong(NULL);
        setCurrentSongToNext();

        AudioData *audioData = getAudioData();

        activateSwitch(audioData);

        ps->skipOutOfOrder = true;
        ps->usingSongDataA = (audioData != NULL && audioData->currentFileIndex == 0);

        if (loadSong)
        {
                loadNextSong(state);
                finishLoading(&(state->uiState));
                state->uiState.loadedNextSong = true;
                state->uiState.doNotifyMPRISSwitched = true;
        }

        resetClock();

        triggerRefresh();

        ps->skipping = false;
        ps->hasSilentlySwitched = true;
        ps->nextSongNeedsRebuilding = true;

        setNextSong(NULL);
}

void silentSwitchToPrev(AppState *state)
{
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        ps->skipping = true;

        setCurrentSongToPrev();
        activateSwitch(audioData);

        state->uiState.loadedNextSong = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        ps->usingSongDataA = !ps->usingSongDataA;

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &ps->loadingdata, &(state->uiState));
        state->uiState.doNotifyMPRISSwitched = true;
        finishLoading(&(state->uiState));

        resetClock();

        triggerRefresh();
        ps->skipping = false;
        ps->nextSongNeedsRebuilding = true;
        setNextSong(NULL);

        ps->skipOutOfOrder = true;
        ps->hasSilentlySwitched = true;
}

void skipToNextSong(AppState *state)
{
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        // Stop if there is no song or no next song
        if (current == NULL || current->next == NULL)
        {
                if (isRepeatListEnabled())
                {
                        clearCurrentSong();
                }
                else if (!isStopped() && !isPaused())
                {
                        stop(state);
                        return;
                }
                else
                {
                        return;
                }
        }

        if (ps->songLoading || ps->nextSongNeedsRebuilding || ps->skipping ||
            ps->clearingErrors)
                return;

        if (isStopped() || isPaused())
        {
                silentSwitchToNext(true, state);
                return;
        }

        if (isShuffleEnabled())
                state->uiState.resetPlaylistDisplay = true;

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        ps->skipping = true;
        ps->skipOutOfOrder = false;

        resetClock();

        skip(state);
}

void skipToPrevSong(AppState *state)
{
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        if (current == NULL)
        {
                if (!isStopped() && !isPaused())
                        stop(state);
                return;
        }

        if (ps->songLoading || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        if (isStopped() || isPaused())
        {
                silentSwitchToPrev(state);
                return;
        }

        Node *song = current;

        setCurrentSongToPrev();

        if (song == current)
        {
                resetClock();
                updatePlaybackPosition(
                    0); // We need to signal to mpris that the song was
                        // reset to the beginning
        }

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        ps->skipping = true;
        ps->skipOutOfOrder = true;
        state->uiState.loadedNextSong = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &ps->loadingdata, &(state->uiState));
        int maxNumTries = 50;
        int numtries = 0;

        while (!state->uiState.loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (ps->songHasErrors)
        {
                ps->songHasErrors = false;
                ps->forceSkip = true;
                skipToPrevSong(state);
        }

        resetClock();

        skip(state);
}

void skipToNumberedSong(int songNumber, AppState *state)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();

        if (ps->songLoading || !uis->loadedNextSong || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        double totalPauseSeconds = getTotalPauseSeconds();
        double pauseSeconds = getTotalPauseSeconds();

        playbackPlay(state);

        setTotalPauseSeconds(totalPauseSeconds);
        setPauseSeconds(pauseSeconds);

        ps->skipping = true;
        ps->skipOutOfOrder = true;
        uis->loadedNextSong = false;
        ps->songLoading = true;
        ps->forceSkip = false;

        setCurrentSong(getSongByNumber(unshuffledPlaylist, songNumber));

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;
        loadSong(getCurrentSong(), &ps->loadingdata, uis);
        int maxNumTries = 50;
        int numtries = 0;

        while (!uis->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        if (ps->songHasErrors)
        {
                ps->songHasErrors = false;
                ps->forceSkip = true;
                if (songNumber < playlist->count)
                        skipToNumberedSong(songNumber + 1, state);
        }

        resetClock();
        skip(state);
}

void skipToLastSong(AppState *state)
{
        PlayList *playlist = getPlaylist();

        Node *song = playlist->head;

        if (!song)
                return;

        int count = 1;
        while (song->next != NULL)
        {
                song = getListNext(song);
                count++;
        }
        skipToNumberedSong(count, state);
}

void repeatList(AppState *state)
{
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        state->uiState.waitingForPlaylist = true;
        ps->nextSongNeedsRebuilding = true;
        audioData->endOfListReached = false;
}

void moveSongUp(AppState *state, int *chosenRow)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();

        if (state->currentView != PLAYLIST_VIEW)
        {
                return;
        }

        bool rebuild = false;

        Node *node = findSelectedEntry(unshuffledPlaylist, *chosenRow);

        if (node == NULL)
        {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        Node *current = getCurrentSong();

        if (node != NULL && current != NULL)
        {
                // Rebuild if current song, the next song or the song after are
                // affected
                if (current != NULL)
                {
                        Node *tmp = current;

                        for (int i = 0; i < 3; i++)
                        {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id)
                                {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }
                }
        }

        moveUpList(unshuffledPlaylist, node);
        Node *plNode = findSelectedEntryById(playlist, node->id);

        if (!isShuffleEnabled())
                moveUpList(playlist, plNode);

        *chosenRow = *chosenRow - 1;
        *chosenRow = (*chosenRow > 0) ? *chosenRow : 0;

        if (rebuild && current != NULL)
        {
                node = NULL;
                ps->nextSongNeedsRebuilding = false;

                setTryNextSong(current->next);
                setNextSong(getListNext(current));
                rebuildNextSong(getNextSong(), state);

                uis->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        triggerRefresh();
}

void moveSongDown(AppState *state, int *chosenRow)
{
        UIState *uis = &(state->uiState);
        PlayList *unshuffledPlaylist = getUnshuffledPlaylist();
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();

        if (state->currentView != PLAYLIST_VIEW)
        {
                return;
        }

        bool rebuild = false;

        Node *node = findSelectedEntry(unshuffledPlaylist, *chosenRow);

        Node *current = getCurrentSong();

        if (node == NULL)
        {
                return;
        }

        int id = node->id;

        pthread_mutex_lock(&(playlist->mutex));

        if (node != NULL && current != NULL)
        {
                // Rebuild if current song, the next song or the previous song
                // are affected
                if (current != NULL)
                {
                        Node *tmp = current;

                        for (int i = 0; i < 2; i++)
                        {
                                if (tmp == NULL)
                                        break;

                                if (tmp->id == id)
                                {
                                        rebuild = true;
                                }
                                tmp = tmp->next;
                        }

                        if (current->prev != NULL && current->prev->id == id)
                                rebuild = true;
                }
        }

        moveDownList(unshuffledPlaylist, node);
        Node *plNode = findSelectedEntryById(playlist, node->id);

        if (!isShuffleEnabled())
                moveDownList(playlist, plNode);

        *chosenRow = *chosenRow + 1;
        *chosenRow = (*chosenRow >= unshuffledPlaylist->count)
                        ? unshuffledPlaylist->count - 1
                        : *chosenRow;

        if (rebuild && current != NULL)
        {
                node = NULL;
                ps->nextSongNeedsRebuilding = false;

                setTryNextSong(current->next);
                setNextSong(getListNext(current));
                rebuildNextSong(getNextSong(), state);
                uis->loadedNextSong = true;
        }

        pthread_mutex_unlock(&(playlist->mutex));

        triggerRefresh();
}

void reshufflePlaylist(void)
{
        PlayList *playlist = getPlaylist();
        PlaybackState *ps = getPlaybackState();

        if (isShuffleEnabled())
        {
                Node *current = getCurrentSong();

                if (current != NULL)
                        shufflePlaylistStartingFromSong(playlist, current);
                else
                        shufflePlaylist(playlist);

                ps->nextSongNeedsRebuilding = true;
        }
}
