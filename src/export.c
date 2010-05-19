/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vomid_local.h"
#include "3rdparty/sha1/sha1.h"

#define BUF_SIZE 1024

#define NULL_TRACK 1
#define LAST_TRACK 1

#define ZERO_DTIME 1
#define SHA (META_HEADER_SIZE + PROPR_HEADER_SIZE + SHA1_SIZE)

#define MIDI_FORMAT 1

typedef struct track_export_ctx_t {
	FILE *fbuf;
	time_t dtime;
} track_export_ctx_t;

typedef struct export_ctx_t {
	file_t *file;
	track_export_ctx_t track[MAX_TRACKS + NULL_TRACK];
} export_ctx_t;

static void
write(FILE *out, const uchar *buf, size_t size, SHA_CTX *sha_ctx)
{
	fwrite(buf, 1, size, out);
	SHA1_Update(sha_ctx, buf, size);
}

static void
write_int(FILE *out, int len, int value, SHA_CTX *sha_ctx)
{
	uchar buf[len];

	for (int i = 0; i < len; i++) {
		buf[len - 1 - i] = value % 0x100;
		value /= 0x100;
	}
	write(out, buf, len, sha_ctx);
}

static void
cat(FILE *src, FILE *dst, SHA_CTX *sha_ctx)
{
	long size = ftell(src);
	char buf[BUF_SIZE];

	rewind(src);
	while (size > 0) {
		int count = size > BUF_SIZE ? BUF_SIZE : size;
		fread(buf, 1, count, src);
		fwrite(buf, 1, count, dst);
		if (sha_ctx != NULL)
			SHA1_Update(sha_ctx, buf, count);
		size -= count;
	}
}

static void
tevent_clb(int track, small_event_t *ev, void *arg)
{
	export_ctx_t *ctx = arg;
	track_export_ctx_t *tctx = &ctx->track[track + NULL_TRACK];

	midi_fwrite_varlen(tctx->fbuf, tctx->dtime);
	fwrite(ev->buf, 1, ev->len, tctx->fbuf);
	tctx->dtime = 0;
}

static status_t
dtime_clb(time_t dtime, void *arg)
{
	export_ctx_t *ctx = arg;

	for (int i = 0; i < ctx->file->tracks + NULL_TRACK; i++)
		ctx->track[i].dtime += dtime;
	return OK;
}

static void
note_clb(const note_t *note, void *arg)
{
	export_ctx_t *ctx = arg;
	track_export_ctx_t *tctx = &ctx->track[track_idx(note->track) + NULL_TRACK];
	if (!notesystem_is_midistd(&note->track->notesystem)) {
		midi_fwrite_varlen(tctx->fbuf, tctx->dtime);
		midi_fwrite_pitch(tctx->fbuf, note->pitch);
		tctx->dtime = 0;
	}
}

static void
write_tracknames(export_ctx_t *ctx)
{
	for (int i = 0; i < ctx->file->tracks; i++) {
		FILE *f = ctx->track[i + NULL_TRACK].fbuf;
		const char *tn = ctx->file->track[i]->name;
		midi_fwrite_varlen(f, 0);
		midi_fwrite_meta(f, META_TRACKNAME, (const uchar *)tn, strlen(tn));
	}
}

static void
write_notesystems(export_ctx_t *ctx)
{
	for (int i = 0; i < ctx->file->tracks; i++) {
		const notesystem_t *ns = &ctx->file->track[i]->notesystem;
		if (!notesystem_is_midistd(ns)) {
			FILE *f = ctx->track[i + NULL_TRACK].fbuf;
			midi_fwrite_varlen(f, 0);
			midi_fwrite_notesystem(f, ns);
		}
	}
}

static void
write_eot(FILE *out, SHA_CTX *sha_ctx)
{
	write_int(out, ZERO_DTIME, 0, sha_ctx);
	write(out, midi_eot, sizeof(midi_eot), sha_ctx);
}

status_t
file_export_f(file_t *file, FILE *out)
{
	export_ctx_t ctx;
	int i;

	ctx.file = file;
	for (i = 0; i < file->tracks + NULL_TRACK; i++) {
		ctx.track[i].fbuf = tmpfile();
		ctx.track[i].dtime = 0;
	}

	write_tracknames(&ctx);
	write_notesystems(&ctx);
	file_play_(file, 0, tevent_clb, dtime_clb, note_clb, &ctx);

	SHA_CTX sha_ctx;
	SHA1_Init(&sha_ctx);

	write(out, magic_mthd, sizeof(magic_mthd), &sha_ctx);
	write_int(out, 4, 2 + 2 + 2, &sha_ctx);
	write_int(out, 2, MIDI_FORMAT, &sha_ctx);
	write_int(out, 2, file->tracks + NULL_TRACK, &sha_ctx);
	write_int(out, 2, file->division, &sha_ctx);
	for (i = 0; i < file->tracks + NULL_TRACK; i++) {
		bool_t write_sha = i == file->tracks;
		size_t add_to_back = ZERO_DTIME + sizeof(midi_eot);
		if (write_sha)
			add_to_back += ZERO_DTIME + SHA;

		write(out, magic_mtrk, sizeof(magic_mtrk), &sha_ctx);
		write_int(out, 4, ftell(ctx.track[i].fbuf) + add_to_back, &sha_ctx);
		cat(ctx.track[i].fbuf, out, &sha_ctx);
		fclose(ctx.track[i].fbuf);

		if (write_sha) {
			uchar sha[SHA1_SIZE];
			SHA1_Final(sha, &sha_ctx);
			midi_fwrite_varlen(out, 0);
			midi_fwrite_propr(out, PROPR_SHA, sha, sizeof(sha));
		}
		write_eot(out, &sha_ctx);
	}
	return OK;
}
