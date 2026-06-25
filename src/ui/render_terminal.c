#include "render_terminal.h"

#include "common/model.h"

#include "ui/chroma.h"
#include "utils/img_utils.h"
#include "utils/term.h"

#include <stdio.h>

typedef struct {
        PixelData fg;
        PixelData bg;

        uint32_t attrs;

        bool ansi_mode;
        int ansi_fg;

        bool initialized;
} RenderState;

static void cursor_move(int row, int col)
{
        // Terminal escape sequences are 1-indexed
        printf("\033[%d;%dH", row + 1, col + 1);
}

void emit_image(ImagePayload *image,
                int row,
                int col,
                bool centered,
                int term_width_cells,
                int corrected_width)
{
    if (!image || !image->data)
        return;

    if (centered)
        col = ((term_width_cells - corrected_width) / 2) + 1;

    const char *src = (const char *)image->data;

    // Allocate generously:
    // original image size +
    // up to ~32 chars of cursor sequence per line.
    //
    size_t src_len = strlen(src);

    size_t line_count = 1;
    for (const char *p = src; *p; ++p)
        if (*p == '\n')
            line_count++;

    size_t out_cap = src_len + (line_count * 32) + 1;
    char *out = malloc(out_cap);

    if (!out)
        return;

    char *dst = out;
    int current_row = row + 1;

    const char *p = src;

    // Build the entire string, with indentations
    while (*p) {
        const char *line_start = p;

        while (*p && *p != '\n')
            p++;

        size_t line_len = (size_t)(p - line_start);

        int n = sprintf(dst,
                        "\033[%d;%dH",
                        current_row++,
                        col + 1);

        dst += n;

        memcpy(dst, line_start, line_len);
        dst += line_len;

        if (*p == '\n')
            p++;
    }

    // Write it all at once
    fwrite(out, 1, (size_t)(dst - out), stdout);
    fflush(stdout);

    free(out);
}

static inline void safe_putchar(int c)
{
#ifdef _WIN32
        putchar(c);
#else
        putchar_unlocked(c);
#endif
}

static inline void emit_codepoint(uint32_t cp)
{
        if (cp < 0x80) {
                safe_putchar((int)cp);
                return;
        }

        char buf[4];
        int n = 0;

        if (cp < 0x800) {
                buf[n++] = 0xC0 | (cp >> 6);
                buf[n++] = 0x80 | (cp & 0x3F);
        } else if (cp < 0x10000) {
                buf[n++] = 0xE0 | (cp >> 12);
                buf[n++] = 0x80 | ((cp >> 6) & 0x3F);
                buf[n++] = 0x80 | (cp & 0x3F);
        } else {
                buf[n++] = 0xF0 | (cp >> 18);
                buf[n++] = 0x80 | ((cp >> 12) & 0x3F);
                buf[n++] = 0x80 | ((cp >> 6) & 0x3F);
                buf[n++] = 0x80 | (cp & 0x3F);
        }

        fwrite(buf, 1, n, stdout);
}

static bool pixel_equal(PixelData a, PixelData b)
{
        return a.r == b.r &&
               a.g == b.g &&
               a.b == b.b &&
               a.a == b.a;
}

static void emit_style_diff(const Cell *cell,
                            RenderState *st)
{
        // Initialize terminal state.
        if (!st->initialized) {

                printf("\033[0m");

                st->fg = (PixelData){0};
                st->bg = (PixelData){0};

                st->attrs = 0;

                st->ansi_mode = false;
                st->ansi_fg = -1;

                st->initialized = true;
        }

        // If attrs changed -> reset and rebuild.
        if (cell->attrs != st->attrs) {

                printf("\033[0m");

                st->attrs = 0;

                st->ansi_mode = false;
                st->ansi_fg = -1;

                st->fg = (PixelData){0};
                st->bg = (PixelData){0};

                if (cell->attrs & ATTR_BOLD)
                        printf("\033[1m");

                if (cell->attrs & ATTR_DIM)
                        printf("\033[2m");

                if (cell->attrs & ATTR_ITALIC)
                        printf("\033[3m");

                if (cell->attrs & ATTR_UNDERLINE)
                        printf("\033[4m");

                if (cell->attrs & ATTR_BLINK)
                        printf("\033[5m");

                if (cell->attrs & ATTR_REVERSE)
                        printf("\033[7m");

                st->attrs = cell->attrs;
        }

        // ANSI color mode
        if (cell->style.isAnsi) {

                if (!st->ansi_mode ||
                    st->ansi_fg != cell->style.fgAnsi) {

                        int fg_code;
                        if (cell->style.fgAnsi < 8) {
                                fg_code = 30 + cell->style.fgAnsi;
                        } else {
                                fg_code = 90 + (cell->style.fgAnsi - 8);
                        }
                        printf("\033[%dm", fg_code);

                        st->ansi_mode = true;
                        st->ansi_fg = cell->style.fgAnsi;
                }

                return;
        }

        // Leaving ANSI mode
        if (st->ansi_mode) {
                printf("\033[39m");
                st->ansi_mode = false;
                st->ansi_fg = -1;
        }

        // RGB foreground
        if (!pixel_equal(cell->style.fg, st->fg)) {

                if (COLOR_IS_DEFAULT(cell->style.fg)) {

                        printf("\033[39m");

                } else {

                        printf("\033[38;2;%u;%u;%um",
                               cell->style.fg.r,
                               cell->style.fg.g,
                               cell->style.fg.b);
                }

                st->fg = cell->style.fg;
        }

        // RGB background
        if (!pixel_equal(cell->style.bg, st->bg)) {

                if (COLOR_IS_DEFAULT(cell->style.bg)) {

                        printf("\033[49m");

                } else {

                        printf("\033[48;2;%u;%u;%um",
                               cell->style.bg.r,
                               cell->style.bg.g,
                               cell->style.bg.b);
                }

                st->bg = cell->style.bg;
        }
}

void terminal_backend_commit(const DrawBuffer *buf,
                             DirtyFlags dirty,
                             TerminalBackendState *state)
{
        if (dirty == DIRTY_ALL)
                clear_screen();

        RenderState style = {0};

        int cur_row = -1;
        int cur_col = -1;

        for (int row = 0; row < buf->rows; row++) {

                // Skip fully clean rows.
                if (dirty != DIRTY_ALL &&
                    buf->dirty_rows &&
                    !buf->dirty_rows[row]) {
                        continue;
                }

                int col = 0;

                while (col < buf->cols) {

                        Cell *cell = &buf->cells[row * buf->cols + col];

                        if (state->render_chroma) {

                                if (cell->kind == CELL_IMAGE_ANCHOR) {

                                        chroma_print_frame(row + 1, col + 1, cell->image->screen_h, false);

                                        // Mark rows as dirtys
                                        for (int i = 0; i < cell->image->screen_h; i++) {
                                                buf->dirty_rows[row + i] = true;
                                        }

                                        cursor_move(row, col);

                                        cur_row = row;
                                        cur_col = col;

                                        col++;
                                        continue;
                                }

                        } else {

                                // Image anchor
                                if (cell->kind == CELL_IMAGE_ANCHOR) {

                                        bool same_image =
                                            (cell->image->id ==
                                             state->last_image_id);

                                        bool same_position =
                                            (row == state->last_row &&
                                             col == state->last_col);

                                        if (!same_image || !same_position || (dirty & DIRTY_SONG)) {

                                                cursor_move(row, col);

                                                cur_row = row;
                                                cur_col = col;

                                                emit_image(cell->image, row, col, false, buf->rows, cell->image->screen_w);

                                                state->last_image_id = cell->image->id;

                                                state->last_row = row;
                                                state->last_col = col;

                                                // Terminal image protocols
                                                // often disturb cursor position.
                                                cursor_move(row, col);

                                                cur_row = row;
                                                cur_col = col;
                                        }

                                        col++;
                                        continue;
                                }
                        }

                        if (cell->kind == CELL_LINK)
                        {
                                cursor_move(row, col);

                                emit_style_diff(cell, &style);

                                printf("%s", cell->link->title);

                                cur_row = row;
                                cur_col = col;
                                col = buf->cols;
                                continue;
                        }

                        // Image occupied, skip cells
                        if (cell->kind == CELL_OCCUPIED) {

                                while (col < buf->cols) {

                                        Cell *next = &buf->cells[row * buf->cols + col];

                                        if (next->kind !=
                                            CELL_OCCUPIED) {
                                                break;
                                        }

                                        col++;
                                }

                                // Single cursor move after skipping.
                                if (cur_row != row ||
                                    cur_col != col) {

                                        cursor_move(row, col);

                                        cur_row = row;
                                        cur_col = col;
                                }

                                continue;
                        }

                        // Move only if cursor drifted.
                        if (cur_row != row ||
                            cur_col != col) {

                                cursor_move(row, col);

                                cur_row = row;
                                cur_col = col;
                        }

                        // Emit only style differences.
                        emit_style_diff(cell, &style);

                        // Emit glyph.
                        emit_codepoint(cell->codepoint);

                        cur_col++;

                        col++;
                }

                cur_row++;
        }

        fflush(stdout);
}
