/**
 * @file trackmanager.[c]
 * @brief Central track indexing and metadata manager.
 *
 * Manages track objects, metadata lookup, and synchronization
 * between the library and active playlists. Provides helper
 * functions for finding and referencing track data.
 */

#include "trackmanager.h"

#include "common/appstate.h"
#include "sound/sound.h"

#include "data/songloader.h"

#include "utils/utils.h"

void loadFirstSong(Node *song)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        if (song == NULL)
                return;

        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = true;
        loadSong(song, &ps->loadingdata);

        int i = 0;
        while (!ps->loadedNextSong && i < 10000)
        {
                if (i != 0 && i % 1000 == 0 && state->uiSettings.uiEnabled)
                        printf(".");
                c_sleep(10);
                fflush(stdout);
                i++;
        }
}

void unloadSongA(void)
{
        PlaybackState *ps = getPlaybackState();

        if (getUserData()->songdataADeleted == false)
        {
                getUserData()->songdataADeleted = true;
                unloadSongData(&(ps->loadingdata.songdataA));
                getUserData()->songdataA = NULL;
        }
}

void unloadSongB(void)
{
        PlaybackState *ps = getPlaybackState();

        if (getUserData()->songdataBDeleted == false)
        {
                getUserData()->songdataBDeleted = true;
                unloadSongData(&(ps->loadingdata.songdataB));
                getUserData()->songdataB = NULL;
        }
}

void unloadPreviousSong(void)
{
        UserData *userData = getUserData();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        pthread_mutex_lock(&(ps->loadingdata.mutex));

        if (ps->usingSongDataA &&
            (ps->skipping || (userData->currentSongData == NULL ||
                              userData->songdataADeleted == false ||
                              (ps->loadingdata.songdataA != NULL &&
                               userData->songdataADeleted == false &&
                               userData->currentSongData->hasErrors == 0 &&
                               userData->currentSongData->trackId != NULL &&
                               strcmp(ps->loadingdata.songdataA->trackId,
                                      userData->currentSongData->trackId) != 0))))
        {
                unloadSongA();

                if (!audioData->endOfListReached)
                        ps->loadedNextSong = false;

                ps->usingSongDataA = false;
        }
        else if (!ps->usingSongDataA &&
                 (ps->skipping ||
                  (userData->currentSongData == NULL ||
                   userData->songdataBDeleted == false ||
                   (ps->loadingdata.songdataB != NULL &&
                    userData->songdataBDeleted == false &&
                    userData->currentSongData->hasErrors == 0 &&
                    userData->currentSongData->trackId != NULL &&
                    strcmp(ps->loadingdata.songdataB->trackId,
                           userData->currentSongData->trackId) != 0))))
        {
                unloadSongB();

                if (!audioData->endOfListReached)
                        ps->loadedNextSong = false;

                ps->usingSongDataA = true;
        }

        pthread_mutex_unlock(&(ps->loadingdata.mutex));
}

int loadFirst(Node *song)
{
        loadFirstSong(song);
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        ps->usingSongDataA = true;

        while (ps->songHasErrors && current->next != NULL)
        {
                ps->songHasErrors = false;
                ps->loadedNextSong = false;
                current = current->next;
                loadFirstSong(current);
        }

        if (ps->songHasErrors)
        {
                // Couldn't play any of the songs
                unloadPreviousSong();
                current = NULL;
                ps->songHasErrors = false;
                return -1;
        }

        UserData *userData = getUserData();

        userData->currentPCMFrame = 0;
        userData->currentSongData = userData->songdataA;

        return 0;
}

void loadNextSong(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();

        ps->songLoading = true;
        ps->nextSongNeedsRebuilding = false;
        ps->skipFromStopped = false;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        setTryNextSong(getListNext(getCurrentSong()));
        setNextSong(getTryNextSong());
        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = false;
        loadSong(getNextSong(), &ps->loadingdata);
}

void finishLoading(void)
{
        PlaybackState *ps = getPlaybackState();

        int maxNumTries = 20;
        int numtries = 0;

        while (!ps->loadedNextSong && numtries < maxNumTries)
        {
                c_sleep(100);
                numtries++;
        }

        ps->loadedNextSong = true;
}

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry)
{
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        ps->waitingForPlaylist = false;
        ps->waitingForNext = true;
        audioData->endOfListReached = false;
        if (firstEnqueuedEntry != NULL)
                setSongToStartFrom(findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist));
        ps->lastPlayedId = -1;
}
