/**
 * @file playback_system.c
 * @brief Low-level audio system integration.
 *
 * Manages initialization, configuration, and teardown of
 * audio backends and decoders. Provides the connection between
 * playback operations and sound output subsystems.
 */

#include "playback_system.h"

#include "common/appstate.h"
#include "sound/sound.h"
#include "sound/soundcommon.h"

#include "data/songloader.h"

void playbackSafeCleanup(void)
{
        AppState *state = getAppState();
        pthread_mutex_lock(&(state->dataSourceMutex));
        cleanupPlaybackDevice();
        pthread_mutex_unlock(&(state->dataSourceMutex));
}

void playbackCleanup(void)
{
        cleanupPlaybackDevice();
}

void playbackSwitchDecoder(void)
{
        switchAudioImplementation();
}

int playbackCreate(void)
{
        return createAudioDevice();
}


void playbackShutdown(void)
{
        if (isContextInitialized())
        {
#ifdef __ANDROID__
                shutdownAndroid();
#else
                playbackCleanup();
#endif
                cleanupAudioContext();
        }
}

void playbackUnloadSongs(UserData *userData)
{
        PlaybackState *ps = getPlaybackState();

        if (!userData->songdataADeleted)
        {
                userData->songdataADeleted = true;
                unloadSongData(&(ps->loadingdata.songdataA));
        }
        if (!userData->songdataBDeleted)
        {
                userData->songdataBDeleted = true;
                unloadSongData(&(ps->loadingdata.songdataB));
        }
}

void skip()
{
        PlaybackState *ps = getPlaybackState();

        setCurrentImplementationType(NONE);

        setRepeatEnabled(false);

        AudioData *audioData = getAudioData();

        audioData->endOfListReached = false;

        if (!isPlaying())
        {
                switchAudioImplementation();
                ps->skipFromStopped = true;
        }
        else
        {
                setSkipToNext(true);
        }

        if (!ps->skipOutOfOrder)
                triggerRefresh();
}

void playbackFreeDecoders(void)
{
        resetAllDecoders();
}


