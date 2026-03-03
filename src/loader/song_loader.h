/**
 * @file song_loader.h
 * @brief Song loading and preparation routines.
 *
 * Responsible for loading song data from file
 */

#include "songdatatype.h"

#include <stdbool.h>

/**
 * @brief Data passed to the loading thread for asynchronous decoding.
 */
typedef struct
{
        char file_path[PATH_MAX];

        bool loadInSlotA;
        bool loadingFirstDecoder;
        bool replaceNextSong;

        SongData *songdataA; // The loaded songs, not the current ones until they are assigned
        SongData *songdataB;

        bool songdataADeleted;
        bool songdataBDeleted;

        pthread_mutex_t mutex;
} LoaderData;

/**
 * @brief Returns the loader data struct.
 *
 * @return Pointer to the LoaderData struct.
 *
 */
LoaderData *get_loader_data(void);

/**
 * @brief Loads all runtime data associated with a song file.
 *
 * Allocates and initializes a new SongData structure for the given
 * audio file path. This includes:
 * - Generating a unique track ID
 * - Loading embedded or external lyrics (.lrc)
 * - Extracting metadata (tags, duration, replay gain, etc.)
 * - Locating or extracting album artwork
 * - Loading cover bitmap data
 * - Computing the dominant UI color from the cover image
 *
 * If metadata extraction fails, fallback album artwork is searched
 * in the file's directory. If color extraction fails, default UI
 * colors are used.
 *
 * @param file_path Path to the audio file.
 *
 * @return Pointer to a newly allocated SongData structure.
 *         The caller takes ownership and must call unload_song_data()
 *         to free associated resources.
 *
 * @note The returned structure contains dynamically allocated members
 *       (e.g., metadata, cover bitmap, track ID, lyrics).
 * @warning This function does not return NULL on allocation failure;
 *          behavior depends on internal allocations.
 */
SongData *load_song_data(char *file_path);

/**
 * @brief Frees all resources associated with a SongData object.
 *
 * Releases:
 * - Cover bitmap memory
 * - Temporary cached cover art file (if applicable)
 * - Lyrics data
 * - Metadata structure
 * - Track ID string
 * - The SongData structure itself
 *
 * Sets the provided pointer to NULL after cleanup.
 *
 * @param songdata Address of the SongData pointer to unload.
 *
 * @note Safe to call with a NULL pointer or if *songdata is NULL.
 * @warning After this call, the SongData pointer must not be used.
 */
void unload_song_data(SongData **songdata);

/**
 * @brief Initializes the song loader module.
 *
 * This function resets the internal loader state, initializes control flags,
 * clears song data pointers, creates the temporary cache, and initializes
 * the internal mutex used for thread synchronization.
 *
 * @return int Returns non-zero (true) if the cache was successfully created
 *             and the loader initialized correctly. Returns 0 (false) if
 *             cache creation failed.
 *
 * @note Must be called before using any other song_loader_* functions.
 * @warning This function is not thread-safe and should be called during
 *          application initialization.
 */
int song_loader_init(void);

/**
 * @brief Destroys the song loader module and releases allocated resources.
 *
 * This function unloads any currently loaded songs, deletes the temporary
 * cache, and destroys the internal mutex.
 *
 * After calling this function, the song loader must not be used unless
 * reinitialized with song_loader_init().
 *
 * @note This function should be called during application shutdown to
 *       prevent memory leaks.
 * @warning This function is not thread-safe and must not be called while
 *          other threads are accessing the song loader.
 */
void song_loader_destroy(void);

/**
 * @brief Unloads all songs currently managed by the loader.
 *
 * Frees and clears any SongData assigned to slot A and slot B.
 * Internal state flags are updated accordingly.
 *
 * After calling this function, both slots will be empty and must be
 * reassigned before playback or processing.
 *
 * @note This function is typically called during shutdown or when
 *       resetting the loader state.
 */
void song_loader_unload_songs(void);

/**
 * @brief Assigns a SongData instance to slot A.
 *
 * The loader maintains two internal song slots (A and B) to support
 * preloading or seamless transitions between songs. This function
 * assigns the provided SongData to slot A.
 *
 * @param songdata Pointer to the SongData structure to assign to slot A.
 *                 Must remain valid for the duration of its use by the loader.
 *
 * @note Any previously assigned SongData in slot A may be unloaded
 *       or replaced depending on internal loader state.
 */
void song_loader_assign_slot_A(SongData *songdata);

/**
 * @brief Assigns a SongData instance to slot B.
 *
 * The loader maintains two internal song slots (A and B) to support
 * preloading or seamless transitions between songs. This function
 * assigns the provided SongData to slot B.
 *
 * @param songdata Pointer to the SongData structure to assign to slot B.
 *                 Must remain valid for the duration of its use by the loader.
 *
 * @note Any previously assigned SongData in slot B may be unloaded
 *       or replaced depending on internal loader state.
 */
void song_loader_assign_slot_B(SongData *songdata);
