/**
 * @file appstate.h
 * @brief Provides globally accessible state structs, getters and setters.
 *
 */

#ifndef APPSTATE_H
#define APPSTATE_H

#include "model.h"

#include "loader/songdatatype.h"

#include "data/playlist.h"

#include "common/path_max.h"

#ifndef KEW_DATADIR
#define KEW_DATADIR "/usr/local/share"
#endif

#include <gio/gio.h>
#include <glib.h>
#include <libintl.h>
#include <miniaudio.h>
#include <stdatomic.h>

#define _(STRING) gettext(STRING)

#ifndef G_USEC_PER_SEC
#define G_USEC_PER_SEC 1000000
#endif

#ifdef __APPLE__
#define ABSOLUTE_MIN_WIDTH 80
#else
#define ABSOLUTE_MIN_WIDTH 65
#endif

#define KEW_LAYOUT_VERSION 11

#define DEFAULT_NUM_PROGRESS_BARS 35

typedef struct {

    bool render_often;
    bool render_search;

    int footer_row;
    int footer_col;

    int resizeFlag;

} RenderContext;

/**
 * Initializes TTY-specific settings.
 *
 * On Windows, enables ANSI escape sequence processing and
 * configures the console to use UTF-8 for input and output.
 * On other platforms, this function currently performs no action.
 */
void tty_init(void);

/* ========================= CONSTRUCTOR ========================= */

/**@brief initializes the model */
void model_init(void);

/* ========================= ARTIST DB ========================= */

/**@brief opens the database that contains urls to artists homepages */
void artists_db_init(void);

/**@brief closes the database that contains urls to artists homepages */
void artists_db_shutdown(void);

/* ========================= GETTERS ========================= */

/** @brief return the global model */
Model *get_model(void);

/** @brief return the global render_context */
RenderContext *get_render_context();

/**
 * Retrieves the current terminal size.
 *
 * Fills the provided TermSize structure with terminal
 * dimensions in both character cells and pixels
 * (if available). Unsupported values are set to -1.
 *
 * @param term_size_out Output parameter receiving terminal size data
 */
void get_tty_size(TermSize *term_size_out);


/** @brief Get the global playback state. */
PlaybackState *get_playback_state(void);

/** @brief Get current pause duration in seconds. */
double get_pause_seconds(void);

/** @brief Get total accumulated pause duration. */
double get_total_pause_seconds(void);

/** @brief Create a new playlist instance. */
void create_playlist(PlayList **playlist);

/** @brief Get the next song in playback order. */
Node *get_next_song(void);

/** @brief Get the song designated to start playback from. */
Node *get_song_to_start_from(void);

/** @brief Get the candidate next song during rebuilding. */
Node *get_try_next_song(void);

/** @brief Get the active shuffled playlist. */
PlayList *get_playlist(void);

/** @brief Get the unshuffled playlist. */
PlayList *get_unshuffled_playlist(void);

/** @brief Get the favorites playlist. */
PlayList *get_favorites_playlist(void);

/** @brief Get the progress bar descriptor. */
ProgressBar *get_progress_bar(void);

/** @brief Get the global application state. */
AppState *get_app_state(void);

/** @brief Get application configuration settings. */
AppSettings *get_app_settings(void);

/** @brief Get the root library entry. */
FileSystemEntry *get_library(void);

/** @brief Get the library root file path. */
char *get_library_file_path(void);

/* ========================= SETTERS ========================= */


/** @brief Set pause duration in seconds. */
void set_pause_seconds(double seconds);

/** @brief Sets dirty flags for the renderer. */
void set_dirty(DirtyFlags dirty);

/** @brief Set total accumulated pause duration. */
void set_total_pause_seconds(double seconds);

/** @brief Set the next song pointer. */
void set_next_song(Node *node);

/** @brief Set the song to start playback from. */
void set_song_to_start_from(Node *node);

/** @brief Set a temporary next-song candidate. */
void set_try_next_song(Node *node);

/** @brief Set the library root entry. */
void set_library(FileSystemEntry *root);

/** @brief Free all playlist instances. */
void free_playlist(PlayList **plist);

/** @brief Set current song data. */
void set_current_song_data(SongData *song_data);

/** @brief Get the realpath of the library root if it differs from full_path, otherwise empty string. */
const char *get_library_real_path_if_diff(void);

/** @brief Set the realpath of the library root if it differs from full_path; pass NULL to clear. */
void set_library_real_path_if_diff(const char *path);

#endif
