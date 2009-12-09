/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdlib.h> /* malloc */
#include <memory.h>
#include "vomid_local.h"

static int
cmp(const void *a, const void *b)
{
	return vmd_note_cmp(a, b);
}

static note_t *
note(bst_node_t *node)
{
	if (node == NULL)
		return NULL;

	return track_note(node);
}

static time_t
on(bst_node_t *node)
{
	note_t *n = note(node);

	return n == NULL ? -1 : n->on_time;
}

static time_t
off(bst_node_t *node)
{
	note_t *n = note(node);

	return n == NULL ? -1 : n->off_time;
}

static time_t
max_off(bst_node_t *node)
{
	if (node == NULL)
		return -1;

	track_note_t *t_note = (track_note_t *)node->data;
	return t_note->max_off;
}

static void
upd(bst_node_t *node)
{
	track_note_t *t_note = (track_note_t *)node->data;

	t_note->max_off = MAX3(off(node), max_off(node->child[0]), max_off(node->child[1]));
}

DEFINE_RANGE_FN

void *
track_for_range(track_t *track, time_t s, time_t e, note_callback_t clb, void *arg)
{
	return range(&track->notes, s, e, clb, arg);
}

struct range_arg {
	note_t *list;
	pitch_t p_beg, p_end;
};

static void *
range_clb(note_t *note, void *_arg)
{
	struct range_arg *arg = _arg;
	if (arg->p_beg <= note->pitch && note->pitch < arg->p_end) {
		note->next = arg->list;
		arg->list = note;
	}
	return NULL;
}

note_t *
track_range(track_t *track, time_t s, time_t e, pitch_t p_beg, pitch_t p_end)
{
	struct range_arg arg = {
		.list = NULL,
		.p_beg = p_beg,
		.p_end = p_end
	};
	track_for_range(track, s, e, range_clb, &arg);
	return arg.list;
}

void
track_init(track_t *track, file_t *file, chanmask_t chanmask)
{
	track->file = file;
	track->next = file->tracks_list;
	file->tracks_list = track;
	bst_init(&track->notes, sizeof(track_note_t), offsetof(note_t, mark), cmp, upd);
	for (int i = 0; i < TVALUES; i++)
		track->value[i] = tvalue_info[i].default_value;
	for (int i = 0; i < TCTRLS; i++)
		map_init(&track->ctrl[i], tctrl_info[i].default_value);
	track->name = "";
	track->primary_program = -1;
	track->notesystem = notesystem_midistd();
	track->chanmask = chanmask;
	track->temp_channels = NULL;
	memset(track->channel_usage, 0, sizeof(track->channel_usage));
}

void
track_fini(track_t *track)
{
	bst_fini(&track->notes);
	for (int i = 0; i < TCTRLS; i++)
		map_fini(&track->ctrl[i]);
	for (channel_t *i = track->temp_channels, *next; i != NULL; i = next) {
		next = i->next;
		channel_destroy(i);
	}
	notesystem_fini(track->notesystem);
}

void
track_clear(track_t *track)
{
	while (!bst_empty(&track->notes))
		erase_note(track_note(bst_root(&track->notes)));
}

static void *
clb(note_t *note, void *arg)
{
	return note;
}

static bool_t
has_free_spot(channel_t *channel, time_t beg, time_t end)
{
	return channel_range(channel, beg, end, clb, NULL) == NULL;
}

channel_t *
track_temp_channel(track_t *track, time_t beg, time_t end)
{
	for (channel_t *i = track->temp_channels; i != NULL; i = i->next)
		if (has_free_spot(i, beg, end))
			return i;

	int lcn = track->temp_channels == NULL ? 0 : track->temp_channels->number;
	channel_t *ret = channel_create(lcn - 1);
	ret->next = track->temp_channels;
	track->temp_channels = ret;
	return ret;
}

note_t *
track_insert(track_t *track, time_t beg, time_t end, pitch_t pitch)
{
	note_t n = {
		.track = track,
		.channel = track_temp_channel(track, beg, end),

		.on_time = beg,
		.on_vel = DEFAULT_VELOCITY,

		.off_time = end,
		.off_vel = DEFAULT_VELOCITY,
	};

	note_t *note = insert_note(&n);
	note_reset_pitch(note, pitch);
	map_set_range(&note->channel->ctrl[CCTRL_PROGRAM], beg, end, track_get_program(track));
	return note;
}

static bool_t
compatible(note_t *n1, note_t *n2)
{
	channel_t *c1 = n1->channel, *c2 = n2->channel;
	time_t s = MAX(n1->on_time, n2->on_time);
	time_t e = MIN(n1->off_time, n2->off_time);

	if (n1->track != n2->track || n1->midipitch == n2->midipitch)
		return FALSE;
	for (int i = 0; i < CCTRLS; i++)
		if (!map_eq(&c1->ctrl[i], &c2->ctrl[i], s, e))
			return FALSE;

	return TRUE;
}

static void *
can_move_clb(note_t *note, void *_note_to_move)
{
	return compatible(note, _note_to_move) ? NULL : note;
}

static bool_t
can_move(note_t *note, channel_t *channel)
{
	return channel_range(channel, note->on_time, note->off_time, can_move_clb, note) == NULL;
}

static void
move(note_t *note, channel_t *channel)
{
	note_set_channel(note, channel);
	for (int i = 0; i < CCTRLS; i++)
		map_copy(&note->channel->ctrl[i], note->on_time, note->off_time, &channel->ctrl[i], note->on_time);
}

//TODO: optimize
static void
channel_join(channel_t *channel, channel_t *temp)
{
	bst_t *notes = &temp->notes;
	note_t *note;

	for (bst_node_t *i = bst_begin(notes), *next; i != bst_end(notes); i = next) {
		next = bst_next(i);
		note = channel_note(i);
		if (can_move(note, channel))
			move(note, channel);
	}
}

static int
chan_cmp(track_t *track, int c1, int c2)
{
	int o1 = 0, o2 = 0;
	for (int i = 0; i < track->file->tracks; i++)
		if (track->file->track[i] != track) {
			o1 += track->file->track[i]->channel_usage[c1];
			o2 += track->file->track[i]->channel_usage[c2];
		}

	CMP(o1, o2);
	CMP(-track->channel_usage[c1], -track->channel_usage[c2]);

	return 0;
}

//TODO: what about killing empty temp channels?
status_t
track_flatten(track_t *track)
{
	int channel[CHANNELS], channels = 0;
	int i;

	for (i = 0; i < CHANNELS; i++)
		if ((track->chanmask & (1 << i)) != 0)
			channel[channels++] = i;
	/* gnome sort :) */
	for (i = 0; i < channels; )
		if (i == 0 || chan_cmp(track, channel[i - 1], channel[i]) <= 0)
			i++;
		else {
			SWAP(channel[i - 1], channel[i], int);
			i--;
		}

	for (channel_t *tc = track->temp_channels; tc != NULL; tc = tc->next) {
		for (i = 0; i < channels; i++)
			channel_join(&track->file->channel[channel[i]], tc);
		if (!bst_empty(&tc->notes))
			return ERROR;
	}
	return OK;
}

//TODO: temp_channels?
void
track_commit(track_t *track, track_rev_t *rev)
{
	rev->notes = bst_commit(&track->notes);
	for (int i = 0; i < TCTRLS; i++)
		rev->ctrl[i] = bst_commit(&track->ctrl[i].bst);
	memcpy(rev->value, track->value, sizeof(rev->value));
	rev->name = track->name;
	rev->primary_program = track->primary_program;
}

void
track_update(track_t *track, track_rev_t *rev)
{
	for (channel_t *i = track->temp_channels; i != NULL; i = i->next)
		while (!bst_empty(&i->notes))
			erase_note(channel_note(bst_root(&i->notes)));

	bst_update(&track->notes, rev->notes);
	for (int i = 0; i < TCTRLS; i++)
		bst_update(&track->ctrl[i].bst, rev->ctrl[i]);
	memcpy(track->value, rev->value, sizeof(track->value));
	track->name = rev->name;
	track->primary_program = rev->primary_program;
}

int
track_get_program(track_t *track){
	if (track->primary_program < 0) {
		if (bst_empty(&track->notes))
			track->primary_program = 0;
		else {
			note_t *n = track_note(bst_root(&track->notes));
			track->primary_program = map_get(&n->channel->ctrl[CCTRL_PROGRAM], n->on_time, NULL);
		}
	}
	return track->primary_program;
}

void
track_set_program(track_t *track, int p){
	track->primary_program = p;
	bst_t *notes = &track->notes;
	for (bst_node_t *i = bst_begin(notes); i != bst_end(notes); i = bst_next(i)) {
		note_t *n = track_note(i);
		//no need to isolate them, we operate on the whole track
		map_set_range(&n->channel->ctrl[CCTRL_PROGRAM], n->on_time, n->off_time, p);
	}
}

time_t
track_length(const track_t *track)
{
	if (bst_empty(&track->notes))
		return 0;
	else
		return max_off(bst_root(&track->notes));
}

void
track_set_notesystem(track_t *track, notesystem_t ns)
{
	notesystem_fini(track->notesystem);
	track->notesystem = ns;
}
