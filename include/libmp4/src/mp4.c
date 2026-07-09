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

ULOG_DECLARE_TAG(libmp4);


const char *mp4_track_type_str(enum mp4_track_type type)
{
	switch (type) {
	case MP4_TRACK_TYPE_VIDEO:
		return "VIDEO";
	case MP4_TRACK_TYPE_AUDIO:
		return "AUDIO";
	case MP4_TRACK_TYPE_HINT:
		return "HINT";
	case MP4_TRACK_TYPE_METADATA:
		return "METADATA";
	case MP4_TRACK_TYPE_TEXT:
		return "TEXT";
	case MP4_TRACK_TYPE_CHAPTERS:
		return "CHAPTERS";
	case MP4_TRACK_TYPE_UNKNOWN:
	default:
		return "UNKNOWN";
	}
}


const char *mp4_video_codec_str(enum mp4_video_codec codec)
{
	switch (codec) {
	case MP4_VIDEO_CODEC_AVC:
		return "AVC";
	case MP4_VIDEO_CODEC_HEVC:
		return "HEVC";
	case MP4_VIDEO_CODEC_UNKNOWN:
	default:
		return "UNKNOWN";
	}
}


const char *mp4_audio_codec_str(enum mp4_audio_codec codec)
{
	if (codec == MP4_AUDIO_CODEC_AAC_LC)
		return "AAC_LC";
	else
		return "UNKNOWN";
}


const char *mp4_metadata_cover_type_str(enum mp4_metadata_cover_type type)
{
	switch (type) {
	case MP4_METADATA_COVER_TYPE_JPEG:
		return "JPEG";
	case MP4_METADATA_COVER_TYPE_PNG:
		return "PNG";
	case MP4_METADATA_COVER_TYPE_BMP:
		return "BMP";
	default:
		return "UNKNOWN";
	}
}


void mp4_video_decoder_config_destroy(struct mp4_video_decoder_config *vdc)
{
	if (vdc == NULL)
		return;

	switch (vdc->codec) {
	case MP4_VIDEO_CODEC_AVC:
		free(vdc->avc.sps);
		free(vdc->avc.pps);
		vdc->avc.sps = NULL;
		vdc->avc.pps = NULL;
		break;
	case MP4_VIDEO_CODEC_HEVC:
		free(vdc->hevc.vps);
		free(vdc->hevc.sps);
		free(vdc->hevc.pps);
		vdc->hevc.vps = NULL;
		vdc->hevc.sps = NULL;
		vdc->hevc.pps = NULL;
		break;
	default:
		return;
	}
}
