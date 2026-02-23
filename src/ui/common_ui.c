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
#include "common/common.h"
#include "common/events.h"

#include "data/theme.h"

#include "utils/term.h"
#include "utils/utils.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned int update_counter = 0;
static const int start_scrolling_delay = 10;
static bool finished_scrolling = false;
static int last_name_position = 0;
static bool is_long_name = false;
static int scroll_delay_skipped_count = 0;

void set_RGB(PixelData p)
{
        // ANSI escape code for setting RGB foreground
        printf("\033[38;2;%d;%d;%dm", p.r, p.g, p.b);
}

void set_album_color(PixelData color)
{
        if (color.r >= 210 && color.g >= 210 && color.b >= 210) {
                AppState *state = get_app_state();
                set_RGB(state->uiSettings.defaultColorRGB);
        } else {
                set_RGB(color);
        }
}

enum EventType get_mouse_action(int num)
{
        enum EventType value = EVENT_NONE;

        switch (num) {
        case 0:
                value = EVENT_NONE;
                break;
        case 1:
                value = EVENT_ENQUEUE;
                break;
        case 2:
                value = EVENT_PLAY_PAUSE;
                break;
        case 3:
                value = EVENT_SCROLLUP;
                break;
        case 4:
                value = EVENT_SCROLLDOWN;
                break;
        case 5:
                value = EVENT_SEEKFORWARD;
                break;
        case 6:
                value = EVENT_SEEKBACK;
                break;
        case 7:
                value = EVENT_VOLUME_UP;
                break;
        case 8:
                value = EVENT_VOLUME_DOWN;
                break;
        case 9:
                value = EVENT_NEXTVIEW;
                break;
        case 10:
                value = EVENT_PREVVIEW;
                break;
        default:
                value = EVENT_NONE;
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
        UISettings *ui = &(state->uiSettings);

        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->stripTrackNumbers = (settings->stripTrackNumbers[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        ui->hideHelp = (settings->hideHelp[0] == '1');
        ui->visualizerEnabled = (settings->visualizerEnabled[0] == '1');
        ui->hideTimeStatus = (settings->hideTimeStatus[0] == '1');
        ui->discordRPCEnabled = (settings->discordRPCEnabled[0] == '1');
        ui->quitAfterStopping = (settings->quitAfterStopping[0] == '1');
        ui->hideGlimmeringText = (settings->hideGlimmeringText[0] == '1');
        ui->mouseEnabled = (settings->mouseEnabled[0] == '1');
        ui->shuffle_enabled = (settings->shuffle_enabled[0] == '1');
        ui->visualizerBrailleMode = (settings->visualizerBrailleMode[0] == '1');
        ui->hideLogo = (settings->hideLogo[0] == '1');
        ui->hideFooter = (settings->hideFooter[0] == '1');
        ui->hideSideCover = (settings->hideSideCover[0] == '1');
        ui->saveRepeatShuffleSettings =
            (settings->saveRepeatShuffleSettings[0] == '1');
        ui->trackTitleAsWindowTitle =
            (settings->trackTitleAsWindowTitle[0] == '1');
        ui->allowNotifications = (settings->allowNotifications[0] == '1');
        ui->coverEnabled = (settings->coverEnabled[0] == '1');
        ui->coverAnsi = (settings->coverAnsi[0] == '1');
        ui->shuffle_enabled = (settings->shuffle_enabled[0] == '1');

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

        enum EventType tmp_event = get_mouse_action(tmp);
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

void reset_name_scroll()
{
        last_name_position = 0;
        is_long_name = false;
        finished_scrolling = false;
        scroll_delay_skipped_count = 0;
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

void copy_half_or_full_width_chars_with_max_width(const char *src, char *dst,
                                                  int max_display_width)
{
        mbstate_t state;
        memset(&state, 0, sizeof(state));

        const char *p = src;
        char *o = dst;
        wchar_t wc;
        int width_sum = 0;

        while (*p) {
                size_t len = mbrtowc(&wc, p, MB_CUR_MAX, &state);

                if (len == (size_t)-1) { // Invalid UTF-8/locale error
                        // Skip one byte, reinit state
                        p++;
                        memset(&state, 0, sizeof(state));
                        continue;
                }
                if (len == (size_t)-2) { // Incomplete sequence
                        break;
                }
                if (len == 0) { // Null terminator
                        break;
                }

                int w = wcwidth(wc);
                if (w < 0) {
                        // Non-printable character; skip it
                        p += len;
                        continue;
                }

                if (width_sum + w > max_display_width)
                        break;

                // Copy valid multibyte sequence
                memcpy(o, p, len);
                o += len;
                p += len;
                width_sum += w;
        }

        *o = '\0';
}

static bool has_fullwidth_chars(const char *str)
{
        mbstate_t state;
        memset(&state, 0, sizeof(state));

        const char *p = str;
        wchar_t wc;

        while (*p) {
                size_t len = mbrtowc(&wc, p, MB_CUR_MAX, &state);
                if (len == (size_t)-1 || len == (size_t)-2 || len == 0)
                        break;

                int w = mk_wcwidth(wc);
                if (w < 0) {
                        break;
                }
                if (w > 1) {
                        return true;
                }
                p += len;
        }
        return false;
}

void process_name(const char *name, char *output, int max_width,
                  bool strip_unneeded_chars, bool strip_suffix)
{
        if (!name) {
                output[0] = '\0';
                return;
        }

        const char *last_dot = strrchr(name, '.');

        if (last_dot != NULL && strip_suffix) {
                char tmp[PATH_MAX];
                size_t len = last_dot - name + 1;
                if (len >= sizeof(tmp))
                        len = sizeof(tmp) - 1;
                c_strcpy(tmp, name, len);
                tmp[len] = '\0';
                copy_half_or_full_width_chars_with_max_width(tmp, output, max_width);
        } else {
                copy_half_or_full_width_chars_with_max_width(name, output, max_width);
        }

        if (strip_unneeded_chars && get_app_state()->uiSettings.stripTrackNumbers)
                format_filename(output);

        trim(output, strlen(output));
}

void process_name_scroll(const char *name, char *output, int max_width,
                         bool is_same_name_as_last_time)
{
        size_t scrollableLength = strnlen(name, max_width);
        size_t nameLength = strlen(name);

        if (scroll_delay_skipped_count <= start_scrolling_delay &&
            nameLength > (size_t)max_width) {
                scrollableLength = max_width;
                scroll_delay_skipped_count++;
                trigger_refresh();
                is_long_name = true;
        }

        int start = (is_same_name_as_last_time) ? last_name_position : 0;

        if (finished_scrolling)
                scrollableLength = max_width;

        if (has_fullwidth_chars(name)) {
                process_name(name, output, max_width, true, true);
        } else if (nameLength <= (size_t)max_width || finished_scrolling) {
                process_name(name, output, scrollableLength, true, true);
        } else {
                is_long_name = true;

                if ((size_t)(start + max_width) > nameLength) {
                        start = 0;
                        finished_scrolling = true;
                }

                c_strcpy(output, name + start, max_width + 1);

                if (get_app_state()->uiSettings.stripTrackNumbers)
                        format_filename(output);

                trim(output, max_width);

                last_name_position++;

                trigger_refresh();
        }
}

bool get_is_long_name()
{
        return is_long_name;
}

PixelData increase_luminosity_for_height(PixelData pixel, int height, int idx, bool reversed)
{
        PixelData pixel2;

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

        return pixel2;
}

PixelData increase_luminosity(PixelData pixel, int amount)
{
        PixelData pixel2;
        pixel2.r = pixel.r + amount <= 255 ? pixel.r + amount : 255;
        pixel2.g = pixel.g + amount <= 255 ? pixel.g + amount : 255;
        pixel2.b = pixel.b + amount <= 255 ? pixel.b + amount : 255;

        return pixel2;
}

#define MIN_CHANNEL 50

PixelData decrease_luminosity_pct(PixelData base, float pct)
{
        PixelData c;

        int r = (int)((float)base.r * pct);
        int g = (int)((float)base.g * pct);
        int b = (int)((float)base.b * pct);

        c.r = (r < MIN_CHANNEL) ? MIN_CHANNEL : r;
        c.g = (g < MIN_CHANNEL) ? MIN_CHANNEL : g;
        c.b = (b < MIN_CHANNEL) ? MIN_CHANNEL : b;

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
