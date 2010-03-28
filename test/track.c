/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include "vomid_test.h"

file_t file;

TEST_SETUP(track)
{
	file_import_f(&file, stdin, NULL);
}

#define ITERATIONS 5

static void *
note_clb(note_t *note, void *count)
{
	--*(int *)count;
	return NULL;
}

static int
verify_subtree(bst_node_t *node)
{
	if (node == NULL)
		return -1;

	note_t *note = track_note(node);

	int m1 = verify_subtree(node->child[0]);
	int m2 = verify_subtree(node->child[1]);

	int m = MAX3(note->off_time, m1, m2);
	ASSERT_EQ_INT(((track_note_t *)node->data)->max_off, m);
	return m;
}

static void
verify_tree(bst_t *tree)
{
	verify_subtree(bst_root(tree));
}

TEST(track, range)
{
	for (int i = 0; i < file.tracks; i++) {
		track_t *track = file.track[i];
		verify_tree(&track->notes);
		for (int j = 0; j < ITERATIONS; j++) {
			bst_node_t *k;
			int count = 0;
			int range_l = rand() % 10000, range_r = rand() % 10000;
			if (range_l > range_r)
				SWAP(range_l, range_r, int);
			for (k = bst_begin(&track->notes); k != bst_end(&track->notes); k = bst_next(k)) {
				note_t *note = track_note(k);
				if (note->on_time < range_r && note->off_time > range_l) {
					note->mark = 1;
					count++;
				} else
					note->mark = 0;
			}
			track_for_range(track, range_l, range_r, note_clb, &count);
			ASSERT_EQ_INT(count, 0);
		}
		erase_notes(track_range(track, 0, track_length(track), 0, MAX_PITCH));
		ASSERT(bst_empty(&track->notes));
	}
}

TEST_TEARDOWN(track)
{
	file_fini(&file);
}
