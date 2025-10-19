/**
 * @file playback_state.c
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "playback_state.h"

#include "sound/playback.h"
#include "sound/volume.h"

static bool repeatListEnabled = false;
static bool shuffleEnabled = false;

bool isRepeatListEnabled(void) { return repeatListEnabled; }

void setRepeatListEnabled(bool value) { repeatListEnabled = value; }

bool isShuffleEnabled(void) { return shuffleEnabled; }

void setShuffleEnabled(bool value) { shuffleEnabled = value; }

bool opsIsRepeatEnabled(void)
{
        return isRepeatEnabled();
}

bool opsIsPaused(void)
{
        return isPaused();
}

bool opsIsStopped(void)
{
        return isStopped();
}

bool opsIsDone(void)
{
        return isPlaybackDone();
}

bool opsIsEof(void)
{
        return isEOFReached();
}

bool opsIsImplSwitchReached(void)
{
        return isImplSwitchReached();
}

bool isCurrentSongDeleted(void)
{

        return (audioData.currentFileIndex == 0)
                   ? audioData.pUserData->songdataADeleted == true
                   : audioData.pUserData->songdataBDeleted == true;
}

bool isValidSong(SongData *songData)
{
        return songData != NULL && songData->hasErrors == false &&
               songData->metadata != NULL;
}

void opsSetEofHandled(void)
{
        setEofHandled();
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
        *currentSongData = (audioData.currentFileIndex == 0)
                               ? audioData.pUserData->songdataA
                               : audioData.pUserData->songdataB;

        bool isDeleted = (audioData.currentFileIndex == 0)
                             ? audioData.pUserData->songdataADeleted == true
                             : audioData.pUserData->songdataBDeleted == true;

        if (isDeleted)
        {
                *currentSongData = (audioData.currentFileIndex != 0)
                                       ? audioData.pUserData->songdataA
                                       : audioData.pUserData->songdataB;

                isDeleted = (audioData.currentFileIndex != 0)
                                ? audioData.pUserData->songdataADeleted == true
                                : audioData.pUserData->songdataBDeleted == true;

                if (!isDeleted)
                {
                        activateSwitch(&audioData);
                        audioData.switchFiles = false;
                }
        }
        return isDeleted;
}

int getVolume()
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


