/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 *
 * import.c
 * importing an SMF file
 */

#include <stdlib.h> /* malloc */
#include <memory.h> /* memset */
#include "vomid_local.h"
#include "3rdparty/sha1/sha1.h"

#define ZERO_DTIME 1

#define note_offed mark

typedef struct noteon_t {
	time_t time;
	pitch_t pitch;
	uchar vel;
} noteon_t;

typedef struct noteoff_t {
	channel_t *channel;
	time_t time;
	midipitch_t midipitch;
	uchar vel;
	uchar offed:1;
} noteoff_t;

typedef struct import_ctx_t {
	file_t *file;
	track_t *track;
	time_t time;

	noteon_t on[CHANNELS][NOTES];
	stack_t offs;

	pitch_t pitch;
	int drums, nodrums;

	unsigned char sha[SHA1_SIZE];
	bool_t sha_specified;
	SHA_CTX sha_ctx;
} import_ctx_t;

static int
read_varlen(uchar **_s, uchar *e)
{
	time_t t = 0;

	for (uchar *s = *_s; s < e; ) {
		uchar b = *s++;

		if (b < 0x80) {
			t += b;
			*_s = s;
			return t;
		} else {
			t += b & 0x7F;
			if(t < 0 || t != t << 7 >> 7)
				return -1;
		}
		t <<= 7;
	}
	return -1;
}

static int
read_int(void *buf, int len)
{
	uchar *b = buf;
	int ret = 0;

	for (int i = 0; i < len; i++) {
		ret *= 256;
		ret += b[i];
	}

	return ret;
}

static void *
off_clb(note_t *note, void *_arg)
{
	noteoff_t *arg = _arg;
	if (note->midipitch != arg->midipitch)
		return NULL;

	if (arg->time < note->off_time || note->note_offed < arg->offed) {
		note_t n = *note;
		n.off_time = arg->time;
		n.off_vel = arg->vel;

		erase_note(note);
		if (n.on_time != n.off_time)
			insert_note(&n)->note_offed = arg->offed;
	}
	return arg;
}

static void
off(channel_t *channel, midipitch_t midipitch, uchar vel, int offed, import_ctx_t *ctx)
{
	noteon_t *on = &ctx->on[channel->number][midipitch];

	if (on->vel != 0) {
		note_t n = {
			.track = ctx->track,
			.channel = channel,

			.on_time = on->time,
			.on_vel = on->vel,

			.off_time = ctx->time,
			.off_vel = vel,

			.pitch = on->pitch,
			.midipitch = midipitch,
		};

		if (n.on_time != n.off_time)
			insert_note(&n)->note_offed = offed;

		on->vel = 0;
	} else {
		noteoff_t off = {
			.channel = channel,
			.time = ctx->time,
			.midipitch = midipitch,
			.vel = vel,
			.offed = offed
		};

		if (channel_range(channel, ctx->time - 1, ctx->time, off_clb, &off) == NULL)
			stack_push(&ctx->offs, &off);
	}
}

static void
v_note_off(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
	off(channel, data[0], data[1], 1, ctx);
}

static void
v_note_on(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
	off(channel, data[0], DEFAULT_VELOCITY, 0, ctx);

	if (data[1] != 0) {
		noteon_t *on = &ctx->on[channel->number][data[0]];

		on->time = ctx->time;
		on->vel = data[1];
		on->pitch = ctx->pitch >= 0 ? ctx->pitch : data[0];

		ctx->pitch = -1;
		if ((1 << channel->number) & CHANMASK_DRUMS)
			ctx->drums++;
		else
			ctx->nodrums++;
	}
}

static void
v_note_aftertouch(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
}

static void
v_controller(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
	uchar ctrl = data[0];
	uchar value = data[1];

	map_set(&channel->ctrl[ctrl], ctx->time, value);
}

static void
v_program(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
	map_set(&channel->ctrl[CCTRL_PROGRAM], ctx->time, data[0]);
}

static void
v_channel_pressure(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
}

static void
v_pitch_wheel(channel_t *channel, uchar *data, import_ctx_t *ctx)
{
	//TODO: sensitivity
	int value = (data[0] + data[1] * 128) - 0x2000;

	map_set(&channel->ctrl[CCTRL_PITCHWHEEL], ctx->time, value);
}

static void
m_eot(uchar *data, int len, import_ctx_t *ctx)
{
	for (int i = 0; i < CHANNELS; i++)
		for (int j = 0; j < NOTES; j++)
			if (ctx->on[i][j].vel != 0)
				off(&ctx->file->channel[i], j, DEFAULT_VELOCITY, 0, ctx);
}

static void
m_trackname(uchar *data, int len, import_ctx_t *ctx)
{
	if (ctx->track->name[0] == '\0') {
		char *trackname = pool_alloc(&ctx->file->pool, len + 1);
		memcpy(trackname, data, len);
		trackname[len] = '\0';
		ctx->track->name = trackname;
	}
}

static void
m_vomid(uchar *data, int len, import_ctx_t *ctx)
{
	len--;
	switch (*data++) {
	case PROPR_NOTESYSTEM:
		if (notesystem_is_midistd(&ctx->track->notesystem)) {
			FILE *f = tmpfile();
			fwrite(data, 1, len, f);
			notesystem_t ns = notesystem_import_f(f);
			if (ns.pitches != NULL)
				track_set_notesystem(ctx->track, ns);
			fclose(f);
		}
		break;
	case PROPR_PITCH:
		if (len >= 2 && ctx->pitch < 0)
			ctx->pitch = read_int(data, 2);
		break;
	case PROPR_SHA:
		if (len == SHA1_SIZE && !ctx->sha_specified) {
			ctx->sha_specified = TRUE;
			memcpy(ctx->sha, data, SHA1_SIZE);
		}
		break;
	}
}

static void
m_proprietary(uchar *data, int len, import_ctx_t *ctx)
{
	size_t s = sizeof(magic_vomid);

	if (len >= s && !memcmp(data, magic_vomid, s))
		m_vomid(data + s, len - s, ctx);
}

typedef void (*voice_handler_t)(channel_t *channel, uchar *data, import_ctx_t *ctx);

typedef struct voice_info_t {
	int len;
	voice_handler_t handler;
} voice_info_t;

static voice_info_t voice_info[7] = {
	{2, v_note_off},
	{2, v_note_on},
	{2, v_note_aftertouch},
	{2, v_controller},
	{1, v_program},
	{1, v_channel_pressure},
	{2, v_pitch_wheel},
};

typedef void (*meta_handler_t)(uchar *data, int len, import_ctx_t *ctx);

static meta_handler_t meta_info[METAS] = {
	[META_TRACKNAME] = m_trackname,
	[META_EOT] = m_eot,
	[META_PROPRIETARY] = m_proprietary,
};

static void
meta(int type, uchar *data, int len, import_ctx_t *ctx)
{
	meta_handler_t specific_handler = meta_info[type];
	if (specific_handler != NULL) {
		specific_handler(data, len, ctx);
		return;
	}

	if (fctrl_info[type].read == NULL) {
		/* not supported */
		return;
	}

	int value = fctrl_info[type].read(data, len);
	map_set(&ctx->file->ctrl[type], ctx->time, value);
}

/* TODO: error handling */
static void
import_track(file_t *file, uchar *chunk, int len, import_ctx_t *ctx)
{
	track_t *track = track_create(file, 0);
	if (track == NULL)
		return;
	file->track[file->tracks++] = track;

	ctx->track = track;
	ctx->time = 0;
	memset(ctx->on, 0, sizeof(ctx->on));
	ctx->pitch = -1;
	ctx->drums = ctx->nodrums = 0;
	ctx->sha_specified = FALSE;

	uchar *end = chunk + len;
	uchar runst = 0;
	while (chunk < end) {
		/* time */
		time_t dt = read_varlen(&chunk, end);
		if (dt < 0 || ctx->time + dt < ctx->time || chunk >= end)
			break;
		ctx->time += dt;

		/* status */
		uchar st = *chunk;
		if (st >= 0x80)
			chunk++;
		else if (runst >= 0x80)
			st = runst;
		else
			break;

		/* event */
		int len;
		if (st == 0xF0 || st == 0xF7) {
			runst = 0;
			len = read_varlen(&chunk, end);
			if (len < 0)
				break;
		} else if (st == 0xFF) {
			if (chunk >= end)
				break;

			uchar type = *chunk++;
			if (type >= 0x80)
				break;

			len = read_varlen(&chunk, end);
			if (end - chunk < len)
				break;

			meta(type, chunk, len, ctx);
		} else if (st >= 0x80 && st < 0xF0) {
			runst = st;
			int idx = (st - 0x80) / 0x10;
			int channel = st & 0xF;

			len = voice_info[idx].len;
			for (int i = 0; i < len; i++)
				if ((uchar)chunk[0] >= 0x80)
					goto eot;

			voice_handler_t handler = voice_info[idx].handler;

			if (handler != NULL)
				handler(&file->channel[channel], chunk, ctx);
		} else {
			/* should not get here */
			break;
		}
		chunk += len;
	}
eot:
	m_eot(NULL, 0, ctx);
	if (ctx->drums > 0)
		track->chanmask = CHANMASK_DRUMS;
	else
		track->chanmask = CHANMASK_NODRUMS;
}

static int
read_chunk(FILE *f, const uchar magic[4], uchar **_chunk, int *_len, SHA_CTX *sha_ctx, vmd_bool_t last)
{
	uchar header[8], *chunk;
	int len;

start:
	if (fread(header, 1, sizeof(header), f) != sizeof(header))
		return ERROR;
	SHA1_Update(sha_ctx, header, sizeof(header));

	len = read_int(header + 4, 4);
	if (memcmp(header, magic, 4) != 0) {
		if (fseek(f, len, SEEK_CUR) != 0)
			return ERROR;
		goto start;
	}

	chunk = malloc(len);
	if (chunk == NULL)
		return ERROR;

	if (fread(chunk, 1, len, f) != len) {
		free(chunk);
		return ERROR;
	}

	*_chunk = chunk;
	*_len = len;
	if (last) {
		len -= ZERO_DTIME + META_HEADER_SIZE + PROPR_HEADER_SIZE + SHA1_SIZE; /* sha */
		len -= ZERO_DTIME + META_HEADER_SIZE;                                 /* eot */
	}
	if (len >= 0)
		SHA1_Update(sha_ctx, chunk, len);
	return OK;
}

static bool_t
check_sha(import_ctx_t *ctx)
{
	if (!ctx->sha_specified)
		return FALSE;

	uchar sha[SHA1_SIZE];
	SHA1_Final(sha, &ctx->sha_ctx);
	return !memcmp(ctx->sha, sha, sizeof(sha));
}

static void
reset_marks(file_t *file)
{
	for (int i = 0; i < file->tracks; i++)
		BST_FOREACH(bst_node_t *j, &file->track[i]->notes)
			track_note(j)->mark = 0;
}

status_t
file_import_f(file_t *file, FILE *f, vmd_bool_t *_sha_ok)
{
	uchar *chunk;
	int len;

	import_ctx_t ctx = {
		.file = file,
	};
	SHA1_Init(&ctx.sha_ctx);

	if (read_chunk(f, magic_mthd, &chunk, &len, &ctx.sha_ctx, FALSE) != OK)
		return ERROR;

	if(len < 6) {
		free(chunk);
		return ERROR;
	}

	int format = read_int(chunk, 2);
	int tracks = read_int(chunk + 2, 2);
	int division = read_int(chunk + 4, 2);
	free(chunk);

	//TODO: format 0
	if (format != 1)
		return ERROR;

	//TODO: negative division
	if (division < 0)
		return ERROR;

	file_init(file);
	file->division = division;

	stack_init(&ctx.offs, sizeof(noteoff_t));

	for (int i = 0; i < tracks; i++) {
		if (read_chunk(f, magic_mtrk, &chunk, &len, &ctx.sha_ctx, i == tracks - 1) != OK) {
			free(chunk);
			break;
		}

		import_track(file, chunk, len, &ctx);
		free(chunk);

		if (i == 0 && bst_empty(&file->track[0]->notes)) {
			track_destroy(file->track[0]);
			file->tracks = 0;
			file->tracks_list = NULL;
		}
	}
	char unused;
	bool_t trailing_stuff = fread(&unused, 1, 1, f) == 1;

	noteoff_t *off;
	while ((off = stack_pop(&ctx.offs)) != NULL)
		channel_range(off->channel, off->time - 1, off->time, off_clb, off);

	stack_fini(&ctx.offs);
	if (_sha_ok != NULL)
		*_sha_ok = !trailing_stuff && check_sha(&ctx);
	file->force_compatible = file_is_compatible(file);
	reset_marks(file);
	return file->tracks ? OK : ERROR;
}
