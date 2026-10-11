/* Fragment sample indexing for complete, seekable ISO BMFF files. */
#include "mp4_priv.h"

#define BOX(a, b, c, d)                                                        \
	((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (c) << 8 | (d))

struct fragment_reader {
	struct mp4_file *file;
	uint64_t pos, end;
	int error;
};
struct fragment_box {
	uint32_t type;
	uint64_t start;
	struct fragment_reader data;
};
struct media_range {
	uint64_t start, end;
};
struct fragment_index {
	struct mp4_file *file;
	struct media_range *media;
	size_t count;
	uint64_t truncated_start;
	bool truncated;
};

struct track_checkpoint {
	struct mp4_track *track;
	uint32_t sample_count;
	uint32_t sync_count;
	uint32_t sample_max_size;
	uint64_t decode_end;
	uint64_t duration;
};

static uint32_t word(struct fragment_reader *r)
{
	unsigned char b[4];
	if (r->error)
		return 0;
	if (r->pos > r->end || r->end - r->pos < 4) {
		r->error = -EPROTO;
		return 0;
	}
	if (lseek(r->file->fd, (off_t)r->pos, SEEK_SET) < 0 ||
	    read(r->file->fd, b, 4) != 4) {
		r->error = -EIO;
		return 0;
	}
	r->pos += 4;
	return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 |
	       (uint32_t)b[2] << 8 | b[3];
}

static uint64_t wide(struct fragment_reader *r)
{
	uint64_t high = word(r);
	return high << 32 | word(r);
}

static int next_box(struct fragment_reader *r, struct fragment_box *box)
{
	if (r->error || r->pos == r->end)
		return 0;
	box->start = r->pos;
	uint64_t size = word(r);
	box->type = word(r);
	if (size == 1)
		size = wide(r);
	else if (size == 0)
		size = r->end - box->start;
	if (r->error || size < r->pos - box->start ||
	    size > r->end - box->start) {
		r->error = -EPROTO;
		return 0;
	}
	box->data = *r;
	box->data.end = box->start + size;
	r->pos = box->data.end;
	return 1;
}

/* Only descend into moov/mvex; media payloads are never read during indexing.*/
static int defaults_read(struct fragment_reader r, unsigned depth)
{
	struct fragment_box b;
	while (next_box(&r, &b)) {
		if ((depth == 0 && b.type == BOX('m', 'v', 'e', 'x'))) {
			int ret = defaults_read(b.data, depth + 1);
			if (ret < 0)
				return ret;
		} else if (depth == 1 && b.type == BOX('t', 'r', 'e', 'x')) {
			uint32_t version = word(&b.data);
			uint32_t id = word(&b.data);
			struct mp4_track *t = mp4_track_find_by_id(r.file, id);
			if (!t || version || t->fragmentDefaults)
				return -EPROTO;
			t->fragmentDescription = word(&b.data);
			t->fragmentDuration = word(&b.data);
			t->fragmentSize = word(&b.data);
			t->fragmentFlags = word(&b.data);
			if (b.data.error)
				return b.data.error;
			t->fragmentDefaults = true;
		}
	}
	return r.error;
}

static int reserve_samples(struct mp4_track *t, uint32_t count)
{
	if (count <= t->fragmentCapacity)
		return 0;
	uint64_t capacity = (uint64_t)t->fragmentCapacity * 2;
	if (capacity < count)
		capacity = count;
	/* Public sample search APIs use signed indices. */
	if (capacity > INT_MAX)
		capacity = count;
	if (capacity > INT_MAX || capacity > SIZE_MAX / sizeof(uint64_t))
		return -EOVERFLOW;
	void *p = realloc(t->sampleSize, capacity * sizeof(*t->sampleSize));
	if (!p)
		return -ENOMEM;
	t->sampleSize = p;
	p = realloc(t->sampleOffset, capacity * sizeof(*t->sampleOffset));
	if (!p)
		return -ENOMEM;
	t->sampleOffset = p;
	p = realloc(t->sampleDecodingTime,
		    capacity * sizeof(*t->sampleDecodingTime));
	if (!p)
		return -ENOMEM;
	t->sampleDecodingTime = p;
	bool all_sync = !t->syncSampleEntries;
	p = realloc(t->syncSampleEntries,
		    capacity * sizeof(*t->syncSampleEntries));
	if (!p)
		return -ENOMEM;
	t->syncSampleEntries = p;
	if (all_sync) {
		for (uint32_t i = 0; i < t->sampleCount; i++)
			t->syncSampleEntries[i] = i + 1;
		t->syncSampleEntryCount = t->sampleCount;
	}
	t->fragmentCapacity = capacity;
	return 0;
}

static bool in_media(const struct fragment_index *idx, uint64_t offset,
		     uint32_t size)
{
	size_t low = 0, high = idx->count;
	while (low < high) {
		size_t mid = low + (high - low) / 2;
		if (idx->media[mid].start <= offset)
			low = mid + 1;
		else
			high = mid;
	}
	return low && offset <= idx->media[low - 1].end &&
	       size <= idx->media[low - 1].end - offset;
}

static int save_tracks(struct mp4_file *file,
		       struct track_checkpoint **checkpoints,
		       size_t *count)
{
	struct mp4_track *t;
	size_t n = 0;
	list_walk_entry_forward(&file->tracks, t, node)
		n++;
	struct track_checkpoint *saved = calloc(n, sizeof(*saved));
	if (n && !saved)
		return -ENOMEM;
	n = 0;
	list_walk_entry_forward(&file->tracks, t, node)
	{
		saved[n++] = (struct track_checkpoint){
			.track = t,
			.sample_count = t->sampleCount,
			.sync_count = t->syncSampleEntryCount,
			.sample_max_size = t->sampleMaxSize,
			.decode_end = t->fragmentDecodeEnd,
			.duration = t->duration,
		};
	}
	*checkpoints = saved;
	*count = n;
	return 0;
}

static void restore_tracks(const struct track_checkpoint *saved, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		struct mp4_track *t = saved[i].track;
		t->sampleCount = saved[i].sample_count;
		t->syncSampleEntryCount = saved[i].sync_count;
		t->sampleMaxSize = saved[i].sample_max_size;
		t->fragmentDecodeEnd = saved[i].decode_end;
		t->duration = saved[i].duration;
	}
}

static int traf_read(struct fragment_index *idx, struct fragment_reader r,
		     uint64_t moof, uint64_t *implicit_base)
{
	struct fragment_box b;
	struct fragment_reader headers = r;
	struct mp4_track *t = NULL;
	uint32_t duration = 0, size = 0, flags = 0, description = 0, tfhd = 0;
	uint64_t base = *implicit_base, time = 0;
	bool have_time = false;
	while (next_box(&headers, &b)) {
		if (b.type == BOX('t', 'f', 'h', 'd')) {
			if (t)
				return -EPROTO;
			tfhd = word(&b.data);
			t = mp4_track_find_by_id(idx->file, word(&b.data));
			if (!t || !t->fragmentDefaults || (tfhd & ~0x03003bU))
				return -EPROTO;
			description = t->fragmentDescription;
			duration = t->fragmentDuration;
			size = t->fragmentSize;
			flags = t->fragmentFlags;
			if (tfhd & 1)
				base = wide(&b.data);
			else if (tfhd & 0x020000)
				base = moof;
			if (tfhd & 2)
				description = word(&b.data);
			if (tfhd & 8)
				duration = word(&b.data);
			if (tfhd & 16)
				size = word(&b.data);
			if (tfhd & 32)
				flags = word(&b.data);
			/* The existing stsd reader supports one decoder configuration. */
			if (description != 1)
				return -ENOSYS;
		} else if (b.type == BOX('t', 'f', 'd', 't')) {
			if (have_time)
				return -EPROTO;
			uint32_t version = word(&b.data);
			if (version != 0 && version != 0x01000000)
				return -EPROTO;
			time = version ? wide(&b.data) : word(&b.data);
			have_time = true;
		} else if (b.type == BOX('s', 'e', 'n', 'c') ||
			   b.type == BOX('s', 'a', 'i', 'z') ||
			   b.type == BOX('s', 'a', 'i', 'o'))
			return -ENOSYS;
		if (b.data.error)
			return b.data.error;
	}
	if (headers.error || !t || !t->timescale)
		return -EPROTO;
	if (!have_time)
		time = t->fragmentDecodeEnd;
	bool check_continuity = t->type == MP4_TRACK_TYPE_AUDIO &&
				t->sampleCount && time != t->fragmentDecodeEnd;
	uint64_t offset = base;
	while (next_box(&r, &b)) {
		if (b.type != BOX('t', 'r', 'u', 'n'))
			continue;
		uint32_t full = word(&b.data), count = word(&b.data);
		uint32_t run_flags = full & 0xffffff;
		if ((full >> 24) > 1 || (run_flags & ~0xf05U) ||
		    ((run_flags & 4) && (run_flags & 0x400)) ||
		    ((tfhd & 0x010000) && count))
			return -EPROTO;
		if (run_flags & 1) {
			int64_t delta = (int32_t)word(&b.data);
			if ((delta < 0 && base < (uint64_t)-delta) ||
			    (delta >= 0 && base > UINT64_MAX - (uint64_t)delta))
				return -EOVERFLOW;
			offset = delta < 0 ? base - (uint64_t)-delta : base + (uint64_t)delta;
		}
		uint32_t first_flags = (run_flags & 4) ? word(&b.data) : flags;
		if (count && ((!size && !(run_flags & 0x200)) ||
			      (!duration && !(run_flags & 0x100))))
			return -EPROTO;
		unsigned fields = !!(run_flags & 0x100) +
				  !!(run_flags & 0x200) +
				  !!(run_flags & 0x400) + !!(run_flags & 0x800);
		if (b.data.error ||
		    (fields &&
		     count > (b.data.end - b.data.pos) / (4 * fields)) ||
		    count > INT_MAX - t->sampleCount ||
		    count > (uint64_t)idx->file->fileSize)
			return -EPROTO;
		int ret = reserve_samples(t, t->sampleCount + count);
		if (ret < 0)
			return ret;
		for (uint32_t i = 0; i < count; i++) {
			uint32_t d =
			    (run_flags & 0x100) ? word(&b.data) : duration;
			uint32_t s = (run_flags & 0x200) ? word(&b.data) : size;
			uint32_t f = (run_flags & 0x400)
					 ? word(&b.data)
					 : (i ? flags : first_flags);
			uint32_t cto = (run_flags & 0x800) ? word(&b.data) : 0;
			if (b.data.error || !d || !s ||
			    !in_media(idx, offset, s) || d > UINT64_MAX - time)
				return -EPROTO;
			if (check_continuity) {
				uint64_t drift = time < t->fragmentDecodeEnd
						 ? t->fragmentDecodeEnd - time
						 : time - t->fragmentDecodeEnd;
				/* Fragment timestamps are independently rescaled by some
				 * muxers. Ignore rounding drift up to one packet. */
				if (drift > d)
					return time < t->fragmentDecodeEnd
						       ? -EPROTO
						       : -ENOSYS;
				time = t->fragmentDecodeEnd;
				check_continuity = false;
			}
			if (cto && t->type == MP4_TRACK_TYPE_AUDIO)
				return -ENOSYS;
			uint32_t n = t->sampleCount++;
			t->sampleSize[n] = s;
			t->sampleOffset[n] = offset;
			t->sampleDecodingTime[n] = time;
			if (!(f & 0x10000))
				t->syncSampleEntries
				    [t->syncSampleEntryCount++] = n + 1;
			if (s > t->sampleMaxSize)
				t->sampleMaxSize = s;
			offset += s;
			time += d;
		}
		if (b.data.pos != b.data.end)
			return -EPROTO;
	}
	t->fragmentDecodeEnd = time;
	if (t->sampleCount && time >= t->sampleDecodingTime[0])
		t->duration = time - t->sampleDecodingTime[0];
	*implicit_base = offset;
	return r.error;
}

int mp4_fragments_read(struct mp4_file *mp4)
{
	struct fragment_index idx = {.file = mp4};
	struct fragment_reader root = {.file = mp4, .end = mp4->fileSize};
	struct fragment_reader r = root;
	struct fragment_box b;
	struct mp4_track *t;
	int ret = 0;
	/* First collect media bounds and defaults, including moov-at-end files.*/
	while (next_box(&r, &b)) {
		if (b.type == BOX('m', 'o', 'o', 'v')) {
			ret = defaults_read(b.data, 0);
			if (ret < 0)
				goto out;
		} else if (b.type == BOX('m', 'd', 'a', 't')) {
			if (idx.count >= SIZE_MAX / sizeof(*idx.media) - 1) {
				ret = -EOVERFLOW;
				goto out;
			}
			void *p = realloc(idx.media,
					  (idx.count + 1) * sizeof(*idx.media));
			if (!p) {
				ret = -ENOMEM;
				goto out;
			}
			idx.media = p;
			idx.media[idx.count++] =
			    (struct media_range){b.data.pos, b.data.end};
		}
	}
	if (r.error) {
		/* A partially written final box is a recoverable tail. Complete
		 * fragments before it remain playable. */
		idx.truncated = true;
		idx.truncated_start = b.start;
		root.end = b.start;
	}
	list_walk_entry_forward(&mp4->tracks, t, node)
	{
		for (uint32_t i = 0; i < t->timeToSampleEntryCount; i++) {
			uint64_t delta =
			    (uint64_t)t->timeToSampleEntries[i].sampleCount *
			    t->timeToSampleEntries[i].sampleDelta;
			if (delta > UINT64_MAX - t->fragmentDecodeEnd) {
				ret = -EOVERFLOW;
				goto out;
			}
			t->fragmentDecodeEnd += delta;
		}
	}
	r = root;
	while (next_box(&r, &b)) {
		if (b.type != BOX('m', 'o', 'o', 'f'))
			continue;
		struct track_checkpoint *saved = NULL;
		size_t saved_count = 0;
		bool recoverable_tail =
			idx.truncated && b.data.end == idx.truncated_start;
		if (recoverable_tail) {
			ret = save_tracks(mp4, &saved, &saved_count);
			if (ret < 0)
				goto out;
		}
		struct fragment_reader children = b.data;
		struct fragment_box child;
		uint64_t base = b.start;
		while (next_box(&children, &child)) {
			if (child.type != BOX('t', 'r', 'a', 'f'))
				continue;
			ret = traf_read(&idx, child.data, b.start, &base);
			if (ret < 0)
				break;
		}
		if (!ret && children.error)
			ret = children.error;
		if (ret < 0 && recoverable_tail) {
			restore_tracks(saved, saved_count);
			free(saved);
			ret = 0;
			break;
		}
		free(saved);
		if (ret < 0) {
			goto out;
		}
	}
	ret = r.error;
	list_walk_entry_forward(&mp4->tracks, t, node)
	{
		if (t->fragmentDefaults && t->timescale && mp4->timescale) {
			uint64_t duration;
			if (mp4_rescale_time(t->duration,
					     t->timescale,
					     mp4->timescale,
					     &duration) < 0) {
				ret = -EOVERFLOW;
				break;
			}
			if (duration > mp4->duration)
				mp4->duration = duration;
		}
		if (t->type == MP4_TRACK_TYPE_AUDIO && !t->sampleCount)
			ret = -ENODATA;
	}
out:
	free(idx.media);
	return ret;
}
