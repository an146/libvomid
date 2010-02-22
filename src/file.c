/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <memory.h> /* memcpy */
#include "vomid_local.h"

void
file_init(file_t *file)
{
	int i;

	file->tracks = 0;
	file->division = 240;
	for (i = 0; i < CHANNELS; i++)
		channel_init(&file->channel[i], i);
	for (i = 0; i < FCTRLS; i++)
		map_init(&file->ctrl[i], fctrl_info[i].default_value);
	map_init(&file->measure_index, 1);
	pool_init(&file->pool);
	file->tracks_list = NULL;
}

void
file_fini(file_t *file)
{
	int i;
	for (i = 0; i < CHANNELS; i++)
		channel_fini(&file->channel[i]);
	for (i = 0; i < FCTRLS; i++)
		map_fini(&file->ctrl[i]);
	map_fini(&file->measure_index);
	for (track_t *t = file->tracks_list, *next; t != NULL; t = next) {
		next = t->next;
		track_destroy(t);
	}
	pool_fini(&file->pool);
}

status_t
file_flatten(file_t *file)
{
	for (int i = 0; i < file->tracks; i++)
		if (track_flatten(file->track[i]) != OK)
			return ERROR;
	return OK;
}

static int
measures(file_t *file, time_t time, int timesig)
{
	time_t part_size = file->division * 4 / TIMESIG_DENOM(timesig);
	time_t measure_size = part_size * TIMESIG_NUMER(timesig);
	return (time + measure_size - 1) / measure_size;
}

static void
regen_measure_index(file_t *file)
{
	bst_clear(&file->measure_index.bst);
	map_t *ts_map = &file->ctrl[FCTRL_TIMESIG];
	int ts = ts_map->default_value;
	time_t time = 0;
	int measure = 1;
	for (bst_node_t *i = bst_begin(&ts_map->bst); i != bst_end(&ts_map->bst); i = bst_next(i)) {
		int dm = measures(file, map_time(i) - time, ts);
		ts = map_value(i);
		time = map_time(i);
		measure += dm;
		map_set(&file->measure_index, time, measure);
	}
}

file_rev_t *
file_commit(file_t *file)
{
	if (file_flatten(file) != OK)
		return NULL;

	file_rev_t *rev = pool_alloc(&file->pool, sizeof(file_rev_t));
	int i;

	rev->tracks = file->tracks;
	for (i = 0; i < file->tracks; i++) {
		rev->track[i] = file->track[i];
		track_commit(file->track[i], &rev->track_rev[i]);
	}
	for (i = 0; i < CHANNELS; i++)
		channel_commit(&file->channel[i], &rev->channel[i]);
	for (i = 0; i < FCTRLS; i++)
		rev->ctrl[i] = bst_commit(&file->ctrl[i].bst);
	regen_measure_index(file);
	return rev;
}

//TODO: dead tracks?
void
file_update(file_t *file, file_rev_t *rev)
{
	int i;

	file->tracks = rev->tracks;
	for (i = 0; i < file->tracks; i++) {
		file->track[i] = rev->track[i];
		track_update(file->track[i], &rev->track_rev[i]);
	}
	for (i = 0; i < CHANNELS; i++)
		channel_update(&file->channel[i], &rev->channel[i]);
	for (i = 0; i < FCTRLS; i++)
		bst_update(&file->ctrl[i].bst, rev->ctrl[i]);
	regen_measure_index(file);
}

status_t
file_import(file_t *file, const char *fn, bool_t *sha_ok)
{
	FILE *f = fopen(fn, "rb");
	if (f == NULL)
		return ERROR;

	status_t ret = file_import_f(file, f, sha_ok);
	fclose(f);
	regen_measure_index(file);
	return ret;
}

status_t
file_export(file_t *file, const char *fn)
{
	FILE *f = fopen(fn, "wb");
	if (f == NULL)
		return ERROR;

	status_t ret = file_export_f(file, f);
	fclose(f);
	return ret;
}

time_t
file_length(const file_t *file)
{
	time_t ret = 0;
	for (int i = 0; i < file->tracks; i++)
		ret = MAX(ret, track_length(file->track[i]));
	return ret;
}

/* TODO:
measure_t *
file_measure(file_t *file, int n)
{
}
*/

static void
copy_measure_clb(const measure_t *src, void *dest)
{
	memcpy(dest, src, sizeof(*src));
}

void
file_measure_at(file_t *file, time_t time, measure_t *measure)
{
	file_measures(file, time, time + 1, copy_measure_clb, measure);
}

static void
process_timesig(file_t *file, time_t beg, time_t end, time_t cutoff, measure_clb_t clb, void *arg)
{
	measure_t measure;
	measure.number = map_get(&file->measure_index, beg, &measure.beg);
	measure.timesig = map_get(&file->ctrl[FCTRL_TIMESIG], beg, NULL),
	measure.part_size = file->division * 4 / TIMESIG_DENOM(measure.timesig);
	int msize = measure.part_size * TIMESIG_NUMER(measure.timesig);

	int n = (beg - measure.beg) / msize;
	measure.number += n;
	measure.beg += n * msize;
	measure.end = measure.beg + msize;
	while (measure.beg < end && measure.beg < cutoff) {
		if (measure.end > cutoff)
			measure.end = cutoff;
		clb(&measure, arg);
		measure.number++;
		measure.beg += msize;
		measure.end += msize;
	}
}

void
file_measures(file_t *file, time_t beg, time_t end, measure_clb_t clb, void *arg)
{
	if (beg >= end)
		return;

	bst_node_t *i = bst_upper_bound(&file->measure_index.bst, &beg);
	while(1) {
		time_t bound = map_time(i);
		process_timesig(file, beg, end, bound, clb, arg);
		if (bound >= end)
			break;
		beg = bound;
		i = bst_next(i);
	}
}

int
track_idx(const track_t *track)
{
	for (int i = 0; i < track->file->tracks; i++)
		if (track->file->track[i] == track)
			return i;
	return -1;
}
