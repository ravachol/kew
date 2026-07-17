/**
 * @file settings.c
 * @brief Application settings management.
 *
 * Loads, saves, and applies configuration settings such as
 * playback behavior, UI preferences, and library paths.
 */

#include "settings.h"
#include "common/model.h"
#include "components.h"

#include "common_ui.h"

#include "common/common.h"
#include "common/events.h"

#include "data/directorytree.h"
#include "termbox2_input.h"

#include "common/appstate.h"

#include "ops/playback_state.h"

#include "utils/file.h"
#include "utils/utils.h"
#include "utils/k_log.h"

#include "update/messages.h"

#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

const char SETTINGS_FILE[] = "kewrc";
const char STATE_FILE[] = "kewstaterc";
const char LAYOUT_FILE[] = "/layouts/current.layout";

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
    {TB_KEY_SPACE, 0, 0, MSG_PLAY_PAUSE, ""},
    // Basic navigation
    {TB_KEY_TAB, 0, TB_MOD_SHIFT, MSG_PREVVIEW, ""},
    {TB_KEY_TAB, 0, 0, MSG_NEXTVIEW, ""},

    // volume
    {0, '+', 0, MSG_VOLUME_UP, "+5%"},
    {0, '=', 0, MSG_VOLUME_UP, "+5%"},
    {0, '-', 0, MSG_VOLUME_DOWN, "-5%"},

    // Tracks
    {0, 'h', 0, MSG_PREV, ""},
    {0, 'l', 0, MSG_NEXT, ""},
    {0, 'k', 0, MSG_SCROLLUP, ""},
    {0, 'j', 0, MSG_SCROLLDOWN, ""},

    // Controls
    {0, 'p', 0, MSG_PLAY_PAUSE, ""},
    {0, 'n', 0, MSG_TOGGLENOTIFICATIONS, ""},
    {0, 'z', 0, MSG_TOGGLECROSSFADE, ""},
    {0, 'v', 0, MSG_CYCLEVISUALIZERMODE, ""},
    {0, 'b', 0, MSG_TOGGLEASCII, ""},
    {0, 'r', 0, MSG_TOGGLEREPEAT, ""},
    {0, 'i', 0, MSG_CYCLECOLORMODE, ""},
    {0, 't', 0, MSG_CYCLETHEMES, ""},
    {0, 'c', 0, MSG_CYCLEVISUALIZATION, ""},
    {0, 's', 0, MSG_SHUFFLE, ""},
    {0, 'a', 0, MSG_SEEKBACK, ""},
    {0, 'd', 0, MSG_SEEKFORWARD, ""},
    {0, 'o', 0, MSG_SORTLIBRARY, ""},
    {0, 'm', 0, MSG_SHOWLYRICSPAGE, ""},
    {0, 's', TB_MOD_SHIFT, MSG_STOP, ""},

    // Playlist actions
    {0, 'x', 0, MSG_SAVEPLAYLIST, ""},
    {0, '.', 0, MSG_ADDTOFAVORITESPLAYLIST, ""},
    {0, 'u', 0, MSG_UPDATELIBRARY, ""},
    {0, 'f', 0, MSG_MOVESONGUP, ""},
    {0, 'g', 0, MSG_MOVESONGDOWN, ""},

    {TB_KEY_ENTER, 0, 0, MSG_ENQUEUE, ""},
    {TB_KEY_BACKSPACE, 0, 0, MSG_CLEARPLAYLIST, ""},

    {TB_KEY_CTRL_G, 0, TB_MOD_CTRL, MSG_ENQUEUEANDPLAY, ""},

    {TB_KEY_ENTER, 0, TB_MOD_ALT, MSG_ENQUEUEANDPLAY, ""},


    // Hard navigation / arrows
    {TB_KEY_ARROW_LEFT, 0, 0, MSG_PREV, ""},
    {TB_KEY_ARROW_RIGHT, 0, 0, MSG_NEXT, ""},
    {TB_KEY_ARROW_UP, 0, 0, MSG_SCROLLUP, ""},
    {TB_KEY_ARROW_DOWN, 0, 0, MSG_SCROLLDOWN, ""},

#if defined(__ANDROID__) || defined(__APPLE__)

    // Show Views macOS/Android
    {0, 'z', TB_MOD_SHIFT, MSG_SHOWPLAYLIST, ""},
    {0, 'x', TB_MOD_SHIFT, MSG_SHOWLIBRARY, ""},
    {0, 'c', TB_MOD_SHIFT, MSG_SHOWTRACK, ""},
    {0, 'v', TB_MOD_SHIFT, MSG_SHOWSEARCH, ""},
    {0, 'b', TB_MOD_SHIFT, MSG_SHOWHELP, ""},
#else
    // Show Views
    {TB_KEY_F2, 0, 0, MSG_SHOWPLAYLIST, ""},
    {TB_KEY_F3, 0, 0, MSG_SHOWLIBRARY, ""},
    {TB_KEY_F4, 0, 0, MSG_SHOWTRACK, ""},
    {TB_KEY_F5, 0, 0, MSG_SHOWSEARCH, ""},
    {TB_KEY_F6, 0, 0, MSG_SHOWHELP, ""},
#endif

    // Crossfade keys
    {0, 'd', TB_MOD_SHIFT, MSG_CROSSFADE_QUICK, ""},
    {0, 'f', TB_MOD_SHIFT, MSG_CROSSFADE_MEDIUM, ""},
    {0, 'g', TB_MOD_SHIFT, MSG_CROSSFADE_SLOW, ""},

    // Page navigation
    {TB_KEY_PGDN, 0, 0, MSG_NEXT_PAGE, ""},
    {TB_KEY_PGUP, 0, 0, MSG_PREV_PAGE, ""},

    // Remove
    {TB_KEY_DELETE, 0, 0, MSG_REMOVE, ""},

    // Mouse events
    {TB_KEY_MOUSE_LEFT, 0, 0, MSG_ENQUEUE, ""},
    {TB_KEY_MOUSE_MIDDLE, 0, 0, MSG_ENQUEUEANDPLAY, ""},
    {TB_KEY_MOUSE_RIGHT, 0, 0, MSG_PLAY_PAUSE, ""},
    {TB_KEY_MOUSE_WHEEL_DOWN, 0, 0, MSG_SCROLLDOWN, ""},
    {TB_KEY_MOUSE_WHEEL_UP, 0, 0, MSG_SCROLLUP, ""},

    {0, 'q', 0, MSG_QUIT, ""},
    {TB_KEY_ESC, 0, 0, MSG_QUIT, ""}};

static const KeyMap key_map[] = {
    // Arrow keys
    {"Up Arrow", TB_KEY_ARROW_UP},
    {"Down Arrow", TB_KEY_ARROW_DOWN},
    {"Left Arrow", TB_KEY_ARROW_LEFT},
    {"Right Arrow", TB_KEY_ARROW_RIGHT},

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
    {"g", TB_KEY_CTRL_G},

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

        return "";
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

static bool key_str_already_added(const char *buf, const char *token)
{
        char temp[256];
        c_strcpy(temp, buf, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *part = strtok(temp, " or ");
        while (part != NULL) {
                if (strcmp(part, token) == 0)
                        return true;
                part = strtok(NULL, " or ");
        }
        return false;
}

const char *get_binding_string(enum MsgType event, bool find_only_one)
{
        enum { NUM_BUFS = 8,
               BUF_SIZE = 256 };

        static char bufs[NUM_BUFS][BUF_SIZE];
        static int buf_index = 0;

        char *buf = bufs[buf_index];
        buf_index = (buf_index + 1) % NUM_BUFS;

        buf[0] = '\0';

        int found = 0;

        for (int i = 0; i < MAX_KEY_BINDINGS; i++) {

                if (found > 0 && find_only_one)
                        return buf;

                if (key_bindings[i].eventType != event)
                        continue;

                const char *key_part = "";

                if (key_bindings[i].key != 0)
                        key_part = get_key_name(key_bindings[i].key);
                else if (key_bindings[i].ch != 0) {
                        static char cbuf[2];
                        cbuf[0] = (char)key_bindings[i].ch;
                        cbuf[1] = '\0';
                        key_part = cbuf;
                }

                const char *mod_part =
                    get_modifier_string(key_bindings[i].mods);

                char full_key[64];

                if (mod_part[0] != '\0')
                        snprintf(full_key, sizeof(full_key),
                                 "%s+%s", mod_part, key_part);
                else
                        snprintf(full_key, sizeof(full_key),
                                 "%s", key_part);

                if (key_str_already_added(buf, full_key))
                        continue;

                if (found > 0)
                        strncat(buf, ", ", BUF_SIZE - strlen(buf) - 1);

                strncat(buf, full_key, BUF_SIZE - strlen(buf) - 1);

                found++;
        }

        if (!found)
                snprintf(buf, BUF_SIZE, "?");

        return buf;
}

int get_minicontrols_text(char *restrict text, size_t size)
{
        Model *model = get_model();

        const char *state_icon= "⏸";

#if defined(__ANDROID__) || defined(__APPLE__) || defined(_WIN32)
        state_icon = "။";
#endif

        if (model->is_stopped || model->is_paused)
                return snprintf(text, KEW_PATH_MAX + 1, "⏮  ▶  ⏭  ∅");
        else
                return snprintf(text, KEW_PATH_MAX + 1, "⏮  %s  ⏭  ∅", state_icon);
}

int get_footer_text(char *restrict text, size_t size)
{
        char playlist[32], library[32], track[32], search[32], help[32];

        snprintf(playlist, sizeof(playlist), "%s", get_binding_string(MSG_SHOWPLAYLIST, true));
        snprintf(library, sizeof(library), "%s", get_binding_string(MSG_SHOWLIBRARY, true));
        snprintf(track, sizeof(track), "%s", get_binding_string(MSG_SHOWTRACK, true));
        snprintf(search, sizeof(search), "%s", get_binding_string(MSG_SHOWSEARCH, true));
        snprintf(help, sizeof(search), "%s", get_binding_string(MSG_SHOWHELP, true));

        return snprintf(text, size,
                        _("%s Playlist|%s Library|%s Track|%s Search|%s Help"), playlist,
                        library, track, search, help);
}

typedef struct ConfigEntry {
        char section[64];
        char key[64];
        char value[256];
        struct ConfigEntry *next;
} ConfigEntry;

static ConfigEntry *config_head = NULL;

char *get_settings_file_path(const char *dir, const char *filename)
{
        char *filepath = malloc(KEW_PATH_MAX);
        if (!filepath) {
                perror("malloc");
                quit();
        }

        int written = snprintf(filepath, KEW_PATH_MAX, "%s/%s", dir, filename);
        if (written < 0 || written >= KEW_PATH_MAX) {
                k_log("Error: filepath truncated.\n");
                free(filepath);
                quit();
        }
        return filepath;
}

void free_layout_config(void)
{
        ConfigEntry *e = config_head;
        while (e) {
                ConfigEntry *next = e->next;
                free(e);
                e = next;
        }
        config_head = NULL;
}

void load_layout_config(void)
{
        free_layout_config();

        char *configdir = get_config_path();
        char *filepath = get_settings_file_path(configdir, LAYOUT_FILE);

        FILE *file = fopen(filepath, "r");

        free(filepath);

        if (!file) {
                set_error_message(
                    "Layout was not found. "
                    "Please reinstall kew, or run 'sudo make install' "
                    "if kew was installed by running make.");

                dispatch_msg((struct Msg){.type = MSG_QUIT});

                free(configdir);

                return;
        }

        char line[512];
        int version = -1;

        // Find version
        while (fgets(line, sizeof(line), file)) {

                if (sscanf(line, "version=%d", &version) == 1)
                        break;
        }

        if (version < KEW_LAYOUT_VERSION) {

                fclose(file);

                free(configdir);

                set_error_message(
                    "Layout with the correct version not found. "
                    "Please reinstall kew, or run 'sudo make install' "
                    "if kew was installed by running make.");

                dispatch_msg((struct Msg){.type = MSG_QUIT});

                return;
        }

        rewind(file);

        char current_section[64] = "";

        ConfigEntry *tail = NULL;

        while (fgets(line, sizeof(line), file)) {

                // Strip newline
                line[strcspn(line, "\r\n")] = '\0';

                // Trim leading whitespace
                char *s = line;

                while (*s == ' ' || *s == '\t')
                        s++;

                // Trim trailing whitespace
                char *end = s + strlen(s) - 1;

                while (end >= s &&
                       (*end == ' ' || *end == '\t')) {
                        *end = '\0';
                        end--;
                }

                // Skip blank/comment lines
                if (*s == '\0' || *s == '#')
                        continue;

                // Section header
                if (*s == '[') {

                        char *section_end = strchr(s, ']');

                        if (!section_end)
                                continue;

                        *section_end = '\0';

                        snprintf(current_section, sizeof(current_section), "%.63s", s + 1);

                        current_section[sizeof(current_section) - 1] = '\0';

                        continue;
                }

                // Must belong to a section
                if (current_section[0] == '\0')
                        continue;

                ConfigEntry *entry = calloc(1, sizeof(ConfigEntry));

                if (!entry)
                        continue;

                snprintf(entry->section, sizeof(entry->section), "%.63s", current_section);

                // key=value OR bare directive
                char *eq = strchr(s, '=');

                if (eq) {

                        *eq = '\0';

                        char *key = s;
                        char *value = eq + 1;

                        // Trim trailing spaces from key
                        char *kend = key + strlen(key) - 1;

                        while (kend >= key &&
                               (*kend == ' ' || *kend == '\t')) {
                                *kend = '\0';
                                kend--;
                        }

                        // Trim leading spaces from value
                        while (*value == ' ' || *value == '\t')
                                value++;

                        // Trim trailing spaces from value
                        char *vend = value + strlen(value) - 1;

                        while (vend >= value &&
                               (*vend == ' ' || *vend == '\t')) {
                                *vend = '\0';
                                vend--;
                        }

                        snprintf(entry->key, sizeof(entry->key), "%.63s", key);

                        snprintf(entry->value, sizeof(entry->value), "%.255s", value);

                } else {

                        // Bare directive: row pane
                        snprintf(entry->key, sizeof(entry->key), "%.63s", s);

                        entry->value[0] = '\0';
                }

                // Append to linked list
                if (!config_head) {

                        config_head = entry;
                        tail = entry;

                } else {

                        tail->next = entry;
                        tail = entry;
                }
        }

        free(configdir);

        fclose(file);
}

static time_t get_file_mtime(const char *path)
{
        struct stat st;

        if (stat(path, &st) == 0)
                return st.st_mtime;

        return 0;
}

void settings_init(AppSettings *settings)
{
        AppState *state = get_app_state();

        keybinding_count = NUM_DEFAULT_KEY_BINDINGS;

        char *configdir = get_config_path();

        char *kewrc =
            get_settings_file_path(configdir, SETTINGS_FILE);

        char *kewstaterc =
            get_settings_file_path(configdir, STATE_FILE);

        time_t kewrc_time = get_file_mtime(kewrc);
        time_t state_time = get_file_mtime(kewstaterc);

        // Whatever is the newest file should be loaded last
        // So that users can drop in a new settings file in there
        // And it takes precedence over the kewstaterc (prefs) file.
        if (kewrc_time <= state_time) {
                get_config(settings, &(state->settings));
                get_prefs(settings, &(state->settings));
        } else {
                get_prefs(settings, &(state->settings));
                get_config(settings, &(state->settings));
        }

        free(kewrc);
        free(kewstaterc);
        free(configdir);
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

static int k_stricmp(const char *a, const char *b)
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
        enum MsgType event;
} EventMap;

static const EventMap event_map[] = {
    {"playPause", MSG_PLAY_PAUSE},
    {"volUp", MSG_VOLUME_UP},
    {"volDown", MSG_VOLUME_DOWN},
    {"nextSong", MSG_NEXT},
    {"prevSong", MSG_PREV},
    {"quit", MSG_QUIT},
    {"toggleRepeat", MSG_TOGGLEREPEAT},
    {"toggleVisualizer", MSG_CYCLEVISUALIZERMODE},
    {"cycleVisualizerMode", MSG_CYCLEVISUALIZERMODE},
    {"toggleAscii", MSG_TOGGLEASCII},
    {"addToFavorites_playlist", MSG_ADDTOFAVORITESPLAYLIST},
    {"deleteFromMainPlaylist", MSG_DELETEFROMMAINPLAYLIST},
    {"exportPlaylist", MSG_SAVEPLAYLIST},
    {"updateLibrary", MSG_UPDATELIBRARY},
    {"shuffle", MSG_SHUFFLE},
    {"keyPress", MSG_KEY_PRESS},
    {"showHelp", MSG_SHOWHELP},
    {"showPlaylist", MSG_SHOWPLAYLIST},
    {"showSearch", MSG_SHOWSEARCH},
    {"enqueue", MSG_ENQUEUE},
    {"gotoBeginningOfPlaylist", MSG_GOTOBEGINNINGOFPLAYLIST},
    {"gotoEndOfPlaylist", MSG_GOTOENDOFPLAYLIST},
    {"cycleColorMode", MSG_CYCLECOLORMODE},
    {"cycleVisualization", MSG_CYCLEVISUALIZATION},
    {"scrollDown", MSG_SCROLLDOWN},
    {"scrollUp", MSG_SCROLLUP},
    {"seekBack", MSG_SEEKBACK},
    {"seekForward", MSG_SEEKFORWARD},
    {"showLibrary", MSG_SHOWLIBRARY},
    {"showTrack", MSG_SHOWTRACK},
    {"nextPage", MSG_NEXT_PAGE},
    {"prevPage", MSG_PREV_PAGE},
    {"remove", MSG_REMOVE},
    {"search", MSG_SEARCH},
    {"nextView", MSG_NEXTVIEW},
    {"prevView", MSG_PREVVIEW},
    {"clearPlaylist", MSG_CLEARPLAYLIST},
    {"moveSongUp", MSG_MOVESONGUP},
    {"moveSongDown", MSG_MOVESONGDOWN},
    {"enqueueAndPlay", MSG_ENQUEUEANDPLAY},
    {"stop", MSG_STOP},
    {"sortLibrary", MSG_SORTLIBRARY},
    {"cycleThemes", MSG_CYCLETHEMES},
    {"toggleNotifications", MSG_TOGGLENOTIFICATIONS},
    {"toggleCrossfade", MSG_TOGGLECROSSFADE},
    {"showLyricsPage", MSG_SHOWLYRICSPAGE},
    {"crossfadequick", MSG_CROSSFADE_QUICK},
    {"crossfademedium", MSG_CROSSFADE_MEDIUM},
    {"crossfadeslow", MSG_CROSSFADE_SLOW},
    {NULL, MSG_NONE} // Sentinel
};

static enum MsgType parse_to_event(const char *name)
{
        if (!name)
                return MSG_NONE;

        for (int i = 0; event_map[i].name != NULL; i++) {
                if (k_stricmp(name, event_map[i].name) == 0)
                        return event_map[i].event;
        }
        return MSG_NONE;
}

static const char *event_to_string(enum MsgType ev)
{
        for (int i = 0; event_map[i].name != NULL; i++) {
                if (event_map[i].event == ev)
                        return event_map[i].name;
        }
        return "MSG_NONE";
}

TBKeyBinding parse_binding(const char *binding_str,
                           const char *event_str,
                           const char *args_str)
{
        TBKeyBinding kb = {0};
        kb.eventType = parse_to_event(event_str);
        kb.args[0] = '\0';

        if (args_str && *args_str)
                c_strcpy(kb.args, args_str, sizeof(kb.args) - 1);

        if (!binding_str || !*binding_str)
                return kb;

        char temp[64];
        c_strcpy(temp, binding_str, sizeof(temp) - 1);
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
                        if (k_stricmp(tokenbuf, "Ctrl") == 0 ||
                            k_stricmp(tokenbuf, "LCtrl") == 0 ||
                            k_stricmp(tokenbuf, "RCtrl") == 0) {
                                kb.mods |= TB_MOD_CTRL;
                        } else if (k_stricmp(tokenbuf, "Alt") == 0 ||
                                   k_stricmp(tokenbuf, "LAlt") == 0 ||
                                   k_stricmp(tokenbuf, "RAlt") == 0) {
                                kb.mods |= TB_MOD_ALT;
                        } else if (k_stricmp(tokenbuf, "Shift") == 0 ||
                                   k_stricmp(tokenbuf, "LShift") == 0 ||
                                   k_stricmp(tokenbuf, "RShift") == 0) {
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
        c_strcpy(settings->coverEnabled, "1", sizeof(settings->coverEnabled));
        c_strcpy(settings->stripTrackNumbers, "1",
                 sizeof(settings->stripTrackNumbers));
#ifdef _WIN32
        c_strcpy(settings->allowNotifications, "0",
                 sizeof(settings->allowNotifications));
#else
        c_strcpy(settings->allowNotifications, "1",
                 sizeof(settings->allowNotifications));
#endif
        c_strcpy(settings->coverAnsi, "0", sizeof(settings->coverAnsi));
        c_strcpy(settings->coverStyle, "auto", sizeof(settings->coverStyle));
        c_strcpy(settings->quitAfterStopping, "0",
                 sizeof(settings->quitAfterStopping));
        c_strcpy(settings->clearListClearsAll, "1",
                 sizeof(settings->clearListClearsAll));
        c_strcpy(settings->hideGlimmeringText, "0",
                 sizeof(settings->hideGlimmeringText));
        c_strcpy(settings->useArtistsDb, "1",
                 sizeof(settings->useArtistsDb));
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
        c_strcpy(settings->saveRepeatShuffleSettings, "0",
                 sizeof(settings->saveRepeatShuffleSettings));
        c_strcpy(settings->trackTitleAsWindowTitle, "1",
                 sizeof(settings->trackTitleAsWindowTitle));
        c_strcpy(settings->discordRPCEnabled, "0",
                 sizeof(settings->discordRPCEnabled));
        c_strcpy(settings->visualizer_mode, "3",
                 sizeof(settings->visualizer_mode));
        c_strcpy(settings->colorMode, "3",
                 sizeof(settings->colorMode));

#ifdef __ANDROID__
        c_strcpy(settings->hideLogo, "1", sizeof(settings->hideLogo));
#else
        c_strcpy(settings->hideLogo, "0", sizeof(settings->hideLogo));
#endif
        c_strcpy(settings->hideFooter, "0", sizeof(settings->hideFooter));
        c_strcpy(settings->hideHelp, "0", sizeof(settings->hideHelp));
        c_strcpy(settings->hideSideCover, "0", sizeof(settings->hideSideCover));
        c_strcpy(settings->collapseTopLevel, "0", sizeof(settings->collapseTopLevel));
        c_strcpy(settings->hideTimeStatus, "0", sizeof(settings->hideTimeStatus));
        c_strcpy(settings->simpleTimeStatus, "1", sizeof(settings->simpleTimeStatus));
        c_strcpy(settings->visualizer_height, "6",
                 sizeof(settings->visualizer_height));
        c_strcpy(settings->titleDelay, "1", sizeof(settings->titleDelay));
        c_strcpy(settings->auto_resume, "1", sizeof(settings->auto_resume));
        c_strcpy(settings->always_crossfade, "0", sizeof(settings->always_crossfade));
        c_strcpy(settings->lastVolume, "100", sizeof(settings->lastVolume));
        c_strcpy(settings->color, "6", sizeof(settings->color));
        c_strcpy(settings->artistColor, "6", sizeof(settings->artistColor));
        c_strcpy(settings->titleColor, "6", sizeof(settings->titleColor));
        c_strcpy(settings->enqueued_color, "6", sizeof(settings->enqueued_color));
        c_strcpy(settings->fade_enter_song_ms, "0", sizeof(settings->fade_enter_song_ms));
        c_strcpy(settings->fade_quick_ms, "3000", sizeof(settings->fade_quick_ms));
        c_strcpy(settings->fade_medium_ms, "5000", sizeof(settings->fade_medium_ms));
        c_strcpy(settings->fade_slow_ms, "10000", sizeof(settings->fade_slow_ms));

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
                        key_bindings[i].eventType = MSG_NONE;
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
                        key_bindings[i].eventType = MSG_NONE;
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

void add_legacy_key_binding(enum MsgType event, char *value)
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
        enum MsgType event = get_mouse_action(action);

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
                } else if (strcmp(lowercase_key, "chromapath") == 0) {
                        snprintf(settings->chromaPath, sizeof(settings->chromaPath), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "chromadevice") == 0) {
                        snprintf(settings->chromaDevice, sizeof(settings->chromaDevice), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "coverenabled") == 0) {
                        snprintf(settings->coverEnabled,
                                 sizeof(settings->coverEnabled), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "coveransi") == 0) {
                        snprintf(settings->coverAnsi, sizeof(settings->coverAnsi),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "coverstyle") == 0) {
                        if (!is_prefs)
                                snprintf(settings->coverStyle, sizeof(settings->coverStyle),
                                         "%s", pair->value);
                } else if (strcmp(lowercase_key, "visualizercolortype") == 0) {
                        snprintf(settings->visualizer_mode,
                                 sizeof(settings->visualizer_mode), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "hidetimestatus") == 0) {
                        snprintf(settings->hideTimeStatus,
                                 sizeof(settings->hideTimeStatus), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "simpletimestatus") == 0) {
                        snprintf(settings->simpleTimeStatus,
                                 sizeof(settings->simpleTimeStatus), "%s",
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
                } else if (strcmp(lowercase_key, "titledelay") == 0) {
                        snprintf(settings->titleDelay,
                                 sizeof(settings->titleDelay), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "autoresume") == 0) {
                        snprintf(settings->auto_resume,
                                 sizeof(settings->auto_resume), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "alwayscrossfade") == 0) {
                        snprintf(settings->always_crossfade,
                                 sizeof(settings->always_crossfade), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "currentsongid") == 0) {
                        snprintf(settings->currentSongId,
                                 sizeof(settings->currentSongId), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "currentsongseconds") == 0) {
                        snprintf(settings->currentSongSeconds,
                                 sizeof(settings->currentSongSeconds), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "fadequick") == 0) {
                        snprintf(settings->fade_quick, sizeof(settings->fade_quick),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "fademedium") == 0) {
                        snprintf(settings->fade_medium, sizeof(settings->fade_medium),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "fadeslow") == 0) {
                        snprintf(settings->fade_slow, sizeof(settings->fade_slow),
                                 "%s", pair->value);
              } else if (strcmp(lowercase_key, "fadeentersongms") == 0) {
                        snprintf(settings->fade_enter_song_ms, sizeof(settings->fade_enter_song_ms),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "fadequickms") == 0) {
                        snprintf(settings->fade_quick_ms, sizeof(settings->fade_quick_ms),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "fademediumms") == 0) {
                        snprintf(settings->fade_medium_ms, sizeof(settings->fade_medium_ms),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "fadeslowms") == 0) {
                        snprintf(settings->fade_slow_ms, sizeof(settings->fade_slow_ms),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "volumeup") == 0) {
                        snprintf(settings->volumeUp, sizeof(settings->volumeUp),
                                 "%s", pair->value);
                        add_legacy_key_binding(MSG_VOLUME_UP, pair->value);
                } else if (strcmp(lowercase_key, "volumeupalt") == 0) {
                        snprintf(settings->volumeUpAlt,
                                 sizeof(settings->volumeUpAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_VOLUME_UP, pair->value);
                } else if (strcmp(lowercase_key, "volumedown") == 0) {
                        snprintf(settings->volumeDown,
                                 sizeof(settings->volumeDown), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_VOLUME_DOWN, pair->value);
                } else if (strcmp(lowercase_key, "previoustrackalt") == 0) {
                        snprintf(settings->previousTrackAlt,
                                 sizeof(settings->previousTrackAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_PREV, pair->value);
                } else if (strcmp(lowercase_key, "nexttrackalt") == 0) {
                        snprintf(settings->nextTrackAlt,
                                 sizeof(settings->nextTrackAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_NEXT, pair->value);
                } else if (strcmp(lowercase_key, "scrollupalt") == 0) {
                        snprintf(settings->scrollUpAlt,
                                 sizeof(settings->scrollUpAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SCROLLUP, pair->value);
                } else if (strcmp(lowercase_key, "scrolldownalt") == 0) {
                        snprintf(settings->scrollDownAlt,
                                 sizeof(settings->scrollDownAlt), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SCROLLDOWN, pair->value);
                } else if (strcmp(lowercase_key, "switchnumberedsong") == 0) {
                        snprintf(settings->switchNumberedSong,
                                 sizeof(settings->switchNumberedSong), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_ENQUEUE, pair->value);
                } else if (strcmp(lowercase_key, "togglepause") == 0) {
                        snprintf(settings->toggle_pause,
                                 sizeof(settings->toggle_pause), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_PLAY_PAUSE, pair->value);
                } else if (strcmp(lowercase_key, "togglecolorsderivedfrom") == 0) {
                        snprintf(settings->cycleColorsDerivedFrom,
                                 sizeof(settings->cycleColorsDerivedFrom), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_CYCLECOLORMODE, pair->value);
                } else if (strcmp(lowercase_key, "cyclethemes") == 0) {
                        snprintf(settings->cycle_themes,
                                 sizeof(settings->cycle_themes), "%s",
                                 pair->value);
                        foundCycleThemesSetting = true;
                        add_legacy_key_binding(MSG_CYCLETHEMES, pair->value);
                } else if (strcmp(lowercase_key, "togglenotifications") == 0) {
                        snprintf(settings->toggle_notifications,
                                 sizeof(settings->toggle_notifications), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_TOGGLENOTIFICATIONS, pair->value);
                } else if (strcmp(lowercase_key, "togglecrossfade") == 0) {
                        snprintf(settings->toggle_crossfade,
                                 sizeof(settings->toggle_crossfade), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "togglevisualizer") == 0) {
                        snprintf(settings->toggle_visualizer,
                                 sizeof(settings->toggle_visualizer), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_CYCLEVISUALIZERMODE, pair->value);
                } else if (strcmp(lowercase_key, "toggleascii") == 0) {
                        snprintf(settings->toggle_ascii,
                                 sizeof(settings->toggle_ascii), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_TOGGLEASCII, pair->value);
                } else if (strcmp(lowercase_key, "togglerepeat") == 0) {
                        snprintf(settings->toggle_repeat,
                                 sizeof(settings->toggle_repeat), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_TOGGLEREPEAT, pair->value);
                } else if (strcmp(lowercase_key, "toggleshuffle") == 0) {
                        snprintf(settings->toggle_shuffle,
                                 sizeof(settings->toggle_shuffle), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SHUFFLE, pair->value);
                } else if (strcmp(lowercase_key, "seekbackward") == 0) {
                        snprintf(settings->seekBackward,
                                 sizeof(settings->seekBackward), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SEEKBACK, pair->value);
                } else if (strcmp(lowercase_key, "seekforward") == 0) {
                        snprintf(settings->seek_forward,
                                 sizeof(settings->seek_forward), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SEEKFORWARD, pair->value);
                } else if (strcmp(lowercase_key, "saveplaylist") == 0) {
                        snprintf(settings->save_playlist,
                                 sizeof(settings->save_playlist), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_SAVEPLAYLIST, pair->value);
                } else if (strcmp(lowercase_key, "addtofavoritesplaylist") == 0) {
                        snprintf(settings->add_to_favorites_playlist,
                                 sizeof(settings->add_to_favorites_playlist), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_ADDTOFAVORITESPLAYLIST, pair->value);
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
                } else if (strcmp(lowercase_key, "collapsetoplevel") == 0) {
                        snprintf(settings->collapseTopLevel, sizeof(settings->collapseTopLevel),
                                 "%s", pair->value);
                } else if (strcmp(lowercase_key, "quitonstop") == 0) {
                        snprintf(settings->quitAfterStopping,
                                 sizeof(settings->quitAfterStopping), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "clearlistclearsall") == 0) {
                        snprintf(settings->clearListClearsAll,
                                 sizeof(settings->clearListClearsAll), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "hideglimmeringtext") == 0) {
                        snprintf(settings->hideGlimmeringText,
                                 sizeof(settings->hideGlimmeringText), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "useartistsdb") == 0) {
                        snprintf(settings->useArtistsDb,
                                 sizeof(settings->useArtistsDb), "%s",
                                 pair->value);
                } else if (strcmp(lowercase_key, "quit") == 0) {
                        snprintf(settings->quit, sizeof(settings->quit), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_QUIT, pair->value);
                } else if (strcmp(lowercase_key, "altquit") == 0) {
                        snprintf(settings->altQuit, sizeof(settings->altQuit),
                                 "%s", pair->value);
                        add_legacy_key_binding(MSG_QUIT, pair->value);
                } else if (strcmp(lowercase_key, "prevpage") == 0) {
                        snprintf(settings->prevPage, sizeof(settings->prevPage),
                                 "%s", pair->value);
                        add_legacy_key_binding(MSG_PREV_PAGE, pair->value);
                } else if (strcmp(lowercase_key, "nextpage") == 0) {
                        snprintf(settings->nextPage, sizeof(settings->nextPage),
                                 "%s", pair->value);
                        add_legacy_key_binding(MSG_NEXT_PAGE, pair->value);
                } else if (strcmp(lowercase_key, "updatelibrary") == 0) {
                        snprintf(settings->update_library,
                                 sizeof(settings->update_library), "%s",
                                 pair->value);
                        add_legacy_key_binding(MSG_UPDATELIBRARY, pair->value);
                } else if (strcmp(lowercase_key, "showplaylistalt") == 0) {
                        if (strcmp(pair->value, "") !=
                            0) // Don't set these to nothing
                                snprintf(settings->showPlaylistAlt,
                                         sizeof(settings->showPlaylistAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SHOWPLAYLIST, pair->value);
                } else if (strcmp(lowercase_key, "showlibraryalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showLibraryAlt,
                                         sizeof(settings->showLibraryAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SHOWLIBRARY, pair->value);
                } else if (strcmp(lowercase_key, "showtrackalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showTrackAlt,
                                         sizeof(settings->showTrackAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SHOWTRACK, pair->value);
                } else if (strcmp(lowercase_key, "showsearchalt") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showSearchAlt,
                                         sizeof(settings->showSearchAlt), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SHOWSEARCH, pair->value);
                } else if (strcmp(lowercase_key, "showlyricspage") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->showLyricsPage,
                                         sizeof(settings->showLyricsPage), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SHOWLYRICSPAGE, pair->value);
                } else if (strcmp(lowercase_key, "movesongup") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->move_song_up,
                                         sizeof(settings->move_song_up), "%s",
                                         pair->value);
                        // Don't add this since this key 't' should be used for themes
                        //add_legacy_key_binding(MSG_MOVESONGUP, pair->value);
                } else if (strcmp(lowercase_key, "movesongdown") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->move_song_down,
                                         sizeof(settings->move_song_down), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_MOVESONGDOWN, pair->value);
                } else if (strcmp(lowercase_key, "enqueueandplay") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->enqueueAndPlay,
                                         sizeof(settings->enqueueAndPlay), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_ENQUEUEANDPLAY, pair->value);
                } else if (strcmp(lowercase_key, "sort") == 0) {
                        if (strcmp(pair->value, "") != 0)
                                snprintf(settings->sort_library,
                                         sizeof(settings->sort_library), "%s",
                                         pair->value);
                        add_legacy_key_binding(MSG_SORTLIBRARY, pair->value);
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
                        add_legacy_key_binding(MSG_SHOWHELP, pair->value);
                } else if (strcmp(lowercase_key, "bind") == 0) {
                        char value_copy[256];
                        c_strcpy(value_copy, pair->value, sizeof(value_copy));
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
                static char music_path[KEW_PATH_MAX];
                snprintf(music_path, sizeof(music_path), "%s/Music", home);
                return music_path;
        } else {
                return NULL; // Return NULL if XDG home is not found.
        }
}

int get_music_library_path(char *path)
{
        char expanded_path[KEW_PATH_MAX];

        if (path[0] != '\0' && path[0] != '\r') {
                if (expand_path(path, expanded_path, KEW_PATH_MAX) >= 0) {
                        c_strcpy(path, expanded_path, sizeof(expanded_path));
                }
        }

        return 0;
}

void map_settings_to_keys(AppSettings *settings, EventMapping *mappings)
{
        mappings[0] = (EventMapping){settings->scrollUpAlt, MSG_SCROLLUP};
        mappings[1] = (EventMapping){settings->scrollDownAlt, MSG_SCROLLDOWN};
        mappings[2] = (EventMapping){settings->nextTrackAlt, MSG_NEXT};
        mappings[3] = (EventMapping){settings->previousTrackAlt, MSG_PREV};
        mappings[4] = (EventMapping){settings->volumeUp, MSG_VOLUME_UP};
        mappings[5] = (EventMapping){settings->volumeUpAlt, MSG_VOLUME_UP};
        mappings[6] = (EventMapping){settings->volumeDown, MSG_VOLUME_DOWN};
        mappings[7] = (EventMapping){settings->toggle_pause, MSG_PLAY_PAUSE};
        mappings[8] = (EventMapping){settings->quit, MSG_QUIT};
        mappings[9] = (EventMapping){settings->altQuit, MSG_QUIT};
        mappings[10] = (EventMapping){settings->toggle_shuffle, MSG_SHUFFLE};
        mappings[11] = (EventMapping){settings->toggle_visualizer, MSG_CYCLEVISUALIZERMODE};
        mappings[12] = (EventMapping){settings->toggle_ascii, MSG_TOGGLEASCII};
        mappings[13] = (EventMapping){settings->switchNumberedSong, MSG_ENQUEUE};
        mappings[14] = (EventMapping){settings->seekBackward, MSG_SEEKBACK};
        mappings[15] = (EventMapping){settings->seek_forward, MSG_SEEKFORWARD};
        mappings[16] = (EventMapping){settings->toggle_repeat, MSG_TOGGLEREPEAT};
        mappings[17] = (EventMapping){settings->save_playlist, MSG_SAVEPLAYLIST};
        mappings[18] = (EventMapping){settings->cycleColorsDerivedFrom, MSG_CYCLECOLORMODE};
        mappings[19] = (EventMapping){settings->add_to_favorites_playlist, MSG_ADDTOFAVORITESPLAYLIST};
        mappings[20] = (EventMapping){settings->update_library, MSG_UPDATELIBRARY};
        mappings[21] = (EventMapping){settings->hardPlayPause, MSG_PLAY_PAUSE};
        mappings[22] = (EventMapping){settings->hardPrev, MSG_PREV};
        mappings[23] = (EventMapping){settings->hardNext, MSG_NEXT};
        mappings[24] = (EventMapping){settings->hardSwitchNumberedSong, MSG_ENQUEUE};
        mappings[25] = (EventMapping){settings->hardScrollUp, MSG_SCROLLUP};
        mappings[26] = (EventMapping){settings->hardScrollDown, MSG_SCROLLDOWN};
        mappings[27] = (EventMapping){settings->hardShowPlaylist, MSG_SHOWPLAYLIST};
        mappings[28] = (EventMapping){settings->hardShowPlaylistAlt, MSG_SHOWPLAYLIST};
        mappings[29] = (EventMapping){settings->showPlaylistAlt, MSG_SHOWPLAYLIST};
        mappings[30] = (EventMapping){settings->hardShowKeys, MSG_SHOWHELP};
        mappings[31] = (EventMapping){settings->hardShowKeysAlt, MSG_SHOWHELP};
        mappings[32] = (EventMapping){settings->showKeysAlt, MSG_SHOWHELP};
        mappings[33] = (EventMapping){settings->hardShowTrack, MSG_SHOWTRACK};
        mappings[34] = (EventMapping){settings->hardShowTrackAlt, MSG_SHOWTRACK};
        mappings[35] = (EventMapping){settings->showTrackAlt, MSG_SHOWTRACK};
        mappings[36] = (EventMapping){settings->hardShowLibrary, MSG_SHOWLIBRARY};
        mappings[37] = (EventMapping){settings->hardShowLibraryAlt, MSG_SHOWLIBRARY};
        mappings[38] = (EventMapping){settings->showLibraryAlt, MSG_SHOWLIBRARY};
        mappings[39] = (EventMapping){settings->hardShowSearch, MSG_SHOWSEARCH};
        mappings[40] = (EventMapping){settings->hardShowSearchAlt, MSG_SHOWSEARCH};
        mappings[41] = (EventMapping){settings->showSearchAlt, MSG_SHOWSEARCH};
        mappings[42] = (EventMapping){settings->nextPage, MSG_NEXT_PAGE};
        mappings[43] = (EventMapping){settings->prevPage, MSG_PREV_PAGE};
        mappings[44] = (EventMapping){settings->hardRemove, MSG_REMOVE};
        mappings[45] = (EventMapping){settings->hardRemove2, MSG_REMOVE};
        mappings[46] = (EventMapping){settings->nextView, MSG_NEXTVIEW};
        mappings[47] = (EventMapping){settings->prevView, MSG_PREVVIEW};
        mappings[55] = (EventMapping){settings->hardClearPlaylist, MSG_CLEARPLAYLIST};
        mappings[56] = (EventMapping){settings->move_song_up, MSG_MOVESONGUP};
        mappings[57] = (EventMapping){settings->move_song_down, MSG_MOVESONGDOWN};
        mappings[58] = (EventMapping){settings->enqueueAndPlay, MSG_ENQUEUEANDPLAY};
        mappings[59] = (EventMapping){settings->hardStop, MSG_STOP};
        mappings[60] = (EventMapping){settings->sort_library, MSG_SORTLIBRARY};
        mappings[61] = (EventMapping){settings->cycle_themes, MSG_CYCLETHEMES};
        mappings[62] = (EventMapping){settings->toggle_notifications, MSG_TOGGLENOTIFICATIONS};
        mappings[63] = (EventMapping){settings->toggle_crossfade, MSG_TOGGLECROSSFADE};
        mappings[64] = (EventMapping){settings->showLyricsPage, MSG_SHOWLYRICSPAGE};
        mappings[65] = (EventMapping){settings->fade_quick, MSG_CROSSFADE_QUICK};
        mappings[66] = (EventMapping){settings->fade_medium, MSG_CROSSFADE_MEDIUM};
        mappings[67] = (EventMapping){settings->fade_slow, MSG_CROSSFADE_SLOW};
}

void migrate_prefs_file(char *new_filepath)
{
        char *prefs_dir = get_prefs_path();
        char *prefs_file_old = get_settings_file_path(prefs_dir, STATE_FILE);

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

void load_settings_into_ui(AppSettings *settings, UISettings *ui)
{
        double sec = get_float(settings->currentSongSeconds);
        if (sec > 0)
                ui->currentSongSeconds = sec;

        int tmp = get_number(settings->currentSongId);
        if (tmp > 0)
                ui->currentSongId = tmp;

        tmp = get_number(settings->repeatState);
        if (tmp >= 0)
                set_repeat_state(tmp);

        if (settings->chromaPreset[0] != '\0') {
                tmp = get_number(settings->chromaPreset);

                if (tmp >= 0)
                        ui->chromaPreset = tmp;
        }

        tmp = get_number(settings->colorMode);
        if (tmp >= 0 && tmp <= 4) {
                ui->colorMode = tmp;
        }

        tmp = get_number(settings->visualizer_mode);
        if (tmp >= 0 && tmp <= 10) {
                ui->visualizer_mode = tmp;
        }

        tmp = get_number(settings->lastVolume);
        if (tmp >= 0)
                set_volume(tmp);

        tmp = get_number(settings->fade_enter_song_ms);
        if (tmp > 0) {
                ui->fade_enter_song_ms = tmp;
        }

        tmp = get_number(settings->fade_quick_ms);
        if (tmp >= 0) {
                ui->fade_quick_ms = tmp;
        }

        tmp = get_number(settings->fade_medium_ms);
        if (tmp >= 0) {
                ui->fade_medium_ms = tmp;
        }

        tmp = get_number(settings->fade_slow_ms);
        if (tmp >= 0) {
                ui->fade_slow_ms = tmp;
        }

        if (ui->colorMode != COLOR_MODE_ALBUM &&
                ui->colorMode != COLOR_MODE_ALBUM_ONE &&
                ui->colorMode != COLOR_MODE_DEFAULT &&
                ui->colorMode != COLOR_MODE_NEUTRAL)
                snprintf(ui->theme_name, sizeof(ui->theme_name), "%s", settings->theme);
        else
                ui->theme_name[0] = '\0';
}

void get_prefs(AppSettings *settings, UISettings *ui)
{
        int pair_count;
        char *configdir = get_config_path();

        setlocale(LC_ALL, "");

        struct stat st = {0};
        if (stat(configdir, &st) == -1) {
                if (create_directory(configdir) != 1) {
                        perror("mkdir");
                        free(configdir);
                        quit();
                }
        }

        char *filepath = get_settings_file_path(configdir, STATE_FILE);

        // Move legacy state file to new location
        migrate_prefs_file(filepath);

        KeyValuePair *pairs =
            read_key_value_pairs(filepath, &pair_count, &(ui->last_time_app_ran));

        free(filepath);
        construct_app_settings(settings, pairs, pair_count, true);

        load_settings_into_ui(settings, ui);

        free(configdir);
}

void get_config(AppSettings *settings, UISettings *ui)
{
        int pair_count;
        char *configdir = get_config_path();

        setlocale(LC_ALL, "");

        struct stat st = {0};
        if (stat(configdir, &st) == -1) {
                if (create_directory(configdir) != 1) {
                        perror("mkdir");
                        quit();
                }
        }

        set_default_config(settings);

        char *filepath = get_settings_file_path(configdir, SETTINGS_FILE);

        FILE *file = fopen(filepath, "r");
        if (file == NULL) {
                set_config(settings, ui);
        } else {
                fclose(file);
        }

        KeyValuePair *pairs =
            read_key_value_pairs(filepath, &pair_count, &(ui->last_time_app_ran));

        free(filepath);

        construct_app_settings(settings, pairs, pair_count, false);

        load_settings_into_ui(settings, ui);

        free(configdir);
}

void set_prefs(AppSettings *settings, UISettings *ui)
{
        // Create the file path
        char *configdir = get_config_path();
        char *filepath = get_settings_file_path(configdir, STATE_FILE);

        setlocale(LC_ALL, "");

        FILE *file = fopen(filepath, "w");
        if (file == NULL) {
                k_log("Error opening file: %s\n", filepath);
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

        snprintf(settings->visualizer_mode,
                 sizeof(settings->visualizer_mode), "%d",
                 ui->visualizer_mode);

        if (ui->saveRepeatShuffleSettings) {
                snprintf(settings->repeatState, sizeof(settings->repeatState), "%d",
                         ui->repeatState);

                ui->shuffle_enabled ? c_strcpy(settings->shuffle_enabled, "1",
                                               sizeof(settings->shuffle_enabled))
                                    : c_strcpy(settings->shuffle_enabled, "0",
                                               sizeof(settings->shuffle_enabled));
        }

        int current_volume = get_volume();
        current_volume = (current_volume < 0) ? 0 : current_volume;
        snprintf(settings->lastVolume, sizeof(settings->lastVolume), "%d",
                 current_volume);

        fprintf(file, "\n[miscellaneous]\n\n");
        fprintf(file, "allowNotifications=%s\n\n", settings->allowNotifications);
        fprintf(file, "alwaysCrossfade=%d\n\n", ui->always_crossfade);

        if (ui->saveRepeatShuffleSettings) {
                fprintf(file, "repeatState=%s\n\n", settings->repeatState);
                fprintf(file, "shuffleEnabled=%s\n\n", settings->shuffle_enabled);
        }

        fprintf(file, "lastVolume=%s\n\n", settings->lastVolume);
        fprintf(file, "[track cover]\n\n");
        fprintf(file, "coverAnsi=%s\n\n", settings->coverAnsi);
        fprintf(file, "[visualizer]\n\n");
        fprintf(file, "visualizerColortype=%s\n\n", settings->visualizer_mode);
        fprintf(file, "[chroma]\n\n");
        fprintf(file, "chromaPreset=%d\n\n", ui->chromaPreset);
        fprintf(file, "[colors]\n\n");
        fprintf(file, "colorMode=%d\n\n", ui->colorMode);

        if (ui->colorMode != COLOR_MODE_THEME)
                settings->theme[0] = '\0';

        fprintf(file, "theme=%s\n\n", settings->theme);

        Node *current = get_current_song();
        Model *model = get_model();

        // Save current song id and seconds for auto-resume
        if (current) {
                FileSystemEntry *entry = find_corresponding_entry(model->library, current->song.file_path);
                fprintf(file, "currentSongId=%d\n", entry->id);
                fprintf(file, "currentSongSeconds=%f\n", model->elapsed_seconds);
        }

        fclose(file);
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
        char *filepath = get_settings_file_path(configdir, SETTINGS_FILE);

        setlocale(LC_ALL, "");

        FILE *file = fopen(filepath, "w");
        if (file == NULL) {
                k_log("Error opening file: %s\n", filepath);
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
        if (settings->coverStyle[0] == '\0')
                c_strcpy(settings->coverStyle, "auto", sizeof(settings->coverStyle));
        if (settings->visualizer_mode[0] == '\0')
                snprintf(settings->visualizer_mode,
                         sizeof(settings->visualizer_mode), "%d",
                         ui->visualizer_mode);
        if (settings->hideTimeStatus[0] == '\0')
                ui->hideTimeStatus
                    ? c_strcpy(settings->hideTimeStatus, "1",
                               sizeof(settings->hideTimeStatus))
                    : c_strcpy(settings->hideTimeStatus, "0",
                               sizeof(settings->hideTimeStatus));
        if (settings->simpleTimeStatus[0] == '\0')
                ui->simpleTimeStatus
                    ? c_strcpy(settings->simpleTimeStatus, "1",
                               sizeof(settings->simpleTimeStatus))
                    : c_strcpy(settings->simpleTimeStatus, "0",
                               sizeof(settings->simpleTimeStatus));
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
        if (settings->clearListClearsAll[0] == '\0')
                ui->clearListClearsAll
                    ? c_strcpy(settings->clearListClearsAll, "1",
                               sizeof(settings->clearListClearsAll))
                    : c_strcpy(settings->clearListClearsAll, "0",
                               sizeof(settings->clearListClearsAll));
        if (settings->hideGlimmeringText[0] == '\0')
                ui->hideGlimmeringText
                    ? c_strcpy(settings->hideGlimmeringText, "1",
                               sizeof(settings->hideGlimmeringText))
                    : c_strcpy(settings->hideGlimmeringText, "0",
                               sizeof(settings->hideGlimmeringText));
        if (settings->useArtistsDb[0] == '\0')
                ui->useArtistsDb
                    ? c_strcpy(settings->useArtistsDb, "1",
                               sizeof(settings->useArtistsDb))
                    : c_strcpy(settings->useArtistsDb, "0",
                               sizeof(settings->useArtistsDb));
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
        if (settings->collapseTopLevel[0] == '\0')
                ui->collapseTopLevel ? c_strcpy(settings->collapseTopLevel, "1",
                                                sizeof(settings->collapseTopLevel))
                                     : c_strcpy(settings->collapseTopLevel, "0",
                                                sizeof(settings->collapseTopLevel));

        if (settings->visualizer_height[0] == '\0')
                snprintf(settings->visualizer_height,
                         sizeof(settings->visualizer_height), "%d",
                         ui->visualizer_height);

        if (settings->titleDelay[0] == '\0')
                snprintf(settings->titleDelay, sizeof(settings->titleDelay),
                         "%d", ui->titleDelay);

        if (settings->auto_resume[0] == '\0')
                snprintf(settings->auto_resume, sizeof(settings->auto_resume),
                         "%d", ui->auto_resume);

        if (settings->always_crossfade[0] == '\0')
                snprintf(settings->always_crossfade, sizeof(settings->always_crossfade),
                         "%d", ui->always_crossfade);

        if (settings->replayGainCheckFirst[0] == '\0')
                snprintf(settings->replayGainCheckFirst,
                         sizeof(settings->replayGainCheckFirst), "%d",
                         ui->replayGainCheckFirst);

        // Write the settings to the file
        fprintf(file, "# kew Settings\n\n");
        fprintf(file, "# IMPORTANT: kew doesn't write to this file, except for when you run kew path.\n");
        fprintf(file, "# Delete this file if you installed a new version of kew and you want to see new settings here.\n");
        fprintf(file, "# kew will then generate the file with all available settings.\n");
        fprintf(file, "# kew tracks all in-app settings changes in kewstaterc, which take precedence over kewrc.\n\n");
        fprintf(file, "[miscellaneous]\n\n");
        fprintf(file, "path=%s\n\n", settings->path);
        fprintf(file, "# Enable artist database, that provides clickable artists links in track view.\n");
        fprintf(file, "useArtistsDb=%s\n\n", settings->useArtistsDb);
        fprintf(file, "allowNotifications=%s\n", settings->allowNotifications);
        fprintf(file, "stripTrackNumbers=%s\n", settings->stripTrackNumbers);
        fprintf(file, "hideLogo=%s\n", settings->hideLogo);
        fprintf(file, "hideHelp=%s\n", settings->hideHelp);
        fprintf(file, "hideTimeStatus=%s\n\n", settings->hideTimeStatus);

         fprintf(file, "# Toggles showing kHz and bitrate.\n");
        fprintf(file, "simpleTimeStatus=%s\n\n", settings->simpleTimeStatus);

        fprintf(file, "hideFooter=%s\n", settings->hideFooter);
        fprintf(file, "hideSideCover=%s\n", settings->hideSideCover);
        fprintf(file, "collapseTopLevel=%s\n", settings->collapseTopLevel);
        fprintf(file, "autoResume=%s\n\n", settings->auto_resume);

        fprintf(file, "# Toggle animated song title, set to 0 to disable.\n");
        fprintf(file, "titleDelay=%s\n\n", settings->titleDelay);

        fprintf(file, "# Same as '--quitonstop' flag, exits after playing the "
                      "whole playlist.\n");
        fprintf(file, "quitOnStop=%s\n\n", settings->quitAfterStopping);

        fprintf(file, "# Whether clearing the playlist also removes the "
                      "currently playing song.\n");
        fprintf(file, "clearListClearsAll=%s\n\n", settings->clearListClearsAll);

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
        fprintf(file, "# 1 = Colors derived from One Album Color Theme, \n");
        fprintf(file, "# 2 = Colors derived from TrueColor theme, \n");
        fprintf(file, "# 3 = Colors derived from Album Colors Theme, \n\n");
        fprintf(file, "# Color Mode:\n");
        fprintf(file, "colorMode=%d\n\n", ui->colorMode);

        fprintf(file, "# Terminal color theme is default.theme in \n");
        fprintf(file, "# ~/.config/kew/themes (on Linux/FreeBSD/Android), \n");
        fprintf(file, "# and ~/Library/Preferences/kew/themes (on macOS).\n\n");

        fprintf(file, "\n[crossfade]\n\n");
        fprintf(file, "alwaysCrossfade=%d\n", ui->always_crossfade);
        fprintf(file, "fadeEnterSongMs=%d\n", ui->fade_enter_song_ms);
        fprintf(file, "fadeQuickMs=%s\n", settings->fade_quick_ms);
        fprintf(file, "fadeMediumMs=%s\n", settings->fade_medium_ms);
        fprintf(file, "fadeSlowMs=%s\n\n", settings->fade_slow_ms);

        fprintf(file, "\n[track cover]\n\n");
        fprintf(file, "coverEnabled=%s\n", settings->coverEnabled);
        fprintf(file, "coverAnsi=%s\n", settings->coverAnsi);
        fprintf(file, "# Cover render style: auto, kitty, sixels, block, braille, ascii, dot, vhalf, quad\n");
        fprintf(file, "coverStyle=%s\n\n", settings->coverStyle);

        fprintf(file, "\n[mouse]\n\n");
        fprintf(file, "mouseEnabled=%s\n\n", settings->mouseEnabled);

        fprintf(file, "\n[discord]\n\n");
        fprintf(file, "discordRPCEnabled=%s\n\n", settings->discordRPCEnabled);

        fprintf(file, "\n[chroma]\n\n");
        fprintf(file, "chromaPath=%s\n", settings->chromaPath);
        fprintf(file, "chromaDevice=%s\n\n", settings->chromaDevice);

        fprintf(file, "\n[visualizer]\n\n");
        fprintf(file, "# Visualizer mode: 0=lighten, 1=flat, 2=reversed lighten, 3=party, 4=vibrant. 5=lum vibrant 6=binning.\n");
        fprintf(file, "visualizerColorType=%s\n", settings->visualizer_mode);
        fprintf(file, "visualizerHeight=%s\n", settings->visualizer_height);
        fprintf(file, "visualizerBrailleMode=%s\n\n",
                settings->visualizerBrailleMode);

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
        config_file_path = get_settings_file_path(configdir, SETTINGS_FILE);

        update_rc(config_file_path, "path", path);

        if (configdir != NULL)
                free(configdir);

        if (config_file_path != NULL)
                free(config_file_path);
}

bool config_has_section(const char *section)
{
        for (ConfigEntry *e = config_head; e; e = e->next)
                if (strcmp(e->section, section) == 0)
                        return true;
        return false;
}

char *config_get(const char *section, const char *key)
{
        for (ConfigEntry *e = config_head; e; e = e->next)
                if (strcmp(e->section, section) == 0 &&
                    strcmp(e->key, key) == 0)
                        return e->value;
        return NULL;
}

typedef struct {
        const char *name;
        ComponentFn fn;
} ComponentEntry;

static const ComponentEntry component_registry[] = {
    {"side_cover", component_side_cover},
    {"playlist_header", component_playlist_header},
    {"playlist_rows", component_playlist_rows},
    {"library_header", component_library_header},
    {"library_rows", component_library_rows},
    {"search", component_search},
    {"search_header", component_search_header},
    {"search_box", component_search_box},
    {"search_results", component_search_results},
    {"version", component_version},
    {"help", component_help},
    {"footer", component_footer},
    {"playback_status", component_playback_status},
    {"error_row", component_error_row},
    {"track", component_track},
    {"track_landscape_lyrics", component_track_landscape_lyrics},
    {"track_landscape_normal", component_track_landscape_normal},
    {"track_portrait_lyrics", component_track_portrait_lyrics},
    {"track_portrait_normal", component_track_portrait_normal},
    {"track_portrait", component_track_portrait},
    {"track_landscape", component_track_landscape},
    {"track_header", component_track_header},
    {"metadata", component_metadata},
    {"visualizer", component_visualizer},
    {"progress_bar", component_progress_bar},
    {"vis_and_progress_bar", component_vis_and_progress_bar},
    {"time", component_time},
    {"time_simple", component_time_simple},
    {"time_simple_and_vol", component_time_simple_and_vol},
    {"volume", component_volume},
    {"cover", component_cover},
    {"cover_centered", component_cover_centered},
    {"logo", component_logo},
    {"logo_art", component_logo_art},
    {"now_playing", component_now_playing},
    {"now_playing_and_artist", component_now_playing_and_artist},
    {"landscape_cover", component_landscape_cover},
    {"timestamped_lyrics", component_timestamped_lyrics},
    {"lyrics_page", component_lyrics_page},
    {"empty", component_empty},
    {NULL, NULL}};

static inline ComponentFn find_component(const char *name)
{
        for (int i = 0; component_registry[i].name != NULL; i++) {
                if (strcmp(component_registry[i].name, name) == 0)
                        return component_registry[i].fn;
        }
        return NULL;
}

static inline DirtyFlags parse_dirty(const char *str)
{
        if (strcmp(str, "refresh") == 0)
                return DIRTY_REFRESH;
        if (strcmp(str, "song") == 0)
                return DIRTY_SONG;
        if (strcmp(str, "library") == 0)
                return DIRTY_LIBRARY;
        if (strcmp(str, "playlist") == 0)
                return DIRTY_PLAYLIST;
        if (strcmp(str, "search") == 0)
                return DIRTY_SEARCH;
        if (strcmp(str, "footer") == 0)
                return DIRTY_FOOTER;
        if (strcmp(str, "visualizer") == 0)
                return DIRTY_VISUALIZER;
        if (strcmp(str, "title") == 0)
                return DIRTY_TITLE;
        if (strcmp(str, "none") == 0)
                return DIRTY_NONE;
        return DIRTY_ALL;
}

static int parse_col(const char *str)
{
        if (!str || !*str)
                return 0;

        if (strcmp(str, "indent") == 0)
                return COL_INDENT;

        if (strcmp(str, "indent_wide") == 0)
                return COL_INDENT;

        return atoi(str);
}

k_Size parse_size(const char *str)
{
        if (strcmp(str, "auto") == 0)
                return (k_Size){SIZE_AUTO, 0};

        if (strncmp(str, "fixed:", 6) == 0)
                return (k_Size){SIZE_FIXED, atoi(str + 6)};

        if (strcmp(str, "indent") == 0)
                return (k_Size){SIZE_INDENT, 0};

        if (strcmp(str, "indent_wide") == 0)
                return (k_Size){SIZE_INDENT_WIDE, 0};

        if (strcmp(str, "from_width") == 0)
                return (k_Size){SIZE_FROM_WIDTH, 0};

        if (strcmp(str, "from_height") == 0)
                return (k_Size){SIZE_FROM_HEIGHT, 0};

        if (strncmp(str, "percent:", 8) == 0)
                return (k_Size){SIZE_PERCENT, atoi(str + 8)};

        if (strncmp(str, "window_minus:", 13) == 0)
                return (k_Size){SIZE_WINDOW_MINUS, atoi(str + 13)};

        return (k_Size){SIZE_AUTO, 0};
}

Layout *load_layout_from_config(const char *layout_name)
{
        if (!config_has_section(layout_name))
                return NULL;

        Layout *layout = calloc(1, sizeof(Layout));

        Row *current_row = NULL;
        Pane *current_pane = NULL;

        for (ConfigEntry *e = config_head; e; e = e->next) {

                if (strcmp(e->section, layout_name) != 0)
                        continue;

                // Start new row
                if (strcmp(e->key, "row") == 0) {

                        if (layout->row_count >= MAX_ROWS)
                                break;

                        current_row = &layout->rows[layout->row_count++];
                        *current_row = (Row){0};

                        current_pane = NULL;

                        continue;
                }

                // Ignore anything before first row
                if (!current_row)
                        continue;

                // Start new pane
                if (strcmp(e->key, "pane") == 0) {

                        if (current_row->pane_count >= MAX_PANES)
                                continue;

                        current_pane =
                            &current_row->panes[current_row->pane_count++];

                        *current_pane = (Pane){0};

                        current_pane->redraws_on = DIRTY_REFRESH;

                        continue;
                }

                // Row-level properties
                if (!current_pane) {

                        if (strcmp(e->key, "height") == 0) {

                                current_row->height = parse_size(e->value);

                        } else if (strcmp(e->key, "col") == 0) {

                                current_row->col = parse_col(e->value);
                        }

                        continue;
                }

                // Pane-level properties
                if (strcmp(e->key, "component") == 0) {

                        current_pane->fn = find_component(e->value);

                } else if (strcmp(e->key, "dirty") == 0) {

                        current_pane->redraws_on = parse_dirty(e->value);

                } else if (strcmp(e->key, "offsetX") == 0) {

                        current_pane->offsetX = atoi(e->value);

                } else if (strcmp(e->key, "offsetY") == 0) {

                        current_pane->offsetY = atoi(e->value);

                } else if (strcmp(e->key, "width") == 0) {

                        current_pane->width = parse_size(e->value);

                        current_pane->align = ALIGN_LEFT;

                } else if (strcmp(e->key, "layout") == 0) {

                        current_pane->child = load_layout_from_config(e->value);
                }
        }

        return layout;
}

const char *get_system_data_dir(void)
{
    return PREFIX "/share/kew";
}

static bool copy_layout_file(const char *src_name,
                             const char *dst_path)
{
        char *config_path = get_config_path();

        if (!config_path)
                return false;

        char src_path[KEW_PATH_MAX];

        char system_layouts[KEW_PATH_MAX];
        snprintf(system_layouts, sizeof(system_layouts), "%s/layouts", get_system_data_dir());
        DIR *dir = opendir(system_layouts);
        if (!dir) {
                snprintf(system_layouts, sizeof(system_layouts), "/usr/share/kew/layouts");
                dir = opendir(system_layouts);
        }

        if (!dir) {
                free(config_path);
                k_log("inacessible system_layouts dir: %s", system_layouts);
                set_error_message("Couldn't copy layouts. Directory wasn't found or kew doesn't have permissions to read from it.");
                quit();
                return false;
        }

        if (snprintf(src_path, sizeof(src_path),
                     "%s/%s",
                     system_layouts,
                     src_name) >= (int)sizeof(src_path)) {
                free(config_path);
                closedir(dir);
                return false;
        }

        free(config_path);
        closedir(dir);

        return copy_file(src_path, dst_path);
}

static bool ensure_layout_file(const char *config_path,
                               const char *filename,
                               const char *backup_name)
{
        struct stat st;

        char layout_path[KEW_PATH_MAX];
        char backup_path[KEW_PATH_MAX];

        if (snprintf(layout_path, sizeof(layout_path),
                     "%s/%s",
                     config_path,
                     filename) >= (int)sizeof(layout_path)) {
                return false;
        }

        if (snprintf(backup_path, sizeof(backup_path),
                     "%s/%s",
                     config_path,
                     backup_name) >= (int)sizeof(backup_path)) {
                return false;
        }

        bool regenerate = false;

        // File missing → generate
        if (stat(layout_path, &st) == -1) {
                regenerate = true;
        } else {

                // Read version from existing config
                FILE *fp = fopen(layout_path, "r");

                if (fp) {
                        char line[256];
                        int version = 0;

                        while (fgets(line, sizeof(line), fp)) {
                                if (sscanf(line, "version=%d", &version) == 1)
                                        break;
                        }

                        fclose(fp);

                        if (version == 0 || version < KEW_LAYOUT_VERSION)
                                regenerate = true;

                } else {
                        regenerate = true;
                }
        }

        if (!regenerate)
                return false;

        // Backup old file if it exists
        if (stat(layout_path, &st) == 0) {
                rename(layout_path, backup_path);
        }

        return copy_layout_file(filename, layout_path);
}

bool ensure_default_layouts(void)
{
        char *config_path = get_config_path();

        if (!config_path)
                return false;

        char layouts_path[KEW_PATH_MAX];
        if (snprintf(layouts_path, sizeof(layouts_path), "%s/layouts", config_path) >= (int)sizeof(layouts_path)) {
                free(config_path);
                return false;
        }

        struct stat st;
        if (stat(layouts_path, &st) == -1) {
                // Directory doesn't exist → create it
                if (create_directory(layouts_path) == -1) {
                        free(config_path);
                        return false;
                }
        }

        bool changed = false;

        changed |= ensure_layout_file(layouts_path, "current.layout", "current.layout.bak");

        changed |= ensure_layout_file(layouts_path, "alt.layout", "alt.layout.bak");

        changed |= ensure_layout_file(layouts_path, "LAYOUTS-HOWTO.md", "LAYOUTS-HOWTO.md.bak");

        free(config_path);

        return changed;
}
