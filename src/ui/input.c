#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

/**
 * @file input.c
 * @brief Handles keyboard and terminal input events.
 */

#include "input.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"
#include "common/model.h"

#include "update/messages.h"

#define TB_IMPL
#include "termbox2_input.h"

#include "common_ui.h"
#include "control_ui.h"
#include "queue_ui.h"
#include "render_ui.h"
#include "settings.h"

#include "ops/playback_clock.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"
#include "ops/search_ops.h"
#include "ops/track_manager.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <ctype.h>
#include <gio/gio.h>
#include <glib.h>
#include <wchar.h> // Needed for netbsd

#define MAX_TMP_SEQ_LEN 256
#define NUM_KEY_MAPPINGS 69

const int COOLDOWN_MS = 500;
const int COOLDOWN2_MS = 100;
const int COOLDOWNDIGITS_MS = 1500;
const int MOUSE_DRAG = 32;
const int MOUSE_CLICK = 0;
const int max_digits_pressed_count = 9;
const int fuzzy_search_threshold = 100;

EventMapping key_mappings[NUM_KEY_MAPPINGS];
struct timespec last_input_time;
char digits_pressed[MAX_SEQ_LEN];
int digits_pressed_count;
double dragged_position_seconds = 0.0;
bool dragging_progressbar = false;
bool dragging_scrollbar = false;

struct Msg map_tb_key_to_event(struct tb_event *ev)
{
        struct Msg event = {0};
        event.type = MSG_NONE;

        TBKeyBinding *key_bindings = get_key_bindings();

        for (size_t i = 0; i < keybinding_count; i++) {
                TBKeyBinding *b = &key_bindings[i];

                if (isupper((unsigned char)ev->ch)) {
                        ev->mod |= TB_MOD_SHIFT;
                        ev->ch = tolower(ev->ch);
                }

                bool keyMatch = (b->key && ev->key == b->key) || (b->ch && ev->ch == b->ch);
                bool modsMatch = (b->mods == ev->mod);

                if (keyMatch && modsMatch) {
                        event.type = b->eventType;
                        c_strcpy(event.args, b->args, sizeof(event.args));
                        event.args[sizeof(event.args) - 1] = '\0';
                        break;
                }
        }

        return event;
}

struct Msg handle_name_playlist_event(struct tb_event *ev)
{
        Model *model = get_model();
        struct Msg event = {0};
        event.type = MSG_NONE;

        if (model->state.ui.naming_playlist) {

                if (ev->key == TB_KEY_SPACE && ev->mod == 0)
                        ev->ch = ' ';

                // Backspace
                if (ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2) {
                        remove_from_playlist_name();
                        event.type = MSG_NAMING_PLAYLIST;
                }
                // Printable character (not escape, enter, tab, carriage return)
#if defined(__ANDROID__) || defined(__APPLE__)
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        if (ev->ch != 'Z' && ev->ch != 'X' && ev->ch != 'C' && ev->ch != 'V' && ev->ch != 'B' && ev->ch != 'N') {
                                char keybuf[5] = {0};
                                tb_utf8_unicode_to_char(keybuf, ev->ch);
                                add_to_playlist_name(keybuf);
                                event.type = MSG_NAMING_PLAYLIST;
                        }
                }
#else
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        char keybuf[5] = {0};
                        tb_utf8_unicode_to_char(keybuf, ev->ch);
                        add_to_playlist_name(keybuf);
                        event.type = MSG_NAMING_PLAYLIST;
                }
#endif
                if (ev->key == TB_KEY_ENTER) {
                        playlist_save();
                        event.type = MSG_NAMING_PLAYLIST;
                }
        }

        return event;
}

struct Msg handle_search_event(struct tb_event *ev, Model *model)
{
        struct Msg event = {0};
        event.type = MSG_NONE;

        AppState *state = get_app_state();

        if (state->currentView == SEARCH_VIEW) {

                if (ev->key == TB_KEY_SPACE && ev->mod == 0)
                        ev->ch = ' ';

                // Backspace
                if (ev->key == TB_KEY_BACKSPACE || ev->key == TB_KEY_BACKSPACE2) {
                        remove_from_search_text(model);
                        reset_search_result(model);
                        fuzzy_search(model->state.ui.search_text, get_library(), fuzzy_search_threshold);
                        event.type = MSG_SEARCH;
                }
                // Printable character (not escape, enter, tab, carriage return)
#if defined(__ANDROID__) || defined(__APPLE__)
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        if (ev->ch != 'Z' && ev->ch != 'X' && ev->ch != 'C' && ev->ch != 'V' && ev->ch != 'B' && ev->ch != 'N') {
                                char keybuf[5] = {0};
                                tb_utf8_unicode_to_char(keybuf, ev->ch);
                                add_to_search_text(model, keybuf);
                                reset_search_result(model);
                                fuzzy_search(model->state.ui.search_text, get_library(), fuzzy_search_threshold);
                                event.type = MSG_SEARCH;
                        }
                }
#else
                else if ((ev->ch > 0 && ev->ch != '\033' && ev->ch != '\n' && ev->ch != '\t' && ev->ch != '\r') ||
                         ev->ch == ' ') {
                        char keybuf[5] = {0};
                        tb_utf8_unicode_to_char(keybuf, ev->ch);
                        add_to_search_text(model, keybuf);
                        reset_search_result(model);
                        fuzzy_search(model->state.ui.search_text, model->library, fuzzy_search_threshold);
                        event.type = MSG_SEARCH;
                }
#endif
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

void key_mappings_init(AppSettings *settings)
{
        map_settings_to_keys(settings, key_mappings);
}

static gint64 last_scroll_event_time = 0;
static gint64 last_seek_event_time = 0;
static gint64 last_page_event_time = 0;
static gint64 last_switch_time = 0;

static gboolean should_throttle(struct Msg *msg)
{
        gint64 now = g_get_real_time(); // microseconds since Epoch
        gint64 delta;

        switch (msg->type) {
        case MSG_NEXT:
        case MSG_PREV:
                delta = now - last_switch_time;
                if (delta < 400 * 1000) // 400ms
                        return TRUE;
                last_switch_time = now;
                break;
        case MSG_SCROLLUP:
        case MSG_SCROLLDOWN:
                delta = now - last_scroll_event_time;
                if (delta < 20 * 1000) // 20ms
                        return TRUE;
                last_scroll_event_time = now;
                break;

        case MSG_SEEKBACK:
        case MSG_SEEKFORWARD:
                delta = now - last_seek_event_time;
                if (delta < 20 * 1000)
                        return TRUE;
                last_seek_event_time = now;
                break;

        case MSG_NEXT_PAGE:
        case MSG_PREV_PAGE:
                delta = now - last_page_event_time;
                if (delta < 20 * 1000)
                        return TRUE;
                last_page_event_time = now;
                break;

        case MSG_ENQUEUE:
        case MSG_ENQUEUEANDPLAY:
                delta = now - last_page_event_time;
                if (delta < 100 * 1000)
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

enum MsgType get_mouse_last_row_event(int mouse_x_on_last_row, const char *footer_text)
{
        enum MsgType result = MSG_NONE;

        if (!footer_text || mouse_x_on_last_row < 0)
                return MSG_NONE;

        int view_clicked = 1; // Which section is clicked
        int col_index = 0;    // terminal column position
        mbstate_t mbs;
        memset(&mbs, 0, sizeof(mbs));

        while (*footer_text) {
                wchar_t wc;
                size_t bytes = mbrtowc(&wc, footer_text, MB_CUR_MAX, &mbs);
                if (bytes == (size_t)-1 || bytes == (size_t)-2) {
                        bytes = 1;
                        wc = (unsigned char)*footer_text;
                }

                int w = mk_wcwidth(wc); // number of terminal columns this char takes
                if (w < 0)
                        w = 0;

                if (col_index + w > mouse_x_on_last_row)
                        break; // cursor is inside this character

                if (wc == L'|')
                        view_clicked++;

                col_index += w;
                footer_text += bytes;
        }

        switch (view_clicked) {
        case 1:
                result = MSG_SHOWPLAYLIST;
                break;
        case 2:
                result = MSG_SHOWLIBRARY;
                break;
        case 3:
                result = MSG_SHOWTRACK;
                break;
        case 4:
                result = MSG_SHOWSEARCH;
                break;
        case 5:
                result = MSG_SHOWHELP;
                break;
        default:
                result = MSG_NONE;
                break;
        }

        Model *model = get_model();
        if (result == MSG_SHOWTRACK && model->songdata == NULL) {
                result = MSG_SHOWLIBRARY;
        }

        return result;
}

enum MsgType get_mouse_minicontrols_event(int mouse_x_on_text, const char *text)
{
        enum MsgType result = MSG_NONE;

        if (!text || mouse_x_on_text < 0)
                return MSG_NONE;

        int view_clicked = 0; // Which section is clicked
        int col_index = 0;    // terminal column position
        bool is_space = false;
        bool count_next = false;
        mbstate_t mbs;
        memset(&mbs, 0, sizeof(mbs));

        while (*text) {
                wchar_t wc;
                size_t bytes = mbrtowc(&wc, text, MB_CUR_MAX, &mbs);
                if (bytes == (size_t)-1 || bytes == (size_t)-2) {
                        bytes = 1;
                        wc = (unsigned char)*text;
                }

                int w = mk_wcwidth(wc); // number of terminal columns this char takes
                if (w < 0)
                        w = 0;

                if (col_index + w > mouse_x_on_text)
                        break; // cursor is inside this character

                if (count_next)
                {
                        is_space = false;
                        count_next = false;
                } else if (wc != L' ')
                {
                        view_clicked++;
                        is_space = false;
                }
                else
                        is_space = true;

                if (wc == L'▶') // occupies two cells
                        count_next = true;

                col_index += w;
                text += bytes;
        }

        // "⏮  ▶  ⏭  ∅"

        if (is_space)
                return MSG_NONE;

        switch (view_clicked) {
        case 1:
                result = MSG_PREV;
                break;
        case 2:
                result = MSG_PLAY_PAUSE;
                break;
        case 3:
                result = MSG_NEXT;
                break;
        case 4:
                result = MSG_CLEARPLAYLIST;
                break;
        default:
                result = MSG_NONE;
                break;
        }

        if (result != MSG_NONE)
                set_dirty(DIRTY_SONG);

        Model *model = get_model();
        if (result == MSG_PLAY_PAUSE && model->songdata == NULL) {
                result = MSG_PLAY;
        }

        return result;
}

int get_footer_row(void)
{
        Model *model = get_model();
        return model->state.ui.footer_row;
}

int get_footer_col(void)
{
        Model *model = get_model();
        return model->state.ui.footer_col;
}

#include <stdlib.h>

void open_url(const char *url)
{
#ifdef _WIN32
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", url);
        system(cmd);
#elif __APPLE__
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "open \"%s\"", url);
        system(cmd);
#else
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "xdg-open \"%s\"", url);
        int result = system(cmd);
        (void)result; // remove warning in editor
#endif
}

bool handle_mouse_event(struct tb_event *ev, struct Msg *event)
{
        if (ev->type != TB_EVENT_MOUSE)
                return false;

        Model *model = get_model();
        int mouse_x = ev->x + 1;
        int mouse_y = ev->y + 1;
        uint16_t mouse_key = ev->key;
        ProgressBar *progress_bar = &model->progressBar;

        // Calculate where the user clicked on the progress bar
        if (progress_bar->length > 0) {
                long long delta_col = (long long)mouse_x - (long long)progress_bar->col;

                if (delta_col >= 0 && delta_col <= (long long)progress_bar->length) {
                        double position = (double)delta_col / (double)progress_bar->length;
                        double duration = model->song_duration;

                        dragged_position_seconds = duration * position;
                }
        }

        int footer_row = get_footer_row();
        int footer_col = get_footer_col();

        char footer_text[100];
        get_footer_text(footer_text, sizeof(footer_text));

        // Footer click (e.g., buttons or indicators on last row)
        if ((mouse_y == footer_row && footer_col > 0 &&
             mouse_x - footer_col >= 0 &&
             mouse_x - footer_col < (int)strlen(footer_text)) &&
            mouse_key != TB_KEY_MOUSE_RELEASE) {
                event->type = get_mouse_last_row_event(mouse_x - footer_col + 1, footer_text);
                return true;
        }

        char minicontrols_text[100];
        get_minicontrols_text(minicontrols_text, sizeof(minicontrols_text));

        // MiniControls click
        if ((mouse_y == model->miniControls.row && model->miniControls.row > 0 &&
             mouse_x - model->miniControls.col >= 0 &&
             mouse_x - model->miniControls.col < (int)strlen(minicontrols_text)) &&
            mouse_key != TB_KEY_MOUSE_RELEASE) {
                event->type = get_mouse_minicontrols_event(mouse_x - model->miniControls.col + 1, minicontrols_text);
                return true;
        }

        if (!model->state.ui.link_clicked) {
                char *link = url_at_pos(mouse_y - 1, mouse_x - 1);

                if (link) {
                        open_url(link);

                        Model *model = get_model();
                        model->state.ui.link_clicked = true;
                }
        }

        if (mouse_key == TB_KEY_MOUSE_RELEASE) {
                // Mouse moved outside progress bar area → stop dragging
                dragging_progressbar = false;
                dragging_scrollbar = false;
                return true;
        }

        // Progress bar click or hold-drag movement
        bool inProgressBar =
            (mouse_y == progress_bar->row) &&
            (mouse_x >= progress_bar->col) &&
            (mouse_x < progress_bar->col + progress_bar->length);

        if (inProgressBar) {
                // Any left press or movement within bar = update
                if (mouse_key == TB_KEY_MOUSE_LEFT || dragging_progressbar) {
                        dragging_progressbar = true;

                        gint64 newPosUs = (gint64)(dragged_position_seconds * G_USEC_PER_SEC);
                        set_position(newPosUs);

                        event->type = MSG_SEEK;

                        return true;
                }
        }

        bool inScrollBar = scrollbar_at_position(mouse_x, mouse_y, dragging_scrollbar);

        if (inScrollBar) {
                if (mouse_key == TB_KEY_MOUSE_LEFT || dragging_scrollbar) {

                        scrollbar_scroll(mouse_y, dragging_scrollbar);

                        dragging_scrollbar = true;

                        return true;
                }
        } else if (mouse_key == TB_KEY_MOUSE_LEFT || mouse_key == TB_KEY_MOUSE_MIDDLE) {
                register_click(mouse_x, mouse_y, mouse_key);
                return true;
        }

        return false;
}

void handle_cooldown(void)
{
        bool cooldownElapsed = false;
        bool cooldownDigitsElapsed = false;

        if (is_cooldown_elapsed(COOLDOWN_MS))
                cooldownElapsed = true;

        if (is_cooldown_elapsed(COOLDOWNDIGITS_MS))
                cooldownDigitsElapsed = true;

        if (digits_pressed_count > 0 && cooldownDigitsElapsed) {
                struct Msg msg;

                msg.args[0] = '\0';
                msg.key[0] = '\0';
                msg.type = MSG_ENQUEUEANDPLAY;

                dispatch_msg(msg);

                reset_digits_pressed();
        }

        if (cooldownElapsed) {
                if (!dragging_progressbar) {
                        if (flush_seek()) {
                                AppState *state = get_app_state();
                                state->ui.isFastForwarding = false;
                                state->ui.isRewinding = false;

                                if (state->currentView != TRACK_VIEW) {
                                        set_dirty(DIRTY_FOOTER);
                                }
                        }
                }

                Model *model = get_model();
                model->state.ui.link_clicked = false;
        }
}

int read_input(char *seq, size_t seq_size)
{
        if (!seq || seq_size < 2)
                return 0;

        if (global.in.len == 0)
                return 0;

        int len = MIN(global.in.len, seq_size - 1);

        memcpy(seq, global.in.buf, len);

        memmove(global.in.buf,
                global.in.buf + len,
                global.in.len - len);

        global.in.len -= len;

        seq[len] = '\0';

        return len;
}

#ifdef _WIN32

// FIXME this should be one function that works on all platforms.
// Duplicating this is a temporary measure (hopefully).
// Btw, this function is the worst offender in the entire codebase.
// It's a rest from the input handling before we hade decent things like termbox2.
// It's only here because some things like multiple keypresses (100 for track 100) and some keys weren't working properly
// through vanilla termbox2. But it should be redone.

static gboolean on_tb_input(GIOChannel *source, GIOCondition cond, gpointer data)
{
        (void)source;
        (void)cond;
        (void)data;

        bool cooldown2Elapsed = false;
        int seq_length = 0;
        char seq[MAX_SEQ_LEN];

        if (is_cooldown_elapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        seq[0] = '\0';
        Model *model = get_model();
        // Drain all available input
        while (1) {
                if (model->state.ui.resumed_in_background)
                        return FALSE;

                if (global.in.len <= 0)
                        break;

                char tmp_seq[MAX_TMP_SEQ_LEN];
                int len = read_input(tmp_seq, sizeof(tmp_seq));
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

                struct tb_event last_ev;
                memset(&last_ev, 0, sizeof(last_ev));
                bool found_event = false;

                struct Msg msg;
                msg.type = MSG_NONE;
                msg.args[0] = '\0';
                msg.key[0] = '\0';

                AppState *state = get_app_state();
                if (state->ui.naming_playlist) {
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (!isMouseEvent) {
                                        msg = handle_name_playlist_event(&ev);
                                        if (msg.type == MSG_NONE)
                                                msg = map_tb_key_to_event(&ev);
                                }
                        }

                } else if (state->currentView == SEARCH_VIEW) {
                        // Process all characters in the buffer (e.g. IME commits
                        // multiple characters at once)
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (!isMouseEvent) {
                                        msg = handle_search_event(&ev, get_model());
                                        if (msg.type == MSG_NONE)
                                                msg = map_tb_key_to_event(&ev);
                                }
                        }
                } else {

                        // Extract all events in the buffer
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (isMouseEvent || map_tb_key_to_event(&ev).type != MSG_NONE) {
                                        last_ev = ev;
                                        found_event = true;
                                }

                                if (msg.type == MSG_SEEK) {
                                        return FALSE;
                                }
                        }

                        if (!found_event)
                                return FALSE;

                        // Process only the last event
                        bool isMouseEvent = handle_mouse_event(&last_ev, &msg);
                        if (!isMouseEvent) {

                                msg = map_tb_key_to_event(&last_ev);
                        }
                }

                if (isdigit(ev.ch) && msg.type == MSG_NONE) {

                        if (digits_pressed_count == 0) {
                                update_last_input_time();
                        }

                        if (digits_pressed_count < max_digits_pressed_count)
                                digits_pressed[digits_pressed_count++] = ev.ch;
                }

                if (msg.type != MSG_NONE) {
                        // Throttle scroll/seek/page events
                        switch (msg.type) {
                        case MSG_SCROLLUP:
                        case MSG_SCROLLDOWN:
                        case MSG_SEEKBACK:
                        case MSG_SEEKFORWARD:
                        case MSG_NEXT_PAGE:
                        case MSG_PREV_PAGE:
                        case MSG_NEXT:
                        case MSG_PREV:
                        case MSG_ENQUEUE:
                        case MSG_ENQUEUEANDPLAY:
                                if (should_throttle(&msg))
                                        return FALSE;
                                break;
                        default:
                                break;
                        }

                        // Handle seek/remove cooldown
                        if (!cooldown2Elapsed &&
                            (msg.type == MSG_REMOVE || msg.type == MSG_SEEKBACK ||
                             msg.type == MSG_SEEKFORWARD))
                                msg.type = MSG_NONE;
                        else if (msg.type == MSG_REMOVE || msg.type == MSG_SEEKBACK ||
                                 msg.type == MSG_SEEKFORWARD) {
                                update_last_input_time();
                        }

                        // Forget Numbers
                        if (msg.type != MSG_ENQUEUE &&
                            msg.type != MSG_GOTOENDOFPLAYLIST && msg.type != MSG_NONE) {
                                reset_digits_pressed();
                        }

                        dispatch_msg(msg);

                        clear_error_message(); // Error messages are cleared when the user does something

                        return FALSE;
                }
        }

        return FALSE;
}

#else

static gboolean on_tb_input(GIOChannel *source, GIOCondition cond, gpointer data)
{
        (void)source;
        (void)cond;
        (void)data;

        char seq[MAX_SEQ_LEN];
        int seq_length = 0;

        seq[0] = '\0';

        bool cooldown2Elapsed = false;

        Model *model = get_model();
        bool old_dragging = dragging_scrollbar;

        if (is_cooldown_elapsed(COOLDOWN2_MS))
                cooldown2Elapsed = true;

        // Drain all available input
        while (1) {
                if (model->state.ui.resumed_in_background)
                        return FALSE;

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

                struct tb_event last_ev;
                memset(&last_ev, 0, sizeof(last_ev));
                bool found_event = false;

                struct Msg msg;
                msg.type = MSG_NONE;
                msg.args[0] = '\0';
                msg.key[0] = '\0';

                AppState *state = get_app_state();
                if (state->ui.naming_playlist) {
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (!isMouseEvent) {
                                        msg = handle_name_playlist_event(&ev);
                                        if (msg.type == MSG_NONE)
                                                msg = map_tb_key_to_event(&ev);
                                }
                        }

                } else if (state->currentView == SEARCH_VIEW) {
                        // Process all characters in the buffer (e.g. IME commits
                        // multiple characters at once)
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (!isMouseEvent) {
                                        msg = handle_search_event(&ev, get_model());
                                        if (msg.type == MSG_NONE)
                                                msg = map_tb_key_to_event(&ev);
                                }
                        }
                } else {

                        // Extract all events in the buffer
                        while (tb_peek_event(&ev, 0) == 0) {
                                bool isMouseEvent = handle_mouse_event(&ev, &msg);
                                if (isMouseEvent || map_tb_key_to_event(&ev).type != MSG_NONE) {
                                        last_ev = ev;
                                        found_event = true;
                                }

                                if (msg.type == MSG_SEEK) {
                                        return TRUE;
                                }
                        }

                        if (!found_event)
                                return TRUE;

                        // Process only the last event
                        dragging_scrollbar = old_dragging;
                        bool isMouseEvent = handle_mouse_event(&last_ev, &msg);
                        if (!isMouseEvent) {

                                msg = map_tb_key_to_event(&last_ev);
                        }
                }

                if (isdigit(ev.ch) && msg.type == MSG_NONE) {

                        if (digits_pressed_count == 0) {
                                update_last_input_time();
                        }

                        if (digits_pressed_count < max_digits_pressed_count)
                                digits_pressed[digits_pressed_count++] = ev.ch;
                }

                if (msg.type != MSG_NONE) {
                        // Throttle scroll/seek/page events
                        switch (msg.type) {
                        case MSG_SCROLLUP:
                        case MSG_SCROLLDOWN:
                        case MSG_SEEKBACK:
                        case MSG_SEEKFORWARD:
                        case MSG_NEXT_PAGE:
                        case MSG_PREV_PAGE:
                        case MSG_NEXT:
                        case MSG_PREV:
                        case MSG_ENQUEUE:
                        case MSG_ENQUEUEANDPLAY:
                                if (should_throttle(&msg))
                                        return TRUE;
                                break;
                        default:
                                break;
                        }

                        // Handle seek/remove cooldown
                        if (!cooldown2Elapsed &&
                            (msg.type == MSG_REMOVE || msg.type == MSG_SEEKBACK ||
                             msg.type == MSG_SEEKFORWARD))
                                msg.type = MSG_NONE;
                        else if (msg.type == MSG_REMOVE || msg.type == MSG_SEEKBACK ||
                                 msg.type == MSG_SEEKFORWARD) {
                                update_last_input_time();
                        }

                        // Forget Numbers
                        if (msg.type != MSG_ENQUEUE &&
                            msg.type != MSG_GOTOENDOFPLAYLIST && msg.type != MSG_NONE) {
                                reset_digits_pressed();
                        }

                        dispatch_msg(msg);

                        clear_error_message(); // Error messages are cleared when the user does something

                        return TRUE;
                }
        }

        return TRUE;
}

#endif

#ifdef _WIN32

static volatile int idle_pending = 0;

static gboolean on_tb_input_idle(gpointer data)
{
        on_tb_input(NULL, 0, data);
        g_atomic_int_set(&idle_pending, 0);

        return FALSE;
}

static DWORD WINAPI win_input_thread(void *arg)
{
        (void)arg;

        DWORD prev_buttons = 0;

        while (1) {

                INPUT_RECORD rec;
                DWORD r;

                if (!ReadConsoleInputA(global.hin, &rec, 1, &r))
                        continue;

                switch (rec.EventType) {

                case MOUSE_EVENT: {
                        MOUSE_EVENT_RECORD *m = &rec.Event.MouseEvent;

                        DWORD buttons = m->dwButtonState;
                        DWORD changed = buttons ^ prev_buttons;

                        DWORD f = m->dwEventFlags;

                        // Ignore pure movement with no buttons
                        if (f == MOUSE_MOVED && buttons == 0)
                                break;

                        COORD p = m->dwMousePosition;

                        int x = p.X + 1;
                        int y = p.Y + 1;

                        int b = 0;
                        int len = 0;
                        char buf[64];

                        // Left
                        if (changed & FROM_LEFT_1ST_BUTTON_PRESSED) {
                                b = 0;
                                if (buttons & FROM_LEFT_1ST_BUTTON_PRESSED) {
                                        // press
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dM", b, x, y);
                                } else {
                                        // release
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dm", b, x, y);
                                }
                        }

                        // Right
                        else if (changed & RIGHTMOST_BUTTON_PRESSED) {
                                b = 2;
                                if (buttons & RIGHTMOST_BUTTON_PRESSED) {
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dM", b, x, y);
                                } else {
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dm", b, x, y);
                                }
                        }

                        // Middle
                        else if (changed & FROM_LEFT_2ND_BUTTON_PRESSED) {
                                b = 1;
                                if (buttons & FROM_LEFT_2ND_BUTTON_PRESSED) {
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dM", b, x, y);
                                } else {
                                        len = snprintf(buf, sizeof(buf),
                                                       "\x1b[<%d;%d;%dm", b, x, y);
                                }
                        }

                        // Drag
                        else if (f == MOUSE_MOVED && buttons) {
                                if (buttons & FROM_LEFT_1ST_BUTTON_PRESSED)
                                        b = 0;
                                else if (buttons & RIGHTMOST_BUTTON_PRESSED)
                                        b = 2;
                                else if (buttons & FROM_LEFT_2ND_BUTTON_PRESSED)
                                        b = 1;

                                len = snprintf(buf, sizeof(buf),
                                               "\x1b[<%d;%d;%dM", b, x, y);
                        }

                        // Wheel
                        else if (f == MOUSE_WHEELED) {
                                short delta = (short)HIWORD(m->dwButtonState);

                                int wb = (delta > 0) ? 64 : 65;

                                len = snprintf(buf, sizeof(buf),
                                               "\x1b[<%d;%d;%dM", wb, x, y);
                        }

                        prev_buttons = buttons;

                        if (len > 0) {
                                bytebuf_nputs(&global.in, buf, len);

                                if (g_atomic_int_compare_and_exchange(&idle_pending, 0, 1))
                                        g_idle_add(on_tb_input_idle, NULL);
                        }

                        break;
                }

                case WINDOW_BUFFER_SIZE_EVENT: {
                        break;
                }

                case KEY_EVENT: {

                        if (!rec.Event.KeyEvent.bKeyDown)
                                continue;

                        char buf[64];

                        int len = encode_win_key_to_ansi(
                            &rec.Event.KeyEvent,
                            buf,
                            sizeof(buf));

                        if (len > 0) {
                                bytebuf_nputs(&global.in, buf, len);

                                if (g_atomic_int_compare_and_exchange(&idle_pending, 0, 1))
                                        g_idle_add(on_tb_input_idle, NULL);
                        }

                        break;
                }

                default:
                        break;
                }
        }

        return 0;
}
#endif

void input_init(void)
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

#ifdef _WIN32

        void restore_console(void)
        {
                if (global.hin)
                        SetConsoleMode(global.hin, global.original_mode);
        }

        BOOL WINAPI ctrl_handler(DWORD type)
        {
                switch (type) {
                case CTRL_C_EVENT:
                case CTRL_BREAK_EVENT:
                case CTRL_CLOSE_EVENT:
                case CTRL_SHUTDOWN_EVENT:
                        restore_console();
                        return FALSE; // Let process exit normally
                }
                return FALSE;
        }

        DWORD mode;
        GetConsoleMode(global.hin, &mode);

        global.original_mode = mode;

        mode &= ~ENABLE_QUICK_EDIT_MODE; // Disable QuickEdit
        mode |= ENABLE_EXTENDED_FLAGS;   // Required
        mode |= ENABLE_MOUSE_INPUT;
        mode |= ENABLE_WINDOW_INPUT;

        SetConsoleMode(global.hin, mode);
        SetConsoleCtrlHandler(ctrl_handler, TRUE);
        CreateThread(NULL, 0, win_input_thread, NULL, 0, NULL);

#else
        g_io_channel_set_encoding(chan, NULL, NULL);
        g_io_add_watch(chan, G_IO_IN, on_tb_input, NULL);
#endif
}

void input_shutdown(void)
{
#ifdef _WIN32
        SetConsoleMode(global.hin, global.original_mode);
#endif

        tb_shutdown();
}
