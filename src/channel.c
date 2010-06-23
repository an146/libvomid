/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdlib.h> /* malloc */
#include "vomid_local.h"

typedef struct channel_note_t {
	note_t *note;
	time_t max_off;
} channel_note_t;

static int
cmp(const void *_a, const void *_b)
{
	const channel_note_t *a = _a;
	const channel_note_t *b = _b;

	return note_cmp(a->note, b->note);
}

static note_t *
note(bst_node_t *node)
{
	if (node == NULL)
		return NULL;

	return channel_note(node);
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

	channel_note_t *c_note = (channel_note_t *)node->data;
	return c_note->max_off;
}

static void
upd(bst_node_t *node)
{
	channel_note_t *c_note = (channel_note_t *)node->data;
	c_note->max_off = MAX3(off(node), off(node->child[0]), off(node->child[1]));
}

DEFINE_RANGE_FN

void *
channel_range(channel_t *channel, time_t s, time_t e,
	note_callback_t clb, void *arg)
{
	return range(&channel->notes, s, e, clb, arg);
}

void
channel_init(channel_t *channel, int number)
{
	channel->number = number;
	bst_init(&channel->notes, sizeof(channel_note_t), sizeof(note_t *), cmp, upd);
	for (int i = 0; i < CCTRLS; i++)
		map_init(&channel->ctrl[i], cctrl_info[i].default_value);
	channel->next = NULL;
}

void
channel_fini(channel_t *channel)
{
	bst_fini(&channel->notes);
	for (int i = 0; i < CCTRLS; i++)
		map_fini(&channel->ctrl[i]);
}

channel_t *
channel_create(int number)
{
        channel_t *ret = malloc(sizeof(*ret));
        channel_init(ret, number);
        return ret;
}

void
channel_commit(channel_t *channel, channel_rev_t *rev)
{
	rev->notes = bst_commit(&channel->notes);
	for (int i = 0; i < CCTRLS; i++)
		rev->ctrl[i] = bst_commit(&channel->ctrl[i].bst);
}

void
channel_update(channel_t *channel, channel_rev_t *rev)
{
	bst_node_t *node = bst_update(&channel->notes, rev->notes);
	for (; node != NULL; node = node->next) {
		if (node->in_tree) {
			channel_note(node)->channel = channel;
			channel_note(node)->track->channel_usage[channel->number]++;
		} else {
			channel_note(node)->channel = NULL;
			channel_note(node)->track->channel_usage[channel->number]--;
		}
	}
	for (int i = 0; i < CCTRLS; i++)
		bst_update(&channel->ctrl[i].bst, rev->ctrl[i]);
}

note_t *
channel_note(bst_node_t *node)
{
        return *(note_t **)node->data;
}
