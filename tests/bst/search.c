#include "common.h"

void
test_search()
{
	for (int i = 0; i < idatalen; i++) {
		bst_node_t *node = bst_find(tree, &idata[i]);

		ASSERT(node != NULL);
		ASSERT_EQ_INT(*(int *)node->data, idata[i]);
		if (node != bst_begin(tree))
			ASSERT(*(int *)bst_node_prev(node)->data < idata[i]);
	}

	if (bst_root(tree)) {
		bst_node_t *begin = bst_begin(tree);
		bst_node_t *end = bst_end(tree);
		//bst_node_t *after_begin = bst_next(bst_begin(tree));
		bst_node_t *before_end = bst_prev(bst_end(tree));

		int min = *(int *)begin->data;
		int max = *(int *)before_end->data;

		min--;
		max++;

		ASSERT(bst_find(tree, &min) == NULL);
		ASSERT(bst_find(tree, &max) == NULL);
		ASSERT(bst_lower_bound(tree, &min) == begin);
		ASSERT(bst_lower_bound(tree, &max) == end);
		ASSERT(bst_upper_bound(tree, &min) == begin);
		ASSERT(bst_upper_bound(tree, &max) == end);
	}
}

