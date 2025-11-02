#include "audio_file_info.h"
#ifdef USE_FAAD
#include "m4a.h"
#endif
#include "miniaudio_libopus.h"
#include "miniaudio_libvorbis.h"
#include "webm.h"

void get_file_info(const char *filename, ma_uint32 *sample_rate,
                   ma_uint32 *channels, ma_format *format)
{
        ma_decoder tmp;
        if (ma_decoder_init_file(filename, NULL, &tmp) == MA_SUCCESS) {
                *sample_rate = tmp.outputSampleRate;
                *channels = tmp.outputChannels;
                *format = tmp.outputFormat;
                ma_decoder_uninit(&tmp);
        } else {
                // Handle file open error.
        }
}

void get_vorbis_file_info(const char *filename, ma_format *format,
                          ma_uint32 *channels, ma_uint32 *sample_rate,
                          ma_channel *channel_map)
{
        ma_libvorbis decoder;
        if (ma_libvorbis_init_file(filename, NULL, NULL, &decoder) ==
            MA_SUCCESS) {
                *format = decoder.format;
                ma_libvorbis_get_data_format(&decoder, format, channels,
                                             sample_rate, channel_map,
                                             MA_MAX_CHANNELS);
                ma_libvorbis_uninit(&decoder, NULL);
        }
}

void get_opus_file_info(const char *filename, ma_format *format,
                        ma_uint32 *channels, ma_uint32 *sample_rate,
                        ma_channel *channel_map)
{
        ma_libopus decoder;

        if (ma_libopus_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS) {
                *format = decoder.format;
                ma_libopus_get_data_format(&decoder, format, channels,
                                           sample_rate, channel_map,
                                           MA_MAX_CHANNELS);
                ma_libopus_uninit(&decoder, NULL);
        }
}

void get_webm_file_info(const char *filename, ma_format *format,
                        ma_uint32 *channels, ma_uint32 *sample_rate,
                        ma_channel *channel_map)
{
        ma_webm tmp;
        if (ma_webm_init_file(filename, NULL, NULL, &tmp) == MA_SUCCESS) {
                *sample_rate = tmp.sample_rate;
                *channels = tmp.channels;
                *format = tmp.format;
                ma_webm_uninit(&tmp, NULL);
        }
        (void)channel_map;
}
#ifdef USE_FAAD
void get_m4a_file_info_full(const char *filename, ma_format *format,
                            ma_uint32 *channels, ma_uint32 *sample_rate,
                            ma_channel *channel_map, int *avg_bit_rate,
                            k_m4adec_filetype *file_type)
{
        m4a_decoder decoder;
        if (m4a_decoder_init_file(filename, NULL, NULL, &decoder) == MA_SUCCESS) {
                *format = decoder.format;
                m4a_decoder_get_data_format(&decoder, format, channels,
                                            sample_rate, channel_map,
                                            MA_MAX_CHANNELS);
                *avg_bit_rate = decoder.avg_bit_rate / 1000;
                *file_type = decoder.file_type;
                m4a_decoder_uninit(&decoder, NULL);
        }
}

void get_m4a_file_info(
    const char *filename,
    ma_format *format,
    ma_uint32 *channels,
    ma_uint32 *sample_rate,
    ma_channel *channel_map)
{
        int unusedBitRate;
        k_m4adec_filetype unusedFileType;

        // We just discard these values here
        get_m4a_file_info_full(filename, format, channels, sample_rate, channel_map,
                               &unusedBitRate, &unusedFileType);
}

#endif
