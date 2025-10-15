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

#include "sound/decoders.h"
#include "sound/m4a.h"
#include "sound/playback.h"

#include "utils/utils.h"

#include <math.h>

static struct timespec startTime;
static struct timespec pauseTime;
static struct timespec current_time;
static double seekAccumulatedSeconds = 0.0;
static struct timespec lastUpdateTime = {0, 0};

static double elapsedSeconds = 0.0;

struct timespec getPauseTime(void) { return pauseTime; }

double getElapsedSeconds(void) { return elapsedSeconds; }

void resetClock(void)
{
        elapsedSeconds = 0.0;
        setPauseSeconds(0.0);
        setTotalPauseSeconds(0.0);
        setSeekElapsed(0.0);
        clock_gettime(CLOCK_MONOTONIC, &startTime);
}

void calcElapsedTime(double duration)
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

bool setPosition(gint64 newPosition, double duration)
{
        if (isPaused())
                return false;

        gint64 currentPositionMicroseconds =
            llround(getElapsedSeconds() * G_USEC_PER_SEC);

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

bool seekPosition(gint64 offset, double duration)
{
        if (isPaused())
                return false;

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

void addToAccumulatedSeconds(double value)
{
        seekAccumulatedSeconds += value;
}

void updatePauseTime(void)
{
        clock_gettime(CLOCK_MONOTONIC, &pauseTime);
}

bool flushSeek(double duration)
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
                                        return false;
                        }
#endif
                }

                setSeekElapsed(getSeekElapsed() + seekAccumulatedSeconds);
                seekAccumulatedSeconds = 0.0;
                calcElapsedTime(duration);

                float percentage = elapsedSeconds / (float)duration * 100.0;

                if (percentage < 0.0)
                {
                        setSeekElapsed(0.0);
                        percentage = 0.0;
                }

                seekPercentage(percentage);

                return true;
        }

        return false;
}
