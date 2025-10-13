/**
 * @file playback_clock.h
 * @brief Playback timing and synchronization utilities.
 *
 * Handles timing measurements for song progress, seek operations,
 * and playback duration calculations. Uses system timers or
 * monotonic clocks to maintain precise playback timing.
 */

#include <time.h>
#include <glib.h>
#include <stdbool.h>

void resetClock(void);
double getElapsedSeconds(void);
struct timespec getPauseTime(void);
void calcElapsedTime(double duration);
bool setPosition(gint64 newPosition, double duration);
bool seekPosition(gint64 offset, double duration);
bool flushSeek(double duration);
void updatePauseTime(void);
void addToAccumulatedSeconds(double value);


