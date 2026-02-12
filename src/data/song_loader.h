/**
 * @file song_loader.h
 * @brief Song loading and preparation routines.
 *
 * Responsible for loading song data from file
 */

#include "common/appstate.h"

#include <stdbool.h>

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
