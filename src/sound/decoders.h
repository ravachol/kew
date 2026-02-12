/**
 * @file sound.[h]
 * @brief Decoders.
 *
 * Decoder related functions.
 */

#ifndef DECODERS_H
#define DECODERS_H

#include "common/appstate.h"

#include "audiotypes.h"
#include "webm.h"

#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <stdbool.h>


/**
 * @brief Switches between two specific decoders.
 *
 * This function toggles the given decoder index between 0 and 1. It is used to
 * switch between different decoder configurations in a decoder chain.
 *
 * @param decoder_index A pointer to the decoder index to toggle.
 */
void switch_specific_decoder(int *decoder_index);


/**
 * @brief Switches between multiple decoders in the system.
 *
 * This function toggles the decoder index for all available decoders in the system.
 * It is used to switch between different decoder types (e.g., Opus, Vorbis, WebM, etc.)
 *
 * @see switch_specific_decoder
 */
void switch_decoder(void);


/**
 * @brief Uninitializes a given decoder.
 *
 * This function releases the resources held by a decoder and uninitializes it.
 * It is used to properly shut down a decoder when it's no longer needed.
 *
 * @param decoder A pointer to the decoder to be uninitialized.
 */
void uninit_ma_decoder(void *decoder);


/**
 * @brief Uninitializes an Opus decoder.
 *
 * This function releases the resources held by an Opus decoder and uninitializes it.
 *
 * @param decoder A pointer to the Opus decoder to be uninitialized.
 */
void uninit_opus_decoder(void *decoder);


/**
 * @brief Uninitializes a Vorbis decoder.
 *
 * This function releases the resources held by a Vorbis decoder and uninitializes it.
 *
 * @param decoder A pointer to the Vorbis decoder to be uninitialized.
 */
void uninit_vorbis_decoder(void *decoder);


/**
 * @brief Uninitializes a WebM decoder.
 *
 * This function releases the resources held by a WebM decoder and uninitializes it.
 *
 * @param decoder A pointer to the WebM decoder to be uninitialized.
 */
void uninit_webm_decoder(void *decoder);


/**
 * @brief Uninitializes an M4A decoder (only if USE_FAAD is defined).
 *
 * This function releases the resources held by an M4A decoder and uninitializes it.
 *
 * @param decoder A pointer to the M4A decoder to be uninitialized.
 */
#ifdef USE_FAAD
void uninit_m4a_decoder(void *decoder);
#endif


/**
 * @brief Uninitializes the previously used decoder.
 *
 * This function uninitializes the decoder at the opposite index of the current decoder
 * and releases any associated resources.
 *
 * @param decoder_array An array of decoders.
 * @param index The index of the current decoder.
 * @param uninit The uninitialization function to call for the decoder type.
 */
void uninit_previous_decoder(void **decoder_array, int index, uninit_func uninit);


/**
 * @brief Checks if a built-in decoder is available for the given file.
 *
 * This function checks whether the given file can be decoded using a built-in decoder,
 * such as for WAV, FLAC, or MP3 files.
 *
 * @param file_path The file path to check for a suitable decoder.
 * @return True if a built-in decoder is available for the file, false otherwise.
 */
bool has_builtin_decoder(const char *file_path);


/**
 * @brief Clears the decoder chain.
 *
 * This function clears the current decoder chain, effectively disconnecting the
 * current decoder from the data source.
 */
void clear_decoder_chain(void);


/**
 * @brief Gets the first available decoder.
 *
 * This function returns the first decoder in the system.
 *
 * @return A pointer to the first available decoder.
 */
ma_decoder *get_first_decoder(void);


/**
 * @brief Gets the current built-in decoder.
 *
 * This function returns the currently active built-in decoder, or the first available
 * decoder if none are active.
 *
 * @return A pointer to the current built-in decoder.
 */
ma_decoder *get_current_builtin_decoder(void);


/**
 * @brief Gets the first available Opus decoder.
 *
 * This function returns the first Opus decoder in the system.
 *
 * @return A pointer to the first available Opus decoder.
 */
ma_libopus *get_first_opus_decoder(void);


/**
 * @brief Gets the current Opus decoder.
 *
 * This function returns the currently active Opus decoder, or the first available
 * Opus decoder if none are active.
 *
 * @return A pointer to the current Opus decoder.
 */
ma_libopus *get_current_opus_decoder(void);


/**
 * @brief Gets the first available Vorbis decoder.
 *
 * This function returns the first Vorbis decoder in the system.
 *
 * @return A pointer to the first available Vorbis decoder.
 */
ma_libvorbis *get_first_vorbis_decoder(void);


/**
 * @brief Gets the current Vorbis decoder.
 *
 * This function returns the currently active Vorbis decoder, or the first available
 * Vorbis decoder if none are active.
 *
 * @return A pointer to the current Vorbis decoder.
 */
ma_libvorbis *get_current_vorbis_decoder(void);


/**
 * @brief Gets the first available WebM decoder.
 *
 * This function returns the first WebM decoder in the system.
 *
 * @return A pointer to the first available WebM decoder.
 */
ma_webm *get_first_webm_decoder(void);


/**
 * @brief Gets the current WebM decoder.
 *
 * This function returns the currently active WebM decoder, or the first available
 * WebM decoder if none are active.
 *
 * @return A pointer to the current WebM decoder.
 */
ma_webm *get_current_webm_decoder(void);


/**
 * @brief Resets the decoders in the system.
 *
 * This function resets all decoders in the system, releasing resources and
 * uninitializing them as necessary.
 *
 * @see reset_decoders
 */
void reset_all_decoders(void);


/**
 * @brief Sets the next decoder in the decoder chain.
 *
 * This function adds the provided decoder to the chain, uninitializing the previous
 * decoder as necessary. It is used to set the next decoder for playback.
 *
 * @param decoder_array An array of decoders.
 * @param decoder A pointer to the next decoder to set.
 * @param first_decoder A pointer to the first decoder in the chain.
 * @param decoder_index The index of the current decoder.
 * @param uninit The uninitialization function to call for the decoder type.
 */
void set_next_decoder(void **decoder_array, void **decoder, void **first_decoder,
                      int *decoder_index, uninit_func uninit);


/**
 * @brief Prepares the next decoder for M4A files.
 *
 * This function prepares the next M4A decoder, checking if the format is compatible
 * and initializing the decoder accordingly.
 *
 * @param song_data A pointer to the song data that contains the file path.
 * @return 0 on success, -1 on failure.
 */
#ifdef USE_FAAD
int prepare_next_m4a_decoder(SongData *song_data);
#endif


/**
 * @brief Prepares the next Vorbis decoder.
 *
 * This function prepares the next Vorbis decoder, checking if the format is compatible
 * and initializing the decoder accordingly.
 *
 * @param filepath The file path of the Vorbis file to prepare.
 * @return 0 on success, -1 on failure.
 */
int prepare_next_vorbis_decoder(const char *filepath);


/**
 * @brief Prepares the next Opus decoder.
 *
 * This function prepares the next Opus decoder, checking if the format is compatible
 * and initializing the decoder accordingly.
 *
 * @param filepath The file path of the Opus file to prepare.
 * @return 0 on success, -1 on failure.
 */
int prepare_next_opus_decoder(const char *filepath);


/**
 * @brief Prepares the next WebM decoder.
 *
 * This function prepares the next WebM decoder, checking if the format is compatible
 * and initializing the decoder accordingly.
 *
 * @param song_data A pointer to the song data that contains the file path.
 * @return 0 on success, -1 on failure.
 */
int prepare_next_webm_decoder(SongData *song_data);


/**
 * @brief Prepares the next decoder for any supported format.
 *
 * This function prepares the next decoder for a general file, checking if the format
 * is compatible and initializing the decoder accordingly.
 *
 * @param filepath The file path to prepare.
 * @return 0 on success, -1 on failure.
 */
int prepare_next_decoder(const char *filepath);


/**
 * @brief Resets the decoders array to their initial state.
 *
 * This function resets the decoders array to its initial state, uninitializing and
 * releasing the resources of the decoders as necessary.
 *
 * @param decoder_array The array of decoders to reset.
 * @param first_decoder A pointer to the first decoder to reset.
 * @param array_size The size of the decoders array.
 * @param decoder_index The index of the current decoder in the array.
 * @param uninit The uninitialization function to use for each decoder type.
 */
void reset_decoders(void **decoder_array, void **first_decoder, int array_size,
                    int *decoder_index, uninit_func uninit);


/**
 * @brief Initializes the decoder chain for M4A files.
 *
 * This function prepares and initializes the decoder for M4A files, checking if the
 * format is compatible before proceeding.
 *
 * @param song_data A pointer to the song data containing the file path.
 * @return 0 on success, -1 on failure.
 */
 #ifdef USE_FAAD
int init_m4a_decoder(SongData *song_data);
#endif

/**
 * @brief Gets the first available M4A decoder.
 *
 * This function returns the first M4A decoder in the system.
 *
 * @return A pointer to the first available M4A decoder.
 */
#ifdef USE_FAAD
m4a_decoder *get_first_m4a_decoder(void);
#endif


/**
 * @brief Gets the current M4A decoder.
 *
 * This function returns the currently active M4A decoder, or the first available
 * M4A decoder if none are active.
 *
 * @return A pointer to the current M4A decoder.
 */
#ifdef USE_FAAD
m4a_decoder *get_current_m4a_decoder(void);
#endif

#endif
