#include "vomid_test.h"

file_t file;

TEST_SETUP("import")
{
	if(file_import_f(&file, stdin, NULL) != OK)
		printf("Invalid file\n");
}

static void *
clb(note_t *note, void *arg)
{
	*(int *)arg -= 1;

	return NULL;
}

TEST("import/notes")
{
	for (int i = 0; i < file.tracks; i++) {
		track_t *track = file.track[i];
		bst_t *notes = &track->notes;
		time_t min_off = MAX_TIME;
		int nnotes = 0;

		for (bst_node_t *j = bst_begin(notes); j != bst_end(notes); j = bst_next(j)) {
			note_t *note = (note_t *)j->data;

			ASSERT(note->track == track);

			printf("%i %i ", note->on_time, note->off_time);
			printf("%i %i ", note->on_vel, note->off_vel);
			printf("%i %i ", note->pitch, note->midipitch);
			printf("%i|", note->channel->number);

			if (note->off_time < min_off)
				min_off = note->off_time;

			nnotes += 1;
		}

		if (!bst_empty(notes)) {
			note_t *last = (note_t *)bst_prev(bst_end(notes))->data;

			track_range(track, min_off - 1, last->on_time + 1, clb, &nnotes);
		}

		ASSERT_EQ_INT(nnotes, 0);

		printf("\n");
	}
}

TEST("import/ctrl")
{
	int i, j;

	for (i = 0; i < FCTRLS; i++) {
		printf("%s:", fctrl_info[i].name);
		map_print(&file.ctrl[i]);
	}
	for (i = 0; i < file.tracks; i++) {
		printf("Track %i: Name:%s\n", i, file.track[i]->name);
		for (j = 0; j < TVALUES; j++) {
			printf("Track %i: %s:", i, tvalue_info[j].name);
			printf("%i\n", file.track[i]->value[j]);
		}
		for (j = 0; j < TCTRLS; j++) {
			printf("Track %i: %s:", i, tctrl_info[j].name);
			map_print(&file.track[i]->ctrl[j]);
		}
	}
	for (i = 0; i < CHANNELS; i++) {
		for (j = 0; j < CCTRLS; j++) {
			printf("Channel %i: %s:", i, cctrl_info[j].name);
			map_print(&file.channel[i].ctrl[j]);
		}
	}
}

TEST("import/export")
{
	file_export_f(&file, stdout);
}

TEST_TEARDOWN("import")
{
	file_fini(&file);
}
