#ifndef ALAC_WRAPPER_H
#define ALAC_WRAPPER_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

        typedef struct
        {
                uint32_t frameLength;
                uint8_t compatibleVersion;
                uint8_t bitDepth;
                uint8_t pb;
                uint8_t mb;
                uint8_t kb;
                uint8_t numChannels;
                uint16_t maxRun;
                uint32_t maxFrameBytes;
                uint32_t avgBitRate;
                uint32_t sampleRate;
        } ALACSpecificConfig;

        typedef struct alac_decoder_t alac_decoder_t;

        alac_decoder_t *alac_decoder_init_from_config(const ALACSpecificConfig *parsedConfig);
        int alac_decoder_decode(alac_decoder_t *ctx, uint8_t *inbuffer, uint32_t inbuffer_size,
                                   int32_t *outbuffer, uint32_t *samples_decoded);
        void alac_decoder_free(alac_decoder_t *decoder);

#ifdef __cplusplus
}
#endif

#endif // ALAC_WRAPPER_H
