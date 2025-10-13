/**
 * @file playback_state.c
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "playback_state.h"

#include "sound/sound.h"
#include "sound/soundcommon.h"

bool playbackIsRepeatEnabled(void)
{
        return isRepeatEnabled();
}

bool playbackIsRepeatListEnabled(void)
{
        return isRepeatListEnabled();
}

bool playbackIsShuffleEnabled(void)
{
        return isShuffleEnabled();
}

bool playbackIsPaused(void)
{
        return isPaused();
}

bool playbackIsStopped(void)
{
        return isStopped();
}

bool playbackisDone(void)
{
        return isPlaybackDone();
}

bool playbackIsEof(void)
{
        return isEOFReached();
}

bool playbackIsImplSwitchReached(void)
{
        return isImplSwitchReached();
}

bool isCurrentSongDeleted(void)
{
        AudioData *audioData = getAudioData();

        return (audioData == NULL || audioData->currentFileIndex == 0)
                   ? getUserData()->songdataADeleted == true
                   : getUserData()->songdataBDeleted == true;
}

bool isValidSong(SongData *songData)
{
        return songData != NULL && songData->hasErrors == false &&
               songData->metadata != NULL;
}

void playbackSetEofHandled(void)
{
        setEOFNotReached();
}

void playbackSetRepeatEnabled(bool value)
{
        setRepeatEnabled(value);
}

void playbackSetRepeatListEnabled(bool value)
{
        setRepeatListEnabled(value);
}

void playbackSetShuffleEnabled(bool value)
{
        setShuffleEnabled(value);
}

UserData *playbackGetUserData(void)
{
        return getUserData();
}

double getCurrentSongDuration(void)
{
        double duration = 0.0;
        SongData *currentSongData = getCurrentSongData();

        if (currentSongData != NULL)
                duration = currentSongData->duration;

        return duration;
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

int playbackGetVolume()
{
        return getCurrentVolume();
}

void getFormatAndSampleRate(ma_format *format, ma_uint32 *sampleRate)
{
        getCurrentFormatAndSampleRate(format, sampleRate);
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


