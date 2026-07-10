/**
 * Copyright (c) 2018 Parrot Drones SAS
 * Copyright (c) 2016 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MP4_H_
#define _MP4_H_

#ifndef ANDROID
#	ifndef _FILE_OFFSET_BITS
#		define _FILE_OFFSET_BITS 64
#	endif
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#	include <winsock2.h>
#else /* !_WIN32 */
#	include <arpa/inet.h>
#endif /* !_WIN32 */

#define ULOG_TAG libmp4
#include <ulog.h>

#include <futils/futils.h>
#include <libmp4.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/* Note: MAX_ALLOC_SIZE must be large enough to hold thumbnail */
#define MAX_ALLOC_SIZE (10 * 1000 * 1000)

#define UNUSED(x) (void)(x)


/* clang-format off */
#define MP4_ISOM                            0x69736f6d /* "isom" */
#define MP4_ISO2                            0x69736f32 /* "iso2" */
#define MP4_MP41                            0x6d703431 /* "mp41" */
#define MP4_AVC1                            0x61766331 /* "avc1" */
#define MP4_HVC1                            0x68766331 /* "hvc1" */
#define MP4_MP4A                            0x6d703461 /* "mp4a" */
#define MP4_UUID                            0x75756964 /* "uuid" */
#define MP4_MHLR                            0x6d686c72 /* "mhlr" */
#define MP4_ROOT_BOX                        0x726f6f74 /* "root" */
#define MP4_FILE_TYPE_BOX                   0x66747970 /* "ftyp" */
#define MP4_FREE_BOX                        0x66726565 /* "free" */
#define MP4_MDAT_BOX                        0x6d646174 /* "mdat" */
#define MP4_MOVIE_BOX                       0x6d6f6f76 /* "moov" */
#define MP4_USER_DATA_BOX                   0x75647461 /* "udta" */
#define MP4_MOVIE_HEADER_BOX                0x6d766864 /* "mvhd" */
#define MP4_TRACK_BOX                       0x7472616b /* "trak" */
#define MP4_TRACK_HEADER_BOX                0x746b6864 /* "tkhd" */
#define MP4_TRACK_REFERENCE_BOX             0x74726566 /* "tref" */
#define MP4_MEDIA_BOX                       0x6d646961 /* "mdia" */
#define MP4_MEDIA_HEADER_BOX                0x6d646864 /* "mdhd" */
#define MP4_HANDLER_REFERENCE_BOX           0x68646c72 /* "hdlr" */
#define MP4_MEDIA_INFORMATION_BOX           0x6d696e66 /* "minf" */
#define MP4_VIDEO_MEDIA_HEADER_BOX          0x766d6864 /* "vmhd" */
#define MP4_SOUND_MEDIA_HEADER_BOX          0x736d6864 /* "smhd" */
#define MP4_HINT_MEDIA_HEADER_BOX           0x686d6864 /* "hmhd" */
#define MP4_NULL_MEDIA_HEADER_BOX           0x6e6d6864 /* "nmhd" */
#define MP4_GENERIC_MEDIA_HEADER_BOX        0x676d6864 /* "gmhd" */
#define MP4_GENERIC_MEDIA_INFO_BOX          0x676d696e /* "gmin" */
#define MP4_DATA_INFORMATION_BOX            0x64696e66 /* "dinf" */
#define MP4_DATA_REFERENCE_BOX              0x64726566 /* "dref" */
#define MP4_SAMPLE_TABLE_BOX                0x7374626c /* "stbl" */
#define MP4_SAMPLE_DESCRIPTION_BOX          0x73747364 /* "stsd" */
#define MP4_AVC_DECODER_CONFIG_BOX          0x61766343 /* "avcC" */
#define MP4_HEVC_DECODER_CONFIG_BOX         0x68766343 /* "hvcC" */
#define MP4_AUDIO_DECODER_CONFIG_BOX        0x65736473 /* "esds" */
#define MP4_DECODING_TIME_TO_SAMPLE_BOX     0x73747473 /* "stts" */
#define MP4_SYNC_SAMPLE_BOX                 0x73747373 /* "stss" */
#define MP4_SAMPLE_SIZE_BOX                 0x7374737a /* "stsz" */
#define MP4_SAMPLE_TO_CHUNK_BOX             0x73747363 /* "stsc" */
#define MP4_CHUNK_OFFSET_BOX                0x7374636f /* "stco" */
#define MP4_CHUNK_OFFSET_64_BOX             0x636f3634 /* "co64" */
#define MP4_META_BOX                        0x6d657461 /* "meta" */
#define MP4_KEYS_BOX                        0x6b657973 /* "keys" */
#define MP4_ILST_BOX                        0x696c7374 /* "ilst" */
#define MP4_DATA_BOX                        0x64617461 /* "data" */
#define MP4_LOCATION_BOX                    0xa978797a /* ".xyz" */
#define MP4_EDTS_BOX                        0x65647473 /* "edts" */
#define MP4_ELST                            0x656c7374 /* "elst" */
#define MP4_PASP                            0x70617370 /* "pasp" */
#define MP4_BTRT                            0x62747274 /* "btrt" */

#define MP4_HANDLER_TYPE_VIDEO              0x76696465 /* "vide" */
#define MP4_HANDLER_TYPE_AUDIO              0x736f756e /* "soun" */
#define MP4_HANDLER_TYPE_HINT               0x68696e74 /* "hint" */
#define MP4_HANDLER_TYPE_METADATA           0x6d657461 /* "meta" */
#define MP4_HANDLER_TYPE_TEXT               0x74657874 /* "text" */

#define MP4_REFERENCE_TYPE_HINT             0x68696e74 /* "hint" */
#define MP4_REFERENCE_TYPE_DESCRIPTION      0x63647363 /* "cdsc" */
#define MP4_REFERENCE_TYPE_HINT_USED        0x68696e64 /* "hind" */
#define MP4_REFERENCE_TYPE_CHAPTERS         0x63686170 /* "chap" */

#define MP4_DATA_REFERENCE_TYPE_URL         0x75726c20 /* "url " */

#define MP4_XML_METADATA_SAMPLE_ENTRY       0x6d657478 /* "metx" */
#define MP4_TEXT_METADATA_SAMPLE_ENTRY      0x6d657474 /* "mett" */
#define MP4_TEXT_SAMPLE_ENTRY               0x74657874 /* "text" */

#define MP4_METADATA_NAMESPACE_MDTA         0x6d647461 /* "mdta" */
#define MP4_METADATA_HANDLER_TYPE_MDIR      0x6d646972 /* "mdir" */
#define MP4_METADATA_HANDLER_TYPE_APPL      0x6170706c /* "appl" */

#define MP4_METADATA_CLASS_UTF8             (1)
#define MP4_METADATA_CLASS_JPEG             (13)
#define MP4_METADATA_CLASS_PNG              (14)
#define MP4_METADATA_CLASS_BMP              (27)

#define MP4_METADATA_TAG_TYPE_ARTIST        0x00415254 /* ".ART" */
#define MP4_METADATA_TAG_TYPE_TITLE         0x006e616d /* ".nam" */
#define MP4_METADATA_TAG_TYPE_DATE          0x00646179 /* ".day" */
#define MP4_METADATA_TAG_TYPE_COMMENT       0x00636d74 /* ".cmt" */
#define MP4_METADATA_TAG_TYPE_COPYRIGHT     0x00637079 /* ".cpy" */
#define MP4_METADATA_TAG_TYPE_MAKER         0x006d616b /* ".mak" */
#define MP4_METADATA_TAG_TYPE_MODEL         0x006d6f64 /* ".mod" */
#define MP4_METADATA_TAG_TYPE_VERSION       0x00737772 /* ".swr" */
#define MP4_METADATA_TAG_TYPE_ENCODER       0x00746f6f /* ".too" */
#define MP4_METADATA_TAG_TYPE_COVER         0x636f7672 /* "covr" */
/* clang-format on */

#define MP4_METADATA_KEY_COVER "com.apple.quicktime.artwork"

#define MP4_MAC_TO_UNIX_EPOCH_OFFSET (0x7c25b080UL)

#define MP4_CHAPTERS_MAX 100
#define MP4_TRACK_REF_MAX 10

/* Track flags definition */
#define TRACK_FLAG_ENABLED (1 << 0)
#define TRACK_FLAG_IN_MOVIE (1 << 1)
#define TRACK_FLAG_IN_PREVIEW (1 << 2)

/* Buffer count is usually 6 (9 for IDR) */
#define MP4_DEFAULT_BUFFER_COUNT 16

#ifndef NAME_MAX
#	ifdef _MAX_FNAME
#		define NAME_MAX _MAX_FNAME
#	elif defined(FILENAME_MAX)
#		define NAME_MAX FILENAME_MAX
#	else
#		define NAME_MAX 255
#	endif
#endif
#ifndef PATH_MAX
#	ifdef _MAX_PATH
#		define PATH_MAX _MAX_PATH
#	else
#		define PATH_MAX 4096
#	endif
#endif
#ifndef METADATA_VALUE_MAX
#	define METADATA_VALUE_MAX UINT16_MAX
#endif


/**
 * Returns the length of a null-terminated string with an upper bound.
 * @param str: Input string (must be null-terminated).
 * @param max_len: Maximum allowed length (excluding null terminator).
 * @return The string length if:
    - str is not NULL
    - a null terminator is found before max_len
    Otherwise, returns 0.
 */
static inline size_t mp4_validate_str_len(const char *str, size_t max_len)
{
	size_t len;

	if (str == NULL)
		return 0;

	len = strnlen(str, max_len);
	if (len >= max_len)
		return 0;

	return len;
}


enum mp4_h265_nalu_type {
	MP4_H265_NALU_TYPE_UNKNOWN = 0, /* Unknown type */
	MP4_H265_NALU_TYPE_VPS = 32, /* Video parameter set */
	MP4_H265_NALU_TYPE_SPS = 33, /* Sequence parameter set */
	MP4_H265_NALU_TYPE_PPS = 34, /* Picture parameter set */
};


enum mp4_time_cmp {
	MP4_TIME_CMP_EXACT, /* Exact match */
	MP4_TIME_CMP_NEAREST, /* Nearest sample */
	MP4_TIME_CMP_LT, /* Less than */
	MP4_TIME_CMP_GT, /* Greater than */
	MP4_TIME_CMP_LT_EQ, /* Less than or equal */
	MP4_TIME_CMP_GT_EQ, /* Greater than or equal */
};


enum mp4_mux_meta_storage {
	/* Stored in moov/meta, keys/ilst format */
	MP4_MUX_META_META = 0,
	/* Stored in moov/udta/meta, ilst only format */
	MP4_MUX_META_UDTA,
	/* Stored in moov/udta, ilst only format */
	MP4_MUX_META_UDTA_ROOT,
};


struct mp4_box {
	uint32_t size;
	uint32_t type;
	uint64_t largesize;
	uint8_t uuid[16];
	unsigned int level;
	struct mp4_box *parent;
	struct list_node children;

	struct {
		off_t (*func)(struct mp4_mux *mux,
			      const struct mp4_box *box,
			      size_t maxBytes);
		void *args;
		int need_free;
	} writer;

	struct list_node node;
};


struct mp4_time_to_sample_entry {
	uint32_t sampleCount;
	uint32_t sampleDelta;
};


struct mp4_sample_to_chunk_entry {
	uint32_t firstChunk;
	uint32_t samplesPerChunk;
	uint32_t sampleDescriptionIndex;
};


/* track structure used by demuxer */
struct mp4_track {
	uint32_t id;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;
	uint32_t nextSample;
	uint64_t pendingSeekTime;
	uint32_t sampleCount;
	uint32_t *sampleSize;
	uint32_t sampleMaxSize;
	uint64_t *sampleDecodingTime;
	uint64_t *sampleOffset;
	uint32_t chunkCount;
	uint64_t *chunkOffset;
	uint32_t timeToSampleEntryCount;
	struct mp4_time_to_sample_entry *timeToSampleEntries;
	uint32_t sampleToChunkEntryCount;
	struct mp4_sample_to_chunk_entry *sampleToChunkEntries;
	uint32_t syncSampleEntryCount;
	uint32_t *syncSampleEntries;
	uint32_t referenceType;
	uint32_t referenceTrackId[MP4_TRACK_REF_MAX];
	unsigned int referenceTrackIdCount;

	struct mp4_video_decoder_config vdc;

	enum mp4_audio_codec audioCodec;
	uint32_t audioChannelCount;
	uint32_t audioSampleSize;
	uint32_t audioSampleRate;
	uint32_t audioSpecificConfigSize;
	uint8_t *audioSpecificConfig;

	char *contentEncoding;
	char *mimeFormat;
	unsigned int staticMetadataCount;
	char **staticMetadataKey;
	char **staticMetadataValue;

	struct mp4_track *metadata;
	struct mp4_track *chapters;

	char *name;
	int enabled;
	int in_movie;
	int in_preview;

	struct list_node node;
};


struct mp4_file {
	int fd;
	off_t fileSize;
	off_t readBytes;
	struct mp4_box *root;
	struct list_node tracks;
	unsigned int trackCount;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;

	char *chaptersName[MP4_CHAPTERS_MAX];
	uint64_t chaptersTime[MP4_CHAPTERS_MAX];
	unsigned int chaptersCount;
	unsigned int finalMetadataCount;
	char **finalMetadataKey;
	char **finalMetadataValue;
	char *udtaLocationKey;
	char *udtaLocationValue;
	off_t finalCoverOffset;
	uint32_t finalCoverSize;
	enum mp4_metadata_cover_type finalCoverType;

	off_t udtaCoverOffset;
	uint32_t udtaCoverSize;
	enum mp4_metadata_cover_type udtaCoverType;
	off_t metaCoverOffset;
	uint32_t metaCoverSize;
	enum mp4_metadata_cover_type metaCoverType;

	unsigned int udtaMetadataCount;
	unsigned int udtaMetadataParseIdx;
	char **udtaMetadataKey;
	char **udtaMetadataValue;
	unsigned int metaMetadataCount;
	char **metaMetadataKey;
	char **metaMetadataValue;
};


struct mp4_demux {
	struct mp4_file mp4;
};


struct mp4_mux_metadata_info {
	struct list_node *metadatas;
	uint8_t *cover;
	enum mp4_metadata_cover_type cover_type;
	size_t cover_size;
};


/* track structure used by muxer */
struct mp4_mux_track {
	/* Opaque handle used to identify the track. */
	uint32_t handle;
	/* Unset: track IDs are computed when writing to the MP4 file to ensure
	 * that enabled tracks have lowest IDs. */
	uint32_t id;
	char *name;
	uint32_t flags;
	uint32_t referenceTrackHandle[MP4_TRACK_REF_MAX];
	size_t referenceTrackHandleCount;
	uint32_t ref;
	int has_ref;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t duration_moov;
	uint64_t creation_time;
	uint64_t modification_time;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint32_t *sizes;
		uint64_t *decoding_times;
		uint64_t *offsets;
	} samples;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint64_t *offsets;
	} chunks;
	struct {
		uint32_t count;
		uint32_t capacity;
		struct mp4_time_to_sample_entry *entries;
	} time_to_sample;
	struct {
		uint32_t count;
		uint32_t capacity;
		struct mp4_sample_to_chunk_entry *entries;
	} sample_to_chunk;
	struct {
		uint32_t count;
		uint32_t capacity;
		uint32_t *entries;
	} sync;
	struct {
		uint32_t samples;
		uint32_t chunks;
		uint32_t time_to_sample;
		uint32_t sample_to_chunk;
		uint32_t sync;
	} stbl_index_write_count;
	bool track_info_written;
	uint32_t meta_write_count;

	union {
		struct mp4_video_decoder_config video;
		struct {
			enum mp4_audio_codec codec;
			uint32_t channel_count;
			uint32_t sample_size;
			uint32_t sample_rate;
			uint32_t specific_config_size;
			uint8_t *specific_config;
		} audio;
		struct {
			char *content_encoding;
			char *mime_type;
		} metadata;
	};

	struct mp4_mux_metadata_info track_metadata;

	struct list_node metadatas;
	struct list_node node;
	int64_t last_dts;
};

struct mp4_mux_metadata {
	char *key;
	char *value;
	enum mp4_mux_meta_storage storage;

	struct list_node node;
};

struct mp4_mux {
	int fd;
	char *filename;
	struct {
		char *tables_file;
		char *tmp_tables_file;
		int fd_tables;
		bool failed_in_close;
		bool enabled;
		uint32_t meta_write_count;
		bool thumb_written;
		bool check_storage_uuid;
	} recovery;
	uint64_t duration;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t timescale;
	off_t data_offset;
	off_t boxes_offset;
	/* Tracks */
	struct list_node tracks;
	uint32_t track_count;
	/* Metadata */
	struct list_node metadatas;
	struct mp4_mux_metadata_info file_metadata;
	bool max_tables_size_reached;
	struct {
		uint8_t *buf;
		off_t offset;
		off_t buf_size;
	} tables;
};


/** Recovery format version history:
 *  Version 1: moov atom entirely written in mrf file (Not supported anymore).
 *  Version 2: supports incremental tables (Not supported anymore).
 *  Version 3: supports tables file pre-allocation (Current, with header).
 */
#define MP4_RECOVERY_FORMAT_V1 1
#define MP4_RECOVERY_FORMAT_V2 2
#define MP4_RECOVERY_FORMAT_V3 3

/** Current version used for new file creation */
static const uint32_t MP4_RECOVERY_FORMAT_VERSION_CURRENT =
	MP4_RECOVERY_FORMAT_V3;

/** Minimum version that can be parsed by the current implementation */
static const uint32_t MP4_RECOVERY_FORMAT_VERSION_MIN_SUPPORTED =
	MP4_RECOVERY_FORMAT_V3;


#define OFF_T_TO_ERRNO(_off, default_errno)                                    \
	((_off >= INT_MIN) ? (int)_off : -default_errno)


#define MP4_READ_32(_file, _val32, _readBytes)                                 \
	do {                                                                   \
		_val32 = 0;                                                    \
		ssize_t _count = read(_file, &_val32, sizeof(uint32_t));       \
		if (_count == -1) {                                            \
			ULOG_ERRNO("read", errno);                             \
			return -errno;                                         \
		} else if ((size_t)_count != sizeof(uint32_t)) {               \
			ULOG_ERRNO("only %zd bytes read instead of %zu",       \
				   EIO,                                        \
				   _count,                                     \
				   sizeof(uint32_t));                          \
			return -EIO;                                           \
		}                                                              \
		_readBytes += sizeof(uint32_t);                                \
	} while (0)


#define MP4_READ_16(_file, _val16, _readBytes)                                 \
	do {                                                                   \
		_val16 = 0;                                                    \
		ssize_t _count = read(_file, &_val16, sizeof(uint16_t));       \
		if (_count == -1) {                                            \
			ULOG_ERRNO("read", errno);                             \
			return -errno;                                         \
		} else if ((size_t)_count != sizeof(uint16_t)) {               \
			ULOG_ERRNO("only %zd bytes read instead of %zu",       \
				   EIO,                                        \
				   _count,                                     \
				   sizeof(uint16_t));                          \
			return -EIO;                                           \
		}                                                              \
		_readBytes += sizeof(uint16_t);                                \
	} while (0)


#define MP4_READ_8(_file, _val8, _readBytes)                                   \
	do {                                                                   \
		_val8 = 0;                                                     \
		ssize_t _count = read(_file, &_val8, sizeof(uint8_t));         \
		if (_count == -1) {                                            \
			ULOG_ERRNO("read", errno);                             \
			return -errno;                                         \
		} else if ((size_t)_count != sizeof(uint8_t)) {                \
			ULOG_ERRNO("only %zd bytes read instead of %zu",       \
				   EIO,                                        \
				   _count,                                     \
				   sizeof(uint8_t));                           \
			return -EIO;                                           \
		}                                                              \
		_readBytes += sizeof(uint8_t);                                 \
	} while (0)


#define MP4_READ_SKIP(_file, _nBytes, _readBytes)                              \
	do {                                                                   \
		__typeof__(_readBytes) _i_nBytes = _nBytes;                    \
		if (_i_nBytes > 0) {                                           \
			off_t _ret = lseek(_file, _i_nBytes, SEEK_CUR);        \
			if (_ret == -1) {                                      \
				ULOG_ERRNO("lseek", errno);                    \
				return -errno;                                 \
			}                                                      \
			_readBytes += _i_nBytes;                               \
		}                                                              \
	} while (0)


#define MP4_WRITE_32(_mux, _val32, _writeBytes, _maxBytes)                     \
	do {                                                                   \
		if (_writeBytes + sizeof(uint32_t) > _maxBytes)                \
			return -ENOSPC;                                        \
		void *_dst = memcpy(_mux->tables.buf + _mux->tables.offset,    \
				    &_val32,                                   \
				    sizeof(uint32_t));                         \
		if (_dst == NULL) {                                            \
			int _ret = -errno;                                     \
			ULOG_ERRNO("memcpy", -_ret);                           \
			return _ret;                                           \
		}                                                              \
		_mux->tables.offset += sizeof(uint32_t);                       \
		_writeBytes += sizeof(uint32_t);                               \
	} while (0)


#define MP4_WRITE_32_INTERNAL(_mux, _val32, _writeBytes, _maxBytes, _reason)   \
	do {                                                                   \
		if (_writeBytes + sizeof(uint32_t) > _maxBytes)                \
			return -ENOSPC;                                        \
		ssize_t _res = write(_mux->fd, &val32, sizeof(uint32_t));      \
		if (_res != sizeof(uint32_t)) {                                \
			ULOGE("%s: write failed writing %s (%zd)",             \
			      _reason,                                         \
			      __func__,                                        \
			      _res);                                           \
			return -ENOSPC;                                        \
		}                                                              \
		_writeBytes += sizeof(uint32_t);                               \
	} while (0)


#define MP4_WRITE_16(_mux, _val16, _writeBytes, _maxBytes)                     \
	do {                                                                   \
		if (_writeBytes + sizeof(uint16_t) > _maxBytes)                \
			return -ENOSPC;                                        \
		void *_dst = memcpy(_mux->tables.buf + _mux->tables.offset,    \
				    &_val16,                                   \
				    sizeof(uint16_t));                         \
		if (_dst == NULL) {                                            \
			int _ret = -errno;                                     \
			ULOG_ERRNO("memcpy", -_ret);                           \
			return _ret;                                           \
		}                                                              \
		_mux->tables.offset += sizeof(uint16_t);                       \
		_writeBytes += sizeof(uint16_t);                               \
	} while (0)


#define MP4_WRITE_8(_mux, _val8, _writeBytes, _maxBytes)                       \
	do {                                                                   \
		if (_writeBytes + sizeof(uint8_t) > _maxBytes)                 \
			return -ENOSPC;                                        \
		void *_dst = memcpy(_mux->tables.buf + _mux->tables.offset,    \
				    &_val8,                                    \
				    sizeof(uint8_t));                          \
		if (_dst == NULL) {                                            \
			int _ret = -errno;                                     \
			ULOG_ERRNO("memcpy", -_ret);                           \
			return _ret;                                           \
		}                                                              \
		_mux->tables.offset += sizeof(uint8_t);                        \
		_writeBytes += sizeof(uint8_t);                                \
	} while (0)


#define MP4_WRITE_SKIP(_mux, _byteCount, _writeBytes, _maxBytes)               \
	do {                                                                   \
		__typeof__(_byteCount) _i_nBytes = _byteCount;                 \
		if (_writeBytes + _i_nBytes > _maxBytes)                       \
			return -ENOSPC;                                        \
		if (_mux->tables.offset + (off_t)_i_nBytes >                   \
		    _mux->tables.buf_size) {                                   \
			return -EPROTO;                                        \
		}                                                              \
		_mux->tables.offset += _i_nBytes;                              \
		_writeBytes += _i_nBytes;                                      \
	} while (0)


#define MP4_WRITE_SKIP_INTERNAL(_mux, _byteCount, _writeBytes, _maxBytes)      \
	do {                                                                   \
		__typeof__(_byteCount) _i_nBytes = _byteCount;                 \
		if (_writeBytes + _i_nBytes > _maxBytes)                       \
			return -ENOSPC;                                        \
		if (lseek(_mux->fd, _i_nBytes, SEEK_CUR) == -1) {              \
			ULOG_ERRNO("lseek", errno);                            \
			return -errno;                                         \
		}                                                              \
		_writeBytes += _i_nBytes;                                      \
	} while (0)


#define MP4_WRITE_ZEROES(_mux, _byteCount, _writeBytes, _maxBytes)             \
	do {                                                                   \
		__typeof__(_byteCount) _i_nBytes = _byteCount;                 \
		if (_writeBytes + _i_nBytes > _maxBytes)                       \
			return -ENOSPC;                                        \
		if (_mux->tables.offset + (off_t)_i_nBytes >                   \
		    _mux->tables.buf_size) {                                   \
			return -EPROTO;                                        \
		}                                                              \
		memset(_mux->tables.buf + _mux->tables.offset,                 \
		       0,                                                      \
		       (size_t)_i_nBytes);                                     \
		_mux->tables.offset += _i_nBytes;                              \
		_writeBytes += _i_nBytes;                                      \
	} while (0)


#define MP4_WRITE_CHECK_SIZE(_mux, _computedSize, _actualSize)                 \
	do {                                                                   \
		if (_computedSize != _actualSize) {                            \
			uint32_t _size32 = htonl(_actualSize);                 \
			if (_computedSize != 0)                                \
				ULOGE("bad size in box (%zu instead of %zu),"  \
				      " fixing size",                          \
				      (size_t)_actualSize,                     \
				      (size_t)_computedSize);                  \
			if (_mux->tables.offset < _actualSize) {               \
				ULOGE("tables offset too small");              \
				return -EPROTO;                                \
			}                                                      \
			_mux->tables.offset -= _actualSize;                    \
			void *_dst =                                           \
				memcpy(_mux->tables.buf + _mux->tables.offset, \
				       &_size32,                               \
				       sizeof(uint32_t));                      \
			if (_dst == NULL) {                                    \
				int _ret = -errno;                             \
				ULOG_ERRNO("memcpy", -_ret);                   \
				return _ret;                                   \
			}                                                      \
			_mux->tables.offset += _actualSize;                    \
		}                                                              \
	} while (0)


static inline char *xstrdup(const char *s)
{
	return s == NULL ? NULL : strdup(s);
}


struct mp4_box *mp4_box_new(struct mp4_box *parent);


/* mp4_box for mux creation */
struct mp4_box *mp4_box_new_container(struct mp4_box *parent, uint32_t type);
struct mp4_box *mp4_box_new_mvhd(struct mp4_box *parent, struct mp4_mux *mux);
struct mp4_box *mp4_box_new_tkhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_cdsc(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_chap(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_mdhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_hdlr(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_vmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_smhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_nmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_gmhd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_dref(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsd(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stts(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stss(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsc(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stsz(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_stco(struct mp4_box *parent,
				 struct mp4_mux_track *track);
struct mp4_box *mp4_box_new_meta(struct mp4_box *parent,
				 struct mp4_mux_metadata_info *meta_info);
struct mp4_box *mp4_box_new_meta_udta(struct mp4_box *parent,
				      struct mp4_mux_metadata_info *meta_info);
struct mp4_box *mp4_box_new_udta_entry(struct mp4_box *parent,
				       struct mp4_mux_metadata *meta);


void mp4_box_destroy(struct mp4_box *box);


void mp4_box_log(const struct mp4_box *box, int level);


off_t mp4_box_children_read(struct mp4_file *mp4,
			    struct mp4_box *parent,
			    off_t maxBytes,
			    struct mp4_track *track);


off_t mp4_box_ftyp_write(struct mp4_mux *mux);


off_t mp4_box_free_write(struct mp4_mux *mux, size_t len);


off_t mp4_box_mdat_write(const struct mp4_mux *mux, uint64_t size);


int mp4_track_is_sync_sample(const struct mp4_track *track,
			     unsigned int sampleIdx,
			     int *prevSyncSampleIdx);


int mp4_track_find_sample_by_time(const struct mp4_track *track,
				  uint64_t time,
				  enum mp4_time_cmp cmp,
				  int sync,
				  int start);


struct mp4_track *mp4_track_add(struct mp4_file *mp4);


int mp4_track_remove(struct mp4_file *mp4, struct mp4_track *track);


struct mp4_track *mp4_track_find(const struct mp4_file *mp4,
				 const struct mp4_track *track);


struct mp4_track *mp4_track_find_by_idx(const struct mp4_file *mp4,
					unsigned int track_idx);


struct mp4_track *mp4_track_find_by_id(const struct mp4_file *mp4,
				       unsigned int track_id);


void mp4_tracks_destroy(const struct mp4_file *mp4);


int mp4_tracks_build(struct mp4_file *mp4);


void mp4_video_decoder_config_destroy(struct mp4_video_decoder_config *vdc);


struct mp4_mux_track *mp4_mux_track_find_by_handle(const struct mp4_mux *mux,
						   uint32_t track_handle);


int mp4_mux_sort_tracks(struct mp4_mux *mux);


int mp4_mux_track_compute_tts(const struct mp4_mux *mux,
			      struct mp4_mux_track *track);


int mp4_mux_grow_samples(struct mp4_mux_track *track, int new_samples);


int mp4_mux_grow_chunks(struct mp4_mux_track *track, int new_chunks);


int mp4_mux_grow_tts(struct mp4_mux_track *track, int new_tts);


int mp4_mux_grow_stc(struct mp4_mux_track *track, int new_stc);


int mp4_mux_grow_sync(struct mp4_mux_track *track, int new_sync);


int mp4_mux_incremental_sync(struct mp4_mux *mux);


int mp4_mux_fill_from_file(const struct mp4_recovery_tables_header *header,
			   const char *tables_file,
			   struct mp4_mux *mux,
			   char **error_msg);


int mp4_mux_recovery_tables_header_fill(
	const struct mp4_mux *mux,
	struct mp4_recovery_tables_header *header);


int mp4_recovery_tables_header_write(
	int tables_file_fd,
	const struct mp4_recovery_tables_header *header,
	bool first_write);


#endif /* !_MP4_H_ */
