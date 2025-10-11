#include "appstate.h"
#include "playerops.h"
#include "term.h"
#include "utils.h"
#include "input.h"
#include "search_ui.h"
#include "settings.h"
#include "player_ui.h"
#include <ctype.h>

#define MAX_TMP_SEQ_LEN 256
#define NUM_KEY_MAPPINGS 63

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

bool isCooldownElapsed(int milliSeconds)
{
        struct timespec currentTime;
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        double elapsedMilliseconds =
            (currentTime.tv_sec - lastInputTime.tv_sec) * 1000.0 +
            (currentTime.tv_nsec - lastInputTime.tv_nsec) / 1000000.0;

        return elapsedMilliseconds >= milliSeconds;
}

void updateLastInputTime(void)
{
        clock_gettime(CLOCK_MONOTONIC, &lastInputTime);
}

void initKeyMappings(AppState *state, AppSettings* settings)
{
        mapSettingsToKeys(settings, &(state->uiSettings), keyMappings);
}

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
                result = EVENT_SHOWKEYBINDINGS;
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

bool mouseInputHandled(AppState *state, char *seq, int i, struct Event *event)
{
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
                        setPosition(newPosUs);
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

struct Event processInput(AppState *state)
{
        struct Event event;
        event.type = EVENT_NONE;
        event.key[0] = '\0';
        bool cooldownElapsed = false;
        bool cooldown2Elapsed = false;

        UISettings *ui = &(state->uiSettings);
        AppSettings *settings = getAppSettings();

        if (isCooldownElapsed(COOLDOWN_MS))
                cooldownElapsed = true;

        if (isCooldownElapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        int seqLength = 0;
        char seq[MAX_SEQ_LEN];
        seq[0] = '\0'; // Set initial value
        int keyReleased = 0;

        bool foundInput = false;

        // Find input
        while (isInputAvailable())
        {
                char tmpSeq[MAX_TMP_SEQ_LEN];

                seqLength =
                    seqLength + readInputSequence(tmpSeq, sizeof(tmpSeq));

                // Release most keys directly, seekbackward and seekforward can
                // be read continuously
                if (seqLength <= 0 &&
                    strcmp(seq + 1, settings->seekBackward) != 0 &&
                    strcmp(seq + 1, settings->seekForward) != 0)
                {
                        keyReleased = 1;
                        break;
                }

                foundInput = true;

                size_t seq_len = strnlen(seq, MAX_SEQ_LEN);
                size_t remaining_space = MAX_SEQ_LEN - seq_len;

                if (remaining_space < 1)
                {
                        break;
                }

                snprintf(seq + seq_len, remaining_space, "%s", tmpSeq);

                // This slows the continous reads down to not get a a too fast
                // scrolling speed
                if (strcmp(seq + 1, settings->hardScrollUp) == 0 ||
                    strcmp(seq + 1, settings->hardScrollDown) == 0 ||
                    strcmp(seq + 1, settings->scrollUpAlt) == 0 ||
                    strcmp(seq + 1, settings->scrollDownAlt) == 0 ||
                    strcmp(seq + 1, settings->seekBackward) == 0 ||
                    strcmp(seq + 1, settings->seekForward) == 0 ||
                    strcmp(seq + 1, settings->nextPage) == 0 ||
                    strcmp(seq + 1, settings->prevPage) == 0)
                {
                        keyReleased = 0;
                        readInputSequence(
                            tmpSeq,
                            sizeof(tmpSeq)); // Dummy read to prevent scrolling
                                             // after key released
                        break;
                }

                keyReleased = 0;
        }

        if (!foundInput && cooldownElapsed)
        {
                if (!draggingProgressBar)
                        flushSeek();
                return event;
        }

        if (keyReleased)
                return event;

        event.type = EVENT_NONE;

        c_strcpy(event.key, seq, MAX_SEQ_LEN);

        if (state->currentView == SEARCH_VIEW)
        {
                if (strcmp(event.key, "\x7F") == 0 ||
                    strcmp(event.key, "\x08") == 0)
                {
                        removeFromSearchText();
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
                else if (((strnlen(event.key, sizeof(event.key)) == 1 &&
                           event.key[0] != '\033' && event.key[0] != '\n' &&
                           event.key[0] != '\t' && event.key[0] != '\r') ||
                          strcmp(event.key, " ") == 0 ||
                          (unsigned char)event.key[0] >= 0xC0) &&
                         strcmp(event.key, "Z") != 0 &&
                         strcmp(event.key, "X") != 0 &&
                         strcmp(event.key, "C") != 0 &&
                         strcmp(event.key, "V") != 0 &&
                         strcmp(event.key, "B") != 0 &&
                         strcmp(event.key, "N") != 0)
                {
                        addToSearchText(event.key, ui);
                        resetSearchResult();
                        fuzzySearch(getLibrary(), fuzzySearchThreshold);
                        event.type = EVENT_SEARCH;
                }
        }

        if (seq[0] == 127)
        {
                seq[0] = '\b'; // Treat as Backspace
        }

        bool handledMouse = false;
        // Set event for pressed key
        for (int i = 0; i < NUM_KEY_MAPPINGS; i++)
        {
                if (keyMappings[i].seq[0] != '\0' &&
                    ((seq[0] == '\033' && strnlen(seq, MAX_SEQ_LEN) > 1 &&
                      strcmp(seq, "\033\n") != 0 &&
                      strcmp(seq + 1, keyMappings[i].seq) == 0) ||
                     strcmp(seq, keyMappings[i].seq) == 0))
                {
                        if (event.type == EVENT_SEARCH &&
                            keyMappings[i].eventType != EVENT_ENQUEUE)
                        {
                                break;
                        }

                        event.type = keyMappings[i].eventType;
                        break;
                }

                // Received mouse input instead of keyboard input
                if (keyMappings[i].seq[0] != '\0' &&
                    strncmp(seq, "\033[<", 3) == 0 &&
                    strnlen(seq, MAX_SEQ_LEN) > 4 && strchr(seq, 'M') != NULL &&
                    mouseInputHandled(state, seq, i, &event))
                {
                        handledMouse = true;
                        break;
                }
        }

        if (!handledMouse)
        {
                // Stop dragging progress bar
                draggingProgressBar = false;
        }

        for (int i = 0; i < NUM_KEY_MAPPINGS; i++)
        {
                if (strcmp(seq, "\033\n") == 0 &&
                    strcmp(keyMappings[i].seq, "^M") == 0) // ALT+ENTER
                {
                        event.type = keyMappings[i].eventType;
                        break;
                }

                if (strcmp(seq, keyMappings[i].seq) == 0 &&
                    strnlen(seq, MAX_SEQ_LEN) > 1) // ALT+something
                {
                        event.type = keyMappings[i].eventType;
                        break;
                }
        }

        // Handle numbers
        if (isdigit(event.key[0]))
        {
                if (digitsPressedCount < maxDigitsPressedCount)
                        digitsPressed[digitsPressedCount++] = event.key[0];
        }
        else
        {
                // Handle multiple digits, sometimes mixed with other keys
                for (int i = 0; i < MAX_SEQ_LEN; i++)
                {
                        if (isdigit(seq[i]))
                        {
                                if (digitsPressedCount < maxDigitsPressedCount)
                                        digitsPressed[digitsPressedCount++] =
                                            seq[i];
                        }
                        else
                        {
                                if (seq[i] == '\0')
                                        break;

                                if (seq[i] != settings->switchNumberedSong[0] &&
                                    seq[i] !=
                                        settings->hardSwitchNumberedSong[0] &&
                                    seq[i] != settings->hardEndOfPlaylist[0])
                                {
                                        memset(digitsPressed, '\0',
                                               sizeof(digitsPressed));
                                        digitsPressedCount = 0;
                                        break;
                                }
                                else if (seq[i] ==
                                         settings->hardEndOfPlaylist[0])
                                {
                                        event.type = EVENT_GOTOENDOFPLAYLIST;
                                        break;
                                }
                                else
                                {
                                        event.type = EVENT_ENQUEUE;
                                        break;
                                }
                        }
                }
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

        return event;
}
