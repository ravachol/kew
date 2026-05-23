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
#include "sound/sound_facade.h"
#include "ui/components.h"
#endif

#ifdef __FreeBSD__
#define __BSD_VISIBLE 1
#endif

#ifndef KEW_VERSION
#define KEW_VERSION "4.0.0"
#endif

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"

#include "sys/discord_rpc.h"
#include "sys/mpris.h"
#include "sys/notifications.h"
#include "sys/sys_integration.h"

#include "ui/chroma.h"
#include "ui/cli.h"
#include "ui/common_ui.h"
#include "ui/control_ui.h"
#include "ui/input.h"
#include "ui/render_ui.h"
#include "ui/queue_ui.h"
#include "ui/settings.h"
#include "ui/termbox2_input.h"
#include "ui/visuals.h"

#include "update/update.h"
#include "update/messages.h"

#include "ops/library_ops.h"
#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/track_manager.h"
#include "ops/search_ops.h"

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

AppState *state_ptr = NULL;



/**
 * @brief Runs the Model-View-Update Tick
 *
 */
void player_tick(Model *model, RenderContext *ctx)
{
        dispatch_msg((struct Msg){.type = MSG_TICK});

        // Process all pending messages
        while (has_pending_msgs()) {
                struct Msg msg = next_msg();

                // Update the model
                UpdateResult res = update(model, &msg);

                // Run commands
                run_command(res);
        }

        if (can_refresh_player()) {

                if (!resize_if_needed())
                {
                        // Render the UI
                        render_ui(model, ctx);

                        // Notify system that a frame was rendered
                        dispatch_msg((struct Msg){.type = MSG_RENDERED});
                }
        }

        // Load music
        dispatch_msg((struct Msg){.type = MSG_LOAD_WAITING_MUSIC});
}

void free_library(void)
{
        FileSystemEntry *library = get_library();

        if (library == NULL)
                return;

        free_tree(library);
}

/**
 * @brief Shuts down the application and cleans up resources.
 *
 * This function stops playback, frees resources, and shuts down the application. It handles
 * cleanup tasks like saving settings, stopping playback, and freeing memory.
 */
void kew_shutdown()
{
        Model *model = get_model();

        shutdown_sound_system();

        emit_playback_stopped_mpris();

        if (chroma_is_started())

                model->state.settings.chromaPreset = chroma_get_current_preset();
        else
                model->state.settings.chromaPreset = -1;

        chroma_stop();

        bool noMusicFound = false;

        if (model->library == NULL || model->library->children == NULL) {
                noMusicFound = true;
        }

#ifdef CHAFA_VERSION_1_16
        retirePassthroughWorkarounds_tmux();
#endif
        bool wait_until_complete = true;
        component_library_helper_reset(model);
        update_library_if_changed_detected(wait_until_complete);
        shutdown_input();

        if (model->state.settings.discordRPCEnabled)
                discord_rpc_shutdown();

        free_search_results();
        cleanup_mpris();
        set_path(model->settings.path);
        set_prefs(&model->settings, &(model->state.settings));
        save_favorites_playlist(model->settings.path, model->favorites_playlist);
        save_library();

        ui_destroy(model);

        free_library();

        free_playlist(&model->playlist);
        free_playlist(&model->unshuffled_playlist);
        free_playlist(&model->favorites_playlist);
        free_layout_config();
        free_visuals();

        set_default_text_color();

        pthread_mutex_destroy(&(model->playbackState.switch_mutex));
        pthread_mutex_destroy(&(model->state.library_mutex));

#ifdef USE_DBUS
        cleanup_notifications();
#endif

#ifdef DEBUG
        if (model->state.ui.logFile)
                fclose(model->state.ui.logFile);
#endif

        if (freopen("/dev/stderr", "w", stderr) == NULL) {
                perror("freopen error");
        }

        if (state_ptr->settings.mouseEnabled)
                disable_terminal_mouse_buttons();

        printf("\n");
        show_cursor();
        exit_alternate_screen_buffer();
        restore_terminal_mode();

        if (state_ptr->settings.trackTitleAsWindowTitle)
                restore_terminal_window_title();

        if (noMusicFound) {
                printf(_("No Music found.\n"));
                printf(_("Please make sure the path is set correctly. \n"));
                printf(_("To set it type: kew path \"/path/to/Music\". \n"));
        } else if (model->state.ui.noPlaylist) {
                printf(_("Music not found.\n"));
        }

        if (has_error_message()) {
                printf(_("%s\n"), get_error_message());
        }

        printf("\n");
        fflush(stdout);
        exit(0);
}

/**
 * @brief Main callback for the event loop, runs periodically.
 *
 * This function handles actions such as updating elapsed time, processing events, and
 * updating the player UI. It runs at different speeds based on the current view.
 *
 * @param data Additional data passed to the callback (unused in this case).
 *
 * @return gboolean Returns TRUE to keep theq callback running.
 */
gboolean mainloop_callback(gpointer data)
{
        (void)data;

        increment_update_counter();
        handle_cooldown();

        if (should_exit()) {
                g_main_loop_quit((GMainLoop *)data);
                return FALSE;
        }

        int update_counter = get_update_counter();

        Model *model = get_model();

        bool has_active_animation = model->glimmer.active || model->title_delay.active || model->name_scroll.active;

        RenderContext *ctx = get_render_context();

        // Throttle updates depending on current view
        gboolean should_run =
            has_active_animation ||
            (update_counter % 2 == 0 && ctx->render_often) ||
            (update_counter % 4 == 0 && ctx->render_search) ||
            update_counter % 6 == 0;

        if (!should_run)
                return TRUE;

        int mutex_result = pthread_mutex_trylock(&(model->state.library_mutex));

        if (mutex_result != 0) {
                fprintf(stderr, "Failed to lock library mutex.\n");
                return TRUE;
        }

        int mutex_result2 = pthread_mutex_trylock(&(model->playbackState.switch_mutex));

        if (mutex_result2 != 0) {
                fprintf(stderr, "Failed to lock switch mutex.\n");
                return TRUE;
        }

        player_tick(model, ctx);

        pthread_mutex_unlock(&(model->playbackState.switch_mutex));
        pthread_mutex_unlock(&(model->state.library_mutex));

        return TRUE;
}

/**
 * @brief Quits the application upon receiving a signal.
 *
 * This function terminates the main loop and cleans up resources upon receiving a termination signal.
 *
 * @param user_data User data (GMainLoop) passed to the signal handler.
 *
 * @return gboolean Returns G_SOURCE_REMOVE to remove the signal source.
 */
static gboolean quit_on_signal(gpointer user_data)
{
        GMainLoop *loop = (GMainLoop *)user_data;
        g_main_loop_quit(loop);
        return G_SOURCE_REMOVE; // Remove the signal source
}

/**
 * @brief Creates and runs the main event loop.
 *
 * This function sets up the main event loop and signals for quitting. It uses
 * `g_main_loop_run()` to run the loop and handles updates at regular intervals.
 */
void create_loop(void)
{
        update_last_input_time();

        GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

        g_unix_signal_add(SIGINT, quit_on_signal, main_loop);
        g_unix_signal_add(SIGHUP, quit_on_signal, main_loop);
        g_unix_signal_add(SIGTERM, quit_on_signal, main_loop);

        Model *model = get_model();

        g_timeout_add(model->tick, mainloop_callback, main_loop);
        g_main_loop_run(main_loop);
        g_main_loop_unref(main_loop);
        kew_shutdown();
}

/**
 * @brief Runs the application with the specified settings.
 *
 * This function initializes various settings and begins playing music, depending on the
 * provided `start_playing` parameter. It also handles the playlist and UI settings.
 *
 * @param start_playing Boolean flag to indicate whether to start playing immediately.
 */
void run(bool start_playing)
{
        Model *model = get_model();
        AppState *state = &model->state;
        PlayList *playlist = model->playlist;
        PlaybackState *ps = &model->playbackState;

        deep_copy_list(playlist, &model->unshuffled_playlist);

        if (state->settings.saveRepeatShuffleSettings) {

                if (state->settings.shuffle_enabled)
                        toggle_shuffle(model);
        }

        if (playlist->head == NULL) {
                state->currentView = LIBRARY_VIEW;
        }

        init_mpris();

        if (state->settings.chromaPreset >= 0) {
                chroma_set_current_preset(state->settings.chromaPreset);
                state->settings.visualizations_instead_of_cover = true;
        }

        ps->loadedNextSong = false;

        if (start_playing)
                ps->waitingForPlaylist = true;

        if (playlist->count != 0)
                check_and_load_next_song();

        create_loop();
}

/**
 * @brief Initializes the locale settings for the application.
 *
 * This function sets up the locale settings and binds text domains for translations.
 */
void init_locale(void)
{
        setlocale(LC_ALL, "");
        setlocale(LC_CTYPE, "");

        bindtextdomain("kew", LOCALEDIR);
        textdomain("kew");
}

void init_logging(Model *model)
{
#ifdef DEBUG
        // g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        model->state.ui.logFile = freopen("error.log", "w", stderr);
        if (model->state.ui.logFile == NULL) {
                fprintf(stdout, "Failed to redirect stderr to error.log\n");
        }
#else
        (void)model;
        FILE *null_stream = freopen("/dev/null", "w", stderr);
        (void)null_stream;
#endif
}

/**
 * @brief Initializes the application and sets up necessary states.
 *
 * This function sets the initial state of the application, initializes resources, and prepares
 * the environment for playback. It handles various settings, file paths, and library initialization.
 *
 * @param set_library_enqueued_status Flag indicating whether to set the library's enqueued status.
 */
void kew_init(bool set_library_enqueued_status)
{
        Model *model = get_model();
        set_nonblocking_mode();
        disable_terminal_line_input();
        init_resize();
        set_term_size();

        load_favorites_playlist(model->settings.path, &model->favorites_playlist);

        enable_scrolling();

        init_input();

        if (model->state.settings.discordRPCEnabled)
                discord_rpc_init();

        // This is to not stop Chroma when we can't keep up with it, instead just return an error
        signal(SIGPIPE, SIG_IGN);

        // The (sometimes shuffled) sequence of songs that will be played

        start_playing(true);

        unsigned int seed = (unsigned int)time(NULL);

        srand(seed);

        create_library(model, set_library_enqueued_status);
        model->state.settings.LAST_ROW = _(" [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]");
        clear_screen();

        hide_cursor();

        fflush(stdout);

        ui_init(model);

        if (create_sound_system() == -1)
                quit();
}

/**
 * @brief Initializes the default state for the application.
 *
 * This function initializes the default UI settings, playback state, and other system resources.
 * It sets up the playlist, resets various flags, and prepares the system for playback.
 */
void init_default_state(void)
{
        bool set_library_enqueued_status = true;
        kew_init(set_library_enqueued_status);

        AppState *state = get_app_state();
        FileSystemEntry *library = get_library();
        PlayList *playlist = get_playlist();
        PlaybackState *ps = get_playback_state();

        add_enqueued_songs_to_playlist(library, playlist);

        reset_list_after_dequeuing_playing_song();

        start_playing(true);
        sound_system_set_end_of_list_reached(sound_sys, true);
        ps->loadedNextSong = false;

        state->currentView = LIBRARY_VIEW;

        run(false);
}

/**
 * @brief Handles the "play" command from the playlist.
 *
 * This function validates paths and processes the playlist for playback. It checks if the
 * provided paths exist and are valid, then initiates the playback with the playlist.
 *
 * @param argc Pointer to the argument count.
 * @param argv Array of argument strings.
 *
 * @return bool Returns true if the command is valid, false otherwise.
 */
static bool handle_play_command_playlist(int *argc, char **argv)
{
        char de_expanded[PATH_MAX];
        // Working with multiple files
        //validate all paths

        for (int i = 2; i < *argc; i++) {
                if ((expand_path(argv[i], de_expanded) != 0) || (exists_file(de_expanded) == -1)) {
                        return false;
                }
        }
        play_command_with_playlist(argc, argv);

        return true;
}

/**
 * @brief Initializes the state for the application.
 *
 * This function sets the application state, initializes variables, and prepares the system for
 * running. It sets up playback, UI settings, and various other application states.
 */
void init_state(void)
{
        AppState *state = get_app_state();

        state->settings.VERSION = KEW_VERSION;
        state->settings.uiEnabled = true;
        state->settings.color.r = 125;
        state->settings.color.g = 125;
        state->settings.color.b = 125;
        state->settings.coverEnabled = true;
        state->settings.hideLogo = false;
        state->settings.hideHelp = false;
        state->settings.hideFooter = false;
        state->settings.hideTimeStatus = true;
        state->settings.quitAfterStopping = false;
        state->settings.hideGlimmeringText = false;
        state->settings.coverAnsi = false;
        state->settings.visualizerEnabled = true;
        state->settings.discordRPCEnabled = true;
        state->settings.visualizer_height = 5;
        state->settings.visualizer_color_type = 0;
        state->settings.visualizerBrailleMode = false;
        state->settings.visualizer_bar_mode = 2;
        state->settings.titleDelay = 9;
        state->settings.cacheLibrary = -1;
        state->settings.mouseEnabled = true;
        state->settings.mouseLeftClickAction = 0;
        state->settings.mouseMiddleClickAction = 1;
        state->settings.mouseRightClickAction = 2;
        state->settings.mouseScrollUpAction = 3;
        state->settings.mouseScrollDownAction = 4;
        state->settings.mouseAltScrollUpAction = 7;
        state->settings.mouseAltScrollDownAction = 8;
        state->settings.replayGainCheckFirst = 0;
        state->settings.saveRepeatShuffleSettings = 1;
        state->settings.repeatState = 0;
        state->settings.shuffle_enabled = 0;
        state->settings.trackTitleAsWindowTitle = 1;
        state->settings.footer_color.r = 120;
        state->settings.footer_color.g = 120;
        state->settings.footer_color.b = 120;
        state->settings.footer_color.a = 255;
        state->settings.default_color = 150;
        state->settings.defaultColorRGB.r = state->settings.default_color;
        state->settings.defaultColorRGB.g = state->settings.default_color;
        state->settings.defaultColorRGB.b = state->settings.default_color;
        state->settings.defaultColorRGB.a = 255;
        state->settings.kewColorRGB.r = 222;
        state->settings.kewColorRGB.g = 43;
        state->settings.kewColorRGB.b = 77;
        state->settings.kewColorRGB.a = 255;
        state->settings.chromaPreset = -1;
        state->settings.visualizations_instead_of_cover = false;
        state->settings.lastVolume = 100;

        state->ui.numDirectoryTreeEntries = 0;
        state->ui.num_progress_bars = DEFAULT_NUM_PROGRESS_BARS;
        state->ui.chosen_node_id = 0;
        state->ui.resetPlaylistDisplay = true;
        state->ui.allowChooseSongs = false;
        state->ui.allowChooseSearchSongs = false;
        state->ui.resizeFlag = 0;
        state->ui.refresh = true;
        state->ui.isFastForwarding = false;
        state->ui.isRewinding = false;
        state->ui.songWasRemoved = false;
        state->ui.startFromTop = false;
        state->ui.lastNotifiedId = -1;
        state->ui.noPlaylist = false;
        state->ui.logFile = NULL;
        state->ui.showLyricsPage = false;
        state->ui.current_lib_entry = NULL;
        state->ui.chosen_search_dir = NULL;
        state->ui.current_search_entry = NULL;
        state->ui.current_lib_entry = NULL;
        state->ui.last_search_parent = NULL;
        state->ui.chosen_dir = NULL;
        state->ui.aspect_ratio = 0;
        state->ui.visualizer_width = 0;
        state->ui.previous_chosen_song = -1;

        state->ui.start_lib_iter = 0;
        state->ui.chosen_lib_row = 0;
        state->ui.lib_row_count = 0;

        state->ui.footer_row = 0;
        state->ui.footer_col = 0;

        state->ui.chosen_dir = NULL;
        state->ui.start_iter = 0;
        state->ui.previous_chosen_song = 0;
        state->ui.render_often = false;
        state->ui.render_search = false;
        state->ui.chosen_lyrics_row = 0;

        PlaybackState *ps = get_playback_state();

        ps->lastPlayedId = -1;
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
        ps->notifySeek = false;

        pthread_mutex_init(&ps->switch_mutex, NULL);
        pthread_mutex_init(&(state->library_mutex), NULL);

        reset_digits_pressed();

        Model *model = get_model();
        init_logging(model);

        init_model();

        state_ptr = state;
}

/**
 * @brief Restores the terminal state when a signal is received.
 *
 * This function handles restoring the terminal state, such as showing the cursor, leaving
 * the alternate screen buffer, and disabling mouse input when a signal is received (e.g., SIGINT).
 *
 * @param sig The signal number.
 */
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

/**
 * @brief Main entry point of the application.
 *
 * This function processes command-line arguments, initializes the application, and runs the main event loop.
 * It handles various cases for displaying help, version, and running the application based on user input.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 *
 * @return int The exit status of the program.
 */
int main(int argc, char *argv[])
{
        AppState *state = get_app_state();
        tty_init();

        init_state();
        init_locale();

        #ifndef DEBUG // Only prevent multiple instances if not debugging. Yes, now you can finally listen to music in kew while you work on kew!
                restart_if_already_running(argv);
        #endif

        Model *model = get_model();

        if ((argc == 2 &&
             ((strcmp(argv[1], "--help") == 0) ||
              (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "-?") == 0)))) {
                print_help();
                exit(0);
        } else if (argc == 2 && (strcmp(argv[1], "--version") == 0 ||
                                 strcmp(argv[1], "-v") == 0)) {
                state->settings.colorMode = COLOR_MODE_ALBUM;
                state->settings.color = state->settings.defaultColorRGB;
                print_about_for_version(model);
                exit(0);
        }
        init_settings(&model->settings);
        transfer_settings_to_ui();
        init_key_mappings(&model->settings);
        set_track_title_as_window_title();

        bool run_for_play_command_with_playlist = false;

        if (argc == 3 && (strcmp(argv[1], "path") == 0)) {
                char de_expanded[PATH_MAX];
                collapse_path(argv[2], de_expanded);
                c_strcpy(model->settings.path, de_expanded, sizeof(model->settings.path));
                set_path(model->settings.path);
                exit(0);
        } else if (argc >= 3 && (strcmp(argv[1], "play") == 0)) {
                run_for_play_command_with_playlist = handle_play_command_playlist(&argc, argv);
        }

        enable_mouse(&(state->settings));
        enter_alternate_screen_buffer();

        signal(SIGINT, force_terminal_restore);
        signal(SIGSEGV, force_terminal_restore);
        signal(SIGABRT, force_terminal_restore);

        if (model->settings.path[0] == '\0') {
                set_music_path();
        }

        bool exact_search = false;
        handle_options(&argc, argv, &exact_search);

        ensure_default_theme_pack();
        ensure_default_layouts();

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
        } else if (argc == 2 && strcmp(argv[1], ".") == 0 && model->favorites_playlist->count != 0) {
                kew_init(false);
                play_favorites_playlist();
                run(true);
        } else if (run_for_play_command_with_playlist) {
                kew_init(false);
                mark_list_as_enqueued(model->library, model->playlist);
                run(true);
        } else if (argc >= 2) {
                kew_init(false);
                make_playlist(&model->playlist, argc, argv, exact_search, model->settings.path);

                if (model->playlist->count == 0) {
                        if (argc > 1 && argv[1] && strcmp(argv[1], "theme") != 0)
                                state->ui.noPlaylist = true;
                        kew_shutdown();
                }

                mark_list_as_enqueued(model->library, model->playlist);
                run(true);
        }

        return 0;
}
