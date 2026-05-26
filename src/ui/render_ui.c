/**
 * @file player_ui.c
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */
#define _XOPEN_SOURCE 700

#include "render_ui.h"

#include "common/events.h"

#include "chroma.h"
#include "common_ui.h"
#include "components.h"
#include "control_ui.h"
#include "input.h"

#include "render_terminal.h"
#include "settings.h"
#include "visuals.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common/model.h"
#include "common_ui.h"

#include "update/messages.h"

#include "ops/playback_state.h"

#include "data/img_func.h"
#include "data/playlist.h"

#include "utils/img_utils.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include <wchar.h>

static const int MAX_TERM_SIZE = 10000;

static Layout *s_layout;
static DrawBuffer *s_buf;
static TerminalBackendState s_backend_state;

static bool rebuilding = false;

static bool rendering = false;

void request_stop_visualization(void)
{
        AppState *state = get_app_state();
        state->settings.visualizations_instead_of_cover = false;
        state->ui.chroma_next_preset_requested = false;
        chroma_stop();
}

bool init_theme(int argc, char *argv[])
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->settings);
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
        UISettings *ui = &(state->settings);

        if (ui->trackTitleAsWindowTitle) {
                save_terminal_window_title();
                set_terminal_window_title("kew");
        }
}

void print_help(void)
{
        int i = system("man kew");

        if (i != 0) {
                printf(_("Run man kew for help.\n"));
        }
}

int print_logo_art_for_version(const UISettings *ui, int indent, bool centered, bool print_tag_line, bool use_gradient)
{
        int h, w;

        get_term_size(&w, &h);

        int centered_col = (w - LOGO_WIDTH) / 2;
        if (centered_col < 0)
                centered_col = 0;

        size_t logoHeight = LOGO_HEIGHT;

        int col = centered ? centered_col : indent;

        for (size_t i = 0; i < logoHeight; i++) {
                unsigned char default_color = ui->default_color;

                PixelData row_color = {ui->color.r, ui->color.g, ui->color.b, 255};

                if (use_gradient && !(ui->color.r == default_color &&
                                      ui->color.g == default_color &&
                                      ui->color.b == default_color)) {
                        row_color = get_gradient_color(ui->color, logoHeight - i,
                                                       logoHeight, 2, 0.8f);
                }

                apply_color(ui->colorMode, ui->theme.logo, row_color);

                clear_line();
                print_blank_spaces(col);
                printf("%s\n", LOGO[i]);
                fflush(stdout);
        }

        if (print_tag_line) {
                printf("\n");
                print_blank_spaces(col);
                printf("MUSIC  FOR  THE  SHELL\n");
        }

        return logoHeight; // lines used by logo
}

int print_logo_art(int row, int col, const UISettings *ui, bool centered, bool print_tag_line, bool use_gradient)
{
        int h, w;

        get_term_size(&w, &h);

        int centered_col = (w - LOGO_WIDTH) / 2;
        if (centered_col < 0)
                centered_col = 0;

        size_t logoHeight = LOGO_HEIGHT;

        col = centered ? centered_col : col;

        for (size_t i = 0; i < logoHeight; i++) {
                unsigned char default_color = ui->default_color;

                PixelData row_color = {ui->color.r, ui->color.g, ui->color.b, 255};

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

int print_logo_for_version(Model *model)
{
        int term_w, term_h;

        UISettings *ui = &model->state.settings;

        get_term_size(&term_w, &term_h);

        int height = 2;

        if (ui->hideLogo) {
                printf("\n");
                clear_line();
        } else {
                height = print_logo_art_for_version(ui, model->indent + 2, false, false, true);
        }

        printf("\n");
        clear_line();

        return height + 1;
}

int print_about_for_version(Model *model)
{
        UISettings *ui = &(model->state.settings);

        clear_line();
        int num_rows = print_logo_for_version(model);

        apply_color(ui->colorMode, ui->theme.text, ui->defaultColorRGB);
        print_blank_spaces(model->indent);
        printf(_("   kew version: "));
        apply_color(ui->colorMode, ui->theme.help, ui->color);
        printf("%s\n", ui->VERSION);
        clear_line();
        printf("\n");
        num_rows += 2;

        return num_rows;
}

void draw_buffer_clear(DrawBuffer *buf)
{
        Cell blank = {
            .codepoint = ' ',
            .attrs = ATTR_NONE,
            .kind = CELL_NORMAL,
            .image = NULL,
        };

        blank.style = cell_style_plain();

        int total = buf->cols * buf->rows;

        for (int i = 0; i < total; i++) {
                if (buf->cells[i].kind == CELL_IMAGE_ANCHOR && buf->cells[i].image) {
                        free(buf->cells[i].image->data);
                        buf->cells[i].image->data = NULL;
                        free(buf->cells[i].image);
                        buf->cells[i].image = NULL;
                }
        }

        for (int i = 0; i < total; i++) {
                buf->cells[i] = blank;
        }
}

void layout_offset(Layout *layout, int dx, int dy)
{
        for (int r = 0; r < layout->row_count; r++) {

                Row *row = &layout->rows[r];

                for (int c = 0; c < row->pane_count; c++) {

                        Pane *pane = &row->panes[c];

                        // store offset instead of mutating region repeatedly
                        pane->offsetX += dx;
                        pane->offsetY += dy;

                        if (pane->child) {
                                layout_offset(pane->child, dx, dy);
                        }
                }
        }
}

void handle_layout_options(Model *model, Layout *layout, Pane *pane, int row_num)
{
        Row *row = &layout->rows[row_num];

        if (pane->fn == component_visualizer ||
            pane->fn == component_vis_and_progress_bar ||
            pane->fn == component_progress_bar) {

                if (row->pane_count == 1 && !model->state.settings.visualizerEnabled) {
                        if (row_num > 0) {
                                layout->rows[row_num - 1].resolved_height += row->resolved_height;
                        }
                        row->height.value = 0;
                }

                if (pane->fn == component_vis_and_progress_bar && model->state.settings.visualizer_height + 1 != row->height.value) {
                        int diff = model->state.settings.visualizer_height + 1 - row->height.value;
                        row->height.value = model->state.settings.visualizer_height + 1;

                        if (row_num > 0) {
                                layout->rows[row_num - 1].resolved_height += diff;
                        }
                }
        }

        if (pane->fn == component_footer ||
            pane->fn == component_error_row) {
                if (row->pane_count == 1 && model->state.settings.hideFooter) {
                        if (row_num > 0) {
                                layout->rows[row_num - 1].resolved_height += row->resolved_height;
                        }
                        row->height.value = 0;
                }
        }

        if (pane->fn == component_logo ||
            pane->fn == component_logo_art) {
                if (row->pane_count == 1 && model->state.settings.hideLogo) {
                        if (row_num > 0) {
                                layout->rows[row_num - 1].resolved_height += row->resolved_height;
                        }
                        row->height.value = 3;
                }
        }

        if (pane->fn == component_library_header ||
            pane->fn == component_playlist_header ||
            pane->fn == component_search_header) {
                if (row->pane_count == 1 && model->state.settings.hideHelp) {
                        if (row_num > 0) {
                                layout->rows[row_num - 1].resolved_height += row->resolved_height;
                        }
                        row->height.value = 0;
                }
        }
}

void layout_reflow(Model *model,
                   Layout *layout,
                   int total_cols,
                   int total_rows,
                   int origin_x,
                   int origin_y)
{
        int fixed_rows = 0;
        int auto_count = 0;

        float aspect = get_aspect_ratio();
        if (aspect == 0.0f)
                aspect = 1.0f;

        if (!layout)
                return;

        // Resolve row heights
        for (int r = 0; r < layout->row_count; r++) {

                Row *row = &layout->rows[r];

                // Disable certain components due to settings
                for (int c = 0; c < row->pane_count; c++) {

                        Pane *pane = &row->panes[c];

                        handle_layout_options(model, layout, pane, r);
                }

                switch (row->height.kind) {

                case SIZE_FIXED:
                        row->resolved_height = row->height.value;
                        fixed_rows += row->resolved_height;
                        break;

                case SIZE_PERCENT:
                        row->resolved_height =
                            (total_rows * row->height.value) / 100;
                        fixed_rows += row->resolved_height;
                        break;

                case SIZE_AUTO:
                        auto_count++;
                        break;

                case SIZE_INDENT:
                        row->resolved_height = model->indent;
                        fixed_rows += row->resolved_height;
                        break;

                case SIZE_INDENT_WIDE:
                        row->resolved_height = model->indent_wide;
                        fixed_rows += row->resolved_height;
                        break;

                case SIZE_FROM_WIDTH:
                        row->resolved_height = total_cols / aspect;
                        fixed_rows += row->resolved_height;
                        break;

                case SIZE_FROM_HEIGHT:
                        break;

                case SIZE_WINDOW_MINUS:
                        row->resolved_height = total_rows - row->height.value;
                        fixed_rows += row->resolved_height;
                        break;
                }
        }

        int auto_height =
            auto_count > 0
                ? (total_rows - fixed_rows) / auto_count
                : 0;

        int current_row = 0;

        // Resolve panes
        for (int r = 0; r < layout->row_count; r++) {

                Row *row = &layout->rows[r];

                if (row->height.kind == SIZE_AUTO)
                        row->resolved_height = auto_height;

                int col = row->col;

                switch (row->col) {
                case COL_INDENT:
                        col = model->indent;
                        break;
                case COL_INDENT_WIDE:
                        col = model->indent_wide;
                        break;
                default:
                        break;
                }

                int available_cols = total_cols - col;

                int fixed_cols = 0;
                int auto_cols = 0;

                // Resolve widths
                for (int c = 0; c < row->pane_count; c++) {

                        Pane *pane = &row->panes[c];

                        switch (pane->width.kind) {

                        case SIZE_FIXED:
                                pane->region.width = pane->width.value;
                                fixed_cols += pane->region.width;
                                break;

                        case SIZE_PERCENT:
                                pane->region.width =
                                    (available_cols * pane->width.value) / 100;
                                fixed_cols += pane->region.width;
                                break;

                        case SIZE_AUTO:
                                auto_cols++;
                                break;

                        case SIZE_INDENT:
                                pane->region.width = model->indent;
                                fixed_cols += pane->region.width;
                                break;

                        case SIZE_INDENT_WIDE:
                                pane->region.width = model->indent_wide;
                                fixed_cols += pane->region.width;
                                break;

                        case SIZE_FROM_WIDTH:
                                break;

                        case SIZE_FROM_HEIGHT:
                                pane->region.width =
                                    row->resolved_height * aspect;
                                fixed_cols += pane->region.width;
                                break;

                        case SIZE_WINDOW_MINUS:
                                pane->region.width =
                                    total_cols - pane->width.value;
                                fixed_cols += pane->region.width;
                                break;
                        }
                }

                int auto_width =
                    auto_cols > 0
                        ? (available_cols - fixed_cols) / auto_cols
                        : 0;

                int current_col = col;

                // Finalize pane regions
                for (int c = 0; c < row->pane_count; c++) {

                        Pane *pane = &row->panes[c];

                        if (pane->width.kind == SIZE_AUTO)
                                pane->region.width = auto_width;

                        // Final position
                        pane->region.row =
                            origin_y + current_row + pane->offsetY;

                        pane->region.col =
                            origin_x + current_col + pane->offsetX;

                        pane->region.height = row->resolved_height;

                        // Recurse into child layout
                        if (pane->child) {

                                layout_reflow(
                                    model,
                                    pane->child,
                                    pane->region.width,
                                    pane->region.height,
                                    pane->region.col,
                                    pane->region.row);
                        }

                        current_col += pane->region.width;
                }

                current_row += row->resolved_height;
        }
}

static Pane *find_pane(Layout *layout, ComponentFn fn)
{
        Pane *res = {0};

        for (int r = 0; r < layout->row_count; r++) {
                Row *row = &layout->rows[r];
                for (int p = 0; p < row->pane_count; p++) {
                        if (row->panes[p].fn == fn)
                                return &row->panes[p];
                        if (row->panes[p].child) {
                                res = find_pane(row->panes[p].child, fn);

                                if (res)
                                        return res;
                        }
                }
        }
        return NULL;
}

Layout *build_playlist_layout(Model *model)
{
        Layout *layout = model->playlist_layout;
        layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);

        Pane *list = find_pane(layout, component_playlist_rows);
        if (list)
                model->state.ui.max_playlist_rows = list->region.height - 1;

        return layout;
}

Layout *build_library_layout(Model *model)
{
        Layout *layout = model->library_layout;
        layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);

        Pane *lib = find_pane(layout, component_library_rows);
        if (lib)
                model->state.ui.max_lib_rows = lib->region.height;

        return layout;
}

Layout *build_track_layout(Model *model)
{
        float aspect = get_aspect_ratio();
        if (aspect == 0.0f)
                aspect = 1.0f;

        int corrected_width = model->term_w / aspect;

        if (corrected_width > model->term_h * 2) {
                Layout *layout = model->track_landscape_layout;
                layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);
                return layout;
        }

        Layout *layout = model->track_layout;
        layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);

        return layout;
}

Layout *build_search_layout(Model *model)
{
        Layout *layout = model->search_layout;
        layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);

        Pane *search = find_pane(layout, component_search_results);
        if (search)
                model->state.ui.max_search_rows = search->region.height;

        return layout;
}

Layout *build_help_layout(Model *model)
{
        Layout *layout = model->help_layout;
        layout_reflow(model, layout, model->term_w, model->term_h, 0, 0);

        return layout;
}

void draw_buffer_resize(int cols, int rows)
{
        DrawBuffer *buf = s_buf;
        if (!buf)
                return;

        Cell *new_cells = realloc(buf->cells, rows * cols * sizeof(Cell));
        bool *new_dirty = realloc(buf->dirty_rows, rows * sizeof(bool));

        if (!new_cells || !new_dirty) {
                return;
        }

        buf->cells = new_cells;
        buf->dirty_rows = new_dirty;

        buf->cols = cols;
        buf->rows = rows;

        // initialize new regions
        memset(buf->dirty_rows, 0, rows * sizeof(bool));
}

DrawBuffer *draw_buffer_create(int cols, int rows)
{
        DrawBuffer *buf = malloc(sizeof(DrawBuffer));
        buf->cols = cols;
        buf->rows = rows;
        buf->cells = calloc(rows * cols, sizeof(Cell));
        buf->dirty_rows = calloc(rows, sizeof(bool));
        return buf;
}

void draw_buffer_destroy(DrawBuffer *buf)
{
        if (!buf)
                return;

        int total = buf->rows * buf->cols;

        for (int i = 0; i < total; i++) {
                Cell *cell = &buf->cells[i];

                if (cell->kind == CELL_IMAGE_ANCHOR && cell->image) {
                        free_image_payload(cell->image);
                        cell->image = NULL;
                }
        }

        free(buf->cells);
        buf->cells = NULL;

        free(buf->dirty_rows);
        buf->dirty_rows = NULL;

        buf->rows = 0;
        buf->cols = 0;

        free(buf);
}

int calc_indent_track_view(Model *model)
{
        int text_width = (ABSOLUTE_MIN_WIDTH > model->preferred_width) ? ABSOLUTE_MIN_WIDTH
                                                                       : model->preferred_width;

        if (!model->songdata || !model->songdata->metadata)
                return get_indentation(text_width - 1) - 1;

        TagSettings *metadata = model->songdata->metadata;

        if (metadata == NULL)
                return calc_indent_normal();

        int title_length = strnlen(metadata->title, METADATA_MAX_LENGTH);
        int album_length = strnlen(metadata->album, METADATA_MAX_LENGTH);
        int max_text_length =
            (album_length > title_length) ? album_length : title_length;

        int max_size = model->term_w - 2;
        if (max_text_length > 0 && max_text_length < max_size &&
            max_text_length > text_width)
                text_width = max_text_length;
        if (text_width > max_size)
                text_width = max_size;

        return get_indentation(text_width - 1) - 1;
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

        Model *model = get_model();

        model->state.ui.aspect_ratio = aspect_ratio;

        if (model->term_w <= 0 || model->term_h <= 0 || model->term_w > MAX_TERM_SIZE ||
            model->term_h > MAX_TERM_SIZE) {
                *width = 1;
                *height = 1;
                return -1;
        }

        const int time_display_height = 1;
        const int height_margin = 4;
        const int min_height = visualizer_height + metatag_height +
                               time_display_height + height_margin;

        if (min_height < 0 || min_height > model->term_h) {
                *width = 1;
                *height = 1;
                return -1;
        }

        int available_height = model->term_h - min_height;
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

        if (calc_width > model->term_w) {
                calc_width = model->term_w;
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

void calc_preferred_img_size(Model *model)
{
        model->min_height = 2 + (model->state.settings.visualizerEnabled ? model->state.settings.visualizer_height : 0);
        int metadata_height = 4;
        calc_ideal_img_size(&model->preferred_width, &model->preferred_height,
                            (model->state.settings.visualizerEnabled ? model->state.settings.visualizer_height : 0),
                            metadata_height);
}

int calc_indent_normal()
{
        Model *model = get_model();

        int text_width = (ABSOLUTE_MIN_WIDTH > model->preferred_width)
                             ? ABSOLUTE_MIN_WIDTH
                             : model->preferred_width;
        return get_indentation(text_width - 1) - 1;
}

void calc_indent(Model *model)
{
        if ((model->state.currentView == TRACK_VIEW && model->songdata == NULL) ||
            model->state.currentView != TRACK_VIEW) {
                model->indent = calc_indent_normal();
        } else {
                model->indent = calc_indent_track_view(model);
        }

        model->indent_wide = model->indent + (model->indent / 4);
        if (model->state.settings.hideSideCover)
                model->indent_wide = model->indent;
}

void calc_visualizer_width(Model *model)
{
        int visualizer_width = (ABSOLUTE_MIN_WIDTH > model->preferred_width)
                                   ? ABSOLUTE_MIN_WIDTH
                                   : model->preferred_width;
        visualizer_width =
            (visualizer_width > model->term_w - 2) ? model->term_w - 2 : visualizer_width;
        visualizer_width -= 1;

        model->state.ui.visualizer_width = visualizer_width;
}

void rebuild_layout(Model *model)
{
        calc_preferred_img_size(model);
        calc_indent(model);
        calc_visualizer_width(model);

        if (rebuilding)
                return;

        rebuilding = true;

        draw_buffer_clear(s_buf);

        switch (model->state.currentView) {

        case PLAYLIST_VIEW:
                s_layout = build_playlist_layout(model);
                break;

        case LIBRARY_VIEW:
                s_layout = build_library_layout(model);
                break;

        case TRACK_VIEW:
                s_layout = build_track_layout(model);
                break;

        case SEARCH_VIEW:
                s_layout = build_search_layout(model);
                break;

        case HELP_VIEW:
                s_layout = build_help_layout(model);
                break;
        }

        Pane *footer = find_pane(s_layout, component_footer);
        if (footer) {
                model->state.ui.footer_row = footer->region.row + 1;
                model->state.ui.footer_col = footer->region.col + 1;
        }

        rebuilding = false;
}

Layout *build_layout_by_name(const char *name)
{
        Layout *layout = NULL;

        if (config_has_section(name)) {
                layout = load_layout_from_config(name);
                if (layout)
                        return layout;
        }

        ensure_default_layouts();
        free_layout_config();
        load_layout_config();

        if (config_has_section(name)) {
                layout = load_layout_from_config(name);
                if (layout)
                        return layout;
        }

        if (!layout) {
                set_error_message("Layout not found. Please reinstall kew, or run 'sudo make install' if kew was installed by running make.");
                dispatch_msg((struct Msg){.type = MSG_QUIT});
        }

        return layout;
}

void ui_init(Model *model)
{
        calc_indent(model);

        load_layout_config();

        // Build templates from config or hardcoded fallback
        model->library_layout = build_layout_by_name("library");
        model->playlist_layout = build_layout_by_name("playlist");
        model->search_layout = build_layout_by_name("search");
        model->help_layout = build_layout_by_name("help");
        model->track_layout = build_layout_by_name("track");
        model->track_landscape_layout = build_layout_by_name("track_landscape");

        component_library_helper_reset(model);

        s_buf = draw_buffer_create(model->term_w, model->term_h);
}

void layout_destroy(Layout *layout)
{
        if (!layout)
                return;

        for (int i = 0; i < layout->row_count; i++) {
                Row *row = &layout->rows[i];

                for (int j = 0; j < row->pane_count; j++) {
                        Pane *pane = &row->panes[j];

                        if (pane->child) {
                                layout_destroy(pane->child);
                                pane->child = NULL;
                        }
                }
        }

        free(layout);
}

void ui_destroy(Model *model)
{
        if (!model)
                return;

        layout_destroy(model->library_layout);
        layout_destroy(model->playlist_layout);
        layout_destroy(model->search_layout);
        layout_destroy(model->help_layout);
        layout_destroy(model->track_layout);
        layout_destroy(model->track_landscape_layout);

        model->library_layout = NULL;
        model->playlist_layout = NULL;
        model->search_layout = NULL;
        model->help_layout = NULL;
        model->track_layout = NULL;
        model->track_landscape_layout = NULL;

        if (s_buf) {
                draw_buffer_destroy(s_buf);
                s_buf = NULL;
        }
}

void layout_render_dirty(const Layout *layout,
                         Model *model,
                         DrawBuffer *buf, RenderContext *ctx)
{
        bool should_draw = false;

        if (!layout)
                return;

        for (int r = 0; r < layout->row_count; r++) {

                const Row *row = &layout->rows[r];

                for (int c = 0; c < row->pane_count; c++) {

                        const Pane *pane = &row->panes[c];

                        // Child layout
                        if (pane->child) {

                                layout_render_dirty(
                                    pane->child,
                                    model,
                                    buf, ctx);

                                continue;
                        }

                        if (model->dirty == DIRTY_ALL) {
                                should_draw = true;
                        } else {
                                should_draw =
                                    (pane->redraws_on & model->dirty) != 0;
                        }

                        if (should_draw) {

                                // Tells main loop to render these components more often
                                if (pane->fn == component_visualizer ||
                                    pane->fn == component_time ||
                                    pane->fn == component_timestamped_lyrics ||
                                    pane->fn == component_track ||
                                    pane->fn == component_track_portrait_normal ||
                                    pane->fn == component_track_landscape ||
                                    pane->fn == component_track_landscape_normal ||
                                    pane->fn == component_vis_and_progress_bar ||
                                    model->state.ui.chroma_started) {

                                        ctx->render_often = true;
                                }

                                if (pane->fn == component_track ||
                                    pane->fn == component_track_portrait_normal ||
                                    pane->fn == component_track_landscape ||
                                    pane->fn == component_track_landscape_normal ||
                                    pane->fn == component_vis_and_progress_bar) {

                                        if (!model->title_delay.active && (model->dirty & DIRTY_SONG || model->dirty == DIRTY_ALL))

                                                dispatch_msg((struct Msg){
                                                    .type = MSG_START_TITLE_ANIM});
                                }

                                // Tells main loop to render these components more often
                                if (pane->fn == component_search_results ||
                                    pane->fn == component_search ||
                                    pane->fn == component_search_box) {

                                        ctx->render_search = true;
                                }

                                ComponentMsg result = pane->fn(model, pane->region, buf, model->dirty);

                                if (result.has_msg)
                                        dispatch_msg(result.msg);
                        }
                }
        }
}

void render_ui(Model *model, RenderContext *ctx)
{
        if (model->dirty == DIRTY_NONE || !model->state.settings.uiEnabled)
                return;

        if (rendering || rebuilding)
                return;

        if (model->dirty == DIRTY_ALL) {
                memset(s_buf->dirty_rows, 0, s_buf->rows);
                rebuild_layout(model);
        }

        rendering = true;

        s_backend_state.render_chroma = (model->state.ui.chroma_start_requested || model->state.ui.chroma_started) &&
                                        model->state.currentView == TRACK_VIEW;

        layout_render_dirty(s_layout, model, s_buf, ctx);
        terminal_backend_commit(s_buf, model->dirty, &s_backend_state);

        rendering = false;
}
