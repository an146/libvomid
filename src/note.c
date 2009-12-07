/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <assert.h>
#include <memory.h>
#include "vomid_local.h"

note_t *
insert_note(const note_t *note)
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
erase_note(note_t *note)
{
	note_set_channel(note, NULL);
	bst_erase(&note->track->notes, bst_node(note));
}

int
erase_notes(note_t *note)
{
	int ret = 0;
	for (; note != NULL; note = note->next) {
		erase_note(note);
		ret++;
	}
	return ret;
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
isolate_note(note_t *note)
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
	note_t n1;
	int pw;
	memcpy(&n1, note, sizeof(note_t));
	n1.pitch = pitch;
	pitch_info(&note->track->notesystem, pitch, &n1.midipitch, &pw);

	isolate_note(note);
	bst_change(&note->track->notes, bst_node(note), &n1);
	map_set_range(&note->channel->ctrl[CCTRL_PITCHWHEEL], note->on_time, note->off_time, pw);
}
