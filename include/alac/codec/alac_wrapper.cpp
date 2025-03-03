

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ALACDecoder.h"
#include "ALACBitUtilities.h"
#include "EndianPortable.h"
#include "ALACAudioTypes.h"
#include <iostream>

// Opaque struct
typedef struct
{
        ALACDecoder decoder;
        uint32_t channels;
        uint32_t bit_depth;
        uint32_t frame_length;
} alac_decoder_t;

extern "C" alac_decoder_t *alac_decoder_init_from_config(const ALACSpecificConfig *parsedConfig)
{
        alac_decoder_t *ctx = new alac_decoder_t();
        if (!ctx)
                return NULL;

        ctx->channels = parsedConfig->numChannels;
        ctx->bit_depth = parsedConfig->bitDepth;
        ctx->frame_length = parsedConfig->frameLength;

        ALACSpecificConfig config_to_decoder = *parsedConfig;
        config_to_decoder.frameLength = Swap32NtoB(parsedConfig->frameLength);
        config_to_decoder.maxRun = Swap16NtoB(parsedConfig->maxRun);
        config_to_decoder.maxFrameBytes = Swap32NtoB(parsedConfig->maxFrameBytes);
        config_to_decoder.avgBitRate = Swap32NtoB(parsedConfig->avgBitRate);
        config_to_decoder.sampleRate = Swap32NtoB(parsedConfig->sampleRate);

        if (ctx->decoder.Init(&config_to_decoder, sizeof(config_to_decoder)) != 0)
        {
                delete ctx;
                return NULL;
        }

        return ctx;
}

extern "C" int alac_decoder_decode(alac_decoder_t *ctx, uint8_t *inbuffer, uint32_t inbuffer_size,
                                   int32_t *outbuffer, uint32_t *samples_decoded)
{
        if (!ctx || !samples_decoded)
                return -1;

        BitBuffer bits;
        BitBufferInit(&bits, inbuffer, inbuffer_size);
        BitBufferByteAlign(&bits, true);

        int32_t ret = ctx->decoder.Decode(&bits,
                                          (uint8_t *)outbuffer,
                                          ctx->frame_length,
                                          ctx->channels,
                                          samples_decoded);

        return ret;
}

extern "C" void alac_decoder_free(alac_decoder_t *ctx)
{
        if (ctx)
                delete ctx;
}
