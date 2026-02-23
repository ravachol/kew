/**
 * @file settings.c
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#include "settings.h"

#include "common_ui.h"

#include "common/common.h"
#include "common/events.h"

#include "termbox2_input.h"

#include "common/appstate.h"

#include "ops/playback_state.h"

#include "sound/volume.h"

#include "utils/file.h"
#include "utils/utils.h"

#include <ctype.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

const char SETTINGS_FILE[] = "kewrc";
const char STATE_FILE[] = "kewstaterc";

#define MAX_LINE 1024

#define MAX_KEY_BINDINGS 200

time_t last_time_app_ran;

size_t keybinding_count = 0;

typedef struct
{
        const char *name;
        uint16_t code;
} KeyMap;

TBKeyBinding key_bindings[MAX_KEY_BINDINGS] = {
    {TB_KEY_SPACE, 0, 0, EVENT_PLAY_PAUSE, ""},
    // Basic navigation
    {TB_KEY_TAB, 0, TB_MOD_SHIFT, EVENT_PREVVIEW, ""},
    {TB_KEY_TAB, 0, 0, EVENT_NEXTVIEW, ""},

    // volume
    {0, '+', 0, EVENT_VOLUME_UP, "+5%"},
    {0, '=', 0, EVENT_VOLUME_UP, "+5%"},
    {0, '-', 0, EVENT_VOLUME_DOWN, "-5%"},

    // Tracks
    {0, 'h', 0, EVENT_PREV, ""},
    {0, 'l', 0, EVENT_NEXT, ""},
    {0, 'k', 0, EVENT_SCROLLUP, ""},
    {0, 'j', 0, EVENT_SCROLLDOWN, ""},

    // Controls
    {0, 'p', 0, EVENT_PLAY_PAUSE, ""},
    {0, 'n', 0, EVENT_TOGGLENOTIFICATIONS, ""},
    {0, 'v', 0, EVENT_TOGGLEVISUALIZER, ""},
    {0, 'b', 0, EVENT_TOGGLEASCII, ""},
    {0, 'r', 0, EVENT_TOGGLEREPEAT, ""},
    {0, 'i', 0, EVENT_CYCLECOLORMODE, ""},
    {0, 't', 0, EVENT_CYCLETHEMES, ""},
    {0, 'c', 0, EVENT_CYCLEVISUALIZATION, ""},
    {0, 's', 0, EVENT_SHUFFLE, ""},
    {0, 'a', 0, EVENT_SEEKBACK, ""},
    {0, 'd', 0, EVENT_SEEKFORWARD, ""},
    {0, 'o', 0, EVENT_SORTLIBRARY, ""},
    {0, 'm', 0, EVENT_SHOWLYRICSPAGE, ""},
    {0, 's', TB_MOD_SHIFT, EVENT_STOP, ""},

    // Playlist actions
    {0, 'x', 0, EVENT_EXPORTPLAYLIST, ""},
    {0, '.', 0, EVENT_ADDTOFAVORITESPLAYLIST, ""},
    {0, 'u', 0, EVENT_UPDATELIBRARY, ""},
    {0, 'f', 0, EVENT_MOVESONGUP, ""},
    {0, 'g', 0, EVENT_MOVESONGDOWN, ""},

    {TB_KEY_ENTER, 0, 0, EVENT_ENQUEUE, ""},
    {0, 'g', TB_MOD_SHIFT, EVENT_ENQUEUE, ""},
    {TB_KEY_BACKSPACE, 0, 0, EVENT_CLEARPLAYLIST, ""},
    {TB_KEY_ENTER, 0, TB_MOD_ALT, EVENT_ENQUEUEANDPLAY, ""},

    // Hard navigation / arrows
    {TB_KEY_ARROW_LEFT, 0, 0, EVENT_PREV, ""},
    {TB_KEY_ARROW_RIGHT, 0, 0, EVENT_NEXT, ""},
    {TB_KEY_ARROW_UP, 0, 0, EVENT_SCROLLUP, ""},
    {TB_KEY_ARROW_DOWN, 0, 0, EVENT_SCROLLDOWN, ""},

#if defined(__ANDROID__) || defined(__APPLE__)

    // Show Views macOS/Android
    {0, 'z', TB_MOD_SHIFT, EVENT_SHOWPLAYLIST, ""},
    {0, 'x', TB_MOD_SHIFT, EVENT_SHOWLIBRARY, ""},
    {0, 'c', TB_MOD_SHIFT, EVENT_SHOWTRACK, ""},
    {0, 'v', TB_MOD_SHIFT, EVENT_SHOWSEARCH, ""},
    {0, 'b', TB_MOD_SHIFT, EVENT_SHOWHELP, ""},
#else
    // Show Views
    {TB_KEY_F2, 0, 0, EVENT_SHOWPLAYLIST, ""},
    {TB_KEY_F3, 0, 0, EVENT_SHOWLIBRARY, ""},
    {TB_KEY_F4, 0, 0, EVENT_SHOWTRACK, ""},
    {TB_KEY_F5, 0, 0, EVENT_SHOWSEARCH, ""},
    {TB_KEY_F6, 0, 0, EVENT_SHOWHELP, ""},
#endif

    // Page navigation
    {TB_KEY_PGDN, 0, 0, EVENT_NEXTPAGE, ""},
    {TB_KEY_PGUP, 0, 0, EVENT_PREVPAGE, ""},

    // Remove
    {TB_KEY_DELETE, 0, 0, EVENT_REMOVE, ""},

    // Mouse events
    {TB_KEY_MOUSE_MIDDLE, 0, 0, EVENT_ENQUEUEANDPLAY, ""},
    {TB_KEY_MOUSE_RIGHT, 0, 0, EVENT_PLAY_PAUSE, ""},
    {TB_KEY_MOUSE_WHEEL_DOWN, 0, 0, EVENT_SCROLLDOWN, ""},
    {TB_KEY_MOUSE_WHEEL_UP, 0, 0, EVENT_SCROLLUP, ""},

    {0, 'q', 0, EVENT_QUIT, ""},
    {TB_KEY_ESC, 0, 0, EVENT_QUIT, ""}};

static const KeyMap key_map[] = {
    // Arrow keys
    {"Up", TB_KEY_ARROW_UP},
    {"Down", TB_KEY_ARROW_DOWN},
    {"Left", TB_KEY_ARROW_LEFT},
    {"Right", TB_KEY_ARROW_RIGHT},

    // Function keys
    {"F1", TB_KEY_F1},
    {"F2", TB_KEY_F2},
    {"F3", TB_KEY_F3},
    {"F4", TB_KEY_F4},
    {"F5", TB_KEY_F5},
    {"F6", TB_KEY_F6},
    {"F7", TB_KEY_F7},
    {"F8", TB_KEY_F8},
    {"F9", TB_KEY_F9},
    {"F10", TB_KEY_F10},
    {"F11", TB_KEY_F11},
    {"F12", TB_KEY_F12},

    // Navigation / editing
    {"Insert", TB_KEY_INSERT},
    {"Ins", TB_KEY_INSERT},
    {"Del", TB_KEY_DELETE},
    {"Delete", TB_KEY_DELETE},
    {"Home", TB_KEY_HOME},
    {"End", TB_KEY_END},
    {"PgUp", TB_KEY_PGUP},
    {"PageUp", TB_KEY_PGUP},
    {"PgDn", TB_KEY_PGDN},
    {"PageDown", TB_KEY_PGDN},
    {"BackTab", TB_KEY_BACK_TAB},
    {"Tab", TB_KEY_TAB},
    {"Backspace", TB_KEY_BACKSPACE},
    {"Enter", TB_KEY_ENTER},
    {"Esc", TB_KEY_ESC},
    {"Escape", TB_KEY_ESC},
    {"Space", TB_KEY_SPACE},

    // Modifiers
    {"Shift", TB_MOD_SHIFT},
    {"Alt", TB_MOD_ALT},
    {"Ctrl", TB_MOD_CTRL},

    // Mouse buttons
    {"mouseLeft", TB_KEY_MOUSE_LEFT},
    {"mouseRight", TB_KEY_MOUSE_RIGHT},
    {"mouseMiddle", TB_KEY_MOUSE_MIDDLE},
    {"mouseRelease", TB_KEY_MOUSE_RELEASE},
    {"mouseWheelUp", TB_KEY_MOUSE_WHEEL_UP},
    {"mouseWheelDown", TB_KEY_MOUSE_WHEEL_DOWN},

    // Symbols
    {"=", '='},
    {"+", '+'},
    {"-", '-'},
    {"*", '*'},
    {"/", '/'},
    {"\\", '\\'},
    {";", ';'},
    {":", ':'},
    {",", ','},
    {".", '.'},
    {"'", '\''},
    {"\"", '\"'},
    {"`", '`'},
    {"~", '~'},
    {"[", '['},
    {"]", ']'},
    {"(", '('},
    {")", ')'},
    {"<", '<'},
    {">", '>'},
    {"!", '!'},
    {"@", '@'},
    {"#", '#'},
    {"$", '$'},
    {"%", '%'},
    {"^", '^'},
    {"&", '&'},
    {"|", '|'},
    {"?", '?'},

    {NULL, 0} // Sentinel
};

const char *get_key_name(int key)
{
        for (int i = 0; key_map[i].name != NULL; i++) {
                if (key_map[i].code == key)
                        return key_map[i].name;
        }

        // Printable ASCII fallback
        static char buf[2];
        if (key >= 32 && key <= 126) {
                buf[0] = (char)key;
                buf[1] = '\0';
                return buf;
        }

        return "?";
}

const char *get_modifier_string(uint8_t mods)
{
        static char buf[64];
        buf[0] = '\0';

        int first = 1;
        if (mods & TB_MOD_CTRL) {
                strcat(buf, "Ctrl");
                first = 0;
        }
        if (mods & TB_MOD_ALT) {
                if (!first)
                        strcat(buf, "+");
                strcat(buf, "Alt");
                first = 0;
        }
        if (mods & TB_MOD_SHIFT) {
                if (!first)
                        strcat(buf, "+");
                strcat(buf, "Shft");
        }

        return buf;
}

static bool key_str_already_added(const char *buf, const char *token)
{
        char temp[256];
        strncpy(temp, buf, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *part = strtok(temp, " or ");
        while (part != NULL) {
                if (strcmp(part, token) == 0)
                        return true;
                part = strtok(NULL, " or ");
        }
        return false;
}

const char *get_binding_string(enum EventType event, bool find_only_one)
{
        static char buf[256];
        buf[0] = '\0';

        int found = 0;

        for (int i = 0; i < MAX_KEY_BINDINGS; i++) {
                if (key_bindings[i].eventType != event)
                        continue;

                const char *key_part = "?";

                // Determine key name
                if (key_bindings[i].key != 0)
                        key_part = get_key_name(key_bindings[i].key);
                else if (key_bindings[i].ch != 0) {
                        static char cbuf[2];
                        cbuf[0] = (char)key_bindings[i].ch;
                        cbuf[1] = '\0';
                        key_part = cbuf;
                }

                const char *mod_part = get_modifier_string(key_bindings[i].mods);

                // Build "Alt+Enter" style string for this binding
                char full_key[64];
                if (mod_part[0] != '\0')
                        snprintf(full_key, sizeof(full_key), "%s+%s", mod_part, key_part);
                else
                        snprintf(full_key, sizeof(full_key), "%s", key_part);

                // Skip duplicate key names (e.g. "Space" vs " ")
                if (key_str_already_added(buf, full_key))
                        continue;

                // Append to output buffer
                if (found > 0) {
                        if (find_only_one)
                                return buf;
                        strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
                }
                strncat(buf, full_key, sizeof(buf) - strlen(buf) - 1);

                found++;
        }

        if (!found)
                snprintf(buf, sizeof(buf), "?");

        return buf;
}

static uint16_t key_name_to_code(const char *name)
{
        if (!name || !*name)
                return 0;

        for (int i = 0; key_map[i].name != NULL; i++) {
                if (strcmp(name, key_map[i].name) == 0)
                        return key_map[i].code;
        }

        // Single printable char fallback
        if (strlen(name) == 1)
                return (uint16_t)(unsigned char)name[0];

        return 0;
}

static const char *key_code_to_name(uint16_t code)
{
        for (int i = 0; key_map[i].name != NULL; i++) {
                if (key_map[i].code == code)
                        return key_map[i].name;
        }

        // If code is printable ASCII, return it as a string
        static char buf[2];
        if (code >= 32 && code <= 126) {
                buf[0] = (char)code;
                buf[1] = '\0';
                return buf;
        }

        return "Unknown";
}

TBKeyBinding *get_key_bindings()
{
        return key_bindings;
}

AppSettings init_settings(void)
{
        AppState *state = get_app_state();
        UserData *user_data = audio_data.pUserData;

        AppSettings settings;

        keybinding_count = NUM_DEFAULT_KEY_BINDINGS;

        get_config(&settings, &(state->uiSettings));
        get_prefs(&settings, &(state->uiSettings));

        user_data->replayGainCheckFirst =
            state->uiSettings.replayGainCheckFirst;

        return settings;
}

void free_key_value_pairs(KeyValuePair *pairs, int count)
{
        if (pairs == NULL || count <= 0) {
                return;
        }

        for (size_t i = 0; i < (size_t)count; i++) {
                free(pairs[i].key);
                free(pairs[i].value);
        }

        free(pairs);
}

static int stricmp(const char *a, const char *b)
{
        while (*a && *b) {
                int diff = tolower(*a) - tolower(*b);
                if (diff != 0)
                        return diff;
                a++;
                b++;
        }
        return *a - *b;
}

typedef struct
{
        const char *name;
        enum EventType event;
} EventMap;

static const EventMap event_map[] = {
    {"playPause", EVENT_PLAY_PAUSE},
    {"volUp", EVENT_VOLUME_UP},
    {"volDown", EVENT_VOLUME_DOWN},
    {"nextSong", EVENT_NEXT},
    {"prevSong", EVENT_PREV},
    {"quit", EVENT_QUIT},
    {"toggleRepeat", EVENT_TOGGLEREPEAT},
    {"toggleVisualizer", EVENT_TOGGLEVISUALIZER},
    {"toggleAscii", EVENT_TOGGLEASCII},
    {"addToFavorites_playlist", EVENT_ADDTOFAVORITESPLAYLIST},
    {"deleteFromMainPlaylist", EVENT_DELETEFROMMAINPLAYLIST},
    {"exportPlaylist", EVENT_EXPORTPLAYLIST},
    {"updateLibrary", EVENT_UPDATELIBRARY},
    {"shuffle", EVENT_SHUFFLE},
    {"keyPress", EVENT_KEY_PRESS},
    {"showHelp", EVENT_SHOWHELP},
    {"showPlaylist", EVENT_SHOWPLAYLIST},
    {"showSearch", EVENT_SHOWSEARCH},
    {"enqueue", EVENT_ENQUEUE},
    {"gotoBeginningOfPlaylist", EVENT_GOTOBEGINNINGOFPLAYLIST},
    {"gotoEndOfPlaylist", EVENT_GOTOENDOFPLAYLIST},
    {"cycleColorMode", EVENT_CYCLECOLORMODE},
    {"cycleVisualization", EVENT_CYCLEVISUALIZATION},
    {"scrollDown", EVENT_SCROLLDOWN},
    {"scrollUp", EVENT_SCROLLUP},
    {"seekBack", EVENT_SEEKBACK},
    {"seekForward", EVENT_SEEKFORWARD},
    {"showLibrary", EVENT_SHOWLIBRARY},
    {"showTrack", EVENT_SHOWTRACK},
    {"nextPage", EVENT_NEXTPAGE},
    {"prevPage", EVENT_PREVPAGE},
    {"remove", EVENT_REMOVE},
    {"search", EVENT_SEARCH},
    {"nextView", EVENT_NEXTVIEW},
    {"prevView", EVENT_PREVVIEW},
    {"clearPlaylist", EVENT_CLEARPLAYLIST},
    {"moveSongUp", EVENT_MOVESONGUP},
    {"moveSongDown", EVENT_MOVESONGDOWN},
    {"enqueueAndPlay", EVENT_ENQUEUEANDPLAY},
    {"stop", EVENT_STOP},
    {"sortLibrary", EVENT_SORTLIBRARY},
    {"cycleThemes", EVENT_CYCLETHEMES},
    {"toggleNotifications", EVENT_TOGGLENOTIFICATIONS},
    {"showLyricsPage", EVENT_SHOWLYRICSPAGE},
    {NULL, EVENT_NONE} // Sentinel
};

static enum EventType parse_to_event(const char *name)
{
        if (!name)
                return EVENT_NONE;

        for (int i = 0; event_map[i].name != NULL; i++) {
                if (stricmp(name, event_map[i].name) == 0)
                        return event_map[i].event;
        }
        return EVENT_NONE;
}

static const char *event_to_string(enum EventType ev)
{
        for (int i = 0; event_map[i].name != NULL; i++) {
                if (event_map[i].event == ev)
                        return event_map[i].name;
        }
        return "EVENT_NONE";
}

TBKeyBinding parse_binding(const char *binding_str,
                           const char *event_str,
                           const char *args_str)
{
        TBKeyBinding kb = {0};
        kb.eventType = parse_to_event(event_str);
        kb.args[0] = '\0';

        if (args_str && *args_str)
                strncpy(kb.args, args_str, sizeof(kb.args) - 1);

        if (!binding_str || !*binding_str)
                return kb;

        char temp[64];
        strncpy(temp, binding_str, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *p = temp;
        while (p && *p) {
                char *delim = strchr(p, '+'); // Next '+' or NULL
                size_t raw_len = delim ? (size_t)(delim - p) : strlen(p);

                char tokenbuf[64];
                size_t token_len = 0;

                if (raw_len == 0) {
                        // Empty between delimiters -> literal '+' token
                        tokenbuf[0] = '+';
                        tokenbuf[1] = '\0';
                } else {
                        // Copy substring [p .. p+raw_len) and trim whitespace
                        size_t s = 0;
                        while (s < raw_len && isspace((unsigned char)p[s]))
                                s++;
                        size_t e = raw_len;
                        while (e > s && isspace((unsigned char)p[e - 1]))
                                e--;
                        token_len = e - s;
                        if (token_len >= sizeof(tokenbuf))
                                token_len = sizeof(tokenbuf) - 1;
                        if (token_len > 0)
                                memcpy(tokenbuf, p + s, token_len);
                        tokenbuf[token_len] = '\0';
                }

                // If tokenbuf is still empty (shouldn't happen), skip it
                if (tokenbuf[0] != '\0') {
                        // Handle modifiers
                        if (stricmp(tokenbuf, "Ctrl") == 0 ||
                            stricmp(tokenbuf, "LCtrl") == 0 ||
                            stricmp(tokenbuf, "RCtrl") == 0) {
                                kb.mods |= TB_MOD_CTRL;
                        } else if (stricmp(tokenbuf, "Alt") == 0 ||
                                   stricmp(tokenbuf, "LAlt") == 0 ||
                                   stricmp(tokenbuf, "RAlt") == 0) {
                                kb.mods |= TB_MOD_ALT;
                        } else if (stricmp(tokenbuf, "Shift") == 0 ||
                                   stricmp(tokenbuf, "LShift") == 0 ||
                                   stricmp(tokenbuf, "RShift") == 0) {
                                kb.mods |= TB_MOD_SHIFT;
                        } else if (strcmp(tokenbuf, "Space") == 0) {
                                kb.key = TB_KEY_SPACE;
                                kb.ch = 0;
                        } else {
                                // Normal key token (including "+" token)
                                uint16_t code = key_name_to_code(tokenbuf);
                                if (code >= 0x20 && code < 0x7f) {
                                        kb.key = 0;
                                        kb.ch = code;
                                } else {
                                        kb.key = code;
                                        kb.ch = 0;
                                }
                        }
                }

                if (!delim)
                        break; // Done
                p = delim + 1; // Advance past '+'
        }

        return kb;
}

void set_default_config(AppSettings *settings)
{
        memset(settings, 0, sizeof(AppSettings));
        c_strcpy(settings->coverEnabled, "1", sizeof(settings->coverEnabled));
        c_strcpy(settings->stripTrackNumbers, "1",
                 sizeof(settings->stripTrackNumbers));
        c_strcpy(settings->allowNotifications, "1",
                 sizeof(settings->allowNotifications));
        c_strcpy(settings->coverAnsi, "0", sizeof(settings->coverAnsi));
        c_strcpy(settings->quitAfterStopping, "0",
                 sizeof(settings->quitAfterStopping));
        c_strcpy(settings->hideGlimmeringText, "0",
                 sizeof(settings->hideGlimmeringText));
        c_strcpy(settings->mouseEnabled, "1", sizeof(settings->mouseEnabled));
        c_strcpy(settings->replayGainCheckFirst, "0",
                 sizeof(settings->replayGainCheckFirst));
        c_strcpy(settings->visualizer_bar_width, "2",
                 sizeof(settings->visualizer_bar_width));
        c_strcpy(settings->visualizerBrailleMode, "0",
                 sizeof(settings->visualizerBrailleMode));
        c_strcpy(settings->progressBarElapsedEvenChar, "━",
                 sizeof(settings->progressBarElapsedEvenChar));
        c_strcpy(settings->progressBarElapsedOddChar, "━",
                 sizeof(settings->progressBarElapsedOddChar));
        c_strcpy(settings->progressBarApproachingEvenChar, "━",
                 sizeof(settings->progressBarApproachingEvenChar));
        c_strcpy(settings->progressBarApproachingOddChar, "━",
                 sizeof(settings->progressBarApproachingOddChar));
        c_strcpy(settings->progressBarCurrentEvenChar, "━",
                 sizeof(settings->progressBarCurrentEvenChar));
        c_strcpy(settings->progressBarCurrentOddChar, "━",
                 sizeof(settings->progressBarCurrentOddChar));
        c_strcpy(settings->saveRepeatShuffleSettings, "1",
                 sizeof(settings->saveRepeatShuffleSettings));
        c_strcpy(settings->trackTitleAsWindowTitle, "1",
                 sizeof(settings->trackTitleAsWindowTitle));
        c_strcpy(settings->discordRPCEnabled, "1",
                 sizeof(settings->discordRPCEnabled));
        c_strcpy(settings->visualizerEnabled, "1",
                 sizeof(settings->visualizerEnabled));
#ifdef __APPLE__
        c_strcpy(settings->colorMode, "0",
                 sizeof(settings->colorMode));
#else
        c_strcpy(settings->colorMode, "1",
                 sizeof(settings->colorMode));
#endif

#ifdef __ANDROID__
        c_strcpy(settings->hideLogo, "1", sizeof(settings->hideLogo));
#else
        c_strcpy(settings->hideLogo, "0", sizeof(settings->hideLogo));
#endif
        c_strcpy(settings->hideFooter, "0", sizeof(settings->hideFooter));
        c_strcpy(settings->hideHelp, "0", sizeof(settings->hideHelp));
        c_strcpy(settings->hideSideCover, "0", sizeof(settings->hideSideCover));
        c_strcpy(settings->hideTimeStatus, "0", sizeof(settings->hideTimeStatus));
        c_strcpy(settings->visualizer_height, "6",
                 sizeof(settings->visualizer_height));
        c_strcpy(settings->visualizer_color_type, "2",
                 sizeof(settings->visualizer_color_type));
        c_strcpy(settings->titleDelay, "9", sizeof(settings->titleDelay));

        c_strcpy(settings->lastVolume, "100", sizeof(settings->lastVolume));
        c_strcpy(settings->color, "6", sizeof(settings->color));
        c_strcpy(settings->artistColor, "6", sizeof(settings->artistColor));
        c_strcpy(settings->titleColor, "6", sizeof(settings->titleColor));
        c_strcpy(settings->enqueued_color, "6", sizeof(settings->enqueued_color));

        memcpy(settings->ansiTheme, "default", 8);
}

bool is_printable_ascii(const char *value)
{
        if (value == NULL || value[0] == '\0' || value[1] != '\0')
                return false; // must be exactly one character

        unsigned char c = (unsigned char)value[0];
        return (c >= 32 && c <= 126); // printable ASCII range
}

uint32_t utf8_to_codepoint(const char *s)
{
        const unsigned char *u = (const unsigned char *)s;
        if (u[0] < 0x80)
                return u[0];
        else if ((u[0] & 0xE0) == 0xC0)
                return ((u[0] & 0x1F) << 6) | (u[1] & 0x3F);
        else if ((u[0] & 0xF0) == 0xE0)
                return ((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        else if ((u[0] & 0xF8) == 0xF0)
                return ((u[0] & 0x07) << 18) | ((u[1] & 0x3F) << 12) | ((u[2] & 0x3F) << 6) | (u[3] & 0x3F);
        else
                return 0; // invalid
}

void remove_printable_key_binding(char *value)
{
        for (size_t i = 0; i < keybinding_count; i++) {
                if (key_bindings[i].ch == utf8_to_codepoint(value) &&
                    key_bindings[i].mods == 0 &&
                    key_bindings[i].key == 0) {
                        key_bindings[i].ch = 0;
                        key_bindings[i].key = 0;
                        key_bindings[i].eventType = EVENT_NONE;
                        key_bindings[i].args[0] = '\0';
                }
        }
}

void remove_special_key_binding(uint16_t value)
{
        for (size_t i = 0; i < keybinding_count; i++) {
                if (key_bindings[i].key == value &&
                    key_bindings[i].mods == 0 &&
                    key_bindings[i].ch == 0) {
                        key_bindings[i].key = 0;
                        key_bindings[i].ch = 0;
                        key_bindings[i].eventType = EVENT_NONE;
                        key_bindings[i].args[0] = '\0';
                }
        }
}

int convert_legacy_key(const char *s)
{
        if (!s || !*s)
                return 0;

        if (strcmp(s, "[A") == 0)
                return TB_KEY_ARROW_UP;
        if (strcmp(s, "[B") == 0)
                return TB_KEY_ARROW_DOWN;
        if (strcmp(s, "[C") == 0)
                return TB_KEY_ARROW_RIGHT;
        if (strcmp(s, "[D") == 0)
                return TB_KEY_ARROW_LEFT;

        if (strcmp(s, "[H") == 0)
                return TB_KEY_HOME;
        if (strcmp(s, "[F") == 0)
                return TB_KEY_END;

        if (strcmp(s, "OP") == 0)
                return TB_KEY_F1;
        if (strcmp(s, "OQ") == 0)
                return TB_KEY_F2;
        if (strcmp(s, "OR") == 0)
                return TB_KEY_F3;
        if (strcmp(s, "OS") == 0)
                return TB_KEY_F4;
        if (strcmp(s, "[15~") == 0)
                return TB_KEY_F5;
        if (strcmp(s, "[17~") == 0)
                return TB_KEY_F6;
        if (strcmp(s, "[18~") == 0)
                return TB_KEY_F7;
        if (strcmp(s, "[19~") == 0)
                return TB_KEY_F8;
        if (strcmp(s, "[20~") == 0)
                return TB_KEY_F9;
        if (strcmp(s, "[21~") == 0)
                return TB_KEY_F10;
        if (strcmp(s, "[23~") == 0)
                return TB_KEY_F11;
        if (strcmp(s, "[24~") == 0)
                return TB_KEY_F12;

        if (strcmp(s, "[5~") == 0)
                return TB_KEY_PGUP;
        if (strcmp(s, "[6~") == 0)
                return TB_KEY_PGDN;
        if (strcmp(s, "[2~") == 0)
                return TB_KEY_INSERT;
        if (strcmp(s, "[3~") == 0)
                return TB_KEY_DELETE;

        return 0;
}

void add_legacy_key_binding(enum EventType event, char *value)
{
        if (keybinding_count >= MAX_KEY_BINDINGS)
                return;

        TBKeyBinding kb = {0};
        kb.eventType = event;

        // Printable single-char key
        if (is_printable_ascii(value)) {
                remove_printable_key_binding(value);
                kb.ch = utf8_to_codepoint(value);
                key_bindings[keybinding_count++] = kb;
        }
        // Escape sequence (starts with 0x1b)
        else {
                int tb_key = convert_legacy_key(value);
                if (tb_key == 0) {
                        return;
                }
                remove_special_key_binding(tb_key);
                kb.key = tb_key;
                key_bindings[keybinding_count++] = kb;
        }
}

bool replace_key_binding(TBKeyBinding binding)
{
        for (size_t i = 0; i < keybinding_count; i++) {
                if (key_bindings[i].key == binding.key &&
                    key_bindings[i].ch == binding.ch &&
                    key_bindings[i].mods == binding.mods) {
                        key_bindings[i].eventType = binding.eventType;
                        snprintf(key_bindings[i].args, sizeof(key_bindings[i].args), "%s", binding.args);

                        return true;
                }
        }

        return false;
}

void add_key_binding(TBKeyBinding binding)
{
        if (!replace_key_binding(binding))
                key_bindings[keybinding_count++] = binding;
}

void add_legacy_mouse_binding(int mouseInputType, int mod, int action)
{
        enum EventType event = get_mouse_action(action);

        TBKeyBinding kb = {mouseInputType, 0, mod, event, ""};

        add_key_binding(kb);
}

static void trim_start(char **s)
{
        while (**s && isspace((unsigned char)**s))
                (*s)++;
}

static char *parse_field(char **s)
{
        trim_start(s);
        if (!**s)
                return NULL;

        char buffer[256];
        int idx = 0;
        int in_quotes = 0;

        if (**s == '"') {
                in_quotes = 1;
                (*s)++;
        }

        while (**s) {
                if (in_quotes) {
                        if (**s == '"') {
                                (*s)++;
                                break;
                        }
                        if (**s == '\\') {
                                (*s)++;
                                if (**s)
                                        buffer[idx++] = **s++;
                        } else
                                buffer[idx++] = *(*s)++;
                } else {
                        if (**s == ',') {
                                (*s)++;
                                break;
                        }
                        if (**s == '\\') {
                                (*s)++;
                                if (**s)
                                        buffer[idx++] = **s++;
                        } else
                                buffer[idx++] = *(*s)++;
                }
                if (idx >= (int)(sizeof(buffer) - 1))
                        break; // avoid overflow
        }

        buffer[idx] = '\0';
        return strdup(buffer); // return a copy
}

void construct_app_settings(AppSettings *settings, KeyValuePair *pairs, int count, bool is_prefs)
{
        if (pairs == NULL) {
                return;
        }

        bool foundCycleThemesSetting = false;

        for (int i = 0; i < count; i++) {
                KeyValuePair *pair = &pairs[i];

                trim(pair->key, strlen(pair->key));
                char *lowercase_key = string_to_lower(pair->key);

                if (strcmp(lowercase_key, "path") == 0) {
                        snprintf(settings->path, sizeof(settings->path), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "theme") == 0) {
                        snprintf(settings->theme, sizeof(settings->theme), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "chromapreset") == 0) {
                        snprintf(settings->chromaPreset, sizeof(settings->chromaPreset), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "coverenabled") == 0) {
                        snprintf(settings->coverEnabled,
                                 sizeof(settings->coverEnabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "coveransi") == 0) {
                        snprintf(settings->coverAnsi, sizeof(settings->coverAnsi),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "visualizerenabled") == 0) {
                        snprintf(settings->visualizerEnabled,
                                 sizeof(settings->visualizerEnabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "hidetimestatus") == 0) {
                        snprintf(settings->hideTimeStatus,
                                 sizeof(settings->hideTimeStatus), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "discordrpcenabled") == 0) {
                        snprintf(settings->discordRPCEnabled,
                                 sizeof(settings->discordRPCEnabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "useconfigcolors") == 0) {
                        snprintf(settings->useConfigColors,
                                 sizeof(settings->useConfigColors), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "visualizerheight") == 0) {
                        snprintf(settings->visualizer_height,
                                 sizeof(settings->visualizer_height), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "visualizercolortype") == 0) {
                        snprintf(settings->visualizer_color_type,
                                 sizeof(settings->visualizer_color_type), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "titledelay") == 0) {
                        snprintf(settings->titleDelay,
                                 sizeof(settings->titleDelay), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "volumeup") == 0) {
                        snprintf(settings->volumeUp, sizeof(settings->volumeUp),
                                 "%s", pair->value);
                        add_legacy_key_binding(EVENT_VOLUME_UP, pair->value);
                } else if (strcmp(lowercase_key, "volumeupalt") == 0) {
                        snprintf(settings->volumeUpAlt,
                                 sizeof(settings->volumeUpAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_VOLUME_UP, pair->value);
                } else if (strcmp(lowercase_key, "volumedown") == 0) {
                        snprintf(settings->volumeDown,
                                 sizeof(settings->volumeDown), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_VOLUME_DOWN, pair->value);
                } else if (strcmp(lowercase_key, "previoustrackalt") == 0) {
                        snprintf(settings->previousTrackAlt,
                                 sizeof(settings->previousTrackAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_PREV, pair->value);
                } else if (strcmp(lowercase_key, "nexttrackalt") == 0) {
                        snprintf(settings->nextTrackAlt,
                                 sizeof(settings->nextTrackAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_NEXT, pair->value);
                } else if (strcmp(lowercase_key, "scrollupalt") == 0) {
                        snprintf(settings->scrollUpAlt,
                                 sizeof(settings->scrollUpAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_SCROLLUP, pair->value);
                } else if (strcmp(lowercase_key, "scrolldownalt") == 0) {
                        snprintf(settings->scrollDownAlt,
                                 sizeof(settings->scrollDownAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_SCROLLDOWN, pair->value);
                } else if (strcmp(lowercase_key, "switchnumberedsong") == 0) {
                        snprintf(settings->switchNumberedSong,
                                 sizeof(settings->switchNumberedSong), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_ENQUEUE, pair->value);
                } else if (strcmp(lowercase_key, "togglepause") == 0) {
                        snprintf(settings->toggle_pause,
                                 sizeof(settings->toggle_pause), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_PLAY_PAUSE, pair->value);
                } else if (strcmp(lowercase_key, "togglecolorsderivedfrom") == 0) {
                        snprintf(settings->cycleColorsDerivedFrom,
                                 sizeof(settings->cycleColorsDerivedFrom), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_CYCLECOLORMODE, pair->value);
                } else if (strcmp(lowercase_key, "cyclethemes") == 0) {
                        snprintf(settings->cycle_themes,
                                 sizeof(settings->cycle_themes), "%s",
                                 pair->value);
                        foundCycleThemesSetting = true;
                        add_legacy_key_binding(EVENT_CYCLETHEMES, pair->value);
                } else if (strcmp(lowercase_key, "togglenotifications") == 0) {
                        snprintf(settings->toggle_notifications,
                                 sizeof(settings->toggle_notifications), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_TOGGLENOTIFICATIONS, pair->value);
                } else if (strcmp(lowercase_key, "togglevisualizer") == 0) {
                        snprintf(settings->toggle_visualizer,
                                 sizeof(settings->toggle_visualizer), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_TOGGLEVISUALIZER, pair->value);
                } else if (strcmp(lowercase_key, "toggleascii") == 0) {
                        snprintf(settings->toggle_ascii,
                                 sizeof(settings->toggle_ascii), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_TOGGLEASCII, pair->value);
                } else if (strcmp(lowercase_key, "togglerepeat") == 0) {
                        snprintf(settings->toggle_repeat,
                                 sizeof(settings->toggle_repeat), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_TOGGLEREPEAT, pair->value);
                } else if (strcmp(lowercase_key, "toggleshuffle") == 0) {
                        snprintf(settings->toggle_shuffle,
                                 sizeof(settings->toggle_shuffle), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_SHUFFLE, pair->value);
                } else if (strcmp(lowercase_key, "seekbackward") == 0) {
                        snprintf(settings->seekBackward,
                                 sizeof(settings->seekBackward), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_SEEKBACK, pair->value);
                } else if (strcmp(lowercase_key, "seekforward") == 0) {
                        snprintf(settings->seek_forward,
                                 sizeof(settings->seek_forward), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_SEEKFORWARD, pair->value);
                } else if (strcmp(lowercase_key, "saveplaylist") == 0) {
                        snprintf(settings->save_playlist,
                                 sizeof(settings->save_playlist), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_EXPORTPLAYLIST, pair->value);
                } else if (strcmp(lowercase_key, "addtofavoritesplaylist") == 0) {
                        snprintf(settings->add_to_favorites_playlist,
                                 sizeof(settings->add_to_favorites_playlist), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_ADDTOFAVORITESPLAYLIST, pair->value);
                } else if (strcmp(lowercase_key, "lastvolume") == 0) {
                        snprintf(settings->lastVolume,
                                 sizeof(settings->lastVolume), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "allownotifications") == 0) {
                        snprintf(settings->allowNotifications,
                                 sizeof(settings->allowNotifications), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "striptracknumbers") == 0) {
                        if (!is_prefs)
                                snprintf(settings->stripTrackNumbers,
                                         sizeof(settings->stripTrackNumbers),
                                         "%s", pair->value);
                } else if (strcmp(lowercase_key, "colormode") == 0) {
                        snprintf(settings->colorMode, sizeof(settings->colorMode), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "color") == 0) {
                        snprintf(settings->color, sizeof(settings->color), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "artistcolor") == 0) {
                        snprintf(settings->artistColor,
                                 sizeof(settings->artistColor), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "enqueuedcolor") == 0) {
                        snprintf(settings->enqueued_color,
                                 sizeof(settings->enqueued_color), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "titlecolor") == 0) {
                        snprintf(settings->titleColor,
                                 sizeof(settings->titleColor), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "mouseenabled") == 0) {
                        snprintf(settings->mouseEnabled,
                                 sizeof(settings->mouseEnabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "repeatstate") == 0) {
                        snprintf(settings->repeatState,
                                 sizeof(settings->repeatState), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "shuffleenabled") == 0) {
                        snprintf(settings->shuffle_enabled,
                                 sizeof(settings->shuffle_enabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "saverepeatshufflesettings") == 0) {
                        snprintf(settings->saveRepeatShuffleSettings,
                                 sizeof(settings->saveRepeatShuffleSettings),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "tracktitleaswindowtitle") == 0) {
                        snprintf(settings->trackTitleAsWindowTitle,
                                 sizeof(settings->trackTitleAsWindowTitle), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "replaygaincheckfirst") == 0) {
                        snprintf(settings->replayGainCheckFirst,
                                 sizeof(settings->replayGainCheckFirst), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "visualizerbarwidth") == 0) {
                        snprintf(settings->visualizer_bar_width,
                                 sizeof(settings->visualizer_bar_width), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "visualizerbraillemode") == 0) {
                        snprintf(settings->visualizerBrailleMode,
                                 sizeof(settings->visualizerBrailleMode), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "mouseleftclickaction") == 0) {
                        snprintf(settings->mouseLeftClickAction,
                                 sizeof(settings->mouseLeftClickAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_LEFT, 0, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mousemiddleclickaction") == 0) {
                        snprintf(settings->mouseMiddleClickAction,
                                 sizeof(settings->mouseMiddleClickAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_MIDDLE, 0, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mouserightclickaction") == 0) {
                        snprintf(settings->mouseRightClickAction,
                                 sizeof(settings->mouseRightClickAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_RIGHT, 0, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mousescrollupaction") == 0) {
                        snprintf(settings->mouseScrollUpAction,
                                 sizeof(settings->mouseScrollUpAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_WHEEL_UP, 0, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mousescrolldownaction") == 0) {
                        snprintf(settings->mouseScrollDownAction,
                                 sizeof(settings->mouseScrollDownAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_WHEEL_DOWN, 0, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mousealtscrollupaction") == 0) {
                        snprintf(settings->mouseAltScrollUpAction,
                                 sizeof(settings->mouseAltScrollUpAction), "%s",
                                 pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_WHEEL_UP, TB_MOD_ALT, get_number(pair->value));
                } else if (strcmp(lowercase_key, "mousealtscrolldownaction") == 0) {
                        snprintf(settings->mouseAltScrollDownAction,
                                 sizeof(settings->mouseAltScrollDownAction),
                                 "%s", pair->value);
                        add_legacy_mouse_binding(TB_KEY_MOUSE_WHEEL_DOWN, TB_MOD_ALT, get_number(pair->value));
                } else if (strcmp(lowercase_key, "hidelogo") == 0) {
                        snprintf(settings->hideLogo, sizeof(settings->hideLogo),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "hidehelp") == 0) {
                        snprintf(settings->hideHelp, sizeof(settings->hideHelp),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "hidefooter") == 0) {
                        snprintf(settings->hideFooter, sizeof(settings->hideFooter),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "hidesidecover") == 0) {
                        snprintf(settings->hideSideCover, sizeof(settings->hideSideCover),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "quitonstop") == 0) {
                        snprintf(settings->quitAfterStopping,
                                 sizeof(settings->quitAfterStopping), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "hideglimmeringtext") == 0) {
                        snprintf(settings->hideGlimmeringText,
                                 sizeof(settings->hideGlimmeringText), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "quit") == 0) {
                        snprintf(settings->quit, sizeof(settings->quit), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_QUIT, pair->value);
                } else if (strcmp(lowercase_key, "altquit") == 0) {
                        snprintf(settings->altQuit, sizeof(settings->altQuit),
                                 "%s", pair->value);
                        add_legacy_key_binding(EVENT_QUIT, pair->value);
                } else if (strcmp(lowercase_key, "prevpage") == 0) {
                        snprintf(settings->prevPage, sizeof(settings->prevPage),
                                 "%s", pair->value);
                        add_legacy_key_binding(EVENT_PREVPAGE, pair->value);
                } else if (strcmp(lowercase_key, "nextpage") == 0) {
                        snprintf(settings->nextPage, sizeof(settings->nextPage),
                                 "%s", pair->value);
                        add_legacy_key_binding(EVENT_NEXTPAGE, pair->value);
                } else if (strcmp(lowercase_key, "updatelibrary") == 0) {
                        snprintf(settings->update_library,
                                 sizeof(settings->update_library), "%s",
                                 pair->value);
                        add_legacy_key_binding(EVENT_UPDATELIBRARY, pair->value);
                } else if (strcmp(lowercase_key, "showplaylistalt") == 0) {
                        if (strcmp(pair->value, "") !=
                            0) // Don't set these to nothing
                                snprintf(settings->showPlaylistAlt,
                                         sizeof(settings->showPlaylistAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWPLAYLIST, pair->value);
                } else if (strcmp(lowercase_key, "showlibraryalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showLibraryAlt,
                                         sizeof(settings->showLibraryAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWLIBRARY, pair->value);
                } else if (strcmp(lowercase_key, "showtrackalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showTrackAlt,
                                         sizeof(settings->showTrackAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWTRACK, pair->value);
                } else if (strcmp(lowercase_key, "showsearchalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showSearchAlt,
                                         sizeof(settings->showSearchAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWSEARCH, pair->value);
                } else if (strcmp(lowercase_key, "showlyricspage") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showLyricsPage,
                                         sizeof(settings->showLyricsPage), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWLYRICSPAGE, pair->value);
                } else if (strcmp(lowercase_key, "movesongup") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->move_song_up,
                                         sizeof(settings->move_song_up), "%s",
                                         pair->value);
                        // Don't add this since this key 't' should be used for themes
                        //add_legacy_key_binding(EVENT_MOVESONGUP, pair->value);
                } else if (strcmp(lowercase_key, "movesongdown") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->move_song_down,
                                         sizeof(settings->move_song_down), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_MOVESONGDOWN, pair->value);
                } else if (strcmp(lowercase_key, "enqueueandplay") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->enqueueAndPlay,
                                         sizeof(settings->enqueueAndPlay), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_ENQUEUEANDPLAY, pair->value);
                } else if (strcmp(lowercase_key, "sort") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->sort_library,
                                         sizeof(settings->sort_library), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SORTLIBRARY, pair->value);
                } else if (strcmp(lowercase_key, "progressbarelapsedevenchar") ==
                           0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarElapsedEvenChar,
                                    sizeof(settings->progressBarElapsedEvenChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key, "progressbarelapsedoddchar") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarElapsedOddChar,
                                    sizeof(settings->progressBarElapsedOddChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key,
                                  "progressbarapproachingevenchar") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarApproachingEvenChar,
                                    sizeof(settings->progressBarApproachingEvenChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key,
                                  "progressbarapproachingoddchar") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarApproachingOddChar,
                                    sizeof(
                                        settings->progressBarApproachingOddChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key, "progressbarcurrentevenchar") ==
                           0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarCurrentEvenChar,
                                    sizeof(settings->progressBarCurrentEvenChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key, "progressbarcurrentoddchar") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(
                                    settings->progressBarCurrentOddChar,
                                    sizeof(settings->progressBarCurrentOddChar),
                                    "%s", pair->value);
                } else if (strcmp(lowercase_key, "showkeysalt") == 0 &&
                           strcmp(pair->value, "N") != 0) {
                        // We need to prevent the previous key B or else config
                        // files wont get updated to the new key N and B for
                        // radio search on macOS

                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showKeysAlt,
                                         sizeof(settings->showKeysAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(EVENT_SHOWHELP, pair->value);
                } else if (strcmp(lowercase_key, "bind") == 0) {
                        char value_copy[256];
                        strncpy(value_copy, pair->value, sizeof(value_copy));
                        value_copy[sizeof(value_copy) - 1] = '\0';
                        char *s = value_copy;

                        char *binding_str = parse_field(&s);
                        char *event_str = parse_field(&s);
                        char *args_str = parse_field(&s);

                        if (binding_str && event_str) {
                                if (!args_str)
                                        args_str = "";

                                TBKeyBinding kb = parse_binding(binding_str, event_str, args_str);
                                add_key_binding(kb);
                        }

                        if (binding_str)
                                free(binding_str);
                        if (event_str)
                                free(event_str);
                        if (args_str && args_str[0] != '\0')
                                free(args_str);
                }

                free(lowercase_key);
        }

        free_key_value_pairs(pairs, count);

        if (!foundCycleThemesSetting) {
                // move_song_up is no longer t, it needs to be changed
                c_strcpy(settings->move_song_up, "f", sizeof(settings->move_song_up));
        }
}

KeyValuePair *read_key_value_pairs(const char *file_path, int *count,
                                   time_t *last_time_app_ran)
{
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
                return NULL;
        }

        struct stat file_stat;
        if (stat(file_path, &file_stat) == -1) {
                return NULL;
        }

        // Save the modification time (mtime) of the file
        *last_time_app_ran = file_stat.st_mtime;

        KeyValuePair *pairs = NULL;
        int pair_count = 0;

        char line[256];
        while (fgets(line, sizeof(line), file)) {
                // Remove trailing newline character if present
                line[strcspn(line, "\n")] = '\0';

                char *comment = strchr(line, '#');

                // Remove comments
                if (comment != NULL)
                        *comment = '\0';

                char *delimiter = strchr(line, '=');

                if (delimiter != NULL) {
                        *delimiter = '\0';
                        char *value = delimiter + 1;

                        pair_count++;
                        pairs =
                            realloc(pairs, pair_count * sizeof(KeyValuePair));
                        KeyValuePair *current_pair = &pairs[pair_count - 1];
                        current_pair->key = strdup(line);
                        current_pair->value = strdup(value);
                }
        }

        fclose(file);

        *count = pair_count;
        return pairs;
}

const char *get_default_music_folder(void)
{
        const char *home = get_home_path();
        if (home != NULL) {
                static char music_path[PATH_MAX];
                snprintf(music_path, sizeof(music_path), "%s/Music", home);
                return music_path;
        } else {
                return NULL; // Return NULL if XDG home is not found.
        }
}

int get_music_library_path(char *path)
{
        char expanded_path[PATH_MAX];

        if (path[0] != '\0' && path[0] != '\r') {
                if (expand_path(path, expanded_path) >= 0) {
                        c_strcpy(path, expanded_path, sizeof(expanded_path));
                }
        }

        return 0;
}

void map_settings_to_keys(AppSettings *settings, EventMapping *mappings)
{
        mappings[0] = (EventMapping){settings->scrollUpAlt, EVENT_SCROLLUP};
        mappings[1] = (EventMapping){settings->scrollDownAlt, EVENT_SCROLLDOWN};
        mappings[2] = (EventMapping){settings->nextTrackAlt, EVENT_NEXT};
        mappings[3] = (EventMapping){settings->previousTrackAlt, EVENT_PREV};
        mappings[4] = (EventMapping){settings->volumeUp, EVENT_VOLUME_UP};
        mappings[5] = (EventMapping){settings->volumeUpAlt, EVENT_VOLUME_UP};
        mappings[6] = (EventMapping){settings->volumeDown, EVENT_VOLUME_DOWN};
        mappings[7] = (EventMapping){settings->toggle_pause, EVENT_PLAY_PAUSE};
        mappings[8] = (EventMapping){settings->quit, EVENT_QUIT};
        mappings[9] = (EventMapping){settings->altQuit, EVENT_QUIT};
        mappings[10] = (EventMapping){settings->toggle_shuffle, EVENT_SHUFFLE};
        mappings[11] = (EventMapping){settings->toggle_visualizer, EVENT_TOGGLEVISUALIZER};
        mappings[12] = (EventMapping){settings->toggle_ascii, EVENT_TOGGLEASCII};
        mappings[13] = (EventMapping){settings->switchNumberedSong, EVENT_ENQUEUE};
        mappings[14] = (EventMapping){settings->seekBackward, EVENT_SEEKBACK};
        mappings[15] = (EventMapping){settings->seek_forward, EVENT_SEEKFORWARD};
        mappings[16] = (EventMapping){settings->toggle_repeat, EVENT_TOGGLEREPEAT};
        mappings[17] = (EventMapping){settings->save_playlist, EVENT_EXPORTPLAYLIST};
        mappings[18] = (EventMapping){settings->cycleColorsDerivedFrom, EVENT_CYCLECOLORMODE};
        mappings[19] = (EventMapping){settings->add_to_favorites_playlist, EVENT_ADDTOFAVORITESPLAYLIST};
        mappings[20] = (EventMapping){settings->update_library, EVENT_UPDATELIBRARY};
        mappings[21] = (EventMapping){settings->hardPlayPause, EVENT_PLAY_PAUSE};
        mappings[22] = (EventMapping){settings->hardPrev, EVENT_PREV};
        mappings[23] = (EventMapping){settings->hardNext, EVENT_NEXT};
        mappings[24] = (EventMapping){settings->hardSwitchNumberedSong, EVENT_ENQUEUE};
        mappings[25] = (EventMapping){settings->hardScrollUp, EVENT_SCROLLUP};
        mappings[26] = (EventMapping){settings->hardScrollDown, EVENT_SCROLLDOWN};
        mappings[27] = (EventMapping){settings->hardShowPlaylist, EVENT_SHOWPLAYLIST};
        mappings[28] = (EventMapping){settings->hardShowPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[29] = (EventMapping){settings->showPlaylistAlt, EVENT_SHOWPLAYLIST};
        mappings[30] = (EventMapping){settings->hardShowKeys, EVENT_SHOWHELP};
        mappings[31] = (EventMapping){settings->hardShowKeysAlt, EVENT_SHOWHELP};
        mappings[32] = (EventMapping){settings->showKeysAlt, EVENT_SHOWHELP};
        mappings[33] = (EventMapping){settings->hardShowTrack, EVENT_SHOWTRACK};
        mappings[34] = (EventMapping){settings->hardShowTrackAlt, EVENT_SHOWTRACK};
        mappings[35] = (EventMapping){settings->showTrackAlt, EVENT_SHOWTRACK};
        mappings[36] = (EventMapping){settings->hardShowLibrary, EVENT_SHOWLIBRARY};
        mappings[37] = (EventMapping){settings->hardShowLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[38] = (EventMapping){settings->showLibraryAlt, EVENT_SHOWLIBRARY};
        mappings[39] = (EventMapping){settings->hardShowSearch, EVENT_SHOWSEARCH};
        mappings[40] = (EventMapping){settings->hardShowSearchAlt, EVENT_SHOWSEARCH};
        mappings[41] = (EventMapping){settings->showSearchAlt, EVENT_SHOWSEARCH};
        mappings[42] = (EventMapping){settings->nextPage, EVENT_NEXTPAGE};
        mappings[43] = (EventMapping){settings->prevPage, EVENT_PREVPAGE};
        mappings[44] = (EventMapping){settings->hardRemove, EVENT_REMOVE};
        mappings[45] = (EventMapping){settings->hardRemove2, EVENT_REMOVE};
        mappings[46] = (EventMapping){settings->nextView, EVENT_NEXTVIEW};
        mappings[47] = (EventMapping){settings->prevView, EVENT_PREVVIEW};
        mappings[55] = (EventMapping){settings->hardClearPlaylist, EVENT_CLEARPLAYLIST};
        mappings[56] = (EventMapping){settings->move_song_up, EVENT_MOVESONGUP};
        mappings[57] = (EventMapping){settings->move_song_down, EVENT_MOVESONGDOWN};
        mappings[58] = (EventMapping){settings->enqueueAndPlay, EVENT_ENQUEUEANDPLAY};
        mappings[59] = (EventMapping){settings->hardStop, EVENT_STOP};
        mappings[60] = (EventMapping){settings->sort_library, EVENT_SORTLIBRARY};
        mappings[61] = (EventMapping){settings->cycle_themes, EVENT_CYCLETHEMES};
        mappings[62] = (EventMapping){settings->toggle_notifications, EVENT_TOGGLENOTIFICATIONS};
        mappings[63] = (EventMapping){settings->showLyricsPage, EVENT_SHOWLYRICSPAGE};
}

char *get_config_file_path(char *configdir)
{
        size_t configdir_length = strnlen(configdir, PATH_MAX - 1);
        size_t settings_file_length =
            strnlen(SETTINGS_FILE, sizeof(SETTINGS_FILE) - 1);

        if (configdir_length + 1 + settings_file_length + 1 > PATH_MAX) {
                fprintf(stderr, "Error: File path exceeds maximum length.\n");
                quit();
        }

        char *filepath = (char *)malloc(PATH_MAX);
        if (filepath == NULL) {
                perror("malloc");
                quit();
        }

        int written =
            snprintf(filepath, PATH_MAX, "%s/%s", configdir, SETTINGS_FILE);
        if (written < 0 || written >= PATH_MAX) {
                fprintf(stderr,
                        "Error: snprintf failed or filepath truncated.\n");
                free(filepath);
                quit();
        }
        return filepath;
}

char *get_prefs_file_path(char *prefsdir)
{
        size_t dir_length = strnlen(prefsdir, PATH_MAX - 1);
        size_t file_length = strnlen(STATE_FILE, sizeof(STATE_FILE) - 1);

        if (dir_length + 1 + file_length + 1 > PATH_MAX) {
                fprintf(stderr, "Error: File path exceeds maximum length.\n");
                quit();
        }

        char *filepath = (char *)malloc(PATH_MAX);
        if (filepath == NULL) {
                perror("malloc");
                quit();
        }

        int written = snprintf(filepath, PATH_MAX, "%s/%s", prefsdir, STATE_FILE);
        if (written < 0 || written >= PATH_MAX) {
                fprintf(stderr, "Error: snprintf failed or filepath truncated.\n");
                free(filepath);
                quit();
        }

        return filepath;
}

int mkdir_p(const char *path, mode_t mode)
{
        if (path == NULL)
                return -1;

        if (path[0] == '~') {
                // Just try a plain mkdir if there's a tilde
                if (mkdir(path, mode) == -1) {
                        if (errno != EEXIST)
                                return -1;
                }
                return 0;
        }

        char tmp[PATH_MAX];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp), "%s", path);
        len = strnlen(tmp, PATH_MAX);
        if (len > 0 && tmp[len - 1] == '/')
                tmp[len - 1] = 0;

        for (p = tmp + 1; *p; p++) {
                if (*p == '/') {
                        *p = 0;
                        if (mkdir(tmp, mode) == -1) {
                                if (errno != EEXIST)
                                        return -1;
                        }
                        *p = '/';
                }
        }
        if (mkdir(tmp, mode) == -1) {
                if (errno != EEXIST)
                        return -1;
        }
        return 0;
}

void migrate_prefs_file(char *new_filepath)
{
        char *prefs_dir = get_prefs_path();
        char *prefs_file_old = get_prefs_file_path(prefs_dir);

        struct stat nfile = {0};
        struct stat ofile = {0};
        if (stat(new_filepath, &nfile) == -1 && stat(prefs_file_old, &ofile) == 0) {
                if (rename(prefs_file_old, new_filepath) != 0) {
                        perror("rename");
                        free(prefs_file_old);
                        free(prefs_dir);
                        quit();
                }
        }

        free(prefs_file_old);
        free(prefs_dir);
        return;
}

void get_prefs(AppSettings *settings, UISettings *ui)
{
        int pair_count;
        char *configdir = get_config_path();

        setlocale(LC_ALL, "");

        struct stat st = {0};
        if (stat(configdir, &st) == -1) {
                if (mkdir_p(configdir, 0700) != 0) {
                        perror("mkdir");
                        free(configdir);
                        quit();
                }
        }

        char *filepath = get_prefs_file_path(configdir);

        // Move legacy state file to new location
        migrate_prefs_file(filepath);

        KeyValuePair *pairs =
            read_key_value_pairs(filepath, &pair_count, &(ui->last_time_app_ran));

        free(filepath);
        construct_app_settings(settings, pairs, pair_count, true);

        int tmp = get_number(settings->repeatState);
        if (tmp >= 0)
                ui->repeatState = tmp;

        if (settings->chromaPreset[0] != '\0') {
                tmp = get_number(settings->chromaPreset);

                if (tmp >= 0)
                        ui->chromaPreset = tmp;
        }

        tmp = get_number(settings->colorMode);
        if (tmp >= 0 && tmp < 3) {
                ui->colorMode = tmp;
        }

        tmp = get_number(settings->lastVolume);
        if (tmp >= 0)
                set_volume(tmp);

        snprintf(ui->theme_name, sizeof(ui->theme_name), "%s", settings->theme);
        free(configdir);
}

void get_config(AppSettings *settings, UISettings *ui)
{
        int pair_count;
        char *configdir = get_config_path();

        setlocale(LC_ALL, "");

        struct stat st = {0};
        if (stat(configdir, &st) == -1) {
                if (mkdir_p(configdir, 0700) != 0) {
                        perror("mkdir");
                        quit();
                }
        }

        set_default_config(settings);

        char *filepath = get_config_file_path(configdir);

        FILE *file = fopen(filepath, "r");
        if (file == NULL) {
                set_config(settings, ui);
        }

        KeyValuePair *pairs =
            read_key_value_pairs(filepath, &pair_count, &(ui->last_time_app_ran));

        free(filepath);

        construct_app_settings(settings, pairs, pair_count, false);

        free(configdir);
}

void set_prefs(AppSettings *settings, UISettings *ui)
{
        // Create the file path
        char *configdir = get_config_path();
        char *filepath = get_prefs_file_path(configdir);

        setlocale(LC_ALL, "");

        FILE *file = fopen(filepath, "w");
        if (file == NULL) {
                fprintf(stderr, "Error opening file: %s\n", filepath);
                free(filepath);
                free(configdir);
                return;
        }

        if (settings->allowNotifications[0] == '\0')
                ui->allowNotifications
                    ? c_strcpy(settings->allowNotifications, "1",
                               sizeof(settings->allowNotifications))
                    : c_strcpy(settings->allowNotifications, "0",
                               sizeof(settings->allowNotifications));

        if (settings->coverEnabled[0] == '\0')
                ui->coverEnabled ? c_strcpy(settings->coverEnabled, "1",
                                            sizeof(settings->coverEnabled))
                                 : c_strcpy(settings->coverEnabled, "0",
                                            sizeof(settings->coverEnabled));
        if (settings->coverAnsi[0] == '\0')
                ui->coverAnsi ? c_strcpy(settings->coverAnsi, "1",
                                         sizeof(settings->coverAnsi))
                              : c_strcpy(settings->coverAnsi, "0",
                                         sizeof(settings->coverAnsi));
        if (settings->visualizerEnabled[0] == '\0')
                ui->visualizerEnabled
                    ? c_strcpy(settings->visualizerEnabled, "1",
                               sizeof(settings->visualizerEnabled))
                    : c_strcpy(settings->visualizerEnabled, "0",
                               sizeof(settings->visualizerEnabled));

        if (ui->saveRepeatShuffleSettings) {
                snprintf(settings->repeatState, sizeof(settings->repeatState), "%d",
                         ui->repeatState);

                ui->shuffle_enabled ? c_strcpy(settings->shuffle_enabled, "1",
                                               sizeof(settings->shuffle_enabled))
                                    : c_strcpy(settings->shuffle_enabled, "0",
                                               sizeof(settings->shuffle_enabled));
        }

        if (settings->visualizer_color_type[0] == '\0')
                snprintf(settings->visualizer_color_type,
                         sizeof(settings->visualizer_color_type), "%d",
                         ui->visualizer_color_type);

        int current_volume = get_current_volume();
        current_volume = (current_volume <= 0) ? 10 : current_volume;
        snprintf(settings->lastVolume, sizeof(settings->lastVolume), "%d",
                 current_volume);

        fprintf(file, "\n[miscellaneous]\n\n");
        fprintf(file, "allowNotifications=%s\n\n", settings->allowNotifications);

        if (ui->saveRepeatShuffleSettings) {
                fprintf(file, "repeatState=%s\n\n", settings->repeatState);
                fprintf(file, "shuffleEnabled=%s\n\n", settings->shuffle_enabled);
        }

        fprintf(file, "lastVolume=%s\n\n", settings->lastVolume);
        fprintf(file, "[track cover]\n\n");
        fprintf(file, "coverAnsi=%s\n\n", settings->coverAnsi);
        fprintf(file, "[visualizer]\n\n");
        fprintf(file, "visualizerEnabled=%s\n\n", settings->visualizerEnabled);
        fprintf(file, "[chroma]\n\n");
        fprintf(file, "chromaPreset=%d\n\n", ui->chromaPreset);
        fprintf(file, "[colors]\n\n");
        fprintf(file, "colorMode=%d\n\n", ui->colorMode);
        fprintf(file, "theme=%s\n\n", ui->theme_name);

        free(configdir);
        free(filepath);
}

static const char *key_binding_to_str(const TBKeyBinding kb)
{
        static char buf[128];
        buf[0] = '\0';

        // Modifiers
        if (kb.mods & TB_MOD_CTRL)
                strcat(buf, "Ctrl+");
        if (kb.mods & TB_MOD_ALT)
                strcat(buf, "Alt+");
        if (kb.mods & TB_MOD_SHIFT)
                strcat(buf, "Shift+");

        // Key
        if (kb.key != 0) {
                // Special key
                const char *key_name = key_code_to_name(kb.key);
                if (key_name)
                        strcat(buf, key_name);
                else
                        sprintf(buf + strlen(buf), "0x%x", kb.key);
        } else if (kb.ch != 0) {
                // Printable character
                size_t len = strlen(buf);
                buf[len] = (char)kb.ch;
                buf[len + 1] = '\0';
        } else {
                strcat(buf, "Unknown");
        }

        // Event
        const char *event_name = event_to_string(kb.eventType);

        // Final Format
        static char final[256];
        if (kb.args[0])
                snprintf(final, sizeof(final), "%s, %s, %s", buf, event_name, kb.args);
        else
                snprintf(final, sizeof(final), "%s, %s", buf, event_name);

        return final;
}

void set_config(AppSettings *settings, UISettings *ui)
{
        // Create the file path
        char *configdir = get_config_path();
        char *filepath = get_config_file_path(configdir);

        setlocale(LC_ALL, "");

        FILE *file = fopen(filepath, "w");
        if (file == NULL) {
                fprintf(stderr, "Error opening file: %s\n", filepath);
                free(filepath);
                free(configdir);
                return;
        }

        // Make sure strings are valid before writing settings to the file
        if (settings->allowNotifications[0] == '\0')
                ui->allowNotifications
                    ? c_strcpy(settings->allowNotifications, "1",
                               sizeof(settings->allowNotifications))
                    : c_strcpy(settings->allowNotifications, "0",
                               sizeof(settings->allowNotifications));

        if (settings->stripTrackNumbers[0] == '\0')
                ui->stripTrackNumbers
                    ? c_strcpy(settings->stripTrackNumbers, "1",
                               sizeof(settings->stripTrackNumbers))
                    : c_strcpy(settings->stripTrackNumbers, "0",
                               sizeof(settings->stripTrackNumbers));
        if (settings->coverEnabled[0] == '\0')
                ui->coverEnabled ? c_strcpy(settings->coverEnabled, "1",
                                            sizeof(settings->coverEnabled))
                                 : c_strcpy(settings->coverEnabled, "0",
                                            sizeof(settings->coverEnabled));
        if (settings->coverAnsi[0] == '\0')
                ui->coverAnsi ? c_strcpy(settings->coverAnsi, "1",
                                         sizeof(settings->coverAnsi))
                              : c_strcpy(settings->coverAnsi, "0",
                                         sizeof(settings->coverAnsi));
        if (settings->visualizerEnabled[0] == '\0')
                ui->visualizerEnabled
                    ? c_strcpy(settings->visualizerEnabled, "1",
                               sizeof(settings->visualizerEnabled))
                    : c_strcpy(settings->visualizerEnabled, "0",
                               sizeof(settings->visualizerEnabled));
        if (settings->hideTimeStatus[0] == '\0')
                ui->hideTimeStatus
                    ? c_strcpy(settings->hideTimeStatus, "1",
                               sizeof(settings->hideTimeStatus))
                    : c_strcpy(settings->hideTimeStatus, "0",
                               sizeof(settings->hideTimeStatus));
        if (settings->discordRPCEnabled[0] == '\0')
                ui->discordRPCEnabled
                    ? c_strcpy(settings->discordRPCEnabled, "1",
                               sizeof(settings->discordRPCEnabled))
                    : c_strcpy(settings->discordRPCEnabled, "0",
                               sizeof(settings->discordRPCEnabled));

        if (settings->quitAfterStopping[0] == '\0')
                ui->quitAfterStopping
                    ? c_strcpy(settings->quitAfterStopping, "1",
                               sizeof(settings->quitAfterStopping))
                    : c_strcpy(settings->quitAfterStopping, "0",
                               sizeof(settings->quitAfterStopping));
        if (settings->hideGlimmeringText[0] == '\0')
                ui->hideGlimmeringText
                    ? c_strcpy(settings->hideGlimmeringText, "1",
                               sizeof(settings->hideGlimmeringText))
                    : c_strcpy(settings->hideGlimmeringText, "0",
                               sizeof(settings->hideGlimmeringText));
        if (settings->mouseEnabled[0] == '\0')
                ui->mouseEnabled ? c_strcpy(settings->mouseEnabled, "1",
                                            sizeof(settings->mouseEnabled))
                                 : c_strcpy(settings->mouseEnabled, "0",
                                            sizeof(settings->mouseEnabled));
        if (settings->trackTitleAsWindowTitle[0] == '\0')
                ui->trackTitleAsWindowTitle
                    ? c_strcpy(settings->trackTitleAsWindowTitle, "1",
                               sizeof(settings->trackTitleAsWindowTitle))
                    : c_strcpy(settings->trackTitleAsWindowTitle, "0",
                               sizeof(settings->trackTitleAsWindowTitle));

        snprintf(settings->repeatState, sizeof(settings->repeatState), "%d",
                 ui->repeatState);

        ui->shuffle_enabled ? c_strcpy(settings->shuffle_enabled, "1",
                                       sizeof(settings->shuffle_enabled))
                            : c_strcpy(settings->shuffle_enabled, "0",
                                       sizeof(settings->shuffle_enabled));

        if (settings->visualizer_bar_width[0] == '\0')
                snprintf(settings->visualizer_bar_width,
                         sizeof(settings->visualizer_bar_width), "%d",
                         ui->visualizer_bar_mode);

        if (settings->visualizerBrailleMode[0] == '\0')
                ui->visualizerBrailleMode
                    ? c_strcpy(settings->visualizerBrailleMode, "1",
                               sizeof(settings->visualizerBrailleMode))
                    : c_strcpy(settings->visualizerBrailleMode, "0",
                               sizeof(settings->visualizerBrailleMode));
        if (settings->hideLogo[0] == '\0')
                ui->hideLogo ? c_strcpy(settings->hideLogo, "1",
                                        sizeof(settings->hideLogo))
                             : c_strcpy(settings->hideLogo, "0",
                                        sizeof(settings->hideLogo));
        if (settings->hideHelp[0] == '\0')
                ui->hideHelp ? c_strcpy(settings->hideHelp, "1",
                                        sizeof(settings->hideHelp))
                             : c_strcpy(settings->hideHelp, "0",
                                        sizeof(settings->hideHelp));
        if (settings->hideFooter[0] == '\0')
                ui->hideFooter ? c_strcpy(settings->hideFooter, "1",
                                          sizeof(settings->hideFooter))
                               : c_strcpy(settings->hideFooter, "0",
                                          sizeof(settings->hideFooter));
        if (settings->hideSideCover[0] == '\0')
                ui->hideSideCover ? c_strcpy(settings->hideSideCover, "1",
                                             sizeof(settings->hideSideCover))
                                  : c_strcpy(settings->hideSideCover, "0",
                                             sizeof(settings->hideSideCover));
        if (settings->visualizer_height[0] == '\0')
                snprintf(settings->visualizer_height,
                         sizeof(settings->visualizer_height), "%d",
                         ui->visualizer_height);
        if (settings->visualizer_color_type[0] == '\0')
                snprintf(settings->visualizer_color_type,
                         sizeof(settings->visualizer_color_type), "%d",
                         ui->visualizer_color_type);
        if (settings->titleDelay[0] == '\0')
                snprintf(settings->titleDelay, sizeof(settings->titleDelay),
                         "%d", ui->titleDelay);

        if (settings->replayGainCheckFirst[0] == '\0')
                snprintf(settings->replayGainCheckFirst,
                         sizeof(settings->replayGainCheckFirst), "%d",
                         ui->replayGainCheckFirst);

        // Write the settings to the file
        fprintf(file, "\n[miscellaneous]\n\n");
        fprintf(file, "path=%s\n", settings->path);
        fprintf(file, "allowNotifications=%s\n", settings->allowNotifications);
        fprintf(file, "stripTrackNumbers=%s\n", settings->stripTrackNumbers);
        fprintf(file, "hideLogo=%s\n", settings->hideLogo);
        fprintf(file, "hideHelp=%s\n", settings->hideHelp);
        fprintf(file, "hideTimeStatus=%s\n", settings->hideTimeStatus);
        fprintf(file, "hideFooter=%s\n", settings->hideFooter);
        fprintf(file, "hideSideCover=%s\n\n", settings->hideSideCover);

        fprintf(file, "# Delay when drawing title in track view, set to 0 to "
                      "have no delay.\n");
        fprintf(file, "titleDelay=%s\n\n", settings->titleDelay);

        fprintf(file, "# Same as '--quitonstop' flag, exits after playing the "
                      "whole playlist.\n");
        fprintf(file, "quitOnStop=%s\n\n", settings->quitAfterStopping);

        fprintf(file, "# Glimmering text on the bottom row.\n");
        fprintf(file, "hideGlimmeringText=%s\n\n",
                settings->hideGlimmeringText);

        fprintf(file, "# Replay gain check first, can be either 0=track, "
                      "1=album or 2=disabled.\n");
        fprintf(file, "replayGainCheckFirst=%s\n\n",
                settings->replayGainCheckFirst);

        fprintf(file, "# Save Repeat and Shuffle Settings.\n");
        fprintf(file, "saveRepeatShuffleSettings=%s\n\n",
                settings->saveRepeatShuffleSettings);

        fprintf(file, "repeatState=%s\n\n", settings->repeatState);
        fprintf(file, "shuffleEnabled=%s\n\n", settings->shuffle_enabled);

        fprintf(file, "# Set the window title to the title of the currently "
                      "playing track\n");
        fprintf(file, "trackTitleAsWindowTitle=%s\n\n",
                settings->trackTitleAsWindowTitle);

        fprintf(file, "\n[colors]\n\n");

        fprintf(file, "# Theme's go in ~/.config/kew/themes (on Linux/FreeBSD/Android), \n");
        fprintf(file, "# and ~/Library/Preferences/kew/themes (on macOS), \n");
        fprintf(file, "theme=%s\n\n", settings->theme);

        fprintf(file, "# Color Mode is:\n");
        fprintf(file, "# 0 = 16-bit color palette from default theme, \n");
        fprintf(file, "# 1 = Colors derived from track cover, \n");
        fprintf(file, "# 2 = Colors derived from TrueColor theme, \n\n");
        fprintf(file, "# Color Mode:\n");
        fprintf(file, "colorMode=%d\n\n", ui->colorMode);

        fprintf(file, "# Terminal color theme is default.theme in \n");
        fprintf(file, "# ~/.config/kew/themes (on Linux/FreeBSD/Android), \n");
        fprintf(file, "# and ~/Library/Preferences/kew/themes (on macOS).\n\n");

        fprintf(file, "\n[track cover]\n\n");
        fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
        fprintf(file, "coverAnsi=%s\n\n", settings->coverAnsi);

        fprintf(file, "\n[mouse]\n\n");
        fprintf(file, "mouseEnabled=%s\n\n", settings->mouseEnabled);

        fprintf(file, "\n[discord]\n\n");
        fprintf(file, "discordRPCEnabled=%s\n\n”", settings->discordRPCEnabled);

        fprintf(file, "\n[visualizer]\n\n");
        fprintf(file, "visualizerEnabled=%s\n", settings->visualizerEnabled);
        fprintf(file, "visualizerHeight=%s\n", settings->visualizer_height);
        fprintf(file, "visualizerBrailleMode=%s\n\n",
                settings->visualizerBrailleMode);

        fprintf(file, "# How colors are laid out in the spectrum visualizer. "
                      "0=lighten, 1=brightness depending on bar height, "
                      "2=reversed, 3=reversed darken.\n");
        fprintf(file, "visualizerColorType=%s\n\n",
                settings->visualizer_color_type);

        fprintf(file, "# 0=Thin bars, 1=Bars twice the width, 2=Auto (depends "
                      "on window size).\n");
        fprintf(file, "visualizerBarWidth=%s\n\n",
                settings->visualizer_bar_width);

        fprintf(file, "\n[progress bar]\n\n");

        fprintf(file, "# Progress bar in track view\n");
        fprintf(file, "# The progress bar can be configured in many ways.\n");
        fprintf(file,
                "# When copying the values below, be sure to include values "
                "that are empty spaces or things will get messed up.\n");
        fprintf(file,
                "# Be sure to have the actual uncommented values last.\n");
        fprintf(
            file,
            "# For instance use the below values for a pill muncher mode:\n\n");

        fprintf(file, "#progressBarElapsedEvenChar= \n");
        fprintf(file, "#progressBarElapsedOddChar= \n");
        fprintf(file, "#progressBarApproachingEvenChar=•\n");
        fprintf(file, "#progressBarApproachingOddChar=·\n");
        fprintf(file, "#progressBarCurrentEvenChar=ᗧ\n");
        fprintf(file, "#progressBarCurrentOddChar=ᗧ\n\n");

        fprintf(file, "# To have a thick line: \n\n");

        fprintf(file, "#progressBarElapsedEvenChar=━\n");
        fprintf(file, "#progressBarElapsedOddChar=━\n");
        fprintf(file, "#progressBarApproachingEvenChar=━\n");
        fprintf(file, "#progressBarApproachingOddChar=━\n");
        fprintf(file, "#progressBarCurrentEvenChar=━\n");
        fprintf(file, "#progressBarCurrentOddChar=━\n\n");

        fprintf(file, "# To have dots (the original): \n\n");

        fprintf(file, "#progressBarElapsedEvenChar=■\n");
        fprintf(file, "#progressBarElapsedOddChar= \n");
        fprintf(file, "#progressBarApproachingEvenChar==\n");
        fprintf(file, "#progressBarApproachingOddChar= \n");
        fprintf(file, "#progressBarCurrentEvenChar=■\n");
        fprintf(file, "#progressBarCurrentOddChar= \n\n");

        fprintf(file, "# Current values: \n\n");

        fprintf(file, "progressBarElapsedEvenChar=%s\n",
                settings->progressBarElapsedEvenChar);
        fprintf(file, "progressBarElapsedOddChar=%s\n",
                settings->progressBarElapsedOddChar);
        fprintf(file, "progressBarApproachingEvenChar=%s\n",
                settings->progressBarApproachingEvenChar);
        fprintf(file, "progressBarApproachingOddChar=%s\n",
                settings->progressBarApproachingOddChar);
        fprintf(file, "progressBarCurrentEvenChar=%s\n",
                settings->progressBarCurrentEvenChar);
        fprintf(file, "progressBarCurrentOddChar=%s\n\n",
                settings->progressBarCurrentOddChar);

        fprintf(file, "\n[key bindings]\n\n");

        for (size_t i = 0; i < keybinding_count; i++) {
                fprintf(file, "bind = ");

                fprintf(file, "%s\n", key_binding_to_str(key_bindings[i]));
        }

        fprintf(file, "\n");
        fclose(file);
        free(filepath);
        free(configdir);
}

int update_rc(const char *path, const char *key, const char *value)
{
        FILE *file = fopen(path, "r");
        if (!file) {
                perror("fopen");
                return -1;
        }

        char **lines = NULL;
        size_t num_lines = 0;
        char line[MAX_LINE];
        int found = 0;

        while (fgets(line, sizeof(line), file)) {
                // Remove trailing newline
                line[strcspn(line, "\n")] = '\0';

                char *eq = strchr(line, '=');
                char *newline;
                if (eq) {
                        *eq = '\0';
                        char *existing_key = line;
                        char *existing_value = eq + 1;

                        if (strcmp(existing_key, key) == 0) {
                                // Replace value
                                size_t needed = strlen(key) + strlen(value) + 2;
                                newline = malloc(needed);
                                if (!newline) {
                                        fclose(file);
                                        return -1;
                                }
                                snprintf(newline, needed, "%s=%s", key, value);
                                found = 1;
                        } else {
                                // Keep original line
                                newline = malloc(strlen(line) + strlen(existing_value) + 2);
                                snprintf(newline, strlen(line) + strlen(existing_value) + 2, "%s=%s", existing_key, existing_value);
                        }
                } else {
                        // Line without '=' or empty line
                        newline = strdup(line);
                }

                lines = realloc(lines, sizeof(char *) * (num_lines + 1));
                lines[num_lines++] = newline;
        }

        fclose(file);

        if (!found) {
                // Append new key=value
                size_t needed = strlen(key) + strlen(value) + 2;
                char *newline = malloc(needed);
                snprintf(newline, needed, "%s=%s", key, value);
                lines = realloc(lines, sizeof(char *) * (num_lines + 1));
                lines[num_lines++] = newline;
        }

        // Write back
        file = fopen(path, "w");
        if (!file) {
                perror("fopen");
                return -1;
        }

        for (size_t i = 0; i < num_lines; i++) {
                fprintf(file, "%s\n", lines[i]);
                free(lines[i]);
        }
        free(lines);
        fclose(file);

        return 0;
}

void set_path(const char *path)
{
        char *configdir = NULL;
        char *config_file_path = NULL;

        configdir = get_config_path();
        config_file_path = get_config_file_path(configdir);

        update_rc(config_file_path, "path", path);

        if (configdir != NULL)
                free(configdir);

        if (config_file_path != NULL)
                free(config_file_path);
}
