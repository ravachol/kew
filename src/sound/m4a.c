
#define MINIMP4_IMPLEMENTATION
#include "../include/minimp4/minimp4.h"
#include <miniaudio.h>

/**
 * @file m4a.[c]
 * @brief M4A/AAC decoder interface.
 *
 * Provides decoding support for M4A and AAC-encoded audio files,
 * wrapping platform or library-specific decoding routines.
 */

#ifdef USE_FAAD
#include "m4a.h"
#endif
