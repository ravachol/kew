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

#include "input.h"

#include "ops/library_ops.h"
#include "ops/playback_clock.h"
#include "ops/playback_state.h"

#include "data/directorytree.h"
#include "data/img_func.h"
#include "data/lyrics.h"
#include "data/playlist.h"
#include "data/song_loader.h"

#include "sys/sys_integration.h"
#include "utils/term.h"
#include "utils/utils.h"

#include "visuals.h"

#include <wchar.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

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

static int min_height = 0;
static int preferred_width = 0;
static int preferred_height = 0;
static int text_width = 0;
static int indent = 0;
static int max_list_size = 0;
static int max_search_list_size = 0;
static int num_top_level_songs = 0;
static int start_lib_iter = 0;
static int start_search_iter = 0;
static int max_lib_list_size = 0;
static int chosen_row = 0;             // The row that is chosen in playlist view
static int chosen_lib_row = 0;          // The row that is chosen in library view
static int chosen_search_result_row = 0; // The row that is chosen in search view
static int lib_iter = 0;
static int lib_song_iter = 0;
static int previous_chosen_lib_row = 0;
static int lib_current_dir_song_count = 0;
static PixelData footer_color = {120, 120, 120};
static FileSystemEntry *last_entry = NULL;
static FileSystemEntry *chosen_dir = NULL;
static bool is_same_name_as_last_time = false;
static int term_w, term_h;

int get_footer_row(void) { return footer_row; }

int get_footer_col(void) { return footer_col; }

bool init_theme(int argc, char *argv[])
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        bool themeLoaded = false;

        // Command-line theme handling
        if (argc > 3 && strcmp(argv[1], "theme") == 0)
        {
                set_error_message("Couldn't load theme. Theme file names shouldn't contain space.");
        }
        else if (argc == 3 && strcmp(argv[1], "theme") == 0)
        {
                // Try to load the user-specified theme
                if (load_theme(argv[2], false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                        snprintf(ui->themeName, sizeof(ui->themeName), "%s", argv[2]);
                }
                else
                {
                        // Failed to load user theme → fall back to
                        // default/ANSI
                        if (ui->colorMode == COLOR_MODE_THEME)
                        {
                                ui->colorMode = COLOR_MODE_DEFAULT;
                        }
                }
        }
        else if (ui->colorMode == COLOR_MODE_THEME)
        {
                // If UI has a themeName stored, try to load it
                if (load_theme(ui->themeName, false) > 0)
                {
                        ui->colorMode = COLOR_MODE_THEME;
                        themeLoaded = true;
                }
        }

        // If still in default mode, load default ANSI theme
        if (ui->colorMode == COLOR_MODE_DEFAULT)
        {
                // Load "default" ANSI theme, but don't overwrite
                // settings->theme
                if (load_theme("default", true))
                {
                        themeLoaded = true;
                }
        }

        if (!themeLoaded && ui->colorMode != COLOR_MODE_ALBUM)
        {
                set_error_message("Couldn't load theme. Forgot to run 'sudo make install'?");
                ui->colorMode = COLOR_MODE_ALBUM;
        }

        return themeLoaded;
}

void set_track_title_as_window_title(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        if (ui->trackTitleAsWindowTitle)
        {
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

int calc_ideal_img_size(int *width, int *height, const int visualizerHeight,
                     const int metatagHeight)
{
        if (!width || !height)
                return -1;

        float aspectRatio = calc_aspect_ratio();

        if (!isfinite(aspectRatio) || aspectRatio <= 0.0f ||
            aspectRatio > 100.0f)
                aspectRatio = 1.0f; // fallback to square

        int term_w = 0, term_h = 0;
        get_term_size(&term_w, &term_h);

        if (term_w <= 0 || term_h <= 0 || term_w > MAX_TERM_SIZE ||
            term_h > MAX_TERM_SIZE)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        const int timeDisplayHeight = 1;
        const int heightMargin = 4;
        const int min_height = visualizerHeight + metatagHeight +
                              timeDisplayHeight + heightMargin + 1;

        if (min_height < 0 || min_height > term_h)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        int availableHeight = term_h - min_height;
        if (availableHeight <= 0)
        {
                *width = 1;
                *height = 1;
                return -1;
        }

        // Safe calculation using double
        double safeHeight = (double)availableHeight;
        double safeAspect = (double)aspectRatio;

        double tempWidth = safeHeight * safeAspect;

        // Clamp to INT_MAX and reasonable limits
        if (tempWidth < 1.0)
                tempWidth = 1.0;
        else if (tempWidth > INT_MAX)
                tempWidth = INT_MAX;

        int calcWidth = (int)ceil(tempWidth);
        int calcHeight = availableHeight;

        if (calcWidth > term_w)
        {
                calcWidth = term_w;
                if (calcWidth <= 0)
                {
                        *width = 1;
                        *height = 1;
                        return -1;
                }

                double tempHeight = (double)calcWidth / safeAspect;

                if (tempHeight < 1.0)
                        tempHeight = 1.0;
                else if (tempHeight > INT_MAX)
                        tempHeight = INT_MAX;

                calcHeight = (int)floor(tempHeight);
        }

        // Final clamping
        if (calcWidth < 1)
                calcWidth = 1;
        if (calcHeight < 2)
                calcHeight = 2;

        // Slight adjustment
        calcHeight -= 1;
        if (calcHeight < 1)
                calcHeight = 1;

        *width = calcWidth;
        *height = calcHeight;

        return 0;
}

void calc_preferred_size(UISettings *ui)
{
        min_height = 2 + (ui->visualizerEnabled ? ui->visualizerHeight : 0);
        int metadataHeight = 4;
        calc_ideal_img_size(&preferred_width, &preferred_height,
                         (ui->visualizerEnabled ? ui->visualizerHeight : 0),
                         metadataHeight);
}

void print_help(void)
{
        int i = system("man kew");

        if (i != 0)
        {
                printf(_("Run man kew for help.\n"));
        }
}

static const char *get_player_status_icon(void)
{
        if (ops_is_paused())
        {
#ifdef __ANDROID__
                return "∥";
#else
                return "⏸";
#endif
        }
        if (ops_is_stopped())
                return "■";
        return "▶";
}

int print_logo_art(const UISettings *ui, int indent, bool centered, bool printTagLine, bool useGradient)
{
        if (ui->hideLogo)
        {
                clear_line();
                printf("\n");
                return 1;
        }

        int h, w;

        get_term_size(&w, &h);

        int centeredCol = (w - LOGO_WIDTH) / 2;
        if (centeredCol < 0)
                centeredCol = 0;

        size_t logoHeight = sizeof(LOGO) / sizeof(LOGO[0]);

        int col = centered ? centeredCol : indent;

        for (size_t i = 0; i < logoHeight; i++)
        {
                unsigned char defaultColor = ui->defaultColor;

                PixelData rowColor = {ui->color.r, ui->color.g, ui->color.b};

                if (useGradient && !(ui->color.r == defaultColor &&
                                     ui->color.g == defaultColor &&
                                     ui->color.b == defaultColor))
                {
                        rowColor = get_gradient_color(ui->color, logoHeight - i,
                                                    logoHeight, 2, 0.8f);
                }

                apply_color(ui->colorMode, ui->theme.logo, rowColor);

                clear_line();
                print_blank_spaces(col);
                printf("%s", LOGO[i]);
        }

        if (printTagLine)
        {
                printf("\n");
                print_blank_spaces(col);
                printf("MUSIC  FOR  THE  SHELL\n");
        }

        return 3; // lines used by logo
}

static void build_song_title(const SongData *songData, const UISettings *ui,
                           char *out, size_t outSize, int indent)
{
        if (!songData || !songData->metadata)
        {
                out[0] = '\0';
                return;
        }

        const char *icon = get_player_status_icon();

        char prettyTitle[METADATA_MAX_LENGTH] = {0};
        snprintf(prettyTitle, METADATA_MAX_LENGTH, "%s",
                 songData->metadata->title);
        trim(prettyTitle, strlen(prettyTitle));

        if (ui->hideLogo && songData->metadata->artist[0] != '\0')
        {
                snprintf(out, outSize, "%*s%s %s - %s", indent, "", icon,
                         songData->metadata->artist, prettyTitle);
        }
        else if (ui->hideLogo)
        {
                snprintf(out, outSize, "%*s%s %s", indent, "", icon,
                         prettyTitle);
        }
        else
        {
                strncpy(out, prettyTitle, outSize - 1);
                out[outSize - 1] = '\0';
        }
}

void print_now_playing(SongData *songData, UISettings *ui, int row, int col, int maxWidth)
{
        char title[MAXPATHLEN + 1];

        build_song_title(songData, ui, title, sizeof(title), indent);

        apply_color(ui->colorMode, ui->theme.nowplaying, ui->color);

        if (title[0] != '\0')
        {
                char processed[MAXPATHLEN + 1] = {0};
                process_name(title, processed, maxWidth, false, false);

                printf("\033[%d;%dH", row, col);
                printf("%s", processed);
                clear_rest_of_line();
        }
}

int print_logo(SongData *songData, UISettings *ui)
{
        int term_w, term_h;

        get_term_size(&term_w, &term_h);

        int logoWidth = ui->hideLogo ? 0 : LOGO_WIDTH;
        int maxWidth =
            term_w - indent - (ui->hideLogo ? 2 : logoWidth + 4);

        int height = print_logo_art(ui, indent, false, false, true);

        print_now_playing(songData, ui, height + 1, indent + logoWidth + 2, maxWidth);

        printf("\n");
        clear_line();
        printf("\n");

        return height + 2;
}

int get_year(const char *dateString)
{
        int year;

        if (sscanf(dateString, "%d", &year) != 1)
        {
                return -1;
        }
        return year;
}

void print_cover_centered(SongData *songdata, UISettings *ui)
{
        if (songdata != NULL && songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        print_square_bitmap_centered(
                            songdata->cover, songdata->coverWidth,
                            songdata->coverHeight, preferred_height);
                }
                else
                {
                        print_in_ascii_centered(songdata->coverArtPath,
                                             preferred_height);
                }
        }
        else
        {
                for (int i = 0; i <= preferred_height; ++i)
                {
                        printf("\n");
                }
        }

        printf("\n\n");
}

void print_cover(int height, SongData *songdata, UISettings *ui)
{
        int row = 2;
        int col = 2;
        int imgHeight = height - 2;

        clear_screen();

        if (songdata != NULL && songdata->cover != NULL && ui->coverEnabled)
        {
                if (!ui->coverAnsi)
                {
                        print_square_bitmap(row, col, songdata->cover,
                                          songdata->coverWidth,
                                          songdata->coverHeight, imgHeight);
                }
                else
                {
                        print_in_ascii(col, songdata->coverArtPath, imgHeight);
                }
        }
}

void print_title_with_delay(int row, int col, const char *text, int delay,
                         int maxWidth)
{
        int max = strnlen(text, maxWidth);

        if (max == maxWidth) // For long names
                max -= 2;    // Accommodate for the cursor that we display after
                             // the name.

        for (int i = 0; i <= max && delay; i++)
        {
                printf("\033[%d;%dH", row, col);
                clear_rest_of_line();

                for (int j = 0; j < i; j++)
                {
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

void print_metadata(int row, int col, int maxWidth,
                   TagSettings const *metadata, UISettings *ui)
{
        if (row < 1)
                row = 1;

        if (col < 1)
                col = 1;

        if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0)
        {
                apply_color(ui->colorMode, ui->theme.trackview_artist,
                           ui->color);
                printf("\033[%d;%dH", row + 1, col);
                clear_rest_of_line();
                printf(" %.*s", maxWidth, metadata->artist);
        }

        if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0)
        {
                apply_color(ui->colorMode, ui->theme.trackview_album, ui->color);
                printf("\033[%d;%dH", row + 2, col);
                clear_rest_of_line();
                printf(" %.*s", maxWidth, metadata->album);
        }

        if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0)
        {
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

        if (pixel.r == 255 && pixel.g == 255 && pixel.b == 255)
        {
                unsigned char defaultColor = ui->defaultColor;

                pixel.r = defaultColor;
                pixel.g = defaultColor;
                pixel.b = defaultColor;
        }

        apply_color(ui->colorMode, ui->theme.trackview_title, pixel);

        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0)
        {
                // Clean up title before printing
                char prettyTitle[MAXPATHLEN + 1];
                prettyTitle[0] = '\0';

                process_name(metadata->title, prettyTitle, maxWidth, false,
                            false);

                print_title_with_delay(row, col + 1, prettyTitle, ui->titleDelay,
                                    maxWidth);
        }
}

int calc_elapsed_bars(double elapsed_seconds, double duration, int numProgressBars)
{
        if (elapsed_seconds == 0)
                return 0;

        return (int)((elapsed_seconds / duration) * numProgressBars);
}

void print_progress(double elapsed_seconds, double total_seconds,
                   ma_uint32 sample_rate, int avgBitRate, int allowedWidth)
{
        int progressWidth = 39;

        if (allowedWidth < progressWidth)
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

        if (total_seconds >= 3600)
        {
                // Song is more than 1 hour long: use full HH:MM:SS format
                printf(" %02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                       elapsed_hours, elapsed_minutes,
                       elapsed_seconds_remainder, total_hours, total_minutes,
                       total_seconds_remainder, progress_percentage, vol);
        }
        else
        {
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

        if (allowedWidth > progressWidth + 10)
        {
                if (rate == (int)rate)
                        printf(" %dkHz", (int)rate);
                else
                        printf(" %.1fkHz", rate);
        }

        if (allowedWidth > progressWidth + 19)
        {
                if (avgBitRate > 0)
                        printf(" %dkb/s ", avgBitRate);
        }
}

void print_time(int row, int col, double elapsed_seconds, ma_uint32 sample_rate,
               int avgBitRate, int allowedWidth)
{
        AppState *state = get_app_state();

        apply_color(state->uiSettings.colorMode,
                   state->uiSettings.theme.trackview_time,
                   state->uiSettings.color);

        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        printf("\033[%d;%dH", row, col);

        if (term_h > min_height)
        {
                double duration = get_current_song_duration();
                double elapsed = elapsed_seconds;

                print_progress(elapsed, duration, sample_rate, avgBitRate, allowedWidth);
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

        int titleLength = strnlen(metadata->title, METADATA_MAX_LENGTH);
        int albumLength = strnlen(metadata->album, METADATA_MAX_LENGTH);
        int maxTextLength =
            (albumLength > titleLength) ? albumLength : titleLength;
        text_width = (ABSOLUTE_MIN_WIDTH > preferred_width) ? ABSOLUTE_MIN_WIDTH
                                                          : preferred_width;
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int maxSize = term_w - 2;
        if (maxTextLength > 0 && maxTextLength < maxSize &&
            maxTextLength > text_width)
                text_width = maxTextLength;
        if (text_width > maxSize)
                text_width = maxSize;

        return get_indentation(text_width - 1) - 1;
}

void calc_indent(SongData *songdata)
{
        AppState *state = get_app_state();

        if ((state->currentView == TRACK_VIEW && songdata == NULL) ||
            state->currentView != TRACK_VIEW)
        {
                indent = calc_indent_normal();
        }
        else
        {
                indent = calc_indent_track_view(songdata->metadata);
        }
}

int get_indent() { return indent; }

void print_glimmering_text(int row, int col, char *text, int textLength,
                         char *nerdFontText, PixelData color)
{
        int brightIndex = 0;
        PixelData vbright = increase_luminosity(color, 120);
        PixelData bright = increase_luminosity(color, 60);

        printf("\033[%d;%dH", row, col);

        clear_rest_of_line();

        while (brightIndex < textLength)
        {
                for (int i = 0; i < textLength; i++)
                {
                        if (i == brightIndex)
                        {
                                set_text_color_RGB(vbright.r, vbright.g,
                                                vbright.b);
                                printf("%c", text[i]);
                        }
                        else if (i == brightIndex - 1 || i == brightIndex + 1)
                        {
                                set_text_color_RGB(bright.r, bright.g, bright.b);
                                printf("%c", text[i]);
                        }
                        else
                        {
                                set_text_color_RGB(color.r, color.g, color.b);
                                printf("%c", text[i]);
                        }

                        fflush(stdout);
                        c_usleep(50);
                }
                printf("%s", nerdFontText);
                fflush(stdout);
                c_usleep(50);

                brightIndex++;
                printf("\033[%d;%dH", row, col);
        }
}

void print_error_row(int row, int col, UISettings *ui)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        printf("\033[%d;%dH", row, col);

        if (!has_printed_error_message() && has_error_message())
        {
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

    for (const char *p = text; *p; p++)
    {
        if ((unsigned char)*p >= 128)
        {
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

        footer_row = row;
        footer_col = col;

        printf("\033[%d;%dH", row, col);

        PixelData fColor;
        fColor.r = footer_color.r;
        fColor.g = footer_color.g;
        fColor.b = footer_color.b;

        apply_color(ui->colorMode, ui->theme.footer, fColor);

        if (ui->themeIsSet && ui->theme.footer.type == COLOR_TYPE_RGB)
        {
                fColor.r = ui->theme.footer.rgb.r;
                fColor.g = ui->theme.footer.rgb.g;
                fColor.b = ui->theme.footer.rgb.b;
        }

        char text[100];
        char playlist[32], library[32], track[32], search[32], help[32];

        snprintf(playlist, sizeof(playlist), "%s", get_binding_string(EVENT_SHOWPLAYLIST, true));
        snprintf(library, sizeof(library), "%s", get_binding_string(EVENT_SHOWLIBRARY, true));
        snprintf(track, sizeof(track), "%s", get_binding_string(EVENT_SHOWTRACK, true));
        snprintf(search, sizeof(search), "%s", get_binding_string(EVENT_SHOWSEARCH, true));
        snprintf(help, sizeof(search), "%s", get_binding_string(EVENT_SHOWHELP, true));

        snprintf(text, sizeof(text),
                 _("%s Playlist|%s Library|%s Track|%s Search|%s Help"), playlist,
                 library, track, search, help);

        char iconsText[100] = "";

        size_t maxLength = sizeof(iconsText);

        size_t currentLength = strnlen(iconsText, maxLength);

#ifndef __ANDROID__
        if (term_w >= ABSOLUTE_MIN_WIDTH)
        {
#endif
                if (ops_is_paused())
                {
#if defined(__ANDROID__) || defined(__APPLE__)
                        char pauseText[] = " ∥";
#else
                char pauseText[] = " ⏸";
#endif
                        snprintf(iconsText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else if (ops_is_stopped())
                {
                        char pauseText[] = " ■";
                        snprintf(iconsText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
                else
                {
                        char pauseText[] = " ▶";
                        snprintf(iconsText + currentLength,
                                 maxLength - currentLength, "%s", pauseText);
                        currentLength += strlen(pauseText);
                }
#ifndef __ANDROID__
        }
#endif

        if (ops_is_repeat_enabled())
        {
                char repeatText[] = " ↻";
                snprintf(iconsText + currentLength,
                         maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }
        else if (is_repeat_list_enabled())
        {
                char repeatText[] = " ↻L";
                snprintf(iconsText + currentLength,
                         maxLength - currentLength, "%s", repeatText);
                currentLength += strlen(repeatText);
        }

        if (is_shuffle_enabled())
        {
                char shuffleText[] = " ⇄";
                snprintf(iconsText + currentLength,
                         maxLength - currentLength, "%s", shuffleText);
                currentLength += strlen(shuffleText);
        }

        if (uis->isFastForwarding)
        {
                char forwardText[] = " ⇉";
                snprintf(iconsText + currentLength,
                         maxLength - currentLength, "%s", forwardText);
                currentLength += strlen(forwardText);
        }

        if (uis->isRewinding)
        {
                char rewindText[] = " ⇇";
                snprintf(iconsText + currentLength,
                         maxLength - currentLength, "%s", rewindText);
                currentLength += strlen(rewindText);
        }

        if (term_w < ABSOLUTE_MIN_WIDTH)
        {
#ifndef __ANDROID__
                if (term_w > (int)currentLength + indent)
                {
                        printf("%s", iconsText); // Print just the shuffle
                                                 // and replay settings
                }
#else
                // Always try to print the footer on Android because it will
                // most likely be too narrow. We use two rows for the footer on
                // Android.
                print_blank_spaces(indent);
                printf("%.*s", term_w * 2, text);
                printf("%s", iconsText);
#endif
                clear_rest_of_line();
                return;
        }

        int textLength = strnlen(text, 100);
        int randomNumber = get_random_number(1, 808);


        if (randomNumber == 808 && !ui->hideGlimmeringText && is_ascii_only(text))
        {
                print_glimmering_text(row, col, text, textLength, iconsText, footer_color);
        }
        else
        {
                printf("%s", text);
                printf("%s", iconsText);
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
        print_footer(term_h, indent);
#endif
}

int print_about(SongData *songdata)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        clear_line();
        int numRows = print_logo(songdata, ui);

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        print_blank_spaces(indent);
        printf(_(" kew version: "));
        apply_color(ui->colorMode, ui->theme.help, ui->color);
        printf("%s\n", ui->VERSION);
        clear_line();
        printf("\n");
        numRows += 2;

        return numRows;
}

#define CHECK_LIST_LIMIT()                                        \
        do                                                        \
        {                                                         \
                if (numPrintedRows >= max_list_size)                \
                {                                                 \
                        printf("\n");                             \
                        calc_and_print_last_row_and_error_row();         \
                        return numPrintedRows;                    \
                }                                                 \
        } while (0)

int show_key_bindings(SongData *songdata)
{
        AppState *state = get_app_state();
        int numPrintedRows = 0;
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_list_size = term_h - 3;

        clear_screen();

        UISettings *ui = &(state->uiSettings);

        numPrintedRows += print_about(songdata);
        CHECK_LIST_LIMIT();
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);

        print_blank_spaces(indent);
        printf(_(" Manual: See"));
        apply_color(ui->colorMode, ui->theme.help, ui->color);
        printf(_(" README"));
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        printf(_(" Or man kew\n\n"));
        numPrintedRows += 2;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Play/Pause: %s\n"), get_binding_string(EVENT_PLAY_PAUSE, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Enqueue/Dequeue: %s\n"), get_binding_string(EVENT_ENQUEUE, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Enqueue and Play: %s\n"), get_binding_string(EVENT_ENQUEUEANDPLAY, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Quit: %s\n"), get_binding_string(EVENT_QUIT, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Switch tracks: %s"),
               get_binding_string(EVENT_PREV, false));
        printf(_(" and %s\n"),
               get_binding_string(EVENT_NEXT, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · volume: %s "), get_binding_string(EVENT_VOLUME_UP, false));
        printf(_("and %s\n"),get_binding_string(EVENT_VOLUME_DOWN, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Clear List: %s\n"), get_binding_string(EVENT_CLEARPLAYLIST, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Change View: %s or "), get_binding_string(EVENT_NEXTVIEW, false));

        printf("%s, ", get_binding_string(EVENT_SHOWPLAYLIST, true));
        printf("%s, ", get_binding_string(EVENT_SHOWLIBRARY, true));
        printf("%s, ", get_binding_string(EVENT_SHOWTRACK, true));
        printf("%s, ", get_binding_string(EVENT_SHOWSEARCH, true));
        printf("%s", get_binding_string(EVENT_SHOWHELP, true));

        printf(_(" or click the footer\n"));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(
            _(" · Cycle Color Mode: %s (default theme, theme or cover colors)\n"),
            get_binding_string(EVENT_CYCLECOLORMODE, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Cycle Themes: %s\n"), get_binding_string(EVENT_CYCLETHEMES, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Stop: %s\n"), get_binding_string(EVENT_STOP, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Update Library: %s\n"), get_binding_string(EVENT_UPDATELIBRARY, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Toggle Visualizer: %s\n"), get_binding_string(EVENT_TOGGLEVISUALIZER, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Toggle ASCII Cover: %s\n"), get_binding_string(EVENT_TOGGLEASCII, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Toggle Lyrics Page on Track View: %s\n"), get_binding_string(EVENT_SHOWLYRICSPAGE, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Toggle Notifications: %s\n"), get_binding_string(EVENT_TOGGLENOTIFICATIONS, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Cycle Repeat: %s (repeat/repeat list/off)\n"),
               get_binding_string(EVENT_TOGGLEREPEAT, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Shuffle: %s\n"), get_binding_string(EVENT_SHUFFLE, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Seek: %s and"), get_binding_string(EVENT_SEEKBACK, false));
        printf(_(" %s\n"), get_binding_string(EVENT_SEEKFORWARD, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Export Playlist: %s (named after the first song)\n"),
               get_binding_string(EVENT_EXPORTPLAYLIST, false));
        numPrintedRows++;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        printf(_(" · Add Song To 'kew favorites.m3u': %s (run with 'kew .')\n\n"),
               get_binding_string(EVENT_ADDTOFAVORITESPLAYLIST, false));
        numPrintedRows += 2;
        CHECK_LIST_LIMIT();
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        print_blank_spaces(indent);
        printf(_(" Theme: "));

        if (ui->colorMode == COLOR_MODE_ALBUM)
        {
                apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf(_("Using "));
                apply_color(ui->colorMode, ui->theme.text, ui->color);
                printf(_("Colors "));
                apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
                printf(_("From Track Covers"));
        }
        else
        {
                apply_color(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_name);
        }

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        if (ui->colorMode != COLOR_MODE_ALBUM)
        {
                printf(_(" Author: "));
                apply_color(ui->colorMode, ui->theme.help, ui->color);
                printf("%s", ui->theme.theme_author);
        }
        printf("\n");
        printf("\n");
        numPrintedRows += 2;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(_(" Project URL:"));
        apply_color(ui->colorMode, ui->theme.link, ui->color);
        printf(" https://codeberg.org/ravachol/kew\n");
        numPrintedRows += 1;
        CHECK_LIST_LIMIT();
        print_blank_spaces(indent);
        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);
        printf(_(" Please Donate:"));
        apply_color(ui->colorMode, ui->theme.link, ui->color);
        printf(" https://ko-fi.com/ravachol\n\n");
        numPrintedRows += 2;
        CHECK_LIST_LIMIT();
        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        print_blank_spaces(indent);
        printf(" Copyright © 2022-2025 Ravachol\n");

        printf("\n");
        numPrintedRows += 2;
        CHECK_LIST_LIMIT();

        while (numPrintedRows < max_list_size)
        {
                printf("\n");
                numPrintedRows++;
        }

        calc_and_print_last_row_and_error_row();

        numPrintedRows += 2;

        return numPrintedRows;
}

void toggle_show_view(ViewState viewToShow)
{
        AppState *state = get_app_state();
        trigger_refresh();

        if (state->currentView == TRACK_VIEW)
                clear_screen();

        if (state->currentView == viewToShow)
        {
                state->currentView = TRACK_VIEW;
        }
        else
        {
                state->currentView = viewToShow;
        }
}

void switch_to_next_view(void)
{
        AppState *state = get_app_state();

        switch (state->currentView)
        {
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
                state->currentView = KEYBINDINGS_VIEW;
                break;
        case KEYBINDINGS_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        }

        trigger_refresh();
}

void switch_to_previous_view(void)
{
        AppState *state = get_app_state();

        switch (state->currentView)
        {
        case PLAYLIST_VIEW:
                state->currentView = KEYBINDINGS_VIEW;
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
        case KEYBINDINGS_VIEW:
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

        if (state->currentView == LIBRARY_VIEW)
        {
                chosen_lib_row += max_lib_list_size - 1;
                start_lib_iter += max_lib_list_size - 1;
                trigger_refresh();
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                chosen_row += max_list_size - 1;
                chosen_row = (chosen_row >= unshuffled_playlist->count)
                                ? unshuffled_playlist->count - 1
                                : chosen_row;
                trigger_refresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosen_search_result_row += max_search_list_size - 1;
                chosen_search_result_row =
                    (chosen_search_result_row >= get_search_results_count())
                        ? get_search_results_count() - 1
                        : chosen_search_result_row;
                start_search_iter += max_search_list_size - 1;
                trigger_refresh();
        }
}

void flip_prev_page(void)
{
        AppState *state = get_app_state();

        if (state->currentView == LIBRARY_VIEW)
        {
                chosen_lib_row -= max_lib_list_size;
                start_lib_iter -= max_lib_list_size;
                trigger_refresh();
        }
        else if (state->currentView == PLAYLIST_VIEW)
        {
                chosen_row -= max_list_size;
                chosen_row = (chosen_row > 0) ? chosen_row : 0;
                trigger_refresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosen_search_result_row -= max_search_list_size;
                chosen_search_result_row =
                    (chosen_search_result_row > 0) ? chosen_search_result_row : 0;
                start_search_iter -= max_search_list_size;
                trigger_refresh();
        }
}

void scroll_next(void)
{
        AppState *state = get_app_state();
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (state->currentView == PLAYLIST_VIEW)
        {
                chosen_row++;
                chosen_row = (chosen_row >= unshuffled_playlist->count)
                                ? unshuffled_playlist->count - 1
                                : chosen_row;
                trigger_refresh();
        }
        else if (state->currentView == LIBRARY_VIEW)
        {
                previous_chosen_lib_row = chosen_lib_row;
                chosen_lib_row++;
                trigger_refresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosen_search_result_row++;
                trigger_refresh();
        }
}

void scroll_prev(void)
{
        AppState *state = get_app_state();

        if (state->currentView == PLAYLIST_VIEW)
        {
                chosen_row--;
                chosen_row = (chosen_row > 0) ? chosen_row : 0;
                trigger_refresh();
        }
        else if (state->currentView == LIBRARY_VIEW)
        {
                previous_chosen_lib_row = chosen_lib_row;
                chosen_lib_row--;
                trigger_refresh();
        }
        else if (state->currentView == SEARCH_VIEW)
        {
                chosen_search_result_row--;
                chosen_search_result_row =
                    (chosen_search_result_row > 0) ? chosen_search_result_row : 0;
                trigger_refresh();
        }
}

int get_row_within_bounds(int row)
{
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (row >= unshuffled_playlist->count)
        {
                row = unshuffled_playlist->count - 1;
        }

        if (row < 0)
                row = 0;

        return row;
}

int print_logo_and_adjustments(SongData *songData, int termWidth, UISettings *ui,
                            int indentation, AppSettings *settings)
{
        int aboutRows = print_logo(songData, ui);

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (termWidth > 52 && !ui->hideHelp)
        {
                print_blank_spaces(indentation);
                clear_line();
                printf(_(" Use ↑/↓ or k/j to select. Enter: Accept. Backspace: clear.\n"));
                print_blank_spaces(indentation);
#ifndef __APPLE__
                clear_line();
                printf(_(" PgUp/PgDn: scroll. Del: remove. %s/%s: move songs.\n"),
                       settings->move_song_up, settings->move_song_down);
                clear_line();
                printf("\n");
#else
                clear_line();
                printf(_(" Fn+↑/↓: scroll. Del: remove. %s/%s: move songs.\n"),
                       settings->move_song_up, settings->move_song_down);
                clear_line();
                printf("\n");
#endif
                clear_line();
                return aboutRows + 3;
        }
        return aboutRows;
}

void show_search(SongData *songData, int *chosen_row)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_search_list_size = term_h - 3;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        goto_first_line_first_row();

        int aboutRows = print_logo(songData, ui);
        max_search_list_size -= aboutRows;

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (term_w > indent + 38 && !ui->hideHelp)
        {
                clear_line();
                print_blank_spaces(indent);
                clear_line();
                printf(_(" Use ↑/↓ to select. Enter: Enqueue. Alt+Enter: Play.\n"));
                clear_line();
                printf("\n");
                max_search_list_size -= 2;
        }

        display_search(max_search_list_size, indent, chosen_row, start_search_iter);

        calc_and_print_last_row_and_error_row();
}

void show_playlist(SongData *songData, PlayList *list, int *chosenSong,
                  int *chosenNodeId, AppSettings *settings)
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        max_list_size = term_h - 3;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        int update_counter = get_update_counter();

        if (get_is_long_name() && is_same_name_as_last_time &&
            update_counter % scrolling_interval != 0)
        {
                increment_update_counter();
                trigger_refresh();
                return;
        }
        else
                cancel_refresh();

        goto_first_line_first_row();

        int aboutRows =
            print_logo_and_adjustments(songData, term_w, ui, indent, settings);
        max_list_size -= aboutRows;

        apply_color(ui->colorMode, ui->theme.header, ui->color);

        if (max_list_size > 0)
        {
                clear_line();
                print_blank_spaces(indent);
                printf(_("   ─ PLAYLIST ─\n"));
        }

        max_list_size -= 1;

        if (max_list_size > 0)
                display_playlist(list, max_list_size, indent, chosenSong,
                                chosenNodeId,
                                state->uiState.resetPlaylistDisplay);

        calc_and_print_last_row_and_error_row();
}

void reset_search_result(void) { chosen_search_result_row = 0; }

void print_progress_bar(int row, int col, AppSettings *settings, UISettings *ui,
                      int elapsedBars, int numProgressBars)
{
        PixelData color = ui->color;

        ProgressBar *progress_bar = get_progress_bar();

        progress_bar->row = row;
        progress_bar->col = col + 1;
        progress_bar->length = numProgressBars;

        printf("\033[%d;%dH", row, col);
        printf(" ");

        for (int i = 0; i < numProgressBars; i++)
        {
                if (i > elapsedBars)
                {
                        if (ui->colorMode == COLOR_MODE_ALBUM)
                        {
                                PixelData tmp = increase_luminosity(color, 50);
                                printf("\033[38;2;%d;%d;%dm", tmp.r, tmp.g,
                                       tmp.b);

                                apply_color(ui->colorMode,
                                           ui->theme.progress_empty, tmp);
                        }
                        else
                        {
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

                if (i < elapsedBars)
                {
                        apply_color(ui->colorMode, ui->theme.progress_filled,
                                   color);

                        if (i % 2 == 0)
                                printf("%s",
                                       settings->progressBarElapsedEvenChar);
                        else
                                printf("%s",
                                       settings->progressBarElapsedOddChar);
                }
                else if (i == elapsedBars)
                {
                        apply_color(ui->colorMode, ui->theme.progress_elapsed,
                                   color);

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

void print_visualizer(int row, int col, int visualizerWidth,
                     AppSettings *settings, double elapsed_seconds)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        int height = state->uiSettings.visualizerHeight;

        int term_w, term_h;
        get_term_size(&term_w, &term_h);

        if (row + height + 2 > term_h)
                height -= (row + height + 1 - term_h);

        if (height < 2)
                return;

        if (ui->visualizerEnabled)
        {
                uis->numProgressBars = (int)visualizerWidth / 2;
                double duration = get_current_song_duration();

                draw_spectrum_visualizer(row, col, height);

                int elapsedBars =
                    calc_elapsed_bars(elapsed_seconds, duration, visualizerWidth);

                print_progress_bar(row + height - 1, col, settings, ui,
                                 elapsedBars, visualizerWidth - 1);
        }
}

FileSystemEntry *get_chosen_dir(void) { return chosen_dir; }

void set_chosen_dir(FileSystemEntry *entry)
{
        AppState *state = get_app_state();

        if (entry == NULL)
        {
                return;
        }

        if (entry->is_directory)
        {
                state->uiState.currentLibEntry = chosen_dir = entry;
        }
}

void reset_chosen_dir(void) { chosen_dir = NULL; }

void apply_tree_item_color(UISettings *ui, int depth,
                        PixelData trackColor, PixelData enqueuedColor,
                        bool isEnqueued, bool is_playing)
{
        if (depth <= 1)
        {
                apply_color(ui->colorMode, ui->theme.library_artist, enqueuedColor);
        }
        else
        {
                if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                        apply_color(COLOR_MODE_ALBUM, ui->theme.library_track,
                                   trackColor);
                else
                        apply_color(ui->colorMode, ui->theme.library_track,
                                   trackColor);
        }

        if (isEnqueued)
        {
                if (is_playing)
                {
                        apply_color(ui->colorMode, ui->theme.library_playing,
                                   ui->color);
                }
                else
                {
                        if (ui->colorMode == COLOR_MODE_ALBUM || ui->colorMode == COLOR_MODE_THEME)
                                apply_color(COLOR_MODE_ALBUM, ui->theme.library_enqueued,
                                           enqueuedColor);
                        else
                                apply_color(ui->colorMode, ui->theme.library_enqueued,
                                           enqueuedColor);
                }
        }
}

int display_tree(FileSystemEntry *root, int depth, int max_list_size,
                int maxNameWidth)
{
        if (maxNameWidth < 0)
                maxNameWidth = 0;

        char dirName[maxNameWidth + 1];
        char filename[MAXPATHLEN + 1];
        bool foundChosen = false;
        int is_playing = 0;
        int extraIndent = 0;

        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        UIState *uis = &(state->uiState);

        Node *current = get_current_song();

        if (current != NULL &&
            (strcmp(current->song.filePath, root->fullPath) == 0))
        {
                is_playing = 1;
        }

        if (start_lib_iter < 0)
                start_lib_iter = 0;

        if (lib_iter >= start_lib_iter + max_list_size)
        {
                return false;
        }

        int threshold = start_lib_iter + (max_list_size + 1) / 2;
        if (chosen_lib_row > threshold)
        {
                start_lib_iter = chosen_lib_row - max_list_size / 2 + 1;
        }

        if (chosen_lib_row < 0)
                start_lib_iter = chosen_lib_row = lib_iter = 0;

        if (root == NULL)
                return false;

        PixelData rowColor = {ui->defaultColor, ui->defaultColor, ui->defaultColor};
        PixelData rowColor2 = {ui->defaultColor, ui->defaultColor, ui->defaultColor};
        PixelData rgbTrack = {ui->defaultColor, ui->defaultColor, ui->defaultColor};
        PixelData rgbEnqueued = {ui->defaultColor, ui->defaultColor, ui->defaultColor};

        if (ui->colorMode == COLOR_MODE_THEME &&
            ui->theme.playlist_rownum.type == COLOR_TYPE_RGB)
        {
                rgbTrack = ui->theme.library_track.rgb;
                rgbEnqueued = ui->theme.library_enqueued.rgb;
        }
        else
        {
                rgbEnqueued = ui->color;
        }

        if (!(rgbTrack.r == ui->defaultColor &&
              rgbTrack.g == ui->defaultColor &&
              rgbTrack.b == ui->defaultColor))
                rowColor = get_gradient_color(rgbTrack, lib_iter - start_lib_iter,
                                            max_list_size,
                                            max_list_size / 2, 0.7f);

        if (!(rgbEnqueued.r == ui->defaultColor &&
              rgbEnqueued.g == ui->defaultColor &&
              rgbEnqueued.b == ui->defaultColor))
                rowColor2 = get_gradient_color(rgbEnqueued, lib_iter - start_lib_iter,
                                             max_list_size,
                                             max_list_size / 2, 0.7f);

        if (!(root->is_directory || (!root->is_directory && depth == 1) ||
              (root->is_directory && depth == 0) ||
              (chosen_dir != NULL && uis->allowChooseSongs &&
               root->parent != NULL &&
               (strcmp(root->parent->fullPath, chosen_dir->fullPath) == 0 ||
                strcmp(root->fullPath, chosen_dir->fullPath) == 0))))
        {
                return foundChosen;
        }

        if (depth >= 0)
        {
                if (state->uiState.currentLibEntry != NULL && state->uiState.currentLibEntry != last_entry &&
                    !state->uiState.currentLibEntry->is_directory &&
                    state->uiState.currentLibEntry->parent != NULL &&
                    state->uiState.currentLibEntry->parent == chosen_dir)
                {
                        FileSystemEntry *tmpc = state->uiState.currentLibEntry->parent->children;

                        lib_current_dir_song_count = 0;

                        while (tmpc != NULL)
                        {
                                if (!tmpc->is_directory)
                                        lib_current_dir_song_count++;
                                tmpc = tmpc->next;
                        }

                        last_entry = state->uiState.currentLibEntry;
                }

                if (lib_iter >= start_lib_iter)
                {
                        apply_tree_item_color(ui, depth, rowColor, rowColor2,
                                           root->isEnqueued, is_playing);

                        clear_line();

                        if (depth >= 2)
                                printf("  ");

                        // If more than two levels deep add an extra
                        // indentation
                        extraIndent = (depth - 2 <= 0) ? 0 : depth - 2;

                        print_blank_spaces(indent + extraIndent);

                        if (chosen_lib_row == lib_iter)
                        {
                                if (root->isEnqueued)
                                {
                                        printf("\x1b[7m * ");
                                }
                                else
                                {
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
                                      strcmp(root->fullPath,
                                             chosen_dir->fullPath) != 0)))
                                {
                                        uis->collapseView = true;
                                        trigger_refresh();

                                        if (!uis->openedSubDir)
                                        {

                                                uis->allowChooseSongs = false;
                                                chosen_dir = NULL;
                                        }
                                }

                                foundChosen = true;
                        }
                        else
                        {
                                if (root->isEnqueued)
                                {
                                        printf(" * ");
                                }
                                else
                                {
                                        printf("   ");
                                }
                        }

                        if (maxNameWidth < extraIndent)
                                maxNameWidth = extraIndent;

                        if (root->is_directory)
                        {
                                dirName[0] = '\0';

                                if (strcmp(root->name, "root") == 0)
                                        snprintf(dirName,
                                                 maxNameWidth + 1 - extraIndent,
                                                 "%s", _("─ MUSIC LIBRARY ─"));
                                else
                                        snprintf(dirName,
                                                 maxNameWidth + 1 - extraIndent,
                                                 "%s", root->name);

                                char *upperDirName = string_to_upper(dirName);

                                if (depth == 1)
                                        printf("%s \n", upperDirName);
                                else
                                {
                                        printf("%s \n", dirName);
                                }
                                free(upperDirName);
                        }
                        else
                        {
                                filename[0] = '\0';

                                is_same_name_as_last_time =
                                    (previous_chosen_lib_row == chosen_lib_row);

                                if (foundChosen)
                                {
                                        previous_chosen_lib_row = chosen_lib_row;
                                }

                                if (!is_same_name_as_last_time)
                                {
                                        reset_name_scroll();
                                }

                                if (foundChosen)
                                {
                                        process_name_scroll(root->name, filename,
                                                          maxNameWidth -
                                                              extraIndent,
                                                          is_same_name_as_last_time);
                                }
                                else
                                {
                                        process_name(root->name, filename,
                                                    maxNameWidth - extraIndent,
                                                    true, true);
                                }

                                if (is_playing)
                                {
                                        if (chosen_lib_row == lib_iter)
                                        {
                                                printf("\x1b[7m");
                                        }
                                }

                                printf("└─ ");

                                // Playlist
                                if (path_ends_with(root->fullPath, "m3u") ||
                                    path_ends_with(root->fullPath, "m3u8"))
                                {
                                        printf("♫ ");
                                        maxNameWidth = maxNameWidth - 2;
                                }

                                if (is_playing && chosen_lib_row != lib_iter)
                                {
                                        printf("\e[4m");
                                }

                                printf("%s\n", filename);

                                lib_song_iter++;
                        }
                }

                lib_iter++;
        }

        FileSystemEntry *child = root->children;
        while (child != NULL)
        {
                if (display_tree(child, depth + 1, max_list_size, maxNameWidth))
                        foundChosen = true;

                child = child->next;
        }

        return foundChosen;
}

void show_library(SongData *songData, AppSettings *settings)
{
        AppState *state = get_app_state();
        // For scrolling names, update every nth time
        if (get_is_long_name() && is_same_name_as_last_time &&
            get_update_counter() % scrolling_interval != 0)
        {
                trigger_refresh();
                return;
        }
        else
                cancel_refresh();

        goto_first_line_first_row();

        if (state->uiState.collapseView)
        {
                if (previous_chosen_lib_row < chosen_lib_row)
                {
                        if (!state->uiState.openedSubDir)
                        {
                                chosen_lib_row -= lib_current_dir_song_count;
                                lib_current_dir_song_count = 0;
                        }
                        else
                        {
                                chosen_lib_row -=
                                    state->uiState.numSongsAboveSubDir;
                                state->uiState.openedSubDir = false;
                                state->uiState.numSongsAboveSubDir = 0;
                                state->uiState.collapseView = false;
                        }
                }
                else
                {
                        if (state->uiState.openedSubDir)
                        {
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
        int totalHeight = term_h;
        max_lib_list_size = totalHeight;
        int aboutSize = print_logo(songData, ui);
        int maxNameWidth = term_w - 10 - indent;
        max_lib_list_size -= aboutSize + 2;

        apply_color(ui->colorMode, ui->theme.help, ui->defaultColorRGB);

        if (term_w > 67 && !ui->hideHelp)
        {
                max_lib_list_size -= 3;
                clear_line();
                print_blank_spaces(indent);
                printf(_(" Use ↑/↓ or k/j to select. Enter: Enqueue/Dequeue. Alt+Enter: Play.\n"));
                clear_line();
                print_blank_spaces(indent);
#ifndef __APPLE__
                printf(_(" PgUp/PgDn: scroll. u: update, o: sort.\n"));
                clear_line();
                printf("\n");
#else
                printf(_(" Fn+↑/↓: scroll. u: update, o: sort.\n"));
                clear_line();
                printf("\n");
#endif
                clear_line();
        }

        num_top_level_songs = 0;

        FileSystemEntry *library = get_library();

        FileSystemEntry *tmp = library->children;

        while (tmp != NULL)
        {
                if (!tmp->is_directory)
                        num_top_level_songs++;

                tmp = tmp->next;
        }

        bool foundChosen = false;

        if (max_lib_list_size <= 0)
                foundChosen = true;
        else
                foundChosen = display_tree(library, 0, max_lib_list_size,
                                          maxNameWidth);

        if (!foundChosen)
        {
                chosen_lib_row--;
                trigger_refresh();
        }

        for (int i = lib_iter - start_lib_iter; i < max_lib_list_size; i++)
        {
                printf("\n");
                clear_line();
        }

        calc_and_print_last_row_and_error_row();

        if (!foundChosen && is_refresh_triggered())
        {
                goto_first_line_first_row();
                show_library(songData, settings);
        }
}

int calc_visualizer_width()
{
        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int visualizerWidth = (ABSOLUTE_MIN_WIDTH > preferred_width)
                                  ? ABSOLUTE_MIN_WIDTH
                                  : preferred_width;
        visualizerWidth =
            (visualizerWidth < text_width && text_width < term_w - 2)
                ? text_width
                : visualizerWidth;
        visualizerWidth =
            (visualizerWidth > term_w - 2) ? term_w - 2 : visualizerWidth;
        visualizerWidth -= 1;

        return visualizerWidth;
}

void print_at(int row, int indent, const char *text, int maxWidth)
{
        char buffer[1024];
        size_t len = strlen(text);

        if (len > (size_t)maxWidth)
                len = maxWidth;

        // Safe copy of exactly len bytes
        memcpy(buffer, text, len);
        buffer[len] = '\0';

        printf("\033[%d;%dH%s", row, indent, buffer);
}

void print_lyrics_page(UISettings *ui, AppSettings *settings, int row, int col, double seconds, SongData *songdata, int height)
{
        clear_rest_of_line();

        if (!songdata)
                return;

        Lyrics *lyrics = songdata->lyrics;

        if (!lyrics || lyrics->count == 0)
        {
                printf("\033[%d;%dH", row, col);
                printf(_(" No lyrics available. Press %s to go back."), settings->showLyricsPage);
                return;
        }

        apply_color(ui->colorMode, ui->theme.trackview_lyrics, ui->color);

        int limit = MIN((int)lyrics->count, height);
        int startat = 0;

        if (lyrics->isTimed)
        {
                for (int i = 0; i < (int)lyrics->count; i++)
                {
                        if (lyrics->lines[i].timestamp >= seconds)
                        {
                                if (i > limit && i > 0)
                                        startat = i - 1; // If the current line would have fallen out of view, start at the current line - 1
                                break;
                        }
                }
        }

        int newlimit = startat + limit;
        if (newlimit > (int)lyrics->count)
                newlimit = (int)lyrics->count;

        if (startat < 0)
                startat = 0;
        if (startat >= (int)lyrics->count)
                startat = (int)lyrics->count - 1;

        for (int i = startat; i < newlimit; i++)
        {
                char linebuf[1024];
                const char *text = lyrics->lines[i].text ? lyrics->lines[i].text : "";
                strncpy(linebuf, text, sizeof(linebuf) - 1);
                linebuf[sizeof(linebuf) - 1] = '\0';

                int length = (int)strnlen(linebuf, songdata->lyrics->maxLength - 1);
                if (length + col > term_w)
                        length = term_w - col;
                if (length < 0)
                        length = 0;

                print_at(row + i - startat, col, linebuf, length);
                clear_rest_of_line();
        }
}

const char *get_lyrics_line(const Lyrics *lyrics, double elapsed_seconds)
{
        if (!lyrics || lyrics->count == 0)
                return "";

        static char line[1024];
        line[0] = '\0';

        double lastTimestamp = -1.0;

        for (size_t i = 0; i < lyrics->count; i++)
        {
                double ts = lyrics->lines[i].timestamp;
                const char *text = lyrics->lines[i].text;

                if (elapsed_seconds < ts)
                        break;

                if (ts == lastTimestamp)
                {
                        // Same timestamp → append
                        strncat(line, " | ", sizeof(line) - strlen(line) - 1);
                        strncat(line, text, sizeof(line) - strlen(line) - 1);
                }
                else
                {
                        // New timestamp → start fresh
                        snprintf(line, sizeof(line), "%s", text);
                        lastTimestamp = ts;
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

        static char prevLine[1024] = "";

        if (line && line[0] != '\0' && (strcmp(line, prevLine) != 0))
        {
                strncpy(prevLine, line, sizeof(prevLine) - 1);
                prevLine[sizeof(prevLine) - 1] = '\0';

                int length = ((int)strnlen(line, songdata->lyrics->maxLength - 1));

                length -= col + length - term_w;

                apply_color(ui->colorMode, ui->theme.trackview_lyrics, ui->color);

                print_at(row, col, line, length);

                clear_rest_of_line();
        }
}

void show_track_view_landscape(int height, int width, float aspectRatio,
                            AppSettings *settings, SongData *songdata,
                            double elapsed_seconds)
{
        AppState *state = get_app_state();
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;
        int timeHeight = 1;

        if (songdata)
        {
                metadata = songdata->metadata;
        }

        int col = height * aspectRatio;

        if (!state->uiSettings.coverEnabled ||
            (songdata && songdata->cover == NULL))
                col = 1;

        int term_w, term_h;
        get_term_size(&term_w, &term_h);
        int visualizerWidth = term_w - col;

        int row = height - metadataHeight - timeHeight - state->uiSettings.visualizerHeight - 3;

        if (row <= 1)
                row = 2;

        if (is_refresh_triggered())
        {
                print_cover(height, songdata, &(state->uiSettings));

                if (!state->uiState.showLyricsPage)
                {
                        if (height > metadataHeight)
                                print_metadata(row, col, visualizerWidth - 1,
                                              metadata, &(state->uiSettings));
                }

                cancel_refresh();
        }
        if (songdata)
        {
                ma_uint32 sample_rate;
                ma_format format;
                avgBitRate = songdata->avgBitRate;
                get_format_and_sample_rate(&format, &sample_rate);

                if (!state->uiState.showLyricsPage)
                {
                        if (height > metadataHeight + timeHeight)
                                print_time(row + 4, col, elapsed_seconds, sample_rate,
                                          avgBitRate, term_w - col);

                        printf("\033[%d;%dH", row + metadataHeight + 1, col);
                        printf(" ");

                        if (row > 0)
                                print_timestamped_lyrics(&(state->uiSettings), songdata, row + metadataHeight + 1, col + 1, term_w, elapsed_seconds);

                        if (row > 0)
                                print_visualizer(row + metadataHeight + 2, col, visualizerWidth,
                                                settings, elapsed_seconds);

                        if (width - col > ABSOLUTE_MIN_WIDTH)
                        {
                                print_error_row(row + metadataHeight + 2 +
                                                  state->uiSettings.visualizerHeight,
                                              col, &(state->uiSettings));
                                print_footer(row + metadataHeight + 2 + state->uiSettings.visualizerHeight + 1, col);
                        }
                }
                else
                {
                        print_now_playing(songdata, &(state->uiSettings), 2, col, term_w - indent);
                        print_lyrics_page(&(state->uiSettings), settings, 4, col, elapsed_seconds, songdata, height - 4);
                }
        }
}

void show_track_view_portrait(int height, AppSettings *settings,
                           SongData *songdata, double elapsed_seconds)
{
        AppState *state = get_app_state();
        TagSettings *metadata = NULL;
        int avgBitRate = 0;

        int metadataHeight = 4;

        int row = height + 3;
        int col = indent;

        int visualizerWidth = calc_visualizer_width();

        if (is_refresh_triggered())
        {
                if (songdata)
                {
                        metadata = songdata->metadata;
                }

                clear_screen();

                if (!state->uiState.showLyricsPage)
                {
                        print_cover_centered(songdata, &(state->uiSettings));
                        print_metadata(row, col, visualizerWidth - 1, metadata,
                                      &(state->uiSettings));
                }

                cancel_refresh();
        }
        if (songdata)
        {
                if (!state->uiState.showLyricsPage)
                {
                        ma_uint32 sample_rate;
                        ma_format format;
                        avgBitRate = songdata->avgBitRate;

                        get_format_and_sample_rate(&format, &sample_rate);
                        print_time(row + metadataHeight, col, elapsed_seconds, sample_rate,
                                  avgBitRate, term_w - col);

                        if (row > 0)
                                print_timestamped_lyrics(&(state->uiSettings), songdata, row + metadataHeight + 1, indent + 1, term_w, elapsed_seconds);

                        print_visualizer(row + metadataHeight + 2, col, visualizerWidth,
                                        settings, elapsed_seconds);
                }
                else
                {
                        clear_screen();
                        printf("\n");
                        print_now_playing(songdata, &(state->uiSettings), 2, indent + 1, term_w - indent);
                        int term_w, term_h;
                        get_term_size(&term_w, &term_h);
                        int lyricsHeight = term_h - 5;
                        if (lyricsHeight > 0)
                                print_lyrics_page(&(state->uiSettings), settings, 4, col + 1, elapsed_seconds, songdata, lyricsHeight);
                }
        }

        calc_and_print_last_row_and_error_row();
}

void show_track_view(int width, int height, AppSettings *settings,
                   SongData *songdata, double elapsed_seconds)
{
        float aspect = get_aspect_ratio();

        if (aspect == 0.0f)
                aspect = 1.0f;

        int correctedWidth = width / aspect;

        if (correctedWidth > height * 2)
        {
                show_track_view_landscape(height, width, aspect, settings,
                                       songdata, elapsed_seconds);
        }
        else
        {
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

        if (!ui->uiEnabled)
        {
                return 0;
        }

        if (is_refresh_triggered())
        {
                hide_cursor();

                if (songdata != NULL && songdata->metadata != NULL &&
                    !songdata->hasErrors && (songdata->hasErrors < 1))
                {
                        ui->color.r = songdata->red;
                        ui->color.g = songdata->green;
                        ui->color.b = songdata->blue;

                        if (ui->trackTitleAsWindowTitle)
                                set_terminal_window_title(
                                    songdata->metadata->title);
                }
                else
                {
                        if (state->currentView == TRACK_VIEW)
                        {
                                state->currentView = LIBRARY_VIEW;
                                clear_screen();
                        }

                        ui->color.r = ui->kewColorRGB.r;
                        ui->color.g = ui->kewColorRGB.g;
                        ui->color.b = ui->kewColorRGB.b;

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

        if (state->currentView == KEYBINDINGS_VIEW && shouldRefresh)
        {
                clear_screen();
                show_key_bindings(songdata);
                save_cursor_position();
                cancel_refresh();
                fflush(stdout);
        }
        else if (state->currentView == PLAYLIST_VIEW && shouldRefresh)
        {
                show_playlist(songdata, unshuffled_playlist, &chosen_row,
                             &(uis->chosenNodeId), settings);
                state->uiState.resetPlaylistDisplay = false;
                fflush(stdout);
        }
        else if (state->currentView == SEARCH_VIEW && shouldRefresh)
        {
                show_search(songdata, &chosen_search_result_row);
                cancel_refresh();
                fflush(stdout);
        }
        else if (state->currentView == LIBRARY_VIEW && shouldRefresh)
        {
                show_library(songdata, settings);
                fflush(stdout);
        }
        else if (state->currentView == TRACK_VIEW)
        {
                show_track_view(term_w, term_h, settings, songdata, elapsed_seconds);
                fflush(stdout);
        }

        return 0;
}

void show_help(void) { print_help(); }

void free_main_directory_tree(void)
{
        AppState *state = get_app_state();
        FileSystemEntry *library = get_library();

        if (library == NULL)
                return;

        char *filepath = get_library_file_path();

        if (state->uiSettings.cacheLibrary)
                free_and_write_tree(library, filepath);
        else
                free_tree(library);

        free(filepath);
}

int get_chosen_row(void) { return chosen_row; }

void set_chosen_row(int row) { chosen_row = row; }

void refresh_player()
{
        AppState *state = get_app_state();
        PlaybackState *ps = get_playback_state();

        int mutexResult = pthread_mutex_trylock(&(state->switch_mutex));

        if (mutexResult != 0)
        {
                fprintf(stderr, "Failed to lock switch mutex.\n");
                return;
        }

        if (ps->notifyPlaying)
        {
                ps->notifyPlaying = false;

                emit_string_property_changed("PlaybackStatus", "Playing");
        }

        if (ps->notifySwitch)
        {
                ps->notifySwitch = false;

                notify_m_p_r_i_s_switch(get_current_song_data());
        }

        if (should_refresh_player())
        {
                print_player(get_current_song_data(), get_elapsed_seconds());
        }

        pthread_mutex_unlock(&(state->switch_mutex));
}
