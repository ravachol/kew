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

#include "img_func.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Disable some warnings for stb headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#pragma GCC diagnostic pop

/* Include after chafa.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#include <io.h>
#else
#include <sys/ioctl.h> /* ioctl */
#endif

#define MACRO_STRLEN(s) (sizeof(s) / sizeof(s[0]))

char scale[] = "$@&B%8WM#ZO0QoahkbdpqwmLCJUYXIjft/\\|()1{}[]l?zcvunxr!<>i;:*-+~_,\"^`'. ";
unsigned int brightness_levels = MACRO_STRLEN(scale) - 2;

#ifdef CHAFA_VERSION_1_16

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

void get_tty_size(TermSize *term_size_out)
{
        TermSize term_size;

        term_size.width_cells = term_size.height_cells = term_size.width_pixels = term_size.height_pixels = -1;

#ifdef G_OS_WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
                CONSOLE_SCREEN_BUFFER_INFO csb_info;

                if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info)) {
                        term_size.width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
                        term_size.height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
                }
        }
#else
        {
                struct winsize w;
                gboolean have_winsz = FALSE;

                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
                        have_winsz = TRUE;

                if (have_winsz) {
                        term_size.width_cells = w.ws_col;
                        term_size.height_cells = w.ws_row;
                        term_size.width_pixels = w.ws_xpixel;
                        term_size.height_pixels = w.ws_ypixel;
                }
        }
#endif

        if (term_size.width_cells <= 0)
                term_size.width_cells = -1;
        if (term_size.height_cells <= 2)
                term_size.height_cells = -1;

        /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
         * aspect information for the font used. Sixel-capable terminals
         * like mlterm set these fields, but most others do not. */

        if (term_size.width_pixels <= 0 || term_size.height_pixels <= 0) {
                term_size.width_pixels = -1;
                term_size.height_pixels = -1;
        }

        *term_size_out = term_size;
}

void tty_init(void)
{
#ifdef G_OS_WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);

                saved_console_output_cp = GetConsoleOutputCP();
                saved_console_input_cp = GetConsoleCP();

                /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */

                if (chd != INVALID_HANDLE_VALUE) {
                        if (!SetConsoleMode(chd,
                                            ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN))
                                win32_stdout_is_file = TRUE;
                }

                /* Set UTF-8 code page I/O */

                SetConsoleOutputCP(65001);
                SetConsoleCP(65001);
        }
#endif
}

static GString *
convert_image(const void *pixels, gint pix_width, gint pix_height,
              gint pix_rowstride, ChafaPixelType pixel_type,
              gint width_cells, gint height_cells,
              gint cell_width, gint cell_height)
{
        ChafaTermInfo *term_info;
        ChafaCanvasMode mode;
        ChafaPixelMode pixel_mode;
        ChafaSymbolMap *symbol_map;
        ChafaCanvasConfig *config;
        ChafaCanvas *canvas;
        GString *printable;

#ifdef CHAFA_VERSION_1_16

        ChafaPassthrough passthrough;
        ChafaFrame *frame;
        ChafaImage *image;
        ChafaPlacement *placement;

        detect_terminal(&term_info, &mode, &pixel_mode,
                        &passthrough, &symbol_map);

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
        chafa_placement_set_tuck(placement, CHAFA_TUCK_STRETCH);
        chafa_placement_set_halign(placement, CHAFA_ALIGN_START);
        chafa_placement_set_valign(placement, CHAFA_ALIGN_START);
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
#else
        detect_terminal(&term_info, &mode, &pixel_mode);

        /* Specify the symbols we want */

        symbol_map = chafa_symbol_map_new();
        chafa_symbol_map_add_by_tags(symbol_map, CHAFA_SYMBOL_TAG_BLOCK);

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

// The function to load and return image data
unsigned char *get_bitmap(const char *image_path, int *width, int *height)
{
        if (image_path == NULL)
                return NULL;

        int channels;

        unsigned char *image = stbi_load(image_path, width, height, &channels, 4); // Force 4 channels (RGBA)
        if (!image) {
                fprintf(stderr, "Failed to load image: %s\n", image_path);
                return NULL;
        }

        return image;
}

float calc_aspect_ratio(void)
{
        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
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

        tty_init();
        get_tty_size(&term_size);

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

        // Calculate corrected width based on aspect ratio correction
        return (float)cell_height / (float)cell_width;
}

void print_square_bitmap(int row, int col, unsigned char *pixels, int width, int height, int base_height)
{
        if (pixels == NULL) {
                set_error_message("Invalid pixel data.\n");
                return;
        }

        // Use the provided width and height
        int pix_width = width;
        int pix_height = height;
        int n_channels = 4; // Assuming RGBA format

        // Validate the image dimensions
        if (pix_width == 0 || pix_height == 0) {
                set_error_message("Invalid image dimensions.\n");
                return;
        }

        TermSize term_size;
        GString *printable;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
            term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
        }

        // Set default cell size for some terminals
        if (cell_width <= -1 || cell_height <= -1) {
                cell_width = 8;
                cell_height = 16;
        }

        if (cell_width == 0 || cell_height == 0) {
                set_error_message("Invalid image cell width dimensions.\n");
                return;
        }

        // Calculate corrected width based on aspect ratio correction
        float aspect_ratio_correction = (float)cell_height / (float)cell_width;
        int corrected_width = (int)(base_height * aspect_ratio_correction);

        if (term_size.width_cells > 0 && corrected_width > term_size.width_cells) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }

        if (term_size.height_cells > 0 && base_height > term_size.height_cells) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }

        // Convert image to a printable string using Chafa
        printable = convert_image(
            pixels,
            pix_width,
            pix_height,
            pix_width * n_channels,         // Row stride
            CHAFA_PIXEL_RGBA8_UNASSOCIATED, // Correct pixel format
            corrected_width,
            base_height,
            cell_width,
            cell_height);

        // Ensure the string is null-terminated
        g_string_append_c(printable, '\0');

        // Split the printable string into lines
        const gchar *delimiters = "\n";
        gchar **lines = g_strsplit(printable->str, delimiters, -1);

        // Print each line with indentation
        for (int i = 0; lines[i] != NULL; i++) {
                printf("\033[%d;%dH", row + i, col);
                printf("%s", lines[i]);
                fflush(stdout);
        }

        // Free allocated memory
        g_strfreev(lines);
        g_string_free(printable, TRUE);
}

void print_square_bitmap_centered(unsigned char *pixels, int width, int height, int base_height)
{
        if (pixels == NULL) {
                set_error_message("Error: Invalid pixel data.\n");
                return;
        }

        // Use the provided width and height
        int pix_width = width;
        int pix_height = height;
        int n_channels = 4; // Assuming RGBA format

        // Validate the image dimensions
        if (pix_width == 0 || pix_height == 0) {
                set_error_message("Invalid image dimensions.\n");
                return;
        }

        TermSize term_size;
        GString *printable;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 &&
            term_size.width_pixels > 0 && term_size.height_pixels > 0) {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
        }

        // Set default cell size for some terminals
        if (cell_width <= -1 || cell_height <= -1) {
                cell_width = 8;
                cell_height = 16;
        }

        if (cell_width == 0 || cell_height == 0) {
                set_error_message("Invalid image cell width dimensions.\n");
                return;
        }

        // Calculate corrected width based on aspect ratio correction
        float aspect_ratio_correction = (float)cell_height / (float)cell_width;
        int corrected_width = (int)(base_height * aspect_ratio_correction);

        if (term_size.width_cells > 0 && corrected_width > term_size.width_cells) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }

        if (term_size.height_cells > 0 && base_height > term_size.height_cells) {
                set_error_message("Invalid terminal dimensions.\n");
                return;
        }

        // Calculate indentation to center the image
        int indentation = ((term_size.width_cells - corrected_width) / 2);

        // Convert image to a printable string using Chafa
        printable = convert_image(
            pixels,
            pix_width,
            pix_height,
            pix_width * n_channels,         // Row stride
            CHAFA_PIXEL_RGBA8_UNASSOCIATED, // Correct pixel format
            corrected_width,
            base_height,
            cell_width,
            cell_height);

        // Ensure the string is null-terminated
        g_string_append_c(printable, '\0');

        // Split the printable string into lines
        const gchar *delimiters = "\n";
        gchar **lines = g_strsplit(printable->str, delimiters, -1);

        // Print each line with indentation
        for (int i = 0; lines[i] != NULL; i++) {
                printf("\n\033[%dC%s", indentation, lines[i]);
        }

        // Free allocated memory
        g_strfreev(lines);
        g_string_free(printable, TRUE);
}

unsigned char luminance_from_r_g_b(unsigned char r, unsigned char g, unsigned char b)
{
        return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

void check_if_bright_pixel(unsigned char r, unsigned char g, unsigned char b, bool *found)
{
        // Calc luminace and use to find Ascii char.
        unsigned char ch = luminance_from_r_g_b(r, g, b);

        if (ch > 80 && !(r < g + 20 && r > g - 20 && g < b + 20 && g > b - 20) && !(r > 150 && g > 150 && b > 150)) {
                *found = true;
        }
}

int get_cover_color(unsigned char *pixels, int width, int height, unsigned char *r, unsigned char *g, unsigned char *b)
{
        if (pixels == NULL || width <= 0 || height <= 0) {
                return -1;
        }

        int channels = 4; // RGBA format

        bool found = false;
        int num_pixels = width * height;

        for (int i = 0; i < num_pixels; i++) {
                int index = i * channels;
                unsigned char red = pixels[index + 0];
                unsigned char green = pixels[index + 1];
                unsigned char blue = pixels[index + 2];

                check_if_bright_pixel(red, green, blue, &found);

                if (found) {
                        *r = red;
                        *g = green;
                        *b = blue;
                        break;
                }
        }

        return found ? 0 : -1;
}

unsigned char calc_ascii_char(PixelData *p)
{
        unsigned char ch = luminance_from_r_g_b(p->r, p->g, p->b);
        int rescaled = ch * brightness_levels / 256;

        return scale[brightness_levels - rescaled];
}

int convert_to_ascii_centered(const char *filepath, unsigned int height)
{
        /*
        Modified, originally by Danny Burrows:
        https://github.com/danny-burrows/img_to_txt

        MIT License

        Copyright (c) 2021 Danny Burrows

        Permission is hereby granted, free of charge, to any person obtaining a copy
        of this software and associated documentation files (the "Software"), to deal
        in the Software without restriction, including without limitation the rights
        to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
        copies of the Software, and to permit persons to whom the Software is
        furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be included in all
        copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
        SOFTWARE.
        */

        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

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
        unsigned int corrected_width = (int)(height * aspect_ratio_correction) - 1;

        // Calculate indentation to center the image
        int indent = ((term_size.width_cells - corrected_width) / 2);

        int rwidth, rheight, rchannels;
        unsigned char *read_data = stbi_load(filepath, &rwidth, &rheight, &rchannels, 3);

        if (read_data == NULL) {
                return -1;
        }

        PixelData *data;
        if (corrected_width != (unsigned)rwidth || height != (unsigned)rheight) {
                // 3 * uint8 for RGB!
                unsigned char *new_data = malloc(3 * sizeof(unsigned char) * corrected_width * height);
                stbir_resize_uint8_srgb(
                    read_data, rwidth, rheight, 0,
                    new_data, corrected_width, height, 0, 3);

                stbi_image_free(read_data);
                data = (PixelData *)new_data;
        } else {
                data = (PixelData *)read_data;
        }

        printf("\n");
        printf("%*s", indent, "");

        for (unsigned int d = 0; d < corrected_width * height; d++) {
                if (d % corrected_width == 0 && d != 0) {
                        printf("\n");
                        printf("%*s", indent, "");
                }

                PixelData *c = data + d;

                printf("\033[1;38;2;%03u;%03u;%03um%c", c->r, c->g, c->b, calc_ascii_char(c));
        }

        printf("\n");

        stbi_image_free(data);
        return 0;
}

int convert_to_ascii(int indentation, const char *filepath, unsigned int height)
{
        /*
        Modified, originally by Danny Burrows:
        https://github.com/danny-burrows/img_to_txt

        MIT License

        Copyright (c) 2021 Danny Burrows

        Permission is hereby granted, free of charge, to any person obtaining a copy
        of this software and associated documentation files (the "Software"), to deal
        in the Software without restriction, including without limitation the rights
        to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
        copies of the Software, and to permit persons to whom the Software is
        furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be included in all
        copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
        SOFTWARE.
        */

        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

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
        unsigned int corrected_width = (int)(height * aspect_ratio_correction) - 1;

        int rwidth, rheight, rchannels;
        unsigned char *read_data = stbi_load(filepath, &rwidth, &rheight, &rchannels, 3);

        if (read_data == NULL) {
                return -1;
        }

        PixelData *data;
        if (corrected_width != (unsigned)rwidth || height != (unsigned)rheight) {
                // 3 * uint8 for RGB!
                unsigned char *new_data = malloc(3 * sizeof(unsigned char) * corrected_width * height);
                stbir_resize_uint8_srgb(
                    read_data, rwidth, rheight, 0,
                    new_data, corrected_width, height, 0, 3);

                stbi_image_free(read_data);
                data = (PixelData *)new_data;
        } else {
                data = (PixelData *)read_data;
        }

        printf("\n");
        printf("%*s", indentation, "");

        for (unsigned int d = 0; d < corrected_width * height; d++) {
                if (d % corrected_width == 0 && d != 0) {
                        printf("\n");
                        printf("%*s", indentation, "");
                }

                PixelData *c = data + d;

                printf("\033[1;38;2;%03u;%03u;%03um%c", c->r, c->g, c->b, calc_ascii_char(c));
        }

        printf("\n");

        stbi_image_free(data);
        return 0;
}

int print_in_ascii(int indentation, const char *path_to_img_file, int height)
{
        printf("\r");

        int ret = convert_to_ascii(indentation, path_to_img_file, (unsigned)height);
        if (ret == -1)
                printf("\033[0m");
        return 0;
}

int print_in_ascii_centered(const char *path_to_img_file, int height)
{
        printf("\r");

        int ret = convert_to_ascii_centered(path_to_img_file, (unsigned)height);
        if (ret == -1)
                printf("\033[0m");
        return 0;
}
