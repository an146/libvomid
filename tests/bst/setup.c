#include "common.h"

void
setup()
{
	bst_init(tree, sizeof(elem_t), sizeof(int), int_cmp, updator);
	ASSERT(tree != NULL);

	if (scanf("%i", &idatalen) != 1) {
		printf("No input\n");
		exit(1);
	}

	idata = malloc(idatalen * sizeof(int));
	for (int i = 0; i < idatalen; i++) {
		scanf("%i", &idata[i]);
		bst_insert(tree, &(elem_t){.v = idata[i]});
	}
	verify_tree(tree);
	ASSERT(bst_size(tree) == idatalen);
}
