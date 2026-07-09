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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


static int mp4_metadata_build(struct mp4_file *mp4)
{
	unsigned int k = 0;
	unsigned int metaCount = 0;
	unsigned int udtaCount = 0;
	unsigned int xyzCount = 0;

	for (unsigned int i = 0; i < mp4->metaMetadataCount; i++) {
		if ((mp4_validate_str_len(mp4->metaMetadataValue[i],
					  METADATA_VALUE_MAX) > 0) &&
		    (mp4_validate_str_len(mp4->metaMetadataKey[i], NAME_MAX) >
		     0))
			metaCount++;
	}

	for (unsigned int i = 0; i < mp4->udtaMetadataCount; i++) {
		if ((mp4_validate_str_len(mp4->udtaMetadataValue[i],
					  METADATA_VALUE_MAX) > 0) &&
		    (mp4_validate_str_len(mp4->udtaMetadataKey[i], NAME_MAX) >
		     0))
			udtaCount++;
	}

	if ((mp4_validate_str_len(mp4->udtaLocationValue, METADATA_VALUE_MAX) >
	     0) &&
	    (mp4_validate_str_len(mp4->udtaLocationKey, NAME_MAX) > 0))
		xyzCount++;

	mp4->finalMetadataCount = metaCount + udtaCount + xyzCount;

	if (mp4->finalMetadataCount == 0)
		goto cover;

	mp4->finalMetadataKey = calloc(mp4->finalMetadataCount, sizeof(char *));
	if (mp4->finalMetadataKey == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	mp4->finalMetadataValue =
		calloc(mp4->finalMetadataCount, sizeof(char *));
	if (mp4->finalMetadataValue == NULL) {
		ULOG_ERRNO("calloc", ENOMEM);
		return -ENOMEM;
	}

	for (unsigned int i = 0; i < mp4->metaMetadataCount; i++) {
		if (k >= mp4->finalMetadataCount)
			break;
		if ((mp4_validate_str_len(mp4->metaMetadataValue[i],
					  METADATA_VALUE_MAX) > 0) &&
		    (mp4_validate_str_len(mp4->metaMetadataKey[i], NAME_MAX) >
		     0)) {
			mp4->finalMetadataKey[k] = mp4->metaMetadataKey[i];
			mp4->finalMetadataValue[k] = mp4->metaMetadataValue[i];
			k++;
		}
	}

	for (unsigned int i = 0; i < mp4->udtaMetadataCount; i++) {
		if (k >= mp4->finalMetadataCount)
			break;
		if ((mp4_validate_str_len(mp4->udtaMetadataValue[i],
					  METADATA_VALUE_MAX) > 0) &&
		    (mp4_validate_str_len(mp4->udtaMetadataKey[i], NAME_MAX) >
		     0)) {
			mp4->finalMetadataKey[k] = mp4->udtaMetadataKey[i];
			mp4->finalMetadataValue[k] = mp4->udtaMetadataValue[i];
			k++;
		}
	}

	if ((mp4_validate_str_len(mp4->udtaLocationValue, METADATA_VALUE_MAX) >
	     0) &&
	    (mp4_validate_str_len(mp4->udtaLocationKey, NAME_MAX) > 0) &&
	    (k < mp4->finalMetadataCount)) {
		mp4->finalMetadataKey[k] = mp4->udtaLocationKey;
		mp4->finalMetadataValue[k] = mp4->udtaLocationValue;
		k++;
	}

cover:
	if (mp4->metaCoverSize > 0) {
		mp4->finalCoverSize = mp4->metaCoverSize;
		mp4->finalCoverOffset = mp4->metaCoverOffset;
		mp4->finalCoverType = mp4->metaCoverType;
	} else if (mp4->udtaCoverSize > 0) {
		mp4->finalCoverSize = mp4->udtaCoverSize;
		mp4->finalCoverOffset = mp4->udtaCoverOffset;
		mp4->finalCoverType = mp4->udtaCoverType;
	}

	return 0;
}


int mp4_demux_open(const char *filename, struct mp4_demux **ret_obj)
{
	int ret;
	off_t err;
	off_t retBytes;
	struct mp4_demux *demux;
	struct mp4_file *mp4;
	int flags = O_RDONLY;

	ULOG_ERRNO_RETURN_ERR_IF(mp4_validate_str_len(filename, PATH_MAX) == 0,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);

	demux = calloc(1, sizeof(*demux));
	if (demux == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("calloc", -ret);
		goto error;
	}
	mp4 = &demux->mp4;
	list_init(&mp4->tracks);

#ifdef O_BINARY
	flags |= O_BINARY;
#endif
	mp4->fd = open(filename, flags);
	if (mp4->fd == -1) {
		ret = -errno;
		ULOG_ERRNO("open:'%s'", -ret, filename);
		goto error;
	}

	mp4->fileSize = lseek(mp4->fd, 0, SEEK_END);
	if (mp4->fileSize < 0) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto error;
	} else if (mp4->fileSize == 0) {
		ret = -ENODATA;
		ULOGW("empty file: '%s'", filename);
		goto error;
	}
	err = lseek(mp4->fd, 0, SEEK_SET);
	if (err == -1) {
		ret = -errno;
		ULOG_ERRNO("lseek", -ret);
		goto error;
	}

	mp4->root = mp4_box_new(NULL);
	if (mp4->root == NULL) {
		ret = -ENOMEM;
		ULOG_ERRNO("mp4_box_new", -ret);
		goto error;
	}
	mp4->root->type = MP4_ROOT_BOX;
	mp4->root->size = 1;
	mp4->root->largesize = mp4->fileSize;

	retBytes = mp4_box_children_read(mp4, mp4->root, mp4->fileSize, NULL);
	if (retBytes < 0) {
		ret = OFF_T_TO_ERRNO(retBytes, EPROTO);
		goto error;
	}
	mp4->readBytes += retBytes;

	ret = mp4_tracks_build(mp4);
	if (ret < 0)
		goto error;

	ret = mp4_metadata_build(mp4);
	if (ret < 0)
		goto error;

	mp4_box_log(mp4->root, ULOG_DEBUG);

	*ret_obj = demux;
	return 0;

error:
	mp4_demux_close(demux);
	*ret_obj = NULL;
	return ret;
}


int mp4_demux_close(struct mp4_demux *demux)
{
	if (demux == NULL)
		return 0;

	if (demux) {
		struct mp4_file *mp4 = &demux->mp4;
		if (mp4->fd)
			close(mp4->fd);
		mp4_box_destroy(mp4->root);
		mp4_tracks_destroy(mp4);
		unsigned int i;
		for (i = 0; i < mp4->chaptersCount; i++)
			free(mp4->chaptersName[i]);
		free(mp4->udtaLocationKey);
		free(mp4->udtaLocationValue);
		for (i = 0; i < mp4->udtaMetadataCount; i++) {
			free(mp4->udtaMetadataKey[i]);
			free(mp4->udtaMetadataValue[i]);
		}
		free(mp4->udtaMetadataKey);
		free(mp4->udtaMetadataValue);
		for (i = 0; i < mp4->metaMetadataCount; i++) {
			free(mp4->metaMetadataKey[i]);
			free(mp4->metaMetadataValue[i]);
		}
		free(mp4->metaMetadataKey);
		free(mp4->metaMetadataValue);
		free(mp4->finalMetadataKey);
		free(mp4->finalMetadataValue);
	}

	free(demux);

	return 0;
}


static int get_seek_sample(const struct mp4_track *tk,
			   int start,
			   uint64_t requested_ts,
			   enum mp4_seek_method method)
{
	int is_sync;
	int prev_sync = -1;
	int nearest;
	int next;
	int next_sync;
	uint64_t start_ts = tk->sampleDecodingTime[start];

	switch (method) {
	case MP4_SEEK_METHOD_PREVIOUS:
		return start;
	case MP4_SEEK_METHOD_PREVIOUS_SYNC:
		is_sync = mp4_track_is_sync_sample(tk, start, &prev_sync);
		if (is_sync) {
			return start;
		} else if (prev_sync >= 0) {
			return prev_sync;
		} else {
			return get_seek_sample(tk,
					       start,
					       requested_ts,
					       MP4_SEEK_METHOD_NEXT_SYNC);
		}
	case MP4_SEEK_METHOD_NEAREST:
		nearest = mp4_track_find_sample_by_time(
			tk, requested_ts, MP4_TIME_CMP_NEAREST, 0, start);
		if (nearest >= 0)
			return nearest;
		else
			return -ENOENT;
	case MP4_SEEK_METHOD_NEAREST_SYNC:
		nearest = mp4_track_find_sample_by_time(
			tk, requested_ts, MP4_TIME_CMP_NEAREST, 1, start);
		if (nearest >= 0)
			return nearest;
		else
			return -ENOENT;
	case MP4_SEEK_METHOD_NEXT:
		next = mp4_track_find_sample_by_time(
			tk, start_ts, MP4_TIME_CMP_GT, 0, start);
		if (next >= 0)
			return next;
		else
			return -ENOENT;
	case MP4_SEEK_METHOD_NEXT_SYNC:
		next_sync = mp4_track_find_sample_by_time(
			tk, start_ts, MP4_TIME_CMP_GT, 1, start);
		if (next_sync >= 0)
			return next_sync;
		else
			return -ENOENT;
	default:
		ULOGE("unsupported seek method: %d", method);
		return -EINVAL;
	}
}


static int get_metadata_sample_from_ref_track(const struct mp4_track *ref_tk,
					      int ref_sample)
{
	int idx;
	const struct mp4_track *metatk = ref_tk->metadata;
	uint64_t prev_sample_time = INT64_MAX;
	uint64_t next_sample_time = INT64_MAX;

	if (!metatk)
		return -ENOENT;

	uint64_t sample_time = ref_tk->sampleDecodingTime[ref_sample];
	if ((ref_sample - 1) >= 0)
		prev_sample_time = ref_tk->sampleDecodingTime[ref_sample - 1];
	if ((ref_sample + 1) < (int)ref_tk->sampleCount)
		next_sample_time = ref_tk->sampleDecodingTime[ref_sample + 1];

	idx = mp4_track_find_sample_by_time(
		metatk,
		mp4_convert_timescale(
			sample_time, ref_tk->timescale, metatk->timescale),
		MP4_TIME_CMP_NEAREST,
		0,
		0);
	if (idx < 0)
		return -ENOENT;

	uint64_t meta_sample_time =
		mp4_convert_timescale(metatk->sampleDecodingTime[idx],
				      metatk->timescale,
				      ref_tk->timescale);

	/* Exact match: return */
	if (meta_sample_time == sample_time)
		return idx;

	uint64_t delta_time =
		llabs((int64_t)meta_sample_time - (int64_t)sample_time);
	uint64_t prev_delta_time =
		llabs((int64_t)meta_sample_time - (int64_t)prev_sample_time);
	uint64_t next_delta_time =
		llabs((int64_t)meta_sample_time - (int64_t)next_sample_time);

	/* Return metadata sample only if closest to the current sample */
	if ((delta_time < prev_delta_time) && (delta_time < next_delta_time))
		return idx;

	return -ENOENT;
}


int mp4_demux_seek(const struct mp4_demux *demux,
		   uint64_t time_offset,
		   enum mp4_seek_method method)
{
	struct mp4_track *tk = NULL;
	const struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	list_walk_entry_forward(&mp4->tracks, tk, node)
	{
		int found = 0;
		int idx = 0;
		uint64_t ts =
			mp4_usec_to_sample_time(time_offset, tk->timescale);
		uint64_t newPendingSeekTime = 0;
		int start = (unsigned int)(((uint64_t)tk->sampleCount * ts +
					    tk->duration - 1) /
					   tk->duration);
		if (start < 0)
			start = 0;
		if ((unsigned)start >= tk->sampleCount)
			start = tk->sampleCount - 1;
		while (((unsigned)start < tk->sampleCount - 1) &&
		       (tk->sampleDecodingTime[start] < ts))
			start++;
		for (int i = start; i >= 0; i--) {
			if (tk->sampleDecodingTime[i] <= ts) {
				idx = get_seek_sample(tk, i, ts, method);
				if (idx < 0)
					break;
				newPendingSeekTime =
					(idx == i) ? 0
						   : tk->sampleDecodingTime[i];
				found = 1;
				break;
			}
		}
		if (found) {
			tk->nextSample = idx;
			tk->pendingSeekTime = newPendingSeekTime;
			ULOGD("seek to %" PRIu64 " -> sample #%d time %" PRIu64,
			      time_offset,
			      idx,
			      mp4_sample_time_to_usec(
				      tk->sampleDecodingTime[idx],
				      tk->timescale));
		} else {
			ULOGE("unable to seek in track");
			return -ENOENT;
		}
	}

	return 0;
}


int mp4_demux_get_media_info(const struct mp4_demux *demux,
			     struct mp4_media_info *media_info)
{
	const struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(media_info == NULL, EINVAL);

	mp4 = &demux->mp4;

	memset(media_info, 0, sizeof(*media_info));

	media_info->duration =
		mp4_sample_time_to_usec(mp4->duration, mp4->timescale);
	media_info->creation_time =
		(mp4->creationTime >= MP4_MAC_TO_UNIX_EPOCH_OFFSET)
			? mp4->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET
			: mp4->creationTime;
	media_info->modification_time =
		(mp4->modificationTime >= MP4_MAC_TO_UNIX_EPOCH_OFFSET)
			? mp4->modificationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET
			: mp4->modificationTime;
	media_info->track_count = mp4->trackCount;

	return 0;
}


int mp4_demux_get_track_count(const struct mp4_demux *demux)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	return demux->mp4.trackCount;
}


int mp4_demux_get_track_info(const struct mp4_demux *demux,
			     unsigned int track_idx,
			     struct mp4_track_info *track_info)
{
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_info == NULL, EINVAL);

	mp4 = &demux->mp4;

	ULOG_ERRNO_RETURN_ERR_IF(track_idx >= mp4->trackCount, ENOENT);

	tk = mp4_track_find_by_idx(mp4, track_idx);
	if (tk == NULL) {
		ULOGE("track index=%d not found", track_idx);
		return -ENOENT;
	}

	memset(track_info, 0, sizeof(*track_info));
	track_info->id = tk->id;
	track_info->name = tk->name;
	track_info->enabled = tk->enabled;
	track_info->in_movie = tk->in_movie;
	track_info->in_preview = tk->in_preview;
	track_info->type = tk->type;
	track_info->timescale = tk->timescale;
	track_info->duration = tk->duration;
	track_info->creation_time =
		(tk->creationTime >= MP4_MAC_TO_UNIX_EPOCH_OFFSET)
			? tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET
			: tk->creationTime;
	track_info->modification_time =
		(tk->modificationTime >= MP4_MAC_TO_UNIX_EPOCH_OFFSET)
			? tk->modificationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET
			: tk->modificationTime;
	track_info->sample_count = tk->sampleCount;
	track_info->sample_max_size = tk->sampleMaxSize;
	track_info->sample_offsets = tk->sampleOffset;
	track_info->sample_sizes = tk->sampleSize;
	track_info->has_metadata = (tk->metadata) ? 1 : 0;
	if (tk->metadata) {
		track_info->metadata_content_encoding =
			tk->metadata->contentEncoding;
		track_info->metadata_mime_format = tk->metadata->mimeFormat;
	}
	if (tk->type == MP4_TRACK_TYPE_METADATA) {
		track_info->content_encoding = tk->contentEncoding;
		track_info->mime_format = tk->mimeFormat;
	} else if (tk->type == MP4_TRACK_TYPE_VIDEO) {
		track_info->video_codec = tk->vdc.codec;
		track_info->video_width = tk->vdc.width;
		track_info->video_height = tk->vdc.height;
	} else if (tk->type == MP4_TRACK_TYPE_AUDIO) {
		track_info->audio_codec = tk->audioCodec;
		track_info->audio_channel_count = tk->audioChannelCount;
		track_info->audio_sample_size = tk->audioSampleSize;
		track_info->audio_sample_rate =
			(float)tk->audioSampleRate / 65536.;
	}

	return 0;
}


int mp4_demux_get_track_video_decoder_config(
	const struct mp4_demux *demux,
	unsigned int track_id,
	struct mp4_video_decoder_config *vdc)
{
	const struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(vdc == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}
	if (tk->type != MP4_TRACK_TYPE_VIDEO) {
		ULOGE("track id=%d is not of video type", track_id);
		return -EINVAL;
	}

	vdc->width = tk->vdc.width;
	vdc->height = tk->vdc.height;

	switch (tk->vdc.codec) {
	case MP4_VIDEO_CODEC_HEVC:
		vdc->codec = MP4_VIDEO_CODEC_HEVC;
		vdc->hevc.hvcc_info = tk->vdc.hevc.hvcc_info;
		if (tk->vdc.hevc.vps) {
			vdc->hevc.vps = tk->vdc.hevc.vps;
			vdc->hevc.vps_size = tk->vdc.hevc.vps_size;
		}
		if (tk->vdc.hevc.sps) {
			vdc->hevc.sps = tk->vdc.hevc.sps;
			vdc->hevc.sps_size = tk->vdc.hevc.sps_size;
		}
		if (tk->vdc.hevc.pps) {
			vdc->hevc.pps = tk->vdc.hevc.pps;
			vdc->hevc.pps_size = tk->vdc.hevc.pps_size;
		}
		break;
	case MP4_VIDEO_CODEC_AVC:
		vdc->codec = MP4_VIDEO_CODEC_AVC;
		if (tk->vdc.avc.sps) {
			vdc->avc.sps = tk->vdc.avc.sps;
			vdc->avc.sps_size = tk->vdc.avc.sps_size;
		}
		if (tk->vdc.avc.pps) {
			vdc->avc.pps = tk->vdc.avc.pps;
			vdc->avc.pps_size = tk->vdc.avc.pps_size;
		}
		break;
	default:
		ULOGE("track id=%d video codec is neither AVC nor HEVC",
		      track_id);
		return -EINVAL;
	}

	return 0;
}


int mp4_demux_get_track_audio_specific_config(const struct mp4_demux *demux,
					      unsigned int track_id,
					      uint8_t **audio_specific_config,
					      unsigned int *asc_size)
{
	const struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	if (audio_specific_config != NULL)
		*audio_specific_config = NULL;
	if (asc_size != NULL)
		*asc_size = 0;

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}
	if (tk->type != MP4_TRACK_TYPE_AUDIO) {
		ULOGE("track id=%d is not of audio type", track_id);
		return -EINVAL;
	}

	if (tk->audioSpecificConfig == NULL) {
		ULOGE("track does not have an AudioSpecificConfig");
		return -EPROTO;
	}

	if (audio_specific_config != NULL)
		*audio_specific_config = tk->audioSpecificConfig;

	if (asc_size != NULL)
		*asc_size = tk->audioSpecificConfigSize;

	return 0;
}


int mp4_demux_get_track_sample(const struct mp4_demux *demux,
			       unsigned int track_id,
			       int advance,
			       uint8_t *sample_buffer,
			       unsigned int sample_buffer_size,
			       uint8_t *metadata_buffer,
			       unsigned int metadata_buffer_size,
			       struct mp4_track_sample *track_sample)
{
	const struct mp4_file *mp4;
	struct mp4_track *tk = NULL;
	int idx;
	uint64_t sampleTime;
	uint32_t sample_size;
	uint32_t metadata_size;
	uint64_t sample_offset;
	uint64_t metadata_offset;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(track_sample == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	memset(track_sample, 0, sizeof(*track_sample));

	if (tk->nextSample >= tk->sampleCount)
		return 0;

	sample_size = tk->sampleSize[tk->nextSample];
	sample_offset = tk->sampleOffset[tk->nextSample];
	track_sample->size = sample_size;
	track_sample->offset = sample_offset;
	if (sample_buffer && (sample_size > 0) &&
	    (sample_size <= sample_buffer_size)) {
		off_t _ret = lseek(mp4->fd, sample_offset, SEEK_SET);
		if (_ret == -1) {
			ULOG_ERRNO("lseek", errno);
			return -errno;
		}
		ssize_t count = read(mp4->fd, sample_buffer, sample_size);
		if (count == -1) {
			track_sample->size = 0;
			_ret = -errno;
			ULOG_ERRNO("read", -_ret);
			return _ret;
		} else if (count != (ssize_t)sample_size) {
			track_sample->size = 0;
			_ret = -ENODATA;
			ULOG_ERRNO("read", -_ret);
			return _ret;
		}
	} else if (sample_buffer && (sample_size > sample_buffer_size)) {
		ULOGE("buffer too small (%d bytes, %d needed)",
		      sample_buffer_size,
		      sample_size);
		return -ENOBUFS;
	}
	sampleTime = tk->sampleDecodingTime[tk->nextSample];
	if (tk->metadata) {
		const struct mp4_track *metatk = tk->metadata;
		idx = get_metadata_sample_from_ref_track(tk, tk->nextSample);
		if (idx < 0) {
			ULOGD("no metadata available at sample time: %" PRIu64,
			      sampleTime);
		} else {
			metadata_size = metatk->sampleSize[idx];
			metadata_offset = metatk->sampleOffset[idx];
			track_sample->metadata_size = metadata_size;
			if (metadata_buffer && (metadata_size > 0) &&
			    (metadata_size <= metadata_buffer_size)) {
				off_t _ret = lseek(
					mp4->fd, metadata_offset, SEEK_SET);
				if (_ret == -1) {
					ULOG_ERRNO("lseek", errno);
					return -errno;
				}
				ssize_t count = read(mp4->fd,
						     metadata_buffer,
						     metadata_size);
				if (count == -1) {
					track_sample->metadata_size = 0;
					_ret = -errno;
					if (_ret == 0)
						_ret = -ENODATA;
					ULOG_ERRNO("read", -_ret);
					return _ret;
				}
			} else if (metadata_buffer &&
				   (metadata_size > metadata_buffer_size)) {
				ULOGE("buffer too small for metadata "
				      "(%d bytes, %d needed)",
				      metadata_buffer_size,
				      metadata_size);
				return -ENOBUFS;
			}
		}
	}
	track_sample->silent =
		((tk->pendingSeekTime) && (sampleTime < tk->pendingSeekTime));
	if (sampleTime >= tk->pendingSeekTime)
		tk->pendingSeekTime = 0;
	track_sample->dts = sampleTime;
	track_sample->next_dts =
		(tk->nextSample < tk->sampleCount - 1)
			? tk->sampleDecodingTime[tk->nextSample + 1]
			: 0;
	idx = mp4_track_find_sample_by_time(
		tk, sampleTime, MP4_TIME_CMP_LT, 1, tk->nextSample);
	if (idx >= 0)
		track_sample->prev_sync_dts = tk->sampleDecodingTime[idx];
	idx = mp4_track_find_sample_by_time(
		tk, sampleTime, MP4_TIME_CMP_GT, 1, tk->nextSample);
	if (idx >= 0)
		track_sample->next_sync_dts = tk->sampleDecodingTime[idx];
	track_sample->sync = mp4_track_is_sync_sample(tk, tk->nextSample, NULL);

	if (advance)
		tk->nextSample++;

	return 0;
}


int mp4_demux_seek_to_track_prev_sample(const struct mp4_demux *demux,
					unsigned int track_id)
{
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	if (tk->nextSample == 1)
		return -ENOENT;

	idx = (tk->nextSample >= 2) ? tk->nextSample - 2 : 0;
	ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
				     tk->timescale);

	return mp4_demux_seek(demux, ts, MP4_SEEK_METHOD_PREVIOUS_SYNC);
}


int mp4_demux_seek_to_track_next_sample(const struct mp4_demux *demux,
					unsigned int track_id,
					bool resync)
{
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;
	int idx;
	uint64_t ts;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	if (resync) {
		if (tk->nextSample >= tk->sampleCount)
			return -ENOENT;
		idx = tk->nextSample;
	} else {
		if (tk->nextSample >= tk->sampleCount - 1)
			return -ENOENT;
		idx = tk->nextSample + 1;
	}

	ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
				     tk->timescale);

	if (resync)
		return mp4_demux_seek(demux, ts, MP4_SEEK_METHOD_PREVIOUS_SYNC);
	else
		return mp4_demux_seek(demux, ts, MP4_SEEK_METHOD_PREVIOUS);
}


int mp4_demux_get_track_prev_sample_time(const struct mp4_demux *demux,
					 unsigned int track_id,
					 uint64_t *sample_time)
{
	int ret = 0;
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;
	uint64_t prev_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	if (tk->nextSample >= 2) {
		prev_ts = mp4_sample_time_to_usec(
			tk->sampleDecodingTime[tk->nextSample - 2],
			tk->timescale);
	} else {
		ret = -ENOENT;
	}

exit:
	*sample_time = prev_ts;
	return ret;
}


int mp4_demux_get_track_next_sample_time(const struct mp4_demux *demux,
					 unsigned int track_id,
					 uint64_t *sample_time)
{
	int ret = 0;
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;
	uint64_t next_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	if (tk->nextSample < tk->sampleCount) {
		next_ts = mp4_sample_time_to_usec(
			tk->sampleDecodingTime[tk->nextSample], tk->timescale);
	} else {
		ret = -ENOENT;
	}

exit:
	*sample_time = next_ts;
	return ret;
}


static int mp4_demux_get_track_sample_time(const struct mp4_demux *demux,
					   unsigned int track_id,
					   uint64_t time,
					   int sync,
					   enum mp4_time_cmp cmp,
					   uint64_t *sample_time)
{
	const struct mp4_file *mp4;
	const struct mp4_track *tk = NULL;
	int idx;
	int ret;
	uint64_t ts = 0;
	uint64_t sample_ts = 0;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(sample_time == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		ret = -ENOENT;
		goto exit;
	}

	ts = mp4_usec_to_sample_time(time, tk->timescale);
	idx = mp4_track_find_sample_by_time(tk, ts, cmp, sync, -1);

	if (idx >= 0) {
		sample_ts = mp4_sample_time_to_usec(tk->sampleDecodingTime[idx],
						    tk->timescale);
		ret = 0;
	} else {
		ULOGE("no sample found for the requested time");
		ret = -ENOENT;
	}

exit:
	*sample_time = sample_ts;
	return ret;
}

int mp4_demux_get_track_prev_sample_time_before(const struct mp4_demux *demux,
						unsigned int track_id,
						uint64_t time,
						int sync,
						uint64_t *sample_time)
{
	return mp4_demux_get_track_sample_time(
		demux, track_id, time, sync, MP4_TIME_CMP_LT, sample_time);
}


int mp4_demux_get_track_next_sample_time_after(const struct mp4_demux *demux,
					       unsigned int track_id,
					       uint64_t time,
					       int sync,
					       uint64_t *sample_time)
{
	return mp4_demux_get_track_sample_time(
		demux, track_id, time, sync, MP4_TIME_CMP_GT, sample_time);
}


int mp4_demux_get_chapters(struct mp4_demux *demux,
			   unsigned int *chapters_count,
			   uint64_t **chapters_time,
			   char ***chapters_name)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_time == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(chapters_name == NULL, EINVAL);

	*chapters_count = demux->mp4.chaptersCount;
	*chapters_time = demux->mp4.chaptersTime;
	*chapters_name = demux->mp4.chaptersName;

	return 0;
}


int mp4_demux_get_metadata_strings(struct mp4_demux *demux,
				   unsigned int *count,
				   char ***keys,
				   char ***values)
{
	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(keys == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(values == NULL, EINVAL);

	*count = demux->mp4.finalMetadataCount;
	*keys = demux->mp4.finalMetadataKey;
	*values = demux->mp4.finalMetadataValue;

	return 0;
}


int mp4_demux_get_track_metadata_strings(const struct mp4_demux *demux,
					 unsigned int track_id,
					 unsigned int *count,
					 char ***keys,
					 char ***values)
{
	const struct mp4_file *mp4;
	struct mp4_track *tk = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(count == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(keys == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(values == NULL, EINVAL);

	mp4 = &demux->mp4;

	tk = mp4_track_find_by_id(mp4, track_id);
	if (tk == NULL) {
		ULOGE("track id=%d not found", track_id);
		return -ENOENT;
	}

	*count = tk->staticMetadataCount;
	*keys = tk->staticMetadataKey;
	*values = tk->staticMetadataValue;

	return 0;
}


int mp4_demux_get_metadata_cover(const struct mp4_demux *demux,
				 uint8_t *cover_buffer,
				 unsigned int cover_buffer_size,
				 unsigned int *cover_size,
				 enum mp4_metadata_cover_type *cover_type)
{
	int ret;
	const struct mp4_file *mp4;

	ULOG_ERRNO_RETURN_ERR_IF(demux == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(cover_size == NULL, EINVAL);

	mp4 = &demux->mp4;

	if (mp4->finalCoverSize > 0) {
		*cover_size = mp4->finalCoverSize;
		if (cover_type)
			*cover_type = mp4->finalCoverType;
		if (cover_buffer &&
		    (mp4->finalCoverSize <= cover_buffer_size)) {
			off_t _ret =
				lseek(mp4->fd, mp4->finalCoverOffset, SEEK_SET);
			if (_ret == -1) {
				ULOG_ERRNO("lseek", errno);
				return -errno;
			}
			ssize_t count = read(
				mp4->fd, cover_buffer, mp4->finalCoverSize);
			if (count == -1) {
				ret = -errno;
				ULOG_ERRNO("read", -ret);
				return ret;
			} else if (count != (ssize_t)mp4->finalCoverSize) {
				ret = -ENODATA;
				ULOG_ERRNO("read", -ret);
				return ret;
			}
		} else if (cover_buffer) {
			ULOGE("buffer too small (%d bytes, %d needed)",
			      cover_buffer_size,
			      mp4->finalCoverSize);
			return -ENOBUFS;
		}
	} else {
		*cover_size = 0;
	}

	return 0;
}
