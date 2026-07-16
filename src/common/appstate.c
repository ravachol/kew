/**
 * @file appstate.c
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#ifdef _WIN32
#include <winsock2.h>
#endif

/* Include after chafa.h for G_OS_WIN32 */
#ifdef _WIN32
#include <io.h>
#include <windows.h>

static UINT saved_console_output_cp;
static UINT saved_console_input_cp;
static int win32_stdout_is_file = 0;

#else
#include <sys/ioctl.h> /* ioctl */
#endif

#include "appstate.h"

#include "common/appstate.h"
#include "common/model.h"
#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

sound_system_t *sound_sys = NULL;

RenderContext render_context;

Model model;

static const char LIBRARY_FILE[] = "library.dat";

static const char ARTISTS_DB_FILE[] = "artists.db";

static char library_real_path_if_diff[KEW_PATH_MAX] = {0};

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

void artists_db_init(void)
{
        Model *model = get_model();

        char filepath[PATH_MAX];

        snprintf(filepath, sizeof(filepath), "%s/kew/%s", KEW_DATADIR, ARTISTS_DB_FILE);

        if (!exists_file(filepath)) {
                snprintf(filepath, sizeof(filepath), "/usr/share/kew/%s", ARTISTS_DB_FILE);

                if (!exists_file(filepath))
                        return;
        }

        if (model->db)
                free(model->db);

        model->db = malloc(sizeof(ArtistDb));

        if (db_open(model->db, filepath) == 0) {
                model->hasArtistDb = true;
        }
}

void artists_db_shutdown(void)
{
        Model *model = get_model();

        db_close(model->db);

        if (model->db)
                free(model->db);
}

#ifdef _WIN32

static bool get_windows_terminal_pixels(int *width, int *height)
{
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

        if (hIn == INVALID_HANDLE_VALUE)
                return false;

        DWORD oldMode;

        if (!GetConsoleMode(hIn, &oldMode))
                return false;

        DWORD mode = oldMode |
                     ENABLE_VIRTUAL_TERMINAL_INPUT;

        // Disable cooked input
        mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

        if (!SetConsoleMode(hIn, mode))
                return false;

        // Clear pending input
        FlushConsoleInputBuffer(hIn);

        printf("\033[14t");
        fflush(stdout);

        char buf[128];
        DWORD read;
        int pos = 0;

        DWORD start = GetTickCount();

        while (pos < (int)sizeof(buf) - 1) {

                if (GetTickCount() - start > 500)
                        break;

                if (!ReadFile(hIn,
                              buf + pos,
                              1,
                              &read,
                              NULL)) {
                        break;
                }

                if (read == 1) {

                        if (buf[pos] == 't') {
                                pos++;
                                break;
                        }

                        pos++;
                }
        }

        buf[pos] = 0;

        int h = 0;
        int w = 0;

        bool ok = sscanf(buf,
                         "\033[4;%d;%dt",
                         &h,
                         &w) == 2;

        SetConsoleMode(hIn, oldMode);

        if (ok) {
                *width = w;
                *height = h;
                return true;
        }

        return false;
}

#endif

void get_tty_size(TermSize *term_size_out)
{
        Model *model = get_model();
        TermSize term_size = {
            .cols = model->term_w,
            .rows = model->term_h,
            .width_pixels = -1,
            .height_pixels = -1};

#ifdef _WIN32

        int width = 0;
        int height = 0;

        if (get_windows_terminal_pixels(&width, &height)) {
                term_size.width_pixels = width;
                term_size.height_pixels = height;
                term_size.cell_width = width / model->term_w;
                term_size.cell_height = height / model->term_h;
        }

        // Fallback
        if ((term_size.width_pixels <= 0 ||
            term_size.height_pixels <= 0) || (term_size.cell_width <= 0 ||  term_size.cell_height <= 0)) {

                int cell_width = 10;
                int cell_height = 20;

                if (term_size.cell_width >  0)
                        cell_width = term_size.cell_width;

                if (term_size.cell_height >  0)
                        cell_height = term_size.cell_height;

                if (term_size.cols > 0 && term_size.rows > 0) {
                        term_size.width_pixels =
                            term_size.cols * cell_width;

                        term_size.height_pixels =
                            term_size.rows * cell_height;
                }
        }

#else

        struct winsize ws;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 ||
            ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 ||
            ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {

                term_size.cols = ws.ws_col;
                term_size.rows = ws.ws_row;
                term_size.width_pixels = ws.ws_xpixel;
                term_size.height_pixels = ws.ws_ypixel;
        }

#endif

        if (term_size.cols <= 0)
                term_size.cols = -1;

        if (term_size.rows <= 0)
                term_size.rows = -1;

        if (term_size.width_pixels <= 0)
                term_size.width_pixels = -1;

        if (term_size.height_pixels <= 0)
                term_size.height_pixels = -1;

        *term_size_out = term_size;
}

void tty_init(void)
{
#ifdef _WIN32
        {
                HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);

                saved_console_output_cp = GetConsoleOutputCP();
                saved_console_input_cp = GetConsoleCP();

                // Enable ANSI escape sequence parsing etc. on MS Windows command prompt
                if (chd != INVALID_HANDLE_VALUE) {
                        if (!SetConsoleMode(chd,
                                            ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN))
                                win32_stdout_is_file = TRUE;
                }

                // Set UTF-8 code page I/O
                SetConsoleOutputCP(65001);
                SetConsoleCP(65001);
        }
#endif
}

void tty_shutdown(void)
{
#ifdef _WIN32
        HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);

        if (chd != INVALID_HANDLE_VALUE && saved_console_mode_valid)
                SetConsoleMode(chd, saved_console_output_mode);

        SetConsoleOutputCP(saved_console_output_cp);
        SetConsoleCP(saved_console_input_cp);
#endif
}

// --- Constructor ---

void model_init(void)
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
        model.state.settings.LAST_ROW = _(" [F2 Playlist|F3 Library|F4 Track|F5 Search|F6 Help]");

        model.state.ui.rendered = false;

        model.state.ui.logfile_path = NULL;

        model.glimmer.active = false;
        model.title_delay.active = false;

        model.elapsed_seconds = 0.0;
        model.song_duration = 0.0;

        model.hasArtistDb = false;
        model.db = NULL;

        model.tick = 17;

        model.mouse_x = -1;
        model.mouse_y = -1;
        model.mouse_key = -1;

        pthread_mutex_init(&(model.playbackState.switch_mutex), NULL);
        pthread_mutex_init(&(model.state.library_mutex), NULL);
        pthread_mutex_init(&(model.state.drawbuffer_mutex), NULL);

        create_playlist(&model.playlist);
        create_playlist(&model.unshuffled_playlist);
        create_playlist(&model.favorites_playlist);
}

// --- Destructor ---

void playlists_shutdown(void)
{
        Model *model = get_model();
        free_playlist(&model->playlist);
        free_playlist(&model->unshuffled_playlist);
        free_playlist(&model->favorites_playlist);
}

void mutexes_shutdown(void)
{
        Model *model = get_model();
        pthread_mutex_destroy(&(model->playbackState.switch_mutex));
        pthread_mutex_destroy(&(model->state.library_mutex));
        pthread_mutex_destroy(&(model->state.drawbuffer_mutex));
}

void model_shutdown(void)
{
        mutexes_shutdown();
        playlists_shutdown();
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
