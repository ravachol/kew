#include "effects.h"

#include "common/appstate.h"
#include "common/common.h"

#include "messages.h"

#include "ui/chroma.h"
#include "ui/components.h"
#include "ui/control_ui.h"
#include "ui/queue_ui.h"
#include "ui/render_ui.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playlist_ops.h"
#include "ops/search_ops.h"
#include "ops/track_manager.h"

#include "sys/discord_rpc.h"
#include "sys/mpris.h"
#include "sys/sys_integration.h"

#include "utils/term.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h> /* ioctl */
#endif

// kew uses the Model-View-Update pattern.
//
// How Model-View-Update is supposed to work:
//
// Update:
//
// Pure state transitions
// Takes (Model, Msg) -> (Model, Cmds)
// Decides how the model changes
// May emit commands/effects
//
// Commands/Effects:
//
// Side effects
// IO
// Timers
// Network
// File access
// Audio playback
// Threading
// Anything non-deterministic or external

void ui_resize(Model *model)
{
        draw_buffer_resize(model->term_w, model->term_h);
        calc_indent(model);

        model->state.ui.num_progress_bars = model->term_w / 2 - model->indent;

        if (model->state.ui.chroma_started) {
                chroma_shutdown();
                model->state.ui.chroma_start_requested = true;
        }

        set_dirty(DIRTY_ALL);
}

static int get_terminal_size(TermSize *ws)
{
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

        if (h == INVALID_HANDLE_VALUE ||
            !GetConsoleScreenBufferInfo(h, &csbi)) {
                return -1;
        }

        ws->cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        ws->rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return 0;

#else
        struct winsize w;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
                return -1;

        ws->cols = w.ws_col;
        ws->rows = w.ws_row;
        return 0;
#endif
}

bool resize_if_needed(void)
{
        Model *model = get_model();
        UIState *uis = &model->state.ui;

        bool resized = false;
        uis->resizeFlag = 0;

        TermSize ws;

        // Get terminal size (platform independent)
        if (get_terminal_size(&ws) == 0) {

                // Fallback ONLY if invalid result
                if (ws.cols == 0 || ws.rows == 0) {
                        ws.cols = 80;
                        ws.rows = 24;
                }

                // Detect resize
                if (ws.cols != model->term_w ||
                    ws.rows != model->term_h) {

                        uis->resizeFlag = 1;
                        model->term_w = ws.cols;
                        model->term_h = ws.rows;
                }
        }

        if (uis->resizeFlag) {
                resize(uis);
                get_term_size(&model->term_w, &model->term_h);
                get_tty_size(&model->term_size);
                ui_resize(model);

                resized = true;
        }

        return resized;
}

void run_tick_commands(Model *model)
{
        if (model->state.ui.resumed_in_background)
                return;

        process_d_bus_events();

        calc_elapsed_time(model->song_duration);

        PlaybackState *ps = &model->playbackState;

        if (ps->notifyPlaying) {
                ps->notifyPlaying = false;

                emit_playback_playing();
        }

        if (ps->notifySwitch) {
                ps->notifySwitch = false;

                component_playlist_helper_update_view_state(model, false);

                if (model->songdata_ok && model->songdata) {
                        notify_mpris_switch(model->songdata);

                        if (model->state.settings.discordRPCEnabled)
                                notify_discord_update(model->songdata, model->elapsed_seconds, model->songdata->duration);
                }
        }

        if (ps->notifySeek) {
                ps->notifySeek = false;

                if (model->state.settings.discordRPCEnabled && model->songdata_ok && model->songdata)
                        notify_discord_update(model->songdata, model->elapsed_seconds, model->songdata->duration);
        }

        if (model->state.settings.discordRPCEnabled && model->songdata_ok && model->songdata) {
                if (is_paused() && !model->last_paused_state) {
                        notify_discord_pause();
                } else if (!is_paused() && model->last_paused_state) {
                        notify_discord_update(model->songdata, model->elapsed_seconds, model->songdata->duration);
                }

                model->last_paused_state = is_paused();
        }

        if (model->state.ui.chroma_started && model->state.last_view != model->state.currentView) {
                chroma_shutdown();
                model->state.ui.chroma_start_requested = true;
        }

        if (is_refresh_triggered() && model->state.settings.trackTitleAsWindowTitle) {

                if (model->songdata_ok) {
                          char title[METADATA_MAX_LENGTH + 100];
                          snprintf(title, sizeof(title), "kew - %s", model->songdata->metadata->title);
                          set_terminal_window_title(title);

                } else {
                        set_terminal_window_title("kew");
                }

                if (model->state.currentView != TRACK_VIEW && chroma_is_started())
                        chroma_shutdown();
        }
}

void run_command(UpdateResult result)
{
        Model *model = result.model;

        switch (result.cmd.type) {

        case CMD_TICK:
                run_tick_commands(model);
                break;

        case CMD_LOAD_WAITING_MUSIC:
                load_waiting_music();
                break;

        case CMD_RENDERED:
                mark_error_message_as_printed();
                break;

        case CMD_VIEW_ENQUEUE:
                view_enqueue(result.cmd.value);
                break;

        case CMD_PLAY:
                play();
                break;

        case CMD_PAUSE:
                pause_song();
                break;

        case CMD_TOGGLE_PAUSE:
                toggle_pause();
                break;

        case CMD_TOGGLE_REPEAT:
                toggle_repeat();
                break;

        case CMD_TOGGLE_ASCII:
                toggle_ascii();
                break;

        case CMD_TOGGLE_SHUFFLE:
                toggle_shuffle(model);
                break;

        case CMD_CYCLE_COLOR_MODE:
                cycle_color_mode();
                break;

        case CMD_CYCLE_THEMES:
                cycle_themes();
                break;

        case CMD_CYCLE_VISUALIZATION:
                if (model->state.ui.chroma_next_preset_requested)
                        chroma_set_next_preset();
                break;

        case CMD_QUIT:
                quit();
                break;

        case CMD_VOLUME_CHANGE:
                volume_change(result.cmd.value);
                emit_volume_changed();
                break;

        case CMD_NEXT:
                switch_to_next_song();
                model->songdata = get_current_song_data(model->songdata);
                component_playlist_helper_update_view_state(model, false);
                break;

        case CMD_PREV:
                switch_to_prev_song();
                model->songdata = get_current_song_data(model->songdata);

                component_playlist_helper_update_view_state(model, false);
                break;

        case CMD_SEEK_BACK:
                seek_back(model);
                break;

        case CMD_SEEK_FORWARD:
                seek_forward(model);
                break;

        case CMD_SEARCH:
                break;

        case CMD_ADD_TO_FAVORITES:
                add_to_favorites_playlist();
                break;

        case CMD_SAVEPLAYLIST: {
                set_save_playlist_mode();
                break;
        }

        case CMD_UPDATE_LIBRARY: {
                AppSettings *settings = get_app_settings();
                update_library(settings->path, false);
                break;
        }

        case CMD_REMOVE:
                handle_remove(model->state.ui.chosen_row);
                reset_list_after_dequeuing_playing_song();
                component_playlist_helper_update_view_state(model, false);
                model->songdata = get_current_song_data(model->songdata);
                break;

        case CMD_CLEAR_PLAYLIST: {
                clear_playlist();
                model->state.ui.resetPlaylistDisplay = true;
                component_playlist_helper_update_view_state(model, false);
                model->songdata = get_current_song_data(model->songdata);
                break;
        }

        case CMD_CROSSFADE: {
                int fade_ms = model->state.settings.fade_medium_ms;
                switch (result.cmd.value) {
                case FADE_QUICK:
                        fade_ms = model->state.settings.fade_quick_ms;
                        break;
                case FADE_SLOW:
                        fade_ms = model->state.settings.fade_slow_ms;
                        break;
                default:
                        fade_ms = model->state.settings.fade_medium_ms;
                        break;
                }
                if (!crossfade(fade_ms, model->state.settings.fade_enter_song_ms))
                        set_error_message("Crossfade disabled. The next song is not loaded yet or is of a different type.");

                break;
        }

        case CMD_TOGGLECROSSFADE:
                sound_system_set_always_crossfade(sound_sys, model->state.settings.always_crossfade, model->state.settings.fade_medium_ms);
                break;

        case CMD_MOVE_SONG_UP:
                move_song_up(&model->state.ui.chosen_row);
                break;

        case CMD_MOVE_SONG_DOWN:
                move_song_down(&model->state.ui.chosen_row);
                break;

        case CMD_STOP:
                stop();
                break;

        case CMD_SORT_LIBRARY:
                sort_library();
                break;

        case CMD_VIEW_CHANGED:
                if (model->state.ui.chroma_started) {
                        chroma_shutdown();
                        model->state.ui.chroma_start_requested = true;
                }
                break;

        case CMD_NONE:
        default:
                break;
        }
}
