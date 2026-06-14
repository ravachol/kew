/**
 * @file control_ui.c
 * @brief Handles playback control interface rendering and input.
 *
 * Draws the transport controls (play/pause, skip, seek) and
 * maps user actions to playback operations.
 */

#include "control_ui.h"

#include "ops/playlist_ops.h"
#include "ui/chroma.h"
#include "ui/queue_ui.h"
#include "ui/render_ui.h"

#include "common/appstate.h"
#include "ops/playback_ops.h"
#include "ops/playback_state.h"

#include "common/common.h"

#include "sys/mpris.h"

#include "data/theme.h"

#include "utils/utils.h"

#include <miniaudio.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int get_num_progress_bars(void)
{
        Model *model = get_model();
        if (model->state.ui.num_progress_bars <= 0)
                return DEFAULT_NUM_PROGRESS_BARS;
        return model->state.ui.num_progress_bars;
}

void seek_forward(Model *model)
{
        AppState *state = &model->state;
        Node *current = get_current_song();
        if (current == NULL)
                return;

        SongData *songdata = model->songdata;

        if (!songdata)
                return;

        double duration = songdata->duration;

        if (duration <= 0.0)
                return;

        double step_percent = 100.0 / get_num_progress_bars();

        int seconds = (int)(duration * (step_percent / 100.0));

        if (seconds == 0)
                seconds = 1;

        seek(seconds);

        state->ui.isFastForwarding = true;
}

void seek_back(Model *model)
{
        AppState *state = &model->state;
        Node *current = get_current_song();

        if (current == NULL)
                return;

        SongData *songdata = model->songdata;

        if (!songdata)
                return;

        double duration = songdata->duration;

        if (duration <= 0.0)
                return;

        double step_percent = 100.0 / get_num_progress_bars();

        int seconds = (int)(duration * (step_percent / 100.0));

        seek(-seconds);

        state->ui.isRewinding = true;
}

void cycle_color_mode(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->settings);

        switch (ui->colorMode) {
        case COLOR_MODE_DEFAULT:
                ui->colorMode = COLOR_MODE_ALBUM_ONE;
                break;
        case COLOR_MODE_ALBUM_ONE:
                ui->colorMode = COLOR_MODE_ALBUM;
                break;
        case COLOR_MODE_ALBUM:
                ui->colorMode = COLOR_MODE_THEME;
                break;
        case COLOR_MODE_THEME:
                ui->colorMode = COLOR_MODE_NEUTRAL;
                break;
        case COLOR_MODE_NEUTRAL:
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
        case COLOR_MODE_ALBUM_ONE:
                if (load_theme("onealbumcolor", false)) {
                        themeLoaded = true;
                }
                break;
        case COLOR_MODE_ALBUM:
                if (load_theme("albumcolors", false)) {
                        themeLoaded = true;
                }
                break;
        case COLOR_MODE_THEME:
                if (ui->theme_name[0] != '\0' &&
                    load_theme(ui->theme_name, false)) {
                        themeLoaded = true;
                        if (ui->visualizer_mode > 2)
                                ui->visualizer_mode = 2;
                }
                break;
        case COLOR_MODE_NEUTRAL:
                if (load_theme("neutral", false)) {
                        themeLoaded = true;
                }
                break;
        }

        if (!themeLoaded) {
                cycle_color_mode();
        }

        set_dirty(DIRTY_ALL);
}

void cycle_visualization(void)
{
        Model *model = get_model();

        model->state.settings.coverAnsi = false;

        if (model->songdata_ok && model->state.settings.coverEnabled) {

                if (model->state.ui.has_chroma == -1)
                        model->state.ui.has_chroma = chroma_is_installed();

                if (model->state.ui.has_chroma == 1) {

                        if (model->state.ui.chroma_started) {

                                model->state.ui.chroma_next_preset_requested = true;

                        } else {

                                if (model->state.settings.chromaPreset >= 0)
                                        chroma_set_current_preset(model->state.settings.chromaPreset);

                                model->state.ui.chroma_start_requested = true;
                        }

                        model->state.settings.visualizations_instead_of_cover = true;
                }
        }
}

void cycle_themes(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->settings);

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
                if (strstr(entry->d_name, ".theme") && !strstr(entry->d_name, ".theme.bak")) {
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

                if (strcmp(ui->theme_name, "onealbumcolor") == 0)
                        ui->colorMode = COLOR_MODE_ALBUM_ONE;
                if (strcmp(ui->theme_name, "albumcolors") == 0)
                        ui->colorMode = COLOR_MODE_ALBUM;
                if (strcmp(ui->theme_name, "neutral") == 0)
                        ui->colorMode = COLOR_MODE_NEUTRAL;
                if (strcmp(ui->theme_name, "default") == 0)
                        ui->colorMode = COLOR_MODE_DEFAULT;

                if (ui->colorMode == COLOR_MODE_THEME)
                        if (ui->visualizer_mode > 2)
                                ui->visualizer_mode = 2;
        }

        set_dirty(DIRTY_ALL);

        for (int i = 0; i < theme_count; i++) {
                free(themes[i]);
        }

        free(config_path);
}

void toggle_ascii(void)
{
        if (chroma_is_started()) {
                request_stop_visualization();
                set_dirty(DIRTY_ALL);
                return;
        }

        AppState *state = get_app_state();
        state->settings.coverAnsi = !state->settings.coverAnsi;
        set_dirty(DIRTY_ALL);
}

void toggle_repeat(void)
{
        AppState *state = get_app_state();

        int repeat_state = get_repeat_state();

        repeat_state++;

        if (repeat_state >= 3)
                repeat_state = 0;

        set_repeat_state(repeat_state);

        if (repeat_state == 0) {
                emit_string_property_changed("loop_status", "None");

                state->settings.repeatState = 0;
        } else if (repeat_state == 1) {

                emit_string_property_changed("loop_status", "Track");
                state->settings.repeatState = 1;
        } else {
                emit_string_property_changed("loop_status", "List");
                state->settings.repeatState = 2;
        }

        set_dirty(DIRTY_FOOTER);
}

void toggle_pause()
{
        if (is_stopped()) {
                view_enqueue(false);
        } else if (is_paused() && get_current_song() == NULL) {
                PlayList *playlist = get_playlist();
                playlist_play(playlist);

                ops_toggle_pause();
        } else {
                ops_toggle_pause();
        }
}

void toggle_shuffle(Model *model)
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        state->settings.shuffle_enabled = !is_shuffle_enabled();
        set_shuffle_enabled(state->settings.shuffle_enabled);

        Node *current = get_current_song();

        if (state->settings.shuffle_enabled) {

                if (model->playlist) {
                        pthread_mutex_lock(&(model->playlist->mutex));

                        shuffle_playlist_starting_from_song(model->playlist, current);

                        pthread_mutex_unlock(&(model->playlist->mutex));
                }

                emit_boolean_property_changed("Shuffle", TRUE);

        } else if (model->playlist && model->unshuffled_playlist) {

                char *path = NULL;

                if (model->playlist && model->unshuffled_playlist) {

                        pthread_mutex_lock(&(model->playlist->mutex));

                        int id = -1;
                        if (current)
                                id = current->id;

                        current = NULL;

                        deep_copy_list(model->unshuffled_playlist, &(model->playlist));

                        if (id >= 0 && current == NULL)
                                current = find_selected_entry_by_id(model->playlist, id);

                        pthread_mutex_unlock(&(model->playlist->mutex));
                }

                if (current != NULL) {
                        path = strdup(current->song.file_path);

                        if (path != NULL) {
                                set_current_song(find_path_in_playlist(path, model->playlist));
                                free(path);
                        }
                }

                emit_boolean_property_changed("Shuffle", FALSE);
        }

        ps->loadedNextSong = false;
        set_next_song(NULL);

        set_dirty(DIRTY_PLAYLIST | DIRTY_LIBRARY | DIRTY_FOOTER);
}

bool can_refresh_player(void)
{
        Model *model = get_model();
        PlaybackState *ps = &model->playbackState;

        return !ps->skipping &&
        !is_EOF_reached() &&
        !is_switching_track() &&
        !should_exit() &&
        !model->state.ui.resumed_in_background;
}

int load_theme(const char *theme_name,
               bool is_ansi_theme)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();

        if (!state || !theme_name)
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
            load_theme_from_file(themes_dir, lower_filename, &state->settings.theme);
        if (!loaded) {
                free(config_path);
                free(lower_filename);
                return 0; // failed to load
        }

        state->settings.themeIsSet = true;

        if (is_ansi_theme) {
                // Default ANSI theme: store in settings->ansiTheme
                snprintf(settings->ansiTheme, sizeof(settings->ansiTheme), "%s",
                         theme_name);
                snprintf(settings->theme, sizeof(settings->theme), "%s",
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
