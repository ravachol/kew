/**
 * @file appstate.c
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#include "common/appstate.h"
#include "common/model.h"
#include "utils/term.h"
#include "utils/utils.h"

/* Include after chafa.h for G_OS_WIN32 */
#ifdef _WIN32
#include <windows.h>
#include <io.h>

static UINT saved_console_output_cp;
static UINT saved_console_input_cp;
static int win32_stdout_is_file = 0;

#else
#include <sys/ioctl.h> /* ioctl */
#endif

sound_system_t *sound_sys = NULL;

RenderContext render_context;

Model model;

static const char LIBRARY_FILE[] = "library.dat";

static char library_real_path_if_diff[PATH_MAX] = {0};

double pause_seconds = 0.0;

double total_pause_seconds = 0.0;

Node *next_song = NULL;

Node *try_next_song = NULL;

Node *song_to_start_from = NULL;

void free_playlist(PlayList **plist)
{
    if (!plist || !*plist)
        return;

    PlayList *list = *plist;

    empty_playlist(list);

    pthread_mutex_destroy(&list->mutex);

    free(list);

    *plist = NULL;
}

void create_playlist(PlayList **playlist)
{
        if (*playlist == NULL) {
                *playlist = malloc(sizeof(PlayList));
                if (*playlist == NULL) {
                        return;
                }

                (*playlist)->count = 0;
                (*playlist)->head = NULL;
                (*playlist)->tail = NULL;
                pthread_mutex_init(&(*playlist)->mutex, NULL);
        }
}

void get_tty_size(TermSize *term_size_out)
{
        TermSize term_size;

        term_size.cols = term_size.rows = term_size.width_pixels = term_size.height_pixels = -1;

#ifdef _WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
                CONSOLE_SCREEN_BUFFER_INFO csb_info;

                if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info)) {
                        term_size.cols = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
                        term_size.rows = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
                }
        }
#else
        {
                struct winsize w;
                gboolean have_winsz = FALSE;

                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
                        have_winsz = TRUE;

                if (have_winsz) {
                        term_size.cols = w.ws_col;
                        term_size.rows = w.ws_row;
                        term_size.width_pixels = w.ws_xpixel;
                        term_size.height_pixels = w.ws_ypixel;
                }
        }
#endif

        if (term_size.cols <= 0)
                term_size.cols = -1;
        if (term_size.rows <= 2)
                term_size.rows = -1;

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
#ifdef _WIN32
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

// --- Constructor ---

void init_model(void)
{
        model.playlist = NULL;
        model.unshuffled_playlist = NULL;
        model.favorites_playlist = NULL;

        model.library = NULL;
        model.songdata = NULL;

        model.library_updated = false;

        model.songdata_ok = false;
        model.volume = 50;
        model.is_paused = false;
        model.is_stopped = true;
        model.should_refresh = true;
        model.restore_volume = false;

        set_term_size();
        get_term_size(&model.term_w, &model.term_h);
        get_tty_size(&model.term_size);

        model.indent = 0;
        model.updateCounter = 0;

        model.current_path = NULL;
        model.current_hash = 0;
        model.last_cover_path_hash = (size_t)-1;

        set_dirty(DIRTY_NONE);

        model.glimmer.active = false;
        model.title_delay.active = false;

        model.search_results = NULL;
        model.state.ui.chosen_search_dir = NULL;
        model.state.ui.current_search_entry = NULL;
        model.state.ui.current_lib_entry = NULL;
        model.state.ui.chosen_dir = NULL;
        model.state.ui.playlist_node = NULL;

        model.state.ui.rendered = false;

        model.glimmer.active = false;
        model.title_delay.active = false;

        model.elapsed_seconds = 0.0;
        model.song_duration = 0.0;

        model.tick = 17;

        pthread_mutex_init(&(model.playbackState.switch_mutex), NULL);
        pthread_mutex_init(&(model.state.library_mutex), NULL);

        create_playlist(&model.playlist);
        create_playlist(&model.unshuffled_playlist);
        create_playlist(&model.favorites_playlist);
}

// --- Getters ---

Model *get_model()
{
        return &model;
}


RenderContext *get_render_context()
{
        return &render_context;
}

AppState *get_app_state()
{
        return &model.state;
}

AppSettings *get_app_settings()
{
        return &model.settings;
}

FileSystemEntry *get_library()
{
        return model.library;
}

PlaybackState *get_playback_state()
{
        return &model.playbackState;
}

char *get_library_file_path(void)
{
        return get_file_path(LIBRARY_FILE);
}

double get_pause_seconds(void)
{
        return pause_seconds;
}

double get_total_pause_seconds(void)
{
        return total_pause_seconds;
}

Node *get_next_song(void)
{
        return next_song;
}

Node *get_song_to_start_from(void)
{
        return song_to_start_from;
}

Node *get_try_next_song(void)
{
        return try_next_song;
}

ProgressBar *get_progress_bar(void)
{
        return &model.progressBar;
}

PlayList *get_playlist(void)
{
        return model.playlist;
}

PlayList *get_unshuffled_playlist(void)
{
        return model.unshuffled_playlist;
}

PlayList *get_favorites_playlist(void)
{
        return model.favorites_playlist;
}

// --- Setters ---

void set_library(FileSystemEntry *root)
{
        model.library = root;
}

void set_dirty(DirtyFlags dirty)
{
        if (dirty == DIRTY_NONE) {
                model.dirty = DIRTY_NONE;
                return;
        }
        model.dirty |= dirty;
}


void set_pause_seconds(double seconds)
{
        pause_seconds = seconds;
}

void set_total_pause_seconds(double seconds)
{
        total_pause_seconds = seconds;
}

void set_next_song(Node *node)
{
        next_song = node;
}

void set_song_to_start_from(Node *node)
{
        song_to_start_from = node;
}

void set_try_next_song(Node *node)
{
        try_next_song = node;
}

const char *get_library_real_path_if_diff(void)
{
        return library_real_path_if_diff;
}

void set_library_real_path_if_diff(const char *path)
{
        if (path == NULL) {
                library_real_path_if_diff[0] = '\0';
                return;
        }
        c_strcpy(library_real_path_if_diff, path, sizeof(library_real_path_if_diff));
}
