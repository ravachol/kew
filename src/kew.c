/* kew - Music For The Shell
Copyright (C) 2022 Ravachol

http://codeberg.org/ravachol/kew

$$\
$$ |
$$ |  $$\  $$$$$$\  $$\  $$\  $$\
$$ | $$  |$$  __$$\ $$ | $$ | $$ |
$$$$$$  / $$$$$$$$ |$$ | $$ | $$ |
$$  _$$<  $$   ____|$$ | $$ | $$ |
$$ | \$$\ \$$$$$$$\ \$$$$$\$$$$  |
\__|  \__| \_______| \_____\____/

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#ifdef __FreeBSD__
#define __BSD_VISIBLE 1
#endif

#include "common/appstate.h"
#include "common/common.h"

#include "sys/mpris.h"
#include "sys/notifications.h"
#include "sys/sys_integration.h"

#include "ui/chroma.h"
#include "ui/cli.h"
#include "ui/common_ui.h"
#include "ui/control_ui.h"
#include "ui/input.h"
#include "ui/player_ui.h"
#include "ui/playlist_ui.h"
#include "ui/queue_ui.h"
#include "ui/search_ui.h"
#include "ui/settings.h"
#include "ui/termbox2_input.h"
#include "ui/visuals.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/track_manager.h"

#include "utils/cache.h"
#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <fcntl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <glib.h>
#include <libintl.h>
#include <locale.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

const char VERSION[] = "3.7.0";

AppState *state_ptr = NULL;

void update_player(void)
{
        AppState *state = get_app_state();
        UIState *uis = &(state->uiState);
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

        if (ws.ws_col != state->uiState.windowSize.ws_col || ws.ws_row != state->uiState.windowSize.ws_row) {
                uis->resizeFlag = 1;
                state->uiState.windowSize = ws;
        }

        if (uis->resizeFlag) {
                resize(uis);
        } else {
                refresh_player();
        }
}

void prepare_next_song(void)
{
        AppState *state = get_app_state();

        reset_clock();
        handle_skip_out_of_order();
        finish_loading();

        set_next_song(NULL);
        trigger_refresh();

        Node *current = get_current_song();

        if (!is_repeat_enabled() || current == NULL) {
                unload_previous_song();
        }

        if (current == NULL) {
                if (state->uiSettings.quitAfterStopping) {
                        quit();
                } else {
                        set_end_of_list_reached();
                }
        } else {
                determine_song_and_notify();
        }
}

int prepare_and_play_song(Node *song)
{
        if (!song)
                return -1;

        set_current_song(song);

        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        stop();

        pthread_mutex_lock(&(ps->loadingdata.mutex));
        pthread_mutex_lock(&(state->data_source_mutex));

        unload_song_a();
        unload_song_b();

        pthread_mutex_unlock(&(ps->loadingdata.mutex));
        pthread_mutex_unlock(&(state->data_source_mutex));

        int res = load_first(get_current_song());

        finish_loading();

        if (res >= 0) {
                res = create_playback_device();
        }

        if (res >= 0) {
                resume_playback();
        }

        if (res < 0)
                set_end_of_list_reached();

        trigger_refresh();
        reset_clock();

        return res;
}

void check_and_load_next_song(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        if (audio_data.restart) {
                PlayList *playlist = get_playlist();
                if (playlist->head == NULL)
                        return;

                if (ps->waitingForPlaylist || ps->waitingForNext) {
                        ps->songLoading = true;

                        Node *next_song = determine_next_song(playlist);
                        if (!next_song)
                                return;

                        audio_data.restart = false;
                        ps->waitingForPlaylist = false;
                        ps->waitingForNext = false;
                        state->uiState.songWasRemoved = false;

                        if (is_shuffle_enabled())
                                reshuffle_playlist();

                        int res = prepare_and_play_song(next_song);

                        if (res < 0)
                                set_end_of_list_reached();

                        ps->loadedNextSong = false;
                        set_next_song(NULL);
                }
        } else if (get_current_song() != NULL &&
                   (ps->nextSongNeedsRebuilding || get_next_song() == NULL) &&
                   !ps->songLoading) {
                update_next_song_if_needed();
        }
}

void load_waiting_music(void)
{
        PlayList *playlist = get_playlist();
        AudioData *audio_data = get_audio_data();
        PlaybackState *ps = get_playback_state();

        if (playlist->head != NULL) {
                if ((ps->skipFromStopped || !ps->loadedNextSong ||
                     ps->nextSongNeedsRebuilding) &&
                    !audio_data->end_of_list_reached) {
                        check_and_load_next_song();
                }

                if (ps->songHasErrors)
                        try_load_next();

                if (is_EOF_reached()) {
                        prepare_next_song();
                        switch_audio_implementation();
                }
        } else {
                set_EOF_handled();
        }
}

gboolean mainloop_callback(gpointer data)
{
        (void)data;

        calc_elapsed_time(get_current_song_duration());
        increment_update_counter();
        handle_cooldown();

        int update_counter = get_update_counter();

        // Different views run at different speeds to lower the impact on system
        // requirements
        if ((update_counter % 2 == 0 && state_ptr->currentView == SEARCH_VIEW) ||
            (state_ptr->currentView == TRACK_VIEW || update_counter % 3 == 0)) {
                process_d_bus_events();

                update_player();

                load_waiting_music();
        }

        return TRUE;
}

static gboolean quit_on_signal(gpointer user_data)
{
        GMainLoop *loop = (GMainLoop *)user_data;
        g_main_loop_quit(loop);
        quit();
        return G_SOURCE_REMOVE; // Remove the signal source
}

void create_loop(void)
{
        update_last_input_time();

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        g_unix_signal_add(SIGINT, quit_on_signal, main_loop);
        g_unix_signal_add(SIGHUP, quit_on_signal, main_loop);
        g_timeout_add(34, mainloop_callback, NULL);
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
}

void run(bool start_playing)
{
        AppState *state = get_app_state();
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();
        PlaybackState *ps = get_playback_state();
        UserData *user_data = audio_data.pUserData;

        if (unshuffled_playlist == NULL) {
                set_unshuffled_playlist(deep_copy_playlist(playlist));
        }

        if (state->uiSettings.saveRepeatShuffleSettings) {
                if (state->uiSettings.repeatState == 1)
                        toggle_repeat();
                if (state->uiSettings.repeatState == 2) {
                        toggle_repeat();
                        toggle_repeat();
                }
                if (state->uiSettings.shuffle_enabled)
                        toggle_shuffle();
        }

        if (playlist->head == NULL) {
                state->currentView = LIBRARY_VIEW;
        }

        init_mpris();

        if (state->uiSettings.chromaPreset >= 0) {
                chroma_set_current_preset(state->uiSettings.chromaPreset);
                state->uiSettings.visualizations_instead_of_cover = true;
        }
        ps->loadedNextSong = false;
        if (start_playing)
                ps->waitingForPlaylist = true;

        audio_data.currentFileIndex = 0;
        user_data->current_song_data = NULL;
        user_data->songdataA = NULL;
        user_data->songdataB = NULL;
        user_data->songdataADeleted = true;
        user_data->songdataBDeleted = true;

        if (playlist->count != 0)
                check_and_load_next_song();

        create_loop();

        clear_screen();
        fflush(stdout);
}

void init_locale(void)
{
        setlocale(LC_ALL, "");
        setlocale(LC_CTYPE, "");
#ifdef __ANDROID__
        // Termux prefix
        const char *locale_dir = "/data/data/com.termux/files/usr/share/locale";
#elif __APPLE__
        const char *locale_dir = "/usr/local/share/locale";
#else
        const char *locale_dir = "/usr/share/locale";
#endif
        bindtextdomain("kew", locale_dir);
        textdomain("kew");
}

void kew_init(bool set_library_enqueued_status)
{
        AppState *state = get_app_state();

        set_nonblocking_mode();

        disable_terminal_line_input();
        init_resize();
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &state->uiState.windowSize);
        enable_scrolling();

        init_input();

        // This is to not stop Chroma when we can't keep up with it, instead just return an error
        signal(SIGPIPE, SIG_IGN);

        PlaybackState *ps = get_playback_state();
        UserData *user_data = audio_data.pUserData;
        PlayList *playlist = get_playlist();
        state->tmpCache = create_cache();

        c_strcpy(ps->loadingdata.file_path, "", sizeof(ps->loadingdata.file_path));
        ps->loadingdata.songdataA = NULL;
        ps->loadingdata.songdataB = NULL;
        ps->loadingdata.loadA = true;
        ps->loadingdata.loadingFirstDecoder = true;
        audio_data.restart = true;
        user_data->songdataADeleted = true;
        user_data->songdataBDeleted = true;
        unsigned int seed = (unsigned int)time(NULL);

        srand(seed);
        pthread_mutex_init(&(ps->loadingdata.mutex), NULL);
        pthread_mutex_init(&(playlist->mutex), NULL);
        free_search_results();
        reset_chosen_dir();
        create_library(set_library_enqueued_status);
        state->uiSettings.LAST_ROW = _(" [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]");
        clear_screen();
        fflush(stdout);

#ifdef DEBUG
        // g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        state->uiState.logFile = freopen("error.log", "w", stderr);
        if (state->uiState.logFile == NULL) {
                fprintf(stdout, "Failed to redirect stderr to error.log\n");
        }
#else
        FILE *null_stream = freopen("/dev/null", "w", stderr);
        (void)null_stream;
#endif
}

void init_default_state(void)
{
        bool set_library_enqueued_status = true;
        kew_init(set_library_enqueued_status);

        AppState *state = get_app_state();
        FileSystemEntry *library = get_library();
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        add_enqueued_songs_to_playlist(library, playlist);

        set_unshuffled_playlist(deep_copy_playlist(playlist));

        reset_list_after_dequeuing_playing_song();

        audio_data.restart = true;
        audio_data.end_of_list_reached = true;
        ps->loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(false);
}

void kew_shutdown()
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();
        FileSystemEntry *library = get_library();
        AppSettings *settings = get_app_settings();
        PlayList *favorites_playlist = get_favorites_playlist();

#ifndef __ANDROID__
        stop_at_shutdown();
#endif

        pthread_mutex_lock(&(state->data_source_mutex));

        sound_shutdown();

        free_decoders();

        emit_playback_stopped_mpris();

        if (chroma_is_started())
                state->uiSettings.chromaPreset = chroma_get_current_preset();
        else
                state->uiSettings.chromaPreset = -1;

        chroma_stop();

        bool noMusicFound = false;

        if (library == NULL || library->children == NULL) {
                noMusicFound = true;
        }

        UserData *user_data = audio_data.pUserData;

        unload_songs(user_data);

#ifdef CHAFA_VERSION_1_16
        retire_passthrough_workarounds_tmux();
#endif
        bool wait_until_complete = true;
        update_library_if_changed_detected(wait_until_complete);
        shutdown_input();
        free_search_results();
        cleanup_mpris();
        set_path(settings->path);
        set_prefs(settings, &(state->uiSettings));
        save_favorites_playlist(settings->path, favorites_playlist);
        delete_cache(state_ptr->tmpCache);
        save_library();
        free_library();
        free_playlists();
        set_default_text_color();

        if (audio_data.pUserData != NULL)
                free(audio_data.pUserData);

        pthread_mutex_destroy(&(ps->loadingdata.mutex));
        pthread_mutex_destroy(&(state->switch_mutex));
        pthread_mutex_unlock(&(state->data_source_mutex));
        pthread_mutex_destroy(&(state->data_source_mutex));

        free_visuals();

#ifdef USE_DBUS
        cleanup_notifications();
#endif

#ifdef DEBUG
        if (state->uiState.logFile)
                fclose(state->uiState.logFile);
#endif

        if (freopen("/dev/stderr", "w", stderr) == NULL) {
                perror("freopen error");
        }

        if (state_ptr->uiSettings.mouseEnabled)
                disable_terminal_mouse_buttons();

        printf("\n");
        show_cursor();
        exit_alternate_screen_buffer();
        restore_terminal_mode();

        if (state_ptr->uiSettings.trackTitleAsWindowTitle)
                restore_terminal_window_title();

        if (noMusicFound) {
                printf(_("No Music found.\n"));
                printf(_("Please make sure the path is set correctly. \n"));
                printf(_("To set it type: kew path \"/path/to/Music\". \n"));
        } else if (state->uiState.noPlaylist) {
                printf(_("Music not found.\n"));
        }

        if (has_error_message()) {
                printf(_("%s\n"), get_error_message());
        }

        fflush(stdout);
}

void init_state(void)
{
        AppState *state = get_app_state();

        state->uiSettings.VERSION = VERSION;
        state->uiSettings.uiEnabled = true;
        state->uiSettings.color.r = 125;
        state->uiSettings.color.g = 125;
        state->uiSettings.color.b = 125;
        state->uiSettings.coverEnabled = true;
        state->uiSettings.hideLogo = false;
        state->uiSettings.hideHelp = false;
        state->uiSettings.quitAfterStopping = false;
        state->uiSettings.hideGlimmeringText = false;
        state->uiSettings.coverAnsi = false;
        state->uiSettings.visualizerEnabled = true;
        state->uiSettings.visualizer_height = 5;
        state->uiSettings.visualizer_color_type = 0;
        state->uiSettings.visualizerBrailleMode = false;
        state->uiSettings.visualizer_bar_mode = 2;
        state->uiSettings.titleDelay = 9;
        state->uiSettings.cacheLibrary = -1;
        state->uiSettings.mouseEnabled = true;
        state->uiSettings.mouseLeftClickAction = 0;
        state->uiSettings.mouseMiddleClickAction = 1;
        state->uiSettings.mouseRightClickAction = 2;
        state->uiSettings.mouseScrollUpAction = 3;
        state->uiSettings.mouseScrollDownAction = 4;
        state->uiSettings.mouseAltScrollUpAction = 7;
        state->uiSettings.mouseAltScrollDownAction = 8;
        state->uiSettings.replayGainCheckFirst = 0;
        state->uiSettings.saveRepeatShuffleSettings = 1;
        state->uiSettings.repeatState = 0;
        state->uiSettings.shuffle_enabled = 0;
        state->uiSettings.trackTitleAsWindowTitle = 1;
        state->uiState.numDirectoryTreeEntries = 0;
        state->uiState.num_progress_bars = 35;
        state->uiState.chosen_node_id = 0;
        state->uiState.resetPlaylistDisplay = true;
        state->uiState.allowChooseSongs = false;
        state->uiState.openedSubDir = false;
        state->uiState.numSongsAboveSubDir = 0;
        state->uiState.resizeFlag = 0;
        state->uiState.collapseView = false;
        state->uiState.refresh = true;
        state->uiState.isFastForwarding = false;
        state->uiState.isRewinding = false;
        state->uiState.songWasRemoved = false;
        state->uiState.startFromTop = false;
        state->uiState.lastNotifiedId = -1;
        state->uiState.noPlaylist = false;
        state->uiState.logFile = NULL;
        state->uiState.showLyricsPage = false;
        state->uiState.currentLibEntry = NULL;
        state->tmpCache = NULL;
        state->uiSettings.default_color = 150;
        state->uiSettings.defaultColorRGB.r = state->uiSettings.default_color;
        state->uiSettings.defaultColorRGB.g = state->uiSettings.default_color;
        state->uiSettings.defaultColorRGB.b = state->uiSettings.default_color;
        state->uiSettings.kewColorRGB.r = 222;
        state->uiSettings.kewColorRGB.g = 43;
        state->uiSettings.kewColorRGB.b = 77;
        state->uiSettings.chromaPreset = -1;
        state->uiSettings.visualizations_instead_of_cover = false;

        PlaybackState *ps = get_playback_state();

        ps->lastPlayedId = -1;
        ps->usingSongDataA = true;
        ps->nextSongNeedsRebuilding = false;
        ps->songHasErrors = false;
        ps->forceSkip = false;
        ps->skipFromStopped = false;
        ps->skipping = false;
        ps->skipOutOfOrder = false;
        ps->clearingErrors = false;
        ps->hasSilentlySwitched = false;
        ps->songLoading = false;
        ps->loadedNextSong = false;
        ps->waitingForNext = false;
        ps->waitingForPlaylist = false;
        ps->notifySwitch = false;
        ps->notifyPlaying = false;

        pthread_mutex_init(&(state->data_source_mutex), NULL);
        pthread_mutex_init(&(state->switch_mutex), NULL);

        set_unshuffled_playlist(NULL);
        set_favorites_playlist(NULL);

        audio_data.pUserData = malloc(sizeof(UserData));

        reset_digits_pressed();

        state_ptr = state;
}

void force_terminal_restore(int sig)
{
    ssize_t res;

    // Show cursor
    res = write(STDOUT_FILENO, "\033[?25h", 7);
    (void)res;

    // Leave alternate screen
    res = write(STDOUT_FILENO, "\033[?1049l", 8);
    (void)res;

    // Disable mouse
    res = write(STDOUT_FILENO, "\033[?1000l", 9);
    (void)res;

    // Restore default handler for this signal
    signal(sig, SIG_DFL);

    // Re-raise the signal so the kernel prints the crash message
    raise(sig);
}

int main(int argc, char *argv[])
{
        AppState *state = get_app_state();

        init_state();
        init_locale();
        restart_if_already_running(argv);

        AppSettings *settings = get_app_settings();
        PlayList *playlist = get_playlist();
        PlayList *favorites_playlist = get_favorites_playlist();

        if ((argc == 2 &&
             ((strcmp(argv[1], "--help") == 0) ||
              (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0)))) {
                show_help();
                exit(0);
        } else if (argc == 2 && (strcmp(argv[1], "--version") == 0 ||
                                 strcmp(argv[1], "-v") == 0)) {
                state->uiSettings.colorMode = COLOR_MODE_ALBUM;
                state->uiSettings.color = state->uiSettings.defaultColorRGB;
                print_about_for_version(NULL);
                exit(0);
        }

        *settings = init_settings();
        transfer_settings_to_ui();
        init_key_mappings(settings);
        set_track_title_as_window_title();

        if (argc == 3 && (strcmp(argv[1], "path") == 0)) {
                char de_expanded[PATH_MAX];
                collapse_path(argv[2], de_expanded);
                c_strcpy(settings->path, de_expanded, sizeof(settings->path));
                set_path(settings->path);
                exit(0);
        }

        enable_mouse(&(state->uiSettings));
        enter_alternate_screen_buffer();
        atexit(kew_shutdown);

        signal(SIGINT, force_terminal_restore);
        signal(SIGSEGV, force_terminal_restore);
        signal(SIGABRT, force_terminal_restore);

        if (settings->path[0] == '\0') {
                set_music_path();
        }

        bool exact_search = false;
        handle_options(&argc, argv, &exact_search);
        load_favorites_playlist(settings->path, &favorites_playlist);

        ensure_default_theme_pack();

        init_theme(argc, argv);

        if (argc == 1) {
                init_default_state();
        } else if (argc == 2 && strcmp(argv[1], "all") == 0) {
                kew_init(false);
                play_all();
                run(true);
        } else if (argc == 2 && strcmp(argv[1], "albums") == 0) {
                kew_init(false);
                play_all_albums();
                run(true);
        } else if (argc == 2 && strcmp(argv[1], ".") == 0 && favorites_playlist->count != 0) {
                kew_init(false);
                play_favorites_playlist();
                run(true);
        } else if (argc >= 2) {
                kew_init(false);
                make_playlist(&playlist, argc, argv, exact_search, settings->path);

                if (playlist->count == 0) {
                        if (argc > 1 && argv[1] && strcmp(argv[1], "theme") != 0)
                                state->uiState.noPlaylist = true;
                        exit(0);
                }

                FileSystemEntry *library = get_library();

                mark_list_as_enqueued(library, playlist);
                run(true);
        }

        return 0;
}
