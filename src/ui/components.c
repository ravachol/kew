#include "components.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"
#include "common/model.h"

#include "data/directorytree.h"
#include "data/img_func.h"
#include "ops/library_ops.h"
#include "ops/playback_state.h"

#include "common_ui.h"
#include "settings.h"
#include "sound/audiotypes.h"
#include "ui/playlist_ui.h"
#include "visuals.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <math.h>
#include <stdio.h>
#include <sys/param.h>

// clang-format off
const char *LOGO[] =  {"  __",
                       " |  |--.-----.--.--.--.",
                       " |    <|  -__|  |  |  |",
                       " |__|__|_____|________|"};
// clang-format on

bool found_last_parent = false;

void prepare_playlist_string(Node *node, char *buffer, int buffer_size)
{
        if (node == NULL || buffer == NULL || node->song.file_path == NULL ||
            buffer_size <= 0) {
                if (buffer && buffer_size > 0)
                        buffer[0] = '\0';
                return;
        }

        if (strnlen(node->song.file_path, PATH_MAX) >= PATH_MAX) {
                buffer[0] = '\0';
                return;
        }

        char file_path[PATH_MAX];
        c_strcpy(file_path, node->song.file_path, sizeof(file_path));

        char *last_slash = strrchr(file_path, '/');
        size_t len = strnlen(file_path, sizeof(file_path));

        if (last_slash != NULL && last_slash < file_path + len) {
                c_strcpy(buffer, last_slash + 1, buffer_size);
                buffer[buffer_size - 1] = '\0';
        } else {
                // If no slash found or invalid pointer arithmetic, just copy
                // whole path safely or clear
                c_strcpy(buffer, file_path, buffer_size);
                buffer[buffer_size - 1] = '\0';
        }
}

static void draw_search_row(DrawBuffer *buf, int row, int col, int width,
                            const char *name, int extra_indent,
                            FileSystemEntry *entry, const UISettings *ui,
                            bool is_chosen, bool is_playing)
{
        ColorValue tc;

        if (is_playing)
                tc = ui->theme.search_playing;
        else if (entry->is_enqueued)
                tc = ui->theme.search_enqueued;
        else if (entry->parent != NULL && entry->parent->parent == NULL)
                tc = ui->theme.library_artist;
        else
                tc = ui->theme.search_result;

        CellStyle plain_style = cell_style_from_theme(tc);
        CellStyle rev_style = plain_style;
        rev_style.attrs |= ATTR_REVERSE;

        CellStyle name_style = plain_style;
        if (is_playing && !is_chosen)
                name_style.attrs |= ATTR_UNDERLINE;

        CellStyle blank = cell_style_plain();

        int indent_col = col + extra_indent;
        int reverse_col = is_chosen ? (indent_col + (entry->is_enqueued ? 1 : 2))
                                    : (indent_col + 3);

        // Clear from col to reverse_col with blank
        draw_buffer_set_string_truncated(buf, row, col, "",
                                         reverse_col - col, blank);

        // Write the plain part of the prefix after indent
        const char *prefix = entry->is_enqueued ? " * " : "   ";
        int plain_prefix_len = reverse_col - indent_col;
        char plain_part[8];
        snprintf(plain_part, sizeof(plain_part), "%.*s", plain_prefix_len, prefix);
        draw_buffer_set_string(buf, row, indent_col, plain_part, plain_style);

        // Write reversed remainder of prefix + name
        char rest[512];
        snprintf(rest, sizeof(rest), "%.*s%s",
                 3 - plain_prefix_len, prefix + plain_prefix_len,
                 name);
        draw_buffer_set_string_truncated(buf, row, reverse_col,
                                         rest, width - (reverse_col - col),
                                         is_chosen ? rev_style : name_style);
}

int determine_depth(FileSystemEntry *entry)
{
        int depth = 0;
        bool found_parent = false;

        Model *model = get_model();

        if (entry->parent && model->state.ui.last_search_parent && entry->parent->id == model->state.ui.last_search_parent->id) {
                found_parent = found_last_parent;
        } else {
                for (int i = 0; i < model->state.ui.search_results_count; i++) {
                        if (entry->parent && model->search_results[i].entry->id == entry->parent->id) {
                                found_parent = true;
                                model->state.ui.last_search_parent = entry->parent;
                        }
                }
        }

        if (found_parent) {
                found_last_parent = true;

                while (entry->parent != NULL) {
                        entry = entry->parent;
                        depth++;
                }
        }

        return depth;
}

int calc_indentation(int depth)
{
        return depth * 2;
}

FileSystemEntry *get_first_parent(FileSystemEntry *entry)
{
        FileSystemEntry *first_parent = entry;
        while (first_parent != NULL) {
                // if root is above first_parent
                if (first_parent->parent != NULL && first_parent->parent->parent == NULL)
                        break;

                first_parent = first_parent->parent;
        }

        return first_parent;
}

void component_library_helper_collapse_top_level(Model *model, int direction)
{
        TreeContext *ctx = &model->state.ui.treeCtx;

        FileSystemEntry *first_parent = get_first_parent(ctx->chosen_dir);
        if (model->state.ui.allowChooseSongs &&
            model->state.settings.collapseTopLevel &&
            ctx->chosen_dir) {
                int num_dirs = count_directories_in_directory(first_parent);
                int num_children = 0;
                if (ctx->chosen_dir != NULL) {
                        FileSystemEntry *child = ctx->chosen_dir->children;

                        while (child != NULL) {
                                if (!child->is_directory)
                                        num_children++;
                                child = child->next;
                        }
                }

                model->state.ui.allowChooseSongs = false;
                if (direction > 0)
                        model->state.ui.chosen_lib_row -= num_dirs + num_children;
                model->state.ui.chosen_dir = ctx->chosen_dir = NULL;
        }
}

void component_library_helper_collapse_view(Model *model, int diff_rows)
{
        UIState *uis = &model->state.ui;
        TreeContext *ctx = &model->state.ui.treeCtx;

        if ((uis->allowChooseSongs || model->state.settings.collapseTopLevel) &&
            (ctx->chosen_dir == NULL ||
             (uis->current_lib_entry != NULL && uis->current_lib_entry->parent != NULL &&
              ctx->chosen_dir != NULL))) {

                set_dirty(DIRTY_LIBRARY);

                int num_children = 0;
                if (ctx->chosen_dir != NULL && diff_rows > 0) {
                        FileSystemEntry *child = ctx->chosen_dir->children;

                        while (child != NULL) {
                                child = child->next;
                                num_children++;
                        }
                }

                // Handle case for when library is collapsed and only artists are visible
                if (model->state.settings.collapseTopLevel) {
                        // If we are exiting the artist, collapse all the albums in it
                        if (ctx->chosen_dir && ctx->chosen_dir->parent && !ctx->chosen_dir->parent->parent &&
                            (strcmp(model->state.ui.current_lib_entry->parent->full_path, ctx->chosen_dir->full_path) == 0 ||
                             (strcmp(model->state.ui.current_lib_entry->full_path, ctx->chosen_dir->full_path) == 0 && diff_rows < 0))) {
                                component_library_helper_collapse_top_level(model, diff_rows);
                        } else if (ctx->chosen_dir && ctx->chosen_dir->parent && ctx->chosen_dir->parent->parent) {

                                // Else if we are exiting a folder within the artist, collapse only that album
                                if (diff_rows > 0 && !ctx->chosen_dir->next)
                                        component_library_helper_collapse_top_level(model, diff_rows);
                                else
                                        model->state.ui.chosen_lib_row -= num_children;
                                model->state.ui.chosen_dir = ctx->chosen_dir = get_first_parent(ctx->chosen_dir);
                        }
                } else {
                        uis->allowChooseSongs = false;
                        model->state.ui.chosen_dir = ctx->chosen_dir = NULL;
                        model->state.ui.chosen_lib_row -= num_children;
                }
        }
}

void component_library_helper_reset(Model *model)
{
        model->state.ui.chosen_dir = NULL;
        model->state.ui.chosen_lib_row = 0;

        model->state.ui.treeCtx.chosen_dir = NULL;
        model->state.ui.treeCtx.start_lib_iter = 0;
        model->state.ui.treeCtx.previous_chosen_row = -1;

        model->state.ui.current_lib_entry = NULL;
        model->state.ui.current_search_entry = NULL;
        model->state.ui.chosen_search_dir = NULL;
        model->state.ui.chosen_dir = NULL;
        model->state.ui.last_search_parent = NULL;
}

static FileSystemEntry *component_library_helper_render_node(const Model *model, FileSystemEntry *entry, int depth,
                                                             int max_list_size, int max_name_width,
                                                             k_Rect region, DrawBuffer *buf, int *row_count, int *iter)
{
        if (entry == NULL)
                return NULL;

        if (max_name_width < 0)
                max_name_width = 0;

        TreeContext ctx = model->state.ui.treeCtx;
        const AppState *state = &model->state;
        const UISettings *ui = &state->settings;
        const UIState *uis = &state->ui;
        Node *current = get_current_song();

        FileSystemEntry *found_chosen = NULL;

        int is_playing = 0;
        int extra_indent = 0;

        if (*iter >= ctx.start_lib_iter + model->state.ui.max_lib_rows) {
                (*iter)++;
                return false;
        }

        if (current != NULL &&
            strcmp(current->song.file_path, entry->full_path) == 0)
                is_playing = 1;

        // Visibility check
        bool show = (entry->is_directory && depth == 1) ||
                    (entry->is_directory && !model->state.settings.collapseTopLevel) ||
                    (!entry->is_directory && depth == 1) ||
                    (entry->is_directory && depth == 0) ||
                    (ctx.chosen_dir != NULL && uis->allowChooseSongs && entry->parent != NULL &&
                     (strcmp(entry->parent->full_path, ctx.chosen_dir->full_path) == 0 || strcmp(entry->full_path, ctx.chosen_dir->full_path) == 0));

        if (ctx.chosen_dir && model->state.settings.collapseTopLevel) {
                FileSystemEntry *artist = get_first_parent(ctx.chosen_dir);
                FileSystemEntry *entry_artist = get_first_parent(entry);

                show = show ||
                       (artist && entry_artist &&
                        ((entry->is_directory && strcmp(artist->full_path, entry_artist->full_path) == 0) || (strcmp(artist->full_path, entry->parent->full_path) == 0)));
        }

        if (!show)
                return NULL;

        // Colors
        PixelData track_color = {ui->default_color, ui->default_color, ui->default_color, 255};
        PixelData enqueued_color = {ui->default_color, ui->default_color, ui->default_color, 255};
        PixelData rgb_track = {ui->default_color, ui->default_color, ui->default_color, 255};
        PixelData rgb_enqueued = {ui->default_color, ui->default_color, ui->default_color, 255};

        CellStyle track_style = cell_style_from_theme(ui->theme.library_track);
        CellStyle enqueued_style = cell_style_from_theme(ui->theme.library_enqueued);
        CellStyle playing_style = cell_style_from_theme(ui->theme.library_playing);

        if (!track_style.isAnsi)
                rgb_track = track_style.fg;

        if (!enqueued_style.isAnsi)
                rgb_enqueued = enqueued_style.fg;

        if (!(rgb_track.r == ui->default_color &&
              rgb_track.g == ui->default_color &&
              rgb_track.b == ui->default_color))
        {
                track_color = get_gradient_color(rgb_track,
                                               *iter - ctx.start_lib_iter,
                                               max_list_size, max_list_size / 2, 0.7f);

                if (!track_style.isAnsi)
                        track_style.fg = track_color;
        }

        if (!(rgb_enqueued.r == ui->default_color &&
              rgb_enqueued.g == ui->default_color &&
              rgb_enqueued.b == ui->default_color))
        {
                enqueued_color = get_gradient_color(rgb_enqueued,
                                                *iter - ctx.start_lib_iter,
                                                max_list_size, max_list_size / 2, 0.7f);
                if (!enqueued_style.isAnsi)
                        enqueued_style.fg = enqueued_color;
        }


        // Render this node
        if (depth >= 0) {

                if (*iter >= ctx.start_lib_iter) {
                        int draw_row = region.row + *row_count;

                        if (draw_row >= region.row + region.height)
                                return NULL;

                        // Extra indent for deep nesting
                        if (depth >= 2)
                                extra_indent += 2;

                        extra_indent += (depth - 2 <= 0) ? 0 : depth - 2;

                        int draw_col = region.col;

                        bool is_chosen = (uis->chosen_lib_row == *iter);

                        // Chosen row state update
                        if (is_chosen) {
                                found_chosen = entry;
                        }

                        CellStyle item_style = cell_style_plain();

                        if (entry->is_enqueued || (depth <= 1 && entry->is_directory))
                                item_style = enqueued_style;
                        else
                                item_style = track_style;

                        if (is_playing)
                                item_style = playing_style;

                        char empty_space[8];
                        snprintf(empty_space, sizeof(empty_space), "%*s",
                                 extra_indent, "");

                        draw_buffer_set_string(buf, draw_row, draw_col, empty_space, item_style);

                        char prefix[8];
                        snprintf(prefix, sizeof(prefix), "%s",
                                 entry->is_enqueued ? " * " : "   ");

                        if (is_chosen && entry->is_enqueued)
                                item_style.attrs |= ATTR_REVERSE;

                        draw_buffer_set_string(buf, draw_row, draw_col + extra_indent, prefix, item_style);
                        int text_col = draw_col + extra_indent + 3;

                        if (max_name_width < extra_indent)
                                max_name_width = extra_indent;

                        if (is_chosen)
                                item_style.attrs |= ATTR_REVERSE;

                        int name_width = max_name_width - extra_indent;

                        // Directory
                        if (entry->is_directory) {
                                char dir_name[256];
                                char orig_name[256];
                                dir_name[0] = '\0';

                                snprintf(orig_name, sizeof(orig_name), "%s", entry->name);

                                if (strcmp(orig_name, "root") == 0) {
                                        snprintf(orig_name, sizeof(orig_name), "%s", _("─ MUSIC LIBRARY ─"));
                                        item_style = cell_style_from_theme(ui->theme.header);
                                        if (is_chosen)
                                                item_style.attrs |= ATTR_REVERSE;
                                }

                                if (depth == 1) {
                                        char *upper = string_to_upper(orig_name);
                                        snprintf(orig_name, sizeof(orig_name), "%s", upper);
                                        free(upper);
                                }

                                snprintf(dir_name, sizeof(dir_name), "%s", orig_name);

                                if (found_chosen != NULL)
                                        process_name_scroll(model, orig_name, dir_name, name_width, false, false);
                                else
                                        process_name(orig_name, dir_name, name_width, false, false);

                                draw_buffer_set_string(buf, draw_row, text_col, dir_name, item_style);

                                // File
                        } else {
                                // "└─ " prefix for files
                                CellStyle file_style = item_style;
                                if (is_playing && !is_chosen)
                                        file_style.attrs |= ATTR_UNDERLINE;

                                // playlist icon
                                if (is_m3u_file(entry)) {
                                        draw_buffer_set_string(buf, draw_row, text_col, "♫ ", file_style);
                                        text_col += 2;
                                        name_width -= 2;
                                }

                                draw_buffer_set_string(buf, draw_row, text_col, "└─ ", file_style);
                                text_col += 3;
                                name_width -= 3;

                                char filename[name_width * 4 + 1];
                                filename[0] = '\0';

                                str_truncate_display_width(entry->name, filename, name_width);

                                if (found_chosen != NULL)
                                        process_name_scroll(model, entry->name, filename, name_width, true, true);
                                else
                                        process_name(entry->name, filename, name_width, true, true);

                                draw_buffer_set_string_truncated(buf, draw_row, text_col, filename, name_width, file_style);
                        }

                        (*row_count)++;
                }

                (*iter)++;
        }

        // Call render_tree_node recursively
        FileSystemEntry *child = entry->children;
        while (child != NULL) {

                FileSystemEntry *result;

                result = component_library_helper_render_node(model, child, depth + 1, max_list_size,
                                                              max_name_width, region, buf, row_count, iter);

                if (result && !found_chosen)
                        found_chosen = result;

                child = child->next;
        }

        return found_chosen;
}

void component_library_helper_update_view_state(Model *model)
{
        TreeContext *ctx = &model->state.ui.treeCtx;

        ctx->chosen_dir = model->state.ui.chosen_dir;

        ctx->start_lib_iter = 0;

        if (ctx->start_lib_iter < 0)
                ctx->start_lib_iter = 0;

        int threshold = ctx->start_lib_iter + (model->state.ui.max_lib_rows + 1) / 2;

        if (model->state.ui.chosen_lib_row > threshold)
                ctx->start_lib_iter = model->state.ui.chosen_lib_row - model->state.ui.max_lib_rows / 2 + 1;

        if (model->state.ui.chosen_lib_row < 0)
                ctx->start_lib_iter = model->state.ui.chosen_lib_row = 0;

        model->state.ui.chosen_dir = ctx->chosen_dir;
}

int get_year(const char *date_string)
{
        int year;

        if (sscanf(date_string, "%d", &year) != 1) {
                return -1;
        }
        return year;
}

int calc_elapsed_bars(double elapsed_seconds, double duration, int num_progress_bars)
{
        if (elapsed_seconds == 0)
                return 0;

        return (int)((elapsed_seconds / duration) * num_progress_bars);
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

static const char *get_player_status_icon(const Model *model)
{
        if (model->is_paused) {
#if defined(__ANDROID__) || defined(__APPLE__)
                return "။";
#else
                return "⏸";
#endif
        }
        if (model->is_paused)
                return "■";
        return "▶";
}

static void build_song_title(const Model *model, const UISettings *ui,
                             char *out, size_t out_size,
                             bool show_play_icon, bool show_artist)
{
        SongData *song_data = model->songdata;

        if (!song_data || !song_data->metadata) {
                out[0] = '\0';
                return;
        }

        char pretty_title[METADATA_MAX_LENGTH] = {0};
        snprintf(pretty_title, METADATA_MAX_LENGTH, "%s",
                 song_data->metadata->title);
        trim(pretty_title, strlen(pretty_title));

        bool has_artist = song_data->metadata->artist[0] != '\0';
        bool want_artist = show_artist || ui->hideLogo;
        const char *icon = get_player_status_icon(model);

        if (show_play_icon && want_artist && has_artist)
                snprintf(out, out_size, "%s %s - %s", icon,
                         song_data->metadata->artist, pretty_title);
        else if (show_play_icon)
                snprintf(out, out_size, "%s %s", icon, pretty_title);
        else if (want_artist && has_artist)
                snprintf(out, out_size, "%s - %s",
                         song_data->metadata->artist, pretty_title);
        else {
                strncpy(out, pretty_title, out_size - 1);
                out[out_size - 1] = '\0';
        }
}

void now_playing(bool show_artist, const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        if (!model->songdata)
                return;

        const UISettings *ui = &model->state.settings;

        char title[PATH_MAX + 1];

        build_song_title(model, ui, title, sizeof(title), true, show_artist);

        CellStyle style = cell_style_from_theme(ui->theme.nowplaying);

        draw_buffer_set_string_truncated(buf, region.row, region.col, "", region.width, cell_style_plain());

        if (title[0] != '\0') {
                char processed[PATH_MAX + 1] = {0};
                process_name(title, processed, region.width, false, false);
                draw_buffer_set_string_truncated(buf, region.row, region.col, processed, region.width, style);
        }
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

static void render_glimmer_frame(const Model *model, DrawBuffer *buf, const char *text, int text_length,
                                 const char *icons, PixelData color, int row, int col,
                                 const AnimationState *g, int max_width, CellStyle style)
{
        if (!is_ascii_only(text))
                return;

        PixelData vbright = increase_luminosity(color, 120);
        PixelData bright = increase_luminosity(color, 60);

        // Write each character with its per-frame color
        int draw_col = col;
        for (int i = 0; i < text_length; i++) {
                PixelData c;
                if (i == g->frame)
                        c = vbright;
                else if (i == g->frame - 1 || i == g->frame + 1)
                        c = bright;
                else
                        c = color;

                CellStyle style = cell_style_from_theme(model->state.settings.theme.footer);
                style.isAnsi = false;
                style.fg = c;
                char ch[2] = {text[i], '\0'};
                draw_buffer_set_string(buf, row, draw_col, ch, style);
                draw_col++;
        }

        draw_buffer_set_string_truncated(buf, row, draw_col, icons, max_width, style);
}

static int draw_cover_ascii(const TermSize *term_size, const char *path, int row, int col,
                            unsigned int height, bool centered,
                            DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        int cell_width = 8;
        int cell_height = 16;

        if (term_size->width_cells > 0 &&
            term_size->height_cells > 0 &&
            term_size->width_pixels > 0 &&
            term_size->height_pixels > 0) {

                cell_width =
                    term_size->width_pixels / term_size->width_cells;

                cell_height =
                    term_size->height_pixels / term_size->height_cells;
        }

        float aspect = (float)cell_height / (float)cell_width;
        unsigned int corr_w = (unsigned int)(height * aspect);

        int rwidth, rheight, rchannels;

        unsigned char *read_data =
            image_load_rgb(path, &rwidth, &rheight, &rchannels);

        if (!read_data)
                return -1;

        float scale_w = (float)corr_w / (float)rwidth;
        float scale_h = (float)height / (float)rheight;

        int fit_w = (int)(rwidth * scale_w + 0.5f);
        int fit_h = (int)(rheight * scale_h + 0.5f);

        fit_w = CLAMP(fit_w, 1, (int)corr_w);
        fit_h = CLAMP(fit_h, 1, (int)height);

        unsigned char *composed =
            calloc((size_t)3 * corr_w * height, 1);

        if (!composed) {
                image_free(read_data);
                return -1;
        }

        unsigned char *fit_pixels = read_data;
        bool did_resize = false;

        if (fit_w != rwidth || fit_h != rheight) {

                fit_pixels = malloc((size_t)3 * fit_w * fit_h);

                if (!fit_pixels) {
                        image_free(read_data);
                        free(composed);
                        return -1;
                }

                image_resize_uint8_srgb(read_data,
                                        rwidth,
                                        rheight,
                                        fit_pixels,
                                        fit_w,
                                        fit_h,
                                        3);

                image_free(read_data);
                did_resize = true;
        }

        int off_x = ((int)corr_w - fit_w) / 2;
        int off_y = ((int)height - fit_h) / 2;

        for (int y = 0; y < fit_h; y++) {
                for (int x = 0; x < fit_w; x++) {

                        size_t dst =
                            (size_t)((off_y + y) * (int)corr_w +
                                     (off_x + x)) *
                            3;

                        size_t src =
                            ((size_t)y * (size_t)fit_w +
                             (size_t)x) *
                            3;

                        composed[dst + 0] = fit_pixels[src + 0];
                        composed[dst + 1] = fit_pixels[src + 1];
                        composed[dst + 2] = fit_pixels[src + 2];
                }
        }

        if (did_resize)
                free(fit_pixels);
        else
                image_free(fit_pixels);

        // Buffer coordinates are usually 0-based.
        if (centered && term_size->width_cells > 0)
                col = (term_size->width_cells - (int)corr_w) / 2;

        for (unsigned int d = 0; d < corr_w * height; d++) {

                int draw_row = row + (int)(d / corr_w);
                int draw_col = col + (int)(d % corr_w);

                if (draw_row < 0 || draw_row >= buf->rows ||
                    draw_col < 0 || draw_col >= buf->cols)
                        continue;

                size_t idx = (size_t)d * 3;

                PixelData px = {
                    .r = composed[idx + 0],
                    .g = composed[idx + 1],
                    .b = composed[idx + 2],
                    .a = 255,
                };

                CellStyle style = {
                    .fg = px,
                    .bg = COLOR_DEFAULT,
                    .attrs = ATTR_BOLD,
                };

                draw_buffer_set_cell(buf,
                                     draw_row,
                                     draw_col,
                                     (uint32_t)calc_ascii_char(&px),
                                     style);
        }

        free(composed);

        return 0;
}

ComponentMsg component_side_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;
        const SongData *songdata = model->songdata;
        const UISettings *ui = &state->settings;
        const TermSize *term_size = &model->term_size;

        if (!ui->coverEnabled || ui->hideSideCover || !songdata || !songdata->cover)
                return (ComponentMsg){0};

        int indent = region.width * 4 / 5;
        int cover_indent = indent / 8;

        int target_height = indent - cover_indent;
        int max_height = region.height - 2;
        if (target_height > max_height)
                target_height = max_height;

        gint cell_width = 8;
        gint cell_height = 16;

        if (term_size->width_cells > 0 && term_size->height_cells > 0 &&
            term_size->width_pixels > 0 && term_size->height_pixels > 0) {
                cell_width = term_size->width_pixels / term_size->width_cells;
                cell_height = term_size->height_pixels / term_size->height_cells;
        }

        float aspect = (float)cell_height / (float)cell_width;
        int corrected_width = (int)(target_height * aspect);

        while (corrected_width > indent - cover_indent && target_height > 0) {
                target_height--;
                corrected_width = (int)(target_height * aspect);
        }

        if (target_height <= MIN_COVER_SIZE)
                return (ComponentMsg){0};

        // Use region as base — row is centered within region, col is offset from region.col
        int row = region.row + lroundf((float)region.height / 2.0f - (float)target_height / 2.0f);
        int col = region.col + cover_indent + 3;

        if (corrected_width <= 0 || target_height <= 0)
                return (ComponentMsg){0};

        if (ui->coverAnsi) {
                draw_cover_ascii(term_size, songdata->cover_art_path,
                                 row, col, target_height,
                                 false, buf, dirty);
        } else {

                draw_square_bitmap_to_buf(buf,
                                          row, col,
                                          songdata->cover,
                                          songdata->coverWidth,
                                          songdata->coverHeight,
                                          corrected_width,
                                          target_height,
                                          term_size,
                                          false,
                                          model->current_hash,
                                          ui->coverStyle, false, true);
        }

        return (ComponentMsg){0};
}

ComponentMsg component_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const AppState *state = &model->state;
        SongData *songdata = model->songdata;

        int row = region.row == 0 ? 1 : region.row; // Always print at least one row down

        if (!state->settings.coverEnabled || !songdata)
                return (ComponentMsg){0};

        if (state->settings.coverAnsi && songdata->cover) {
                draw_cover_ascii(&model->term_size, songdata->cover_art_path,
                                 row, region.col,
                                 region.height, false, buf, dirty);
        }

        bool draw_cover_marker = model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        bool draw_occupied_markers = !model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        draw_square_bitmap_to_buf(buf,
                                  row, region.col,
                                  songdata->cover,
                                  songdata->coverWidth,
                                  songdata->coverHeight,
                                  region.width,
                                  region.height,
                                  &model->term_size,
                                  false,
                                  model->current_hash,
                                  state->settings.coverStyle, draw_cover_marker, draw_occupied_markers);
        ;

        return (ComponentMsg){0};
}

ComponentMsg component_cover_centered(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const AppState *state = &model->state;
        SongData *songdata = model->songdata;

        if (!state->settings.coverEnabled || !songdata)
                return (ComponentMsg){0};

        if (state->settings.coverAnsi && songdata->cover) {
                draw_cover_ascii(&model->term_size, songdata->cover_art_path,
                                 region.row, region.col,
                                 region.height, true, buf, dirty);
        }

        bool draw_cover_marker = model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        bool draw_occupied_markers = !model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        draw_square_bitmap_to_buf(buf,
                                  region.row, region.col,
                                  songdata->cover,
                                  songdata->coverWidth,
                                  songdata->coverHeight,
                                  region.width,
                                  region.height,
                                  &model->term_size,
                                  true,
                                  model->current_hash,
                                  state->settings.coverStyle, draw_cover_marker, draw_occupied_markers);

        return (ComponentMsg){0};
}

ComponentMsg component_landscape_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;
        SongData *songdata = model->songdata;
        const UISettings *ui = &state->settings;
        const TermSize *term_size = &model->term_size;

        if (!ui->coverEnabled || !songdata)
                return (ComponentMsg){0};

        int cover_indent = 1;

        int target_height = region.height;

        gint cell_width = 8;
        gint cell_height = 16;

        if (term_size->width_cells > 0 && term_size->height_cells > 0 &&
            term_size->width_pixels > 0 && term_size->height_pixels > 0) {
                cell_width = term_size->width_pixels / term_size->width_cells;
                cell_height = term_size->height_pixels / term_size->height_cells;
        }

        float aspect = (float)cell_height / (float)cell_width;
        int corrected_width = (int)(target_height * aspect);

        while (corrected_width > region.width - cover_indent && target_height > 0) {
                target_height--;
                corrected_width = (int)(target_height * aspect);
        }

        if (target_height <= MIN_COVER_SIZE)
                return (ComponentMsg){0};

        // Use region as base, row is centered within region, col is offset from region.col
        int row = region.row + lroundf((float)region.height / 2.0f - (float)target_height / 2.0f);
        int col = region.col + cover_indent;

        // Clear skipped lines
        if (row > 0) {
                CellStyle style = cell_style_plain();

                for (int i = 0; i < row; i++) {
                        draw_buffer_set_string_truncated(buf, i, region.col,
                                                         "", region.width, style);
                }
        }

        if (corrected_width <= 0 || target_height <= 0)
                return (ComponentMsg){0};

        if (ui->coverAnsi) {
                draw_cover_ascii(&model->term_size, songdata->cover_art_path,
                                 row, col, target_height,
                                 false, buf, dirty);
        }

        bool draw_cover_marker = model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        bool draw_occupied_markers = !model->state.settings.coverAnsi || model->state.ui.chroma_started || model->state.ui.chroma_start_requested;

        draw_square_bitmap_to_buf(buf,
                                  row, col,
                                  songdata->cover,
                                  songdata->coverWidth,
                                  songdata->coverHeight,
                                  corrected_width,
                                  target_height,
                                  term_size,
                                  false,
                                  model->current_hash,
                                  state->settings.coverStyle, draw_cover_marker, draw_occupied_markers);

        return (ComponentMsg){0};
}

ComponentMsg component_now_playing(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        bool show_artist = false;
        now_playing(show_artist, model, region, buf, dirty);

        return (ComponentMsg){0};
}

ComponentMsg component_now_playing_and_artist(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        bool show_artist = true;
        now_playing(show_artist, model, region, buf, dirty);

        return (ComponentMsg){0};
}

ComponentMsg component_logo_art(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        size_t logo_lines = sizeof(LOGO) / sizeof(LOGO[0]);
        const UISettings *ui = &model->state.settings;

        for (size_t i = 0; i < logo_lines; i++) {
                int row = region.row + (int)i;
                if (row >= region.row + region.height)
                        break;

                PixelData color = {ui->color.r, ui->color.g, ui->color.b, 255};
                unsigned char def = ui->default_color;

                bool is_default_color = color.r == def && color.g == def && color.b == def;

                if (!(is_default_color)) {
                        color = get_gradient_color(ui->color, logo_lines - i, logo_lines, 2, 0.8f);
                }

                CellStyle style = cell_style_from_theme(ui->theme.logo);

                draw_buffer_set_string(buf, row, region.col, LOGO[i], style);
        }

        return (ComponentMsg){0};
}

ComponentMsg component_logo(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const UISettings *ui = &model->state.settings;

        int logo_width = ui->hideLogo ? 1 : LOGO_WIDTH;
        int now_playing_col = region.col + logo_width + 1;
        int now_playing_width = region.width - logo_width - 1;
        int now_playing_row = (ui->hideLogo) ? 1 : LOGO_HEIGHT - 1;
        int now_playing_height = (ui->hideLogo) ? 2 : 1;

        if (!ui->hideLogo) {
                component_logo_art(model, region, buf, dirty);
        }

        // Now playing header
        k_Rect header_rect = {
            .row = region.row + now_playing_row,
            .col = now_playing_col,
            .width = now_playing_width,
            .height = now_playing_height,
        };
        component_now_playing(model, header_rect, buf, dirty);

        return (ComponentMsg){0};
}

ComponentMsg component_playback_status(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        const UIState *uis = &model->state.ui;

        if (model->preferred_width < 0 || model->preferred_height < 0)
                return (ComponentMsg){0};

        if (ui->hideFooter)
                return (ComponentMsg){0};

        CellStyle style = cell_style_from_theme(ui->theme.playbackstatus);

        if (!style.isAnsi && style.fg.a == 0)
                style = cell_style_from_theme(ui->theme.footer);

        // Icons
        char icons[100] = "";
        size_t icons_len = 0;

#ifndef __ANDROID__
        if (region.width >= ABSOLUTE_MIN_WIDTH) {
#endif
                const char *state_icon;
                if (model->is_paused) {
#if defined(__ANDROID__) || defined(__APPLE__)
                        state_icon = "။ ";
#else
                state_icon = "⏸ ";
#endif
                } else if (model->is_stopped) {
                        state_icon = "■ ";
                } else {
                        state_icon = "";
                }
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "%s", state_icon);
                icons_len = strnlen(icons, sizeof(icons));
#ifndef __ANDROID__
        }
#endif

        if (model->state.settings.repeatState == SOUND_STATE_REPEAT) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "↻ ");
                icons_len = strnlen(icons, sizeof(icons));
        } else if (model->state.settings.repeatState == SOUND_STATE_REPEAT_LIST) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "↻L ");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (model->state.settings.shuffle_enabled) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "⇄ ");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (uis->isFastForwarding) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "⇉ ");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (uis->isRewinding) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "⇇ ");
                icons_len = strnlen(icons, sizeof(icons));
        }

        char line[256];
        snprintf(line, sizeof(line), "%s", icons);
        draw_buffer_set_string_truncated(buf, region.row, region.col, line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_footer(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        const UIState *uis = &model->state.ui;

        if (model->preferred_width < 0 || model->preferred_height < 0)
                return (ComponentMsg){0};

        if (ui->hideFooter)
                return (ComponentMsg){0};

        // Footer Color
        PixelData f_color = model->state.settings.footer_color;

        if (ui->themeIsSet && ui->theme.footer.type == COLOR_TYPE_RGB) {
                f_color.r = ui->theme.footer.rgb.r;
                f_color.g = ui->theme.footer.rgb.g;
                f_color.b = ui->theme.footer.rgb.b;
        }

        CellStyle style = cell_style_from_theme(ui->theme.footer);

        // Main Text
        char text[100] = "";
        get_footer_text(text, sizeof(text));

        // Icons
        char icons[100] = "";
        size_t icons_len = 0;

#ifndef __ANDROID__
        if (region.width >= ABSOLUTE_MIN_WIDTH) {
#endif
                const char *state_icon;
                if (model->is_paused) {
#if defined(__ANDROID__) || defined(__APPLE__)
                        state_icon = " ။";
#else
                state_icon = " ⏸";
#endif
                } else if (model->is_stopped) {
                        state_icon = " ■";
                } else {
                        state_icon = " ▶";
                }
                snprintf(icons + icons_len, sizeof(icons) - icons_len, "%s", state_icon);
                icons_len = strnlen(icons, sizeof(icons));
#ifndef __ANDROID__
        }
#endif

        if (model->state.settings.repeatState == 1) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, " ↻");
                icons_len = strnlen(icons, sizeof(icons));
        } else if (model->state.settings.repeatState == 2) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, " ↻L");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (model->state.settings.shuffle_enabled) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, " ⇄");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (uis->isFastForwarding) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, " ⇉");
                icons_len = strnlen(icons, sizeof(icons));
        }

        if (uis->isRewinding) {
                snprintf(icons + icons_len, sizeof(icons) - icons_len, " ⇇");
                icons_len = strnlen(icons, sizeof(icons));
        }

        // Narrow terminal
        if (region.width < ABSOLUTE_MIN_WIDTH) {
#ifndef __ANDROID__
                if (region.width > (int)icons_len + model->indent)
                        draw_buffer_set_string(buf, region.row, region.col, icons, style);
#else
                char android_line[256];
                snprintf(android_line, sizeof(android_line), "%.*s%s", region.width * 2, text, icons);
                draw_buffer_set_string_truncated(buf, region.row, region.col,
                                                 android_line, region.width, style);
#endif
                return (ComponentMsg){0};
        }

        if (model->glimmer.active) {
                int text_length = strnlen(text, sizeof(text));
                render_glimmer_frame(model, buf, text, text_length, icons, f_color, region.row,
                                     region.col, &model->glimmer, region.width, style);
                return (ComponentMsg){0};
        }

        char full_line[256];
        snprintf(full_line, sizeof(full_line), "%s%s", text, icons);
        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                         full_line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_error_row(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        CellStyle style = cell_style_plain();

        if (!has_printed_error_message() && has_error_message()) {
                style = cell_style_from_theme(ui->theme.status_error);

                char msg[256];
                snprintf(msg, sizeof(msg), "%s", get_error_message());
                draw_buffer_set_string_truncated(buf, region.row, region.col,
                                                 msg, region.width, style);
                return (ComponentMsg){0};
        }

        // Write blanks to clear any previous message
        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                         "", region.width, style);

        return (ComponentMsg){0};
}

void move_start_node_into_position(Model *model, int found_at, Node **start_node)
{
        int start_iter = model->state.ui.start_iter;

        // Go up to adjust the start_node
        for (int i = found_at; i > start_iter; i--) {
                if (i > 0 && (*start_node)->prev != NULL)
                        *start_node = (*start_node)->prev;
        }

        // Go down to adjust the start_node
        for (int i = (found_at == -1) ? 0 : found_at; i < start_iter; i++) {
                if ((*start_node)->next != NULL)
                        *start_node = (*start_node)->next;
        }
}

void component_playlist_helper_update_view_state(Model *model)
{
        model->state.ui.playlist_node = determine_start_node(model->playlist->head, &model->state.ui.current_at_row, model->unshuffled_playlist->count);
        model->state.ui.start_iter = determine_playlist_start(&model->state.ui.start_iter,
                                                              model->state.ui.current_at_row, model->state.ui.max_playlist_rows, &model->state.ui.chosen_row,
                                                              model->state.ui.resetPlaylistDisplay,
                                                              sound_system_is_end_of_list_reached(sound_sys));

        if (!model->state.ui.playlist_node)
                return;

        move_start_node_into_position(model, model->state.ui.current_at_row, &model->state.ui.playlist_node);

        Node *node = model->state.ui.playlist_node;
        for (int i = model->state.ui.start_iter; i < model->state.ui.start_iter + model->state.ui.max_playlist_rows; i++) {

                if (!node)
                        break;

                if (i == model->state.ui.chosen_row) {

                        model->state.ui.previous_chosen_song = model->state.ui.chosen_row;
                        model->state.ui.chosen_node_id = node->id;
                        break;
                }

                node = node->next;
        }
}

ComponentMsg component_playlist_rows(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        const PlayList *list = model->playlist;
        const UIState *uis = &model->state.ui;

        if (!list)
                return (ComponentMsg){0};

        unsigned char def = ui->default_color;

        int max_rows = model->state.ui.max_playlist_rows;
        int row_offset = region.row;

        int start_iter = model->state.ui.start_iter;

        int printed = 0;

        Node *node = model->state.ui.playlist_node;
        CellStyle header = cell_style_from_theme(ui->theme.header);
        CellStyle playing_style = cell_style_from_theme(ui->theme.playlist_playing);

        draw_buffer_set_string(buf, row_offset, region.col, _("─ PLAYLIST ─"), header);

        row_offset++;

        for (int i = start_iter; i < start_iter + max_rows; i++) {

                CellStyle rownum_style = cell_style_from_theme(ui->theme.playlist_rownum);
                CellStyle title_style = cell_style_from_theme(ui->theme.playlist_title);

                char buffer[NAME_MAX + 1];
                char filename[NAME_MAX + 1];
                buffer[0] = filename[0] = '\0';

                int draw_row = row_offset + printed;
                int col = region.col;

                if (node == NULL) {
                        draw_buffer_set_string_truncated(buf, draw_row, col, filename, region.width, cell_style_plain());

                        printed++;

                        continue;
                }

                prepare_playlist_string(node, buffer, NAME_MAX);

                if (buffer[0] == '\0') {
                        node = node->next;
                        i++;
                        continue;
                }

                // Row number color
                PixelData row_num_color = {def, def, def, 255};
                PixelData title_color = {def, def, def, 255};

                if (rownum_style.isAnsi == false) {
                        row_num_color = rownum_style.fg;
                }

                if (title_style.isAnsi == false) {
                        title_color = title_style.fg;
                }

                bool rownum_is_default = row_num_color.r == def &&
                                         row_num_color.g == def &&
                                         row_num_color.b == def;
                bool title_is_default = title_color.r == def &&
                                        title_color.g == def &&
                                        title_color.b == def;

                if (!rownum_is_default)
                        row_num_color = get_gradient_color(row_num_color, printed, max_rows, max_rows / 2, 0.7f);
                if (!title_is_default)
                        title_color = get_gradient_color(title_color, printed, max_rows, max_rows / 2, 0.7f);

                rownum_style.fg = row_num_color;
                title_style.fg = title_color;

                // Row number
                char rownum[18];
                int num = i + 1;
                int spaces = 0;

                if (num < 10)
                        spaces = 2;
                else if (i < 100)
                        spaces = 1;
                else
                        spaces = 0;

                snprintf(rownum, sizeof(rownum), "%d.", i + 1);

                int rownum_len = utf8_display_width(rownum);

                draw_buffer_set_string_truncated(buf, draw_row, col, rownum, rownum_len, rownum_style);

                col += rownum_len;

                for (int i = 0; i < spaces; i++) {
                        draw_buffer_set_string_truncated(buf, draw_row, col, " ", 1, cell_style_plain());
                }

                col++;

                // Title
                bool is_chosen = (i == uis->chosen_row);
                bool is_playing = false;

                Node *current = get_current_song();
                if (current != NULL)
                        is_playing = (current->id == node->id);

                int max_name_width = region.width - (col - region.col);

                if (is_chosen) {
                        process_name_scroll(model, buffer, filename, max_name_width, true, true);
                } else {
                        process_name(buffer, filename, max_name_width, true, true);
                }

                if (is_playing) {
                        title_style = playing_style;
                }

                if (is_chosen)
                        title_style.attrs |= ATTR_REVERSE;
                if (is_playing && is_chosen)
                        title_style.attrs |= ATTR_REVERSE;
                if (is_playing && !is_chosen)
                        title_style.attrs |= ATTR_UNDERLINE;

                draw_buffer_set_string_truncated(buf, draw_row, col, filename, region.width, title_style);

                node = node->next;
                printed++;
        }

        return (ComponentMsg){0};
}

ComponentMsg component_playlist_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        CellStyle style = cell_style_from_theme(ui->theme.help);

        int row = region.row;
        int col = region.col;

        if (model->term_w > MIN_HEADER_WIDTH && !ui->hideHelp) {
                char line1[256];
                char line2[256];

                snprintf(line1, sizeof(line1), _(" Select:↑/↓ or k/j. Accept:%s. Clear:%s."),
                         get_binding_string(MSG_ENQUEUE, true),
                         get_binding_string(MSG_CLEARPLAYLIST, true));

#ifndef __APPLE__
                snprintf(line2, sizeof(line2), _(" Scroll:PgUp/PgDn. Remove:%s. Move songs:%s/%s."),
                         get_binding_string(MSG_REMOVE, true),
                         get_binding_string(MSG_MOVESONGUP, true),
                         get_binding_string(MSG_MOVESONGDOWN, true));
#else
                snprintf(line2, sizeof(line2), _(" Scroll:Fn+↑/↓. Remove:%s. Move songs:%s/%s."),
                         get_binding_string(MSG_REMOVE, true),
                         get_binding_string(MSG_MOVESONGUP, true),
                         get_binding_string(MSG_MOVESONGDOWN, true));
#endif

                draw_buffer_set_string(buf, row, col, line1, style);
                draw_buffer_set_string(buf, row + 1, col, line2, style);
                draw_buffer_set_string(buf, row + 2, col, "", style); // blank line below

                row += 3;
        }

        return (ComponentMsg){0};
}

ComponentMsg component_library_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        CellStyle style = cell_style_from_theme(ui->theme.help);

        int row = region.row;
        int col = region.col;

        if (model->term_w > MIN_HEADER_WIDTH && !ui->hideHelp) {
                char line1[256];
                char line2[256];

                snprintf(line1, sizeof(line1), _(" Select:↑/↓ or k/j. Enqueue/Dequeue:%s. Play:%s."),
                         get_binding_string(MSG_ENQUEUE, true),
                         get_binding_string(MSG_ENQUEUEANDPLAY, true));

#ifndef __APPLE__
                snprintf(line2, sizeof(line2), _(" Scroll:Fn+↑/↓. Update:%s. Sort:%s."),
#else
                snprintf(line2, sizeof(line2), _(" Scroll:PgUp/PgDn. Update:%s. Sort:%s."),
#endif
                         get_binding_string(MSG_UPDATELIBRARY, true),
                         get_binding_string(MSG_SORTLIBRARY, true));

                draw_buffer_set_string(buf, row, col, line1, style);
                draw_buffer_set_string(buf, row + 1, col, line2, style);
        }

        return (ComponentMsg){0};
}

ComponentMsg component_search_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        CellStyle style = cell_style_from_theme(ui->theme.help);

        int row = region.row;
        int col = region.col;

        if (model->term_w > MIN_HEADER_WIDTH && !ui->hideHelp) {
                char line1[256];

                snprintf(line1, sizeof(line1), _("Select:↑/↓. Enqueue:%s. Play:%s."),
                         get_binding_string(MSG_ENQUEUE, true),
                         get_binding_string(MSG_ENQUEUEANDPLAY, true));

                draw_buffer_set_string(buf, row, col, line1, style);
        }

        return (ComponentMsg){0};
}

ComponentMsg component_metadata(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        SongData *songdata = model->songdata;

        if (!songdata || !songdata->metadata)
                return (ComponentMsg){0};

        TagSettings *metadata = songdata->metadata;
        int max_width = region.width; // -1 for the leading space

        if (dirty & DIRTY_SONG) {

                // Artist
                if (strnlen(metadata->artist, METADATA_MAX_LENGTH) > 0 && region.height >= 2) {
                        CellStyle style = cell_style_from_theme(ui->theme.trackview_artist);
                        char line[METADATA_MAX_LENGTH + 2];
                        snprintf(line, sizeof(line), "%s", metadata->artist);

                        if (strcmp(metadata->artist, "Ice Cube") == 0)
                        {
                                draw_link_to_buffer(buf, region.row + 1, region.col, max_width,
                                        "https://icecube.com", metadata->artist, style);
                        }
                        else if (strcmp(metadata->artist, "milkypossum") == 0)
                        {
                                draw_link_to_buffer(buf, region.row + 1, region.col, max_width,
                                        "https://milkypossumofficial.com", metadata->artist, style);
                        }
                        else {
                                draw_buffer_set_string_truncated(buf, region.row + 1, region.col,
                                                         line, max_width, style);
                        }
                }

                // Album
                if (strnlen(metadata->album, METADATA_MAX_LENGTH) > 0 && region.height >= 3) {
                        CellStyle style = cell_style_from_theme(ui->theme.trackview_album);
                        char line[METADATA_MAX_LENGTH + 2];
                        snprintf(line, sizeof(line), "%s", metadata->album);
                        draw_buffer_set_string_truncated(buf, region.row + 2, region.col,
                                                         line, max_width, style);
                }

                // Year
                if (strnlen(metadata->date, METADATA_MAX_LENGTH) > 0 && region.height >= 4) {
                        CellStyle style = cell_style_from_theme(ui->theme.trackview_year);
                        char line[METADATA_MAX_LENGTH + 2];
                        int year = get_year(metadata->date);
                        if (year == -1)
                                snprintf(line, sizeof(line), "%s", metadata->date);
                        else
                                snprintf(line, sizeof(line), "%d", year);

                        draw_buffer_set_string_truncated(buf, region.row + 3, region.col,
                                                         line, max_width, style);
                }
        }

        // Title
        if (strnlen(metadata->title, METADATA_MAX_LENGTH) > 0 && region.height >= 1) {

                CellStyle style = cell_style_from_theme(ui->theme.trackview_title);

                char pretty_title[PATH_MAX + 1];
                pretty_title[0] = '\0';
                process_name(metadata->title, pretty_title, max_width, false, false);

                int frame = model->title_delay.frame;
                int width = MIN(frame, max_width);

                // Title delay animation
                if (model->title_delay.active && frame <= width) {
                        char display[PATH_MAX + 4];
                        char current_text[PATH_MAX + 1];
                        const char *p = pretty_title;
                        int byte_pos = 0;

                        for (int i = 0; i < width && *p; i++) {
                                int bytes = 0;
                                utf8_next(p, &bytes);

                                p += bytes;
                                byte_pos += bytes;
                        }

                        memcpy(current_text, pretty_title, byte_pos);
                        current_text[byte_pos] = '\0';

                        snprintf(display, sizeof(display), "%s█", current_text);

                        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                                         display, max_width, style);
                } else {
                        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                                         pretty_title, max_width, style);
                }
        }

        return (ComponentMsg){0};
}

ComponentMsg component_progress_bar(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        const AppSettings *settings = &model->settings;

        // leading space
        CellStyle plain = cell_style_plain();
        draw_buffer_set_string(buf, region.row, region.col, " ", plain);
        CellStyle empty = cell_style_from_theme(ui->theme.progress_empty);

        int draw_col = region.col;

        int elapsed_bars = calc_elapsed_bars(model->elapsed_seconds,
                                             model->songdata ? model->songdata->duration : 0, region.width);

        CellStyle filled_style = cell_style_from_theme(ui->theme.progress_filled);
        CellStyle current_style = cell_style_from_theme(ui->theme.progress_elapsed);

        if (ui->colorMode == COLOR_MODE_ALBUM_ONE) {

                // If it's an album color
                if (!empty.isAnsi && ui->theme.progress_empty.ansiIndex >= 100) {
                        PixelData emptyC = {0};
                        emptyC.r = empty.fg.r;
                        emptyC.g = empty.fg.g;
                        emptyC.b = empty.fg.b;
                        emptyC.a = empty.fg.a;

                        empty.fg = increase_luminosity(emptyC, 50);

                        if (empty.fg.r >= model->state.settings.default_color &&
                            empty.fg.g >= model->state.settings.default_color &&
                            empty.fg.b >= model->state.settings.default_color) {
                                empty.fg = decrease_luminosity_pct(emptyC, 0.7);
                        }
                }
        }

        if (empty.fg.r == filled_style.fg.r &&
            empty.fg.g == filled_style.fg.g &&
            empty.fg.b == filled_style.fg.b) {
                empty.fg = decrease_luminosity_pct(empty.fg, 0.7);
        }

        if ((empty.isAnsi &&
                (!model->songdata || !model->songdata->cover)) ||
                        (model->state.settings.colorMode == COLOR_MODE_DEFAULT)) {

                // If it's missing a cover
                if (empty.fgAnsi == -1) {
                        empty.fg = decrease_luminosity_pct(ui->color, 0.7);
                        empty.isAnsi = false;
                }
        }

        CellStyle style = empty;

        for (int i = 0; i < region.width; i++) {
                const char *ch;

                if (i > elapsed_bars) {
                        // Empty / Approaching
                        style = empty;

                        ch = (i % 2 == 0) ? settings->progressBarApproachingEvenChar
                                          : settings->progressBarApproachingOddChar;

                } else if (i < elapsed_bars) {
                        // Filled / Elapsed
                        style = filled_style;
                        ch = (i % 2 == 0) ? settings->progressBarElapsedEvenChar
                                          : settings->progressBarElapsedOddChar;

                } else {
                        // Current position
                        style = current_style;
                        ch = (i % 2 == 0) ? settings->progressBarCurrentEvenChar
                                          : settings->progressBarCurrentOddChar;
                }

                draw_buffer_set_string(buf, region.row, draw_col, ch, style);
                draw_col++;
        }

        ComponentMsg result = (ComponentMsg){0};

        result.has_msg = true;
        result.msg = (struct Msg){
            .type = MSG_PROGRESS_BARS_SET,
            .region = region};

        return result;
}

ComponentMsg component_time_simple(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;

        int progress_width = 39;
        if (region.width < progress_width)
                return (ComponentMsg){0};

        double elapsed_seconds = model->elapsed_seconds;
        double total_seconds = get_current_song_duration();

        char line[256];
        int pos = 0;

        // Time
        if (total_seconds >= 3600) {
                int eh = (int)(elapsed_seconds / 3600);
                int em = (int)(((int)elapsed_seconds / 60) % 60);
                int es = (int)elapsed_seconds % 60;
                int th = (int)(total_seconds / 3600);
                int tm = (int)(((int)total_seconds / 60) % 60);
                int ts = (int)total_seconds % 60;

                pos += snprintf(line + pos, sizeof(line) - pos,
                                "%02d:%02d:%02d / %02d:%02d:%02d",
                                eh, em, es, th, tm, ts);
        } else {
                int em = (int)(elapsed_seconds / 60);
                int es = (int)elapsed_seconds % 60;
                int tm = (int)(total_seconds / 60);
                int ts = (int)total_seconds % 60;

                pos += snprintf(line + pos, sizeof(line) - pos, "%d:%02d / %d:%02d", em, es, tm, ts);
        }

        CellStyle style = cell_style_from_theme(ui->theme.trackview_time);
        draw_buffer_set_string_truncated(buf, region.row, region.col, line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_volume(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;

        int progress_width = 39;
        if (region.width < progress_width)
                return (ComponentMsg){0};

        char line[256];

        int vol = model->volume;

        snprintf(line, sizeof(line), "Vol:%d%%", vol);

        CellStyle style = cell_style_from_theme(ui->theme.trackview_time);
        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                         line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_time_simple_and_vol(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;

        int progress_width = 39;
        if (region.width < progress_width)
                return (ComponentMsg){0};

        double elapsed_seconds = model->elapsed_seconds;
        double total_seconds = get_current_song_duration();
        int vol = model->volume;

        char line[256];
        int pos = 0;

        // Time
        if (total_seconds >= 3600) {
                int eh = (int)(elapsed_seconds / 3600);
                int em = (int)(((int)elapsed_seconds / 60) % 60);
                int es = (int)elapsed_seconds % 60;
                int th = (int)(total_seconds / 3600);
                int tm = (int)(((int)total_seconds / 60) % 60);
                int ts = (int)total_seconds % 60;

                pos += snprintf(line + pos, sizeof(line) - pos,
                                "%02d:%02d:%02d / %02d:%02d:%02d Vol:%d%%",
                                eh, em, es, th, tm, ts, vol);
        } else {
                int em = (int)(elapsed_seconds / 60);
                int es = (int)elapsed_seconds % 60;
                int tm = (int)(total_seconds / 60);
                int ts = (int)total_seconds % 60;

                pos += snprintf(line + pos, sizeof(line) - pos, "%d:%02d / %d:%02d Vol:%d%%", em, es, tm, ts, vol);
        }

        CellStyle style = cell_style_from_theme(ui->theme.trackview_time);
        draw_buffer_set_string_truncated(buf, region.row, region.col, line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_time(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        SongData *songdata = model->songdata;

        int progress_width = 39;
        if (region.width < progress_width)
                return (ComponentMsg){0};

        double elapsed_seconds = model->elapsed_seconds;
        double total_seconds = get_current_song_duration();
        int sample_rate = get_current_sample_rate();
        int avg_bit_rate = songdata ? songdata->avg_bit_rate : 0;
        int vol = model->volume;

        char line[256];
        int pos = 0;

        // Time
        if (total_seconds >= 3600) {
                int eh = (int)(elapsed_seconds / 3600);
                int em = (int)(((int)elapsed_seconds / 60) % 60);
                int es = (int)elapsed_seconds % 60;
                int th = (int)(total_seconds / 3600);
                int tm = (int)(((int)total_seconds / 60) % 60);
                int ts = (int)total_seconds % 60;
                int pct = (int)((elapsed_seconds / total_seconds) * 100);

                pos += snprintf(line + pos, sizeof(line) - pos,
                                "%02d:%02d:%02d / %02d:%02d:%02d (%d%%) Vol:%d%%",
                                eh, em, es, th, tm, ts, pct, vol);
        } else {
                int em = (int)(elapsed_seconds / 60);
                int es = (int)elapsed_seconds % 60;
                int tm = (int)(total_seconds / 60);
                int ts = (int)total_seconds % 60;
                int pct = total_seconds > 0
                              ? (int)((elapsed_seconds / total_seconds) * 100)
                              : 0;

                pos += snprintf(line + pos, sizeof(line) - pos,
                                "%d:%02d / %d:%02d (%d%%) Vol:%d%%",
                                em, es, tm, ts, pct, vol);
        }

        // Sample Rate
        if (region.width > progress_width + 10) {
                double rate = sample_rate / 1000.0;
                if (rate == (int)rate)
                        pos += snprintf(line + pos, sizeof(line) - pos, " %dkHz", (int)rate);
                else
                        pos += snprintf(line + pos, sizeof(line) - pos, " %.1fkHz", rate);
        }

        // Bit Rate
        if (region.width > progress_width + 19) {
                if (avg_bit_rate > 0)
                        pos += snprintf(line + pos, sizeof(line) - pos, " %dkb/s", avg_bit_rate);
        }

        CellStyle style = cell_style_from_theme(ui->theme.trackview_time);
        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                         line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_visualizer(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const AppState *state = &model->state;
        const UISettings *ui = &state->settings;

        if (ui->visualizer_mode == VIZ_OFF)
                return (ComponentMsg){0};

        int height = region.height;

        // clamp height to region
        if (height > region.height)
                height = region.height;

        if (height < 2 || model->is_paused)
                return (ComponentMsg){0};

        draw_spectrum_visualizer_to_buf(model, buf, sound_sys, region.row, region.col,
                                        height, region.width);

        return (ComponentMsg){0};
}

ComponentMsg component_timestamped_lyrics(const Model *model, k_Rect region, DrawBuffer *buf,
                                          DirtyFlags dirty)
{
        (void)dirty;

        SongData *songdata = model->songdata;
        const UISettings *ui = &model->state.settings;

        if (!songdata || !songdata->lyrics || songdata->lyrics->isTimed != 1)
                return (ComponentMsg){0};

        CellStyle style = cell_style_from_theme(ui->theme.trackview_lyrics);
        draw_buffer_set_string_truncated(buf, region.row, region.col,
                                         model->state.ui.lyrics_line, region.width, style);

        return (ComponentMsg){0};
}

ComponentMsg component_track_landscape_normal(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;
        const UISettings *ui = &state->settings;
        SongData *songdata = model->songdata;

        if (!songdata)
                return (ComponentMsg){0};

        int height = region.height;
        int metadata_height = 4;
        int time_height = 1;
        int lyrics_height = 1;
        int visualizer_width = region.width;
        int visualizer_height = ui->visualizer_height;
        int progress_bar_height = 1;
        int col = 0;

        visualizer_height = region.height - metadata_height - time_height - lyrics_height;

        if (visualizer_height > ui->visualizer_height) {
                visualizer_height = ui->visualizer_height;
        }

        if (visualizer_height < 0)
                visualizer_height = 0;

        int meta_row = region.height - metadata_height - time_height - lyrics_height - visualizer_height;

        if (meta_row < 1)
                meta_row = 1;

        if (meta_row > 3)
                meta_row -= 2; // include footer and error row

        // Metadata
        if (height > meta_row) {
                k_Rect meta_rect = {
                    .row = region.row + meta_row,
                    .col = region.col + col,
                    .width = visualizer_width - 1,
                    .height = metadata_height,
                };
                if (dirty & (DIRTY_SONG | DIRTY_TITLE))
                        component_metadata(model, meta_rect, buf, dirty);
        }

        // Time
        if (height > meta_row + metadata_height + time_height) {
                k_Rect time_rect = {
                    .row = region.row + meta_row + metadata_height,
                    .col = region.col + col,
                    .width = visualizer_width,
                    .height = 1,
                };
                if (dirty & DIRTY_VISUALIZER) {
                        if (model->state.settings.simpleTimeStatus)
                                component_time_simple_and_vol(model, time_rect, buf, dirty);
                        else
                                component_time(model, time_rect, buf, dirty);
                }
        } else {
                time_height = 0;
        }

        // Timestamped lyrics
        if (height > meta_row + metadata_height + time_height + lyrics_height) {
                k_Rect lyrics_rect = {
                    .row = region.row + meta_row + metadata_height + time_height,
                    .col = region.col + col,
                    .width = visualizer_width - 1,
                    .height = 1,
                };
                if (dirty & DIRTY_VISUALIZER)
                        component_timestamped_lyrics(model, lyrics_rect, buf, dirty);
        } else {
                lyrics_height = 0;
        }

        ComponentMsg result = (ComponentMsg){0};
        // Visualizer
        if (height >= meta_row + metadata_height + time_height + lyrics_height + visualizer_height - 1) {

                k_Rect viz_rect = {
                    .row = region.row + meta_row + metadata_height + lyrics_height + time_height,
                    .col = region.col + col,
                    .width = visualizer_width,
                    .height = visualizer_height - 1,
                };
                if (dirty & DIRTY_VISUALIZER)
                        component_visualizer(model, viz_rect, buf, dirty);
        } else {
                visualizer_height = 0;
        }

        // Progress bar
        if (height >= meta_row + metadata_height + time_height + lyrics_height + visualizer_height - 1) {
                // Progress bar on last row of visualizer
                k_Rect progress_rect = {
                    .row = region.row + meta_row + metadata_height + lyrics_height + time_height + visualizer_height - 1,
                    .col = region.col + col,
                    .width = visualizer_width,
                    .height = progress_bar_height,
                };
                if (dirty & DIRTY_VISUALIZER)
                        result = component_progress_bar(model, progress_rect, buf, dirty);
        }

        // Error row
        if (height >= meta_row + metadata_height + time_height + lyrics_height + visualizer_height + 1) {
                // Progress bar on last row of visualizer
                k_Rect progress_rect = {
                    .row = region.row + meta_row + metadata_height + lyrics_height + time_height + visualizer_height,
                    .col = region.col + col,
                    .width = visualizer_width,
                    .height = 1,
                };
                if (dirty & DIRTY_VISUALIZER)
                        result = component_error_row(model, progress_rect, buf, dirty);
        }

        // Footer
        if (height >= meta_row + metadata_height + time_height + lyrics_height + visualizer_height + 2) {
                // Progress bar on last row of visualizer
                k_Rect progress_rect = {
                    .row = region.row + meta_row + metadata_height + lyrics_height + time_height + visualizer_height + 1,
                    .col = region.col + col,
                    .width = visualizer_width,
                    .height = 1,
                };
                if (dirty & DIRTY_VISUALIZER)
                        result = component_footer(model, progress_rect, buf, dirty);
        }

        return result;
}

ComponentMsg component_lyrics_page(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        SongData *songdata = model->songdata;

        if (!songdata)
                return (ComponentMsg){0};

        Lyrics *lyrics = songdata->lyrics;

        if (!lyrics || lyrics->count == 0) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         _("No lyrics available. Press %s to go back."),
                         get_binding_string(MSG_SHOWLYRICSPAGE, true));
                CellStyle plain = cell_style_plain();
                draw_buffer_set_string_truncated(buf, region.row, region.col,
                                                 msg, region.width, plain);
                return (ComponentMsg){0};
        }

        double seconds = model->elapsed_seconds;
        int height = region.height;

        // Find highlight and scroll offset
        int offset = 0;
        int offset_result = -1;
        int highlight = -1;

        if (lyrics->isTimed) {
                for (int i = 0; i < (int)lyrics->count; i++) {
                        if (lyrics->lines[i].timestamp <= seconds)
                                highlight = i;
                        else
                                break;
                }
                if (highlight > height / 2)
                        offset = highlight - (height / 2);
        } else {
                if (height < (int)lyrics->count) {
                        if (model->state.ui.chosen_lyrics_row < 0) {
                                offset = 0;
                        } else if (model->state.ui.chosen_lyrics_row > (int)lyrics->count - height) {
                                offset = (int)lyrics->count - height;
                        } else {
                                offset = model->state.ui.chosen_lyrics_row;
                        }
                        offset_result = offset;
                }
        }

        // Render visible lines
        int last_line = MIN((int)lyrics->count, height + offset);

        for (int i = offset; i < last_line; i++) {
                const char *text = lyrics->lines[i].text ? lyrics->lines[i].text : "";

                CellStyle style;
                if (highlight == i && lyrics->isTimed)
                        style = cell_style_from_theme(ui->theme.nowplaying);
                else
                        style = cell_style_from_theme(ui->theme.trackview_lyrics);

                int draw_row = region.row + (i - offset);
                draw_buffer_set_string_truncated(buf, draw_row, region.col,
                                                 text, region.width, style);
        }

        // Blank remaining rows
        CellStyle plain = cell_style_plain();
        int printed = last_line - offset;
        for (int r = printed; r < height; r++) {
                draw_buffer_set_string_truncated(buf, region.row + r, region.col,
                                                 "", region.width, plain);
        }

        ComponentMsg result = (ComponentMsg){0};

        if (offset_result >= 0) {
                result.has_msg = true;
                result.msg = (struct Msg){
                    .type = MSG_LYRICS_UPDATED,
                    .lyrics_offset = offset_result};
        }

        return result;
}

ComponentMsg component_track_portrait_lyrics(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        SongData *songdata = model->songdata;

        if (!songdata)
                return (ComponentMsg){0};

        // Now playing header
        k_Rect header_rect = {
            .row = region.row,
            .col = region.col,
            .width = region.width,
            .height = 2,
        };
        component_now_playing(model, header_rect, buf, dirty);

        // Full lyrics page
        k_Rect lyrics_rect = {
            .row = region.row + 2,
            .col = region.col,
            .width = region.width,
            .height = region.height - 2,
        };
        ComponentMsg result = component_lyrics_page(model, lyrics_rect, buf, dirty);

        return result;
}

ComponentMsg component_track_landscape_lyrics(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        SongData *songdata = model->songdata;

        if (!songdata)
                return (ComponentMsg){0};

        // Now playing header
        k_Rect header_rect = {
            .row = region.row + 1,
            .col = region.col,
            .width = region.width,
            .height = 1,
        };
        component_now_playing(model, header_rect, buf, dirty);

        // Full lyrics page
        k_Rect lyrics_rect = {
            .row = region.row + 3,
            .col = region.col,
            .width = region.width,
            .height = region.height - 3,
        };
        ComponentMsg result = component_lyrics_page(model, lyrics_rect, buf, dirty);

        return result;
}

ComponentMsg component_track_landscape(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;

        ComponentMsg result = (ComponentMsg){0};

        if (state->ui.showLyricsPage)
                result = component_track_landscape_lyrics(model, region, buf, dirty);
        else
                result = component_track_landscape_normal(model, region, buf, dirty);

        return result;
}

ComponentMsg component_vis_and_progress_bar(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;
        const UISettings *ui = &state->settings;

        int visualizer_width = model->state.ui.visualizer_width;
        int visualizer_height = region.height;

        if (ui->visualizer_mode == VIZ_OFF)
                visualizer_height = 0;

        if (ui->visualizer_height < visualizer_height)
                visualizer_height = ui->visualizer_height;

        // Visualizer
        k_Rect viz_rect = {
            .row = region.row,
            .col = region.col,
            .width = visualizer_width + 1,
            .height = (visualizer_height <= 1) ? 0 : visualizer_height - 1,
        };
        if (dirty & DIRTY_VISUALIZER)
                component_visualizer(model, viz_rect, buf, dirty);

        // Progress bar
        ComponentMsg result = (ComponentMsg){0};
        k_Rect progress_rect = {
            .row = region.row + visualizer_height - 1,
            .col = region.col,
            .width = visualizer_width + 1,
            .height = 1,
        };
        if (dirty & DIRTY_VISUALIZER)
                result = component_progress_bar(model, progress_rect, buf, dirty);

        return result;
}

ComponentMsg component_track_portrait_normal(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        SongData *songdata = model->songdata;

        if (!songdata)
                return (ComponentMsg){0};

        int visualizer_width = model->state.ui.visualizer_width;
        int visualizer_height = model->state.settings.visualizer_height;
        int metadata_height = 4;

        // Cover art or chroma frame
        k_Rect cover_rect = {
            .row = region.row,
            .col = region.col,
            .width = region.width,
            .height = model->preferred_height,
        };
        if (dirty & DIRTY_SONG)
                component_cover_centered(model, cover_rect, buf, dirty);

        // Metadata block starts below cover
        int meta_row = region.row + model->preferred_height + 1;

        k_Rect meta_rect = {
            .row = meta_row,
            .col = region.col,
            .width = region.width,
            .height = metadata_height,
        };
        if (dirty & (DIRTY_SONG | DIRTY_TITLE))
                component_metadata(model, meta_rect, buf, dirty);

        // Time / progress
        k_Rect time_rect = {
            .row = meta_row + metadata_height,
            .col = region.col,
            .width = region.width,
            .height = 1,
        };
        if (dirty & DIRTY_VISUALIZER) {

                if (model->state.settings.simpleTimeStatus)
                        component_time_simple_and_vol(model, time_rect, buf, dirty);
                else
                        component_time(model, time_rect, buf, dirty);
        }

        // Lyrics (timestamped, inline)
        k_Rect lyrics_rect = {
            .row = meta_row + metadata_height + 1,
            .col = region.col,
            .width = region.width - 1,
            .height = 1,
        };
        if (dirty & DIRTY_VISUALIZER && songdata->lyrics && songdata->lyrics->isTimed)
                component_timestamped_lyrics(model, lyrics_rect, buf, dirty);

        ComponentMsg result = (ComponentMsg){0};

        // Visualizer
        k_Rect viz_rect = {
            .row = meta_row + metadata_height + 2,
            .col = region.col,
            .width = visualizer_width + 1,
            .height = visualizer_height,
        };
        if (dirty & DIRTY_VISUALIZER)
                result = component_vis_and_progress_bar(model, viz_rect, buf, dirty);

        return result;
}

ComponentMsg component_track_portrait(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const AppState *state = &model->state;

        ComponentMsg result = (ComponentMsg){0};
        if (state->ui.showLyricsPage)
                result = component_track_portrait_lyrics(model, region, buf, dirty);
        else
                result = component_track_portrait_normal(model, region, buf, dirty);

        return result;
}

ComponentMsg component_track_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;
        (void)model;
        CellStyle plain = cell_style_plain();
        draw_buffer_set_string(buf, region.row, region.col, " ", plain);

        return (ComponentMsg){0};
}

ComponentMsg component_empty(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)model;
        (void)dirty;

        CellStyle plain = cell_style_plain();

        for (int r = 0; r < region.height; r++) {
                draw_buffer_set_string(buf, region.row + r, region.col, " ", plain);
        }

        return (ComponentMsg){0};
}

ComponentMsg component_track(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        ComponentMsg result = component_track_portrait(model, region, buf, dirty);

        return result;
}

ComponentMsg component_version(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;
        const UISettings *ui = &model->state.settings;
        CellStyle text_style = cell_style_from_theme(ui->theme.text);
        CellStyle version_style = cell_style_from_theme(ui->theme.help);

        const char *label = _("kew version: ");
        draw_buffer_set_string(buf, region.row, region.col, label, text_style);
        draw_buffer_set_string(buf, region.row,
                               region.col + utf8_display_width(label),
                               ui->VERSION, version_style);

        return (ComponentMsg){0};
}

ComponentMsg component_help(const Model *model, k_Rect region, DrawBuffer *buf,
                            DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;

        CellStyle text_style = cell_style_from_theme(ui->theme.text);
        CellStyle help_style = cell_style_from_theme(ui->theme.help);
        CellStyle link_style = cell_style_from_theme(ui->theme.link);

        int row = region.row;
        int col = region.col;
        int max_width = region.width;

        // Theme line, split into segments with different colors
        char theme_line[512];

        if (ui->colorMode == COLOR_MODE_ALBUM_ONE) {
                CellStyle color_style = cell_style_from_theme(ui->theme.link);
                draw_buffer_set_string(buf, row, col, _(" Theme: "), text_style);
                int c = col + utf8_display_width(_(" Theme: "));
                draw_buffer_set_string(buf, row, c, _("Using "), text_style);
                c += utf8_display_width(_("Using "));
                draw_buffer_set_string(buf, row, c, _("One Color "), color_style);
                c += utf8_display_width(_("One Color "));
                draw_buffer_set_string(buf, row, c, _("From Track Covers"), text_style);
        } else if (ui->colorMode == COLOR_MODE_ALBUM) {
                CellStyle color_style = cell_style_from_theme(ui->theme.link);
                draw_buffer_set_string(buf, row, col, _(" Theme: "), text_style);
                int c = col + utf8_display_width(_(" Theme: "));
                draw_buffer_set_string(buf, row, c, _("Using "), text_style);
                c += utf8_display_width(_("Using "));
                draw_buffer_set_string(buf, row, c, _("Colors "), color_style);
                c += utf8_display_width(_("Colors "));
                draw_buffer_set_string(buf, row, c, _("From Track Covers"), text_style);
        } else {
                snprintf(theme_line, sizeof(theme_line), _(" Theme: "));
                draw_buffer_set_string(buf, row, col, theme_line, text_style);
                int c = col + utf8_display_width(theme_line);
                draw_buffer_set_string(buf, row, c, ui->theme.theme_name, help_style);
                if (strcmp(ui->theme.theme_author, "") != 0) {
                        c += utf8_display_width(ui->theme.theme_name);
                        draw_buffer_set_string(buf, row, c, _(" Author: "), text_style);
                        c += utf8_display_width(_(" Author: "));
                        draw_buffer_set_string(buf, row, c, ui->theme.theme_author, help_style);
                }
        }
        row += 2;
        if (row >= region.row + region.height)
                return (ComponentMsg){0};

// Keybinding lines
#define HELP_LINE(fmt, ...)                                                                      \
        do {                                                                                     \
                if (row >= region.row + region.height)                                           \
                        return (ComponentMsg){0};                                                \
                char _line[512];                                                                 \
                snprintf(_line, sizeof(_line), fmt, ##__VA_ARGS__);                              \
                draw_buffer_set_string_truncated(buf, row++, col, _line, max_width, help_style); \
        } while (0)

        HELP_LINE(_(" · Play/Pause: %s"), get_binding_string(MSG_PLAY_PAUSE, false));
        HELP_LINE(_(" · Enqueue/Dequeue: %s"), get_binding_string(MSG_ENQUEUE, false));
        HELP_LINE(_(" · Enqueue and Play: %s"), get_binding_string(MSG_ENQUEUEANDPLAY, false));
        HELP_LINE(_(" · Quit: %s"), get_binding_string(MSG_QUIT, false));
        HELP_LINE(_(" · Switch tracks: %s and %s"),
                  get_binding_string(MSG_PREV, false),
                  get_binding_string(MSG_NEXT, false));
        HELP_LINE(_(" · Volume: %s and %s"),
                  get_binding_string(MSG_VOLUME_UP, false),
                  get_binding_string(MSG_VOLUME_DOWN, false));
        HELP_LINE(_(" · Scroll: %s, %s"),
                  get_binding_string(MSG_PREV_PAGE, false),
                  get_binding_string(MSG_NEXT_PAGE, false));
        HELP_LINE(_(" · Clear List: %s"), get_binding_string(MSG_CLEARPLAYLIST, false));
        HELP_LINE(_(" · Remove from playlist: %s"), get_binding_string(MSG_REMOVE, true));
        HELP_LINE(_(" · Move songs: %s/%s"),
                  get_binding_string(MSG_MOVESONGUP, true),
                  get_binding_string(MSG_MOVESONGDOWN, true));

        // Change view line
        if (row < region.row + region.height) {
                char view_line[512];
                snprintf(view_line, sizeof(view_line),
                         _(" · Change View: %s or %s, %s, %s, %s, %s or click the footer"),
                         get_binding_string(MSG_NEXTVIEW, false),
                         get_binding_string(MSG_SHOWPLAYLIST, true),
                         get_binding_string(MSG_SHOWLIBRARY, true),
                         get_binding_string(MSG_SHOWTRACK, true),
                         get_binding_string(MSG_SHOWSEARCH, true),
                         get_binding_string(MSG_SHOWHELP, true));
                draw_buffer_set_string_truncated(buf, row++, col, view_line, max_width, help_style);
        }

        HELP_LINE(_(" · Stop: %s"), get_binding_string(MSG_STOP, false));
        HELP_LINE(_(" · Update Library: %s"), get_binding_string(MSG_UPDATELIBRARY, false));
        HELP_LINE(_(" · Sort Library: %s"), get_binding_string(MSG_SORTLIBRARY, true));

        HELP_LINE(_(" · Cycle Color Mode: %s"),
                  get_binding_string(MSG_CYCLECOLORMODE, false));
        HELP_LINE(_(" · Cycle Themes: %s"), get_binding_string(MSG_CYCLETHEMES, false));
        HELP_LINE(_(" · Cycle Chroma Visualization: %s (requires Chroma)"),
                  get_binding_string(MSG_CYCLEVISUALIZATION, false));
        HELP_LINE(_(" · Cycle Repeat: %s (repeat/repeat list/off)"),
                  get_binding_string(MSG_TOGGLEREPEAT, false));
        HELP_LINE(_(" · Cycle Visualizer Mode: %s"), get_binding_string(MSG_CYCLEVISUALIZERMODE, false));

        HELP_LINE(_(" · Toggle ASCII Cover: %s (disables Chroma)"),
                  get_binding_string(MSG_TOGGLEASCII, false));
        HELP_LINE(_(" · Toggle Lyrics Page on Track View: %s"),
                  get_binding_string(MSG_SHOWLYRICSPAGE, false));
        HELP_LINE(_(" · Toggle Notifications: %s"),
                  get_binding_string(MSG_TOGGLENOTIFICATIONS, false));
        HELP_LINE(_(" · Toggle Crossfade (experimental) Always on: %s"),
                  get_binding_string(MSG_TOGGLECROSSFADE, false));

        HELP_LINE(_(" · Shuffle: %s"), get_binding_string(MSG_SHUFFLE, false));
        HELP_LINE(_(" · Seek: %s and %s"),
                  get_binding_string(MSG_SEEKBACK, false),
                  get_binding_string(MSG_SEEKFORWARD, false));

        HELP_LINE(_(" · Crossfade (experimental): quick %s medium %s slow %s"),
                  get_binding_string(MSG_CROSSFADE_QUICK, false),
                  get_binding_string(MSG_CROSSFADE_MEDIUM, false),
                  get_binding_string(MSG_CROSSFADE_SLOW, false));

        HELP_LINE(_(" · Export Playlist: %s (named after the first song)"),
                  get_binding_string(MSG_EXPORTPLAYLIST, false));
        HELP_LINE(_(" · Show Folders in Playlist: %s"),
                  get_binding_string(MSG_TOGGLEFOLDERDISPLAY, false));
        HELP_LINE(_(" · Add Song To 'kew favorites.m3u': %s (run with 'kew .')"),
                  get_binding_string(MSG_ADDTOFAVORITESPLAYLIST, false));

#undef HELP_LINE

        row += 2;
        if (row >= region.row + region.height)
                return (ComponentMsg){0};

        // Project links
        draw_buffer_set_string(buf, row, col, _(" Project URL: "), help_style);
        draw_buffer_set_string(buf, row, col + utf8_display_width(_(" Project URL: ")),
                               "https://codeberg.org/ravachol/kew", link_style);
        row++;
        if (row >= region.row + region.height)
                return (ComponentMsg){0};

        draw_buffer_set_string(buf, row, col, _(" Please Donate: "), help_style);
        draw_buffer_set_string(buf, row, col + utf8_display_width(_(" Please Donate: ")),
                               "https://ko-fi.com/ravachol", link_style);
        row += 3;
        if (row >= region.row + region.height)
                return (ComponentMsg){0};

        // Copyright
        draw_buffer_set_string_truncated(buf, row, col,
                                         " Copyright © 2022-2026 Ravachol",
                                         max_width, text_style);

        return (ComponentMsg){0};
}

ComponentMsg component_library_rows(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        ComponentMsg result = (ComponentMsg){0};

        if (model->library == NULL)
                return result;

        int max_name_width = region.width - 2;
        if (max_name_width < 0)
                max_name_width = 0;

        int row_count = 0;
        int iter = 0;

        FileSystemEntry *current_lib_entry = component_library_helper_render_node(model, model->library, 0, region.height, max_name_width, region, buf, &row_count, &iter);

        // Clear remaining rows
        CellStyle empty = cell_style_plain();
        for (int i = row_count; i < region.height; i++) {
                draw_buffer_set_string(buf, region.row + i, region.col, "", empty);
        }

        int chosen_row = model->state.ui.chosen_lib_row;
        if (chosen_row > iter) {
                chosen_row = iter--;
        }

        result.has_msg = true;
        result.msg = (struct Msg){
            .type = MSG_LIBRARY_ROW_SELECTED,
            .current_lib_entry = current_lib_entry,
            .chosen_lib_row = chosen_row,
            .num_lib_rows = iter};

        return result;
}

ComponentMsg component_search_box(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        const UISettings *ui = &model->state.settings;
        CellStyle label_style = cell_style_from_theme(ui->theme.search_label);
        CellStyle query_style = cell_style_from_theme(ui->theme.search_query);

        char line[256];
        char query[256];

        int max_width = region.width;

        snprintf(line, sizeof(line), _("Search:"));
        snprintf(query, sizeof(query), "%s█", model->state.ui.search_text);;
        int line_width = utf8_display_width(line);
        draw_buffer_set_string_truncated(buf, region.row, region.col, line, line_width, label_style);
        draw_buffer_set_string_truncated(buf, region.row, region.col + line_width + 1, query, max_width, query_style);
        draw_buffer_set_string(buf, region.row + 2, region.col, "", label_style); // blank line below

        return (ComponentMsg){0};
}

void component_search_helper_collapse_view(Model *model, int diff_rows)
{
        UIState *uis = &model->state.ui;

        if (uis->allowChooseSearchSongs && (model->state.ui.chosen_search_dir == NULL ||
                                            (uis->current_search_entry != NULL && uis->current_search_entry->parent != NULL &&
                                             model->state.ui.chosen_search_dir != NULL))) {

                set_dirty(DIRTY_LIBRARY);

                uis->allowChooseSearchSongs = false;

                int num_children = 0;
                if (model->state.ui.chosen_search_dir != NULL && diff_rows > 0) {
                        FileSystemEntry *child = model->state.ui.chosen_search_dir->children;

                        while (child != NULL) {
                                child = child->next;
                                num_children++;
                        }
                }

                model->state.ui.chosen_search_dir = NULL;

                model->state.ui.chosen_search_result_row -= num_children;
        }
}

ComponentMsg component_search_results(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        (void)dirty;

        ComponentMsg result = (ComponentMsg){0};

        const AppState *state = &model->state;
        const UISettings *ui = &state->settings;
        const UIState *uis = &state->ui;

        int max_list_size = region.height;
        int max_name_width = region.width - 5;

        if (!ui->hideSideCover)
                max_name_width -= region.col / 4;
        if (max_name_width < 0)
                max_name_width = 0;

        int chosen_row = uis->chosen_search_result_row;

        FileSystemEntry *chosen_dir = model->state.ui.chosen_search_dir;
        FileSystemEntry *current_search_entry = model->state.ui.current_search_entry;

        // Clamp chosen row
        if (chosen_row >= (int)model->state.ui.search_results_count - 1)
                chosen_row = (int)model->state.ui.search_results_count - 1;
        if (chosen_row < 0)
                chosen_row = 0;

        // Calculate start iterator
        int start_search_iter = 0;

        if (chosen_row > (int)(start_search_iter + round(max_list_size / 2)))
                start_search_iter = chosen_row - (int)round(max_list_size / 2) + 1;

        if (chosen_row < start_search_iter)
                start_search_iter = chosen_row;

        if (chosen_row < 0)
                start_search_iter = chosen_row = 0;

        int iter = start_search_iter;

        // First pass, count total rows, skip directory entries if dir not open
        int total_rows = 0;

        for (int i = 0; i < model->state.ui.search_results_count; i++) {
                if (start_search_iter >= model->state.ui.search_results_count)
                        start_search_iter = model->state.ui.search_results_count - 1;

                if (i < start_search_iter && model->search_results[i].entry->is_directory && model->search_results[i].entry->parent != NULL && model->search_results[i].entry->parent->parent != NULL) {

                        int num = model->search_results[i].num_children;

                        if (!uis->allowChooseSearchSongs || (chosen_dir != NULL && model->search_results[i].entry->id != chosen_dir->id && !is_contained_within(chosen_dir, model->search_results[i].entry))) {
                                if (i < start_search_iter) {
                                        start_search_iter += num;
                                        i += num;
                                }
                                total_rows += num;
                        }
                }

                total_rows++;
        }

        // Second pass, use total_rows as upper bound, not results_count
        int printed_rows = 0;
        int draw_row = region.row;
        bool is_chosen = false;

        for (int i = start_search_iter; i < total_rows; i++) {

                if (printed_rows >= max_list_size)
                        break;

                // Clamp chosen row at end of results
                if (((model->state.ui.search_results_count <= i + 1) ||
                     (model->search_results[i].entry->is_directory &&
                      !uis->allowChooseSearchSongs &&
                      model->state.ui.search_results_count == i + 1 + model->search_results[i].num_children)) &&
                    chosen_row > iter) {

                        chosen_row = iter;
                }

                is_chosen = (chosen_row == iter);

                if (is_chosen)
                        current_search_entry = model->search_results[i].entry;

                int depth = determine_depth(model->search_results[i].entry);
                int extra_indent = calc_indentation(depth);
                int name_width = max_name_width - extra_indent;
                if (name_width < 0)
                        name_width = 0;

                Node *current = get_current_song();
                bool is_playing = current != NULL && strcmp(current->song.file_path,
                                                            model->search_results[i].entry->full_path) == 0;

                // Build display name
                char name[max_name_width + 1];
                name[0] = '\0';

                if (model->search_results[i].entry->is_directory) {

                        if (model->search_results[i].entry->parent != NULL && model->search_results[i].entry->parent->parent == NULL) {
                                char *upper = string_to_upper(model->search_results[i].entry->name);
                                snprintf(name, name_width + 1, "%s", upper);
                                free(upper);
                        } else {
                                snprintf(name, name_width + 1, "[%s]",
                                         model->search_results[i].entry->name);
                        }
                } else {
                        snprintf(name, name_width + 1, "%s [%s]",
                                 model->search_results[i].entry->name,
                                 model->search_results[i].entry->parent != NULL
                                     ? model->search_results[i].entry->parent->name
                                     : "Root");
                }

                draw_search_row(buf, draw_row, region.col, region.width,
                                name, extra_indent,
                                model->search_results[i].entry, ui, is_chosen, is_playing);

                draw_row++;
                printed_rows++;

                // Skip children of collapsed directory
                if (model->search_results[i].entry->is_directory &&
                    model->search_results[i].entry->parent != NULL &&
                    model->search_results[i].entry->parent->parent != NULL &&
                    (!uis->allowChooseSearchSongs || (chosen_dir && model->search_results[i].entry->id != chosen_dir->id))) {

                        FileSystemEntry *child = model->search_results[i].entry->children;

                        int count = model->state.ui.search_results_count;
                        while (child != NULL) {
                                if (!child->is_directory && i + 1 < count && model->search_results[i + 1].entry->id == child->id)
                                        i++;
                                child = child->next;
                        }
                }

                iter++;
        }

        // Blank remaining rows
        CellStyle plain = cell_style_plain();
        while (printed_rows < max_list_size) {
                draw_buffer_set_string_truncated(buf, draw_row++, region.col,
                                                 "", region.width, plain);
                printed_rows++;
        }

        result.has_msg = true;
        result.msg = (struct Msg){
            .type = MSG_SEARCH_ROW_SELECTED,
            .current_search_entry = current_search_entry,
            .chosen_search_result_row = chosen_row};

        return result;
}

ComponentMsg component_search(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty)
{
        const UISettings *ui = &model->state.settings;

        int header_height = (model->term_w > MIN_HEADER_WIDTH && !model->state.settings.hideHelp) ? 3 : 1;
        int logo_height = (ui->hideLogo) ? 3 : LOGO_HEIGHT;

        k_Rect logo_rect = {
            .row = 0,
            .col = region.col + 1,
            .width = region.width,
            .height = logo_height,
        };
        if (dirty & DIRTY_SONG)
                component_logo(model, logo_rect, buf, dirty);

        k_Rect header_rect = {
            .row = logo_rect.height,
            .col = region.col + 2,
            .width = region.width,
            .height = header_height,
        };
        if (dirty & DIRTY_ALL)
                component_search_header(model, header_rect, buf, dirty);

        k_Rect search_box_rect = {
            .row = header_rect.row + header_rect.height,
            .col = region.col,
            .width = region.width,
            .height = 2,
        };

        k_Rect search_results_rect = {
            .row = search_box_rect.row + search_box_rect.height,
            .col = region.col,
            .width = region.width,
            .height = region.height,
        };

        if (dirty & DIRTY_SEARCH) {
                component_search_box(model, search_box_rect, buf, dirty);
                component_search_results(model, search_results_rect, buf, dirty);
        }

        return (ComponentMsg){0};
}
