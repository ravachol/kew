/**
 * @file control_ui.c
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include "control_ui.h"

#include "ui/chroma.h"
#include "ui/player_ui.h"
#include "ui/queue_ui.h"

#include "common/appstate.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"

#include "common/common.h"

#include "sys/mpris.h"

#include "data/theme.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <miniaudio.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void seek_forward(void)
{
        AppState *state = get_app_state();
        Node *current = get_current_song();
        if (current == NULL)
                return;

        double duration = current->song.duration;
        if (duration <= 0.0)
                return;

        double step_percent = 100.0 / state->uiState.num_progress_bars;

        int seconds = (int)(duration * (step_percent / 100.0));

        seek(seconds);

        state->uiState.isFastForwarding = true;
}

void seek_back(void)
{
        AppState *state = get_app_state();
        Node *current = get_current_song();

        if (current == NULL)
                return;

        double duration = current->song.duration;
        if (duration <= 0.0)
                return;

        double step_percent = 100.0 / state->uiState.num_progress_bars;

        int seconds = (int)(duration * (step_percent / 100.0));

        seek(-seconds);

        state->uiState.isRewinding = true;
}

void cycle_color_mode(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        clear_screen();

        switch (ui->colorMode) {
        case COLOR_MODE_DEFAULT:
                ui->colorMode = COLOR_MODE_ALBUM;
                break;
        case COLOR_MODE_ALBUM:
                ui->colorMode = COLOR_MODE_THEME;
                break;
        case COLOR_MODE_THEME:
                ui->colorMode = COLOR_MODE_DEFAULT;
                break;
        }

        bool themeLoaded = false;

        switch (ui->colorMode) {
        case COLOR_MODE_DEFAULT:
                if (load_theme("default", true)) {
                        themeLoaded = true;
                }
                break;
        case COLOR_MODE_ALBUM:
                themeLoaded = true;
                break;
        case COLOR_MODE_THEME:
                if (ui->theme_name[0] != '\0' &&
                    load_theme(ui->theme_name, true)) {
                        themeLoaded = true;
                }
        }

        if (!themeLoaded) {
                cycle_color_mode();
        }

        trigger_redraw_side_cover();
        trigger_refresh();
}

void cycle_visualization(void)
{
        if (chroma_is_started())
                request_stop_visualization();

        request_next_visualization();
}

void cycle_themes(void)
{
        clear_screen();

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        char *config_path = get_config_path();
        if (!config_path)
                return;

        char themes_path[PATH_MAX];
        snprintf(themes_path, sizeof(themes_path), "%s/themes", config_path);

        DIR *dir = opendir(themes_path);
        if (!dir) {
                perror("Failed to open themes directory");
                return;
        }

        struct dirent *entry;
        char *themes[256];
        int theme_count = 0;

        // Collect all *.theme files
        while ((entry = readdir(dir)) != NULL) {
                if (strstr(entry->d_name, ".theme")) {
                        themes[theme_count++] = strdup(entry->d_name);
                }
        }
        closedir(dir);

        if (theme_count == 0) {
                set_error_message("No themes found.");
                free(config_path);
                return;
        }

        // Find the index of the current theme
        int current_index = -1;
        char current_filename[NAME_MAX];

        strncpy(current_filename, ui->theme_name, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';

        if (strlen(current_filename) + 6 < sizeof(current_filename))
                strcat(current_filename, ".theme");

        for (int i = 0; i < theme_count; i++) {
                if (strcmp(themes[i], current_filename) == 0) {
                        current_index = i;
                        break;
                }
        }

        // Get next theme (wrap around)
        int next_index = (current_index + 1) % theme_count;

        if (load_theme(themes[next_index], false)) {
                ui->colorMode = COLOR_MODE_THEME;

                snprintf(ui->theme_name, sizeof(ui->theme_name), "%s",
                         themes[next_index]);
                char *dot = strstr(ui->theme_name, ".theme");
                if (dot)
                        *dot = '\0';
        }

        trigger_redraw_side_cover();
        trigger_refresh();

        for (int i = 0; i < theme_count; i++) {
                free(themes[i]);
        }

        free(config_path);
}

void toggle_visualizer(void)
{
        AppSettings *settings = get_app_settings();
        AppState *state = get_app_state();

        state->uiSettings.visualizerEnabled = !state->uiSettings.visualizerEnabled;
        c_strcpy(settings->visualizerEnabled, state->uiSettings.visualizerEnabled ? "1" : "0",
                 sizeof(settings->visualizerEnabled));

        restore_cursor_position();
        trigger_redraw_side_cover();
        trigger_refresh();
}

void toggle_show_lyrics_page(void)
{
        AppState *state = get_app_state();
        state->uiState.showLyricsPage = !state->uiState.showLyricsPage;
        trigger_refresh();
}

void toggle_ascii(void)
{
        if (chroma_is_started()) {
                request_stop_visualization();
                trigger_refresh();
                return;
        }

        AppSettings *settings = get_app_settings();
        AppState *state = get_app_state();

        state->uiSettings.coverAnsi = !state->uiSettings.coverAnsi;
        c_strcpy(settings->coverAnsi, state->uiSettings.coverAnsi ? "1" : "0",
                 sizeof(settings->coverAnsi));
        trigger_redraw_side_cover();
        trigger_refresh();
}

void toggle_repeat(void)
{
        AppState *state = get_app_state();
        bool repeat_enabled = is_repeat_enabled();
        bool repeat_list_enabled = is_repeat_list_enabled();

        if (repeat_enabled) {
                set_repeat_enabled(false);
                set_repeat_list_enabled(true);
                emit_string_property_changed("loop_status", "List");
                state->uiSettings.repeatState = 2;
        } else if (repeat_list_enabled) {
                set_repeat_enabled(false);
                set_repeat_list_enabled(false);
                emit_string_property_changed("loop_status", "None");
                state->uiSettings.repeatState = 0;
        } else {
                set_repeat_enabled(true);
                set_repeat_list_enabled(false);
                emit_string_property_changed("loop_status", "Track");
                state->uiSettings.repeatState = 1;
        }

        if (state->currentView != TRACK_VIEW)
                trigger_refresh();
}

void toggle_pause()
{
        if (is_stopped()) {
                view_enqueue(false);
        } else {
                ops_toggle_pause();
        }
}

void toggle_notifications(void)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();
        UISettings *ui = &(state->uiSettings);

        ui->allowNotifications = !ui->allowNotifications;
        c_strcpy(settings->allowNotifications,
                 ui->allowNotifications ? "1" : "0",
                 sizeof(settings->allowNotifications));
}

void toggle_shuffle(void)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        state->uiSettings.shuffle_enabled = !is_shuffle_enabled();
        set_shuffle_enabled(state->uiSettings.shuffle_enabled);

        Node *current = get_current_song();
        PlayList *playlist = get_playlist();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (state->uiSettings.shuffle_enabled) {
                pthread_mutex_lock(&(playlist->mutex));

                shuffle_playlist_starting_from_song(playlist, current);

                pthread_mutex_unlock(&(playlist->mutex));

                emit_boolean_property_changed("Shuffle", TRUE);
        } else {
                char *path = NULL;
                if (current != NULL) {
                        path = strdup(current->song.file_path);
                }

                PlayList *playlist = get_playlist();

                deep_copy_play_list_onto_list(unshuffled_playlist, &playlist);

                if (path != NULL) {
                        set_current_song(find_path_in_playlist(path, playlist));
                        free(path);
                }

                emit_boolean_property_changed("Shuffle", FALSE);
        }

        ps->loadedNextSong = false;
        set_next_song(NULL);

        if (state->currentView == PLAYLIST_VIEW ||
            state->currentView == LIBRARY_VIEW)
                trigger_refresh();

        emit_shuffle_changed();
}

bool should_refresh_player(void)
{
        PlaybackState *ps = get_playback_state();

        return !ps->skipping && !is_EOF_reached() && !is_impl_switch_reached();
}

int load_theme(const char *theme_name,
               bool is_ansi_theme)
{
        AppState *app_state = get_app_state();
        AppSettings *settings = get_app_settings();

        if (!app_state || !theme_name)
                return 0;

        char *config_path = get_config_path();
        if (!config_path)
                return 0;

        // Check if config directory exists
        struct stat st = {0};
        if (stat(config_path, &st) == -1) {
                free(config_path);
                return 0;
        }

        // Build full theme filename
        char filename[NAME_MAX];
        const char *extension = ".theme";
        size_t themeNameLen = strlen(theme_name);
        size_t extLen = strlen(extension);

        // Check if theme_name already ends with ".theme"
        int has_extension =
            (themeNameLen >= extLen &&
             strcmp(theme_name + themeNameLen - extLen, extension) == 0);

        if (has_extension) {
                if (snprintf(filename, sizeof(filename), "%s", theme_name) >=
                    (int)sizeof(filename)) {
                        fprintf(stderr, "Theme filename is too long\n");
                        set_error_message("Theme filename is too long");
                        free(config_path);
                        return 0;
                }
        } else {
                if (snprintf(filename, sizeof(filename), "%s.theme",
                             theme_name) >= (int)sizeof(filename)) {
                        fprintf(stderr, "Theme filename is too long\n");
                        set_error_message("Theme filename is too long");
                        free(config_path);
                        return 0;
                }
        }

        // Build full themes directory path: configDir + "/themes"
        char themes_dir[PATH_MAX];
        if (snprintf(themes_dir, sizeof(themes_dir), "%s/themes", config_path) >=
            (int)sizeof(themes_dir)) {
                fprintf(stderr, "Themes path is too long\n");
                set_error_message("Themes path is too long");
                free(config_path);
                return 0;
        }

        char *lower_filename = string_to_lower(filename);

        // Call the loader
        int loaded =
            load_theme_from_file(themes_dir, lower_filename, &app_state->uiSettings.theme);
        if (!loaded) {
                free(config_path);
                free(lower_filename);
                return 0; // failed to load
        }

        app_state->uiSettings.themeIsSet = true;

        if (is_ansi_theme) {
                // Default ANSI theme: store in settings->ansiTheme
                snprintf(settings->ansiTheme, sizeof(settings->ansiTheme), "%s",
                         theme_name);
        } else {
                // Truecolor theme: store in settings->theme
                snprintf(settings->theme, sizeof(settings->theme), "%s",
                         theme_name);
        }

        free(config_path);
        free(lower_filename);

        return 1;
}
