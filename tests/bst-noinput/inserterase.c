#include "vomid_test.h"

static int
int_cmp(const void *_a, const void *_b)
{
	int a = *(int *)_a;
	int b = *(int *)_b;

	return a - b;
}

void
test_inserterase()
{
	int i = 0;
	bst_t *tree = malloc(sizeof(*tree));
	bst_init(tree, sizeof(int), sizeof(int), int_cmp, NULL);
	ASSERT(tree != NULL);

	bst_rev_t *initial_commit = bst_commit(tree);
	bst_node_t *node = bst_insert(tree, &i);
	i++;
	bst_change(tree, node, &i);
	bst_erase(tree, node);
	ASSERT(tree->erased == NULL);
	ASSERT(tree->free == node);
	ASSERT(bst_commit(tree) == initial_commit);

	node = bst_insert(tree, &i);
	bst_rev_t *c1 = bst_commit(tree);
	i++;
	bst_change(tree, node, &i);
	bst_rev_t *c2 = bst_commit(tree);
	bst_update(tree, c1);
	ASSERT_EQ_INT(*(int *)node->data, 1);
	bst_update(tree, c2);
	ASSERT_EQ_INT(*(int *)node->data, 2);
	bst_fini(tree);
	free(tree);
}
