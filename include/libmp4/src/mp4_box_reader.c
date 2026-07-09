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

/**
 * ISO/IEC 14496-12 - ISO base media file format
 */

#include "mp4_priv.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

/* Enable to log all table entries */
#define LOG_ALL 0

#define MAX_ENTRY_COUNT (10000000)


#define CHECK_SIZE(_max, _expected)                                            \
	do {                                                                   \
		if ((int64_t)(_max) < (int64_t)(_expected)) {                  \
			ULOGE("invalid size: %" PRIi64 ", expected %" PRIi64   \
			      " min",                                          \
			      (int64_t)(_max),                                 \
			      (int64_t)(_expected));                           \
			return -EINVAL;                                        \
		}                                                              \
	} while (0)


struct mp4_box *mp4_box_new(struct mp4_box *parent)
{
	struct mp4_box *box = calloc(1, sizeof(*box));
	if (box == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return NULL;
	}
	list_node_unref(&box->node);
	list_init(&box->children);

	box->parent = parent;
	if (parent) {
		list_add_after(list_last(&parent->children), &box->node);
		box->level = parent->level + 1;
	}

	return box;
}


void mp4_box_destroy(struct mp4_box *box)
{
	if (box == NULL)
		return;

	struct mp4_box *child = NULL;
	struct mp4_box *tmp = NULL;
	list_walk_entry_forward_safe(&box->children, child, tmp, node)
	{
		mp4_box_destroy(child);
	}

	if (list_node_is_ref(&box->node))
		list_del(&box->node);
	if (box->writer.need_free)
		free(box->writer.args);
	free(box);
}


static void mp4_box_log_internal(const struct mp4_box *box,
				 int level,
				 int last,
				 uint64_t level_bf)
{
	char spaces[101];
	unsigned int indent;

	ULOG_ERRNO_RETURN_IF(box == NULL, EINVAL);

	indent = box->level;
	if (indent > 50)
		indent = 50;
	if (last && indent > 0)
		level_bf &= ~(UINT64_C(1) << (indent - 1));
	for (unsigned int i = 0; i < indent; i++) {
		int pipe = !!(level_bf & (UINT64_C(1) << i));
		char pipec = pipe ? '|' : ' ';
		spaces[2 * i] = (last && i == indent - 1) ? '\\' : pipec;
		spaces[2 * i + 1] = (i == indent - 1) ? '-' : ' ';
	}
	spaces[indent * 2] = '\0';
	char a = (char)((box->type >> 24) & 0xFF);
	char b = (char)((box->type >> 16) & 0xFF);
	char c = (char)((box->type >> 8) & 0xFF);
	char d = (char)(box->type & 0xFF);
	ULOG_PRI(level,
		 "%s- %c%c%c%c size %" PRIu64 "\n",
		 spaces,
		 isprint(a) ? a : '.',
		 isprint(b) ? b : '.',
		 isprint(c) ? c : '.',
		 isprint(d) ? d : '.',
		 (box->size == 1) ? box->largesize : box->size);

	const struct mp4_box *child = NULL;
	level_bf |= UINT64_C(1) << indent;
	list_walk_entry_forward(&box->children, child, node)
	{
		mp4_box_log_internal(child,
				     level,
				     list_is_last(&box->children, &child->node),
				     level_bf);
	}
}


void mp4_box_log(const struct mp4_box *box, int level)
{
	mp4_box_log_internal(box, level, 0, 0);
}


/**
 * ISO/IEC 14496-12 - chap. 4.3 - File Type Box
 */
static off_t mp4_box_ftyp_read(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 8);

	/* 'major_brand' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t majorBrand = ntohl(val32);
	ULOGD("- ftyp: major_brand=%c%c%c%c",
	      (char)((majorBrand >> 24) & 0xFF),
	      (char)((majorBrand >> 16) & 0xFF),
	      (char)((majorBrand >> 8) & 0xFF),
	      (char)(majorBrand & 0xFF));

	/* 'minor_version' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t minorVersion = ntohl(val32);
	ULOGD("- ftyp: minor_version=%" PRIu32, minorVersion);

	int k = 0;
	while (boxReadBytes + 4 <= maxBytes) {
		/* 'compatible_brands[]' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t compatibleBrands = ntohl(val32);
		ULOGD("- ftyp: compatible_brands[%d]=%c%c%c%c",
		      k,
		      (char)((compatibleBrands >> 24) & 0xFF),
		      (char)((compatibleBrands >> 16) & 0xFF),
		      (char)((compatibleBrands >> 8) & 0xFF),
		      (char)(compatibleBrands & 0xFF));
		k++;
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.2.2 - Movie Header Box
 */
static off_t mp4_box_mvhd_read(struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 25 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- mvhd: version=%d", version);
	ULOGD("- mvhd: flags=%" PRIu32, flags);

	if (version == 1) {
		CHECK_SIZE(maxBytes, 28 * 4);

		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- mvhd: creation_time=%" PRIu64, mp4->creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- mvhd: modification_time=%" PRIu64,
		      mp4->modificationTime);

		/* 'timescale' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->timescale = ntohl(val32);
		ULOGD("- mvhd: timescale=%" PRIu32, mp4->timescale);
		if (mp4->timescale == 0) {
			ULOGE("mvhd timescale is 0");
			return -EPROTO;
		}

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
				       mp4->timescale / 60 / 60);
		unsigned int min =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
					       mp4->timescale / 60 -
				       hrs * 60);
		unsigned int sec =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
					       mp4->timescale -
				       hrs * 60 * 60 - min * 60);
		ULOGD("- mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
		      mp4->duration,
		      hrs,
		      min,
		      sec);
	} else {
		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->creationTime = ntohl(val32);
		ULOGD("- mvhd: creation_time=%" PRIu64, mp4->creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->modificationTime = ntohl(val32);
		ULOGD("- mvhd: modification_time=%" PRIu64,
		      mp4->modificationTime);

		/* 'timescale' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->timescale = ntohl(val32);
		ULOGD("- mvhd: timescale=%" PRIu32, mp4->timescale);
		if (mp4->timescale == 0) {
			ULOGE("mvhd timescale is 0");
			return -EPROTO;
		}

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		mp4->duration = ntohl(val32);
		unsigned int hrs =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
				       mp4->timescale / 60 / 60);
		unsigned int min =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
					       mp4->timescale / 60 -
				       hrs * 60);
		unsigned int sec =
			(unsigned int)((mp4->duration + mp4->timescale / 2) /
					       mp4->timescale -
				       hrs * 60 * 60 - min * 60);
		ULOGD("- mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
		      mp4->duration,
		      hrs,
		      min,
		      sec);
	}

	/* 'rate' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float rate = (float)ntohl(val32) / 65536.;
	ULOGD("- mvhd: rate=%.4f", rate);

	/* 'volume' & 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	ULOGD("- mvhd: volume=%.2f", volume);

	/* 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'matrix' */
	for (int k = 0; k < 9; k++)
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'pre_defined' */
	for (int k = 0; k < 6; k++)
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'next_track_ID' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t next_track_ID = ntohl(val32);
	ULOGD("- mvhd: next_track_ID=%" PRIu32, next_track_ID);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.3.2 - Track Header Box
 */
static off_t mp4_box_tkhd_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 21 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- tkhd: version=%d", version);
	ULOGD("- tkhd: flags=%" PRIu32, flags);
	track->enabled = !!(flags & TRACK_FLAG_ENABLED);
	track->in_movie = !!(flags & TRACK_FLAG_IN_MOVIE);
	track->in_preview = !!(flags & TRACK_FLAG_IN_PREVIEW);

	if (version == 1) {
		CHECK_SIZE(maxBytes, 24 * 4);

		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- tkhd: creation_time=%" PRIu64, creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- tkhd: modification_time=%" PRIu64, modificationTime);

		/* 'track_ID' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->id = ntohl(val32);
		ULOGD("- tkhd: track_ID=%" PRIu32, track->id);

		/* 'reserved' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint64_t duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs =
			(unsigned int)((duration + mp4->timescale / 2) /
				       mp4->timescale / 60 / 60);
		unsigned int min =
			(unsigned int)((duration + mp4->timescale / 2) /
					       mp4->timescale / 60 -
				       hrs * 60);
		unsigned int sec =
			(unsigned int)((duration + mp4->timescale / 2) /
					       mp4->timescale -
				       hrs * 60 * 60 - min * 60);
		ULOGD("- tkhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
		      duration,
		      hrs,
		      min,
		      sec);
	} else {
		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t creationTime = ntohl(val32);
		ULOGD("- tkhd: creation_time=%" PRIu32, creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t modificationTime = ntohl(val32);
		ULOGD("- tkhd: modification_time=%" PRIu32, modificationTime);

		/* 'track_ID' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->id = ntohl(val32);
		ULOGD("- tkhd: track_ID=%" PRIu32, track->id);

		/* 'reserved' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t duration = ntohl(val32);
		unsigned int hrs = ((duration + mp4->timescale / 2) /
				    mp4->timescale / 60 / 60);
		unsigned int min =
			((duration + mp4->timescale / 2) / mp4->timescale / 60 -
			 hrs * 60);
		unsigned int sec =
			((duration + mp4->timescale / 2) / mp4->timescale -
			 hrs * 60 * 60 - min * 60);
		ULOGD("- tkhd: duration=%" PRIu32 " (%02d:%02d:%02d)",
		      duration,
		      hrs,
		      min,
		      sec);
	}

	/* 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'layer' & 'alternate_group' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	int16_t layer = (int16_t)(ntohl(val32) >> 16);
	int16_t alternateGroup = (int16_t)(ntohl(val32) & 0xFFFF);
	ULOGD("- tkhd: layer=%i", layer);
	ULOGD("- tkhd: alternate_group=%i", alternateGroup);

	/* 'volume' & 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	ULOGD("- tkhd: volume=%.2f", volume);

	/* 'matrix' */
	for (unsigned int k = 0; k < 9; k++)
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'width' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float width = (float)ntohl(val32) / 65536.;
	ULOGD("- tkhd: width=%.2f", width);

	/* 'height' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float height = (float)ntohl(val32) / 65536.;
	ULOGD("- tkhd: height=%.2f", height);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.3.3 - Track Reference Box
 */
static off_t mp4_box_tref_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 3 * 4);

	/* 'reference_type' size */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t referenceTypeSize = ntohl(val32);
	ULOGD("- tref: reference_type_size=%" PRIu32, referenceTypeSize);

	/* 'reference_type' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->referenceType = ntohl(val32);
	ULOGD("- tref: reference_type=%c%c%c%c",
	      (char)((track->referenceType >> 24) & 0xFF),
	      (char)((track->referenceType >> 16) & 0xFF),
	      (char)((track->referenceType >> 8) & 0xFF),
	      (char)(track->referenceType & 0xFF));

	/* 'track_IDs' */
	track->referenceTrackIdCount = 0;
	while ((boxReadBytes + 4 <= maxBytes) &&
	       (track->referenceTrackIdCount < MP4_TRACK_REF_MAX)) {
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->referenceTrackId[track->referenceTrackIdCount] =
			ntohl(val32);
		ULOGD("- tref: track_id=%" PRIu32,
		      track->referenceTrackId[track->referenceTrackIdCount]);
		track->referenceTrackIdCount++;
	}

	if (maxBytes - boxReadBytes > 0) {
		ULOGW("tref: track_IDs count exceeds internal max count (%d) - "
		      "subsequent references ignored",
		      MP4_TRACK_REF_MAX);
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.2 - Media Header Box
 */
static off_t mp4_box_mdhd_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 6 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- mdhd: version=%d", version);
	ULOGD("- mdhd: flags=%" PRIu32, flags);

	if (version == 1) {
		CHECK_SIZE(maxBytes, 9 * 4);

		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- mdhd: creation_time=%" PRIu64, track->creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->modificationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		ULOGD("- mdhd: modification_time=%" PRIu64,
		      track->modificationTime);

		/* 'timescale' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		ULOGD("- mdhd: timescale=%" PRIu32, track->timescale);
		if (track->timescale == 0) {
			ULOGE("mdhd timescale is 0");
			return -EPROTO;
		}

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)((track->duration +
						   track->timescale / 2) /
						  track->timescale / 60 / 60);
		unsigned int min =
			(unsigned int)((track->duration +
					track->timescale / 2) /
					       track->timescale / 60 -
				       hrs * 60);
		unsigned int sec = (unsigned int)((track->duration +
						   track->timescale / 2) /
							  track->timescale -
						  hrs * 60 * 60 - min * 60);
		ULOGD("- mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
		      track->duration,
		      hrs,
		      min,
		      sec);
	} else {
		/* 'creation_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->creationTime = ntohl(val32);
		ULOGD("- mdhd: creation_time=%" PRIu64, track->creationTime);

		/* 'modification_time' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->modificationTime = ntohl(val32);
		ULOGD("- mdhd: modification_time=%" PRIu64,
		      track->modificationTime);

		/* 'timescale' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		ULOGD("- mdhd: timescale=%" PRIu32, track->timescale);
		if (track->timescale == 0) {
			ULOGE("mdhd timescale is 0");
			return -EPROTO;
		}

		/* 'duration' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32);
		unsigned int hrs = (unsigned int)((track->duration +
						   track->timescale / 2) /
						  track->timescale / 60 / 60);
		unsigned int min =
			(unsigned int)((track->duration +
					track->timescale / 2) /
					       track->timescale / 60 -
				       hrs * 60);
		unsigned int sec = (unsigned int)((track->duration +
						   track->timescale / 2) /
							  track->timescale -
						  hrs * 60 * 60 - min * 60);
		ULOGD("- mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
		      track->duration,
		      hrs,
		      min,
		      sec);
	}

	/* 'language' & 'pre_defined' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint16_t language = (uint16_t)(ntohl(val32) >> 16) & 0x7FFF;
	ULOGD("- mdhd: language=%" PRIu16, language);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.5.2 - Video Media Header Box
 */
static off_t mp4_box_vmhd_read(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;
	uint16_t val16;

	CHECK_SIZE(maxBytes, 3 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- vmhd: version=%d", version);
	ULOGD("- vmhd: flags=%" PRIu32, flags);

	/* 'graphicsmode' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	uint16_t graphicsmode = ntohs(val16);
	ULOGD("- vmhd: graphicsmode=%" PRIu16, graphicsmode);

	/* 'opcolor' */
	uint16_t opcolor[3];
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	opcolor[0] = ntohs(val16);
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	opcolor[1] = ntohs(val16);
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	opcolor[2] = ntohs(val16);
	ULOGD("- vmhd: opcolor=(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
	      opcolor[0],
	      opcolor[1],
	      opcolor[2]);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.5.3 - Sound Media Header Box
 */
static off_t mp4_box_smhd_read(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 2 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- smhd: version=%d", version);
	ULOGD("- smhd: flags=%" PRIu32, flags);

	/* 'balance' & 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	float balance =
		(float)((int16_t)((ntohl(val32) >> 16) & 0xFFFF)) / 256.;
	ULOGD("- smhd: balance=%.2f", balance);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.5.4 - Hint Media Header Box
 */
static off_t mp4_box_hmhd_read(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 5 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- hmhd: version=%d", version);
	ULOGD("- hmhd: flags=%" PRIu32, flags);

	/* 'maxPDUsize' and 'avgPDUsize' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint16_t maxPDUsize = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	uint16_t avgPDUsize = (uint16_t)(ntohl(val32) & 0xFFFF);
	ULOGD("- hmhd: maxPDUsize=%" PRIu16, maxPDUsize);
	ULOGD("- hmhd: avgPDUsize=%" PRIu16, avgPDUsize);

	/* 'maxbitrate' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t maxbitrate = ntohl(val32);
	ULOGD("- hmhd: maxbitrate=%" PRIu32, maxbitrate);

	/* 'avgbitrate' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t avgbitrate = ntohl(val32);
	ULOGD("- hmhd: avgbitrate=%" PRIu32, avgbitrate);

	/* 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.5.5 - Null Media Header Box
 */
static off_t mp4_box_nmhd_read(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- nmhd: version=%d", version);
	ULOGD("- nmhd: flags=%" PRIu32, flags);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.4.3 - Handler Reference Box
 */
static off_t mp4_box_hdlr_read(const struct mp4_file *mp4,
			       const struct mp4_box *box,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	CHECK_SIZE(maxBytes, 6 * 4);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- hdlr: version=%d", version);
	ULOGD("- hdlr: flags=%" PRIu32, flags);

	/* 'pre_defined' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);

	/* 'handler_type' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t handlerType = ntohl(val32);
	ULOGD("- hdlr: handler_type=%c%c%c%c",
	      (char)((handlerType >> 24) & 0xFF),
	      (char)((handlerType >> 16) & 0xFF),
	      (char)((handlerType >> 8) & 0xFF),
	      (char)(handlerType & 0xFF));

	if (track && box && box->parent &&
	    (box->parent->type == MP4_MEDIA_BOX)) {
		switch (handlerType) {
		case MP4_HANDLER_TYPE_VIDEO:
			track->type = MP4_TRACK_TYPE_VIDEO;
			break;
		case MP4_HANDLER_TYPE_AUDIO:
			track->type = MP4_TRACK_TYPE_AUDIO;
			break;
		case MP4_HANDLER_TYPE_HINT:
			track->type = MP4_TRACK_TYPE_HINT;
			break;
		case MP4_HANDLER_TYPE_METADATA:
			track->type = MP4_TRACK_TYPE_METADATA;
			break;
		case MP4_HANDLER_TYPE_TEXT:
			track->type = MP4_TRACK_TYPE_TEXT;
			break;
		default:
			track->type = MP4_TRACK_TYPE_UNKNOWN;
			break;
		}
	}

	/* 'reserved' */
	unsigned int k;
	for (k = 0; k < 3; k++)
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

	char name[100];
	memset(name, 0, sizeof(name));
	for (k = 0; (k < sizeof(name) - 1) && (boxReadBytes < maxBytes); k++) {
		MP4_READ_8(mp4->fd, name[k], boxReadBytes);
		if (name[k] == '\0')
			break;
	}
	ULOGD("- hdlr: name=%s", name);

	if (track && box && box->parent &&
	    (box->parent->type == MP4_MEDIA_BOX)) {
		free(track->name);
		track->name = strdup(name);
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-15 - chap. 5.3.3.1 - AVC decoder configuration record
 */
static off_t mp4_box_avcc_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	off_t minBytes = 6;
	uint32_t val32;
	uint16_t val16;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, minBytes);

	/* 'version' & 'profile' & 'level' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	val32 = htonl(val32);
	uint8_t version = (val32 >> 24) & 0xFF;
	uint8_t profile = (val32 >> 16) & 0xFF;
	uint8_t profile_compat = (val32 >> 8) & 0xFF;
	uint8_t level = val32 & 0xFF;
	ULOGD("- avcC: version=%d", version);
	ULOGD("- avcC: profile=%d", profile);
	ULOGD("- avcC: profile_compat=%d", profile_compat);
	ULOGD("- avcC: level=%d", level);

	/* 'length_size' and 'sps_count' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	val16 = htons(val16);
	uint8_t lengthSize = ((val16 >> 8) & 0x3) + 1;
	uint8_t spsCount = val16 & 0x1F;
	ULOGD("- avcC: length_size=%d", lengthSize);
	ULOGD("- avcC: sps_count=%d", spsCount);

	minBytes += 2 * spsCount;
	CHECK_SIZE(maxBytes, minBytes);

	int i;
	for (i = 0; i < spsCount; i++) {
		/* 'sps_length' */
		MP4_READ_16(mp4->fd, val16, boxReadBytes);
		uint16_t spsLength = htons(val16);
		ULOGD("- avcC: sps_length=%" PRIu16, spsLength);

		minBytes += spsLength;
		CHECK_SIZE(maxBytes, minBytes);

		if ((!track->vdc.avc.sps) && spsLength) {
			/* First SPS found */
			track->vdc.avc.sps_size = spsLength;
			track->vdc.avc.sps = malloc(spsLength);
			if (track->vdc.avc.sps == NULL) {
				ULOG_ERRNO("malloc", ENOMEM);
				return -ENOMEM;
			}
			ssize_t count =
				read(mp4->fd, track->vdc.avc.sps, spsLength);
			if (count == -1) {
				ULOG_ERRNO("read", errno);
				return -errno;
			}
		} else {
			/* Ignore any other SPS */
			off_t ret = lseek(mp4->fd, spsLength, SEEK_CUR);
			if (ret == -1) {
				ULOG_ERRNO("lseek", errno);
				return -errno;
			}
		}
		boxReadBytes += spsLength;
	}

	minBytes++;
	CHECK_SIZE(maxBytes, minBytes);

	/* 'pps_count' */
	uint8_t ppsCount;
	MP4_READ_8(mp4->fd, ppsCount, boxReadBytes);
	ULOGD("- avcC: pps_count=%d", ppsCount);
	if (ppsCount > INT8_MAX) {
		ULOGE("ppsCount exceeds the maximum count %" PRIu8, ppsCount);
		return -EPROTO;
	}

	minBytes += 2 * ppsCount;
	CHECK_SIZE(maxBytes, minBytes);

	for (i = 0; i < ppsCount; i++) {
		/* 'pps_length' */
		MP4_READ_16(mp4->fd, val16, boxReadBytes);
		uint16_t ppsLength = htons(val16);
		ULOGD("- avcC: pps_length=%" PRIu16, ppsLength);

		minBytes += ppsLength;
		CHECK_SIZE(maxBytes, minBytes);

		if ((!track->vdc.avc.pps) && ppsLength) {
			/* First PPS found */
			track->vdc.avc.pps_size = ppsLength;
			track->vdc.avc.pps = malloc(ppsLength);
			if (track->vdc.avc.pps == NULL) {
				ULOG_ERRNO("malloc", ENOMEM);
				return -ENOMEM;
			}
			ssize_t count =
				read(mp4->fd, track->vdc.avc.pps, ppsLength);
			if (count == -1) {
				ULOG_ERRNO("read", errno);
				return -errno;
			}
		} else {
			/* Ignore any other PPS */
			off_t ret = lseek(mp4->fd, ppsLength, SEEK_CUR);
			if (ret == -1) {
				ULOG_ERRNO("lseek", errno);
				return -errno;
			}
		}
		boxReadBytes += ppsLength;
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-15 - chap. 8.3.3.1.2 - HVCC decoder configuration record
 */
static off_t mp4_box_hvcc_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	off_t minBytes = 22;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;
	uint8_t nb_arrays;
	struct mp4_hvcc_info *hvcc;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, minBytes);

	/* 'version' */
	uint8_t version;
	MP4_READ_8(mp4->fd, version, boxReadBytes);
	if (version != 1)
		ULOGE("hvcC configurationVersion mismatch: %u (expected 1)",
		      version);
	ULOGD("- hvcC: version=%d", version);

	hvcc = &track->vdc.hevc.hvcc_info;

	/* 'general_profile_space', 'general_tier_flag', 'general_profile_idc'
	 */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->general_profile_space = val8 >> 6;
	hvcc->general_tier_flag = (val8 >> 5) & 0x01;
	hvcc->general_profile_idc = val8 & 0x1F;
	ULOGD("- hvcC: general_profile_space=%d", hvcc->general_profile_space);
	ULOGD("- hvcC: general_tier_flag=%d", hvcc->general_tier_flag);
	ULOGD("- hvcC: general_profile_idc=%d", hvcc->general_profile_idc);

	/* 'general_profile_compatibility_flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	hvcc->general_profile_compatibility_flags = htonl(val32);
	ULOGD("- hvcC: general_profile_compatibility_flags= %#" PRIx32,
	      hvcc->general_profile_compatibility_flags);

	/* 'general_constraints_indicator_flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	val32 = htonl(val32);
	val16 = htons(val16);
	hvcc->general_constraints_indicator_flags =
		(((uint64_t)val32) << 16) + val16;
	ULOGD("- hvcC: general_constraints_indicator_flags=%#" PRIx64,
	      hvcc->general_constraints_indicator_flags);

	/* 'general_level_idc' */
	MP4_READ_8(mp4->fd, hvcc->general_level_idc, boxReadBytes);
	ULOGD("- hvcC: level_idc=%d", hvcc->general_level_idc);

	/* 'min_spatial_segmentation_idc' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	val16 = htons(val16);
	hvcc->min_spatial_segmentation_idc = val16 & 0x0FFF;
	ULOGD("- hvcC: min_sseg_idc=%d", hvcc->min_spatial_segmentation_idc);

	/* 'parallelismType' */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->parallelism_type = val8 & 0x02;
	ULOGD("- hvcC: parallel_type=%d", hvcc->parallelism_type);

	/* 'chromaFormat' */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->chroma_format = val8 & 0x02;
	ULOGD("- hvcC: chroma_format=%d", hvcc->chroma_format);

	/* 'bitDepthLuma' */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->bit_depth_luma = (val8 & 0x03) + 8;
	ULOGD("- hvcC: bit_depth_luma=%d", hvcc->bit_depth_luma);

	/* 'bitDepthChroma' */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->bit_depth_chroma = (val8 & 0x03) + 8;
	ULOGD("- hvcC: bit_depth_chroma=%d", hvcc->bit_depth_chroma);

	/* 'avgFrameRate' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	hvcc->avg_framerate = htons(val16);
	ULOGD("- hvcC: avg_framerate=%d", hvcc->avg_framerate);

	/* 'constantFrameRate', 'numTemporalLayers', 'temporalIdNested'
	   'lengthSize'	*/
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	hvcc->constant_framerate = (val8 >> 6) & 0x03;
	hvcc->num_temporal_layers = (val8 >> 3) & 0x7;
	hvcc->temporal_id_nested = (val8 >> 2) & 0x01;
	hvcc->length_size = (val8 & 0x03) + 1;
	ULOGD("- hvcC: constant_framerate=%d", hvcc->constant_framerate);
	ULOGD("- hvcC: num_temporal_layers=%d", hvcc->num_temporal_layers);
	ULOGD("- hvcC: temporal_id_nested=%d", hvcc->temporal_id_nested);
	ULOGD("- hvcC: length_size=%d", hvcc->length_size);

	/* 'numOfArrays' */
	MP4_READ_8(mp4->fd, val8, boxReadBytes);
	if (val8 > 16) {
		ULOGE("hvcC: invalid numOfArrays=%d", val8);
		return -EINVAL;
	}
	nb_arrays = val8;
	ULOGD("- hvcC: array_size=%d", nb_arrays);

	for (int i = 0; i < nb_arrays; i++) {
		uint8_t array_completeness;
		uint8_t nalu_type;
		uint16_t nb_nalus;

		ULOGD("- hvcC:     ------------------ NALU #%d", i);
		/* 'array_completeness' and 'NAL_unit_type' */
		MP4_READ_8(mp4->fd, val8, boxReadBytes);
		array_completeness = (val8 >> 7) & 0x01;
		nalu_type = val8 & 0x3F;
		ULOGD("- hvcC:     array_completeness=%d", array_completeness);
		ULOGD("- hvcC:     nal_unit_type=%d", nalu_type);

		/* 'numNalus' */
		MP4_READ_16(mp4->fd, val16, boxReadBytes);
		nb_nalus = htons(val16);
		if (nb_nalus > 16) {
			ULOGE("hvcC: invalid numNalus=%d", nb_nalus);
			return -EINVAL;
		}
		ULOGD("- hvcC:     num_nalus=%d", nb_nalus);

		for (int j = 0; j < nb_nalus; j++) {
			uint8_t **nalu_pptr = NULL;
			uint16_t nalu_length;

			/* 'nalUnitLength' */
			MP4_READ_16(mp4->fd, val16, boxReadBytes);
			nalu_length = htons(val16);
			ULOGD("- hvcC:         nalu_length = %d", nalu_length);
			nalu_pptr = NULL;
			if (nalu_length) {
				switch (nalu_type) {
				case MP4_H265_NALU_TYPE_VPS:
					/* Ignore all but 1st VPS */
					if (track->vdc.hevc.vps)
						break;
					/* First VPS found */
					track->vdc.hevc.vps_size = nalu_length;
					nalu_pptr = &track->vdc.hevc.vps;
					ULOGD("- hvcC:         "
					      "track->vdc.hevc.vps_size=%zu",
					      track->vdc.hevc.vps_size);
					break;
				case MP4_H265_NALU_TYPE_SPS:
					/* Ignore all but 1st SPS */
					if (track->vdc.hevc.sps)
						break;
					/* First SPS found */
					track->vdc.hevc.sps_size = nalu_length;
					nalu_pptr = &track->vdc.hevc.sps;
					ULOGD("- hvcC:         "
					      "track->vdc.hevc.sps_size=%zu",
					      track->vdc.hevc.sps_size);
					break;
				case MP4_H265_NALU_TYPE_PPS:
					/* Ignore all but 1st PPS */
					if (track->vdc.hevc.pps)
						break;
					/* First PPS found */
					track->vdc.hevc.pps_size = nalu_length;
					nalu_pptr = &track->vdc.hevc.pps;
					ULOGD("- hvcC:         "
					      "track->vdc.hevc.pps_size=%zu",
					      track->vdc.hevc.pps_size);
					break;
				default:
					ULOGD("- hvcC:         ignoring NALU "
					      "(type = %u)",
					      nalu_type);
					break;
				}
			}
			if (nalu_pptr != NULL) {
				/* Store the 1st nalu of a given type */
				*nalu_pptr =
					calloc(nalu_length, sizeof(uint8_t));
				if (*nalu_pptr == NULL) {
					ULOG_ERRNO("calloc", ENOMEM);
					return -ENOMEM;
				}
				ssize_t count =
					read(mp4->fd, *nalu_pptr, nalu_length);
				if (count == -1) {
					ULOG_ERRNO("read", errno);
					return -errno;
				}
			} else {
				/* Skip the latter nalu of a given type */
				off_t ret =
					lseek(mp4->fd, nalu_length, SEEK_CUR);
				if (ret == -1) {
					ULOG_ERRNO("lseek", errno);
					return -errno;
				}
			}
			boxReadBytes += nalu_length;
		}
	}
	return boxReadBytes;
}


/**
 * ISO/IEC 14496-14 - chap. 5.6 - Sample Description Boxes
 */
static off_t mp4_box_esds_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	int ret;
	off_t boxReadBytes = 0;
	off_t minBytes = 9;
	uint32_t val32;
	uint16_t val16;
	uint8_t val8;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, minBytes);

	/* 'version', always 0 */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	ULOGD("- esds: version=%" PRIu32, val32);

	/* 'ESDescriptor' */
	uint8_t tag;
	MP4_READ_8(mp4->fd, tag, boxReadBytes);
	if (tag != 3) {
		ULOGE("invalid ESDescriptor tag: %" PRIu8 ", expected %d",
		      tag,
		      0x03);
		return -EPROTO;
	}
	ULOGD("- esds: ESDescriptor tag:0x%x", tag);

	off_t size = 0;
	int cnt = 0;
	do {
		MP4_READ_8(mp4->fd, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid ESDescriptor size: more than 4 bytes");
		return -EPROTO;
	}
	ULOGD("- esds: ESDescriptor size:%zd (%d bytes)", (ssize_t)size, cnt);

	minBytes = boxReadBytes + size;

	CHECK_SIZE(maxBytes, minBytes);

	/* 'ES_ID' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	val16 = ntohs(val16);
	ULOGD("- esds: ESDescriptor ES_ID:%" PRIu16, val16);

	/* 'flags' */
	uint8_t flags;
	MP4_READ_8(mp4->fd, flags, boxReadBytes);
	ULOGD("- esds: ESDecriptor flags:0x%02x", flags);

	if (flags & 0x80) {
		/* 'dependsOn_ES_ID' */
		MP4_READ_16(mp4->fd, val16, boxReadBytes);
		val16 = ntohs(val16);
		ULOGD("- esds: ESDescriptor dependsOn_ES_ID:%" PRIu16, val16);
	}
	if (flags & 0x40) {
		/* URL_Flag : read 'url_len' & 'url' */
		MP4_READ_8(mp4->fd, val8, boxReadBytes);
		ULOGD("- esds: ESDescriptor url_len:%" PRIu8, val8);
		MP4_READ_SKIP(mp4->fd, val8, boxReadBytes);
		ULOGD("- esds: skipped %" PRIu8 " bytes", val8);
	}

	/* 'DecoderConfigDescriptor' */
	MP4_READ_8(mp4->fd, tag, boxReadBytes);
	if (tag != 4) {
		ULOGE("invalid DecoderConfigDescriptor tag: %" PRIu8
		      ", expected %d",
		      tag,
		      0x04);
		return -EPROTO;
	}
	ULOGD("- esds: DecoderConfigDescriptor tag:0x%x", tag);
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(mp4->fd, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid DecoderConfigDescriptor size: "
		      "more than 4 bytes");
		return -EPROTO;
	}
	ULOGD("- esds: DCD size:%zd (%d bytes)", (ssize_t)size, cnt);

	minBytes = boxReadBytes + size;
	CHECK_SIZE(maxBytes, minBytes);

	/* 'DecoderConfigDescriptor.objectTypeIndication' */
	uint8_t objectTypeIndication;
	MP4_READ_8(mp4->fd, objectTypeIndication, boxReadBytes);
	if (objectTypeIndication != 0x40) {
		ULOGE("invalid objectTypeIndication: %" PRIu8 ", expected 0x%x",
		      objectTypeIndication,
		      0x40);
		return -EPROTO;
	}
	ULOGD("- esds: objectTypeIndication:0x%x", objectTypeIndication);

	/* 'DecoderConfigDescriptor.streamType' */
	uint8_t streamType;
	MP4_READ_8(mp4->fd, streamType, boxReadBytes);
	streamType >>= 2;
	if (streamType != 0x5) {
		ULOGE("invalid streamType: %" PRIu8 ", expected 0x%x",
		      streamType,
		      0x5);
		return -EPROTO;
	}
	ULOGD("- esds: streamType:0x%x", streamType);

	/* Next 11 bytes unused */
	MP4_READ_SKIP(mp4->fd, 11, boxReadBytes);
	ULOGD("- esds: skipped 11 bytes");

	/* 'DecoderSpecificInfo' */
	MP4_READ_8(mp4->fd, tag, boxReadBytes);
	if (tag != 5) {
		ULOGE("invalid DecoderSpecificInfo tag: %" PRIu8
		      ", expected %d",
		      tag,
		      0x05);
		return -EPROTO;
	}
	ULOGD("- esds: DecoderSpecificInfo tag:0x%x", tag);
	size = 0;
	cnt = 0;
	do {
		MP4_READ_8(mp4->fd, val8, boxReadBytes);
		size = (size << 7) + (val8 & 0x7F);
		cnt++;
	} while (val8 & 0x80 && cnt < 4);
	if ((val8 & 0x80) != 0) {
		ULOGE("invalid DecoderSpecificInfo size: more than 4 bytes");
		return -EPROTO;
	}
	ULOGD("- esds: DSI size:%zd (%d bytes)", (ssize_t)size, cnt);

	minBytes = boxReadBytes + size;
	CHECK_SIZE(maxBytes, minBytes);

	/* Only read the first audioSpecificConfig */
	if ((!track->audioSpecificConfig) && (size > 0)) {
		track->audioSpecificConfigSize = size;
		track->audioSpecificConfig = malloc(size);
		if (track->audioSpecificConfig == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}
		ssize_t count = read(mp4->fd, track->audioSpecificConfig, size);
		if (count == -1) {
			ret = -errno;
			ULOG_ERRNO("read", -ret);
			return ret;
		} else if (count != size) {
			ret = -ENODATA;
			ULOG_ERRNO("read", -ret);
			return ret;
		}
		ULOGD("- esds: read %zd bytes for audioSpecificConfig",
		      (ssize_t)size);
		boxReadBytes += size;

		val8 = *(track->audioSpecificConfig);
		uint8_t audioObjectType = val8 >> 3;
		ULOGD("- esds: audioSpecificConfig.audioObjectType: %d",
		      audioObjectType);
		if (audioObjectType == 2)
			track->audioCodec = MP4_AUDIO_CODEC_AAC_LC;
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.5.2 - Sample Description Box
 */
static off_t mp4_box_stsd_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	off_t ret;
	uint32_t val32;
	uint16_t val16;
	size_t len;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stsd: version=%d", version);
	ULOGD("- stsd: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t entryCount = ntohl(val32);
	ULOGD("- stsd: entry_count=%" PRIu32, entryCount);
	if (entryCount > MAX_ENTRY_COUNT) {
		ULOGE("entry count exceeds the maximum count %" PRIu32,
		      entryCount);
		return -EPROTO;
	}

	for (unsigned int i = 0; i < entryCount; i++) {
		switch (track->type) {
		case MP4_TRACK_TYPE_VIDEO: {
			ULOGD("- stsd: video handler type");

			CHECK_SIZE(maxBytes, 102);

			/* 'size' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			ULOGD("- stsd: size=%" PRIu32, size);

			/* 'type' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			ULOGD("- stsd: type=%c%c%c%c",
			      (char)((type >> 24) & 0xFF),
			      (char)((type >> 16) & 0xFF),
			      (char)((type >> 8) & 0xFF),
			      (char)(type & 0xFF));

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);

			/* 'reserved' & 'data_reference_index' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			ULOGD("- stsd: data_reference_index=%" PRIu16,
			      dataReferenceIndex);

			for (int k = 0; k < 4; k++) {
				/* 'pre_defined' & 'reserved' */
				MP4_READ_32(mp4->fd, val32, boxReadBytes);
			}

			/* 'width' & 'height' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			track->vdc.width = ((ntohl(val32) >> 16) & 0xFFFF);
			track->vdc.height = (ntohl(val32) & 0xFFFF);
			ULOGD("- stsd: width=%" PRIu32, track->vdc.width);
			ULOGD("- stsd: height=%" PRIu32, track->vdc.height);

			/* 'horizresolution' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			float horizresolution = (float)(ntohl(val32)) / 65536.;
			ULOGD("- stsd: horizresolution=%.2f", horizresolution);

			/* 'vertresolution' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			float vertresolution = (float)(ntohl(val32)) / 65536.;
			ULOGD("- stsd: vertresolution=%.2f", vertresolution);

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);

			/* 'frame_count' */
			MP4_READ_16(mp4->fd, val16, boxReadBytes);
			uint16_t frameCount = ntohs(val16);
			ULOGD("- stsd: frame_count=%" PRIu16, frameCount);

			/* 'compressorname' */
			char compressorname[32];
			ssize_t count = read(mp4->fd,
					     &compressorname,
					     sizeof(compressorname));
			if (count == -1) {
				ret = -errno;
				ULOG_ERRNO("read", -ret);
				return ret;
			} else if (count != sizeof(compressorname)) {
				ret = -ENODATA;
				ULOG_ERRNO("read", -ret);
				return ret;
			}
			boxReadBytes += sizeof(compressorname);
			ULOGD("- stsd: compressorname=%s", compressorname);

			/* 'depth' & 'pre_defined' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint16_t depth =
				(uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
			ULOGD("- stsd: depth=%" PRIu16, depth);

			/* Codec specific size */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t codecSize = ntohl(val32);
			ULOGD("- stsd: codec_size=%" PRIu32, codecSize);

			/* Codec specific */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t codec = ntohl(val32);
			ULOGD("- stsd: codec=%c%c%c%c",
			      (char)((codec >> 24) & 0xFF),
			      (char)((codec >> 16) & 0xFF),
			      (char)((codec >> 8) & 0xFF),
			      (char)(codec & 0xFF));

			switch (codec) {
			case MP4_AVC_DECODER_CONFIG_BOX:
				track->vdc.codec = MP4_VIDEO_CODEC_AVC;
				ret = mp4_box_avcc_read(
					mp4, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					ULOGE("mp4_box_avcc_read() failed"
					      " (%" PRIi64 ")",
					      (int64_t)ret);
					return ret;
				}
				boxReadBytes += ret;
				break;
			case MP4_HEVC_DECODER_CONFIG_BOX:
				track->vdc.codec = MP4_VIDEO_CODEC_HEVC;
				ret = mp4_box_hvcc_read(
					mp4, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					ULOGE("mp4_box_hvcc_read() failed"
					      " (%" PRIi64 ")",
					      (int64_t)ret);
					return ret;
				}
				boxReadBytes += ret;
				break;
			default:
				ULOGE("unsupported decoder config box");
				return -ENOSYS;
			}
			break;
		}
		case MP4_TRACK_TYPE_AUDIO: {
			ULOGD("- stsd: audio handler type");

			CHECK_SIZE(maxBytes, 44);

			/* 'size' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			ULOGD("- stsd: size=%" PRIu32, size);

			/* 'type' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			ULOGD("- stsd: type=%c%c%c%c",
			      (char)((type >> 24) & 0xFF),
			      (char)((type >> 16) & 0xFF),
			      (char)((type >> 8) & 0xFF),
			      (char)(type & 0xFF));

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);

			/* 'reserved' & 'data_reference_index' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			ULOGD("- stsd: data_reference_index=%" PRIu16,
			      dataReferenceIndex);

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			MP4_READ_32(mp4->fd, val32, boxReadBytes);

			/* 'channelcount' & 'samplesize' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			track->audioChannelCount =
				((ntohl(val32) >> 16) & 0xFFFF);
			track->audioSampleSize = (ntohl(val32) & 0xFFFF);
			ULOGD("- stsd: channelcount=%" PRIu32,
			      track->audioChannelCount);
			ULOGD("- stsd: samplesize=%" PRIu32,
			      track->audioSampleSize);

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);

			/* 'samplerate' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			track->audioSampleRate = ntohl(val32);
			ULOGD("- stsd: samplerate=%.2f",
			      (float)track->audioSampleRate / 65536.);

			/* Codec specific size */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t codecSize = ntohl(val32);
			ULOGD("- stsd: codec_size=%" PRIu32, codecSize);

			/* Codec specific */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t codec = ntohl(val32);
			ULOGD("- stsd: codec=%c%c%c%c",
			      (char)((codec >> 24) & 0xFF),
			      (char)((codec >> 16) & 0xFF),
			      (char)((codec >> 8) & 0xFF),
			      (char)(codec & 0xFF));

			if (codec == MP4_AUDIO_DECODER_CONFIG_BOX) {
				ret = mp4_box_esds_read(
					mp4, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					ULOGE("mp4_box_esds_read() failed"
					      " (%" PRIi64 ")",
					      (int64_t)ret);
					return ret;
				}
				boxReadBytes += ret;
			}

			break;
		}
		case MP4_TRACK_TYPE_HINT:
			ULOGD("- stsd: hint handler type");
			break;
		case MP4_TRACK_TYPE_METADATA: {
			ULOGD("- stsd: metadata handler type");

			CHECK_SIZE(maxBytes, 24);

			/* 'size' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			ULOGD("- stsd: size=%" PRIu32, size);

			/* 'type' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			ULOGD("- stsd: type=%c%c%c%c",
			      (char)((type >> 24) & 0xFF),
			      (char)((type >> 16) & 0xFF),
			      (char)((type >> 8) & 0xFF),
			      (char)(type & 0xFF));

			/* 'reserved' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			MP4_READ_16(mp4->fd, val16, boxReadBytes);

			/* 'data_reference_index' */
			MP4_READ_16(mp4->fd, val16, boxReadBytes);
			uint16_t dataReferenceIndex = ntohl(val16);
			ULOGD("- stsd: size=%d", dataReferenceIndex);

			char str[1000];
			memset(str, 0, sizeof(str));
			unsigned int k;
			for (k = 0;
			     (k < sizeof(str) - 1) && (boxReadBytes < maxBytes);
			     k++) {
				MP4_READ_8(mp4->fd, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			len = mp4_validate_str_len(str, sizeof(str));
			if (len > 0)
				track->contentEncoding = strdup(str);
			ULOGD("- stsd: content_encoding=%s", str);

			memset(str, 0, sizeof(str));
			for (k = 0;
			     (k < sizeof(str) - 1) && (boxReadBytes < maxBytes);
			     k++) {
				MP4_READ_8(mp4->fd, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			len = mp4_validate_str_len(str, sizeof(str));
			if (len > 0)
				track->mimeFormat = strdup(str);
			ULOGD("- stsd: mime_format=%s", str);

			break;
		}
		case MP4_TRACK_TYPE_TEXT:
			ULOGD("- stsd: text handler type");
			break;
		default:
			ULOGD("- stsd: unknown handler type");
			break;
		}
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.6.1.2 - Decoding Time to Sample Box
 */
static off_t mp4_box_stts_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->timeToSampleEntries != NULL) {
		ULOGE("time to sample table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stts: version=%d", version);
	ULOGD("- stts: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->timeToSampleEntryCount = ntohl(val32);
	ULOGD("- stts: entry_count=%" PRIu32, track->timeToSampleEntryCount);
	if (track->timeToSampleEntryCount > MAX_ENTRY_COUNT) {
		ULOGE("timeToSampleEntryCount exceeds maximum entry "
		      "count %" PRIu32,
		      track->timeToSampleEntryCount);
		return -EPROTO;
	}

	track->timeToSampleEntries =
		malloc(track->timeToSampleEntryCount *
		       sizeof(struct mp4_time_to_sample_entry));
	if (track->timeToSampleEntries == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	CHECK_SIZE(maxBytes, 8 + track->timeToSampleEntryCount * 8);

	for (unsigned int i = 0; i < track->timeToSampleEntryCount; i++) {
		/* 'sample_count' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleCount = ntohl(val32);

		/* 'sample_delta' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleDelta = ntohl(val32);
#if LOG_ALL
		ULOGD("- stts: sample_count=%" PRIu32 " sample_delta=%" PRIu32,
		      track->timeToSampleEntries[i].sampleCount,
		      track->timeToSampleEntries[i].sampleDelta);
#endif /* LOG_ALL */
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.6.2 - Sync Sample Box
 */
static off_t mp4_box_stss_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->syncSampleEntries != NULL) {
		ULOGE("sync sample table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stss: version=%d", version);
	ULOGD("- stss: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->syncSampleEntryCount = ntohl(val32);
	ULOGD("- stss: entry_count=%" PRIu32, track->syncSampleEntryCount);
	if (track->syncSampleEntryCount > MAX_ENTRY_COUNT) {
		ULOGE("track->syncSampleEntryCount exceeds maximum "
		      "size %" PRIu32,
		      track->syncSampleEntryCount);
		return -EPROTO;
	}

	track->syncSampleEntries =
		malloc(track->syncSampleEntryCount * sizeof(uint32_t));
	if (track->syncSampleEntries == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	CHECK_SIZE(maxBytes, 8 + track->syncSampleEntryCount * 4);

	for (unsigned int i = 0; i < track->syncSampleEntryCount; i++) {
		/* 'sample_number' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->syncSampleEntries[i] = ntohl(val32);
#if LOG_ALL
		ULOGD("- stss: sample_number=%" PRIu32,
		      track->syncSampleEntries[i]);
#endif /* LOG_ALL */
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.7.3.2 - Sample Size Box
 */
static off_t mp4_box_stsz_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->sampleSize != NULL) {
		ULOGE("sample size table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 12);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stsz: version=%d", version);
	ULOGD("- stsz: flags=%" PRIu32, flags);

	/* 'sample_size' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t sampleSize = ntohl(val32);
	ULOGD("- stsz: sample_size=%" PRIu32, sampleSize);

	/* 'sample_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->sampleCount = ntohl(val32);
	ULOGD("- stsz: sample_count=%" PRIu32, track->sampleCount);
	if (track->sampleCount > MAX_ENTRY_COUNT) {
		ULOGE("track->sampleCount exceeds maximum size %" PRIu32,
		      track->sampleCount);
		return -EPROTO;
	}

	track->sampleSize = malloc(track->sampleCount * sizeof(uint32_t));
	if (track->sampleSize == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	if (sampleSize == 0) {
		CHECK_SIZE(maxBytes, 12 + track->sampleCount * 4);

		for (unsigned int i = 0; i < track->sampleCount; i++) {
			/* 'entry_size' */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			track->sampleSize[i] = ntohl(val32);
			if (track->sampleSize[i] > track->sampleMaxSize)
				track->sampleMaxSize = track->sampleSize[i];
#if LOG_ALL
			ULOGD("- stsz: entry_size=%" PRIu32,
			      track->sampleSize[i]);
#endif /* LOG_ALL */
		}
	} else {
		for (unsigned int i = 0; i < track->sampleCount; i++)
			track->sampleSize[i] = sampleSize;
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.7.4 - Sample To Chunk Box
 */
static off_t mp4_box_stsc_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->sampleToChunkEntries != NULL) {
		ULOGE("sample to chunk table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stsc: version=%d", version);
	ULOGD("- stsc: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->sampleToChunkEntryCount = ntohl(val32);
	if (track->sampleToChunkEntryCount > MAX_ENTRY_COUNT) {
		ULOGE("track->sampleToChunkEntryCount exceeds maximum entry "
		      " count %" PRIu32,
		      track->sampleToChunkEntryCount);
		return -EPROTO;
	}
	ULOGD("- stsc: entry_count=%" PRIu32, track->sampleToChunkEntryCount);

	track->sampleToChunkEntries =
		malloc(track->sampleToChunkEntryCount *
		       sizeof(struct mp4_sample_to_chunk_entry));
	if (track->sampleToChunkEntries == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	CHECK_SIZE(maxBytes, 8 + track->sampleToChunkEntryCount * 12);

	for (unsigned int i = 0; i < track->sampleToChunkEntryCount; i++) {
		/* 'first_chunk' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->sampleToChunkEntries[i].firstChunk = ntohl(val32);
#if LOG_ALL
		ULOGD("- stsc: first_chunk=%" PRIu32,
		      track->sampleToChunkEntries[i].firstChunk);
#endif /* LOG_ALL */

		/* 'samples_per_chunk' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->sampleToChunkEntries[i].samplesPerChunk = ntohl(val32);
#if LOG_ALL
		ULOGD("- stsc: samples_per_chunk=%" PRIu32,
		      track->sampleToChunkEntries[i].samplesPerChunk);
#endif /* LOG_ALL */

		/* 'sample_description_index' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->sampleToChunkEntries[i].sampleDescriptionIndex =
			ntohl(val32);
#if LOG_ALL
		ULOGD("- stsc: sample_description_index=%" PRIu32,
		      track->sampleToChunkEntries[i].sampleDescriptionIndex);
#endif /* LOG_ALL */
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.7.5 - Chunk Offset Box (32-bit)
 */
static off_t mp4_box_stco_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->chunkOffset != NULL) {
		ULOGE("chunk offset table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- stco: version=%d", version);
	ULOGD("- stco: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	ULOGD("- stco: entry_count=%" PRIu32, track->chunkCount);
	if (track->chunkCount > MAX_ENTRY_COUNT) {
		ULOGE("track->chunkCount exceeds maximum entry count %" PRIu32,
		      track->chunkCount);
		return -EPROTO;
	}

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	if (track->chunkOffset == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	CHECK_SIZE(maxBytes, 8 + track->chunkCount * 4);

	for (unsigned int i = 0; i < track->chunkCount; i++) {
		/* 'chunk_offset' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32);
#if LOG_ALL
		ULOGD("- stco: chunk_offset=%" PRIu64, track->chunkOffset[i]);
#endif /* LOG_ALL */
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * ISO/IEC 14496-12 - chap. 8.7.5 - Chunk Offset Box (64-bit)
 */
static off_t mp4_box_co64_read(const struct mp4_file *mp4,
			       off_t maxBytes,
			       struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (track->chunkOffset != NULL) {
		ULOGE("chunk offset table already defined");
		return -EEXIST;
	}

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- co64: version=%d", version);
	ULOGD("- co64: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	ULOGD("- co64: entry_count=%" PRIu32, track->chunkCount);
	if (track->chunkCount > MAX_ENTRY_COUNT) {
		ULOGE("track->chunkCount exceeds maximum entry count %" PRIu32,
		      track->chunkCount);
		return -EPROTO;
	}

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	if (track->chunkOffset == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}

	CHECK_SIZE(maxBytes, 8 + track->chunkCount * 8);

	for (unsigned int i = 0; i < track->chunkCount; i++) {
		/* 'chunk_offset' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		track->chunkOffset[i] |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
#if LOG_ALL
		ULOGD("- co64: chunk_offset=%" PRIu64, track->chunkOffset[i]);
#endif /* LOG_ALL */
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * Android-specific 'xyz' location box
 */
static off_t mp4_box_xyz_read(struct mp4_file *mp4,
			      const struct mp4_box *box,
			      off_t maxBytes)
{
	int ret;
	off_t boxReadBytes = 0;
	uint16_t val16;

	ULOG_ERRNO_RETURN_ERR_IF(box == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 4);

	/* 'location_size' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	uint16_t locationSize = ntohs(val16);
	ULOGD("- xyz: location_size=%d", locationSize);

	/* 'language_code' */
	MP4_READ_16(mp4->fd, val16, boxReadBytes);
	uint16_t languageCode = ntohs(val16);
	ULOGD("- xyz: language_code=%d", languageCode);

	CHECK_SIZE(maxBytes, 4 + locationSize);

	if (mp4->udtaLocationKey != NULL)
		free(mp4->udtaLocationKey);
	mp4->udtaLocationKey = malloc(5);
	if (mp4->udtaLocationKey == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}
	mp4->udtaLocationKey[0] = ((box->type >> 24) & 0xFF);
	mp4->udtaLocationKey[1] = ((box->type >> 16) & 0xFF);
	mp4->udtaLocationKey[2] = ((box->type >> 8) & 0xFF);
	mp4->udtaLocationKey[3] = (box->type & 0xFF);
	mp4->udtaLocationKey[4] = '\0';

	if (mp4->udtaLocationValue != NULL)
		free(mp4->udtaLocationValue);
	mp4->udtaLocationValue = malloc(locationSize + 1);
	if (mp4->udtaLocationValue == NULL) {
		ULOG_ERRNO("malloc", ENOMEM);
		return -ENOMEM;
	}
	ssize_t count = read(mp4->fd, mp4->udtaLocationValue, locationSize);
	if (count == -1) {
		ret = -errno;
		ULOG_ERRNO("read", -ret);
		return ret;
	} else if (count != (ssize_t)locationSize) {
		ret = -ENODATA;
		ULOG_ERRNO("read", -ret);
		return ret;
	}

	boxReadBytes += locationSize;
	mp4->udtaLocationValue[locationSize] = '\0';
	ULOGD("- xyz: location=%s", mp4->udtaLocationValue);

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * Apple QuickTime File Format - chap. Metadata
 * https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
 */
static int mp4_ilst_sub_box_count(const struct mp4_file *mp4, off_t maxBytes)
{
	off_t totalReadBytes = 0;
	off_t boxReadBytes = 0;
	off_t originalOffset;
	off_t realBoxSize;
	uint32_t val32;
	int lastBox = 0;
	int count = 0;

	CHECK_SIZE(maxBytes, 8);

	originalOffset = lseek(mp4->fd, 0, SEEK_CUR);
	if (originalOffset == -1) {
		ULOG_ERRNO("lseek", errno);
		return -errno;
	}

	while ((totalReadBytes + 8 <= maxBytes) && (!lastBox)) {
		boxReadBytes = 0;

		/* Box size */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t size = ntohl(val32);

		/* Box type */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);

		if (size == 0) {
			/* Box extends to end of file */
			/* TODO */
			ULOGE("size == 0 for list element is not implemented");
			return -ENOSYS;
		} else if (size == 1) {
			CHECK_SIZE(maxBytes, boxReadBytes + 16);

			/* Large size */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			realBoxSize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			realBoxSize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
			if (realBoxSize > (mp4->fileSize - mp4->readBytes)) {
				ULOGE("realBoxSize exceeds maximum size %ld",
				      (long)realBoxSize);
				return -EPROTO;
			}
		} else
			realBoxSize = size;

		count++;

		/* Skip the rest of the box */
		MP4_READ_SKIP(
			mp4->fd, realBoxSize - boxReadBytes, boxReadBytes);
		totalReadBytes += realBoxSize;
	}

	off_t ret = lseek(mp4->fd, -totalReadBytes, SEEK_CUR);
	if (ret == -1) {
		ULOG_ERRNO("lseek", errno);
		return -errno;
	}

	return count;
}


/**
 * Apple QuickTime File Format - chap. Metadata
 * https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
 */
static off_t mp4_box_meta_keys_read(struct mp4_file *mp4,
				    off_t maxBytes,
				    struct mp4_track *track)
{
	int ret;
	off_t boxReadBytes = 0;
	uint32_t val32;
	unsigned int metadataCount;
	char **metadataKey;
	char **metadataValue;

	CHECK_SIZE(maxBytes, 8);

	/* 'version' & 'flags' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	ULOGD("- keys: version=%d", version);
	ULOGD("- keys: flags=%" PRIu32, flags);

	/* 'entry_count' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	metadataCount = ntohl(val32);
	ULOGD("- keys: entry_count=%" PRIu32, metadataCount);
	if (metadataCount > MAX_ENTRY_COUNT) {
		ULOGE("metadataCount exceeds maximum entry count %" PRIu32,
		      metadataCount);
		return -EPROTO;
	}

	CHECK_SIZE(maxBytes, 4 + metadataCount * 8);

	metadataKey = calloc(metadataCount, sizeof(char *));
	if (metadataKey == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	metadataValue = calloc(metadataCount, sizeof(char *));
	if (metadataValue == NULL) {
		free(metadataKey);
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	if (track) {
		track->staticMetadataCount = metadataCount;
		track->staticMetadataKey = metadataKey;
		track->staticMetadataValue = metadataValue;
	} else {
		mp4->metaMetadataCount = metadataCount;
		mp4->metaMetadataKey = metadataKey;
		mp4->metaMetadataValue = metadataValue;
	}

	for (uint32_t i = 0; i < metadataCount; i++) {
		/* 'key_size' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t keySize = ntohl(val32);
		ULOGD("- keys: key_size=%" PRIu32, keySize);

		if (keySize < 8) {
			ULOGE("invalid key size: %" PRIu32 ", expected %d min",
			      keySize,
			      8);
			return -EPROTO;
		}
		keySize -= 8;

		/* 'key_namespace' */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		uint32_t keyNamespace = ntohl(val32);
		ULOGD("- keys: key_namespace=%c%c%c%c",
		      (char)((keyNamespace >> 24) & 0xFF),
		      (char)((keyNamespace >> 16) & 0xFF),
		      (char)((keyNamespace >> 8) & 0xFF),
		      (char)(keyNamespace & 0xFF));

		CHECK_SIZE(maxBytes - boxReadBytes, keySize);

		metadataKey[i] = malloc(keySize + 1);
		if (metadataKey[i] == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}
		ssize_t count = read(mp4->fd, metadataKey[i], keySize);
		if (count == -1) {
			ret = -errno;
			ULOG_ERRNO("read", -ret);
			return ret;
		} else if (count != (ssize_t)keySize) {
			ret = -ENODATA;
			ULOG_ERRNO("read", -ret);
			return ret;
		}

		boxReadBytes += keySize;
		metadataKey[i][keySize] = '\0';
		ULOGD("- keys: key_value[%i]=%s", i, metadataKey[i]);
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


/**
 * Apple QuickTime File Format - chap. Metadata
 * https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html
 */
static off_t mp4_box_meta_data_read(struct mp4_file *mp4,
				    const struct mp4_box *box,
				    off_t maxBytes,
				    struct mp4_track *track)
{
	int ret;
	off_t boxReadBytes = 0;
	uint32_t val32;
	unsigned int metadataCount;
	char **metadataKey;
	char **metadataValue;

	ULOG_ERRNO_RETURN_ERR_IF(box->parent == NULL, EINVAL);

	CHECK_SIZE(maxBytes, 9);

	/* 'version' & 'class' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);
	uint32_t clazz = ntohl(val32);
	uint8_t version = (clazz >> 24) & 0xFF;
	clazz &= 0xFF;
	ULOGD("- data: version=%d", version);
	ULOGD("- data: class=%" PRIu32, clazz);

	/* 'reserved' */
	MP4_READ_32(mp4->fd, val32, boxReadBytes);

	unsigned int valueLen = maxBytes - boxReadBytes;

	if (clazz == MP4_METADATA_CLASS_UTF8) {
		switch (box->parent->type & 0xFFFFFF) {
		case MP4_METADATA_TAG_TYPE_ARTIST:
		case MP4_METADATA_TAG_TYPE_TITLE:
		case MP4_METADATA_TAG_TYPE_DATE:
		case MP4_METADATA_TAG_TYPE_COMMENT:
		case MP4_METADATA_TAG_TYPE_COPYRIGHT:
		case MP4_METADATA_TAG_TYPE_MAKER:
		case MP4_METADATA_TAG_TYPE_MODEL:
		case MP4_METADATA_TAG_TYPE_VERSION:
		case MP4_METADATA_TAG_TYPE_ENCODER: {
			uint32_t idx = mp4->udtaMetadataParseIdx++;
			mp4->udtaMetadataKey[idx] = malloc(5);
			if (mp4->udtaMetadataKey[idx] == NULL) {
				ULOG_ERRNO("malloc", ENOMEM);
				return -ENOMEM;
			}
			mp4->udtaMetadataKey[idx][0] =
				((box->parent->type >> 24) & 0xFF);
			mp4->udtaMetadataKey[idx][1] =
				((box->parent->type >> 16) & 0xFF);
			mp4->udtaMetadataKey[idx][2] =
				((box->parent->type >> 8) & 0xFF);
			mp4->udtaMetadataKey[idx][3] =
				(box->parent->type & 0xFF);
			mp4->udtaMetadataKey[idx][4] = '\0';
			mp4->udtaMetadataValue[idx] = malloc(valueLen + 1);
			if (mp4->udtaMetadataValue[idx] == NULL) {
				ULOG_ERRNO("malloc", ENOMEM);
				return -ENOMEM;
			}
			ssize_t count = read(
				mp4->fd, mp4->udtaMetadataValue[idx], valueLen);
			if (count == -1) {
				ret = -errno;
				ULOG_ERRNO("read", -ret);
				return ret;
			} else if (count != (ssize_t)valueLen) {
				ret = -ENODATA;
				ULOG_ERRNO("read", -ret);
				return ret;
			}
			boxReadBytes += valueLen;
			mp4->udtaMetadataValue[idx][valueLen] = '\0';
			ULOGD("- data: value[%s]=%s",
			      mp4->udtaMetadataKey[idx],
			      mp4->udtaMetadataValue[idx]);
			break;
		}
		default: {
			if (track) {
				metadataCount = track->staticMetadataCount;
				metadataKey = track->staticMetadataKey;
				metadataValue = track->staticMetadataValue;
			} else {
				metadataCount = mp4->metaMetadataCount;
				metadataKey = mp4->metaMetadataKey;
				metadataValue = mp4->metaMetadataValue;
			}
			if ((box->parent->type > 0) &&
			    (box->parent->type <= metadataCount)) {
				uint32_t idx = box->parent->type - 1;
				metadataValue[idx] = malloc(valueLen + 1);
				if (metadataValue[idx] == NULL) {
					ULOG_ERRNO("malloc", ENOMEM);
					return -ENOMEM;
				}
				ssize_t count = read(
					mp4->fd, metadataValue[idx], valueLen);
				if (count == -1) {
					ret = -errno;
					ULOG_ERRNO("read", -ret);
					return ret;
				} else if (count != (ssize_t)valueLen) {
					ret = -ENODATA;
					ULOG_ERRNO("read", -ret);
					return ret;
				}
				boxReadBytes += valueLen;
				metadataValue[idx][valueLen] = '\0';
				ULOGD("- data: value[%s]=%s",
				      metadataKey[idx],
				      metadataValue[idx]);
			}
			break;
		}
		}
	} else if (((clazz == MP4_METADATA_CLASS_JPEG) ||
		    (clazz == MP4_METADATA_CLASS_PNG) ||
		    (clazz == MP4_METADATA_CLASS_BMP)) &&
		   (track == NULL)) {
		uint32_t type = box->parent->type;
		if (type == MP4_METADATA_TAG_TYPE_COVER) {
			mp4->udtaCoverOffset = lseek(mp4->fd, 0, SEEK_CUR);
			mp4->udtaCoverSize = valueLen;
			switch (clazz) {
			case MP4_METADATA_CLASS_JPEG:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				mp4->udtaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			default:
				break;
			}
			ULOGD("- data: udta cover size=%d type=%d",
			      mp4->udtaCoverSize,
			      mp4->udtaCoverType);
		} else if ((type > 0) && (type <= mp4->metaMetadataCount) &&
			   (!strcmp(mp4->metaMetadataKey[type - 1],
				    MP4_METADATA_KEY_COVER))) {
			mp4->metaCoverOffset = lseek(mp4->fd, 0, SEEK_CUR);
			mp4->metaCoverSize = valueLen;
			switch (clazz) {
			case MP4_METADATA_CLASS_JPEG:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				mp4->metaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			default:
				break;
			}
			ULOGD("- data: meta cover size=%d type=%d",
			      mp4->metaCoverSize,
			      mp4->metaCoverType);
		}
	}

	/* Skip the rest of the box */
	MP4_READ_SKIP(mp4->fd, maxBytes - boxReadBytes, boxReadBytes);

	return boxReadBytes;
}


off_t mp4_box_children_read(struct mp4_file *mp4,
			    struct mp4_box *parent,
			    off_t maxBytes,
			    struct mp4_track *track)
{
	off_t parentReadBytes = 0;
	off_t currOff = 0;
	off_t eof = 0;
	int ret = 0;
	int firstBox = 1;
	int lastBox = 0;

	ULOG_ERRNO_RETURN_ERR_IF(mp4 == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(parent == NULL, EINVAL);


	errno = 0;
	currOff = lseek(mp4->fd, 0, SEEK_CUR);
	if (currOff == -1) {
		ULOG_ERRNO("lseek", errno);
		return -errno;
	}
	eof = lseek(mp4->fd, 0, SEEK_END);
	if (eof == -1) {
		ULOG_ERRNO("lseek", errno);
		return -errno;
	}

	currOff = lseek(mp4->fd, currOff, SEEK_SET);
	if (currOff == -1) {
		ULOG_ERRNO("lseek", errno);
		return -errno;
	}

	while (currOff < eof && !lastBox && (parentReadBytes + 8 < maxBytes)) {
		off_t boxReadBytes = 0;
		off_t realBoxSize;
		uint32_t val32;

		/* Keep the box in the tree */
		struct mp4_box *box = mp4_box_new(parent);
		if (box == NULL) {
			ULOG_ERRNO("mp4_box_new", ENOMEM);
			return -ENOMEM;
		}

		/* Box size */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		box->size = ntohl(val32);

		/* Box type */
		MP4_READ_32(mp4->fd, val32, boxReadBytes);
		box->type = ntohl(val32);

		/* MP4 file format validity: the first box should be 'ftyp' */
		if ((parent->level == 0) && firstBox &&
		    (box->type != MP4_FILE_TYPE_BOX)) {
			ULOGE("invalid mp4 file: 'ftyp' not found");
			return -EPROTO;
		}

		if ((parent->type == MP4_ILST_BOX) &&
		    (box->type <= mp4->metaMetadataCount))
			ULOGD("offset 0x%" PRIx64 " metadata box size %" PRIu32,
			      (int64_t)lseek(mp4->fd, 0, SEEK_CUR) - 8,
			      box->size);
		else
			ULOGD("offset 0x%" PRIx64
			      " box '%c%c%c%c' size %" PRIu32,
			      (int64_t)lseek(mp4->fd, 0, SEEK_CUR) - 8,
			      (box->type >> 24) & 0xFF,
			      (box->type >> 16) & 0xFF,
			      (box->type >> 8) & 0xFF,
			      box->type & 0xFF,
			      box->size);

		if (box->size == 0) {
			/* Box extends to end of file */
			lastBox = 1;
			realBoxSize = mp4->fileSize - mp4->readBytes;
		} else if (box->size == 1) {
			CHECK_SIZE(maxBytes, parentReadBytes + 16);

			/* Large size */
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			box->largesize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(mp4->fd, val32, boxReadBytes);
			box->largesize |=
				(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
			realBoxSize = box->largesize;
		} else
			realBoxSize = box->size;

		/* Check for truncated box */
		if (maxBytes < parentReadBytes + realBoxSize) {
			/* Not a fatal error */
			ULOGW("box '%c%c%c%c': invalid size: %" PRIi64
			      ", expected %" PRIi64 " min",
			      (box->type >> 24) & 0xFF,
			      (box->type >> 16) & 0xFF,
			      (box->type >> 8) & 0xFF,
			      box->type & 0xFF,
			      (int64_t)maxBytes,
			      (int64_t)parentReadBytes + realBoxSize);
			return maxBytes;
		}

		if (realBoxSize < boxReadBytes ||
		    realBoxSize - boxReadBytes >
			    (mp4->fileSize - mp4->readBytes)) {
			ULOGE("size to read exceeds maximum size %ld",
			      (long)(realBoxSize - boxReadBytes));
			goto skip_box;
		}

		switch (box->type) {
		case MP4_UUID: {
			CHECK_SIZE(realBoxSize - boxReadBytes,
				   sizeof(box->uuid));

			/* Box extended type */
			ssize_t count =
				read(mp4->fd, box->uuid, sizeof(box->uuid));
			if (count == -1) {
				ULOG_ERRNO("read", errno);
				return -errno;
			}
			boxReadBytes += sizeof(box->uuid);
			break;
		}
		case MP4_MOVIE_BOX:
		case MP4_USER_DATA_BOX:
		case MP4_MEDIA_BOX:
		case MP4_MEDIA_INFORMATION_BOX:
		case MP4_DATA_INFORMATION_BOX:
		case MP4_SAMPLE_TABLE_BOX: {
			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_FILE_TYPE_BOX: {
			off_t _ret = mp4_box_ftyp_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MOVIE_HEADER_BOX: {
			off_t _ret = mp4_box_mvhd_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_BOX: {
			/* Keep the track in the list */
			struct mp4_track *tk = mp4_track_add(mp4);
			if (tk == NULL) {
				ULOG_ERRNO("mp4_track_add", ENOMEM);
				return -ENOMEM;
			}

			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, tk);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_HEADER_BOX: {
			off_t _ret = mp4_box_tkhd_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_REFERENCE_BOX: {
			off_t _ret = mp4_box_tref_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HANDLER_REFERENCE_BOX: {
			off_t _ret = mp4_box_hdlr_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MEDIA_HEADER_BOX: {
			off_t _ret = mp4_box_mdhd_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_VIDEO_MEDIA_HEADER_BOX: {
			off_t _ret = mp4_box_vmhd_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SOUND_MEDIA_HEADER_BOX: {
			off_t _ret = mp4_box_smhd_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HINT_MEDIA_HEADER_BOX: {
			off_t _ret = mp4_box_hmhd_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_NULL_MEDIA_HEADER_BOX: {
			off_t _ret = mp4_box_nmhd_read(
				mp4, realBoxSize - boxReadBytes);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_DESCRIPTION_BOX: {
			off_t _ret = mp4_box_stsd_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DECODING_TIME_TO_SAMPLE_BOX: {
			off_t _ret = mp4_box_stts_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SYNC_SAMPLE_BOX: {
			off_t _ret = mp4_box_stss_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_SIZE_BOX: {
			off_t _ret = mp4_box_stsz_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_TO_CHUNK_BOX: {
			off_t _ret = mp4_box_stsc_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_BOX: {
			off_t _ret = mp4_box_stco_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_64_BOX: {
			off_t _ret = mp4_box_co64_read(
				mp4, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_META_BOX: {
			if (parent->type == MP4_USER_DATA_BOX) {
				CHECK_SIZE(realBoxSize - boxReadBytes, 4);

				/* 'version' & 'flags' */
				MP4_READ_32(mp4->fd, val32, boxReadBytes);
				uint32_t flags = ntohl(val32);
				uint8_t version = (flags >> 24) & 0xFF;
				flags &= ((1 << 24) - 1);
				ULOGD("- meta: version=%d", version);
				ULOGD("- meta: flags=%" PRIu32, flags);

				off_t _ret = mp4_box_children_read(
					mp4,
					box,
					realBoxSize - boxReadBytes,
					track);
				if (_ret < 0)
					return OFF_T_TO_ERRNO(_ret, EPROTO);
				boxReadBytes += _ret;
			} else if ((parent->type == MP4_MOVIE_BOX) ||
				   (parent->type == MP4_TRACK_BOX)) {
				off_t _ret = mp4_box_children_read(
					mp4,
					box,
					realBoxSize - boxReadBytes,
					track);
				if (_ret < 0)
					return OFF_T_TO_ERRNO(_ret, EPROTO);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_ILST_BOX: {
			if ((parent->parent) &&
			    (parent->parent->type == MP4_USER_DATA_BOX)) {
				if (realBoxSize - boxReadBytes < 0 ||
				    realBoxSize - boxReadBytes >
					    (mp4->fileSize - mp4->readBytes)) {
					ULOGE("ilst box size exceeds maximum "
					      "size (%ld)",
					      (long)(realBoxSize -
						     boxReadBytes));
					return -EPROTO;
				}
				int count = mp4_ilst_sub_box_count(
					mp4, realBoxSize - boxReadBytes);
				if (count < 0)
					return count;
				/* Free previous metadata. This fixes a memory
				 * leak if more than one ilst boxes are present
				 * in the file, but only the last one will be
				 * saved */
				for (unsigned int i = 0;
				     i < mp4->udtaMetadataCount;
				     i++) {
					if (mp4->udtaMetadataKey)
						free(mp4->udtaMetadataKey[i]);
					if (mp4->udtaMetadataValue)
						free(mp4->udtaMetadataValue[i]);
				}
				free(mp4->udtaMetadataKey);
				free(mp4->udtaMetadataValue);
				mp4->udtaMetadataCount = count;
				if (mp4->udtaMetadataCount > 0) {
					char **key =
						calloc(mp4->udtaMetadataCount,
						       sizeof(char *));
					if (key == NULL) {
						ULOG_ERRNO("calloc", ENOMEM);
						return -ENOMEM;
					}
					mp4->udtaMetadataKey = key;

					char **value =
						calloc(mp4->udtaMetadataCount,
						       sizeof(char *));
					if (value == NULL) {
						ULOG_ERRNO("calloc", ENOMEM);
						return -ENOMEM;
					}
					mp4->udtaMetadataValue = value;

					mp4->udtaMetadataParseIdx = 0;
				}
			}
			off_t _ret = mp4_box_children_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DATA_BOX: {
			if (realBoxSize - boxReadBytes >
			    (mp4->fileSize - mp4->readBytes)) {
				ULOGE("meta data box size exceeds maximum "
				      "size (%ld)",
				      (long)(realBoxSize - boxReadBytes));
				return -EPROTO;
			}
			off_t _ret = mp4_box_meta_data_read(
				mp4, box, realBoxSize - boxReadBytes, track);
			if (_ret < 0)
				return OFF_T_TO_ERRNO(_ret, EPROTO);
			boxReadBytes += _ret;
			break;
		}
		case MP4_LOCATION_BOX: {
			if (parent->type == MP4_USER_DATA_BOX) {
				off_t _ret = mp4_box_xyz_read(
					mp4, box, realBoxSize - boxReadBytes);
				if (_ret < 0)
					return OFF_T_TO_ERRNO(_ret, EPROTO);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_KEYS_BOX: {
			if (parent->type == MP4_META_BOX) {
				off_t _ret = mp4_box_meta_keys_read(
					mp4, realBoxSize - boxReadBytes, track);
				if (_ret < 0)
					return OFF_T_TO_ERRNO(_ret, EPROTO);
				boxReadBytes += _ret;
			}
			break;
		}
		default: {
			if (parent->type == MP4_ILST_BOX) {
				off_t _ret = mp4_box_children_read(
					mp4,
					box,
					realBoxSize - boxReadBytes,
					track);
				if (_ret < 0)
					return OFF_T_TO_ERRNO(_ret, EPROTO);
				boxReadBytes += _ret;
			}
			break;
		}
		}
		/* clang-format off */
skip_box:
		/* clang-format on */

		/* Skip the rest of the box */
		if (realBoxSize < boxReadBytes) {
			ULOGE("invalid box size %" PRIi64
			      " (read bytes: %" PRIi64 ")",
			      (int64_t)realBoxSize,
			      (int64_t)boxReadBytes);
			ret = -EIO;
			break;
		}
		off_t _ret =
			lseek(mp4->fd, realBoxSize - boxReadBytes, SEEK_CUR);
		if (_ret == -1) {
			ULOGE("failed to seek %" PRIi64
			      " bytes forward in file",
			      (int64_t)realBoxSize - boxReadBytes);
			ret = -errno;
			break;
		}

		parentReadBytes += realBoxSize;
		firstBox = 0;

		currOff = lseek(mp4->fd, 0, SEEK_CUR);
		if (currOff == -1) {
			ULOG_ERRNO("lseek", errno);
			return -errno;
		}
	}

	return (ret < 0) ? ret : parentReadBytes;
}


/**
 * ISO/IEC 14496-15 - chap. 5.3.3.1 - AVC decoder configuration record
 */
int mp4_generate_avc_decoder_config(const uint8_t *sps,
				    unsigned int sps_size,
				    const uint8_t *pps,
				    unsigned int pps_size,
				    uint8_t *avcc,
				    unsigned int *avcc_size)
{
	off_t off = 0;

	ULOG_ERRNO_RETURN_ERR_IF(sps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sps_size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(pps == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(pps_size == 0, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(avcc == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(avcc_size == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(*avcc_size < (sps_size + pps_size + 11),
				 EINVAL);

	/* 'configurationVersion' = 1, 'AVCProfileIndication',
	 * 'profile_compatibility', 'AVCLevelIndication' */
	avcc[off++] = 0x01;
	avcc[off++] = sps[1];
	avcc[off++] = sps[2];
	avcc[off++] = sps[3];
	/* 'reserved' (6 bits), 'lengthSizeMinusOne' = 3 (2 bits),
	 * 'reserved' (3bits), 'numOfSequenceParameterSets' (5 bits) */
	avcc[off++] = 0xFF;
	avcc[off++] = 0xE1;
	/* 'sequenceParameterSetLength' */
	avcc[off++] = sps_size >> 8;
	avcc[off++] = sps_size & 0xFF;
	/* 'sequenceParameterSetNALUnit' */
	memcpy(&avcc[off], sps, sps_size);
	off += sps_size;
	/* 'numOfPictureParameterSets' */
	avcc[off++] = 0x01;
	/* 'pictureParameterSetLength' */
	avcc[off++] = pps_size >> 8;
	avcc[off++] = pps_size & 0xFF;
	/* 'pictureParameterSetNALUnit' */
	memcpy(&avcc[off], pps, pps_size);
	off += pps_size;

	*avcc_size = off;
	return 0;
}


MP4_API int mp4_generate_chapter_sample(const char *chapter_str,
					uint8_t **buffer,
					unsigned int *buffer_size)
{
	uint16_t val16;
	uint8_t *buf = NULL;
	size_t buf_size = 0;
	size_t chap_len = 0;

	ULOG_ERRNO_RETURN_ERR_IF(buffer == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(buffer_size == NULL, EINVAL);
	chap_len = (uint16_t)mp4_validate_str_len(chapter_str, NAME_MAX);
	ULOG_ERRNO_RETURN_ERR_IF(chap_len < 1, EINVAL);

	buf_size = sizeof(val16) + chap_len + 1;
	buf = calloc(1, buf_size);
	if (buf == NULL)
		return -ENOMEM;

	val16 = htons(chap_len);
	memcpy(buf, &val16, sizeof(val16));
	memcpy(buf + sizeof(val16), chapter_str, chap_len);

	*buffer = buf;
	*buffer_size = buf_size;

	return 0;
}

#pragma GCC diagnostic pop
