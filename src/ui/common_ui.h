/**
 * @file common_ui.h
 * @brief Shared UI utilities and layout helpers.
 *
 * Contains reusable functions for drawing text, handling resizing,
 * and rendering shared UI components across multiple screens.
 */

#ifndef COMMON_UI_H
#define COMMON_UI_H

#include "common/model.h"

#include <stdbool.h>

/**
 * @brief Sets the RGB foreground color using the provided pixel data.
 *
 * This function sends an ANSI escape sequence to set the text color for the
 * terminal using the RGB values from the `PixelData` structure. The RGB values
 * are specified in the range [0-255] for each channel.
 *
 * @param p A `PixelData` structure containing the RGB values.
 */
void set_RGB(PixelData p);

/**
 * @brief Sets the album color for the UI.
 *
 * This function sets the foreground color based on the provided album color.
 * If the color is light (i.e., all channels are greater than or equal to 210),
 * it applies a default color from the application state. Otherwise, it sets the
 * color directly to the provided RGB value.
 *
 * @param color The `PixelData` structure representing the album color.
 */
void set_album_color(PixelData color);

/**
 * @brief Increments the update counter.
 *
 * This function increases the value of the global update counter. It is used
 * to track the number of updates or changes in the system.
 */
void increment_update_counter(void);

/**
 * @brief Inverts the text color.
 *
 * This function inverts the color of the text in the terminal using an ANSI
 * escape sequence. This can be useful for creating visually distinct effects
 * for text output.
 */
void inverse_text(void);

void apply_color(PixelData color);

/**
 * @brief Processes a name for scrolling within a specified width.
 *
 * This function processes a name string to fit within the specified maximum
 * width, ensuring that it scrolls correctly. It handles situations where
 * the name is too long, and scrolls it while ensuring that the name's
 * length and scroll behavior are properly managed.
 *
 * @param name The name string to be processed.
 * @param output The output buffer to store the processed name.
 * @param max_width The maximum width for the name.
 * @param is_same_name_as_last_time Boolean flag indicating if the name is the same as the previous one.
 * @param strip_unneeded_chars Flag indicating whether unneeded characters should be stripped.
 * @param strip_suffix Flag indicating whether the suffix (e.g., file extension) should be stripped.
 */
int process_name_scroll(const Model *model, const char *name, char *output, int max_width,
                        bool strip_unneeded_chars, bool strip_suffix);

/**
 * @brief Resets the terminal color.
 *
 * This function resets the terminal color back to the default color by sending
 * the ANSI escape code for resetting the text formatting.
 */
void reset_color(void);

/**
 * @brief Processes a name with various options for formatting.
 *
 * This function processes a name string, optionally stripping unwanted characters
 * and suffixes, and ensuring the name fits within the provided width. It also
 * allows for the stripping of track numbers and other formatting options.
 *
 * @param name The name string to be processed.
 * @param output The output buffer to store the processed name.
 * @param max_width The maximum width for the name.
 * @param strip_unneeded_chars Flag indicating whether unneeded characters should be stripped.
 * @param strip_suffix Flag indicating whether the suffix (e.g., file extension) should be stripped.
 * @return The length of the string.
 */
long process_name(const char *name, char *output, int max_width,
                  bool strip_unneeded_chars, bool strip_suffix);

/**
 * @brief Transfers application settings to the UI settings.
 *
 * This function takes the application settings and applies them to the UI settings.
 * This includes options like notifications, track number stripping, cover art
 * display, and more. It also updates various UI-related settings based on the
 * current application configuration.
 */
void transfer_settings_to_ui(void);

/**
 * @brief Enables mouse support for the UI.
 *
 * This function enables mouse support in the terminal for the provided UI settings.
 * If mouse support is enabled, the appropriate terminal escape sequences are used
 * to activate mouse input.
 *
 * @param ui A pointer to the `UISettings` structure to apply mouse settings.
 */
void enable_mouse(UISettings *ui);

/**
 * @brief Gets the current update counter.
 *
 * This function retrieves the current value of the global update counter.
 *
 * @return The current value of the update counter.
 */
int get_update_counter(void);

/**
 * @brief Determines if the current name is too long to fit in the available space.
 *
 * This function checks whether the current name is long enough to require
 * scrolling or if it fits within the allowed display width.
 *
 * @return A boolean value indicating whether the name is too long (true) or not (false).
 */
bool get_is_long_name(void);

/**
 * @brief Gets the mouse action corresponding to a mouse button or event.
 *
 * This function returns the appropriate `EventType` for a specific mouse action
 * based on the provided button number or event identifier.
 *
 * @param num The button or event number representing the mouse action.
 * @return The corresponding `EventType` value.
 */
enum MsgType get_mouse_action(int num);

/**
 * @brief Increases the luminosity of a pixel by a fixed amount.
 *
 * This function increases the RGB channels of a `PixelData` structure by a
 * specified amount, clamping each channel to the maximum value of 255.
 *
 * @param pixel The original `PixelData` to increase.
 * @param amount The amount to increase the luminosity by (0 to 255).
 * @return The modified `PixelData` with increased luminosity.
 */
PixelData increase_luminosity(PixelData pixel, int amount);

/**
 * @brief Increases the luminosity of a pixel based on its height and index.
 *
 * This function increases the luminosity of a pixel in a gradient fashion,
 * adjusting the color based on the height of the pixel and its index in the list.
 * It can also reverse the gradient effect if specified.
 *
 * @param pixel The original `PixelData` to adjust.
 * @param height The total height for the gradient.
 * @param idx The index of the pixel in the gradient.
 * @param reversed Boolean flag to indicate if the gradient should be reversed.
 * @return The adjusted `PixelData` with increased luminosity.
 */
PixelData increase_luminosity_for_height(PixelData pixel, int height, int idx, bool reversed);

/**
 * @brief Decreases the luminosity of a pixel by a given percentage.
 *
 * This function decreases the luminosity of a pixel by multiplying each RGB
 * channel by a specified percentage. It ensures that the channels are clamped
 * to a minimum value (50) to avoid completely dark pixels.
 *
 * @param base The original `PixelData` to decrease.
 * @param pct The percentage by which to decrease the luminosity (0.0 to 1.0).
 * @return The adjusted `PixelData` with decreased luminosity.
 */
PixelData decrease_luminosity_pct(PixelData base, float pct);

/**
 * @brief Gets a gradient color for a specific row in the list.
 *
 * This function calculates a gradient color based on the given base color,
 * row index, and the maximum list size. The function applies a smooth transition
 * to the color as the row number increases, adjusting the luminosity.
 *
 * @param base_color The base color to start the gradient from.
 * @param row The current row index.
 * @param max_list_size The maximum size of the list.
 * @param start_gradient The row number at which to start the gradient.
 * @param min_pct The minimum luminosity percentage for the gradient.
 * @return The calculated `PixelData` for the gradient color.
 */
PixelData get_gradient_color(PixelData base_color, int row, int max_list_size, int start_gradient, float min_pct);

/**
 * @brief Returns the current line of lyrics.
 *
 * @param lyrics A struct containing the lyrics.
 * @param elapsed_seconds How far we are into the song.
 * @return The current line of lyrics.
 */
const char *get_lyrics_line(const Lyrics *lyrics, double elapsed_seconds);

/**
 * @brief Returns the next UTF-8 codepoint
 *
 * @param s A string.
 * @param bytes_consumed How many bytes were used.
 * @return The next codepoint.
 */
uint32_t utf8_next(const char *s, int *bytes_consumed);

/**
 * @brief Returns the display width of a Unicode code point in columns.
 *
 * @param cp The Unicode code point.
 * @return The number of display columns required (e.g., 1 or 2),
 *         or 0 for non-printing characters.
 */
int codepoint_display_width(uint32_t cp);

// Write a UTF-8 string into the buffer at (row, col), stopping at
// max_width display columns. Remaining columns up to max_width are
// filled with spaces. Clips if row/col is outside the buffer.
void draw_buffer_set_string_truncated(DrawBuffer *buf,
                                      int row, int col,
                                      const char *str,
                                      int max_width,
                                      CellStyle style);

/**
 * @brief Writes a UTF-8 string into the buffer, stopping at buf->cols.
 *
 * @param buf The draw buffer.
 * @param row The target row index.
 * @param col The target column index.
 * @param str The UTF-8 string to write.
 * @param style The cell style to apply.
 */
void draw_buffer_set_string(DrawBuffer *buf, int row, int col,
                            const char *str, CellStyle style);

/**
 * @brief Sets a single cell in the buffer to a specific code point and style.
 *
 * Clips if row/col is outside the buffer.
 *
 * @param buf The draw buffer.
 * @param row The target row index.
 * @param col The target column index.
 * @param cp The Unicode code point to display.
 * @param style The cell style to apply.
 */
void draw_buffer_set_cell(DrawBuffer *buf,
                          int row,
                          int col,
                          uint32_t cp,
                          CellStyle style);


/**
 * @brief Returns the default cell style.
 *
 * @return The cell style
 */
CellStyle cell_style_plain(void);

/**
 * @brief Returns a cell style based on a color.
 *
 * @param color
 * @return The cell style
 */
CellStyle cell_style_fg(PixelData color);

/**
 * @brief Returns a cell style.
 *
 * @param theme The theme color settings.
 * @return The cell style
 */
CellStyle cell_style_from_theme(ColorValue theme);

/**
 * @brief Returns the footer text.

 * @param text The footer text that we get.
 * @param size The max size of the text.
 * @return An int indicating whether the footer was copied to text successfully
 */
int get_footer_text(char *restrict text, size_t size);

/**
 * @brief Returns the display width of a UTF-8 string.

 * @param s the string
 * @return An int indicating the length in display columns
 */
int utf8_display_width(const char *s);

void draw_link_to_buffer(DrawBuffer *buf, int row, int col, int width,
                        const char *url, char *title, CellStyle style);

void free_link_payload(LinkPayload *link);

int mk_wcwidth(uint32_t ucs);

void view_changed(Model *model);

void switch_view(ViewState view_to_show);

void render_scroll_bar(DrawBuffer *buf, k_Rect region, k_ScrollBar scrollbar, CellStyle style);

bool scrollbar_at_position(int mouse_x, int mouse_y, bool dragging);

void scrollbar_scroll(int mouse_y, bool dragging);

void scrollbar_reset(k_ScrollBar scrollbar);

void register_click(int mouse_x, int mouse_y, int mouse_key);

#endif
