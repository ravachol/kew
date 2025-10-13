/**
 * @file playback_ops.[c]
 * @brief Core playback control API.
 *
 * Contains functions to control playback: play, pause, stop, seek,
 * volume adjustments, and track skipping. This module is UI-agnostic
 * and interacts directly with the playback state and audio backends.
 */

#include "playback_ops.h"

#include "playback_clock.h"
#include "playback_system.h"
#include "playback_state.h"
#include "playlist_ops.h"

#include "common/appstate.h"

#include "sound/sound.h"
#include "sound/soundcommon.h"

#include "utils/file.h"
#include "utils/utils.h"

#include "data/songloader.h"

#include "sys/systemintegration.h"

void playbackStop(void)
{
        stopPlayback();
}

void playbackResumePlayback(void)
{
        resumePlayback();
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
                        unloadSongData(&(ps->loadingdata.songdataA));
                }
        }
        else
        {
                if (!getUserData()->songdataBDeleted)
                {
                        getUserData()->songdataBDeleted = true;
                        unloadSongData(&(ps->loadingdata.songdataB));
                }
        }

        if (existsFile(filepath) >= 0)
        {
                songdata = loadSongData(filepath);
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

        ps->loadedNextSong = true;
        ps->skipping = false;
        ps->songLoading = false;

        return NULL;
}

void loadSong(Node *song, LoadingThreadData *loadingdata)
{
        PlaybackState *ps = getPlaybackState();

        if (song == NULL)
        {
                ps->loadedNextSong = true;
                ps->skipping = false;
                ps->songLoading = false;
                return;
        }

        c_strcpy(loadingdata->filePath, song->song.filePath,
                 sizeof(loadingdata->filePath));

        pthread_t loadingThread;
        pthread_create(&loadingThread, NULL, songDataReaderThread, ps);
}

void tryLoadNext(void)
{
        AppState *state = getAppState();
        PlaybackState *ps = getPlaybackState();
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
                loadSong(tryNextSong, &ps->loadingdata);
        }
        else
        {
                ps->clearingErrors = false;
        }
}

void playbackPause(struct timespec *pause_time)
{
        if (!isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                clock_gettime(CLOCK_MONOTONIC, pause_time);
        }
        pausePlayback();
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

void playbackPlay(void)
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

        resumePlayback();

        if (ps->hasSilentlySwitched)
        {
                setTotalPauseSeconds(0);
                prepareIfSkippedSilent();
        }
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

int play(Node *node)
{
        AppState *state = getAppState();
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

        ps->loadedNextSong = false;

        // Cancel starting from top
        if (ps->waitingForPlaylist || audioData->restart)
        {
                ps->waitingForPlaylist = false;
                audioData->restart = false;

                if (isShuffleEnabled())
                        reshufflePlaylist();
        }

        ps->loadingdata.state = state;
        ps->loadingdata.loadA = !ps->usingSongDataA;
        ps->loadingdata.loadingFirstDecoder = true;

        loadSong(node, &ps->loadingdata);

        int maxNumTries = 50;
        int numtries = 0;

        while (!ps->loadedNextSong && numtries < maxNumTries)
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
        skip();

        return 0;
}

void playbackVolumeChange(int changePercent)
{
        adjustVolumePercent(changePercent);
}

void skipToSong(int id, bool startPlaying)
{
        PlaybackState *ps = getPlaybackState();

        if (ps->songLoading || !ps->loadedNextSong || ps->skipping || ps->clearingErrors)
                if (!ps->forceSkip)
                        return;

        PlayList *playlist = getPlaylist();
        Node *found = NULL;

        findNodeInList(playlist, id, &found);

        if (startPlaying)
        {
                double totalPauseSeconds = getTotalPauseSeconds();
                double pauseSeconds = getTotalPauseSeconds();

                playbackPlay();

                setTotalPauseSeconds(totalPauseSeconds);
                setPauseSeconds(pauseSeconds);
        }

        play(found);
}

void stop(void)
{
        stopPlayback();

        if (isStopped())
        {
                skipToBegginningOfSong();
                emitStringPropertyChanged("PlaybackStatus", "Stopped");
        }
}

void togglePause(void)
{
        PlaybackState *ps = getPlaybackState();

        if (isStopped())
        {
                resetClock();
        }

        togglePausePlayback();

        if (isPaused())
        {
                emitStringPropertyChanged("PlaybackStatus", "Paused");
                updatePauseTime();
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

void playbackSeekBack(int seconds)
{
        Node *current = getCurrentSong();
        if (current == NULL)
                return;

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
        if (duration <= 0.0)
                return;

        addToAccumulatedSeconds(-seconds);
}

void playbackSeekForward(int seconds)
{
        Node *current = getCurrentSong();
        if (current == NULL)
                return;

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
        if (duration <= 0.0)
                return;

        addToAccumulatedSeconds(seconds);
}

