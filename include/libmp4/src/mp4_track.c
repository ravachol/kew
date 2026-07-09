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

#include "mp4_priv.h"


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"


int mp4_track_is_sync_sample(const struct mp4_track *track,
			     unsigned int sampleIdx,
			     int *prevSyncSampleIdx)
{
	unsigned int i;

	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	if (!track->syncSampleEntries)
		return 1;

	for (i = 0; i < track->syncSampleEntryCount; i++) {
		if (track->syncSampleEntries[i] - 1 == sampleIdx)
			return 1;
		else if (track->syncSampleEntries[i] - 1 > sampleIdx) {
			if (prevSyncSampleIdx && (i > 0)) {
				*prevSyncSampleIdx =
					track->syncSampleEntries[i - 1] - 1;
			}
			return 0;
		}
	}

	if (prevSyncSampleIdx && (i > 0))
		*prevSyncSampleIdx = track->syncSampleEntries[i - 1] - 1;
	return 0;
}


static inline int
is_sample_searchable(const struct mp4_track *track, int i, int sync)
{
	if (!sync)
		return 1;

	return mp4_track_is_sync_sample(track, i, NULL);
}


static inline int
mp4_track_find_sample_by_time_exact(const struct mp4_track *track,
				    uint64_t time,
				    int sync,
				    int start)
{
	if (start < 0)
		start = 0;
	if (start >= (int)track->sampleCount)
		start = (int)track->sampleCount - 1;

	for (int i = start; i < (int)track->sampleCount; i++) {
		if (track->sampleDecodingTime[i] == time) {
			if (is_sample_searchable(track, i, sync))
				return i;
		} else if (track->sampleDecodingTime[i] > time) {
			break;
		}
	}

	return -ENOENT;
}


static inline int
mp4_track_find_sample_by_time_nearest(const struct mp4_track *track,
				      uint64_t time,
				      int sync,
				      int start)
{
	int is_sync;
	int min_idx = -1;
	uint64_t delta_ts;
	uint64_t prev_delta_ts = UINT64_MAX;
	uint64_t min_delta_ts = UINT64_MAX;

	if (start < 0)
		start = 0;
	if (start >= (int)track->sampleCount)
		start = (int)track->sampleCount - 1;

	for (int i = start; i < (int)track->sampleCount; i++, is_sync = 0) {
		if (sync)
			is_sync = mp4_track_is_sync_sample(track, i, NULL);
		if (sync && !is_sync)
			continue;
		delta_ts = llabs((int64_t)time -
				 (int64_t)track->sampleDecodingTime[i]);
		if (delta_ts < min_delta_ts) {
			min_delta_ts = delta_ts;
			min_idx = i;
		}
		if (prev_delta_ts < delta_ts)
			break;
		prev_delta_ts = delta_ts;
	}
	if (min_idx >= 0)
		return min_idx;

	return -ENOENT;
}


static inline int
mp4_track_find_sample_by_time_lt(const struct mp4_track *track,
				 uint64_t time,
				 enum mp4_time_cmp cmp,
				 int sync,
				 int start)
{
	if (start < 0)
		start = (int)track->sampleCount - 1;
	if (start >= (int)track->sampleCount)
		start = (int)track->sampleCount - 1;

	for (int i = start; i >= 0; i--) {
		bool check_cmp = ((cmp == MP4_TIME_CMP_LT) &&
				  (track->sampleDecodingTime[i] < time)) ||
				 ((cmp == MP4_TIME_CMP_LT_EQ) &&
				  (track->sampleDecodingTime[i] <= time));
		if (check_cmp && is_sample_searchable(track, i, sync))
			return i;
	}

	return -ENOENT;
}


static inline int
mp4_track_find_sample_by_time_gt(const struct mp4_track *track,
				 uint64_t time,
				 enum mp4_time_cmp cmp,
				 int sync,
				 int start)
{
	if (start < 0)
		start = 0;
	if (start >= (int)track->sampleCount)
		start = (int)track->sampleCount - 1;

	for (int i = start; i < (int)track->sampleCount; i++) {
		bool check_cmp = ((cmp == MP4_TIME_CMP_GT) &&
				  (track->sampleDecodingTime[i] > time)) ||
				 ((cmp == MP4_TIME_CMP_GT_EQ) &&
				  (track->sampleDecodingTime[i] >= time));
		if (check_cmp && is_sample_searchable(track, i, sync))
			return i;
	}

	return -ENOENT;
}


int mp4_track_find_sample_by_time(const struct mp4_track *track,
				  uint64_t time,
				  enum mp4_time_cmp cmp,
				  int sync,
				  int start)
{
	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	switch (cmp) {
	case MP4_TIME_CMP_EXACT:
		return mp4_track_find_sample_by_time_exact(
			track, time, sync, start);
	case MP4_TIME_CMP_NEAREST: {
		return mp4_track_find_sample_by_time_nearest(
			track, time, sync, start);
	}
	case MP4_TIME_CMP_LT:
	case MP4_TIME_CMP_LT_EQ:
		return mp4_track_find_sample_by_time_lt(
			track, time, cmp, sync, start);
	case MP4_TIME_CMP_GT:
	case MP4_TIME_CMP_GT_EQ:
		return mp4_track_find_sample_by_time_gt(
			track, time, cmp, sync, start);
	default:
		ULOGE("unsupported comparison type: %d", cmp);
		return -EINVAL;
	}
}


static struct mp4_track *mp4_track_new(void)
{
	struct mp4_track *track = calloc(1, sizeof(*track));
	if (track == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return NULL;
	}
	list_node_unref(&track->node);

	return track;
}


static int mp4_track_destroy(struct mp4_track *track)
{
	if (track == NULL)
		return 0;

	mp4_video_decoder_config_destroy(&track->vdc);
	free(track->timeToSampleEntries);
	free(track->sampleDecodingTime);
	free(track->sampleSize);
	free(track->chunkOffset);
	free(track->sampleToChunkEntries);
	free(track->sampleOffset);
	free(track->syncSampleEntries);
	free(track->audioSpecificConfig);
	free(track->contentEncoding);
	free(track->mimeFormat);
	for (unsigned int i = 0; i < track->staticMetadataCount; i++) {
		free(track->staticMetadataKey[i]);
		free(track->staticMetadataValue[i]);
	}
	free(track->staticMetadataKey);
	free(track->staticMetadataValue);
	free(track->name);
	free(track);

	return 0;
}


struct mp4_track *mp4_track_add(struct mp4_file *mp4)
{
	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	struct mp4_track *track = mp4_track_new();
	if (track == NULL) {
		ULOG_ERRNO("mp4_track_new", ENOMEM);
		return NULL;
	}
	list_node_unref(&track->node); /* TODO: remove */

	/* Add to the list */
	list_add_after(list_last(&mp4->tracks), &track->node);
	mp4->trackCount++;

	return track;
}


int mp4_track_remove(struct mp4_file *mp4, struct mp4_track *track)
{
	const struct mp4_track *_track = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(mp4 == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track == NULL, EINVAL);

	_track = mp4_track_find(mp4, track);
	if (_track != track) {
		ULOG_ERRNO("mp4_track_find", ENOENT);
		return -ENOENT;
	}

	/* Remove from the list */
	list_del(&track->node);
	mp4->trackCount--;

	return mp4_track_destroy(track);
}


struct mp4_track *mp4_track_find(const struct mp4_file *mp4,
				 const struct mp4_track *track)
{
	struct mp4_track *_track = NULL;
	int found = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);
	ULOG_ERRNO_RETURN_VAL_IF(track == NULL, EINVAL, NULL);

	list_walk_entry_forward(&mp4->tracks, _track, node)
	{
		if (_track == track) {
			found = 1;
			break;
		}
	}

	if (!found)
		return NULL;

	return _track;
}


struct mp4_track *mp4_track_find_by_idx(const struct mp4_file *mp4,
					unsigned int track_idx)
{
	struct mp4_track *_track = NULL;
	int found = 0;
	unsigned int k = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	list_walk_entry_forward(&mp4->tracks, _track, node)
	{
		if (k == track_idx) {
			found = 1;
			break;
		}
		k++;
	}

	if (!found)
		return NULL;

	return _track;
}


struct mp4_track *mp4_track_find_by_id(const struct mp4_file *mp4,
				       unsigned int track_id)
{
	struct mp4_track *_track = NULL;
	int found = 0;

	ULOG_ERRNO_RETURN_VAL_IF(mp4 == NULL, EINVAL, NULL);

	list_walk_entry_forward(&mp4->tracks, _track, node)
	{
		if (_track->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return NULL;

	return _track;
}


void mp4_tracks_destroy(const struct mp4_file *mp4)
{
	struct mp4_track *track = NULL;
	struct mp4_track *tmp = NULL;

	ULOG_ERRNO_RETURN_IF(mp4 == NULL, EINVAL);

	list_walk_entry_forward_safe(&mp4->tracks, track, tmp, node)
	{
		mp4_track_destroy(track);
	}
}


int mp4_tracks_build(struct mp4_file *mp4)
{
	int ret;
	struct mp4_track *tk = NULL;
	struct mp4_track *videoTk = NULL;
	struct mp4_track *metaTk = NULL;
	const struct mp4_track *chapTk = NULL;
	int videoTrackCount = 0;
	int audioTrackCount = 0;
	int hintTrackCount = 0;
	int metadataTrackCount = 0;
	int textTrackCount = 0;
	bool max_chapter_reached = false;

	ULOG_ERRNO_RETURN_ERR_IF(mp4 == NULL, EINVAL);

	list_walk_entry_forward(&mp4->tracks, tk, node)
	{
		unsigned int i;
		unsigned int j;
		unsigned int k;
		unsigned int n;
		uint32_t lastFirstChunk = 1;
		uint32_t lastSamplesPerChunk = 0;
		uint32_t chunkCount;
		uint32_t sampleCount = 0;
		uint32_t chunkIdx;
		uint64_t offsetInChunk;
		for (i = 0; i < tk->sampleToChunkEntryCount; i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				     lastFirstChunk;
			sampleCount += chunkCount * lastSamplesPerChunk;
			lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		sampleCount += chunkCount * lastSamplesPerChunk;

		if (sampleCount != tk->sampleCount) {
			ULOGE("sample count mismatch: %d, expected %d",
			      sampleCount,
			      tk->sampleCount);
			return -EPROTO;
		}

		if (sampleCount == 0) {
			ULOGE("invalid sample count");
			return -EPROTO;
		}
		tk->sampleOffset = malloc(sampleCount * sizeof(uint64_t));
		if (tk->sampleOffset == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}

		lastFirstChunk = 1;
		lastSamplesPerChunk = 0;
		for (i = 0, n = 0, chunkIdx = 0;
		     i < tk->sampleToChunkEntryCount;
		     i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				     lastFirstChunk;
			for (j = 0; j < chunkCount; j++, chunkIdx++) {
				for (k = 0, offsetInChunk = 0;
				     k < lastSamplesPerChunk;
				     k++, n++) {
					tk->sampleOffset[n] =
						tk->chunkOffset[chunkIdx] +
						offsetInChunk;
					offsetInChunk += tk->sampleSize[n];
				}
			}
			lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		for (j = 0; j < chunkCount; j++, chunkIdx++) {
			for (k = 0, offsetInChunk = 0; k < lastSamplesPerChunk;
			     k++, n++) {
				tk->sampleOffset[n] =
					tk->chunkOffset[chunkIdx] +
					offsetInChunk;
				offsetInChunk += tk->sampleSize[n];
			}
		}

		for (i = 0, sampleCount = 0; i < tk->timeToSampleEntryCount;
		     i++)
			sampleCount += tk->timeToSampleEntries[i].sampleCount;

		if (sampleCount != tk->sampleCount) {
			ULOGE("sample count mismatch: %d, expected %d",
			      sampleCount,
			      tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleDecodingTime =
			malloc(tk->sampleCount * sizeof(uint64_t));
		if (tk->sampleDecodingTime == NULL) {
			ULOG_ERRNO("malloc", ENOMEM);
			return -ENOMEM;
		}

		uint64_t ts = 0;
		k = 0;
		for (i = 0; i < tk->timeToSampleEntryCount; i++) {
			const struct mp4_time_to_sample_entry *entry =
				&tk->timeToSampleEntries[i];

			for (j = 0; j < entry->sampleCount; j++, k++) {
				if (k >= sampleCount) {
					ULOGE("time-to-sample entries exceed "
					      "sample count");
					free(tk->sampleDecodingTime);
					tk->sampleDecodingTime = NULL;
					return -EPROTO;
				}

				tk->sampleDecodingTime[k] = ts;

				if (entry->sampleDelta > UINT64_MAX - ts) {
					ULOGE("timestamp overflow at sample %u",
					      k);
					free(tk->sampleDecodingTime);
					tk->sampleDecodingTime = NULL;
					return -EOVERFLOW;
				}

				ts += entry->sampleDelta;
			}
		}

		switch (tk->type) {
		case MP4_TRACK_TYPE_VIDEO:
			videoTrackCount++;
			videoTk = tk;
			break;
		case MP4_TRACK_TYPE_AUDIO:
			audioTrackCount++;
			break;
		case MP4_TRACK_TYPE_HINT:
			hintTrackCount++;
			break;
		case MP4_TRACK_TYPE_METADATA:
			metadataTrackCount++;
			metaTk = tk;
			break;
		case MP4_TRACK_TYPE_TEXT:
			textTrackCount++;
			break;
		default:
			break;
		}

		/* Link tracks using track references */
		for (i = 0; i < tk->referenceTrackIdCount; i++) {
			struct mp4_track *tkRef;
			tkRef = mp4_track_find_by_id(mp4,
						     tk->referenceTrackId[i]);
			if (tkRef == NULL) {
				ULOGW("track reference: track ID %d not found",
				      tk->referenceTrackId[i]);
				continue;
			}

			if ((tk->referenceType ==
			     MP4_REFERENCE_TYPE_DESCRIPTION) &&
			    (tk->type == MP4_TRACK_TYPE_METADATA)) {
				tkRef->metadata = tk;
			} else if ((tk->referenceType ==
				    MP4_REFERENCE_TYPE_CHAPTERS) &&
				   (tkRef->type == MP4_TRACK_TYPE_TEXT)) {
				tk->chapters = tkRef;
				tkRef->type = MP4_TRACK_TYPE_CHAPTERS;
				chapTk = tkRef;
			}
		}
	}

	/* Workaround: if we have only 1 video track and 1 metadata
	 * track with no track reference, link them anyway */
	if ((videoTrackCount == 1) && (metadataTrackCount == 1) &&
	    (audioTrackCount == 0) && (hintTrackCount == 0) && videoTk &&
	    metaTk && (!videoTk->metadata))
		videoTk->metadata = metaTk;

	/* Build the chapter list */
	if (chapTk) {
		for (unsigned int i = 0; i < chapTk->sampleCount; i++) {
			unsigned int sampleSize;
			unsigned int readBytes = 0;
			uint16_t sz;
			sampleSize = chapTk->sampleSize[i];
			off_t _ret = lseek(
				mp4->fd, chapTk->sampleOffset[i], SEEK_SET);
			if (_ret == -1) {
				ULOG_ERRNO("lseek", errno);
				return -errno;
			}
			MP4_READ_16(mp4->fd, sz, readBytes);
			sz = ntohs(sz);
			if (sz <= sampleSize - readBytes) {
				if (mp4->chaptersCount >= MP4_CHAPTERS_MAX) {
					max_chapter_reached = true;
					break;
				}
				char *chapName = malloc(sz + 1);
				if (chapName == NULL)
					return -ENOMEM;
				mp4->chaptersName[mp4->chaptersCount] =
					chapName;
				ssize_t count = read(mp4->fd, chapName, sz);
				if (count == -1) {
					ret = -errno;
					ULOG_ERRNO("read", -ret);
					return ret;
				} else if (count != (ssize_t)sz) {
					ret = -ENODATA;
					ULOG_ERRNO("read", -ret);
					return ret;
				}
				chapName[sz] = '\0';
				uint64_t chapTime = mp4_sample_time_to_usec(
					chapTk->sampleDecodingTime[i],
					chapTk->timescale);
				ULOGD("chapter #%d time=%" PRIu64 " '%s'",
				      mp4->chaptersCount + 1,
				      chapTime,
				      chapName);
				mp4->chaptersTime[mp4->chaptersCount] =
					chapTime;
				mp4->chaptersCount++;
			}
		}
	}
	if (max_chapter_reached && chapTk->sampleCount > MP4_CHAPTERS_MAX) {
		ULOGW("chapter count exceeds limit (%d/%d), extra chapters "
		      "ignored",
		      chapTk->sampleCount,
		      MP4_CHAPTERS_MAX);
	}
	return 0;
}

#pragma GCC diagnostic pop

