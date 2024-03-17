#include "chafafunc.h"

/*

chafafunc.c

 Functions related to printing images to the terminal with chafa.

*/

/* Include after chafa.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#include <io.h>
#else
#include <sys/ioctl.h> /* ioctl */
#endif

typedef struct
{
        gint width_cells, height_cells;
        gint width_pixels, height_pixels;
} TermSize;

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

        if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1))
        {
                pixel_mode = CHAFA_PIXEL_MODE_KITTY;
                mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        }
        else if (chafa_term_info_have_seq(term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS))
        {
                pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
                mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        }
        else
        {
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

static void
get_tty_size(TermSize *term_size_out)
{
        TermSize term_size;

        term_size.width_cells = term_size.height_cells = term_size.width_pixels = term_size.height_pixels = -1;

#ifdef G_OS_WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
                CONSOLE_SCREEN_BUFFER_INFO csb_info;

                if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info))
                {
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

                if (have_winsz)
                {
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

        if (term_size.width_pixels <= 0 || term_size.height_pixels <= 0)
        {
                term_size.width_pixels = -1;
                term_size.height_pixels = -1;
        }

        *term_size_out = term_size;
}

static void
tty_init(void)
{
#ifdef G_OS_WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);

                saved_console_output_cp = GetConsoleOutputCP();
                saved_console_input_cp = GetConsoleCP();

                /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */

                if (chd != INVALID_HANDLE_VALUE)
                {
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

        if (cell_width > 0 && cell_height > 0)
        {
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
}

void printImage(const char *image_path, int width, int height)
{
        FreeImage_Initialise(false);

        // Detect the image type
        FREE_IMAGE_FORMAT image_format = FreeImage_GetFileType(image_path, 0);
        if (image_format == FIF_UNKNOWN)
        {
                // Failed to detect the image type
                printf("Unknown image format: %s\n", image_path);
                return;
        }

        // Load the image
        FIBITMAP *image = FreeImage_Load(image_format, image_path, 0);
        if (!image)
        {
                // Failed to load the image
                printf("Failed to load image: %s\n", image_path);
                return;
        }

        // Convert the image to a bitmap
        FIBITMAP *bitmap = FreeImage_ConvertTo32Bits(image);
        if (!bitmap)
        {
                // Failed to convert the image to a bitmap
                printf("Failed to convert image to bitmap.\n");
                FreeImage_Unload(image);
                return;
        }

        int pix_width = FreeImage_GetWidth(bitmap);
        int pix_height = FreeImage_GetHeight(bitmap);
        int n_channels = FreeImage_GetBPP(bitmap) / 8;
        unsigned char *pixels = (unsigned char *)FreeImage_GetBits(bitmap);

        FreeImage_FlipVertical(bitmap);

        TermSize term_size;
        GString *printable;
        gfloat font_ratio = 0.5;
        gint cell_width = -1, cell_height = -1;
        gint width_cells, height_cells;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0)
        {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
                font_ratio = (gdouble)cell_width / (gdouble)cell_height;
        }

        width_cells = term_size.width_cells;
        height_cells = term_size.height_cells;

        chafa_calc_canvas_geometry(pix_width, pix_height, &width_cells, &height_cells, font_ratio, TRUE, FALSE);

        /* Convert the image to a printable string */
        printable = convert_image(pixels, pix_width, pix_height, pix_width * n_channels, CHAFA_PIXEL_BGRA8_UNASSOCIATED,
                                  width, height, cell_width, cell_height);

        /* Print the string */

        fwrite(printable->str, sizeof(char), printable->len, stdout);
        fputc('\n', stdout);

        // Free resources
        FreeImage_Unload(bitmap);
        FreeImage_Unload(image);
        g_string_free(printable, TRUE);
}

FIBITMAP *getBitmap(const char *image_path)
{
        if (image_path == NULL)
                return NULL;

        FreeImage_Initialise(false);
        FREE_IMAGE_FORMAT image_format = FreeImage_GetFileType(image_path, 0);
        if (image_format == FIF_UNKNOWN)
        {
                return NULL;
        }
        FIBITMAP *image = FreeImage_Load(image_format, image_path, 0);
        if (!image)
        {
                return NULL;
        }
        FIBITMAP *bitmap = FreeImage_ConvertTo32Bits(image);
        FreeImage_FlipVertical(bitmap);
        FreeImage_Unload(image);

        if (!bitmap)
        {
                return NULL;
        }

        return bitmap;
}

void printBitmap(FIBITMAP *bitmap, int width, int height)
{
        if (bitmap == NULL)
        {
                return;
        }

        int pix_width = FreeImage_GetWidth(bitmap);
        int pix_height = FreeImage_GetHeight(bitmap);
        int n_channels = FreeImage_GetBPP(bitmap) / 8;
        unsigned char *pixels = (unsigned char *)FreeImage_GetBits(bitmap);

        TermSize term_size;
        GString *printable;
        gfloat font_ratio = 0.5;
        gint cell_width = -1, cell_height = -1;
        gint width_cells, height_cells;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0)
        {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
                font_ratio = (gdouble)cell_width / (gdouble)cell_height;
        }

        width_cells = term_size.width_cells;
        height_cells = term_size.height_cells;

        chafa_calc_canvas_geometry(pix_width, pix_height, &width_cells, &height_cells, font_ratio, TRUE, FALSE);

        // Convert image to a printable string
        printable = convert_image(pixels, pix_width, pix_height, pix_width * n_channels, CHAFA_PIXEL_BGRA8_UNASSOCIATED,
                                  width, height, cell_width, cell_height);

        fwrite(printable->str, sizeof(char), printable->len, stdout);
        g_string_free(printable, TRUE);
}

float calcAspectRatio()
{
        TermSize term_size;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0)
        {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
        }

        // Set default for some terminals
        if (cell_width == -1 && cell_height == -1)
        {
                cell_width = 8;
                cell_height = 16;
        }
        
        return (float)cell_height / (float)cell_width;        
}

void printSquareBitmapCentered(FIBITMAP *bitmap, int baseHeight)
{
        if (bitmap == NULL)
        {
                return;
        }
        int pix_width = FreeImage_GetWidth(bitmap);
        int pix_height = FreeImage_GetHeight(bitmap);
        int n_channels = FreeImage_GetBPP(bitmap) / 8;
        unsigned char *pixels = (unsigned char *)FreeImage_GetBits(bitmap);

        TermSize term_size;
        GString *printable;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0)
        {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
        }

        // Set default for some terminals
        if (cell_width == -1 && cell_height == -1)
        {
                cell_width = 8;
                cell_height = 16;
        }
        float aspect_ratio_correction = (float)cell_height / (float)cell_width;

        int correctedWidth = (int)(baseHeight * aspect_ratio_correction);

        // Convert image to a printable string
        printable = convert_image(pixels, pix_width, pix_height, pix_width * n_channels, CHAFA_PIXEL_BGRA8_UNASSOCIATED,
                                  correctedWidth, baseHeight, cell_width, cell_height);
        g_string_append_c(printable, '\0');
        const gchar *delimiters = "\n";
        gchar **lines = g_strsplit(printable->str, delimiters, -1);

        int indentation = ((term_size.width_cells - correctedWidth) / 2) + 1;

        for (int i = 0; lines[i] != NULL; i++)
        {
                printf("\n%*s%s", indentation, "", lines[i]);
        }

        g_strfreev(lines);
        g_string_free(printable, TRUE);
}

void printBitmapCentered(FIBITMAP *bitmap, int width, int height)
{
        if (bitmap == NULL)
        {
                return;
        }
        int pix_width = FreeImage_GetWidth(bitmap);
        int pix_height = FreeImage_GetHeight(bitmap);
        int n_channels = FreeImage_GetBPP(bitmap) / 8;
        unsigned char *pixels = (unsigned char *)FreeImage_GetBits(bitmap);

        TermSize term_size;
        GString *printable;
        gint cell_width = -1, cell_height = -1;

        tty_init();
        get_tty_size(&term_size);

        if (term_size.width_cells > 0 && term_size.height_cells > 0 && term_size.width_pixels > 0 && term_size.height_pixels > 0)
        {
                cell_width = term_size.width_pixels / term_size.width_cells;
                cell_height = term_size.height_pixels / term_size.height_cells;
        }

        // Convert image to a printable string
        printable = convert_image(pixels, pix_width, pix_height, pix_width * n_channels, CHAFA_PIXEL_BGRA8_UNASSOCIATED,
                                  width, height, cell_width, cell_height);
        g_string_append_c(printable, '\0');
        const gchar *delimiters = "\n";
        gchar **lines = g_strsplit(printable->str, delimiters, -1);

        int indentation = ((term_size.width_cells - width) / 2) + 1;

        for (int i = 0; lines[i] != NULL; i++)
        {
                printf("\n%*s%s", indentation, "", lines[i]);
        }

        g_strfreev(lines);
        g_string_free(printable, TRUE);
}

unsigned char luminance(unsigned char r, unsigned char g, unsigned char b)
{
        return (unsigned char)(0.2126 * r + 0.7152 * g + 0.0722 * b);
}

void checkIfBrightPixel(unsigned char r, unsigned char g, unsigned char b, bool *found)
{
        // Calc luminace and use to find Ascii char.
        unsigned char ch = luminance(r, g, b);

        if (ch > 80 && !(r < g + 20 && r > g - 20 && g < b + 20 && g > b - 20) && !(r > 150 && g > 150 && b > 150))
        {
                *found = true;
        }
}

int getCoverColor(FIBITMAP *bitmap, unsigned char *r, unsigned char *g, unsigned char *b)
{
        int rwidth = FreeImage_GetWidth(bitmap);
        int rheight = FreeImage_GetHeight(bitmap);
        int rchannels = FreeImage_GetBPP(bitmap) / 8;
        unsigned char *read_data = (unsigned char *)FreeImage_GetBits(bitmap);

        if (read_data == NULL)
        {
                return -1;
        }

        bool found = false;
        int numPixels = rwidth * rheight;

        for (int i = 0; i < numPixels; i++)
        {
                int index = i * rchannels;
                unsigned char blue = 0;
                unsigned char green = 0;
                unsigned char red = 0;

                if (rchannels >= 3)
                {
                        blue = read_data[index + 0];
                        green = read_data[index + 1];
                        red = read_data[index + 2];
                }
                else if (rchannels >= 1)
                {
                        blue = green = red = read_data[index];
                }

                checkIfBrightPixel(red, green, blue, &found);

                if (found)
                {
                        *(r) = red;
                        *(g) = green;
                        *(b) = blue;
                        break;
                }
        }

        return 0;
}
