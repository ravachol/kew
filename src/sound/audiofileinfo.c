#include "audiofileinfo.h"

#include "m4a.h"
#include "miniaudio_libopus.h"
#include "miniaudio_libvorbis.h"
#include "webm.h"


void getFileInfo(const char *filename, ma_uint32 *sampleRate,
                 ma_uint32 *channels, ma_format *format)
{
        ma_decoder tmp;
        if (ma_decoder_init_file(filename, NULL, &tmp) == MA_SUCCESS)
        {
                *sampleRate = tmp.outputSampleRate;
                *channels = tmp.outputChannels;
                *format = tmp.outputFormat;
                ma_decoder_uninit(&tmp);
        }
        else
        {
                // Handle file open error.
        }
}

void getVorbisFileInfo(const char *filename, ma_format *format,
                       ma_uint32 *channels, ma_uint32 *sampleRate,
                       ma_channel *channelMap)
{
        ma_libvorbis decoder;
        if (ma_libvorbis_init_file(filename, NULL, NULL, &decoder) ==
            MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libvorbis_get_data_format(&decoder, format, channels,
                                             sampleRate, channelMap,
                                             MA_MAX_CHANNELS);
                ma_libvorbis_uninit(&decoder, NULL);
        }
}

void getOpusFileInfo(const char *filename, ma_format *format,
                     ma_uint32 *channels, ma_uint32 *sampleRate,
                     ma_channel *channelMap)
{
        ma_libopus decoder;

        if (ma_libopus_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                ma_libopus_get_data_format(&decoder, format, channels,
                                           sampleRate, channelMap,
                                           MA_MAX_CHANNELS);
                ma_libopus_uninit(&decoder, NULL);
        }
}

void getWebmFileInfo(const char *filename, ma_format *format,
                     ma_uint32 *channels, ma_uint32 *sampleRate,
                     ma_channel *channelMap)
{
        ma_webm tmp;
        if (ma_webm_init_file(filename, NULL, NULL, &tmp) == MA_SUCCESS)
        {
                *sampleRate = tmp.sampleRate;
                *channels = tmp.channels;
                *format = tmp.format;
                ma_webm_uninit(&tmp, NULL);
        }
        (void)channelMap;
}
#ifdef USE_FAAD
void getM4aFileInfoFull(const char *filename, ma_format *format,
                    ma_uint32 *channels, ma_uint32 *sampleRate,
                    ma_channel *channelMap, int *avgBitRate,
                    k_m4adec_filetype *fileType)
{
        m4a_decoder decoder;
        if (m4a_decoder_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS)
        {
                *format = decoder.format;
                m4a_decoder_get_data_format(&decoder, format, channels,
                                            sampleRate, channelMap,
                                            MA_MAX_CHANNELS);
                *avgBitRate = decoder.avgBitRate / 1000;
                *fileType = decoder.fileType;
                m4a_decoder_uninit(&decoder, NULL);
        }
}

void getM4aFileInfo(
    const char *filename,
    ma_format *format,
    ma_uint32 *channels,
    ma_uint32 *sampleRate,
    ma_channel *channelMap
)
{
    int unusedBitRate;
    k_m4adec_filetype unusedFileType;

    // We just discard these values here
    getM4aFileInfoFull(filename, format, channels, sampleRate, channelMap,
                   &unusedBitRate, &unusedFileType);
}

void getM4aExtraInfo(
    const char *filename,
    int *avgBitRate,
    k_m4adec_filetype *fileType
)
{
    ma_format dummyFormat;
    ma_uint32 dummyChannels, dummySampleRate;
    ma_channel dummyChannelMap[MA_MAX_CHANNELS];

    getM4aFileInfoFull(filename,
                   &dummyFormat,
                   &dummyChannels,
                   &dummySampleRate,
                   dummyChannelMap,
                   avgBitRate,
                   fileType);
}
#endif
