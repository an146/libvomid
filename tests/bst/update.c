#include <memory.h> /* memcpy */
#include "common.h"

#define REVS 15
#define UPDATES 10

void
test_update()
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
