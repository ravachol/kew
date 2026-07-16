/**
 * @file playback_clock.c
 * @brief Playback timing and synchronization utilities.
 *
 * Handles timing measurements for song progress, seek operations,
 * and playback duration calculations. Uses system timers or
 * monotonic clocks to maintain precise playback timing.
 */

#include "playback_clock.h"

#include "common/appstate.h"

#include "playback_state.h"

#include "sound/sound_facade.h"

#include "sys/sys_integration.h"

#include "utils/utils.h"
#include "utils/k_log.h"

#include <math.h>

static struct timespec start_time;
static struct timespec pause_time;
static struct timespec current_time;
static double seek_accumulated_seconds = 0.0;
static struct timespec last_update_time = {0, 0};

double get_elapsed_seconds(void)
{
        Model *model = get_model();
        return model->elapsed_seconds;
}

void timespec_add_ms(struct timespec *ts, long offset_ms)
{
        ts->tv_sec += offset_ms / 1000;
        ts->tv_nsec += (offset_ms % 1000) * 1000000L;

        if (ts->tv_nsec >= 1000000000L) {
                ts->tv_sec += ts->tv_nsec / 1000000000L;
                ts->tv_nsec %= 1000000000L;
        }
}

void clock_add_offset(long offset_ms)
{
        timespec_add_ms(&start_time, offset_ms);
}

void fprintf_timespec(const struct timespec *ts)
{
        k_log("tv_sec = %lld, tv_nsec = %ld\n",
                (long long)ts->tv_sec,
                ts->tv_nsec);
}

void clock_log_time(void)
{
        fprintf_timespec(&start_time);
        fprintf_timespec(&current_time);

        Model *model = get_model();
        k_log("elapsed seconds: %f\n", model->elapsed_seconds);
}

void reset_clock(void)
{
        Model *model = get_model();
        model->elapsed_seconds = 0.0;
        set_pause_seconds(0.0);
        set_total_pause_seconds(0.0);
        sound_system_set_seek_elapsed(0.0);
        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void calc_elapsed_time(double duration)
{
        Model *model = get_model();

        if (sound_system_get_state(sound_sys) == SOUND_STATE_STOPPED)
                return;

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        double time_since_last_update =
            (double)(current_time.tv_sec - last_update_time.tv_sec) +
            (double)(current_time.tv_nsec - last_update_time.tv_nsec) / 1e9;

        if (sound_system_get_state(sound_sys) != SOUND_STATE_PAUSED) {
                model->elapsed_seconds =
                    (double)(current_time.tv_sec - start_time.tv_sec) +
                    (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
                double seek_elapsed = sound_system_get_seek_elapsed();
                double diff =
                    model->elapsed_seconds +
                    (seek_elapsed + seek_accumulated_seconds - get_total_pause_seconds());

                if (diff < 0)
                        seek_elapsed -= diff;

                model->elapsed_seconds +=
                    seek_elapsed + seek_accumulated_seconds - get_total_pause_seconds();

                if (model->elapsed_seconds > duration)
                        model->elapsed_seconds = duration;

                sound_system_set_seek_elapsed(seek_elapsed);

                if (model->elapsed_seconds < 0.0) {
                        model->elapsed_seconds = 0.0;
                }

                if (get_current_song() != NULL && time_since_last_update >= 1.0) {
                        last_update_time = current_time;
                }
        } else {
                set_pause_seconds((double)(current_time.tv_sec - pause_time.tv_sec) +
                                  (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9);
        }
}

bool set_position(gint64 new_position)
{
        Model *model = get_model();
        double duration = model->song_duration;

        if (sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED)
                return false;

        gint64 currentPositionMicroseconds =
            llround(model->elapsed_seconds * G_USEC_PER_SEC);

        if (duration != 0.0) {
                gint64 step = new_position - currentPositionMicroseconds;
                step = step / G_USEC_PER_SEC;

                seek_accumulated_seconds += step;
                return true;
        } else {
                return false;
        }
}

bool seek_position(gint64 offset, double duration)
{
        if (sound_system_get_state(sound_sys) == SOUND_STATE_PAUSED)
                return false;

        if (duration != 0.0) {
                gint64 step = offset;
                step = step / G_USEC_PER_SEC;
                seek_accumulated_seconds += step;
                return true;
        } else {
                return false;
        }
}

void add_to_accumulated_seconds(double value)
{
        seek_accumulated_seconds += value;
}

void update_pause_time(void)
{
        clock_gettime(CLOCK_MONOTONIC, &pause_time);
}

bool flush_seek(void)
{
        Model *model = get_model();

        if (seek_accumulated_seconds != 0.0) {

                sound_system_set_seek_elapsed(sound_system_get_seek_elapsed() + seek_accumulated_seconds);
                seek_accumulated_seconds = 0.0;
                double duration = model->song_duration;
                calc_elapsed_time(duration);
                float percentage = model->elapsed_seconds / (float)duration * 100.0;

                if (percentage < 0.0) {
                        sound_system_set_seek_elapsed(0.0);
                        percentage = 0.0;
                }

                sound_system_seek_percentage(sound_sys, percentage);

                emit_seeked_signal(model->elapsed_seconds);

                if (model->restore_volume) {
                        c_sleep(200);
                        set_volume(model->volume);
                        model->restore_volume = false;
                }

                get_playback_state()->notifySeek = true;

                return true;
        }

        return false;
}
