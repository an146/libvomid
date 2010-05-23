#include "common.h"

void
test_erase()
{
	int n = idatalen / 2;

	for (int i = n; i < idatalen; i++) {
		bst_node_t *node = bst_find(tree, &idata[i]);

		ASSERT(node != NULL);
		ASSERT(node != bst_end(tree));
		bst_erase(tree, node);
	}
	qsort(idata, n, sizeof(int), int_cmp);
	assert_eq(bst_begin(tree), bst_end(tree), idata, idata + n);
}


