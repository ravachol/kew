/**
 * @file player_ui.c
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */

#include "player_ui.h"

#include "common/events.h"
#include "control_ui.h"
#include "playlist_ui.h"
#include "search_ui.h"
#include "settings.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common_ui.h"

#include "chroma.h"
#include "input.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_state.h"

#include "data/directorytree.h"
#include "data/img_func.h"
#include "data/lyrics.h"
#include "data/playlist.h"
#include "data/song_loader.h"

#include "sys/discord_rpc.h"
#include "sys/sys_integration.h"

#include "utils/term.h"
#include "utils/utils.h"

#include "visuals.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <wchar.h>

#ifdef __APPLE__
const int ABSOLUTE_MIN_WIDTH = 80;
#else
const int ABSOLUTE_MIN_WIDTH = 65;
#endif

// clang-format off
static const char *LOGO[] = {" __\n",
                             "|  |--.-----.--.--.--.\n",
                             "|    <|  -__|  |  |  |\n",
                             "|__|__|_____|________|"};
// clang-format on

static int footer_col = 0;
static int footer_row = 0;
static const int MAX_TERM_SIZE = 10000;
static const int scrolling_interval = 1;
static const int LOGO_WIDTH = 22;
static const int MIN_COVER_SIZE = 5;

static int min_height = 0;
static int preferred_width = 0;
static int preferred_height = 0;
static int text_width = 0;
static int indent = 0;
static int max_list_size = 0;
static int max_search_list_size = 0;
static int num_top_level_songs = 0;
static int start_lib_iter = 0;
static int max_lib_list_size = 0;
static int chosen_row = 0;               // The row that is chosen in playlist view
static int chosen_lib_row = 0;           // The row that is chosen in library view
static int chosen_search_result_row = 0; // The row that is chosen in search view
static int lib_iter = 0;
static int lib_song_iter = 0;
static int lib_row_count = 0;
static int previous_chosen_lib_row = 0;
static int lib_current_dir_song_count = 0;
static PixelData footer_color = {120, 120, 120};
static FileSystemEntry *last_entry = NULL;
static FileSystemEntry *chosen_dir = NULL;
static bool is_same_name_as_last_time = false;
static bool last_paused_state;
static int term_w, term_h;
static int has_chroma = -1;
static bool next_visualization_requested = false;
static bool redraw_side_cover = true;
static ViewState last_view = LIBRARY_VIEW;
static const char *last_cover_path_ptr = NULL;
static size_t last_cover_path_hash = (size_t)-1;

void request_next_visualization(void)
{
        AppState *state = get_app_state();
        state->uiSettings.visualizations_instead_of_cover = true;

        next_visualization_requested = true;
}

void request_stop_visualization(void)
{
        AppState *state = get_app_state();
        state->uiSettings.visualizations_instead_of_cover = false;
        next_visualization_requested = false;
        chroma_stop();
}

int get_footer_row(void)
{
        return footer_row;
}

int get_footer_col(void)
{
        return footer_col;
}

void trigger_redraw_side_cover(void)
{
        redraw_side_cover = true;
}

bool init_theme(int argc, char *argv[])
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        bool themeLoaded = false;

        // Command-line theme handling
        if (argc > 3 && strcmp(argv[1], "theme") == 0) {
                set_error_message("Couldn't load theme. Theme file names shouldn't contain space.");
        } else if (argc == 3 && strcmp(argv[1], "theme") == 0) {
                // Try to load the user-specified theme
                if (load_theme(argv[2], false) > 0) {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                        snprintf(ui->theme_name, sizeof(ui->theme_name), "%s", argv[2]);
                } else {
                        // Failed to load user theme → fall back to
                        // default/ANSI
                        if (ui->colorMode == COLOR_MODE_THEME) {
                                ui->colorMode = COLOR_MODE_DEFAULT;
                        }
                }
        } else if (ui->colorMode == COLOR_MODE_THEME) {
                // If UI has a theme_name stored, try to load it
                if (load_theme(ui->theme_name, false) > 0) {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                }
        }

        // If still in default mode, load default ANSI theme
        if (ui->colorMode == COLOR_MODE_DEFAULT) {
                // Load "default" ANSI theme, but don't overwrite
                // settings->theme
                if (load_theme("default", true)) {
                        themeLoaded = true;
                }
        }

        if (!themeLoaded && ui->colorMode != COLOR_MODE_ALBUM) {
                set_error_message("Couldn't load theme. Forgot to run 'sudo make install'?");
                ui->colorMode = COLOR_MODE_ALBUM;
        }

        return themeLoaded;
}

void set_track_title_as_window_title(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        if (ui->trackTitleAsWindowTitle) {
                save_terminal_window_title();
                set_terminal_window_title("kew");
        }
}

void set_current_as_chosen_dir(void)
{
        AppState *state = get_app_state();

        if (state->uiState.currentLibEntry && state->uiState.currentLibEntry->is_directory)
                chosen_dir = state->uiState.currentLibEntry;
}

int calc_ideal_img_size(int *width, int *height, const int visualizer_height,
                        const int metatag_height)
{
        if (!width || !height)
                return -1;

        float aspect_ratio = calc_aspect_ratio();

        if (!isfinite(aspect_ratio) || aspect_ratio <= 0.0f ||
            aspect_ratio > 100.0f)
                aspect_ratio = 1.0f; // fallback to square

        int term_w = 0, term_h = 0;
        get_term_size(&term_w, &term_h);

        if (term_w <= 0 || term_h <= 0 || term_w > MAX_TERM_SIZE ||
            term_h > MAX_TERM_SIZE) {
                *width = 1;
                *height = 1;
                return -1;
        }

        const int time_display_height = 1;
        const int height_margin = 4;
        const int min_height = visualizer_height + metatag_height +
                               time_display_height + height_margin + 1;

        if (min_height < 0 || min_height > term_h) {
                *width = 1;
                *height = 1;
                return -1;
        }

        int available_height = term_h - min_height;
        if (available_height <= 0) {
                *width = 1;
                *height = 1;
                return -1;
        }

        // Safe calculation using double
        double safe_height = (double)available_height;
        double safe_aspect = (double)aspect_ratio;

        double temp_width = safe_height * safe_aspect;

        // Clamp to INT_MAX and reasonable limits
        if (temp_width < 1.0)
                temp_width = 1.0;
        else if (temp_width > INT_MAX)
                temp_width = INT_MAX;

        int calc_width = (int)ceil(temp_width);
        int calc_height = available_height;

        if (calc_width > term_w) {
                calc_width = term_w;
                if (calc_width <= 0) {
                        *width = 1;
                        *height = 1;
                        return -1;
                }

                double temp_height = (double)calc_width / safe_aspect;

                if (temp_height < 1.0)
                        temp_height = 1.0;
                else if (temp_height > INT_MAX)
                        temp_height = INT_MAX;

                calc_height = (int)floor(temp_height);
        }

        // Final clamping
        if (calc_width < 1)
                calc_width = 1;
        if (calc_height < 2)
                calc_height = 2;

        // Slight adjustment
        calc_height -= 1;
        if (calc_height < 1)
                calc_height = 1;

        *width = calc_width;
        *height = calc_height;

        return 0;
}

void calc_preferred_size(UISettings *ui)
{
        min_height = 2 + (ui->visualizerEnabled ? ui->visualizer_height : 0);
        int metadata_height = 4;
        calc_ideal_img_size(&preferred_width, &preferred_height,
                            (ui->visualizerEnabled ? ui->visualizer_height : 0),
                            metadata_height);
}

void print_help(void)
{
        int i = system("man kew");

        if (i != 0) {
                printf(_("Run man kew for help.\n"));
        }
}

static const char *get_player_status_icon(void)
{
        if (is_paused()) {
#if defined(__ANDROID__) || defined(__APPLE__)
                return "။";
#else
                return "⏸";
#endif
        }
        if (is_stopped())
                return "■";
        return "▶";
}

int print_logo_art(int row, int col, const UISettings *ui, bool centered, bool print_tag_line, bool use_gradient)
{
        int h, w;

        get_term_size(&w, &h);

        int centered_col = (w - LOGO_WIDTH) / 2;
        if (centered_col < 0)
                centered_col = 0;

        size_t logoHeight = sizeof(LOGO) / sizeof(LOGO[0]);

        col = centered ? centered_col : col;

        for (size_t i = 0; i < logoHeight; i++) {
                unsigned char default_color = ui->default_color;

                PixelData row_color = {ui->color.r, ui->color.g, ui->color.b};

                if (use_gradient && !(ui->color.r == default_color &&
                                      ui->color.g == default_color &&
                                      ui->color.b == default_color)) {
                        row_color = get_gradient_color(ui->color, logoHeight - i,
                                                       logoHeight, 2, 0.8f);
                }

                apply_color(ui->colorMode, ui->theme.logo, row_color);

                printf("\033[%d;%dH", row, col);
                printf("%s", LOGO[i]);
                row++;
        }

        if (print_tag_line) {
                printf("\033[%d;%dH", row++, col);
                printf("MUSIC  FOR  THE  SHELL");
                logoHeight += 2;
        }

        return logoHeight; // lines used by logo
}

static void build_song_title(const SongData *song_data, const UISettings *ui,
                             char *out, size_t out_size, bool show_play_icon)
{
        const char *icon = get_player_status_icon();

        if (!song_data || !song_data->metadata) {
                out[0] = '\0'; //snprintf(out, out_size, "%*s%s", indent, "", icon);
                return;
        }

        char pretty_title[METADATA_MAX_LENGTH] = {0};
        snprintf(pretty_title, METADATA_MAX_LENGTH, "%s",
                 song_data->metadata->title);
        trim(pretty_title, strlen(pretty_title));

        if (ui->hideLogo && song_data->metadata->artist[0] != '\0') {
                snprintf(out, out_size, "%s %s - %s", icon,
                         song_data->metadata->artist, pretty_title);
        } else if (ui->hideLogo || show_play_icon) {
                snprintf(out, out_size, "%s %s", icon, pretty_title);
        } else {
                strncpy(out, pretty_title, out_size - 1);
                out[out_size - 1] = '\0';
        }
}

void print_now_playing(SongData *song_data, UISettings *ui, int row, int col, int max_width)
{
        char title[PATH_MAX + 1];

        build_song_title(song_data, ui, title, sizeof(title), true);

        apply_color(ui->colorMode, ui->theme.nowplaying, ui->color);

        clear_rest_of_line();

        if (title[0] != '\0') {
                char processed[PATH_MAX + 1] = {0};
                process_name(title, processed, max_width, false, false);

                printf("\033[%d;%dH", row, col);
                printf("%s", processed);
        }
}

// FIXME duplicated code print_logo_art and print_logo_art_for_version
int print_logo_art_for_version(const UISettings *ui, int indent, bool centered, bool print_tag_line, bool use_gradient)
{
        int h, w;

        get_term_size(&w, &h);

        int centered_col = (w - LOGO_WIDTH) / 2;
        if (centered_col < 0)
                centered_col = 0;

        size_t logoHeight = sizeof(LOGO) / sizeof(LOGO[0]);

        int col = centered ? centered_col : indent;

        for (size_t i = 0; i < logoHeight; i++) {
                unsigned char default_color = ui->default_color;

                PixelData row_color = {ui->color.r, ui->color.g, ui->color.b};

                if (use_gradient && !(ui->color.r == default_color &&
                                      ui->color.g == default_color &&
                                      ui->color.b == default_color)) {
                        row_color = get_gradient_color(ui->color, logoHeight - i,
                                                       logoHeight, 2, 0.8f);
                }

                apply_color(ui->colorMode, ui->theme.logo, row_color);

                clear_line();
                print_blank_spaces(col);
                printf("%s", LOGO[i]);
        }

        if (print_tag_line) {
                printf("\n");
                print_blank_spaces(col);
                printf("MUSIC  FOR  THE  SHELL\n");
        }

        return logoHeight; // lines used by logo
}

// FIXME duplicated code print_logo and print_logo_for_version
int print_logo_for_version(SongData *song_data, UISettings *ui)
{
        int term_w, term_h;

        get_term_size(&term_w, &term_h);

        int logo_width = (ui->hideLogo == false) ? LOGO_WIDTH : 0;
        int max_width = term_w - indent - 4 - logo_width - indent;
        int height = 2;

        if (ui->hideLogo) {
                printf("\n");
                clear_line();
        } else {
                height = print_logo_art_for_version(ui, indent + 2, false, false, true);
        }

        print_now_playing(song_data, ui, height, indent + logo_width + 4, max_width);

        printf("\n");
        clear_line();

        printf("\n");
        clear_line();

        return height + 1;
}

int print_logo(SongData *song_data, UISettings *ui, int row, int col)
{
        int term_w, term_h;

        printf("\033[%d;1H", row);

        get_term_size(&term_w, &term_h);

        int logo_width = (ui->hideLogo == false) ? LOGO_WIDTH : 0;
        int max_width = term_w - indent - 4 - logo_width - indent;
        int height = 2;

        printf("\033[%d;%dH", row, col);

        if (ui->hideLogo) {
                clear_rest_of_line();
                printf("\033[%d;%dH", row++, col);
                clear_rest_of_line();
        } else {
                height = print_logo_art(row, col, ui, false, false, true);
        }

        print_now_playing(song_data, ui, height, col + logo_width + 1, max_width);

        row += height;

        printf("\033[%d;%dH", row++, col);
        clear_rest_of_line();

        printf("\033[%d;%dH", row++, col);
        clear_rest_of_line();

        return height + 1;
}

// FIXME duplicated code: print_about, print_about_for_version
int print_about_for_version(SongData *songdata)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        clear_line();
        int num_rows = print_logo_for_version(songdata, ui);

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        print_blank_spaces(indent);
        printf(_("   kew version: "));
        apply_color(ui->colorMode, ui->theme.help, ui->color);
        printf("%s\n", ui->VERSION);
        clear_line();
        printf("\n");
        num_rows += 2;

        return num_rows;
}

int get_year(const char *date_string)
{
        int year;

        if (sscanf(date_string, "%d", &year) != 1) {
                return -1;
        }
        return year;
}

void print_cover(int row, int col, int target_height,
                 bool centered,
                 SongData *songdata)
{
        AppState *state = get_app_state();

        if (songdata != NULL && songdata->cover != NULL && state->uiSettings.coverEnabled) {

                if (!state->uiSettings.coverAnsi) {
                        print_square_bitmap(row, col,
                                            songdata->cover,
                                            songdata->coverWidth,
                                            songdata->coverHeight,
                                            target_height,
                                            centered);
                } else {
                        print_in_ascii(row, col, songdata->cover_art_path,
                                       target_height, centered);
                }

        } else {
                if (centered) {
                        for (int i = 0; i <= target_height; ++i)
                                printf("\n");
                }
        }
}

void print_title_with_delay(int row, int col, const char *text, int delay,
                            int max_width)
{
        int max = strnlen(text, max_width);

        if (max == max_width) // For long names
                max -= 2;     // Accommodate for the cursor that we display after
                              // the name.

        for (int i = 0; i <= max && delay; i++) {
                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();

                for (int j = 0; j < i; j++) {
                        printf("%c", text[j]);
                }
                printf("█");
                fflush(stdout);

                c_sleep(delay);
        }
        if (delay)
                c_sleep(delay * 20);

        printf("\033[%d;%dH", row, col);
        clear_rest_of_line();
        printf("%s", text);
        printf("\n");
        fflush(stdout);
}

void print_metadata(int row, int col, int max_width,
                    TagSettings const *metadata, UISettings *ui)
{
        if (row < 1)
                row = 1;

        if (col < 1)
                col = 1;

        if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0) {
                apply_color(ui->colorMode, ui->theme.trackview_artist,
                            ui->color);
                printf("\033[%d;%dH", row + 1, col);
                clear_rest_of_line();
                printf(" %.*s", max_width, metadata->artist);
        }

        if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0) {
                apply_color(ui->colorMode, ui->theme.trackview_album, ui->color);
                printf("\033[%d;%dH", row + 2, col);
                clear_rest_of_line();
                printf(" %.*s", max_width, metadata->album);
        }

        if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0) {
                apply_color(ui->colorMode, ui->theme.trackview_year, ui->color);
                printf("\033[%d;%dH", row + 3, col);
                clear_rest_of_line();
                int year = get_year(metadata->date);
                if (year == -1)
                        printf(" %s", metadata->date);
                else
                        printf(" %d", year);
        }

        PixelData pixel = increase_luminosity(ui->color, 20);

        if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255) {
                unsigned char default_color = ui->default_color;

                pixel.r = default_color;
                pixel.g = default_color;
                pixel.b = default_color;
        }

        apply_color(ui->colorMode, ui->theme.trackview_title, pixel);

        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0) {
                // Clean up title before printing
                char pretty_title[PATH_MAX + 1];
                pretty_title[0] = '\0';

                process_name(metadata->title, pretty_title, max_width, false,
                             false);

                print_title_with_delay(row, col + 1, pretty_title, ui->titleDelay,
                                       max_width);
        }
}

int calc_elapsed_bars(double elapsed_seconds, double duration, int num_progress_bars)
{
        if (elapsed_seconds == 0)
                return 0;

        return (int)((elapsed_seconds / duration) * num_progress_bars);
}

void print_progress(double elapsed_seconds, double total_seconds,
                    ma_uint32 sample_rate, int avg_bit_rate, int allowed_width)
{
        int progress_width = 39;

        if (allowed_width < progress_width)
                return;

        int elapsed_hours = (int)(elapsed_seconds / 3600);
        int elapsed_minutes = (int)(((int)elapsed_seconds / 60) % 60);
        int elapsed_seconds_remainder = (int)elapsed_seconds % 60;

        int total_hours = (int)(total_seconds / 3600);
        int total_minutes = (int)(((int)total_seconds / 60) % 60);
        int total_seconds_remainder = (int)total_seconds % 60;

        int progress_percentage =
            (int)((elapsed_seconds / total_seconds) * 100);
        int vol = get_volume();

        if (total_seconds >= 3600) {
                // Song is more than 1 hour long: use full HH:MM:SS format
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                       elapsed_hours, elapsed_minutes,
                       elapsed_seconds_remainder, total_hours, total_minutes,
                       total_seconds_remainder, progress_percentage, vol);
        } else {
                // Song is less than 1 hour: use M:SS format
                int elapsed_total_minutes = elapsed_seconds / 60;
                int elapsed_secs = (int)elapsed_seconds % 60;
                int total_total_minutes = total_seconds / 60;
                int total_secs = (int)total_seconds % 60;

                printf(" %d:%02d / %d:%02d (%d%%) Vol:%d%%",
                       elapsed_total_minutes, elapsed_secs, total_total_minutes,
                       total_secs, progress_percentage, vol);
        }

        double rate = ((float)sample_rate) / 1000;

        if (allowed_width > progress_width + 10) {
                if (rate == (int)rate)
                        printf(" %dkHz", (int)rate);
                else
                        printf(" %.1fkHz", rate);
        }

        if (allowed_width > progress_width + 19) {
                if (avg_bit_rate > 0)
                        printf(" %dkb/s ", avg_bit_rate);
        }
}

void print_time(int row, int col, double elapsed_seconds, ma_uint32 sample_rate,
                int avg_bit_rate, int allowed_width)
{
        AppState *state = get_app_state();

        if (state->uiSettings.hideTimeStatus)
                return;

        apply_color(state->uiSettings.colorMode,
                    state->uiSettings.theme.trackview_time,
                    state->uiSettings.color);

        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        printf("\033[%d;%dH", row, col);

        if (term_h > min_height) {
                double duration = get_current_song_duration();
                double elapsed = elapsed_seconds;

                print_progress(elapsed, duration, sample_rate, avg_bit_rate, allowed_width);
                clear_rest_of_line();
        }
}

int calc_indent_normal(void)
{
        int text_width = (ABSOLUTE_MIN_WIDTH > preferred_width)
                             ? ABSOLUTE_MIN_WIDTH
                             : preferred_width;
        return get_indentation(text_width - 1) - 1;
}

int calc_indent_track_view(TagSettings *metadata)
{
        if (metadata == NULL)
                return calc_indent_normal();

        int title_length = strnlen(metadata->title, METADATA_MAX_LENGTH);
        int album_length = strnlen(metadata->album, METADATA_MAX_LENGTH);
        int max_text_length =
            (album_length > title_length) ? album_length : title_length;
        text_width = (ABSOLUTE_MIN_WIDTH > preferred_width) ? ABSOLUTE_MIN_WIDTH
                                                            : preferred_width;
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int max_size = term_w - 2;
        if (max_text_length > 0 && max_text_length < max_size &&
            max_text_length > text_width)
                text_width = max_text_length;
        if (text_width > max_size)
                text_width = max_size;

        return get_indentation(text_width - 1) - 1;
}

void calc_indent(SongData *songdata)
{
        AppState *state = get_app_state();

        if ((state->currentView == TRACK_VIEW && songdata == NULL) ||
            state->currentView != TRACK_VIEW) {
                indent = calc_indent_normal();
        } else {
                indent = calc_indent_track_view(songdata->metadata);
        }
}

int get_indent()
{
        return indent;
}

void print_glimmering_text(int row, int col, char *text, int text_length,
                           char *nerd_font_text, PixelData color)
{
        int bright_index = 0;
        PixelData vbright = increase_luminosity(color, 120);
        PixelData bright = increase_luminosity(color, 60);

        printf("\033[%d;%dH", row, col);

        clear_rest_of_line();

        while (bright_index < text_length) {
                for (int i = 0; i < text_length; i++) {
                        if (i == bright_index) {
                                set_text_color_RGB(vbright.r, vbright.g,
                                                   vbright.b);
                                printf("%c", text[i]);
                        } else if (i == bright_index - 1 || i == bright_index + 1) {
                                set_text_color_RGB(bright.r, bright.g, bright.b);
                                printf("%c", text[i]);
                        } else {
                                set_text_color_RGB(color.r, color.g, color.b);
                                printf("%c", text[i]);
                        }

                        fflush(stdout);
                        c_usleep(50);
                }
                printf("%s", nerd_font_text);
                fflush(stdout);
                c_usleep(50);

                bright_index++;
                printf("\033[%d;%dH", row, col);
        }
}

void print_error_row(int row, int col, UISettings *ui)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        printf("\033[%d;%dH", row, col);

        if (!has_printed_error_message() && has_error_message()) {
                apply_color(ui->colorMode, ui->theme.footer, footer_color);
                printf(" %s", get_error_message());
                mark_error_message_as_printed();
        }

        clear_rest_of_line();
        fflush(stdout);
}

bool is_ascii_only(const char *text)
{
        if (text == NULL)
                return false; // or true, depending on how you want to handle NULL

        for (const char *p = text; *p; p++) {
                if ((unsigned char)*p >= 128) {
                        return false;
                }
        }

        return true;
}

void print_footer(int row, int col)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        if (preferred_width < 0 || preferred_height < 0) // mini view
                return;

        if (ui->hideFooter)
                return;

        footer_row = row;
        footer_col = col;

        printf("\033[%d;%dH", row, col);

        PixelData f_color;
        f_color.r = footer_color.r;
        f_color.g = footer_color.g;
        f_color.b = footer_color.b;

        apply_color(ui->colorMode, ui->theme.footer, f_color);

        if (ui->themeIsSet && ui->theme.footer.type == COLOR_TYPE_RGB) {
                f_color.r = ui->theme.footer.rgb.r;
                f_color.g = ui->theme.footer.rgb.g;
                f_color.b = ui->theme.footer.rgb.b;
        }

        char text[100];
        get_footer_text(text, sizeof(text));

        char icons_text[100] = "";

        size_t max_length = sizeof(icons_text);

        size_t currentLength = strnlen(icons_text, max_length);

#ifndef __ANDROID__
        if (term_w >= ABSOLUTE_MIN_WIDTH) {
#endif
                if (is_paused()) {
#if defined(__ANDROID__) || defined(__APPLE__)
                        char pause_text[] = " ။";
#else
                char pause_text[] = " ⏸";
#endif
                        snprintf(icons_text + currentLength,
                                 max_length - currentLength, "%s", pause_text);
                        currentLength += strlen(pause_text);
                } else if (is_stopped()) {
                        char pause_text[] = " ■";
                        snprintf(icons_text + currentLength,
                                 max_length - currentLength, "%s", pause_text);
                        currentLength += strlen(pause_text);
                } else {
                        char pause_text[] = " ▶";
                        snprintf(icons_text + currentLength,
                                 max_length - currentLength, "%s", pause_text);
                        currentLength += strlen(pause_text);
                }
#ifndef __ANDROID__
        }
#endif

        if (is_repeat_enabled()) {
                char repeat_text[] = " ↻";
                snprintf(icons_text + currentLength,
                         max_length - currentLength, "%s", repeat_text);
                currentLength += strlen(repeat_text);
        } else if (is_repeat_list_enabled()) {
                char repeat_text[] = " ↻L";
                snprintf(icons_text + currentLength,
                         max_length - currentLength, "%s", repeat_text);
                currentLength += strlen(repeat_text);
        }

        if (is_shuffle_enabled()) {
                char shuffle_text[] = " ⇄";
                snprintf(icons_text + currentLength,
                         max_length - currentLength, "%s", shuffle_text);
                currentLength += strlen(shuffle_text);
        }

        if (uis->isFastForwarding) {
                char forward_text[] = " ⇉";
                snprintf(icons_text + currentLength,
                         max_length - currentLength, "%s", forward_text);
                currentLength += strlen(forward_text);
        }

        if (uis->isRewinding) {
                char rewind_text[] = " ⇇";
                snprintf(icons_text + currentLength,
                         max_length - currentLength, "%s", rewind_text);
                currentLength += strlen(rewind_text);
        }

        if (term_w < ABSOLUTE_MIN_WIDTH) {
#ifndef __ANDROID__
                if (term_w > (int)currentLength + indent) {
                        printf("%s", icons_text); // Print just the shuffle
                                                  // and replay settings
                }
#else
                // Always try to print the footer on Android because it will
                // most likely be too narrow. We use two rows for the footer on
                // Android.
                print_blank_spaces(indent);
                printf("%.*s", term_w * 2, text);
                printf("%s", icons_text);
#endif
                clear_rest_of_line();
                return;
        }

        int text_length = strnlen(text, 100);
        int random_number = get_random_number(1, 808);

        if (random_number == 808 && !ui->hideGlimmeringText && is_ascii_only(text)) {
                print_glimmering_text(row, col, text, text_length, icons_text, f_color);
        } else {
                printf("%s", text);
                printf("%s", icons_text);
        }

        clear_rest_of_line();
}

void calc_and_print_last_row_and_error_row(void)
{
        AppState *state = get_app_state();
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

#if defined(__ANDROID__)
        // Use two rows for the footer on Android. It makes everything
        // fit even with narrow terminal widths.
        if (has_error_message())
                print_error_row(term_h - 1, indent, &(state->uiSettings));
        else
                print_footer(term_h - 1, indent);
#else
        print_error_row(term_h - 1, indent, &(state->uiSettings));
        print_footer(term_h, indent + 1);
#endif
}

int print_about(SongData *songdata)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        clear_line();
        int col = indent + (indent / 4);
        if (ui->hideSideCover)
                col = indent;
        int row = print_logo(songdata, ui, 1, col + 2);

        printf("\033[%d;%dH", ++row, col);
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printf(_("   kew version: "));
        apply_color(ui->colorMode, ui->theme.help, ui->color);
        printf("%s", ui->VERSION);
        clear_rest_of_line();
        printf("\033[%d;%dH", ++row, col);
        clear_rest_of_line();

        return row;
}

#define CHECK_LIST_LIMIT()                                       \
        do {                                                     \
                if (num_printed_rows > max_list_size) {          \
                        calc_and_print_last_row_and_error_row(); \
                        return num_printed_rows;                 \
                }                                                \
        } while (0)

int show_key_bindings(SongData *songdata)
{
        AppState *state = get_app_state();
        int num_printed_rows = 0;
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_list_size = term_h - 3;

        UISettings *ui = &(state->uiSettings);

        num_printed_rows += print_about(songdata);

        CHECK_LIST_LIMIT();
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);

        int indentation = indent + (indent / 4) + 1;
        if (ui->hideSideCover)
                indentation = indent + 1;

        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printf(_(" Theme: "));

        if (ui->colorMode == COLOR_MODE_ALBUM) {
                apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf(_("Using "));
                apply_color(ui->colorMode, ui->theme.text, ui->color);
                printf(_("Colors "));
                apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf(_("From Track Covers"));
        } else {
                apply_color(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_name);
        }

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        if (ui->colorMode != COLOR_MODE_ALBUM && strcmp(ui->theme.theme_author, "Ravachol") != 0) {
                printf(_(" Author: "));
                apply_color(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_author);
        }
        num_printed_rows += 2;
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", num_printed_rows, indentation + 1);
        printf(_(" · Play/Pause: %s"), get_binding_string(EVENT_PLAY_PAUSE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Enqueue/Dequeue: %s"), get_binding_string(EVENT_ENQUEUE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Enqueue and Play: %s"), get_binding_string(EVENT_ENQUEUEANDPLAY, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Quit: %s"), get_binding_string(EVENT_QUIT, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Switch tracks: %s"),
               get_binding_string(EVENT_PREV, false));
        printf(_(" and %s"),
               get_binding_string(EVENT_NEXT, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Volume: %s "), get_binding_string(EVENT_VOLUME_UP, false));
        printf(_("and %s"), get_binding_string(EVENT_VOLUME_DOWN, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Scroll: %s"), get_binding_string(EVENT_PREVPAGE, false));
        printf(_(", %s"), get_binding_string(EVENT_NEXTPAGE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Clear List: %s"), get_binding_string(EVENT_CLEARPLAYLIST, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Remove from playlist: %s"), get_binding_string(EVENT_REMOVE, true));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Move songs: %s"), get_binding_string(EVENT_MOVESONGUP, true));
        printf(_("/%s"), get_binding_string(EVENT_MOVESONGDOWN, true));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Change View: %s or "), get_binding_string(EVENT_NEXTVIEW, false));

        printf("%s, ", get_binding_string(EVENT_SHOWPLAYLIST, true));
        printf("%s, ", get_binding_string(EVENT_SHOWLIBRARY, true));
        printf("%s, ", get_binding_string(EVENT_SHOWTRACK, true));
        printf("%s, ", get_binding_string(EVENT_SHOWSEARCH, true));
        printf("%s", get_binding_string(EVENT_SHOWHELP, true));

        printf(_(" or click the footer"));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(
            _(" · Cycle Color Mode: %s (default theme, theme or cover colors)"),
            get_binding_string(EVENT_CYCLECOLORMODE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Cycle Themes: %s"), get_binding_string(EVENT_CYCLETHEMES, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Cycle Chroma Visualization: %s (requires Chroma)"), get_binding_string(EVENT_CYCLEVISUALIZATION, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Stop: %s"), get_binding_string(EVENT_STOP, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Update Library: %s"), get_binding_string(EVENT_UPDATELIBRARY, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Sort Library: %s"), get_binding_string(EVENT_SORTLIBRARY, true));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Toggle Visualizer: %s"), get_binding_string(EVENT_TOGGLEVISUALIZER, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Toggle ASCII Cover: %s (disables Chroma)"), get_binding_string(EVENT_TOGGLEASCII, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Toggle Lyrics Page on Track View: %s"), get_binding_string(EVENT_SHOWLYRICSPAGE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Toggle Notifications: %s"), get_binding_string(EVENT_TOGGLENOTIFICATIONS, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Cycle Repeat: %s (repeat/repeat list/off)"),
               get_binding_string(EVENT_TOGGLEREPEAT, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Shuffle: %s"), get_binding_string(EVENT_SHUFFLE, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Seek: %s and"), get_binding_string(EVENT_SEEKBACK, false));
        printf(_(" %s"), get_binding_string(EVENT_SEEKFORWARD, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Export Playlist: %s (named after the first song)"),
               get_binding_string(EVENT_EXPORTPLAYLIST, false));
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        printf(_(" · Add Song To 'kew favorites.m3u': %s (run with 'kew .')"),
               get_binding_string(EVENT_ADDTOFAVORITESPLAYLIST, false));
        num_printed_rows += 2;
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", num_printed_rows, indentation + 1);
        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(_(" Project URL: "));
        apply_color(ui->colorMode, ui->theme.link, ui->color);
        printf("https://codeberg.org/ravachol/kew");
        CHECK_LIST_LIMIT();
        printf("\033[%d;%dH", ++num_printed_rows, indentation + 1);
        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(_(" Please Donate: "));
        apply_color(ui->colorMode, ui->theme.link, ui->color);
        printf("https://ko-fi.com/ravachol");
        num_printed_rows += 2;
        CHECK_LIST_LIMIT();
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printf("\033[%d;%dH", num_printed_rows, indentation + 1);
        printf(" Copyright © 2022-2026 Ravachol\n");

        num_printed_rows += 1;
        CHECK_LIST_LIMIT();

        calc_and_print_last_row_and_error_row();

        num_printed_rows += 2;

        return num_printed_rows;
}

void toggle_show_view(ViewState view_to_show)
{
        AppState *state = get_app_state();
        trigger_refresh();

        if (state->currentView == TRACK_VIEW)
                clear_screen();

        if (state->currentView == view_to_show) {
                state->currentView = TRACK_VIEW;
        } else {
                state->currentView = view_to_show;
        }
}

void switch_to_next_view(void)
{
        AppState *state = get_app_state();

        switch (state->currentView) {
        case PLAYLIST_VIEW:
                state->currentView = LIBRARY_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView =
                    (get_current_song() != NULL) ? TRACK_VIEW : SEARCH_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = SEARCH_VIEW;
                clear_screen();
                break;
        case SEARCH_VIEW:
                state->currentView = HELP_VIEW;
                break;
        case HELP_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        }

        trigger_refresh();
}

void switch_to_previous_view(void)
{
        AppState *state = get_app_state();

        switch (state->currentView) {
        case PLAYLIST_VIEW:
                state->currentView = HELP_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = LIBRARY_VIEW;
                clear_screen();
                break;
        case SEARCH_VIEW:
                state->currentView =
                    (get_current_song() != NULL) ? TRACK_VIEW : LIBRARY_VIEW;
                break;
        case HELP_VIEW:
                state->currentView = SEARCH_VIEW;
                break;
        }

        trigger_refresh();
}

void show_track(void)
{
        AppState *state = get_app_state();

        trigger_refresh();

        state->currentView = TRACK_VIEW;
}

void flip_next_page(void)
{
        AppState *state = get_app_state();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (state->currentView == LIBRARY_VIEW) {
                chosen_lib_row += max_lib_list_size - 1;
                start_lib_iter += max_lib_list_size - 1;
                trigger_refresh();
        } else if (state->currentView == PLAYLIST_VIEW) {
                chosen_row += max_list_size - 1;
                chosen_row = (chosen_row >= unshuffled_playlist->count)
                                 ? unshuffled_playlist->count - 1
                                 : chosen_row;
                trigger_refresh();
        } else if (state->currentView == SEARCH_VIEW) {
                if (!is_at_last_row()) {
                        chosen_search_result_row += max_search_list_size - 1;
                        chosen_search_result_row =
                            (chosen_search_result_row >= get_search_results_count())
                                ? get_search_results_count() - 1
                                : chosen_search_result_row;
                        trigger_refresh();
                }
        }
}

void flip_prev_page(void)
{
        AppState *state = get_app_state();

        if (state->currentView == LIBRARY_VIEW) {
                chosen_lib_row -= max_lib_list_size;
                start_lib_iter -= max_lib_list_size;
                trigger_refresh();
        } else if (state->currentView == PLAYLIST_VIEW) {
                chosen_row -= max_list_size;
                chosen_row = (chosen_row > 0) ? chosen_row : 0;
                trigger_refresh();
        } else if (state->currentView == SEARCH_VIEW) {
                chosen_search_result_row -= max_search_list_size;
                chosen_search_result_row =
                    (chosen_search_result_row > 0) ? chosen_search_result_row : 0;
                trigger_refresh();
        }
}

void scroll_next(void)
{
        AppState *state = get_app_state();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (state->currentView == PLAYLIST_VIEW) {
                chosen_row++;
                chosen_row = (chosen_row >= unshuffled_playlist->count)
                                 ? unshuffled_playlist->count - 1
                                 : chosen_row;
                trigger_refresh();
        } else if (state->currentView == LIBRARY_VIEW) {
                previous_chosen_lib_row = chosen_lib_row;
                chosen_lib_row++;
                trigger_refresh();
        } else if (state->currentView == SEARCH_VIEW) {
                state->uiState.previous_chosen_search_row = chosen_search_result_row;

                if (!is_at_last_row()) {
                        chosen_search_result_row++;
                        trigger_refresh();
                }
        }
}

void scroll_prev(void)
{
        AppState *state = get_app_state();

        if (state->currentView == PLAYLIST_VIEW) {
                chosen_row--;
                chosen_row = (chosen_row > 0) ? chosen_row : 0;
                trigger_refresh();
        } else if (state->currentView == LIBRARY_VIEW) {
                previous_chosen_lib_row = chosen_lib_row;
                chosen_lib_row--;
                trigger_refresh();
        } else if (state->currentView == SEARCH_VIEW) {
                chosen_search_result_row--;
                chosen_search_result_row =
                    (chosen_search_result_row > 0) ? chosen_search_result_row : 0;
                trigger_refresh();
        }
}

int get_row_within_bounds(int row)
{
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (row >= unshuffled_playlist->count) {
                row = unshuffled_playlist->count - 1;
        }

        if (row < 0)
                row = 0;

        return row;
}

int print_logo_and_adjustments(SongData *song_data, int term_width, UISettings *ui,
                               int indentation)
{
        int about_rows = print_logo(song_data, ui, 1, indentation + 2);

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        int row = about_rows;
        int col = indentation + 1;

        if (term_width > 52 && !ui->hideHelp) {
                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();
                printf("\033[%d;%dH", ++row, col + 1);
                printf(_(" Select:↑/↓ or k/j."));
                printf(_(" Accept:%s."), get_binding_string(EVENT_ENQUEUE, true));
                printf(_(" Clear:%s."), get_binding_string(EVENT_CLEARPLAYLIST, true));
                printf("\033[%d;%dH", ++row, col);
                clear_rest_of_line();
                printf("\033[%d;%dH", row, col + 1);
#ifndef __APPLE__
                printf(_(" Scroll:PgUp/PgDn."));
#else
                printf(_(" Scroll:Fn+↑/↓."));
#endif
                printf(_(" Remove:%s."), get_binding_string(EVENT_REMOVE, true));
                printf(_(" Move songs:%s"), get_binding_string(EVENT_MOVESONGUP, true));
                printf(_("/%s."), get_binding_string(EVENT_MOVESONGDOWN, true));
                printf("\033[%d;%dH", ++row, col);
                clear_rest_of_line();
        }

        printf("\033[%d;%dH", ++row, col);
        clear_rest_of_line();

        return row;
}

void show_search(SongData *song_data, int *chosen_row)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_search_list_size = term_h - 2;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        goto_first_line_first_row();

        int col = indent + (indent / 4);
        if (ui->hideSideCover)
                col = indent;

        int about_rows = print_logo(song_data, ui, 1, col + 2);

        max_search_list_size -= about_rows;

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        int row = about_rows;

        printf("\033[%d;%dH", row, col + 2);
        clear_rest_of_line();

        if (term_w > col + 38 && !ui->hideHelp) {
                printf("\033[%d;%dH", ++row, col + 2);
                printf(_(" Select:↑/↓."));
                printf(_(" Enqueue:%s."), get_binding_string(EVENT_ENQUEUE, true));
                printf(_(" Play:%s."), get_binding_string(EVENT_ENQUEUEANDPLAY, true));
                printf("\033[%d;%dH", ++row, col + 2);
                clear_rest_of_line();

                max_search_list_size -= 2;
        }

        printf("\033[%d;%dH", ++row, col);
        clear_rest_of_line();

        display_search(row, col, max_search_list_size, chosen_row);
        calc_and_print_last_row_and_error_row();
}

void show_playlist(SongData *song_data, PlayList *list, int *chosen_song,
                   int *chosen_node_id)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_list_size = term_h - 1;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        int update_counter = get_update_counter();

        if (get_is_long_name() && is_same_name_as_last_time &&
            update_counter % scrolling_interval != 0) {
                increment_update_counter();
                trigger_refresh();
                return;
        } else
                cancel_refresh();

        goto_first_line_first_row();

        int row = 1;
        int col = indent + (indent / 4);
        if (ui->hideSideCover)
                col = indent;

        int about_rows = print_logo_and_adjustments(song_data, term_w, ui, col);
        max_list_size -= about_rows;

        row = about_rows;

        apply_color(ui->colorMode, ui->theme.header, ui->color);

        if (max_list_size > 0) {
                printf("\033[%d;%dH", row, col + 1);
                printf(_("   ─ PLAYLIST ─"));
                clear_rest_of_line();
        }

        max_list_size -= 2;

        if (max_list_size > 0)
                display_playlist(row + 1, col + 1, list, max_list_size, chosen_song,
                                 chosen_node_id, state->uiState.resetPlaylistDisplay);

        calc_and_print_last_row_and_error_row();
}

void reset_search_result(void)
{
        chosen_search_result_row = 0;
}

void print_progress_bar(int row, int col, AppSettings *settings, UISettings *ui,
                        int elapsed_bars, int num_progress_bars)
{
        PixelData color = ui->color;

        ProgressBar *progress_bar = get_progress_bar();

        progress_bar->row = row;
        progress_bar->col = col + 1;
        progress_bar->length = num_progress_bars;

        printf("\033[%d;%dH", row, col);
        printf(" ");

        for (int i = 0; i < num_progress_bars; i++) {
                if (i > elapsed_bars) {
                        if (ui->colorMode == COLOR_MODE_ALBUM) {
                                PixelData tmp = increase_luminosity(color, 50);
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g,
                                       tmp.b);

                                apply_color(ui->colorMode,
                                            ui->theme.progress_empty, tmp);
                        } else {
                                apply_color(ui->colorMode,
                                            ui->theme.progress_empty, color);
                        }

                        if (i % 2 == 0)
                                printf(
                                    "%s",
                                    settings->progressBarApproachingEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarApproachingOddChar);

                        continue;
                }

                if (i < elapsed_bars) {
                        apply_color(ui->colorMode, ui->theme.progress_filled, color);

                        if (i % 2 == 0)
                                printf("%s",
                                       settings->progressBarElapsedEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarElapsedOddChar);
                } else if (i == elapsed_bars) {
                        apply_color(ui->colorMode, ui->theme.progress_elapsed, color);

                        if (i % 2 == 0)
                                printf("%s",
                                       settings->progressBarCurrentEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarCurrentOddChar);
                }
        }
        clear_rest_of_line();
}

void print_visualizer(int row, int col, int visualizer_width,
                      AppSettings *settings, double elapsed_seconds)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        int height = state->uiSettings.visualizer_height;

        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        if (row + height + 2 > term_h)
                height -= (row + height + 1 - term_h);

        if (height < 2)
                return;

        if (ui->visualizerEnabled) {
                uis->num_progress_bars = (int)visualizer_width / 2;
                double duration = get_current_song_duration();

                draw_spectrum_visualizer(row, col, height, visualizer_width);

                int elapsed_bars =
                    calc_elapsed_bars(elapsed_seconds, duration, visualizer_width);

                print_progress_bar(row + height - 1, col, settings, ui,
                                   elapsed_bars, visualizer_width - 1);
        }
}

FileSystemEntry *get_chosen_dir(void)
{
        return chosen_dir;
}

void set_chosen_dir(FileSystemEntry *entry)
{
        AppState *state = get_app_state();

        if (entry == NULL) {
                return;
        }

        if (entry->is_directory) {
                state->uiState.currentLibEntry = chosen_dir = entry;
        }
}

void reset_chosen_dir(void)
{
        chosen_dir = NULL;
}

void apply_tree_item_color(UISettings *ui, int depth,
                           PixelData track_color, PixelData enqueued_color,
                           bool is_enqueued, bool is_playing)
{
        if (depth <= 1) {
                apply_color(ui->colorMode, ui->theme.library_artist, enqueued_color);
        } else {
                if (ui->colorMode == COLOR_MODE_ALBUM || (ui->colorMode == COLOR_MODE_THEME && ui->theme.library_track.type == COLOR_TYPE_RGB))
                        apply_color(COLOR_MODE_ALBUM, ui->theme.library_track,
                                    track_color);
                else
                        apply_color(ui->colorMode, ui->theme.library_track,
                                    track_color);
        }

        if (is_enqueued) {
                if (is_playing) {
                        apply_color(ui->colorMode, ui->theme.library_playing,
                                    ui->color);
                } else {
                        if (ui->colorMode == COLOR_MODE_ALBUM || (ui->colorMode == COLOR_MODE_THEME && ui->theme.library_enqueued.type == COLOR_TYPE_RGB))
                                apply_color(COLOR_MODE_ALBUM, ui->theme.library_enqueued,
                                            enqueued_color);
                        else
                                apply_color(ui->colorMode, ui->theme.library_enqueued,
                                            enqueued_color);
                }
        }
}

int display_tree(FileSystemEntry *root, int depth, int max_list_size,
                 int max_name_width, int row, int col)
{
        if (max_name_width < 0)
                max_name_width = 0;

        char dir_name[max_name_width + 1];
        char filename[PATH_MAX + 1];
        bool foundChosen = false;
        int is_playing = 0;
        int extra_indent = 0;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        Node *current = get_current_song();

        if (current != NULL &&
            (strcmp(current->song.file_path, root->full_path) == 0)) {
                is_playing = 1;
        }

        if (start_lib_iter < 0)
                start_lib_iter = 0;

        if (lib_iter >= start_lib_iter + max_list_size) {
                return false;
        }

        int threshold = start_lib_iter + (max_list_size + 1) / 2;
        if (chosen_lib_row > threshold) {
                start_lib_iter = chosen_lib_row - max_list_size / 2 + 1;
        }

        if (chosen_lib_row < 0)
                start_lib_iter = chosen_lib_row = lib_iter = 0;

        if (root == NULL)
                return false;

        PixelData row_color = {ui->default_color, ui->default_color, ui->default_color};
        PixelData row_color2 = {ui->default_color, ui->default_color, ui->default_color};
        PixelData rgb_track = {ui->default_color, ui->default_color, ui->default_color};
        PixelData rgb_enqueued = {ui->default_color, ui->default_color, ui->default_color};

        if (ui->colorMode == COLOR_MODE_THEME &&
            ui->theme.library_track.type == COLOR_TYPE_RGB) {
                rgb_track = ui->theme.library_track.rgb;
                rgb_enqueued = ui->theme.library_enqueued.rgb;
        } else {
                rgb_enqueued = ui->color;
        }

        if (!(rgb_track.r == ui->default_color &&
              rgb_track.g == ui->default_color &&
              rgb_track.b == ui->default_color))
                row_color = get_gradient_color(rgb_track, lib_iter - start_lib_iter,
                                               max_list_size,
                                               max_list_size / 2, 0.7f);

        if (!(rgb_enqueued.r == ui->default_color &&
              rgb_enqueued.g == ui->default_color &&
              rgb_enqueued.b == ui->default_color))
                row_color2 = get_gradient_color(rgb_enqueued, lib_iter - start_lib_iter,
                                                max_list_size,
                                                max_list_size / 2, 0.7f);

        if (!(root->is_directory || (!root->is_directory && depth == 1) ||
              (root->is_directory && depth == 0) ||
              (chosen_dir != NULL && uis->allowChooseSongs &&
               root->parent != NULL &&
               (strcmp(root->parent->full_path, chosen_dir->full_path) == 0 ||
                strcmp(root->full_path, chosen_dir->full_path) == 0)))) {
                return foundChosen;
        }

        if (depth >= 0) {
                if (state->uiState.currentLibEntry != NULL && state->uiState.currentLibEntry != last_entry &&
                    !state->uiState.currentLibEntry->is_directory &&
                    state->uiState.currentLibEntry->parent != NULL &&
                    state->uiState.currentLibEntry->parent->parent != NULL &&
                    state->uiState.currentLibEntry->parent == chosen_dir) {
                        FileSystemEntry *tmpc = state->uiState.currentLibEntry->parent->children;

                        lib_current_dir_song_count = 0;

                        while (tmpc != NULL) {
                                if (!tmpc->is_directory)
                                        lib_current_dir_song_count++;
                                tmpc = tmpc->next;
                        }

                        last_entry = state->uiState.currentLibEntry;
                }

                if (lib_iter >= start_lib_iter) {
                        apply_tree_item_color(ui, depth, row_color, row_color2,
                                              root->is_enqueued, is_playing);

                        printf("\033[%d;%dH", row + lib_row_count, col);

                        clear_rest_of_line();

                        if (depth >= 2)
                                extra_indent += 2;

                        // If more than two levels deep add an extra
                        // indentation
                        extra_indent += (depth - 2 <= 0) ? 0 : depth - 2;

                        printf("\033[%d;%dH", row + lib_row_count, col + extra_indent);

                        if (chosen_lib_row == lib_iter) {
                                if (root->is_enqueued) {
                                        printf("\x1b[7m * ");
                                } else {
                                        printf("  \x1b[7m ");
                                }

                                state->uiState.currentLibEntry = root;

                                if (uis->allowChooseSongs == true &&
                                    (chosen_dir == NULL ||
                                     (state->uiState.currentLibEntry != NULL &&
                                      state->uiState.currentLibEntry->parent != NULL &&
                                      chosen_dir != NULL &&
                                      !is_contained_within(state->uiState.currentLibEntry,
                                                           chosen_dir) &&
                                      strcmp(root->full_path,
                                             chosen_dir->full_path) != 0))) {
                                        uis->collapseView = true;
                                        trigger_refresh();

                                        if (!uis->openedSubDir) {

                                                uis->allowChooseSongs = false;
                                                chosen_dir = NULL;
                                        }
                                }

                                foundChosen = true;
                        } else {
                                if (root->is_enqueued) {
                                        printf(" * ");
                                } else {
                                        printf("   ");
                                }
                        }

                        if (max_name_width < extra_indent)
                                max_name_width = extra_indent;

                        if (root->is_directory) {
                                dir_name[0] = '\0';

                                if (strcmp(root->name, "root") == 0)
                                        snprintf(dir_name,
                                                 max_name_width + 1 - extra_indent,
                                                 "%s", _("─ MUSIC LIBRARY ─"));
                                else
                                        snprintf(dir_name,
                                                 max_name_width + 1 - extra_indent,
                                                 "%s", root->name);

                                char *upper_dir_name = string_to_upper(dir_name);

                                if (depth == 1) {
                                        printf("%s", upper_dir_name);
                                } else {
                                        printf("%s", dir_name);
                                }

                                if (strcmp(dir_name, "Warlord") == 0)
                                        fflush(stdout);

                                free(upper_dir_name);
                        } else {
                                filename[0] = '\0';

                                is_same_name_as_last_time =
                                    (previous_chosen_lib_row == chosen_lib_row);

                                if (foundChosen) {
                                        previous_chosen_lib_row = chosen_lib_row;
                                }

                                if (!is_same_name_as_last_time) {
                                        reset_name_scroll();
                                }

                                if (foundChosen) {
                                        process_name_scroll(root->name, filename,
                                                            max_name_width -
                                                                extra_indent,
                                                            is_same_name_as_last_time);
                                } else {
                                        process_name(root->name, filename,
                                                     max_name_width - extra_indent,
                                                     true, true);
                                }

                                if (is_playing) {
                                        if (chosen_lib_row == lib_iter) {
                                                printf("\x1b[7m");
                                        }
                                }

                                printf("└─ ");

                                // Playlist
                                if (path_ends_with(root->full_path, "m3u") ||
                                    path_ends_with(root->full_path, "m3u8")) {
                                        printf("♫ ");
                                        max_name_width = max_name_width - 2;
                                }

                                if (is_playing && chosen_lib_row != lib_iter) {
                                        printf("\e[4m");
                                }

                                printf("%s", filename);

                                apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);
                                lib_song_iter++;
                        }
                        lib_row_count++;
                }

                lib_iter++;
        }

        FileSystemEntry *child = root->children;
        while (child != NULL) {
                if (display_tree(child, depth + 1, max_list_size, max_name_width, row, col))
                        foundChosen = true;

                child = child->next;
        }

        return foundChosen;
}

size_t string_hash(const char *str)
{
        if (str == NULL)
                return 0;
        size_t hash = 5381;
        int c;
        while ((c = *str++))
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
}

void print_side_cover(SongData *songdata)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        if (state->currentView != last_view)
                redraw_side_cover = true;

        last_view = state->currentView;

        const char *current_ptr = songdata ? songdata->cover_art_path : NULL;
        if (current_ptr != last_cover_path_ptr) {
                last_cover_path_ptr = current_ptr;
                last_cover_path_hash = current_ptr ? string_hash(current_ptr) : (size_t)-1;
                redraw_side_cover = true;
        } else if (current_ptr != NULL) {
                size_t current_hash = string_hash(current_ptr);
                if (current_hash != last_cover_path_hash) {
                        last_cover_path_hash = current_hash;
                        redraw_side_cover = true;
                }
        }

        if (state->currentView == TRACK_VIEW)
                return;

        int cover_indent = (indent + (indent / 4)) / 8;
        int row = 2;
        int col = cover_indent + 3;
        int target_height = indent - cover_indent;

        if (redraw_side_cover) {

                clear_screen();

                redraw_side_cover = false;

                if (ui->coverEnabled && term_w - 4 > indent / 2) {
                        if (row < 2)
                                row = 2;

                        if (col < 1)
                                col = 1;

                        TermSize term_size;
                        gint cell_width = -1, cell_height = -1;

                        tty_init();
                        get_tty_size(&term_size);

                        int max_height = term_h - 4;

                        if (target_height > max_height)
                                target_height = max_height;

                        if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
                            term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                                cell_width = term_size.width_pixels / term_size.width_cells;
                                cell_height = term_size.height_pixels / term_size.height_cells;
                        }

                        // Set default cell size for some terminals
                        if (cell_width == -1 || cell_height == -1) {
                                cell_width = 8;
                                cell_height = 16;
                        }

                        float aspect_ratio_correction = (float)cell_height / (float)cell_width;
                        int corrected_width = (int)(target_height * aspect_ratio_correction);

                        while (corrected_width > indent - cover_indent) {
                                target_height--;
                                corrected_width = (int)(target_height * aspect_ratio_correction);
                        }

                        row = lroundf((float)term_h / 2.0 - ((float)target_height / 2.0));

                        if (target_height > MIN_COVER_SIZE && ui->hideSideCover != 1) {
                                print_cover(row, col, target_height, false, songdata);
                        }
                }
        }
}

void show_library(SongData *song_data, AppSettings *settings)
{
        AppState *state = get_app_state();

        bool refresh_triggered = is_refresh_triggered();

        // For scrolling names, update every nth time
        if (get_is_long_name() && is_same_name_as_last_time &&
            get_update_counter() % scrolling_interval != 0) {
                trigger_refresh();
                return;
        } else
                cancel_refresh();

        goto_first_line_first_row();

        if (state->uiState.collapseView) {
                if (previous_chosen_lib_row < chosen_lib_row) {
                        if (!state->uiState.openedSubDir) {
                                chosen_lib_row -= lib_current_dir_song_count;
                                lib_current_dir_song_count = 0;
                        } else {
                                chosen_lib_row -=
                                    state->uiState.numSongsAboveSubDir;
                                state->uiState.openedSubDir = false;
                                state->uiState.numSongsAboveSubDir = 0;
                                state->uiState.collapseView = false;
                        }
                } else {
                        if (state->uiState.openedSubDir) {
                                chosen_lib_row -=
                                    state->uiState.numSongsAboveSubDir;
                        }
                        lib_current_dir_song_count = 0;
                        state->uiState.openedSubDir = false;
                        state->uiState.numSongsAboveSubDir = 0;
                }
                state->uiState.collapseView = false;
        }

        UISettings *ui = &(state->uiSettings);

        lib_iter = 0;
        lib_song_iter = 0;
        start_lib_iter = 0;

        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int total_height = term_h;
        max_lib_list_size = total_height;
        int row = 1;
        int col = indent + (indent / 4);
        if (ui->hideSideCover)
                col = indent;
        int about_size = print_logo(song_data, ui, 1, col + 2);

        int max_name_width = term_w - 10 - indent - (indent / 4);
        if (ui->hideSideCover)
                max_name_width = term_w - 10 - indent;
        max_lib_list_size -= about_size + 2;

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        row = about_size + 1;
        col += 1;

        if (term_w > 67 && !ui->hideHelp) {
                max_lib_list_size -= 3;
                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();
                printf("\033[%d;%dH", row++, col + 1);
                printf(_(" Select:↑/↓."));
                printf(_(" Enqueue/Dequeue:%s."), get_binding_string(EVENT_ENQUEUE, true));
                printf(_(" Play:%s."), get_binding_string(EVENT_ENQUEUEANDPLAY, true));
                clear_rest_of_line();
                printf("\033[%d;%dH", row++, col + 1);
                clear_rest_of_line();
#ifndef __APPLE__
                printf(_(" Scroll:PgUp/PgDn."));
#else
                printf(_(" Scroll:Fn+↑/↓."));
#endif
                printf(_(" Update:%s."), get_binding_string(EVENT_UPDATELIBRARY, true));
                printf(_(" Sort:%s."), get_binding_string(EVENT_SORTLIBRARY, true));
                clear_rest_of_line();
                printf("\033[%d;%dH", row++, col);
                clear_rest_of_line();
        }

        num_top_level_songs = 0;

        FileSystemEntry *library = get_library();

        FileSystemEntry *tmp = library->children;

        while (tmp != NULL) {
                if (!tmp->is_directory)
                        num_top_level_songs++;

                tmp = tmp->next;
        }

        bool foundChosen = false;
        lib_row_count = 0;

        if (max_lib_list_size <= 0)
                foundChosen = true;
        else
                foundChosen = display_tree(library, 0, max_lib_list_size,
                                           max_name_width, row, col);

        fflush(stdout);

        if (!foundChosen) {
                chosen_lib_row--;
                trigger_refresh();
        }

        printf("\e[0m");
        apply_color(COLOR_MODE_ALBUM, ui->theme.text,
                    ui->defaultColorRGB);

        int empty_lines = max_lib_list_size - lib_row_count;
        for (int i = 0; i < empty_lines; i++) {
                printf("\033[%d;%dH", row + lib_row_count + i, col);
                clear_rest_of_line();
        }

        calc_and_print_last_row_and_error_row();

        if (!foundChosen && is_refresh_triggered()) {
                goto_first_line_first_row();
                show_library(song_data, settings);
        } else if (refresh_triggered)
                print_side_cover(song_data);
}

int calc_visualizer_width()
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int visualizer_width = (ABSOLUTE_MIN_WIDTH > preferred_width)
                                   ? ABSOLUTE_MIN_WIDTH
                                   : preferred_width;
        visualizer_width =
            (visualizer_width < text_width && text_width < term_w - 2)
                ? text_width
                : visualizer_width;
        visualizer_width =
            (visualizer_width > term_w - 2) ? term_w - 2 : visualizer_width;
        visualizer_width -= 1;

        return visualizer_width;
}

void print_at(int row, int indent, const char *text, int max_width)
{
        char buffer[1024];
        size_t len = strlen(text);

        if (len > (size_t)max_width)
                len = max_width;

        // Safe copy of exactly len bytes
        memcpy(buffer, text, len);
        buffer[len] = '\0';

        printf("\033[%d;%dH%s", row, indent, buffer);
}

void print_lyrics_page(UISettings *ui, int row, int col, double seconds, SongData *songdata, int height)
{
        clear_rest_of_line();

        if (!songdata)
                return;

        Lyrics *lyrics = songdata->lyrics;

        if (!lyrics || lyrics->count == 0) {
                printf("\033[%d;%dH", row, col);
                printf(_(" No lyrics available. Press %s to go back."), get_binding_string(EVENT_SHOWLYRICSPAGE, true));
                return;
        }

        int offset = 0;     // start index of the visible lines
        int highlight = -1; // current highlighted line index
        if (lyrics->isTimed) {
                for (int i = 0; i < (int)lyrics->count; i++) {
                        if (lyrics->lines[i].timestamp <= seconds) {
                                highlight = i;
                        } else {
                                break;
                        }
                }
                if (highlight > height / 2) {
                        // text scrolls and highlighted line stays in the middle
                        offset = highlight - (height / 2);
                }
        }

        for (int i = offset; i < MIN((int)lyrics->count, height + offset); i++) {
                char linebuf[1024];
                const char *text = lyrics->lines[i].text ? lyrics->lines[i].text : "";
                strncpy(linebuf, text, sizeof(linebuf) - 1);
                linebuf[sizeof(linebuf) - 1] = '\0';

                int length = (int)strnlen(linebuf, songdata->lyrics->max_length - 1);
                if (length + col > term_w)
                        length = term_w - col;
                if (length < 0)
                        length = 0;

                if (highlight == i && lyrics->isTimed)
                        apply_color(ui->colorMode, ui->theme.nowplaying, ui->defaultColorRGB);
                else
                        apply_color(ui->colorMode, ui->theme.trackview_lyrics, ui->color);
                print_at(row + i - offset, col, linebuf, length);
                clear_rest_of_line();
        }
}

const char *get_lyrics_line(const Lyrics *lyrics, double elapsed_seconds)
{
        if (!lyrics || lyrics->count == 0)
                return "";

        static char line[1024];
        line[0] = '\0';

        double last_timestamp = -1.0;

        for (size_t i = 0; i < lyrics->count; i++) {
                double ts = lyrics->lines[i].timestamp;
                const char *text = lyrics->lines[i].text;

                if (elapsed_seconds < ts)
                        break;

                if (ts == last_timestamp) {
                        // Same timestamp → append
                        strncat(line, " | ", sizeof(line) - strlen(line) - 1);
                        strncat(line, text, sizeof(line) - strlen(line) - 1);
                } else {
                        // New timestamp → start fresh
                        snprintf(line, sizeof(line), "%s", text);
                        last_timestamp = ts;
                }
        }

        return line;
}

void print_timestamped_lyrics(UISettings *ui, SongData *songdata, int row, int col, int term_w, double elapsed_seconds)
{
        if (!songdata)
                return;

        if (!songdata->lyrics)
                return;

        if (songdata->lyrics->isTimed != 1)
                return;

        const char *line = get_lyrics_line(songdata->lyrics, elapsed_seconds);

        if (!line)
                return;

        static char prev_line[1024] = "";

        if (line && line[0] != '\0' && (strcmp(line, prev_line) != 0)) {
                strncpy(prev_line, line, sizeof(prev_line) - 1);
                prev_line[sizeof(prev_line) - 1] = '\0';

                int length = ((int)strnlen(line, songdata->lyrics->max_length - 1));

                length -= col + length - term_w;

                apply_color(ui->colorMode, ui->theme.trackview_lyrics, ui->color);

                print_at(row, col, line, length);

                clear_rest_of_line();
        }
}

void show_track_view_landscape(int height, int width, float aspect_ratio,
                               AppSettings *settings, SongData *songdata,
                               double elapsed_seconds)
{
        AppState *state = get_app_state();
        TagSettings *metadata = NULL;
        int avg_bit_rate = 0;

        int metadata_height = 4;
        int time_height = 1;

        if (songdata) {
                metadata = songdata->metadata;
        }

        int col = height * aspect_ratio;

        if (!state->uiSettings.coverEnabled)
                col = 1;

        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int visualizer_width = term_w - col;

        int row = height - metadata_height - time_height - state->uiSettings.visualizer_height - 3;

        if (row <= 1)
                row = 2;

        if (is_refresh_triggered()) {
                if (!chroma_is_started()) {
                        clear_screen();
                        print_cover(2, 2, height - 2, false, songdata);
                }
                if (!state->uiState.showLyricsPage) {
                        if (height > metadata_height)
                                print_metadata(row, col, visualizer_width - 1,
                                               metadata, &(state->uiSettings));
                }

                cancel_refresh();
        }
        if (songdata) {
                ma_uint32 sample_rate;
                ma_format format;
                avg_bit_rate = songdata->avg_bit_rate;
                get_format_and_sample_rate(&format, &sample_rate);

                if (!state->uiState.showLyricsPage) {

                        if (chroma_is_started()) {
                                chroma_print_frame(2, 2, false);
                        }

                        if (height > metadata_height + time_height)
                                print_time(row + 4, col, elapsed_seconds, sample_rate,
                                           avg_bit_rate, term_w - col);

                        printf("\033[%d;%dH", row + metadata_height + 1, col);
                        printf(" ");

                        if (row > 0)
                                print_timestamped_lyrics(&(state->uiSettings), songdata, row + metadata_height + 1, col + 1, term_w, elapsed_seconds);

                        if (row > 0)
                                print_visualizer(row + metadata_height + 2, col, visualizer_width,
                                                 settings, elapsed_seconds);

                        if (width - col > ABSOLUTE_MIN_WIDTH) {
                                print_error_row(row + metadata_height + 2 +
                                                    state->uiSettings.visualizer_height,
                                                col, &(state->uiSettings));
                                print_footer(row + metadata_height + 2 + state->uiSettings.visualizer_height + 1, col);
                        }
                } else {
                        print_now_playing(songdata, &(state->uiSettings), 2, col, term_w - indent);
                        print_lyrics_page(&(state->uiSettings), 4, col, elapsed_seconds, songdata, height - 4);
                        clear_rest_of_screen();
                }
        }
}

void show_track_view_portrait(int height, AppSettings *settings,
                              SongData *songdata, double elapsed_seconds)
{
        AppState *state = get_app_state();
        TagSettings *metadata = NULL;
        int avg_bit_rate = 0;

        int metadata_height = 4;

        int cover_row = 2;
        int row = height + 3;
        int col = indent;

        int visualizer_width = calc_visualizer_width();

        if (is_refresh_triggered()) {
                if (songdata) {
                        metadata = songdata->metadata;
                }

                clear_screen();

                if (!state->uiState.showLyricsPage) {

                        if (!chroma_is_started()) {
                                clear_screen();
                                print_cover(cover_row, indent, preferred_height, true, songdata);
                                printf("\n\n");
                        }
                        print_metadata(row, col, visualizer_width - 1, metadata,
                                       &(state->uiSettings));
                }

                cancel_refresh();
        }
        if (songdata) {
                if (!state->uiState.showLyricsPage) {
                        ma_uint32 sample_rate;
                        ma_format format;
                        avg_bit_rate = songdata->avg_bit_rate;

                        if (chroma_is_started()) {
                                chroma_print_frame(2, col + 1, true);
                        }

                        get_format_and_sample_rate(&format, &sample_rate);
                        print_time(row + metadata_height, col, elapsed_seconds, sample_rate,
                                   avg_bit_rate, term_w - col);

                        if (row > 0)
                                print_timestamped_lyrics(&(state->uiSettings), songdata, row + metadata_height + 1, indent + 1, term_w, elapsed_seconds);

                        print_visualizer(row + metadata_height + 2, col, visualizer_width + 1,
                                         settings, elapsed_seconds);
                } else {
                        clear_screen();
                        printf("\n");
                        print_now_playing(songdata, &(state->uiSettings), 2, indent + 1, term_w - indent);
                        int term_w, term_h;
                        get_term_size(&term_w, &term_h);
                        int lyrics_height = term_h - 5;
                        if (lyrics_height > 0)
                                print_lyrics_page(&(state->uiSettings), 4, col + 1, elapsed_seconds, songdata, lyrics_height);
                }
        }

        calc_and_print_last_row_and_error_row();
}

static int lastHeight = 0;
static time_t last_restart = 0;

void show_track_view(int width, int height, AppSettings *settings,
                     SongData *songdata, double elapsed_seconds, UISettings *ui)
{
        time_t now = time(NULL);

        bool landscape_layout = false;
        float aspect = get_aspect_ratio();

        if (aspect == 0.0f)
                aspect = 1.0f;

        int corrected_width = width / aspect;
        int cover_height = preferred_height;

        if (corrected_width > height * 2) {
                landscape_layout = true;
                cover_height = height - 2;
        }

        if (height != lastHeight && chroma_is_started() && now != last_restart) {
                last_restart = now;
                lastHeight = height;
                chroma_stop();
                clear_screen();
                chroma_start(cover_height);
        }

        if (songdata && ui->visualizations_instead_of_cover && (!chroma_is_started() || next_visualization_requested) && ui->coverEnabled) {
                if (has_chroma == -1)
                        has_chroma = chroma_is_installed();

                if (has_chroma == 1) {
                        lastHeight = height;

                        if (next_visualization_requested) {
                                clear_screen();
                                chroma_set_next_preset(cover_height);
                                trigger_refresh();
                        } else {
                                clear_screen();
                                if (ui->chromaPreset >= 0)
                                        chroma_set_current_preset(ui->chromaPreset);
                                chroma_start(cover_height);
                                trigger_refresh();
                        }

                        if (!chroma_is_started()) {
                                ui->visualizations_instead_of_cover = false;
                                trigger_refresh();
                        }
                }

                next_visualization_requested = false;
        }

        if (landscape_layout) {
                show_track_view_landscape(height, width, aspect, settings,
                                          songdata, elapsed_seconds);
        } else {
                show_track_view_portrait(preferred_height, settings, songdata,
                                         elapsed_seconds);
        }
}

int print_player(SongData *songdata, double elapsed_seconds)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (has_printed_error_message() && is_refresh_triggered())
                clear_error_message();

        if (!ui->uiEnabled) {
                return 0;
        }

        if (is_refresh_triggered()) {
                hide_cursor();

                if (songdata != NULL && songdata->metadata != NULL &&
                    !songdata->hasErrors && (songdata->hasErrors < 1)) {
                        ui->color.r = songdata->red;
                        ui->color.g = songdata->green;
                        ui->color.b = songdata->blue;

                        if (ui->trackTitleAsWindowTitle)
                                set_terminal_window_title(
                                    songdata->metadata->title);
                } else {
                        if (state->currentView == TRACK_VIEW) {
                                state->currentView = LIBRARY_VIEW;
                                clear_screen();
                        }

                        ui->color.r = ui->defaultColorRGB.r;
                        ui->color.g = ui->defaultColorRGB.g;
                        ui->color.b = ui->defaultColorRGB.b;

                        if (ui->trackTitleAsWindowTitle)
                                set_terminal_window_title("kew");
                }

                calc_preferred_size(ui);
                calc_indent(songdata);
        }

        get_term_size(&term_w, &term_h);

        bool shouldRefresh = is_refresh_triggered() || state->uiState.isFastForwarding || state->uiState.isRewinding;

        if (state->currentView != PLAYLIST_VIEW)
                state->uiState.resetPlaylistDisplay = true;

        if ((state->currentView != TRACK_VIEW || is_refresh_triggered()) && chroma_is_started())
                chroma_stop();

        print_side_cover(songdata);

        if (state->currentView == HELP_VIEW && shouldRefresh) {
                show_key_bindings(songdata);
                save_cursor_position();
                cancel_refresh();
                fflush(stdout);
        } else if (state->currentView == PLAYLIST_VIEW && shouldRefresh) {
                show_playlist(songdata, unshuffled_playlist, &chosen_row,
                              &(uis->chosen_node_id));
                state->uiState.resetPlaylistDisplay = false;
                fflush(stdout);
        } else if (state->currentView == SEARCH_VIEW && shouldRefresh) {
                cancel_refresh();
                show_search(songdata, &chosen_search_result_row);
                fflush(stdout);
        } else if (state->currentView == LIBRARY_VIEW && shouldRefresh) {
                show_library(songdata, settings);
                fflush(stdout);
        } else if (state->currentView == TRACK_VIEW) {
                show_track_view(term_w, term_h, settings, songdata, elapsed_seconds, ui);
                fflush(stdout);
        }

        return 0;
}

void show_help(void)
{
        print_help();
}

void save_library(void)
{
        FileSystemEntry *library = get_library();
        char *filepath = get_library_file_path();

        reset_sort_library();

        write_tree_to_binary(library, filepath);

        free(filepath);
}

void free_library(void)
{
        FileSystemEntry *library = get_library();

        if (library == NULL)
                return;

        free_tree(library);
}

int get_chosen_row(void)
{
        return chosen_row;
}

void set_chosen_row(int row)
{
        chosen_row = row;
}

void refresh_player()
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        int mutex_result = pthread_mutex_trylock(&(state->switch_mutex));

        if (mutex_result != 0) {
                fprintf(stderr, "Failed to lock switch mutex.\n");
                return;
        }

        if (ps->notifyPlaying) {
                ps->notifyPlaying = false;

                emit_string_property_changed("PlaybackStatus", "Playing");
        }

        if (ps->notifySwitch) {
                ps->notifySwitch = false;

                notify_mpris_switch(get_current_song_data());

                if (state->uiSettings.discordRPCEnabled)
                        notify_discord_switch(get_current_song_data());
        }

        if (state->uiSettings.discordRPCEnabled) {
                if (is_paused() && !last_paused_state) {
                        notify_discord_pause();
                } else if (!is_paused() && last_paused_state) {
                        notify_discord_resume(get_current_song_data());
                }

                last_paused_state = is_paused();
        }

        if (should_refresh_player()) {
                print_player(get_current_song_data(), get_elapsed_seconds());
        }

        pthread_mutex_unlock(&(state->switch_mutex));
}

int get_footer_text(char *restrict text, size_t size)
{
        char playlist[32], library[32], track[32], search[32], help[32];

        snprintf(playlist, sizeof(playlist), "%s", get_binding_string(EVENT_SHOWPLAYLIST, true));
        snprintf(library, sizeof(library), "%s", get_binding_string(EVENT_SHOWLIBRARY, true));
        snprintf(track, sizeof(track), "%s", get_binding_string(EVENT_SHOWTRACK, true));
        snprintf(search, sizeof(search), "%s", get_binding_string(EVENT_SHOWSEARCH, true));
        snprintf(help, sizeof(search), "%s", get_binding_string(EVENT_SHOWHELP, true));

        return snprintf(text, size,
                        _("%s Playlist|%s Library|%s Track|%s Search|%s Help"), playlist,
                        library, track, search, help);
}
