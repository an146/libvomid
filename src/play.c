/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <string.h>
#include "vomid_local.h"

#define MAX_EVENTS (FCTRLS + MAX_TRACKS * (2 * NOTES + TCTRLS) + CHANNELS * CCTRLS)
#define MAX_EVENT_LENGTH 1024

typedef struct event_t event_t;
typedef struct play_ctx_t play_ctx_t;
typedef struct ctrl_ctx_t ctrl_ctx_t;

struct event_t {
	time_t time;
	int prio;
	int track;
	int channel;
	bst_node_t *node;
	ctrl_ctx_t *cctx;
	void (*move_on)(event_t *, play_ctx_t *);
	void (*write_event)(event_t *, uchar *, int *, play_ctx_t *);
};

struct ctrl_ctx_t {
	ctrl_info_t *ctrl_info;
	bst_node_t *write_cache;
	int value;
};

struct play_ctx_t {
	file_t *file;
	tevent_clb_t tevent_clb;
	note_clb_t note_clb;
	void *arg;

	stack_t ev_pool;
	event_t *ev_heap[MAX_EVENTS];
	int events;

	ctrl_ctx_t fctrl[FCTRLS];
	//ctrl_ctx_t tctrl[MAX_TRACKS][TCTRLS];
	ctrl_ctx_t cctrl[CHANNELS][CCTRLS];

	int channel_notes[CHANNELS];
	int channel_owner[CHANNELS];
};

static void
ctrl_ctx_init(ctrl_ctx_t *ctrl_ctx, ctrl_info_t *ctrl_info)
{
	ctrl_ctx->ctrl_info = ctrl_info;
	ctrl_ctx->write_cache = NULL;
	ctrl_ctx->value = ctrl_info->default_value;
}

static int
heap_cmp(play_ctx_t *ctx, int i, int j)
{
	CMP(ctx->ev_heap[i]->time, ctx->ev_heap[j]->time);
	CMP(ctx->ev_heap[i]->prio, ctx->ev_heap[j]->prio);
	return 0;
}

static void
heap_down(play_ctx_t *ctx, int i)
{
	while (1) {
		int lesser = i;
#define CHECK(idx) \
	if (idx < ctx->events && heap_cmp(ctx, idx, lesser) < 0) \
		lesser = idx;

		CHECK(i * 2 + 1);
		CHECK(i * 2 + 2);
#undef CHECK
		if (lesser == i)
			break;
		SWAP(ctx->ev_heap[i], ctx->ev_heap[lesser], event_t *);
		i = lesser;
	}
}

static void
heap_up(play_ctx_t *ctx, int i)
{
	int parent;
	while (heap_cmp(ctx, i, parent = (i - 1) / 2) < 0) {
		SWAP(ctx->ev_heap[i], ctx->ev_heap[parent], event_t *);
		i = parent;
	}
}

static void
heap_push(play_ctx_t *ctx, const event_t *ev)
{
	int idx = ctx->events++;

	ctx->ev_heap[idx] = stack_push(&ctx->ev_pool, ev);
	heap_up(ctx, idx);
}

static void
move_on_map(event_t *ev, play_ctx_t *ctx)
{
	ev->node = bst_next(ev->node);
	ev->time = bst_node_is_end(ev->node) ? -1 : map_time(ev->node);
}

static void
move_on_note(event_t *ev, play_ctx_t *ctx)
{
	ev->node = bst_next(ev->node);
	ev->time = bst_node_is_end(ev->node) ? -1 : track_note(ev->node)->on_time;
}

static void
move_on_discard(event_t *ev, play_ctx_t *ctx)
{
	ev->time = -1;
}

static void
flush_cctrl_cache(play_ctx_t *ctx, int ch)
{
	for (int i = 0; i < CCTRLS; i++) {
		ctrl_ctx_t *cctx = &ctx->cctrl[ch][i];
		if (cctx->write_cache != NULL && cctx->value != map_value(cctx->write_cache)) {
			uchar buf[MAX_EVENT_LENGTH];
			int len;

			cctx->value = map_value(cctx->write_cache);
			cctx->ctrl_info->write(cctx->ctrl_info, ch, map_value(cctx->write_cache), buf, &len);
			ctx->tevent_clb(ctx->channel_owner[ch], buf, len, ctx->arg);
		}
		cctx->write_cache = NULL;
	}
}

static void
write_tvalues(play_ctx_t *ctx, int ch, int prev_owner)
{
	int owner = ctx->channel_owner[ch];
	for (int i = 0; i < TVALUES; i++) {
		int prev_value;
		if (prev_owner >= 0)
			prev_value = ctx->file->track[prev_owner]->value[i];
		else
			prev_value = tvalue_info[i].default_value;

		int value = ctx->file->track[owner]->value[i];
		if (value != prev_value) {
			uchar buf[MAX_EVENT_LENGTH];
			int len;

			tvalue_info[i].write(&tvalue_info[i], ch, value, buf, &len);
			ctx->tevent_clb(owner, buf, len, ctx->arg);
		}
	}
}

static void
write_ctrl(event_t *ev, uchar *buf, int *len, play_ctx_t *ctx)
{
	if (ev->channel >= 0 && ctx->channel_notes[ev->channel] == 0) {
		// cctrl without effect
		ev->cctx->write_cache = ev->node;
		*len = -1;
	}
	else {
		if (ev->channel >= 0)
			ev->track = ctx->channel_owner[ev->channel]; //cctrl

		ctrl_info_t *ci = ev->cctx->ctrl_info;
		if (ev->cctx->value != map_value(ev->node)) {
			ev->cctx->value = map_value(ev->node);
			ci->write(ci, ev->channel, map_value(ev->node), buf, len);
		}
	}
}

static void
write_noteoff(event_t *ev, uchar *buf, int *len, play_ctx_t *ctx)
{
	note_t *note = track_note(ev->node);

	midi_write_noteoff(note, buf, len);
	ctx->channel_notes[note->channel->number]--;
}

static void
write_noteon(event_t *ev, uchar *buf, int *len, play_ctx_t *ctx)
{
	note_t *note = track_note(ev->node);
	int channel = note->channel->number;
	assert(channel >= 0 && channel < CHANNELS);

	int prev_owner = ctx->channel_owner[channel];
	if (ctx->channel_notes[channel] == 0) {
		ctx->channel_owner[channel] = ev->track;
		flush_cctrl_cache(ctx, channel);
		if (prev_owner != ev->track)
			write_tvalues(ctx, channel, prev_owner);
	} else
		assert(prev_owner == ev->track);

	if (ctx->note_clb)
		ctx->note_clb(note, ctx->arg);
	midi_write_noteon(note, buf, len);
	ctx->channel_notes[channel]++;

	assert(ev->time < note->off_time); /* the operation below doesn't change ev_heap[0] */
	heap_push(ctx, &(event_t){
		.time = note->off_time,
		.prio = -1,
		.track = ev->track,
		.node = ev->node,
		.move_on = move_on_discard,
		.write_event = write_noteoff
	});
}

/*
static void
write_tctrl(event_t *ev, uchar *buf, int *len)
{
	ctrl_info_t *ctrl_info = ev->data;
	ctrl_info->write(ctrl_info, ev->channel, map_value(ev->node), buf, len);
}
*/

static void
process_event(event_t *ev, play_ctx_t *ctx)
{
	uchar buf[MAX_EVENT_LENGTH];
	int len;

	ev->write_event(ev, buf, &len, ctx);
	if (len > 0)
		ctx->tevent_clb(ev->track, buf, len, ctx->arg);
	ev->move_on(ev, ctx);
}

static void
push_map(play_ctx_t *ctx, event_t *ev, bst_t *bst, time_t time)
{
	bst_node_t *beg = bst_upper_bound(bst, &time);
	if (beg != bst_begin(bst))
		beg = bst_prev(beg);

	if (beg != bst_end(bst)) {
		ev->time = map_time(beg);
		ev->node = beg;
		ev->move_on = move_on_map;
		heap_push(ctx, ev);
	}
}

//TODO: tctrls
status_t
file_play_(file_t *file, time_t time, tevent_clb_t tevent_clb,
		dtime_clb_t dtime_clb, note_clb_t note_clb, void *arg)
{
	status_t ret = OK;
	play_ctx_t ctx = {
		.file = file,
		.tevent_clb = tevent_clb,
		.note_clb = note_clb,
		.arg = arg,
		.events = 0
	};
	int i, j;

	stack_init(&ctx.ev_pool, sizeof(event_t));
	for (i = 0; i < CHANNELS; i++)
		ctx.channel_owner[i] = -1;

	file_flatten(file);

	for (i = 0; i < FCTRLS; i++) {
		ctrl_ctx_init(&ctx.fctrl[i], &fctrl_info[i]);
		push_map(&ctx, &(event_t){
			.prio = i,
			.track = -1,
			.channel = -1,
			.cctx = &ctx.fctrl[i],
			.write_event = write_ctrl
		}, &file->ctrl[i].bst, time);
	}

	for (i = 0; i < CHANNELS; i++)
		for (j = 0; j < CCTRLS; j++) {
			ctrl_ctx_init(&ctx.cctrl[i][j], &cctrl_info[j]);
			push_map(&ctx, &(event_t){
				.prio = 0,
				.track = -1,
				.channel = i,
				.cctx = &ctx.cctrl[i][j],
				.write_event = write_ctrl
			}, &file->channel[i].ctrl[j].bst, time);
		}

	for (i = 0; i < file->tracks; i++) {
		/* notes */
		bst_node_t *beg = bst_lower_bound(&file->track[i]->notes, &(note_t){
			.on_time = time,
			.off_time = 0,
			.midipitch = 0
		});
		if (beg != bst_end(&file->track[i]->notes))
			heap_push(&ctx, &(event_t){
				.prio = 1,
				.track = i,
				.time = track_note(beg)->on_time,
				.node = beg,
				.move_on = move_on_note,
				.write_event = write_noteon
			});

		/* end-of-track */
		/*
		heap_push(&ctx, &(event_t){
			.prio = INT_MAX,
			.track = i,
			.time = track_note(beg)->on_time,
			.ptr = beg,
			.move_on = move_on_note,
			.write_event = write_noteon
		});
		*/
	}

	while (ctx.events != 0) {
		if (ctx.ev_heap[0]->time > time) {
			switch (dtime_clb(ctx.ev_heap[0]->time - time, arg)) {
			case STOP:
				ret = STOP;
				goto stop;
			}
			time = ctx.ev_heap[0]->time;
		}
		process_event(ctx.ev_heap[0], &ctx);
		if (ctx.ev_heap[0]->time < 0)
			ctx.ev_heap[0] = ctx.ev_heap[--ctx.events];
		heap_down(&ctx, 0);
	}
stop:
	stack_fini(&ctx.ev_pool);
	return ret;
}

struct file_play_args {
	file_t *file;
	time_t time;
	event_clb_t event_clb;
	delay_clb_t delay_clb;
	void *arg;
};

static void
tevent_clb(int track, unsigned char *event, size_t len, void *_args)
{
	struct file_play_args *args = _args;
	args->event_clb(event, len, args->arg);
}

static status_t
dtime_clb(time_t dtime, void *_args)
{
	struct file_play_args *args = _args;
	status_t ret = args->delay_clb(
		dtime,
		map_get(&args->file->ctrl[FCTRL_TEMPO], args->time, NULL),
		args->arg
	);
	args->time += dtime;
	return ret;
}

status_t
file_play(file_t *file, time_t time, event_clb_t event_clb, delay_clb_t delay_clb, void *arg)
{
	struct file_play_args args = {
		.file = file,
		.time = time,
		.event_clb = event_clb,
		.delay_clb = delay_clb,
		.arg = arg
	};
	return file_play_(file, time, tevent_clb, dtime_clb, NULL, &args);
}
