#include "playback.h"
#include "appstate.h"
#include "file.h"
#include "sound.h"
#include "soundcommon.h"
#include "systemintegration.h"
#include "utils.h"
#include <math.h>
#include "songloader.h"

static struct timespec current_time;
static struct timespec startTime;
static struct timespec pauseTime;
static struct timespec lastUpdateTime = {0, 0};
static double seekAccumulatedSeconds = 0.0;
static double elapsedSeconds = 0.0;

struct timespec getPauseTime(void) { return pauseTime; }

double getElapsedSeconds(void) { return elapsedSeconds; }

void autostartIfStopped(FileSystemEntry *firstEnqueuedEntry, UIState *uis)
{
        PlayList *playlist = getPlaylist();
        AudioData *audioData = getAudioData();
        PlaybackState *ps = getPlaybackState();

        uis->waitingForPlaylist = false;
        uis->waitingForNext = true;
        audioData->endOfListReached = false;
        if (firstEnqueuedEntry != NULL)
                setSongToStartFrom(findPathInPlaylist(firstEnqueuedEntry->fullPath, playlist));
        ps->lastPlayedId = -1;
}

void resetClock(void)
{
        elapsedSeconds = 0.0;
        setPauseSeconds(0.0);
        setTotalPauseSeconds(0.0);
        setSeekElapsed(0.0);
        clock_gettime(CLOCK_MONOTONIC, &startTime);
}

bool isCurrentSongDeleted(void)
{
        AudioData *audioData = getAudioData();

        return (audioData == NULL || audioData->currentFileIndex == 0)
                   ? getUserData()->songdataADeleted == true
                   : getUserData()->songdataBDeleted == true;
}

int loadDecoder(SongData *songData, bool *songDataDeleted)
{
        PlaybackState *ps = getPlaybackState();
        int result = 0;

        if (songData != NULL)
        {
                *songDataDeleted = false;

                // This should only be done for the second song, as
                // switchAudioImplementation() handles the first one
                if (!ps->loadingdata.loadingFirstDecoder)
                {
                        if (hasBuiltinDecoder(songData->filePath))
                                result = prepareNextDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "opus"))
                                result =
                                    prepareNextOpusDecoder(songData->filePath);
                        else if (pathEndsWith(songData->filePath, "ogg"))
                                result = prepareNextVorbisDecoder(
                                    songData->filePath);
                        else if (pathEndsWith(songData->filePath, "webm"))
                                result = prepareNextWebmDecoder(songData);
#ifdef USE_FAAD
                        else if (pathEndsWith(songData->filePath, "m4a") ||
                                 pathEndsWith(songData->filePath, "aac"))
                                result = prepareNextM4aDecoder(songData);

#endif
                }
        }
        return result;
}

int assignLoadedData(void)
{
        PlaybackState *ps = getPlaybackState();
        int result = 0;

        if (ps->loadingdata.loadA)
        {
                getUserData()->songdataA = ps->loadingdata.songdataA;
                result = loadDecoder(ps->loadingdata.songdataA,
                                     &(getUserData()->songdataADeleted));
        }
        else
        {
                getUserData()->songdataB = ps->loadingdata.songdataB;
                result = loadDecoder(ps->loadingdata.songdataB,
                                     &(getUserData()->songdataBDeleted));
        }

        return result;
}


void *songDataReaderThread(void *arg)
{
        PlaybackState *ps = (PlaybackState *)arg;

        // Acquire the mutex lock
        pthread_mutex_lock(&(ps->loadingdata.mutex));

        char filepath[MAXPATHLEN];
        c_strcpy(filepath, ps->loadingdata.filePath, sizeof(filepath));

        SongData *songdata = NULL;

        if (ps->loadingdata.loadA)
        {
                if (!getUserData()->songdataADeleted)
                {
                        getUserData()->songdataADeleted = true;
                        unloadSongData(&(ps->loadingdata.songdataA), ps->loadingdata.state);
                }
        }
        else
        {
                if (!getUserData()->songdataBDeleted)
                {
                        getUserData()->songdataBDeleted = true;
                        unloadSongData(&(ps->loadingdata.songdataB), ps->loadingdata.state);
                }
        }

        if (existsFile(filepath) >= 0)
        {
                songdata = loadSongData(filepath, ps->loadingdata.state);
        }
        else
                songdata = NULL;

        if (ps->loadingdata.loadA)
        {
                ps->loadingdata.songdataA = songdata;
        }
        else
        {
                ps->loadingdata.songdataB = songdata;
        }

        int result = assignLoadedData();

        if (result < 0)
                songdata->hasErrors = true;

        // Release the mutex lock
        pthread_mutex_unlock(&(ps->loadingdata.mutex));

        if (songdata == NULL || songdata->hasErrors)
        {
                ps->songHasErrors = true;
                ps->clearingErrors = true;
                setNextSong(NULL);
        }
        else
        {
                ps->songHasErrors = false;
                ps->clearingErrors = false;
                setNextSong(getTryNextSong());
                setTryNextSong(NULL);
        }

        ps->loadingdata.state->uiState.loadedNextSong = true;
        ps->skipping = false;
        ps->songLoading = false;

        return NULL;
}

void loadSong(Node *song, LoadingThreadData *loadingdata, UIState *uis)
{
        PlaybackState *ps = getPlaybackState();

        if (song == NULL)
        {
                uis->loadedNextSong = true;
                ps->skipping = false;
                ps->songLoading = false;
                return;
        }

        c_strcpy(loadingdata->filePath, song->song.filePath,
                 sizeof(loadingdata->filePath));

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread, ps);
}

void tryLoadNext(AppState *state)
{
        PlaybackState *ps = getPlaybackState();
        UIState *uis = &(state->uiState);
        Node *current = getCurrentSong();
        Node *tryNextSong = getTryNextSong();

        ps->songHasErrors = false;
        ps->clearingErrors = true;

        if (tryNextSong == NULL && current != NULL)
                tryNextSong = current->next;
        else if (tryNextSong != NULL)
                tryNextSong = tryNextSong->next;

        if (tryNextSong != NULL)
        {
                ps->songLoading = true;
                ps->loadingdata.state = state;
                ps->loadingdata.loadA = !ps->usingSongDataA;
                ps->loadingdata.loadingFirstDecoder = false;
                loadSong(tryNextSong, &ps->loadingdata, uis);
        }
        else
        {
                ps->clearingErrors = false;
        }
}

bool isValidSong(SongData *songData)
{
        return songData != NULL && songData->hasErrors == false &&
               songData->metadata != NULL;
}

bool determineCurrentSongData(SongData **currentSongData)
{
        AudioData *audioData = getAudioData();

        *currentSongData = (audioData->currentFileIndex == 0)
                               ? getUserData()->songdataA
                               : getUserData()->songdataB;

        bool isDeleted = (audioData->currentFileIndex == 0)
                             ? getUserData()->songdataADeleted == true
                             : getUserData()->songdataBDeleted == true;

        if (isDeleted)
        {
                *currentSongData = (audioData->currentFileIndex != 0)
                                       ? getUserData()->songdataA
                                       : getUserData()->songdataB;

                isDeleted = (audioData->currentFileIndex != 0)
                                ? getUserData()->songdataADeleted == true
                                : getUserData()->songdataBDeleted == true;

                if (!isDeleted)
                {
                        activateSwitch(audioData);
                        audioData->switchFiles = false;
                }
        }
        return isDeleted;
}

void playbackPause(AppState *state, struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback(state);
}

SongData *getCurrentSongData(void)
{
        if (getCurrentSong() == NULL)
                return NULL;

        if (isCurrentSongDeleted())
                return NULL;

        SongData *songData = NULL;

        bool isDeleted = determineCurrentSongData(&songData);

        if (isDeleted)
                return NULL;

        if (!isValidSong(songData))
                return NULL;

        return songData;
}

double getCurrentSongDuration(void)
{
        double duration = 0.0;
        SongData *currentSongData = getCurrentSongData();

        if (currentSongData != NULL)
                duration = currentSongData->duration;

        return duration;
}

bool setPosition(gint64 newPosition)
{
        if (isPaused())
                return false;

        gint64 currentPositionMicroseconds =
            llround(elapsedSeconds * G_USEC_PER_SEC);
        double duration = getCurrentSongDuration();

        if (duration != 0.0)
        {
                gint64 step = newPosition - currentPositionMicroseconds;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
}

bool seekPosition(gint64 offset)
{
        if (isPaused())
                return false;

        double duration = getCurrentSongDuration();
        if (duration != 0.0)
        {
                gint64 step = offset;
                step = step / G_USEC_PER_SEC;
                seekAccumulatedSeconds += step;
                return true;
        }
        else
        {
                return false;
        }
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

void skipToBegginningOfSong(void)
{
        resetClock();

        if (getCurrentSong() != NULL)
        {
                seekPercentage(0);
                emitSeekedSignal(0.0);
        }
}

void prepareIfSkippedSilent(void)
{
        PlaybackState *ps = getPlaybackState();

        if (ps->hasSilentlySwitched)
        {
                ps->skipping = true;
                ps->hasSilentlySwitched = false;
                resetClock();
                setCurrentImplementationType(NONE);
                setRepeatEnabled(false);

                AudioData *audioData = getAudioData();

                audioData->endOfListReached = false;

                ps->usingSongDataA = !ps->usingSongDataA;

                ps->skipping = false;
        }
}

void playbackPlay(AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        if (isPaused())
        {
                setTotalPauseSeconds(getTotalPauseSeconds() + getPauseSeconds());
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
        else if (isStopped())
        {
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }

        if (isStopped() && !ps->hasSilentlySwitched)
        {
                skipToBegginningOfSong();
        }

        resumePlayback(state);

        if (ps->hasSilentlySwitched)
        {
                setTotalPauseSeconds(0);
                prepareIfSkippedSilent();
        }
}

void loadFirstSong(Node *song, AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        if (song == NULL)
                return;

        ps->loadingdata.state = state;
        ps->loadingdata.loadingFirstDecoder = true;
        loadSong(song, &ps->loadingdata, &(state->uiState));

        int i = 0;
        while (!state->uiState.loadedNextSong && i < 10000)
        {
                if (i != 0 && i % 1000 == 0 && state->uiSettings.uiEnabled)
                        printf(".");
                c_sleep(10);
                fflush(stdout);
                i++;
        }
}

void unloadSongA(AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        if (getUserData()->songdataADeleted == false)
        {
                getUserData()->songdataADeleted = true;
                unloadSongData(&(ps->loadingdata.songdataA), state);
                getUserData()->songdataA = NULL;
        }
}

void unloadSongB(AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        if (getUserData()->songdataBDeleted == false)
        {
                getUserData()->songdataBDeleted = true;
                unloadSongData(&(ps->loadingdata.songdataB), state);
                getUserData()->songdataB = NULL;
        }
}

void unloadPreviousSong(AppState *state)
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
                unloadSongA(state);

                if (!audioData->endOfListReached)
                        state->uiState.loadedNextSong = false;

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
                unloadSongB(state);

                if (!audioData->endOfListReached)
                        state->uiState.loadedNextSong = false;

                ps->usingSongDataA = true;
        }

        pthread_mutex_unlock(&(ps->loadingdata.mutex));
}

int loadFirst(Node *song, AppState *state)
{
        loadFirstSong(song, state);
        Node *current = getCurrentSong();
        PlaybackState *ps = getPlaybackState();

        ps->usingSongDataA = true;

        while (ps->songHasErrors && current->next != NULL)
        {
                ps->songHasErrors = false;
                state->uiState.loadedNextSong = false;
                current = current->next;
                loadFirstSong(current, state);
        }

        if (ps->songHasErrors)
        {
                // Couldn't play any of the songs
                unloadPreviousSong(state);
                current = NULL;
                ps->songHasErrors = false;
                return -1;
        }

        UserData *userData = getUserData();

        userData->currentPCMFrame = 0;
        userData->currentSongData = userData->songdataA;

        return 0;
}

void skip(AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        setCurrentImplementationType(NONE);

        setRepeatEnabled(false);

        AudioData *audioData = getAudioData();

        audioData->endOfListReached = false;

        if (!isPlaying())
        {
                switchAudioImplementation(state);
                ps->skipFromStopped = true;
        }
        else
        {
                setSkipToNext(true);
        }

        if (!ps->skipOutOfOrder)
                triggerRefresh();
}

bool isValidAudioNode(Node *node)
{
        if (!node)
                return false;
        if (node->id < 0)
                return false;
        if (!node->song.filePath ||
            strnlen(node->song.filePath, MAXPATHLEN) == 0)
                return false;

        return true;
}

int play(Node *node, AppState *state)
{
        PlaybackState *ps = getPlaybackState();
        AudioData *audioData = getAudioData();

        if (!isValidAudioNode(node))
        {
                fprintf(stderr, "Song is invalid.\n");
                return -1;
        }

        setCurrentSong(node);

        ps->skipping = true;
        ps->skipOutOfOrder = true;
        ps->songLoading = true;
        ps->forceSkip = false;

        state->uiState.loadedNextSong = false;

        // Cancel starting from top
        if (state->uiState.waitingForPlaylist || audioData->restart)
        {
                state->uiState.waitingForPlaylist = false;
                audioData->restart = false;

                if (isShuffleEnabled())
                        reshufflePlaylist();
        }

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;

        loadSong(node, &ps->loadingdata, &(state->uiState));

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

                if (node->next != NULL)
                {
                        return -1;
                }
        }

        resetClock();
        skip(state);

        return 0;
}

void skipToSong(int id, bool startPlaying, AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        if (ps->songLoading || !state->uiState.loadedNextSong || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        PlayList *playlist = getPlaylist();
        Node *found = NULL;

        findNodeInList(playlist, id, &found);

        if (startPlaying)
        {
                //FIXME: These code blocks are sinful
                double totalPauseSeconds = getTotalPauseSeconds();
                double pauseSeconds = getTotalPauseSeconds();

                playbackPlay(state);

                setTotalPauseSeconds(totalPauseSeconds);
                setPauseSeconds(pauseSeconds);
        }

        play(found, state);
}

void stop(AppState *state)
{
        stopPlayback(state);

        if (isStopped())
        {
                skipToBegginningOfSong();
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
        }
}

void calcElapsedTime(void)
{
        if (isStopped())
                return;

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        double timeSinceLastUpdate =
            (double)(current_time.tv_sec - lastUpdateTime.tv_sec) +
            (double)(current_time.tv_nsec - lastUpdateTime.tv_nsec) / 1e9;

        if (!isPaused())
        {
                elapsedSeconds =
                    (double)(current_time.tv_sec - startTime.tv_sec) +
                    (double)(current_time.tv_nsec - startTime.tv_nsec) / 1e9;
                double seekElapsed = getSeekElapsed();
                double diff =
                    elapsedSeconds +
                    (seekElapsed + seekAccumulatedSeconds - getTotalPauseSeconds());
                double duration = getCurrentSongDuration();

                if (diff < 0)
                        seekElapsed -= diff;

                elapsedSeconds +=
                    seekElapsed + seekAccumulatedSeconds - getTotalPauseSeconds();

                if (elapsedSeconds > duration)
                        elapsedSeconds = duration;

                setSeekElapsed(seekElapsed);

                if (elapsedSeconds < 0.0)
                {
                        elapsedSeconds = 0.0;
                }

                if (getCurrentSong() != NULL && timeSinceLastUpdate >= 1.0)
                {
                        lastUpdateTime = current_time;
                }
        }
        else
        {
                setPauseSeconds((double)(current_time.tv_sec - pauseTime.tv_sec) +
                                (double)(current_time.tv_nsec - pauseTime.tv_nsec) / 1e9);
        }
}

void flushSeek(void)
{
        if (seekAccumulatedSeconds != 0.0)
        {
                Node *current = getCurrentSong();

                if (current != NULL)
                {
#ifdef USE_FAAD
                        if (pathEndsWith(current->song.filePath, "aac"))
                        {
                                m4a_decoder *decoder = getCurrentM4aDecoder();
                                if (decoder->fileType == k_rawAAC)
                                        return;
                        }
#endif
                }

                setSeekElapsed(getSeekElapsed() + seekAccumulatedSeconds);
                seekAccumulatedSeconds = 0.0;
                calcElapsedTime();
                double duration = getCurrentSongDuration();
                float percentage = elapsedSeconds / (float)duration * 100.0;

                if (percentage < 0.0)
                {
                        setSeekElapsed(0.0);
                        percentage = 0.0;
                }

                seekPercentage(percentage);

                emitSeekedSignal(elapsedSeconds);
        }
}

void togglePause(AppState *state)
{
        PlaybackState *ps = getPlaybackState();

        togglePausePlayback(state);
        if (isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, &pauseTime);
        }
        else
        {
                if (ps->hasSilentlySwitched && !ps->skipping)
                {
                        setTotalPauseSeconds(0);
                        prepareIfSkippedSilent();
                }
                else
                {
                        setTotalPauseSeconds(getTotalPauseSeconds() + getPauseSeconds());
                }
                emitStringPropertyChanged("PlaybackStatus", "Playing");
        }
}

void seekForward(UIState *uis)
{
        Node *current = getCurrentSong();

        if (current != NULL)
        {
#ifdef USE_FAAD
                if (pathEndsWith(current->song.filePath, "aac"))
                {
                        m4a_decoder *decoder = getCurrentM4aDecoder();
                        if (decoder != NULL && decoder->fileType == k_rawAAC)
                                return;
                }
#endif
                if (isPaused())
                        return;

                double duration = current->song.duration;
                if (duration != 0.0)
                {
                        float step = 100 / uis->numProgressBars;
                        seekAccumulatedSeconds += step * duration / 100.0;
                }
                uis->isFastForwarding = true;
        }
}

void seekBack(UIState *uis)
{
        Node *current = getCurrentSong();

        if (current != NULL)
        {
#ifdef USE_FAAD
                if (pathEndsWith(current->song.filePath, "aac"))
                {

                        m4a_decoder *decoder = getCurrentM4aDecoder();
                        if (decoder != NULL && decoder->fileType == k_rawAAC)
                                return;
                }
#endif
                if (isPaused())
                        return;

                double duration = current->song.duration;
                if (duration != 0.0)
                {
                        float step = 100 / uis->numProgressBars;
                        seekAccumulatedSeconds -= step * duration / 100.0;
                }
                uis->isRewinding = true;
        }
}
