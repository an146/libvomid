/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include "vomid_local.h"

static int
cmp(const void *_a, const void *_b)
{
	const map_bstdata_t *a = (const map_bstdata_t *)_a;
	const map_bstdata_t *b = (const map_bstdata_t *)_b;

	return a->time - b->time;
}

void
map_init(map_t *map, int default_value)
{
	bst_init(&map->bst, sizeof(map_bstdata_t), sizeof(map_bstdata_t), cmp, NULL);
	map->default_value = default_value;
}

void
map_fini(map_t *map)
{
	bst_fini(&map->bst);
}

int
map_get(map_t *map, time_t time, time_t *change_time)
{
	bst_node_t *node = bst_upper_bound(&map->bst, &time);

	if (node == bst_begin(&map->bst)) {
		if (change_time != NULL)
			*change_time = 0;
		return map->default_value;
	}

	node = bst_node_prev(node);
	if (change_time != NULL)
		*change_time = map_time(node);
	return map_value(node);
}

void
map_set(map_t *map, time_t time, int value)
{
	bst_node_t *ex = bst_find(&map->bst, &time);
	if (ex != NULL)
		bst_erase(&map->bst, ex);

	if (map_get(map, time, NULL) != value)
		bst_insert(&map->bst, &(map_bstdata_t){.time = time, .value = value});
}

void
map_set_range(map_t *map, time_t beg, time_t end, int value)
{
	if (beg >= end)
		return;

	int end_value = map_get(map, end, NULL);

	bst_erase_range(
		&map->bst,
		bst_upper_bound(&map->bst, &beg),
		bst_lower_bound(&map->bst, &end)
	);

	map_set(map, beg, value);
	map_set(map, end, end_value);
}

void
map_set_node(map_t *map, bst_node_t *node, int value)
{
	map_bstdata_t data = *(map_bstdata_t *)node->data;
	data.value = value;
	bst_change(&map->bst, node, &data);
}

void
map_add(map_t *map, time_t beg, time_t end, int dvalue)
{
	while (beg < end) {
		time_t next = map_time(bst_upper_bound(&map->bst, &beg));
		map_set(map, beg, map_get(map, beg, NULL) + dvalue);
		beg = next;
	}
}

bool_t
map_eq(map_t *a, map_t *b, time_t beg, time_t end)
{
	int value = map_get(a, beg, NULL);
	if (value != map_get(b, beg, NULL))
		return FALSE;

	bst_node_t *i = bst_upper_bound(&a->bst, &beg);
	bst_node_t *j = bst_upper_bound(&b->bst, &beg);
	for (; ; i = bst_next(i), j = bst_next(j)) {
		while (map_time(i) < end && map_value(i) == value)
			i = bst_next(i);
		while (map_time(j) < end && map_value(j) == value)
			j = bst_next(j);

		time_t t1 = map_time(i);
		time_t t2 = map_time(j);
		if (t1 >= end || t2 >= end)
			return t1 >= end && t2 >= end;
		if (t1 != t2 || map_value(i) != map_value(j))
			return FALSE;
	}
	return TRUE;
}

void
map_copy(map_t *map1, time_t beg1, time_t end1, map_t *map2, time_t beg2)
{
	if (beg1 >= end1)
		return;

	time_t end2 = beg2 + (end1 - beg1);
	int end_value = map_get(map2, end2, NULL);

	bst_erase_range(
		&map2->bst,
		bst_upper_bound(&map2->bst, &beg2),
		bst_lower_bound(&map2->bst, &end2)
	);

	map_set(map2, beg2, map_get(map1, beg1, NULL));
	map_set(map2, end2, end_value);

	bst_node_t *s = bst_upper_bound(&map1->bst, &beg1);
	bst_node_t *e = bst_lower_bound(&map1->bst, &end1);
	for (bst_node_t *i = s; i != e; i = bst_next(i))
		bst_insert(&map2->bst, &(map_bstdata_t){
			.time = map_time(i) + (beg2 - beg1),
			.value = map_value(i)
		});
}

time_t
map_time(bst_node_t *node)
{
        if (bst_node_is_end(node))
                return VMD_MAX_TIME;

        return ((map_bstdata_t *)node->data)->time;
}

int
map_value(bst_node_t *node)
{
        return ((map_bstdata_t *)node->data)->value;
}
