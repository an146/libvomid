#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include "vomid_test.h"

typedef struct elem_t {
	int v;
	bst_node_t *child[2];
	int size;
} elem_t;

static int
int_cmp(const void *_a, const void *_b)
{
	int a = *(int *)_a;
	int b = *(int *)_b;

	return a - b;
}

static void
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

static bst_t *tree = &(bst_t){};
int idatalen;
int *idata;

/*
static void
print_subtree(bst_node_t *node, void (*print)(void *), int indent)
{
	for (int i = 0; i < indent; i++)
		printf(" ");

	if (node == NULL) {
		printf("nil\n");
	} else {
		print(node->data);
		printf(" (balance: %i) (ptr: %p) (parent: %p) (childs: %p %p)\n",
			node->balance, node, node->parent, node->child[0], node->child[1]);

		if(node->child[0] != NULL || node->child[1] != NULL) {
			print_subtree(node->child[0], print, indent + 1);
			print_subtree(node->child[1], print, indent + 1);
		}
	}
}

static void
print_tree(bst_t *tree, void (*print)(void *))
{
	printf("Tree dump:\n");
	printf("==========\n");
	print_subtree(bst_root(tree), print, 0);
}
*/

/*
 * checks balancing and linking
 * returns height of subtree
 */
static void
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

static void
verify_tree(bst_t *tree)
{
	int h, s;
	verify_subtree(bst_root(tree), &h, &s);
	ASSERT(s == bst_size(tree));
}

static void
assert_eq(bst_node_t *s1, bst_node_t *e1, int *s2, int *e2)
{
	for (; s1 != e1 && s2 != e2; s1 = bst_node_next(s1), s2++)
		ASSERT_EQ_INT(*(int *)s1->data, *s2);
	ASSERT(s1 == e1);
	ASSERT(s2 == e2);
}

static void
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

TEST_SETUP("bst")
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

TEST("bst/traversal")
{
	qsort(idata, idatalen, sizeof(int), int_cmp);
	assert_eq(bst_begin(tree), bst_end(tree), idata, idata + idatalen);
}

TEST("bst/search")
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

TEST("bst/erase")
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

static int
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

TEST("bst/revert")
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

#define REVS 15
#define UPDATES 10

TEST("bst/update")
{
	if (idatalen == 0)
		return;

	int i, j;
	struct {
		bst_rev_t *rev;
		int s;
		int e;
	} revs[REVS];

	for (i = 0; i < REVS; i++) {
		if (i > 0)
			bst_update(tree, revs[rand() % i].rev);

		revs[i].s = rand() % idatalen;
		revs[i].e = rand() % idatalen;
		if (revs[i].s > revs[i].e)
			SWAP(revs[i].s, revs[i].e, int);

		// actually, this shit has no effect
		{
			for (j = revs[i].s; j != revs[i].e; j++)
				bst_insert(tree, &idata[j]);
			bst_clear(tree);
			bst_revert(tree);
			for (j = revs[i].s; j != revs[i].e; j++)
				bst_insert(tree, &idata[j]);
			change_tree(tree);
			bst_commit(tree);
		}

		bst_clear(tree);
		for (j = revs[i].s; j != revs[i].e; j++) {
			if (rand() % 2)
				bst_insert(tree, &idata[j]);
			else {
				int x = 12345;
				bst_change(tree, bst_insert(tree, &x), &idata[j]);
			}
		}

		revs[i].rev = bst_commit(tree);
	}

	int *data = malloc(idatalen * sizeof(int));
	for (i = 0; i < UPDATES; i++) {
		int nrev = rand() % REVS;
		int s = revs[nrev].s;
		int e = revs[nrev].e;

		int size = bst_size(tree);
		int ds = size_change(bst_update(tree, revs[nrev].rev));
		verify_tree(tree);
		ASSERT(ds == bst_size(tree) - size);

		memcpy(data, idata + s, (e - s) * sizeof(int));

		qsort(data, e - s, sizeof(int), int_cmp);
		assert_eq(bst_begin(tree), bst_end(tree), data, data + (e - s));

	}
	free(data);
}

TEST_TEARDOWN("bst")
{
	verify_tree(tree);
	bst_fini(tree);
	free(idata);
}

TEST("bst-inserterase")
{
	int i = 0;
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
}
