/**
 * @file img_func.c
 * @brief Image rendering and conversion helpers using Chafa.
 *
 * Handles loading and displaying album art or other images in the terminal
 * using the Chafa library. Supports scaling, aspect-ratio preservation,
 * and different rendering modes (truecolor, ASCII, etc.).
 */

#include "common/appstate.h"
#include "common/common.h"
#include "common/model.h"

#include "img_func.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Include after chafa.h for G_OS_WIN32 */
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/ioctl.h> /* ioctl */
#endif

#if CHAFA_VERSION_CUR_STABLE >= G_ENCODE_VERSION(1, 16)

static gchar *tmux_allow_passthrough_original;
static gboolean tmux_allow_passthrough_is_changed;

static gboolean
apply_passthrough_workarounds_tmux(void)
{
        gboolean result = FALSE;
        gchar *standard_output = NULL;
        gchar *standard_error = NULL;
        gchar **argv = NULL;
        gint wait_status = -1;
        gchar *mode = NULL;

        /* Use g_spawn_sync with explicit argv to avoid shell injection */
        argv = g_new0(gchar *, 4);
        argv[0] = g_strdup("tmux");
        argv[1] = g_strdup("show");
        argv[2] = g_strdup("allow-passthrough");
        argv[3] = NULL;

        if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, &standard_output, &standard_error,
                          &wait_status, NULL)) {
                g_strfreev(argv);
                goto out;
        }
        g_strfreev(argv);

        /* Parse output safely */
        if (standard_output && *standard_output) {
                gchar **lines = g_strsplit(standard_output, "\n", 2);
                if (lines[0]) {
                        gchar **parts = g_strsplit(lines[0], " ", 3);
                        if (parts[0] && parts[1]) {
                                mode = g_ascii_strdown(parts[1], -1);
                                g_strstrip(mode);
                        }
                        g_strfreev(parts);
                }
                g_strfreev(lines);
        }

        if (!mode || (strcmp(mode, "on") && strcmp(mode, "all"))) {
                argv = g_new0(gchar *, 4);
                argv[0] = g_strdup("tmux");
                argv[1] = g_strdup("set-option");
                argv[2] = g_strdup("allow-passthrough on");
                argv[3] = NULL;

                result = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                      NULL, NULL, &standard_output, &standard_error,
                                      &wait_status, NULL);
                g_strfreev(argv);

                if (result) {
                        tmux_allow_passthrough_original = mode;
                        tmux_allow_passthrough_is_changed = TRUE;
                }
        } else {
                g_free(mode);
        }

out:
        g_free(standard_output);
        g_free(standard_error);
        g_free(mode);
        return result;
}

gboolean
retirePassthroughWorkarounds_tmux(void)
{
        gboolean result = FALSE;
        gchar *standard_output = NULL;
        gchar *standard_error = NULL;
        gint wait_status = -1;
        gchar **argv = NULL;

        if (!tmux_allow_passthrough_is_changed)
                return TRUE;

        if (tmux_allow_passthrough_original) {
                // Use argument array to avoid shell injection
                argv = g_new0(gchar *, 5);
                argv[0] = g_strdup("tmux");
                argv[1] = g_strdup("set-option");
                argv[2] = g_strdup("allow-passthrough");
                argv[3] = g_strdup(tmux_allow_passthrough_original);
                argv[4] = NULL;
        } else {
                // Use argument array for unsetting the option
                argv = g_new0(gchar *, 4);
                argv[0] = g_strdup("tmux");
                argv[1] = g_strdup("set-option");
                argv[2] = g_strdup("-u");
                argv[3] = g_strdup("allow-passthrough");
        }

        result = g_spawn_sync(
            NULL, argv, NULL,
            G_SPAWN_SEARCH_PATH,
            NULL, NULL,
            &standard_output, &standard_error,
            &wait_status, NULL);

        // Free the argument array
        for (int i = 0; argv[i] != NULL; i++)
                g_free(argv[i]);
        g_free(argv);

        if (result) {
                g_free(tmux_allow_passthrough_original);
                tmux_allow_passthrough_original = NULL;
                tmux_allow_passthrough_is_changed = FALSE;
        }

        g_free(standard_output);
        g_free(standard_error);
        return result;
}

static void detect_terminal(ChafaTermInfo **term_info_out, ChafaCanvasMode *mode_out, ChafaPixelMode *pixel_mode_out,
                            ChafaPassthrough *passthrough_out, ChafaSymbolMap **symbol_map_out)
{
        ChafaCanvasMode mode;
        ChafaPixelMode pixel_mode;
        ChafaPassthrough passthrough;
        ChafaTermInfo *term_info;
        gchar **envp;

        /* Examine the environment variables and guess what the terminal can do */

        envp = g_get_environ();
        term_info = chafa_term_db_detect(chafa_term_db_get_default(), envp);

        /* Pick the most high-quality rendering possible */

        mode = chafa_term_info_get_best_canvas_mode(term_info);
        pixel_mode = chafa_term_info_get_best_pixel_mode(term_info);
        passthrough = chafa_term_info_get_is_pixel_passthrough_needed(term_info, pixel_mode)
                          ? chafa_term_info_get_passthrough_type(term_info)
                          : CHAFA_PASSTHROUGH_NONE;

        const gchar *term_name = chafa_term_info_get_name(term_info);

        if (strstr(term_name, "tmux") != NULL && pixel_mode != CHAFA_PIXEL_MODE_KITTY) {
                /* Always use sixels in tmux */
                pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
                mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        }

        *symbol_map_out = chafa_symbol_map_new();
        chafa_symbol_map_add_by_tags(*symbol_map_out,
                                     chafa_term_info_get_safe_symbol_tags(term_info));

        /* Hand over the information to caller */

        *term_info_out = term_info;
        *mode_out = mode;
        *pixel_mode_out = pixel_mode;
        *passthrough_out = passthrough;

        /* Cleanup */

        g_strfreev(envp);
}

#else

static void detect_terminal(ChafaTermInfo **term_info_out, ChafaCanvasMode *mode_out, ChafaPixelMode *pixel_mode_out)
{
        ChafaCanvasMode mode;
        ChafaPixelMode pixel_mode;
        ChafaTermInfo *term_info;
        gchar **envp;

        /* Examine the environment variables and guess what the terminal can do */

        envp = g_get_environ();
        term_info = chafa_term_db_detect(chafa_term_db_get_default(), envp);

        /* See which control sequences were defined, and use that to pick the most
         * high-quality rendering possible */

        if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1)) {
                pixel_mode = CHAFA_PIXEL_MODE_KITTY;
                mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        } else if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS)) {
                pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
                mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        } else {
                pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

                if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT))
                        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
                else if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_256) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_256) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_256))
                        mode = CHAFA_CANVAS_MODE_INDEXED_240;
                else if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_16) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_16))
                        mode = CHAFA_CANVAS_MODE_INDEXED_16;
                else if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_INVERT_COLORS) && chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_RESET_ATTRIBUTES))
                        mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
                else
                        mode = CHAFA_CANVAS_MODE_FGBG;
        }

        /* Hand over the information to caller */

        *term_info_out = term_info;
        *mode_out = mode;
        *pixel_mode_out = pixel_mode;

        /* Cleanup */

        g_strfreev(envp);
}
#endif

static ChafaPixelMode style_to_pixel_mode(const char *style)
{
        if (strcmp(style, "kitty") == 0)
                return CHAFA_PIXEL_MODE_KITTY;
        if (strcmp(style, "sixels") == 0)
                return CHAFA_PIXEL_MODE_SIXELS;
        if (strcmp(style, "block") == 0 ||
            strcmp(style, "braille") == 0 ||
            strcmp(style, "ascii") == 0 ||
            strcmp(style, "dot") == 0 ||
            strcmp(style, "vhalf") == 0 ||
            strcmp(style, "quad") == 0)
                return CHAFA_PIXEL_MODE_SYMBOLS;
        return (ChafaPixelMode)-1;
}

static ChafaSymbolTags style_to_symbol_tag(const char *style)
{
        if (strcmp(style, "braille") == 0)
                return CHAFA_SYMBOL_TAG_BRAILLE;
        if (strcmp(style, "ascii") == 0)
                return CHAFA_SYMBOL_TAG_ASCII;
        if (strcmp(style, "dot") == 0)
                return CHAFA_SYMBOL_TAG_DOT;
        if (strcmp(style, "vhalf") == 0)
                return CHAFA_SYMBOL_TAG_VHALF;
        if (strcmp(style, "quad") == 0)
                return CHAFA_SYMBOL_TAG_QUAD;
        return CHAFA_SYMBOL_TAG_BLOCK;
}

static GString *
convert_image(const void *pixels, gint pix_width, gint pix_height,
              gint pix_rowstride, ChafaPixelType pixel_type,
              gint width_cells, gint height_cells,
              gint cell_width, gint cell_height, const char *cover_style)
{
        ChafaTermInfo *term_info;
        ChafaCanvasMode mode;
        ChafaPixelMode pixel_mode;
        ChafaSymbolMap *symbol_map;
        ChafaCanvasConfig *config;
        ChafaCanvas *canvas;
        GString *printable;

#if CHAFA_VERSION_CUR_STABLE >= G_ENCODE_VERSION(1, 16)

        ChafaPassthrough passthrough;
        ChafaFrame *frame;
        ChafaImage *image;
        ChafaPlacement *placement;

        detect_terminal(&term_info, &mode, &pixel_mode,
                        &passthrough, &symbol_map);

#ifdef _WIN32
        if (getenv("WT_SESSION")) {
                pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
                passthrough = CHAFA_PASSTHROUGH_NONE;
        }
#endif

        ChafaPixelMode forced_mode = style_to_pixel_mode(cover_style);
        if (forced_mode != (ChafaPixelMode)-1) {
                pixel_mode = forced_mode;
                if (forced_mode == CHAFA_PIXEL_MODE_SYMBOLS)
                        passthrough = CHAFA_PASSTHROUGH_NONE;
                chafa_symbol_map_unref(symbol_map);
                symbol_map = chafa_symbol_map_new();
                chafa_symbol_map_add_by_tags(symbol_map, style_to_symbol_tag(cover_style));
        }

        if (passthrough == CHAFA_PASSTHROUGH_TMUX)
                apply_passthrough_workarounds_tmux();

        config = chafa_canvas_config_new();
        chafa_canvas_config_set_canvas_mode(config, mode);
        chafa_canvas_config_set_pixel_mode(config, pixel_mode);
        chafa_canvas_config_set_geometry(config, width_cells, height_cells);

        if (cell_width > 0 && cell_height > 0) {
                /* We know the pixel dimensions of each cell. Store it in the config. */

                chafa_canvas_config_set_cell_geometry(config, cell_width, cell_height);
        }

        chafa_canvas_config_set_passthrough(config, passthrough);
        chafa_canvas_config_set_symbol_map(config, symbol_map);

        canvas = chafa_canvas_new(config);
        frame = chafa_frame_new_borrow((gpointer)pixels, pixel_type,
                                       pix_width, pix_height, pix_rowstride);
        image = chafa_image_new();
        chafa_image_set_frame(image, frame);

        placement = chafa_placement_new(image, 1);
        chafa_placement_set_tuck(placement, CHAFA_TUCK_FIT);
        chafa_placement_set_halign(placement, CHAFA_ALIGN_CENTER);
        chafa_placement_set_valign(placement, CHAFA_ALIGN_CENTER);
        chafa_canvas_set_placement(canvas, placement);

        printable = chafa_canvas_print(canvas, NULL);

        /* Clean up and return */

        chafa_placement_unref(placement);
        chafa_image_unref(image);
        chafa_frame_unref(frame);
        chafa_canvas_unref(canvas);
        chafa_canvas_config_unref(config);
        chafa_symbol_map_unref(symbol_map);
        chafa_term_info_unref(term_info);
        canvas = NULL;
        config = NULL;
        symbol_map = NULL;
        term_info = NULL;

        return printable;
#elif CHAFA_VERSION_CUR_STABLE >= G_ENCODE_VERSION(1, 14)

        ChafaFrame *frame;
        ChafaImage *image;
        ChafaPlacement *placement;

        detect_terminal(&term_info, &mode, &pixel_mode);

        ChafaPixelMode forced_mode_14 = style_to_pixel_mode(cover_style);
        if (forced_mode_14 != (ChafaPixelMode)-1)
                pixel_mode = forced_mode_14;

        symbol_map = chafa_symbol_map_new();
        chafa_symbol_map_add_by_tags(symbol_map, style_to_symbol_tag(cover_style));

        config = chafa_canvas_config_new();
        chafa_canvas_config_set_canvas_mode(config, mode);
        chafa_canvas_config_set_pixel_mode(config, pixel_mode);
        chafa_canvas_config_set_geometry(config, width_cells, height_cells);
        chafa_canvas_config_set_symbol_map(config, symbol_map);

        if (cell_width > 0 && cell_height > 0)
                chafa_canvas_config_set_cell_geometry(config, cell_width, cell_height);

        canvas = chafa_canvas_new(config);
        frame = chafa_frame_new_borrow((gpointer)pixels, pixel_type, pix_width,
                                       pix_height, pix_rowstride);
        image = chafa_image_new();
        chafa_image_set_frame(image, frame);

        placement = chafa_placement_new(image, 1);
        chafa_placement_set_tuck(placement, CHAFA_TUCK_FIT);
        chafa_placement_set_halign(placement, CHAFA_ALIGN_CENTER);
        chafa_placement_set_valign(placement, CHAFA_ALIGN_CENTER);
        chafa_canvas_set_placement(canvas, placement);

        printable = chafa_canvas_print(canvas, term_info);

        chafa_placement_unref(placement);
        chafa_image_unref(image);
        chafa_frame_unref(frame);
        chafa_canvas_unref(canvas);
        chafa_canvas_config_unref(config);
        chafa_symbol_map_unref(symbol_map);
        chafa_term_info_unref(term_info);
        canvas = NULL;
        config = NULL;
        symbol_map = NULL;
        term_info = NULL;

        return printable;
#else
        detect_terminal(&term_info, &mode, &pixel_mode);

        ChafaPixelMode forced_mode_old = style_to_pixel_mode(cover_style);
        if (forced_mode_old != (ChafaPixelMode)-1)
                pixel_mode = forced_mode_old;

        symbol_map = chafa_symbol_map_new();
        chafa_symbol_map_add_by_tags(symbol_map, style_to_symbol_tag(cover_style));

        /* Set up a configuration with the symbols and the canvas size in characters */

        config = chafa_canvas_config_new();
        chafa_canvas_config_set_canvas_mode(config, mode);
        chafa_canvas_config_set_pixel_mode(config, pixel_mode);
        chafa_canvas_config_set_geometry(config, width_cells, height_cells);
        chafa_canvas_config_set_symbol_map(config, symbol_map);

        if (cell_width > 0 && cell_height > 0) {
                /* We know the pixel dimensions of each cell. Store it in the config. */

                chafa_canvas_config_set_cell_geometry(config, cell_width, cell_height);
        }

        /* Create canvas */

        canvas = chafa_canvas_new(config);

        /* Draw pixels to the canvas */

        chafa_canvas_draw_all_pixels(canvas,
                                     pixel_type,
                                     pixels,
                                     pix_width,
                                     pix_height,
                                     pix_rowstride);

        /* Build printable string */
        printable = chafa_canvas_print(canvas, term_info);

        /* Clean up and return */

        chafa_canvas_unref(canvas);
        chafa_canvas_config_unref(config);
        chafa_symbol_map_unref(symbol_map);
        chafa_term_info_unref(term_info);
        canvas = NULL;
        config = NULL;
        symbol_map = NULL;
        term_info = NULL;

        return printable;
#endif
}

float calc_aspect_ratio(void)
{
        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        get_tty_size(&term_size);

        if (term_size.cols > 0 && term_size.rows > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                cell_width = term_size.width_pixels / term_size.cols;
                cell_height = term_size.height_pixels / term_size.rows;
        }

        // Set default for some terminals
        if (cell_width == -1 && cell_height == -1) {
                cell_width = 8;
                cell_height = 16;
        }

        return (float)cell_height / (float)cell_width;
}

float get_aspect_ratio()
{
        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        get_tty_size(&term_size);

        if (term_size.cols > 0 && term_size.rows > 0 &&
            term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                cell_width = term_size.width_pixels / term_size.cols;
                cell_height = term_size.height_pixels / term_size.rows;
        }

        // Set default cell size for some terminals
        if (cell_width == -1 || cell_height == -1) {
                cell_width = 8;
                cell_height = 16;
        }

        // Calculate corrected width based on aspect ratio correction
        return (float)cell_height / (float)cell_width;
}

void free_image_payload(ImagePayload *img)
{
        if (!img)
                return;

        free(img->data);
        free(img);
}

void draw_square_bitmap_to_buf(DrawBuffer *buf, int row, int col,
                               unsigned char *pixels, int width, int height, int max_width,
                               int base_height, const TermSize *term_size, bool centered, size_t img_hash,
                               const char *cover_style, int just_mark_cover, bool draw_occupied_markers)
{
        int cell_width = 8;
        int cell_height = 16;

        printf("\033[%d;%dH", row, col);

        if (term_size->cols > 0 && term_size->rows > 0 &&
            term_size->width_pixels > 0 && term_size->height_pixels > 0) {
                cell_width = term_size->width_pixels / term_size->cols;
                cell_height = term_size->height_pixels / term_size->rows;
        }

        if (cell_width == 0 || cell_height == 0) {
                set_error_message("Invalid cell dimensions.\n");
                return;
        }

        float aspect = (float)cell_height / (float)cell_width;
        int corrected_width = (int)(base_height * aspect);

        if (corrected_width > max_width)
                corrected_width = max_width;

        if (term_size->cols > 0 && corrected_width > term_size->cols) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }
        if (term_size->rows > 0 && base_height > term_size->rows) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }

        if (centered && term_size->cols > 0)
                col = ((term_size->cols - corrected_width) / 2) + 1;

        Cell *anchor = &buf->cells[row * buf->cols + col];

        if (anchor->image)
                free_image_payload(anchor->image);

        // Store the encoded blob in an ImagePayload and place it in the buffer.
        // The terminal backend emits it verbatim at commit time — same as sixels.
        ImagePayload *img = calloc(1, sizeof(ImagePayload));
        if (!img) {
                return;
        }

        img->protocol = IMAGE_SIXEL;
        img->pixel_w = corrected_width * cell_width;
        img->pixel_h = base_height * cell_height;
        img->id = img_hash;
        img->screen_h = base_height;
        img->screen_w = corrected_width;

        anchor->kind = CELL_IMAGE_ANCHOR;
        anchor->image = img;

        // Place anchor cell
        if (row < 0 || row >= buf->rows || col < 0 || col >= buf->cols) {
                free(img->data);
                free(img);
                return;
        }

        if (draw_occupied_markers) {
                int rows = MIN(row + base_height, buf->rows);
                int cols = MIN(col + corrected_width, buf->cols);

                // Mark occupied region
                for (int r = row; r < rows; r++) {
                        for (int c = col; c < cols; c++) {
                                if (r == row && c == col)
                                        continue;
                                buf->cells[r * buf->cols + c].kind = CELL_OCCUPIED;
                        }
                }
        }

        if (width == 0 || height == 0 || !pixels) {
                return;
        }

        if (!just_mark_cover) {
                GString *printable = convert_image(
                    pixels, width, height,
                    width * 4,
                    CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                    corrected_width, base_height,
                    cell_width, cell_height, cover_style);

                if (!printable)
                        return;

                g_string_append_c(printable, '\0');
                img->data = (uint8_t *)g_string_free(printable, FALSE);
                img->data_len = strlen((char *)img->data);
        }
}
