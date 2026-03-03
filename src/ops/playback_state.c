/**
 * @file playback_state.c
 * @brief Maintains playback runtime state.
 *
 * Stores and manages the current playback status, elapsed time,
 * current song pointer, and related flags (paused, stopped, etc.).
 * Provides accessors and mutators for playback state data.
 */

#include "playback_state.h"

#include "common/appstate.h"

#include "common/appstate.h"

#include "sound/sound_facade.h"

int shuffle_enabled;

bool is_shuffle_enabled(void)
{
        return shuffle_enabled;
}

void set_shuffle_enabled(int value)
{
        AppState *state = get_app_state();

        shuffle_enabled = value == 1 ? 1 : 0;
        state->uiSettings.shuffle_enabled = value;
}

bool is_repeat_list_enabled(void)
{
        return (sound_system_get_repeat_state() == SOUND_STATE_REPEAT_LIST);
}

void set_repeat_state(int repeat_state)
{
        AppState *state = get_app_state();

        state->uiSettings.repeatState = repeat_state;
        sound_system_set_repeat_state(repeat_state);
}

int get_repeat_state(void)
{
        return sound_system_get_repeat_state();
}

int get_current_sample_rate(void)
{
        return sound_system_get_sample_rate(sound_sys);
}

bool is_repeat_enabled(void)
{
        return (sound_system_get_repeat_state() == SOUND_STATE_REPEAT);
}

bool is_paused(void)
{
        return (sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED);
}

bool is_stopped(void)
{
        return (sound_system_get_state(sound_sys) == SOUND_STATE_STOPPED);
}

bool is_EOF_reached(void)
{
        return sound_system_is_EOF_reached();
}

bool is_switching_track(void)
{
        return sound_system_is_switching_track(sound_sys);
}

bool is_current_song_deleted(void)
{
        return sound_system_is_current_song_deleted(sound_sys);
}

bool is_valid_song(SongData *song_data)
{
        return song_data != NULL && song_data->hasErrors == false &&
               song_data->metadata != NULL;
}

void set_EOF_handled(void)
{
        sound_system_set_EOF_handled(sound_sys);
}

double get_current_song_duration(void)
{
        double duration = 0.0;
        SongData *current_song_data = get_current_song_data();

        if (current_song_data != NULL)
                duration = current_song_data->duration;

        return duration;
}

int get_volume()
{
        return (int)(sound_system_get_volume(sound_sys) * 100);
}

void set_volume(int vol)
{
        sound_system_set_volume(sound_sys, ((float)vol / 100));
}

SongData *get_current_song_data(void)
{
        if (get_current_song() == NULL)
                return NULL;

        if (is_current_song_deleted())
                return NULL;

        SongData *song_data = NULL;

        song_data = sound_system_get_current_song(sound_sys);

        bool isDeleted = sound_system_is_current_song_deleted(sound_sys);

        if (isDeleted && !sound_system_no_song_loaded(sound_sys))
                sound_system_switch_song_immediate(sound_sys);

        if (isDeleted)
                return NULL;

        if (!is_valid_song(song_data))
                return NULL;

        return song_data;
}
