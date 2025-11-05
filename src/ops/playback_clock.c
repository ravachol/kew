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

#include "sound/decoders.h"
#ifdef USE_FAAD
#include "sound/m4a.h"
#endif
#include "sound/playback.h"

#include "sys/sys_integration.h"
#include "utils/utils.h"

#include <math.h>

static struct timespec start_time;
static struct timespec pause_time;
static struct timespec current_time;
static double seek_accumulated_seconds = 0.0;
static struct timespec last_update_time = {0, 0};

static double elapsed_seconds = 0.0;

struct timespec get_pause_time(void)
{
        return pause_time;
}

double get_elapsed_seconds(void)
{
        return elapsed_seconds;
}

void reset_clock(void)
{
        elapsed_seconds = 0.0;
        set_pause_seconds(0.0);
        set_total_pause_seconds(0.0);
        set_seek_elapsed(0.0);
        clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void calc_elapsed_time(double duration)
{
        if (pb_is_stopped())
                return;

        clock_gettime(CLOCK_MONOTONIC, &current_time);

        double time_since_last_update =
            (double)(current_time.tv_sec - last_update_time.tv_sec) +
            (double)(current_time.tv_nsec - last_update_time.tv_nsec) / 1e9;

        if (!pb_is_paused()) {
                elapsed_seconds =
                    (double)(current_time.tv_sec - start_time.tv_sec) +
                    (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
                double seek_elapsed = get_seek_elapsed();
                double diff =
                    elapsed_seconds +
                    (seek_elapsed + seek_accumulated_seconds - get_total_pause_seconds());

                if (diff < 0)
                        seek_elapsed -= diff;

                elapsed_seconds +=
                    seek_elapsed + seek_accumulated_seconds - get_total_pause_seconds();

                if (elapsed_seconds > duration)
                        elapsed_seconds = duration;

                set_seek_elapsed(seek_elapsed);

                if (elapsed_seconds < 0.0) {
                        elapsed_seconds = 0.0;
                }

                if (get_current_song() != NULL && time_since_last_update >= 1.0) {
                        last_update_time = current_time;
                }
        } else {
                set_pause_seconds((double)(current_time.tv_sec - pause_time.tv_sec) +
                                  (double)(current_time.tv_nsec - pause_time.tv_nsec) / 1e9);
        }
}

bool set_position(gint64 new_position, double duration)
{
        if (pb_is_paused())
                return false;

        gint64 currentPositionMicroseconds =
            llround(get_elapsed_seconds() * G_USEC_PER_SEC);

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
        if (pb_is_paused())
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
        if (seek_accumulated_seconds != 0.0) {
                Node *current_song = get_current_song();

                if (current_song != NULL) {
#ifdef USE_FAAD
                        if (path_ends_with(current_song->song.file_path, "aac")) {
                                m4a_decoder *decoder = get_current_m4a_decoder();
                                if (decoder->file_type == k_rawAAC)
                                        return false;
                        }
#endif
                }

                set_seek_elapsed(get_seek_elapsed() + seek_accumulated_seconds);
                seek_accumulated_seconds = 0.0;
                double duration = get_current_song_duration();
                calc_elapsed_time(duration);
                float percentage = elapsed_seconds / (float)duration * 100.0;

                if (percentage < 0.0) {
                        set_seek_elapsed(0.0);
                        percentage = 0.0;
                }

                seek_percentage(percentage);

                emit_seeked_signal(elapsed_seconds);

                return true;
        }

        return false;
}
