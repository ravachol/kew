#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "common/events.h"

/**
 * @file input.c
 * @brief Handles keyboard and terminal input events.
 */

#include "input.h"

#include "common/appstate.h"

#define TB_IMPL
#include "termbox2input.h"

#include "control_ui.h"
#include "player_ui.h"
#include "queue_ui.h"
#include "search_ui.h"
#include "settings.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"

#include "sys/mpris.h"
#include "sys/systemintegration.h"

#include "utils/term.h"

#include <ctype.h>
#include <gio/gio.h>
#include <glib.h>

#define MAX_TMP_SEQ_LEN 256
#define NUM_KEY_MAPPINGS 64

const int COOLDOWN_MS = 500;
const int COOLDOWN2_MS = 100;
const int MOUSE_DRAG = 32;
const int MOUSE_CLICK = 0;
const int maxDigitsPressedCount = 9;
const int fuzzySearchThreshold = 100;

EventMapping keyMappings[NUM_KEY_MAPPINGS];
struct timespec lastInputTime;
char digitsPressed[MAX_SEQ_LEN];
int digitsPressedCount;
double draggedPositionSeconds = 0.0;
bool draggingProgressBar = false;

struct Event mapTBKeyToEvent(struct tb_event *ev)
{
        struct Event event = {0};
        event.type = EVENT_NONE;

        TBKeyBinding *keyBindings = getKeyBindings();

        AppState *state = getAppState();
        if (state->currentView == SEARCH_VIEW)
        {
                // Backspace
                if (ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2)
                {
                        removeFromSearchText();
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
                // Printable character (not escape, enter, tab, carriage return)
#if defined(__ANDROID__) || defined(__APPLE__)
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ')
                {
                        char keybuf[5] = {0};
                        tb_utf8_unicode_to_char(keybuf, ev->ch);

                        if (keybuf != 'Z' && keybuf != 'X' && keybuf != 'C' && keybuf != 'V' && keybuf != 'B' && keybuf != 'N')
                        {
                                addToSearchText(keybuf);
                                resetSearchResult();
                                fuzzySearch(getLibrary(), fuzzySearchThreshold);
                                event.type = EVENT_SEARCH;
                        }
                }
#else
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ')
                {
                        char keybuf[5] = {0};
                        tb_utf8_unicode_to_char(keybuf, ev->ch);
                        addToSearchText(keybuf);
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
#endif
        }

        if (event.type == EVENT_NONE)
        {
                for (size_t i = 0; i < keybindingCount; i++)
                {
                        TBKeyBinding *b = &keyBindings[i];

                        bool keyMatch = (b->key && ev->key == b->key) || (b->ch && ev->ch == b->ch);
                        bool modsMatch = (b->mods == ev->mod);

                        if (keyMatch && modsMatch)
                        {
                                event.type = b->eventType;
                                strncpy(event.args, b->args, sizeof(event.args));
                                event.args[sizeof(event.args) - 1] = '\0';
                                break;
                        }
                }
        }

        return event;
}

bool isDigitsPressed(void)
{
        return (digitsPressedCount != 0);
}

char *getDigitsPressed(void)
{
        return digitsPressed;
}

void pressDigit(int digit)
{
        digitsPressed[0] = digit;
        digitsPressed[1] = '\0';
        digitsPressedCount = 1;
}

void resetDigitsPressed(void)
{
        memset(digitsPressed, '\0', sizeof(digitsPressed));
        digitsPressedCount = 0;
}

void updateLastInputTime(void)
{
        clock_gettime(CLOCK_MONOTONIC, &lastInputTime);
}

bool isCooldownElapsed(int milliSeconds)
{
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        double elapsedMilliseconds =
            (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
            (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;

        return elapsedMilliseconds >= milliSeconds;
}

void initKeyMappings(AppSettings *settings)
{
        AppState *state = getAppState();
        mapSettingsToKeys(settings, &(state->uiSettings), keyMappings);
}

int parseVolumeArg(const char *argStr)
{
        if (!argStr || !*argStr)
                return 0;

        // Make a copy so we can strip characters like '%'
        char buf[64];
        size_t len = 0;

        // Skip leading spaces
        while (*argStr && isspace((unsigned char)*argStr))
                argStr++;

        // Copy allowed characters (+, -, digits)
        while (*argStr && len < sizeof(buf) - 1)
        {
                if (*argStr == '+' || *argStr == '-' || isdigit((unsigned char)*argStr))
                        buf[len++] = *argStr;
                else if (*argStr == '%')
                        break; // stop at %
                else if (isspace((unsigned char)*argStr))
                        break; // stop at space
                else
                        break; // stop on anything unexpected
                argStr++;
        }

        buf[len] = '\0';

        if (len == 0)
                return 0;

        // Convert to integer
        return atoi(buf);
}

void handleEvent(struct Event *event)
{
        AppState *state = getAppState();
        AppSettings *settings = getAppSettings();
        PlayList *playlist = getPlaylist();
        int chosenRow = getChosenRow();

        switch (event->type)
        {
                break;
        case EVENT_ENQUEUE:
                viewEnqueue(false);
                break;
        case EVENT_ENQUEUEANDPLAY:
                viewEnqueue(true);
                break;
        case EVENT_PLAY_PAUSE:
                togglePause();
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggleVisualizer();
                break;
        case EVENT_TOGGLEREPEAT:
                toggleRepeat();
                break;
        case EVENT_TOGGLEASCII:
                toggleAscii();
                break;
        case EVENT_TOGGLENOTIFICATIONS:
                toggleNotifications();
                break;
        case EVENT_SHUFFLE:
                toggleShuffle();
                break;
        case EVENT_SHOWLYRICSPAGE:
                toggleShowLyricsPage();
                break;
        case EVENT_CYCLECOLORMODE:
                cycleColorMode();
                break;
        case EVENT_CYCLETHEMES:
                cycleThemes();
                break;
        case EVENT_QUIT:
                quit();
                break;
        case EVENT_SCROLLDOWN:
                scrollNext();
                break;
        case EVENT_SCROLLUP:
                scrollPrev();
                break;
        case EVENT_VOLUME_UP:
                if (event->args[0] != '\0')
                        volumeChange(parseVolumeArg(event->args));
                else
                        volumeChange(5);
                emitVolumeChanged();
                break;
        case EVENT_VOLUME_DOWN:
                if (event->args[0] != '\0')
                        volumeChange(parseVolumeArg(event->args));
                else
                        volumeChange(-5);
                emitVolumeChanged();
                break;
        case EVENT_NEXT:
                skipToNextSong();
                break;
        case EVENT_PREV:
                skipToPrevSong();
                break;
        case EVENT_SEEKBACK:
                seekBack();
                break;
        case EVENT_SEEKFORWARD:
                seekForward();
                break;
        case EVENT_ADDTOFAVORITESPLAYLIST:
                addToFavoritesPlaylist();
                break;
        case EVENT_EXPORTPLAYLIST:
                exportCurrentPlaylist(settings->path, playlist);
                break;
        case EVENT_UPDATELIBRARY:
                freeSearchResults();
                updateLibrary(settings->path);
                break;
        case EVENT_SHOWHELP:
                toggleShowView(KEYBINDINGS_VIEW);
                break;
        case EVENT_SHOWPLAYLIST:
                toggleShowView(PLAYLIST_VIEW);
                break;
        case EVENT_SHOWSEARCH:
                toggleShowView(SEARCH_VIEW);
                break;
                break;
        case EVENT_SHOWLIBRARY:
                toggleShowView(LIBRARY_VIEW);
                break;
        case EVENT_NEXTPAGE:
                flipNextPage();
                break;
        case EVENT_PREVPAGE:
                flipPrevPage();
                break;
        case EVENT_REMOVE:
                handleRemove(getChosenRow());
                resetListAfterDequeuingPlayingSong();
                break;
        case EVENT_SHOWTRACK:
                showTrack();
                break;
        case EVENT_NEXTVIEW:
                switchToNextView();
                break;
        case EVENT_PREVVIEW:
                switchToPreviousView();
                break;
        case EVENT_CLEARPLAYLIST:
                dequeueAllExceptPlayingSong();
                state->uiState.resetPlaylistDisplay = true;
                break;
        case EVENT_MOVESONGUP:
                moveSongUp(&chosenRow);
                setChosenRow(chosenRow);
                break;
        case EVENT_MOVESONGDOWN:
                moveSongDown(&chosenRow);
                setChosenRow(chosenRow);
                break;
        case EVENT_STOP:
                stop();
                break;
        case EVENT_SORTLIBRARY:
                sortLibrary();
                break;

        default:
                state->uiState.isFastForwarding = false;
                state->uiState.isRewinding = false;
                break;
        }
}

static gint64 lastScrollEventTime = 0;
static gint64 lastSeekEventTime = 0;
static gint64 lastPageEventTime = 0;

static gboolean shouldThrottle(struct Event *event)
{
        gint64 now = g_get_real_time(); // microseconds since Epoch
        gint64 delta;

        switch (event->type)
        {
        case EVENT_SCROLLUP:
        case EVENT_SCROLLDOWN:
                delta = now - lastScrollEventTime;
                if (delta < 20 * 1000) // 20ms
                        return TRUE;
                lastScrollEventTime = now;
                break;

        case EVENT_SEEKBACK:
        case EVENT_SEEKFORWARD:
                delta = now - lastSeekEventTime;
                if (delta < 20 * 1000)
                        return TRUE;
                lastSeekEventTime = now;
                break;

        case EVENT_NEXTPAGE:
        case EVENT_PREVPAGE:
                delta = now - lastPageEventTime;
                if (delta < 20 * 1000)
                        return TRUE;
                lastPageEventTime = now;
                break;

        default:
                break;
        }

        return FALSE;
}

#define MAX_SEQ_LEN 1024 // Maximum length of sequence buffer
#define MAX_TMP_SEQ_LEN 256

enum EventType getMouseLastRowEvent(int mouseXOnLastRow)
{
        enum EventType result = EVENT_NONE;
        AppState *state = getAppState();

        size_t lastRowLen = strlen(state->uiSettings.LAST_ROW);
        if (mouseXOnLastRow < 0 || (size_t)mouseXOnLastRow > lastRowLen)
        {
                // Out of bounds, return default
                return EVENT_NONE;
        }

        int viewClicked = 1;
        for (int i = 0; i < mouseXOnLastRow; i++)
        {
                if (state->uiSettings.LAST_ROW[i] == '|')
                {
                        viewClicked++;
                }
        }

        switch (viewClicked)
        {
        case 1:
                result = EVENT_SHOWPLAYLIST;
                break;
        case 2:
                result = EVENT_SHOWLIBRARY;
                break;
        case 3:
                result = EVENT_SHOWTRACK;
                break;
        case 4:
                result = EVENT_SHOWSEARCH;
                break;
        case 5:
                result = EVENT_SHOWHELP;
                break;
        default:
                result = EVENT_NONE;
                break;
        }

        // Switch to library view if track view is clicked and no song is
        // currently playing
        if (result == EVENT_SHOWTRACK && getCurrentSongData() == NULL)
        {
                result = EVENT_SHOWLIBRARY;
        }

        return result;
}

bool mouseInputHandled(char *seq, int i, struct Event *event)
{
        AppState *state = getAppState();

        if (!seq || !event)
                return false;

        if (i < 0 || i >= NUM_KEY_MAPPINGS || keyMappings[i].seq == NULL)
                return false;

        const char *expected = keyMappings[i].seq;

        char tmpSeq[MAX_SEQ_LEN];
        size_t src_len = strnlen(seq, MAX_SEQ_LEN - 1);

        if (src_len < 4) // Must be at least ESC[ M + 3 digits
                return false;

        snprintf(tmpSeq, sizeof tmpSeq, "%.*s", (int)src_len, seq);

        int mouseButton = 0, mouseX = 0, mouseY = 0;
        const char *end = tmpSeq + src_len;
        const char *p = tmpSeq + 3; // Skip ESC[ M

        for (int field = 0; field < 3 && p && *p && p < end; ++field)
        {
                char *endptr;
                long val = strtol(p, &endptr, 10);
                if (endptr == p || endptr > end) // no progress or out of bounds
                        break;
                p = endptr;

                if (*p == ';')
                        ++p;

                switch (field)
                {
                case 0:
                        mouseButton = (int)val;
                        break;
                case 1:
                        mouseX = (int)val;
                        break;
                case 2:
                        mouseY = (int)val;
                        break;
                }
        }

        ProgressBar *progressBar = getProgressBar();

        if (progressBar->length > 0)
        {
                long long deltaCol =
                    (long long)mouseX - (long long)progressBar->col;

                if (deltaCol >= 0 && deltaCol <= (long long)progressBar->length)
                {
                        double position =
                            (double)deltaCol / (double)progressBar->length;
                        double duration = getCurrentSongDuration();
                        draggedPositionSeconds = duration * position;
                }
                else
                {
                        draggedPositionSeconds = 0.0;
                }
        }
        else
        {
                draggedPositionSeconds = 0.0;
        }

        int footerRow = getFooterRow();
        int footerCol = getFooterCol();

        if (mouseY == footerRow && footerCol > 0 && mouseX - footerCol > 0 &&
            mouseX - footerCol < (int)strlen(state->uiSettings.LAST_ROW) &&
            mouseButton != MOUSE_DRAG)
        {
                event->type = getMouseLastRowEvent(mouseX - footerCol);
                return true;
        }

        if ((mouseY == progressBar->row || draggingProgressBar) &&
            mouseX - progressBar->col >= 0 &&
            mouseX - progressBar->col < progressBar->length &&
            state->currentView == TRACK_VIEW)
        {
                if (mouseButton == MOUSE_DRAG || mouseButton == MOUSE_CLICK)
                {
                        draggingProgressBar = true;
                        gint64 newPosUs =
                            (gint64)(draggedPositionSeconds * G_USEC_PER_SEC);
                        setPosition(newPosUs, getCurrentSongDuration());
                }
                return true;
        }

        size_t expected_len = strlen(expected);
        if (strlen(seq) < expected_len + 1)
                return false;

        if (strncmp(seq + 1, expected, expected_len) == 0)
        {
                event->type = keyMappings[i].eventType;
                return true;
        }

        return false;
}

bool handleMouseEvent(struct tb_event *ev, struct Event *event)
{
        if (ev->type != TB_EVENT_MOUSE)
                return false;

        int mouseX = ev->x + 1;
        int mouseY = ev->y + 1;
        uint16_t mouseKey = ev->key;
        ProgressBar *progressBar = getProgressBar();
        AppState *state = getAppState();

        // Calculate where the user clicked on the progress bar
        if (progressBar->length > 0)
        {
                long long deltaCol = (long long)mouseX - (long long)progressBar->col;

                if (deltaCol >= 0 && deltaCol <= (long long)progressBar->length)
                {
                        double position = (double)deltaCol / (double)progressBar->length;
                        double duration = getCurrentSongDuration();
                        draggedPositionSeconds = duration * position;
                }
        }

        int footerRow = getFooterRow();
        int footerCol = getFooterCol();

        // Footer click (e.g., buttons or indicators on last row)
        if ((mouseY == footerRow && footerCol > 0 &&
             mouseX - footerCol >= 0 &&
             mouseX - footerCol < (int)strlen(state->uiSettings.LAST_ROW)) &&
            mouseKey != TB_KEY_MOUSE_RELEASE)
        {
                event->type = getMouseLastRowEvent(mouseX - footerCol);
                return true;
        }

        if (mouseKey == TB_KEY_MOUSE_RELEASE)
        {
                // Mouse moved outside progress bar area → stop dragging
                draggingProgressBar = false;
                return true;
        }

        // Progress bar click or hold-drag movement
        bool inProgressBar =
            (mouseY == progressBar->row) &&
            (mouseX >= progressBar->col) &&
            (mouseX < progressBar->col + progressBar->length);

        if (inProgressBar && state->currentView == TRACK_VIEW)
        {
                // Any left press or movement within bar = update
                if (mouseKey == TB_KEY_MOUSE_LEFT || draggingProgressBar)
                {
                        draggingProgressBar = true;

                        gint64 newPosUs = (gint64)(draggedPositionSeconds * G_USEC_PER_SEC);
                        setPosition(newPosUs, getCurrentSongDuration());
                        return true;
                }
        }

        return false;
}

static gboolean onTbInput(GIOChannel *source, GIOCondition cond, gpointer data)
{
        (void)source;
        (void)cond;
        (void)data;

        char seq[MAX_SEQ_LEN];
        int seqLength = 0;

        seq[0] = '\0';

        bool cooldownElapsed = false;
        bool cooldown2Elapsed = false;

        if (isCooldownElapsed(COOLDOWN_MS))
                cooldownElapsed = true;

        if (isCooldownElapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        if (cooldownElapsed)
        {
                if (!draggingProgressBar)
                        flushSeek();
        }

        // Drain all available input
        while (1)
        {
                if (!isInputAvailable())
                        break;

                char tmpSeq[MAX_TMP_SEQ_LEN];
                int len = readInputSequence(tmpSeq, sizeof(tmpSeq));
                if (len <= 0)
                        break;

                size_t seq_len = strnlen(seq, MAX_SEQ_LEN);
                size_t remaining_space = MAX_SEQ_LEN - seq_len - 1;
                if (remaining_space < (size_t)len)
                        len = remaining_space;

                memcpy(seq + seq_len, tmpSeq, len);
                seq[seq_len + len] = '\0';
                seqLength += len;
        }

        // If we got a sequence, convert to tb_event and handle
        if (seqLength > 0)
        {
                struct tb_event ev;
                memset(&ev, 0, sizeof(ev));

                // Feed the sequence into Termbox internal buffer
                if (seqLength > 0)
                {
                        bytebuf_nputs(&global.in, seq, seqLength); // feed entire sequence at once
                }

                struct Event event;

                // Extract all events in the buffer
                while (tb_peek_event(&ev, 0) == 0)
                {
                        bool isMouseEvent = handleMouseEvent(&ev, &event);

                        if (!isMouseEvent)
                        {
                                event = mapTBKeyToEvent(&ev);
                        }

                        if (isdigit(ev.ch) && event.type == EVENT_NONE)
                        {
                                if (digitsPressedCount < maxDigitsPressedCount)
                                        digitsPressed[digitsPressedCount++] = ev.ch;
                        }

                        if (event.type != EVENT_NONE)
                        {
                                // Throttle scroll/seek/page events
                                switch (event.type)
                                {
                                case EVENT_SCROLLUP:
                                case EVENT_SCROLLDOWN:
                                case EVENT_SEEKBACK:
                                case EVENT_SEEKFORWARD:
                                case EVENT_NEXTPAGE:
                                case EVENT_PREVPAGE:
                                        if (shouldThrottle(&event))
                                                continue;
                                        break;
                                default:
                                        break;
                                }

                                // Handle song prev/next cooldown
                                if (!cooldownElapsed &&
                                    (event.type == EVENT_NEXT || event.type == EVENT_PREV))
                                        event.type = EVENT_NONE;
                                else if (event.type == EVENT_NEXT || event.type == EVENT_PREV)
                                        updateLastInputTime();

                                // Handle seek/remove cooldown
                                if (!cooldown2Elapsed &&
                                    (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
                                     event.type == EVENT_SEEKFORWARD))
                                        event.type = EVENT_NONE;
                                else if (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
                                         event.type == EVENT_SEEKFORWARD)
                                        updateLastInputTime();

                                // Forget Numbers
                                if (event.type != EVENT_ENQUEUE &&
                                    event.type != EVENT_GOTOENDOFPLAYLIST && event.type != EVENT_NONE)
                                {
                                        memset(digitsPressed, '\0', sizeof(digitsPressed));
                                        digitsPressedCount = 0;
                                }

                                handleEvent(&event);
                        }
                }
        }

        return TRUE;
}

void initInput(void)
{
        tb_init();
        // Enable SGR (1006) + drag-motion (1002)
        const char *enable_mouse = "\033[?1000h\033[?1002h\033[?1006h";
        ssize_t result = write(tb_get_output_fd(), enable_mouse, strlen(enable_mouse));
        if (result < 0)
                tb_set_input_mode(TB_INPUT_ALT | TB_INPUT_MOUSE | TB_INPUT_ESC);
        else
                tb_set_input_mode(TB_INPUT_ALT | TB_INPUT_ESC);
        int fd = tb_get_input_fd();
        GIOChannel *chan = g_io_channel_unix_new(fd);
        g_io_channel_set_encoding(chan, NULL, NULL); // binary
        g_io_add_watch(chan, G_IO_IN, (GIOFunc)onTbInput, NULL);
}

void shutdownInput(void)
{
        tb_shutdown();
}
