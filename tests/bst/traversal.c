#include "common.h"

void
test_traversal()
{
	qsort(idata, idatalen, sizeof(int), int_cmp);
	assert_eq(bst_begin(tree), bst_end(tree), idata, idata + idatalen);
}
