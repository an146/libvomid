/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <memory.h>
#include "vomid_local.h"

#define TETS 256

static notesystem_t tets[TETS] = {};

static status_t
pitch_info_midistd(const notesystem_t *ns, pitch_t pitch, midipitch_t *midipitch, int *wheel)
{
	if (pitch < 0 || pitch >= NOTES)
		return ERROR;

	*midipitch = pitch;
	*wheel = 0;
	return OK;
}

static status_t
pitch_info_tet(const notesystem_t *ns, pitch_t pitch, midipitch_t *midipitch, int *wheel)
{
	int n = ns->id;
	if (pitch < 0 || pitch >= (int)(128.0f / 12 * n))
		return ERROR;

	float fmn = (float)pitch / n * 12;
	*midipitch = (int)(fmn + 0.5f);
	*wheel = (int)((fmn - *midipitch) * 0x1000);
	return OK;
}

static int
level_style_midistd(int level)
{
	if (level % 2)
		return LEVEL_NORMAL;
	else if (level % 14)
		return LEVEL_LINE;
	else
		return LEVEL_OCTAVE_LINE;
}

static int
pitch2level_midistd(pitch_t p)
{
	int octave_lines = 1 + p / 12;
	int ef_lines = (p + 7) / 12;
	return p + octave_lines + ef_lines;
}

const notesystem_t notesystem_midistd = {
	.chanmask = CHANMASK_NODRUMS,
	.pitch_info = pitch_info_midistd,
	.id = -1,
	.default_rendersystem = {
		.levels = /* pitch_level_midistd(127) + 1 */ 127 + 11 + 11 + 1,
		.level_style = level_style_midistd,
		.pitch2level = pitch2level_midistd
	}
};

const notesystem_t notesystem_drums = {
	.chanmask = CHANMASK_DRUMS,
	.pitch_info = pitch_info_midistd,
	.id = 0,
	.default_rendersystem = {
		.levels = /* pitch_level_midistd(127) + 1 */ 127 + 11 + 11 + 1,
		.level_style = level_style_midistd,
		.pitch2level = pitch2level_midistd
	}
};

const notesystem_t *
notesystem_tet(int n)
{
	if (n < 0 || n >= TETS)
		return NULL;
	else if (n == 0)
		return &notesystem_drums;

	if (tets[n].pitch_info == NULL) {
		tets[n].chanmask = CHANMASK_NODRUMS;
		tets[n].pitch_info = pitch_info_tet;
		tets[n].id = n;
	}
	return &tets[n];
}

pitch_t
rendersystem_level2pitch(const rendersystem_t *rs, int level)
{
	pitch_t beg = 0, end = NOTES;
	while (beg < end) {
		pitch_t p = (beg + end) / 2;
		int l = rs->pitch2level(p);
		if (level < l)
			end = p;
		else if (level == l)
			return p;
		else
			beg = p + 1;
	}
	return -1;
}

note_t *
note_insert(const note_t *note)
{
	bst_node_t *t_node = bst_insert(&note->track->notes, note);
	note_t *ret = (note_t *)t_node->data;
	ret->channel = note->channel;

	bst_insert(&note->channel->notes, &ret);
	if (note->channel->number >= 0)
		note->track->channel_usage[note->channel->number]++;
	return ret;
}

void
note_erase(note_t *note)
{
	note_set_channel(note, NULL);
	bst_erase(&note->track->notes, bst_node(note));
}

void
note_set_channel(note_t *note, channel_t *channel)
{
	assert(note->channel != NULL);
	bst_erase(&note->channel->notes, bst_find(&note->channel->notes, &note));

	if (note->channel->number >= 0)
		note->track->channel_usage[note->channel->number]--;
	if (channel != NULL && channel->number >= 0)
		note->track->channel_usage[channel->number]++;

	if (channel != NULL) {
		bst_insert(&channel->notes, &note);
		for (int i = 0; i < CCTRLS; i++)
			map_copy(&note->channel->ctrl[i], note->on_time, note->off_time,
					&channel->ctrl[i], note->on_time);
	}
	note->channel = channel;
}

void
note_isolate(note_t *note)
{
	channel_t *tc = track_temp_channel(note->track, note->on_time, note->off_time);
	note_set_channel(note, tc);
}

int
note_cmp(const note_t *a, const note_t *b)
{
	CMP(a->on_time, b->on_time);
	CMP(a->off_time, b->off_time);
	CMP(a->midipitch, b->midipitch);
	return 0;
}

void
note_set_pitch(note_t *note, pitch_t pitch)
{
	const notesystem_t *ns = note->track->notesystem;
	note_t n1;
	int pw;
	memcpy(&n1, note, sizeof(note_t));
	n1.pitch = pitch;
	ns->pitch_info(ns, pitch, &n1.midipitch, &pw);

	note_isolate(note);
	bst_change(&note->track->notes, bst_node(note), &n1);
	map_set_range(&note->channel->ctrl[CCTRL_PITCHWHEEL], note->on_time, note->off_time, pw);
}
