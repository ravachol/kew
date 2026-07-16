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

#include "sound/sound_facade.h"

#include "loader/song_loader.h"

#ifdef DEBUG
#include "utils/k_log.h"
#endif

int shuffle_enabled;

bool is_shuffle_enabled(void)
{
        return shuffle_enabled;
}

void set_shuffle_enabled(int value)
{
        AppState *state = get_app_state();

        shuffle_enabled = value == 1 ? 1 : 0;
        state->settings.shuffle_enabled = value;
}

bool is_repeat_list_enabled(void)
{
        return (sound_system_get_repeat_state() == SOUND_STATE_REPEAT_LIST);
}

void set_repeat_state(int repeat_state)
{
        AppState *state = get_app_state();

        state->settings.repeatState = repeat_state;
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

bool is_metadata_switch_reached(void)
{
        return sound_system_is_metadata_switch_reached();
}

bool is_switching_track(void)
{
        return sound_system_is_switching_track(sound_sys);
}

bool is_current_song_deleted(void)
{
        return sound_system_is_current_song_deleted(sound_sys);
}

bool is_valid_song(SongData *songdata)
{
        return songdata != NULL && songdata->hasErrors == false && songdata->magic == SONG_MAGIC &&
               songdata->metadata != NULL;
}

int get_volume()
{
        Model *model = get_model();
        return model->volume;
}

void set_volume(int vol)
{
        sound_system_set_volume(sound_sys, ((float)vol / 100));

        Model *model = get_model();
        model->volume = sound_system_get_volume(sound_sys) * 100;
}

SongData *get_current_song_data(SongData *previous_songdata)
{
        if (get_current_song() == NULL)
        {
                unload_song_data(&previous_songdata);
                return NULL;
        }
        if (is_current_song_deleted())
        {
                unload_song_data(&previous_songdata);
                return NULL;
        }

        bool isDeleted = sound_system_is_current_song_deleted(sound_sys);

        if (isDeleted && !sound_system_no_song_loaded(sound_sys))
                sound_system_switch_song_immediate(sound_sys);
        if (isDeleted)
        {
                unload_song_data(&previous_songdata);
                return NULL;
        }

        SongData *song_data = NULL;
        song_data = sound_system_get_current_song(sound_sys);

        if (!is_valid_song(song_data))
        {
                unload_song_data(&previous_songdata);
                return NULL;
        }

        if (previous_songdata && strcmp(previous_songdata->track_id, song_data->track_id) == 0)
        {
                return previous_songdata;
        }
        else
                unload_song_data(&previous_songdata);

        set_dirty(DIRTY_ALL);

        Model *model = get_model();
        model->playbackState.notifySwitch = 1;

#ifdef DEBUG
        // Log basic stats
        k_log("New songdata: %s duration: %f", song_data->file_path, song_data->duration);
#endif
        return songdata_clone(song_data);
}
