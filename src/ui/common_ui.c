#define _XOPEN_SOURCE 700

/**
* @file common_ui.c
* @brief Shared UI utilities and layout helpers.
*
* Contains reusable functions for drawing text, handling resizing,
* and rendering shared UI components across multiple screens.
*/

#include "common_ui.h"

#include "common/appstate.h"
#include "common/events.h"

#include "data/theme.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned int update_counter = 0;

void set_RGB(PixelData p)
{
        // ANSI escape code for setting RGB foreground
        printf("\033[38;2;%d;%d;%dm", p.r, p.g, p.b);
}

void set_album_color(PixelData color)
{
        if (color.r >= 210 && color.g >= 210 && color.b >= 210) {
                AppState *state = get_app_state();
                set_RGB(state->settings.defaultColorRGB);
        } else {
                set_RGB(color);
        }
}

enum MsgType get_mouse_action(int num)
{
        enum MsgType value = MSG_NONE;

        switch (num) {
        case 0:
                value = MSG_NONE;
                break;
        case 1:
                value = MSG_ENQUEUE;
                break;
        case 2:
                value = MSG_PLAY_PAUSE;
                break;
        case 3:
                value = MSG_SCROLLUP;
                break;
        case 4:
                value = MSG_SCROLLDOWN;
                break;
        case 5:
                value = MSG_SEEKFORWARD;
                break;
        case 6:
                value = MSG_SEEKBACK;
                break;
        case 7:
                value = MSG_VOLUME_UP;
                break;
        case 8:
                value = MSG_VOLUME_DOWN;
                break;
        case 9:
                value = MSG_NEXTVIEW;
                break;
        case 10:
                value = MSG_PREVVIEW;
                break;
        default:
                value = MSG_NONE;
                break;
        }

        return value;
}

int get_bytes_in_first_char(const char *str)
{
        if (str == NULL || str[0] == '\0') {
                return 0;
        }

        mbstate_t state;
        memset(&state, 0, sizeof(state));
        wchar_t wc;
        int num_bytes = mbrtowc(&wc, str, MB_CUR_MAX, &state);

        return num_bytes;
}

void enable_mouse(UISettings *ui)
{
        if (ui->mouseEnabled)
                enable_terminal_mouse_buttons();
}

void transfer_settings_to_ui(void)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();
        UISettings *ui = &(state->settings);

        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->stripTrackNumbers = (settings->stripTrackNumbers[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        snprintf(ui->coverStyle, sizeof(ui->coverStyle), "%s",
                 settings->coverStyle[0] ? settings->coverStyle : "auto");
        ui->hideHelp = (settings->hideHelp[0] == '1');
        ui->visualizerEnabled = (settings->visualizerEnabled[0] == '1');
        ui->hideTimeStatus = (settings->hideTimeStatus[0] == '1');
        ui->discordRPCEnabled = (settings->discordRPCEnabled[0] == '1');
        ui->quitAfterStopping = (settings->quitAfterStopping[0] == '1');
        ui->clearListClearsAll = (settings->clearListClearsAll[0] == '1');
        ui->hideGlimmeringText = (settings->hideGlimmeringText[0] == '1');
        ui->mouseEnabled = (settings->mouseEnabled[0] == '1');
        ui->shuffle_enabled = (settings->shuffle_enabled[0] == '1');
        ui->visualizerBrailleMode = (settings->visualizerBrailleMode[0] == '1');
        ui->hideLogo = (settings->hideLogo[0] == '1');
        ui->hideFooter = (settings->hideFooter[0] == '1');
        ui->hideSideCover = (settings->hideSideCover[0] == '1');
        ui->collapseTopLevel = (settings->collapseTopLevel[0] == '1');
        ui->saveRepeatShuffleSettings = (settings->saveRepeatShuffleSettings[0] == '1');
        ui->trackTitleAsWindowTitle = (settings->trackTitleAsWindowTitle[0] == '1');
        ui->auto_resume = (settings->auto_resume[0] == '1');
        ui->showFoldersInPlaylist = (settings->showFoldersInPlaylist[0] == '1');
        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        snprintf(ui->coverStyle, sizeof(ui->coverStyle), "%s",
                 settings->coverStyle[0] ? settings->coverStyle : "auto");
        ui->shuffle_enabled = (settings->shuffle_enabled[0] == '1');

        snprintf(ui->chromaPath, sizeof(ui->chromaPath), "%s", settings->chromaPath);
        snprintf(ui->chromaDevice, sizeof(ui->chromaDevice), "%s", settings->chromaDevice);

        int tmp_num_bytes =
            get_bytes_in_first_char(settings->progressBarElapsedEvenChar);
        if (tmp_num_bytes != 0)
                settings->progressBarElapsedEvenChar[tmp_num_bytes] = '\0';

        tmp_num_bytes = get_bytes_in_first_char(settings->progressBarElapsedOddChar);
        if (tmp_num_bytes != 0)
                settings->progressBarElapsedOddChar[tmp_num_bytes] = '\0';

        tmp_num_bytes =
            get_bytes_in_first_char(settings->progressBarApproachingEvenChar);
        if (tmp_num_bytes != 0)
                settings->progressBarApproachingEvenChar[tmp_num_bytes] = '\0';

        tmp_num_bytes =
            get_bytes_in_first_char(settings->progressBarApproachingOddChar);
        if (tmp_num_bytes != 0)
                settings->progressBarApproachingOddChar[tmp_num_bytes] = '\0';

        tmp_num_bytes = get_bytes_in_first_char(settings->progressBarCurrentEvenChar);
        if (tmp_num_bytes != 0)
                settings->progressBarCurrentEvenChar[tmp_num_bytes] = '\0';

        tmp_num_bytes = get_bytes_in_first_char(settings->progressBarCurrentOddChar);
        if (tmp_num_bytes != 0)
                settings->progressBarCurrentOddChar[tmp_num_bytes] = '\0';

        int tmp = get_number(settings->repeatState);
        if (tmp >= 0)
                ui->repeatState = tmp;

        tmp = get_number(settings->colorMode);
        if (tmp >= 0 && tmp < 3) {
                ui->colorMode = tmp;
        }

        tmp = get_number(settings->replayGainCheckFirst);
        if (tmp >= 0)
                ui->replayGainCheckFirst = tmp;

        tmp = get_number(settings->mouseLeftClickAction);

        enum MsgType tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseLeftClickAction = tmp_event;

        tmp = get_number(settings->mouseMiddleClickAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseMiddleClickAction = tmp_event;

        tmp = get_number(settings->mouseRightClickAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseRightClickAction = tmp_event;

        tmp = get_number(settings->mouseScrollUpAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseScrollUpAction = tmp_event;

        tmp = get_number(settings->mouseScrollDownAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseScrollDownAction = tmp_event;

        tmp = get_number(settings->mouseAltScrollUpAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseAltScrollUpAction = tmp_event;

        tmp = get_number(settings->mouseAltScrollDownAction);
        tmp_event = get_mouse_action(tmp);
        if (tmp >= 0)
                ui->mouseAltScrollDownAction = tmp_event;

        tmp = get_number(settings->visualizer_height);
        if (tmp > 0)
                ui->visualizer_height = tmp;

        tmp = get_number(settings->visualizer_bar_width);
        if (tmp >= 0)
                ui->visualizer_bar_mode = tmp;

        tmp = get_number(settings->visualizer_color_type);
        if (tmp >= 0)
                ui->visualizer_color_type = tmp;

        tmp = get_number(settings->titleDelay);
        if (tmp >= 0)
                ui->titleDelay = tmp;

        snprintf(ui->theme_name, sizeof(ui->theme_name), "%s", settings->theme);

        if (!(ui->colorMode >= 0 && ui->colorMode < 3)) {
                bool useConfigColors = (settings->useConfigColors[0] == '1');

                if (useConfigColors)
                        ui->colorMode = COLOR_MODE_DEFAULT;
                else
                        ui->colorMode = COLOR_MODE_ALBUM;
        }
}

int get_update_counter()
{
        return update_counter;
}

void increment_update_counter()
{
        update_counter++;
}

void reset_color(void)
{
        printf("\033[0m");
}

void inverse_text(void)
{
        printf("\x1b[7m");
}

void apply_color(ColorMode mode, ColorValue theme_color, PixelData album_color)
{
        reset_color();

        switch (mode) {
        case COLOR_MODE_ALBUM:
                set_album_color(album_color);
                break;

        case COLOR_MODE_THEME:
        case COLOR_MODE_DEFAULT:
                if (theme_color.type == COLOR_TYPE_RGB) {
                        set_RGB(theme_color.rgb);
                } else {
                        set_terminal_color(theme_color.ansiIndex);
                }
                break;
        }
}

/*
 * Markus Kuhn -- 2007-05-26 (Unicode 5.0)
 *
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version: http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct interval {
        wchar_t first;
        wchar_t last;
};

// Auxiliary function for binary search in interval table
static int bisearch(wchar_t ucs, const struct interval *table, int max)
{
        if (table == NULL || max < 0)
                return 0;

        // Range validation to avoid unsafe casts
        if (table[0].first > ucs || table[max].last < ucs)
                return 0;

        size_t min = 0;
        size_t maxIndex = (size_t)max;

        while (min <= maxIndex) {
                size_t mid = min + ((maxIndex - min) >> 1);

                if (ucs > table[mid].last) {
                        min = mid + 1;
                } else if (ucs < table[mid].first) {
                        if (mid == 0)
                                return 0;
                        maxIndex = mid - 1;
                } else {
                        return 1;
                }
        }

        return 0;
}

int mk_wcwidth(wchar_t ucs)
{
        /* sorted list of non-overlapping intervals of non-spacing characters */
        /* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B
         * c" */
        static const struct interval combining[] = {
            {0x0300, 0x036F}, {0x0483, 0x0486}, {0x0488, 0x0489}, {0x0591, 0x05BD}, {0x05BF, 0x05BF}, {0x05C1, 0x05C2}, {0x05C4, 0x05C5}, {0x05C7, 0x05C7}, {0x0600, 0x0603}, {0x0610, 0x0615}, {0x064B, 0x065E}, {0x0670, 0x0670}, {0x06D6, 0x06E4}, {0x06E7, 0x06E8}, {0x06EA, 0x06ED}, {0x070F, 0x070F}, {0x0711, 0x0711}, {0x0730, 0x074A}, {0x07A6, 0x07B0}, {0x07EB, 0x07F3}, {0x0901, 0x0902}, {0x093C, 0x093C}, {0x0941, 0x0948}, {0x094D, 0x094D}, {0x0951, 0x0954}, {0x0962, 0x0963}, {0x0981, 0x0981}, {0x09BC, 0x09BC}, {0x09C1, 0x09C4}, {0x09CD, 0x09CD}, {0x09E2, 0x09E3}, {0x0A01, 0x0A02}, {0x0A3C, 0x0A3C}, {0x0A41, 0x0A42}, {0x0A47, 0x0A48}, {0x0A4B, 0x0A4D}, {0x0A70, 0x0A71}, {0x0A81, 0x0A82}, {0x0ABC, 0x0ABC}, {0x0AC1, 0x0AC5}, {0x0AC7, 0x0AC8}, {0x0ACD, 0x0ACD}, {0x0AE2, 0x0AE3}, {0x0B01, 0x0B01}, {0x0B3C, 0x0B3C}, {0x0B3F, 0x0B3F}, {0x0B41, 0x0B43}, {0x0B4D, 0x0B4D}, {0x0B56, 0x0B56}, {0x0B82, 0x0B82}, {0x0BC0, 0x0BC0}, {0x0BCD, 0x0BCD}, {0x0C3E, 0x0C40}, {0x0C46, 0x0C48}, {0x0C4A, 0x0C4D}, {0x0C55, 0x0C56}, {0x0CBC, 0x0CBC}, {0x0CBF, 0x0CBF}, {0x0CC6, 0x0CC6}, {0x0CCC, 0x0CCD}, {0x0CE2, 0x0CE3}, {0x0D41, 0x0D43}, {0x0D4D, 0x0D4D}, {0x0DCA, 0x0DCA}, {0x0DD2, 0x0DD4}, {0x0DD6, 0x0DD6}, {0x0E31, 0x0E31}, {0x0E34, 0x0E3A}, {0x0E47, 0x0E4E}, {0x0EB1, 0x0EB1}, {0x0EB4, 0x0EB9}, {0x0EBB, 0x0EBC}, {0x0EC8, 0x0ECD}, {0x0F18, 0x0F19}, {0x0F35, 0x0F35}, {0x0F37, 0x0F37}, {0x0F39, 0x0F39}, {0x0F71, 0x0F7E}, {0x0F80, 0x0F84}, {0x0F86, 0x0F87}, {0x0F90, 0x0F97}, {0x0F99, 0x0FBC}, {0x0FC6, 0x0FC6}, {0x102D, 0x1030}, {0x1032, 0x1032}, {0x1036, 0x1037}, {0x1039, 0x1039}, {0x1058, 0x1059}, {0x1160, 0x11FF}, {0x135F, 0x135F}, {0x1712, 0x1714}, {0x1732, 0x1734}, {0x1752, 0x1753}, {0x1772, 0x1773}, {0x17B4, 0x17B5}, {0x17B7, 0x17BD}, {0x17C6, 0x17C6}, {0x17C9, 0x17D3}, {0x17DD, 0x17DD}, {0x180B, 0x180D}, {0x18A9, 0x18A9}, {0x1920, 0x1922}, {0x1927, 0x1928}, {0x1932, 0x1932}, {0x1939, 0x193B}, {0x1A17, 0x1A18}, {0x1B00, 0x1B03}, {0x1B34, 0x1B34}, {0x1B36, 0x1B3A}, {0x1B3C, 0x1B3C}, {0x1B42, 0x1B42}, {0x1B6B, 0x1B73}, {0x1DC0, 0x1DCA}, {0x1DFE, 0x1DFF}, {0x200B, 0x200F}, {0x202A, 0x202E}, {0x2060, 0x2063}, {0x206A, 0x206F}, {0x20D0, 0x20EF}, {0x302A, 0x302F}, {0x3099, 0x309A}, {0xA806, 0xA806}, {0xA80B, 0xA80B}, {0xA825, 0xA826}, {0xFB1E, 0xFB1E}, {0xFE00, 0xFE0F}, {0xFE20, 0xFE23}, {0xFEFF, 0xFEFF}, {0xFFF9, 0xFFFB}, {0x10A01, 0x10A03}, {0x10A05, 0x10A06}, {0x10A0C, 0x10A0F}, {0x10A38, 0x10A3A}, {0x10A3F, 0x10A3F}, {0x1D167, 0x1D169}, {0x1D173, 0x1D182}, {0x1D185, 0x1D18B}, {0x1D1AA, 0x1D1AD}, {0x1D242, 0x1D244}, {0xE0001, 0xE0001}, {0xE0020, 0xE007F}, {0xE0100, 0xE01EF}};

        /* test for 8-bit control characters */
        if (ucs == 0)
                return 0;
        if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
                return -1;

        /* binary search in table of non-spacing characters */
        if (bisearch(ucs, combining,
                     sizeof(combining) / sizeof(struct interval) - 1))
                return 0;

        /* if we arrive here, ucs is not a combining or C0/C1 control character
         */

        return 1 + (ucs >= 0x1100 &&
                    (ucs <= 0x115f || /* Hangul Jamo init. consonants */
                     ucs == 0x2329 || ucs == 0x232a ||
                     (ucs >= 0x2e80 && ucs <= 0xa4cf &&
                      ucs != 0x303f) ||                  /* CJK ... Yi */
                     (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
                     (ucs >= 0xf900 &&
                      ucs <= 0xfaff) ||                  /* CJK Compatibility Ideographs */
                     (ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
                     (ucs >= 0xfe30 &&
                      ucs <= 0xfe6f) ||                  /* CJK Compatibility Forms */
                     (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
                     (ucs >= 0xffe0 && ucs <= 0xffe6) ||
                     (ucs >= 0x20000 && ucs <= 0x2fffd) ||
                     (ucs >= 0x30000 && ucs <= 0x3fffd)));
}

int mk_wcswidth(const wchar_t *pwcs, size_t n)
{
        int width = 0;

        for (; *pwcs && n-- > 0; pwcs++) {
                int w;
                if ((w = mk_wcwidth(*pwcs)) < 0)
                        return -1;
                else
                        width += w;
        }
        return width;
}

/* End Markus Kuhn code */

void process_name_strip_suffix(const char *name, char *output)
{
        const gchar *last_dot = g_utf8_strrchr(name, -1, '.');

        if (last_dot != NULL) {
                gchar *tmp = g_utf8_substring(name, 0, g_utf8_pointer_to_offset(name, last_dot));
                g_utf8_strncpy(output, tmp, g_utf8_strlen(tmp, -1));
                g_free(tmp);
        }
}

void process_name_strip_unneeded_chars(char *str)
{
        if (get_app_state()->settings.stripTrackNumbers)
                format_filename(str);

        g_strstrip(str);
}

void process_name(const char *name, char *output, int max_width,
                  bool strip_unneeded_chars, bool strip_suffix)
{
        if (!name) {
                output[0] = '\0';
                return;
        }

        if (strip_suffix)
                process_name_strip_suffix(name, output);
        else
                g_utf8_strncpy(output, name, g_utf8_strlen(name, -1));

        if (strip_unneeded_chars)
                process_name_strip_unneeded_chars(output);

        g_strstrip(output);

        str_truncate_display_width(output, output, max_width);
}

int process_name_scroll(const Model *model, const char *name, char *output, int max_width,
                        bool strip_unneeded_chars, bool strip_suffix)
{
        if (strip_suffix)
                process_name_strip_suffix(name, output);
        else
                g_utf8_strncpy(output, name, g_utf8_strlen(name, -1));

        if (strip_unneeded_chars)
                process_name_strip_unneeded_chars(output);

        int name_width = str_calculate_display_width(output);

        if (name_width < max_width - 1) {
                process_name(name, output, max_width, true, true);
        } else if (model->name_scroll.frame >= 0 && model->name_scroll.frame + max_width <= name_width) {
                gchar *tmp = g_utf8_substring(output, model->name_scroll.frame, -1);
                str_truncate_display_width(tmp, output, max_width);
                g_free(tmp);
        }

        return name_width;
}

PixelData increase_luminosity_for_height(PixelData pixel, int height, int idx, bool reversed)
{
        PixelData pixel2 = {0};

        // Calculate how much each color can be lightened, based on how far the color is from white (255)
        float lightening_factor_r = (255 - pixel.r) / 255.0f;
        float lightening_factor_g = (255 - pixel.g) / 255.0f;
        float lightening_factor_b = (255 - pixel.b) / 255.0f;

        // Use a "luminosity factor" that scales with height but prevents going too quickly to white
        float factor_r = lightening_factor_r * 0.55f; // Adjust this factor to control how much to lighten
        float factor_g = lightening_factor_g * 0.55f;
        float factor_b = lightening_factor_b * 0.55f;

        // Adjust the amount of lightening by using the height. Larger heights should light up more gradually.
        // We scale the factor based on `height` but ensure it doesn't get too small.
        float increment_r = factor_r * (255.0f / height);
        float increment_g = factor_g * (255.0f / height);
        float increment_b = factor_b * (255.0f / height);

        // Calculate the new colors based on whether we're lightening or darkening
        float current_r, current_g, current_b;

        if (!reversed) { // Lighten mode (increase luminosity)
                current_r = pixel.r + (increment_r * idx);
                current_g = pixel.g + (increment_g * idx);
                current_b = pixel.b + (increment_b * idx);
        } else { // Reversed mode (lighten from bottom to top)
                current_r = pixel.r + (increment_r * (height - idx));
                current_g = pixel.g + (increment_g * (height - idx));
                current_b = pixel.b + (increment_b * (height - idx));
        }

        // Clamp values to the valid range [0, 255]
        pixel2.r = (current_r > 255) ? 255 : (current_r < 0) ? 0
                                                             : current_r;
        pixel2.g = (current_g > 255) ? 255 : (current_g < 0) ? 0
                                                             : current_g;
        pixel2.b = (current_b > 255) ? 255 : (current_b < 0) ? 0
                                                             : current_b;

        pixel2.a = 255;

        return pixel2;
}

PixelData increase_luminosity(PixelData pixel, int amount)
{
        PixelData pixel2 = {0};
        pixel2.r = pixel.r + amount <= 255 ? pixel.r + amount : 255;
        pixel2.g = pixel.g + amount <= 255 ? pixel.g + amount : 255;
        pixel2.b = pixel.b + amount <= 255 ? pixel.b + amount : 255;
        pixel2.a = 255;

        return pixel2;
}

#define MIN_CHANNEL 50

PixelData decrease_luminosity_pct(PixelData base, float pct)
{
        PixelData c = {0};

        int r = (int)((float)base.r * pct);
        int g = (int)((float)base.g * pct);
        int b = (int)((float)base.b * pct);

        c.r = (r < MIN_CHANNEL) ? MIN_CHANNEL : r;
        c.g = (g < MIN_CHANNEL) ? MIN_CHANNEL : g;
        c.b = (b < MIN_CHANNEL) ? MIN_CHANNEL : b;

        c.a = 255;

        return c;
}

PixelData get_gradient_color(PixelData base_color, int row, int max_list_size,
                             int start_gradient, float min_pct)
{
        if (row < start_gradient)
                return base_color;

        int steps = max_list_size - start_gradient;

        float pct;

        if (steps <= 1)
                pct = min_pct;
        else
                pct = 1.0f -
                      ((row - start_gradient) * (1.0f - min_pct) / (steps - 1));

        if (pct < min_pct)
                pct = min_pct;
        if (pct > 1.0f)
                pct = 1.0f;

        return decrease_luminosity_pct(base_color, pct);
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

// Advance one UTF-8 codepoint, return it. *bytes_consumed is set to the
// number of bytes eaten. Returns 0xFFFD on invalid sequences.
uint32_t utf8_next(const char *s, int *bytes_consumed)
{
        unsigned char c = (unsigned char)*s;

        if (c == 0) {
                *bytes_consumed = 0;
                return 0;
        }
        if (c < 0x80) {
                *bytes_consumed = 1;
                return c;
        }
        if (c < 0xC2) {
                *bytes_consumed = 1;
                return 0xFFFD;
        } // continuation or overlong

        int len;
        uint32_t cp;

        if (c < 0xE0) {
                len = 2;
                cp = c & 0x1F;
        } else if (c < 0xF0) {
                len = 3;
                cp = c & 0x0F;
        } else if (c < 0xF8) {
                len = 4;
                cp = c & 0x07;
        } else {
                *bytes_consumed = 1;
                return 0xFFFD;
        }

        for (int i = 1; i < len; i++) {
                unsigned char b = (unsigned char)s[i];
                if ((b & 0xC0) != 0x80) {
                        *bytes_consumed = i;
                        return 0xFFFD;
                }
                cp = (cp << 6) | (b & 0x3F);
        }

        *bytes_consumed = len;
        return cp;
}

int codepoint_display_width(uint32_t cp)
{
        int w = wcwidth((wchar_t)cp);
        return (w > 0) ? w : 1; // treat non-printing as width 1
}

void draw_buffer_set_cell(DrawBuffer *buf,
                          int row,
                          int col,
                          uint32_t cp,
                          CellStyle style)
{
        if (!buf)
                return;

        if (row < 0 || row >= buf->rows)
                return;

        if (col < 0 || col >= buf->cols)
                return;

        int w = codepoint_display_width(cp);

        if (col + w > buf->cols)
                return;

        Cell *anchor = &buf->cells[row * buf->cols + col];

        anchor->codepoint = cp;
        anchor->style.fg = style.fg;
        anchor->style.bg = style.bg;
        anchor->style.fgAnsi = style.fgAnsi;
        anchor->style.bgAnsi = style.bgAnsi;
        anchor->style.isAnsi = style.isAnsi;
        anchor->attrs = style.attrs;
        anchor->kind = CELL_NORMAL;

        if (w == 2) {
                Cell *cont =
                    &buf->cells[row * buf->cols + col + 1];

                cont->codepoint = 0;
                cont->style = anchor->style;
                cont->attrs = style.attrs;
                cont->kind = CELL_WIDE_CONT;
        }
}

// Write a UTF-8 string into the buffer at (row, col), stopping at
// max_width display columns. Remaining columns up to max_width are
// filled with spaces. Clips if row/col is outside the buffer.
void draw_buffer_set_string_truncated(
    DrawBuffer *restrict buf,
    int row,
    int col,
    const char *restrict str,
    int max_width,
    CellStyle style)
{
        if ((unsigned)row >= (unsigned)buf->rows ||
            (unsigned)col >= (unsigned)buf->cols)
                return;

        if (buf->dirty_rows)
                buf->dirty_rows[row] = true;

        int col_end = col + max_width;
        if (col_end > buf->cols)
                col_end = buf->cols;

        Cell *cell = &buf->cells[row * buf->cols + col];
        Cell *end = &buf->cells[row * buf->cols + col_end];

        const char *p = str;

        while (cell < end && *p) {
                int bytes;
                uint32_t cp = utf8_next(p, &bytes);

                if (bytes == 0)
                        break;

                p += bytes;

                int w = codepoint_display_width(cp);

                if (cell + w > end)
                        break;

                cell->codepoint = cp;
                cell->style = style;
                cell->attrs = style.attrs;
                cell->kind = CELL_NORMAL;

                if (w == 2) {
                        Cell *cont = cell + 1;

                        cont->codepoint = 0;
                        cont->style = style;
                        cont->attrs = style.attrs;
                        cont->kind = CELL_WIDE_CONT;
                }

                cell += w;
        }

        while (cell < end) {
                cell->codepoint = ' ';
                cell->style = style;
                cell->attrs = ATTR_NONE;
                cell->kind = CELL_NORMAL;
                cell++;
        }
}

void draw_buffer_set_string(DrawBuffer *buf, int row, int col,
                            const char *str, CellStyle style)
{
        int max_width = buf->cols - col;
        draw_buffer_set_string_truncated(buf, row, col, str, max_width, style);
}

CellStyle cell_style_plain(void)
{
        return (CellStyle){.fg = COLOR_DEFAULT, .bg = COLOR_DEFAULT, .isAnsi = false, .attrs = ATTR_NONE};
}

CellStyle cell_style_fg(PixelData color)
{
        return (CellStyle){
            .fg = color,
            .bg = COLOR_DEFAULT,
            .attrs = ATTR_NONE,
        };
}

CellStyle cell_style_from_color(ColorMode mode, ColorValue theme, PixelData color)
{
        CellStyle style = cell_style_plain();
        AppState *state = get_app_state();

        switch (mode) {
        case COLOR_MODE_ALBUM:

                if (color.r >= state->settings.default_color &&
                    color.g >= state->settings.default_color &&
                    color.b >= state->settings.default_color) {

                        style.fg = state->settings.defaultColorRGB;
                } else {
                        style.fg = color;
                }
                break;

        case COLOR_MODE_THEME:
        case COLOR_MODE_DEFAULT:

                if (theme.type == COLOR_TYPE_RGB) {
                        style.fg = theme.rgb;
                } else {
                        style.fgAnsi = theme.ansiIndex;
                        style.isAnsi = true;
                }
                break;
        }

        return style;
}

int utf8_display_width(const char *s)
{
        wchar_t *ws;
        int width;
        size_t len;

        len = mbstowcs(NULL, s, 0);
        if (len == (size_t)-1)
                return -1;

        ws = malloc((len + 1) * sizeof(wchar_t));
        if (!ws)
                return -1;

        mbstowcs(ws, s, len + 1);
        width = wcswidth(ws, len);

        free(ws);
        return width;
}
