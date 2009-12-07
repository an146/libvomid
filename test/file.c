/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include "vomid_test.h"

file_t file;

#define MUTED 28
#define DISTORTION 30

TEST_SETUP("file")
{
	file_init(&file);
}

TEST("file")
{
	int i, j;
	note_t *note;

	const chanmask_t cm[] = {
		CHANMASK_NODRUMS,
		CHANMASK_NODRUMS,
		CHANMASK_DRUMS,
	};
	for (i = 0; i < 3; i++) {
		char buf[16];
		sprintf(buf, "Track %i", i + 1);

		file.track[i] = track_create(&file, cm[i]);
		file.track[i]->name = file_copy_string(&file, buf);
		file.tracks++;
	}
	track_set_notesystem(file.track[0], notesystem_tet(17));

	map_set(&file.ctrl[FCTRL_TIMESIG], 0, TIMESIG(3, 4));
	map_set(&file.ctrl[FCTRL_TEMPO], 0, TEMPO_BPM(240));

#define prog(note, p) \
	map_set_range(&note->channel->ctrl[CCTRL_PROGRAM], note->on_time, note->off_time, p);

	for (i = 0; i < 18; i++) {
		note = track_insert(file.track[0], i * 240, (i + 1) * 240, 51 + i);
		int p = i % 3 == 2 ? DISTORTION : MUTED;
		prog(note, p);
	}
	note_t *xnote = track_insert(file.track[0], 0, 240, 51 + 17);
	prog(xnote, MUTED);
	file_flatten(&file);
	ASSERT_EQ_INT(file.track[0]->channel_usage[0], 19);

	note = track_insert(file.track[1], 19 * 240, 20 * 240, 48);
	prog(note, DISTORTION);
	file_flatten(&file);
	ASSERT(note->channel == &file.channel[1]);

	note = track_insert(file.track[2], 0, 240, 81); /* open triangle */
	file_flatten(&file);
	ASSERT(note->channel == &file.channel[9]);

	file_rev_t *rev = file_commit(&file);
	ASSERT(rev != NULL);

	/* screwing up everything... */
	for (i = 0; i < file.tracks; i++) {
		while (!bst_empty(&file.track[i]->notes))
			erase_note(track_note(bst_root(&file.track[i]->notes)));
		file.track[i]->name = "";
	}
	for (i = 0; i < FCTRLS; i++)
		map_set_range(&file.ctrl[i], 0, 10000, 123);
	for (i = 0; i < CHANNELS; i++)
		for (j = 0; j < FCTRLS; j++)
			map_set_range(&file.channel[i].ctrl[j], 0, 10000, 246);

	for (i = 0; i < CHANNELS + 1; i++)
		note = track_insert(file.track[1], 19 * 240, 20 * 240, 48);
	prog(note, DISTORTION);
	ASSERT(file_commit(&file) == NULL);

	file_update(&file, rev);
	for (channel_t *j = file.track[1]->temp_channels; j != NULL; j = j->next)
		ASSERT(bst_empty(&j->notes));

	note = track_insert(file.track[0], 2 * 240, 3 * 240, 59);
	prog(note, DISTORTION);
	file_flatten(&file);
	ASSERT(note->channel == &file.channel[2]);
	ASSERT(file.track[0]->channel_usage[0] == 19);
	ASSERT(file.track[0]->channel_usage[2] == 1);

	isolate_note(xnote);
	prog(xnote, DISTORTION);
	file_flatten(&file);
	ASSERT(xnote->channel == &file.channel[2]);

	file_export_f(&file, stdout);
}

static void
measure_clb(const measure_t *m, void *arg)
{
	printf("%i: %i/%i %i-%i partsize: %i\n",
		m->number,
		TIMESIG_NUMER(m->timesig),
		TIMESIG_DENOM(m->timesig),
		m->beg,
		m->end,
		m->part_size
	);
}

TEST("file/measures")
{
	file_measures(&file, 0, 240 * 4, measure_clb, NULL);
	map_set(&file.ctrl[FCTRL_TIMESIG], 240 * 5, TIMESIG(3, 4));
	file_commit(&file);
	file_measures(&file, 240 * 4 - 1, 240 * 8, measure_clb, NULL);
	file_measures(&file, 240 * 8 + 1, 240 * 8 + 2, measure_clb, NULL);
}

TEST_TEARDOWN("file")
{
	file_fini(&file);
}
