#include "common.h"

bst_t *tree = &(bst_t){};
int idatalen;
int *idata;

void
assert_eq(bst_node_t *s1, bst_node_t *e1, int *s2, int *e2)
{
	for (; s1 != e1 && s2 != e2; s1 = bst_node_next(s1), s2++)
		ASSERT_EQ_INT(*(int *)s1->data, *s2);
	ASSERT(s1 == e1);
	ASSERT(s2 == e2);
}

int
int_cmp(const void *_a, const void *_b)
{
	int a = *(int *)_a;
	int b = *(int *)_b;

	return a - b;
}

void
updator(bst_node_t *node)
{
	elem_t *data = (elem_t *)node->data;

	data->size = 1;
	for (int i = 0; i < 2; i++) {
		data->child[i] = node->child[i];
		if (node->child[i] != NULL)
			data->size += ((elem_t *)node->child[i]->data)->size;
	}
}

/*
 * checks balancing and linking
 * returns height of subtree
 */
void
verify_subtree(bst_node_t *node, int *height, int *size)
{
	if (node == NULL) {
		*height = 0;
		*size = 0;
		return;
	}

	elem_t *data = (elem_t *)node->data;
	int h[2], s[2];
	for (int i = 0; i < 2; i++) {
		bst_node_t *c = node->child[i];

		ASSERT(c == data->child[i]);
		if (c != NULL) {
			ASSERT(c->parent == node);
			ASSERT(c->idx == i);
		}
		verify_subtree(node->child[i], &h[i], &s[i]);
	}

	ASSERT_EQ_INT(h[0] - h[1], node->balance);
	ASSERT(-1 <= node->balance && node->balance <= 1);
	*height = MAX(h[0], h[1]) + 1;
	*size = s[0] + s[1] + 1;
	ASSERT_EQ_INT(*size, data->size);
}

void
verify_tree(bst_t *tree)
{
	int h, s;
	verify_subtree(bst_root(tree), &h, &s);
	ASSERT(s == bst_size(tree));
}

void
change_tree(bst_t *tree)
{
	int i, n = idatalen / 2;

	for (i = 0; i < n; i++)
		bst_insert(tree, &idata[i]);

	for (i = n; i < idatalen; i++) {
		bst_node_t *node = bst_find(tree, &idata[i]);

		if (node != NULL) {
			if (rand() % 2)
				bst_erase(tree, node);
			else {
				int r = rand();
				bst_change(tree, node, &r);
			}
		}
	}
}

int
size_change(bst_node_t *l)
{
	int ret = 0;

	for (; l != NULL; l = l->next)
		if (l->in_tree)
			ret++;
		else
			ret--;
	return ret;
}
