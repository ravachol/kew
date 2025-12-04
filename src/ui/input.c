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
#include "common/common.h"

#define TB_IMPL
#include "termbox2_input.h"

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
#include "sys/sys_integration.h"

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
const int max_digits_pressed_count = 9;
const int fuzzy_search_threshold = 100;

EventMapping key_mappings[NUM_KEY_MAPPINGS];
struct timespec last_input_time;
char digits_pressed[MAX_SEQ_LEN];
int digits_pressed_count;
double dragged_position_seconds = 0.0;
bool dragging_progress_bar = false;

struct Event map_tb_key_to_event(struct tb_event *ev)
{
        struct Event event = {0};
        event.type = EVENT_NONE;

        TBKeyBinding *key_bindings = get_key_bindings();

        AppState *state = get_app_state();
        if (state->currentView == SEARCH_VIEW) {

                if (ev->key == TB_KEY_SPACE && ev->mod == 0)
                        ev->ch =  ' ';

                // Backspace
                if (ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2) {
                        remove_from_search_text();
                        reset_search_result();
                        fuzzy_search(get_library(), fuzzy_search_threshold);
                        event.type = EVENT_SEARCH;
                }
                // Printable character (not escape, enter, tab, carriage return)
#if defined(__ANDROID__) || defined(__APPLE__)
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        if (ev->ch != 'Z' && ev->ch != 'X' && ev->ch != 'C' && ev->ch != 'V' && ev->ch != 'B' && ev->ch != 'N') {
                                char keybuf[5] = {0};
                                tb_utf8_unicode_to_char(keybuf, ev->ch);
                                add_to_search_text(keybuf);
                                reset_search_result();
                                fuzzy_search(get_library(), fuzzy_search_threshold);
                                event.type = EVENT_SEARCH;
                        }
                }
#else
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        char keybuf[5] = {0};
                        tb_utf8_unicode_to_char(keybuf, ev->ch);
                        add_to_search_text(keybuf);
                        reset_search_result();
                        fuzzy_search(get_library(), fuzzy_search_threshold);
                        event.type = EVENT_SEARCH;
                }
#endif
        }

        if (event.type == EVENT_NONE) {
                for (size_t i = 0; i < keybinding_count; i++) {
                        TBKeyBinding *b = &key_bindings[i];

                        if (isupper((unsigned char)ev->ch))
                        {
                                ev->mod |= TB_MOD_SHIFT;
                                ev->ch = tolower(ev->ch);
                        }

                        bool keyMatch = (b->key && ev->key == b->key) || (b->ch && ev->ch == b->ch);
                        bool modsMatch = (b->mods == ev->mod);

                        if (keyMatch && modsMatch) {
                                event.type = b->eventType;
                                strncpy(event.args, b->args, sizeof(event.args));
                                event.args[sizeof(event.args) - 1] = '\0';
                                break;
                        }
                }
        }

        return event;
}

bool is_digits_pressed(void)
{
        return (digits_pressed_count != 0);
}

char *get_digits_pressed(void)
{
        return digits_pressed;
}

void press_digit(int digit)
{
        digits_pressed[0] = digit;
        digits_pressed[1] = '\0';
        digits_pressed_count = 1;
}

void reset_digits_pressed(void)
{
        memset(digits_pressed, '\0', sizeof(digits_pressed));
        digits_pressed_count = 0;
}

void update_last_input_time(void)
{
        clock_gettime(CLOCK_MONOTONIC, &last_input_time);
}

bool is_cooldown_elapsed(int milli_seconds)
{
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        double elapsed_milliseconds =
            (current_time.tv_sec - last_input_time.tv_sec) * 1000.0 +
            (current_time.tv_nsec - last_input_time.tv_nsec) / 1000000.0;

        return elapsed_milliseconds >= milli_seconds;
}

void init_key_mappings(AppSettings *settings)
{
        map_settings_to_keys(settings, key_mappings);
}

int parse_volume_arg(const char *arg_str)
{
        if (!arg_str || !*arg_str)
                return 0;

        // Make a copy so we can strip characters like '%'
        char buf[64];
        size_t len = 0;

        // Skip leading spaces
        while (*arg_str && isspace((unsigned char)*arg_str))
                arg_str++;

        // Copy allowed characters (+, -, digits)
        while (*arg_str && len < sizeof(buf) - 1) {
                if (*arg_str == '+' || *arg_str == '-' || isdigit((unsigned char)*arg_str))
                        buf[len++] = *arg_str;
                else if (*arg_str == '%')
                        break; // stop at %
                else if (isspace((unsigned char)*arg_str))
                        break; // stop at space
                else
                        break; // stop on anything unexpected
                arg_str++;
        }

        buf[len] = '\0';

        if (len == 0)
                return 0;

        // Convert to integer
        return atoi(buf);
}

void handle_event(struct Event *event)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();
        PlayList *playlist = get_playlist();
        int chosen_row = get_chosen_row();

        switch (event->type) {
                break;
        case EVENT_ENQUEUE:
                view_enqueue(false);
                break;
        case EVENT_ENQUEUEANDPLAY:
                view_enqueue(true);
                break;
        case EVENT_PLAY_PAUSE:
                toggle_pause();
                break;
        case EVENT_TOGGLEVISUALIZER:
                toggle_visualizer();
                break;
        case EVENT_TOGGLEREPEAT:
                toggle_repeat();
                break;
        case EVENT_TOGGLEASCII:
                toggle_ascii();
                break;
        case EVENT_TOGGLENOTIFICATIONS:
                toggle_notifications();
                break;
        case EVENT_SHUFFLE:
                toggle_shuffle();
                break;
        case EVENT_SHOWLYRICSPAGE:
                toggle_show_lyrics_page();
                break;
        case EVENT_CYCLECOLORMODE:
                cycle_color_mode();
                break;
        case EVENT_CYCLETHEMES:
                cycle_themes();
                break;
        case EVENT_CYCLEVISUALIZATION:
                // FIXME: Enable Chroma
                // cycle_visualization();
                break;
        case EVENT_QUIT:
                quit();
                break;
        case EVENT_SCROLLDOWN:
                scroll_next();
                break;
        case EVENT_SCROLLUP:
                scroll_prev();
                break;
        case EVENT_VOLUME_UP:
                if (event->args[0] != '\0')
                        volume_change(parse_volume_arg(event->args));
                else
                        volume_change(5);
                emit_volume_changed();
                break;
        case EVENT_VOLUME_DOWN:
                if (event->args[0] != '\0')
                        volume_change(parse_volume_arg(event->args));
                else
                        volume_change(-5);
                emit_volume_changed();
                break;
        case EVENT_NEXT:
                skip_to_next_song();
                break;
        case EVENT_PREV:
                skip_to_prev_song();
                break;
        case EVENT_SEEKBACK:
                seek_back();
                break;
        case EVENT_SEEKFORWARD:
                seek_forward();
                break;
        case EVENT_ADDTOFAVORITESPLAYLIST:
                add_to_favorites_playlist();
                break;
        case EVENT_EXPORTPLAYLIST:
                export_current_playlist(settings->path, playlist);
                break;
        case EVENT_UPDATELIBRARY:
                free_search_results();
                set_error_message("Updating Library...");
                bool wait_until_complete = false;
                update_library(settings->path, wait_until_complete);
                break;
        case EVENT_SHOWHELP:
                toggle_show_view(HELP_VIEW);
                break;
        case EVENT_SHOWPLAYLIST:
                toggle_show_view(PLAYLIST_VIEW);
                break;
        case EVENT_SHOWSEARCH:
                toggle_show_view(SEARCH_VIEW);
                break;
                break;
        case EVENT_SHOWLIBRARY:
                toggle_show_view(LIBRARY_VIEW);
                break;
        case EVENT_NEXTPAGE:
                flip_next_page();
                break;
        case EVENT_PREVPAGE:
                flip_prev_page();
                break;
        case EVENT_REMOVE:
                handle_remove(get_chosen_row());
                reset_list_after_dequeuing_playing_song();
                break;
        case EVENT_SHOWTRACK:
                show_track();
                break;
        case EVENT_NEXTVIEW:
                switch_to_next_view();
                break;
        case EVENT_PREVVIEW:
                switch_to_previous_view();
                break;
        case EVENT_CLEARPLAYLIST:
                dequeue_all_except_playing_song();
                state->uiState.resetPlaylistDisplay = true;
                break;
        case EVENT_MOVESONGUP:
                move_song_up(&chosen_row);
                set_chosen_row(chosen_row);
                break;
        case EVENT_MOVESONGDOWN:
                move_song_down(&chosen_row);
                set_chosen_row(chosen_row);
                break;
        case EVENT_STOP:
                stop();
                break;
        case EVENT_SORTLIBRARY:
                sort_library();
                break;

        default:
                state->uiState.isFastForwarding = false;
                state->uiState.isRewinding = false;
                break;
        }
}

static gint64 last_scroll_event_time = 0;
static gint64 last_seek_event_time = 0;
static gint64 last_page_event_time = 0;
static gint64 last_switch_time = 0;

static gboolean should_throttle(struct Event *event)
{
        gint64 now = g_get_real_time(); // microseconds since Epoch
        gint64 delta;

        switch (event->type) {
        case EVENT_NEXT:
        case EVENT_PREV:
                delta = now - last_switch_time;
                if (delta < 400 * 1000) // 400ms
                        return TRUE;
                last_switch_time = now;
                break;
        case EVENT_SCROLLUP:
        case EVENT_SCROLLDOWN:
                delta = now - last_scroll_event_time;
                if (delta < 20 * 1000) // 20ms
                        return TRUE;
                last_scroll_event_time = now;
                break;

        case EVENT_SEEKBACK:
        case EVENT_SEEKFORWARD:
                delta = now - last_seek_event_time;
                if (delta < 20 * 1000)
                        return TRUE;
                last_seek_event_time = now;
                break;

        case EVENT_NEXTPAGE:
        case EVENT_PREVPAGE:
                delta = now - last_page_event_time;
                if (delta < 20 * 1000)
                        return TRUE;
                last_page_event_time = now;
                break;

        default:
                break;
        }

        return FALSE;
}

#define MAX_SEQ_LEN 1024 // Maximum length of sequence buffer
#define MAX_TMP_SEQ_LEN 256

enum EventType get_mouse_last_row_event(int mouse_x_on_last_row)
{
        enum EventType result = EVENT_NONE;
        AppState *state = get_app_state();

        const char *s = state->uiSettings.LAST_ROW;
        if (!s || mouse_x_on_last_row < 0)
                return EVENT_NONE;

        int view_clicked = 1; // Which section is clicked
        int col_index = 0;    // terminal column position
        const char *ptr = state->uiSettings.LAST_ROW;
        mbstate_t mbs;
        memset(&mbs, 0, sizeof(mbs));

        while (*ptr) {
                wchar_t wc;
                size_t bytes = mbrtowc(&wc, ptr, MB_CUR_MAX, &mbs);
                if (bytes == (size_t)-1 || bytes == (size_t)-2) {
                        bytes = 1;
                        wc = (unsigned char)*ptr;
                }

                int w = wcwidth(wc); // number of terminal columns this char takes
                if (w < 0)
                        w = 0;

                if (wc == L'|')
                        view_clicked++;

                if (col_index + w > mouse_x_on_last_row)
                        break; // cursor is inside this character

                col_index += w;
                ptr += bytes;
        }

        switch (view_clicked) {
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

        if (result == EVENT_SHOWTRACK && get_current_song_data() == NULL) {
                result = EVENT_SHOWLIBRARY;
        }

        return result;
}

bool handle_mouse_event(struct tb_event *ev, struct Event *event)
{
        if (ev->type != TB_EVENT_MOUSE)
                return false;

        int mouse_x = ev->x + 1;
        int mouse_y = ev->y + 1;
        uint16_t mouse_key = ev->key;
        ProgressBar *progress_bar = get_progress_bar();
        AppState *state = get_app_state();

        // Calculate where the user clicked on the progress bar
        if (progress_bar->length > 0) {
                long long delta_col = (long long)mouse_x - (long long)progress_bar->col;

                if (delta_col >= 0 && delta_col <= (long long)progress_bar->length) {
                        double position = (double)delta_col / (double)progress_bar->length;
                        double duration = get_current_song_duration();
                        dragged_position_seconds = duration * position;
                }
        }

        int footer_row = get_footer_row();
        int footer_col = get_footer_col();

        // Footer click (e.g., buttons or indicators on last row)
        if ((mouse_y == footer_row && footer_col > 0 &&
             mouse_x - footer_col >= 0 &&
             mouse_x - footer_col < (int)strlen(state->uiSettings.LAST_ROW)) &&
            mouse_key != TB_KEY_MOUSE_RELEASE) {
                event->type = get_mouse_last_row_event(mouse_x - footer_col + 1);
                return true;
        }

        if (mouse_key == TB_KEY_MOUSE_RELEASE) {
                // Mouse moved outside progress bar area → stop dragging
                dragging_progress_bar = false;
                return true;
        }

        // Progress bar click or hold-drag movement
        bool inProgressBar =
            (mouse_y == progress_bar->row) &&
            (mouse_x >= progress_bar->col) &&
            (mouse_x < progress_bar->col + progress_bar->length);

        if (inProgressBar && state->currentView == TRACK_VIEW) {
                // Any left press or movement within bar = update
                if (mouse_key == TB_KEY_MOUSE_LEFT || dragging_progress_bar) {
                        dragging_progress_bar = true;

                        gint64 newPosUs = (gint64)(dragged_position_seconds * G_USEC_PER_SEC);
                        set_position(newPosUs, get_current_song_duration());
                        return true;
                }
        }

        return false;
}

void handle_cooldown(void)
{
        bool cooldownElapsed = false;

        if (is_cooldown_elapsed(COOLDOWN_MS))
                cooldownElapsed = true;

        if (cooldownElapsed) {
                if (!dragging_progress_bar) {
                        if (flush_seek()) {
                                AppState *state = get_app_state();
                                state->uiState.isFastForwarding = false;
                                state->uiState.isRewinding = false;

                                if (state->currentView != TRACK_VIEW)
                                        trigger_refresh();
                        }
                }
        }
}

static gboolean on_tb_input(GIOChannel *source, GIOCondition cond, gpointer data)
{
        (void)source;
        (void)cond;
        (void)data;

        char seq[MAX_SEQ_LEN];
        int seq_length = 0;

        seq[0] = '\0';

        bool cooldown2Elapsed = false;

        if (is_cooldown_elapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        // Drain all available input
        while (1) {
                if (!is_input_available())
                        break;

                char tmp_seq[MAX_TMP_SEQ_LEN];
                int len = read_input_sequence(tmp_seq, sizeof(tmp_seq));
                if (len <= 0)
                        break;

                size_t seq_len = strnlen(seq, MAX_SEQ_LEN);
                size_t remaining_space = MAX_SEQ_LEN - seq_len - 1;
                if (remaining_space < (size_t)len)
                        len = remaining_space;

                memcpy(seq + seq_len, tmp_seq, len);
                seq[seq_len + len] = '\0';
                seq_length += len;
        }

        // If we got a sequence, convert to tb_event and handle
        if (seq_length > 0) {
                struct tb_event ev;
                memset(&ev, 0, sizeof(ev));

                // Feed the sequence into Termbox internal buffer
                if (seq_length > 0) {
                        bytebuf_nputs(&global.in, seq, seq_length); // feed entire sequence at once
                }

                struct Event event;
                event.type = EVENT_NONE;
                event.args[0] = '\0';
                event.key[0] = '\0';

                // Extract all events in the buffer
                while (tb_peek_event(&ev, 0) == 0) {
                        bool isMouseEvent = handle_mouse_event(&ev, &event);

                        if (!isMouseEvent) {
                                event = map_tb_key_to_event(&ev);
                        }

                        if (isdigit(ev.ch) && event.type == EVENT_NONE) {
                                if (digits_pressed_count < max_digits_pressed_count)
                                        digits_pressed[digits_pressed_count++] = ev.ch;
                        }

                        if (event.type != EVENT_NONE) {
                                // Throttle scroll/seek/page events
                                switch (event.type) {
                                case EVENT_SCROLLUP:
                                case EVENT_SCROLLDOWN:
                                case EVENT_SEEKBACK:
                                case EVENT_SEEKFORWARD:
                                case EVENT_NEXTPAGE:
                                case EVENT_PREVPAGE:
                                case EVENT_NEXT:
                                case EVENT_PREV:
                                        if (should_throttle(&event))
                                                continue;
                                        break;
                                default:
                                        break;
                                }

                                // Handle seek/remove cooldown
                                if (!cooldown2Elapsed &&
                                    (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
                                     event.type == EVENT_SEEKFORWARD))
                                        event.type = EVENT_NONE;
                                else if (event.type == EVENT_REMOVE || event.type == EVENT_SEEKBACK ||
                                         event.type == EVENT_SEEKFORWARD) {
                                        update_last_input_time();
                                }
                                // Forget Numbers
                                if (event.type != EVENT_ENQUEUE &&
                                    event.type != EVENT_GOTOENDOFPLAYLIST && event.type != EVENT_NONE) {
                                        memset(digits_pressed, '\0', sizeof(digits_pressed));
                                        digits_pressed_count = 0;
                                }

                                handle_event(&event);
                        }
                }
        }

        return TRUE;
}

void init_input(void)
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
        g_io_add_watch(chan, G_IO_IN, (GIOFunc)on_tb_input, NULL);
}

void shutdown_input(void)
{
        tb_shutdown();
}
