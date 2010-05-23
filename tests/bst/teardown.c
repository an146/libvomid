#include "common.h"

void
teardown()
{
	verify_tree(tree);
	bst_fini(tree);
	free(idata);
}

