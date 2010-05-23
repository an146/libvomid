#include "common.h"

void
test_revert()
{
	bst_commit(tree);
	int s1 = bst_size(tree);

	change_tree(tree);
	int s2 = bst_size(tree);

	verify_tree(tree);
	int ds = size_change(bst_revert(tree));
	verify_tree(tree);
	ASSERT_EQ_INT(ds, s1 - s2);

	bst_clear(tree);
	ds = size_change(bst_revert(tree));
	ASSERT_EQ_INT(ds, s1);

	qsort(idata, idatalen, sizeof(int), int_cmp);
	assert_eq(bst_begin(tree), bst_end(tree), idata, idata + idatalen);
}
