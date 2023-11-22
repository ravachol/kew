#ifndef SOUND_COMMON_H
#define SOUND_COMMON_H
#include <stdbool.h>
#include <stdlib.h>
#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include <glib.h>
#include <FreeImage.h>
#include <stdatomic.h>
#include "file.h"
#include "cutils.h"
#include "stringfunc.h"
#include "player.h"

#define CHANNELS 2
#define SAMPLE_RATE 192000
#define SAMPLE_WIDTH 3
#define SAMPLE_FORMAT ma_format_s24

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 3600
#endif

#ifndef TAGSETTINGS_STRUCT
#define TAGSETTINGS_STRUCT

typedef struct
{
        char title[256];
        char artist[256];
        char album_artist[256];
        char album[256];
        char date[256];
} TagSettings;

#endif

#ifndef SONGDATA_STRUCT
#define SONGDATA_STRUCT

typedef struct
{
        gchar *trackId;
        char filePath[MAXPATHLEN];
        char coverArtPath[MAXPATHLEN];
        char pcmFilePath[MAXPATHLEN];
        unsigned char *red;
        unsigned char *green;
        unsigned char *blue;
        TagSettings *metadata;
        FIBITMAP *cover;
        double *duration;
        char *pcmFile;
        long pcmFileSize;
        bool hasErrors;
        bool deleted;

} SongData;

#endif

#ifndef USERDATA_STRUCT
#define USERDATA_STRUCT
typedef struct
{
        char *filenameA;
        char *filenameB;
        SongData *songdataA;
        SongData *songdataB;
        SongData *currentSongData;
        ma_uint32 currentPCMFrame;
} UserData;
#endif


#ifndef PCMFILEDATASOURCE_STRUCT
#define PCMFILEDATASOURCE_STRUCT
typedef struct
{
        ma_data_source_base base;
        UserData *pUserData;
        const char *filenameA;
        const char *filenameB;
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint32 currentPCMFrame;
        ma_decoder decoderA;
        ma_decoder decoderB;
        ma_decoder currentDecoder;
        FILE *fileA;
        FILE *fileB;
        bool switchFiles;
        int currentFileIndex;
        ma_uint64 totalFrames;
} PCMFileDataSource;
#endif

enum AudioImplementation
{
        PCM,
        BUILTIN,
        VORBIS,
        OPUS,
        M4A,
        NONE
};

extern bool doQuit;

ma_libopus *getOpus();

ma_libvorbis *getVorbis();

enum AudioImplementation getCurrentImplementationType();

void setCurrentImplementationType(enum AudioImplementation value);

int getBufferSize();

void setBufferSize(int value);

ma_decoder *getFirstDecoder();

ma_decoder *getCurrentDecoder();

ma_decoder *getPreviousDecoder();

void switchDecoder();

void switchVorbisDecoder();

void prepareNextDecoder(char *filepath);

void resetDecoders();

ma_libopus *getCurrentOpusDecoder();

void switchOpusDecoder();

void prepareNextOpusDecoder(char *filepath);

void resetOpusDecoders();

ma_libopus *getFirstOpusDecoder();

ma_libvorbis *getFirstVorbisDecoder();

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

ma_libvorbis *getCurrentVorbisDecoder();

void switchVorbisDecoder();

void prepareNextVorbisDecoder(char *filepath);

void resetVorbisDecoders();

ma_libvorbis *getFirstVorbisDecoder();

void getFileInfo(const char* filename, ma_uint32* sampleRate, ma_uint32* channels, ma_format* format);

void initAudioBuffer();

ma_int32 *getAudioBuffer();

void setAudioBuffer(ma_int32 *buf);

void resetAudioBuffer();

void freeAudioBuffer();

bool isRepeatEnabled();

void setRepeatEnabled(bool value);

bool isShuffleEnabled();

void setShuffleEnabled(bool value);

bool isSkipToNext();

void setSkipToNext(bool value);

double getSeekElapsed();

void setSeekElapsed(double value);

bool isEOFReached();

void setEOFReached();

void setEOFNotReached();

void skip();

bool isPlaybackDone();

float getSeekPercentage();

double getPercentageElapsed();

bool isSeekRequested();

void setSeekRequested(bool value);

void seekPercentage(float percent);

void resumePlayback();

void pausePlayback();

void cleanupPlaybackDevice();

void togglePausePlayback();

bool isPaused();

void resetDevice();

ma_device *getDevice();

bool hasBuiltinDecoder(char *filePath);

void activateSwitch(PCMFileDataSource *pPCMDataSource);

void executeSwitch(PCMFileDataSource *pPCMDataSource);

#endif