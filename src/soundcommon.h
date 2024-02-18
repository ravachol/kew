#ifndef SOUND_COMMON_H
#define SOUND_COMMON_H
#include <stdbool.h>
#include <stdlib.h>
#include <miniaudio.h>
#include <miniaudio_libopus.h>
#include <miniaudio_libvorbis.h>
#include "m4a.h"
#include <glib.h>
#include <FreeImage.h>
#include <stdatomic.h>
#include "file.h"
#include "cutils.h"
#include "player.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE 4800
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
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        TagSettings *metadata;
        FIBITMAP *cover;
        double duration;
        bool hasErrors;
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
        bool songdataADeleted;
        bool songdataBDeleted;
        SongData *currentSongData;
        ma_uint32 currentPCMFrame;
} UserData;
#endif


#ifndef AUDIODATA_STRUCT
#define AUDIODATA_STRUCT
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
        bool endOfListReached;
        bool restart;
} AudioData;
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

extern bool allowNotifications;

extern bool doQuit;

extern pthread_mutex_t dataSourceMutex;

extern pthread_mutex_t switchMutex;

enum AudioImplementation getCurrentImplementationType();

void setCurrentImplementationType(enum AudioImplementation value);

int getBufferSize();

void setBufferSize(int value);

void setPlayingStatus(bool playing);

bool isPlaying();

ma_decoder *getFirstDecoder();

ma_decoder *getCurrentBuiltinDecoder();

ma_decoder *getPreviousDecoder();

ma_format getCurrentFormat();

void switchDecoder();

void switchVorbisDecoder();

int prepareNextDecoder(char *filepath);

void resetDecoders();

ma_libopus *getCurrentOpusDecoder();

void switchOpusDecoder();

int prepareNextOpusDecoder(char *filepath);

void resetOpusDecoders();

m4a_decoder *getCurrentM4aDecoder();

void switchM4aDecoder();

m4a_decoder *getFirstM4aDecoder();

ma_libopus *getFirstOpusDecoder();

ma_libvorbis *getFirstVorbisDecoder();

void getVorbisFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getM4aFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

void getOpusFileInfo(const char *filename, ma_format *format, ma_uint32*channels, ma_uint32 *sampleRate, ma_channel *channelMap);

ma_libvorbis *getCurrentVorbisDecoder();

void switchVorbisDecoder();

int prepareNextVorbisDecoder(char *filepath);

int prepareNextM4aDecoder(char *filepath);

void resetVorbisDecoders();

void resetM4aDecoders();

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

bool isImplSwitchReached();

void setImplSwitchReached();

void setImplSwitchNotReached();

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

void activateSwitch(AudioData *pPCMDataSource);

void executeSwitch(AudioData *pPCMDataSource);

int displaySongNotification(const char *artist, const char *title, const char *cover);

#endif
