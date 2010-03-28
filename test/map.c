#include <stdio.h>
#include "vomid_test.h"

void
map_print(map_t *map)
{
	for (bst_node_t *i = bst_begin(&map->bst); i != bst_end(&map->bst); i = bst_next(i)) {
		if (i != bst_begin(&map->bst))
			printf(" ");
		map_bstdata_t *data = (map_bstdata_t *)i->data;
		printf("%i %i", data->time, data->value);
	}
	printf("\n");
}

TEST(map, map)
{
	int def;
	int beg, end, val;
	map_t map;

	scanf("%i", &def);
	map_init(&map, def);

	while (scanf("%i %i %i", &beg, &end, &val) == 3)
		map_set_range(&map, (time_t)beg, (time_t)end, val);
	map_print(&map);

	map_fini(&map);
}

TEST(map, equality)
{
	map_t map1;
	map_init(&map1, 0);
	map_set(&map1, 1, 1);
	map_set(&map1, 10, 0);
	map_set(&map1, 7, 0);
	map_set(&map1, 3, 0);
	map_set(&map1, 2, 0);
	map_set(&map1, 1, 0);

	map_t map2;
	map_init(&map2, 0);
	map_set(&map2, 2, 1);
	map_set(&map2, 9, 0);
	map_set(&map2, 6, 0);
	map_set(&map2, 3, 0);
	map_set(&map2, 2, 0);

	ASSERT(map_eq(&map1, &map2, 0, 100));
}
