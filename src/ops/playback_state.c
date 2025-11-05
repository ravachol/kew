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

static bool repeat_list_enabled = false;
static bool shuffle_enabled = false;

bool is_repeat_list_enabled(void)
{
        return repeat_list_enabled;
}

void set_repeat_list_enabled(bool value)
{
        repeat_list_enabled = value;
}

bool is_shuffle_enabled(void)
{
        return shuffle_enabled;
}

void set_shuffle_enabled(bool value)
{
        shuffle_enabled = value;
}

bool is_repeat_enabled(void)
{
        return pb_is_repeat_enabled();
}

bool is_paused(void)
{
        return pb_is_paused();
}

bool is_stopped(void)
{
        return pb_is_stopped();
}

bool is_EOF_reached(void)
{
        return pb_is_EOF_reached();
}

bool is_impl_switch_reached(void)
{
        return pb_is_impl_switch_reached();
}

bool is_current_song_deleted(void)
{

        return (audio_data.currentFileIndex == 0)
                   ? audio_data.pUserData->songdataADeleted == true
                   : audio_data.pUserData->songdataBDeleted == true;
}

bool is_valid_song(SongData *song_data)
{
        return song_data != NULL && song_data->hasErrors == false &&
               song_data->metadata != NULL;
}

void set_EOF_handled(void)
{
        pb_set_EOF_handled();
}

double get_current_song_duration(void)
{
        double duration = 0.0;
        SongData *current_song_data = get_current_song_data();

        if (current_song_data != NULL)
                duration = current_song_data->duration;

        return duration;
}

bool determine_current_song_data(SongData **current_song_data)
{
        *current_song_data = (audio_data.currentFileIndex == 0)
                                 ? audio_data.pUserData->songdataA
                                 : audio_data.pUserData->songdataB;

        bool isDeleted = (audio_data.currentFileIndex == 0)
                             ? audio_data.pUserData->songdataADeleted == true
                             : audio_data.pUserData->songdataBDeleted == true;

        if (isDeleted) {
                *current_song_data = (audio_data.currentFileIndex != 0)
                                         ? audio_data.pUserData->songdataA
                                         : audio_data.pUserData->songdataB;

                isDeleted = (audio_data.currentFileIndex != 0)
                                ? audio_data.pUserData->songdataADeleted == true
                                : audio_data.pUserData->songdataBDeleted == true;

                if (!isDeleted) {
                        activate_switch(&audio_data);
                        audio_data.switchFiles = false;
                }
        }
        return isDeleted;
}

int get_volume()
{
        return get_current_volume();
}

void get_format_and_sample_rate(ma_format *format, ma_uint32 *sample_rate)
{
        get_current_format_and_sample_rate(format, sample_rate);
}

SongData *get_current_song_data(void)
{
        if (get_current_song() == NULL)
                return NULL;

        if (is_current_song_deleted())
                return NULL;

        SongData *song_data = NULL;

        bool isDeleted = determine_current_song_data(&song_data);

        if (isDeleted)
                return NULL;

        if (!is_valid_song(song_data))
                return NULL;

        return song_data;
}
