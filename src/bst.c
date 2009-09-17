/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 *
 * versioned binary search tree
 *
 * features:
 * - snapshots (in terms of revisions, committing and updating, just like in Mercurial SCM)
 *   committing/updating back doesn't change the node pointers.
 *   this is essential, because the intended usage of
 *   these snapshots is, well, snapshotting of a large piece of the program's state.
 *   so if you commit a tree, commit a tree of that tree' node pointers, do something,
 *   and update back - that's OK. Trees may end up balanced a different way, though.
 * - dynamic data in nodes (used for interval trees).
 *   just like in Cormen book and libstdc++' pbassoc.
 * bugs:
 * - ATM there's *no* way to change the node data in place.
 *   You must do erase&insert (essentially, copy on write).
 *   If you change the node data, the change will propagate on snapshots
 *   (hm, maybe somehow make a feature out of it? :)
 * - erased nodes are reused only after committing
 * implementation notes:
 * - all write operations are implemented in terms of inserting and erasing.
 *   inserted/erased notes get tracked, and at commit time, data from erased nodes
 *   get bundled and stored in rev. erased nodes go to the free nodes pool.
 *   revs are essentially diffs. when updating, the path from current revision to needed one
 *   is traversed, and revs on it are "toggled" - reversed and applied. it's slow, but
 *   I use it for undo/redo, so in most cases only 1 rev is toggled.
 *   bst/update test is horribly slow, though.
 * - more about performance when snapshots are used:
 *   there's practically no overhead to insert/erase.
 *   first commit() is free.
 *   commit() is O(inserted + erased)
 *   toggle() is O((i + e) * log(allnodes)). Just like manual inserting&erasing to needed state.
 * - memory usage:
 *   let's make a commit, I times insert(), E times erase(), and again commit:
 *   initial commit is free.
 *   second committing will alloc (I+E)*sizeof(void *) + E*tree->csize bytes
 *   updating back to initial commit will alloc I*tree->csize bytes.
 *   later updating back and forth will be free.
 *   nodes are never freed, and always reused after committing.
 *
 * most of the things above are about once committed tree. if you don't use snapshots,
 * it will perform just like a plain AVL tree implementation, without any overhead.
 */

#include <memory.h>
#include <stddef.h> /* offsetof */
#include <assert.h>
#include "vomid_local.h"

/* intrusive single-linked list of nodes */

static int
slist_size(bst_node_t *head, int in_tree)
{
	int size = 0;

	for (; head != NULL; head = head->next)
		if (head->in_tree == in_tree)
			size++;
	return size;
}

static void
slist_push(bst_node_t **list, bst_node_t *node)
{
	node->next = *list;
	*list = node;
}

/* intrusive double-linked list of nodes */

static void
dlist_insert(bst_node_t **list, bst_node_t *node)
{
	bst_node_t *head = *list;

	if (head == NULL) {
		node->child[0] = node->child[1] = node;
		*list = node;
	} else {
		node->child[0] = head;
		node->child[1] = head->child[1];
		head->child[1] = node;
		node->child[1]->child[0] = node;
	}
}

static void
dlist_erase(bst_node_t **list, bst_node_t *node)
{
	if (node->child[0] == node) {
		*list = NULL;
		return;
	}

	if (*list == node)
		*list = node->child[0];

	node->child[1]->child[0] = node->child[0];
	node->child[0]->child[1] = node->child[1];
}

static void
fix_child(bst_node_t *node, int dir)
{
	bst_node_t *child = node->child[dir];

	if (child != NULL) {
		child->parent = node;
		child->idx = dir;
	}
}

static void
set_child(bst_node_t *node, int dir, bst_node_t *child)
{
	node->child[dir] = child;
	fix_child(node, dir);
}

static void
set_root(bst_t *tree, bst_node_t *root)
{
	set_child(&tree->head, 0, root);
}

static void
swap(bst_node_t *a, bst_node_t *b)
{
	SWAP(a->parent->child[a->idx], b->parent->child[b->idx], bst_node_t *);
	SWAP(a->parent, b->parent, bst_node_t *);
	SWAP(a->child[0], b->child[0], bst_node_t *);
	SWAP(a->child[1], b->child[1], bst_node_t *);
	SWAP(a->balance, b->balance, int);
	SWAP(a->idx, b->idx, int);

	fix_child(a, 0);
	fix_child(a, 1);
	fix_child(b, 0);
	fix_child(b, 1);
}

static void
update_to_top(bst_t *tree, bst_node_t *node)
{
	if (tree->upd != NULL) {
		for (; node != &tree->head; node = node->parent)
			tree->upd(node);
	}
}

/*
 * rebalances a subtree, does *not* update parent balance
 * returns non-zero if rebalance occured *and* decreased the subtree height by 1
 */
static int
rebalance(bst_t *tree, bst_node_t *node)
{
	if (node->balance >= -1 && node->balance <= 1)
		return 0;

	int dir = node->balance > 0;
	bst_node_t *c = node->child[!dir];
	int dbl = c->balance && ((node->balance > 0) != (c->balance > 0));
	bst_node_t *r = dbl ? c->child[dir] : c;

	/* rotation doesn't change the subtree height if c->balance == 0 */
	int ret = c->balance;

	set_child(node->parent, node->idx, r);
	set_child(node, !dir, r->child[dir]);
	set_child(r, dir, node);

	if (dbl) {
		set_child(c, dir, r->child[!dir]);
		set_child(r, !dir, c);

		if (tree->upd)
			tree->upd(c);

		node->balance = 0;
		c->balance = 0;
		if (r->balance) {
			if (dir == (r->balance > 0))
				node->balance = -(r->balance);
			else
				c->balance = -(r->balance);
			r->balance = 0;
		}
	} else {
		if (c->balance)
			node->balance = c->balance = 0;
		else if (dir) {
			node->balance = 1;
			c->balance = -1;
		} else {
			node->balance = -1;
			c->balance = 1;
		}
	}

	update_to_top(tree, node);

	return ret;
}

static void
insert_node(bst_t *tree, bst_node_t *node)
{
	bst_node_t *i = &tree->head;
	int dir = 0;

	node->balance = 0;
	node->child[0] = node->child[1] = NULL;

	while (i->child[dir] != NULL) {
		i = i->child[dir];
		dir = (tree->cmp(node->data, i->data) >= 0);
	}

	set_child(i, dir, node);
	update_to_top(tree, node);

	for (; i != &tree->head; i = i->parent) {
		i->balance += (dir ? -1 : 1);
		if (i->balance == 0 || rebalance(tree, i))
			break;
		dir = i->idx;
	}

	assert(!node->in_tree);
	node->in_tree = 1;
	tree->tree_size++;
}

static bst_node_t *
erase_node(bst_t *tree, bst_node_t *node)
{
	bst_node_t *i, *j, *next = bst_node_next(node);

	if (node->child[0] && node->child[1])
		swap(node, next);

	set_child(node->parent, node->idx, node->child[0] ? node->child[0] : node->child[1]);
	update_to_top(tree, node->parent);

	int dir = node->idx;
	for (i = node->parent, j = i->parent; i != &tree->head; i = j, j = i->parent){
		i->balance += dir ? 1 : -1;
		dir = i->idx;
		if (i->balance && !rebalance(tree, i))
			break;
	}

	assert(node->in_tree);
	node->in_tree = 0;
	tree->tree_size--;
	return next;
}

/* called in bst_erase() and bst_clear() */
static void
erased_node(bst_t *tree, bst_node_t *node)
{
	if (tree->tip == NULL)
		free(node);
	else if (node->inserted)
		dlist_insert(&tree->free, node);
	else
		slist_push(&tree->erased, node);
}

static void
erase_subtree(bst_t *tree, bst_node_t *node)
{
	if (node == NULL)
		return;

	erase_subtree(tree, node->child[0]);
	erase_subtree(tree, node->child[1]);

	assert(node->in_tree);
	node->in_tree = 0;

	erased_node(tree, node);
}

static bst_rev_t *
create_rev(bst_rev_t *parent)
{
	bst_rev_t *rev = calloc(1, sizeof(bst_rev_t));
	rev->parent = parent;

	if (parent != NULL) {
		rev->brother = parent->child;
		parent->child = rev;
	}

	return rev;
}

static void
destroy_rev(bst_rev_t *rev)
{
	free(rev->inserted);
	free(rev->erased);
	free(rev->changed);
	free(rev->uninserted_data);
	free(rev->erased_data);
	free(rev->changed_data);
	free(rev);
}

static void
destroy_revs(bst_rev_t *rev)
{
	if (rev == NULL)
		return;

	destroy_revs(rev->brother);
	destroy_revs(rev->child);

	destroy_rev(rev);
}

static void
unfree_erased(bst_t *tree, bst_rev_t *rev)
{
	for (int i = 0; i < rev->erased_count; i++) {
		dlist_erase(&tree->free, rev->erased[i]);
		memcpy(rev->erased[i]->data, rev->erased_data + i * tree->csize, tree->csize);
	}
}

static void
free_erased(bst_t *tree, bst_rev_t *rev)
{
	int i;
	if (rev->erased == NULL)
		return;

	if (rev->erased_data == NULL) {
		rev->erased_data = malloc(tree->csize * rev->erased_count);
		for (i = 0; i < rev->erased_count; i++)
			memcpy(rev->erased_data + i * tree->csize, rev->erased[i]->data, tree->csize);
	}

	for (i = 0; i < rev->erased_count; i++)
		dlist_insert(&tree->free, rev->erased[i]);
}

bst_node_t *
bst_node_child_most(bst_node_t *node, int dir)
{
	while(node->child[dir] != NULL)
		node = node->child[dir];

	return node;
}

/* prev/next node */
bst_node_t *
bst_node_adj(bst_node_t *node, int dir)
{
	if (node->child[dir] != NULL)
		return bst_node_child_most(node->child[dir], !dir);

	for (; !bst_node_is_end(node); node = node->parent) {
		if (node->idx != dir)
			return node->parent;
	}

	return node;
}

void
bst_init(bst_t *tree, size_t dsize, size_t csize, bst_cmp_t cmp, bst_upd_t upd)
{
	memset(tree, 0, sizeof(*tree));

	tree->dsize = dsize;
	tree->csize = csize;
	tree->cmp = cmp;
	tree->upd = upd;
	tree->head.parent = &tree->head;
}

void
bst_fini(bst_t *tree)
{
	bst_clear(tree);

	if (tree->tip != NULL) {
		bst_commit(tree); //TODO: this is unnecessary

		bst_rev_t *head;
		for (head = tree->tip; head->parent != NULL; head = head->parent)
			;
		destroy_revs(head);
	}

	if (tree->free != NULL) {
		bst_node_t *i = tree->free;
		do {
			bst_node_t *next = i->child[1];
			free(i);
			i = next;
		} while (i != tree->free);
	}
}

void
bst_clear(bst_t *tree)
{
	erase_subtree(tree, bst_root(tree));
	set_root(tree, NULL);
	tree->tree_size = 0;
}

static bst_node_t *
create_node(bst_t *tree)
{
	size_t node_size = MAX(
		sizeof(bst_node_t),
		offsetof(bst_node_t, data[tree->dsize])
	);

	bst_node_t *ret = malloc(node_size);
	ret->in_tree = 0;
	ret->inserted = 0;
	ret->saved = 0;
	return ret;
}

static bst_node_t *
alloc_node(bst_t *tree)
{
	if (tree->free != NULL) {
		bst_node_t *node = tree->free;
		dlist_erase(&tree->free, node);
		return node;
	} else
		return create_node(tree);
}

bst_node_t *
bst_insert(bst_t *tree, const void *data)
{
	bst_node_t *node = alloc_node(tree);
	if (node == NULL)
		return NULL;

	if (tree->tip != NULL) {
		if (!node->inserted)
			slist_push(&tree->inserted, node);
		node->inserted = 1;
	}

	memcpy(node->data, data, tree->csize);
	insert_node(tree, node);

	return node;
}

bst_node_t *
bst_erase(bst_t *tree, bst_node_t *node)
{
	bst_node_t *next = erase_node(tree, node);

	erased_node(tree, node);
	return next;
}

void
bst_erase_range(bst_t *tree, bst_node_t *beg, bst_node_t *end)
{
	while (beg != end)
		beg = bst_erase(tree, beg);
}

void
bst_change(bst_t *tree, bst_node_t *node, const void *data)
{
	if (node->in_tree && !node->inserted && !node->saved && tree->tip != NULL) {
		bst_node_t *backup = alloc_node(tree);
		memcpy(backup->data, node->data, tree->csize);
		backup->parent = node;
		dlist_insert(&tree->save, backup);

		node->saved = 1;
	}
	memcpy(node->data, data, tree->csize);

	//TODO: optimize
	erase_node(tree, node);
	insert_node(tree, node);
}

/**
 * Find a node.
 * @return
 *   The first node, for which tree->cmp(node->data, data) == 0,
 *   or NULL if there's no such node.
 */
bst_node_t *
bst_find(bst_t *tree, const void *data)
{
	bst_node_t *lb = bst_lower_bound(tree, data);

	if (bst_node_is_end(lb) || tree->cmp(lb->data, data) != 0)
		return NULL;
	else
		return lb;
}

/**
 * Find a bound node.
 * @return
 *   The first node, for which tree->cmp(node->data, data) >= bound,
 *   or head if there's no such node.
 */
bst_node_t *
bst_bound(bst_t *tree, const void *data, int bound)
{
	bst_node_t *ret = bst_end(tree), *i;
	int dir;

	for (i = bst_root(tree); i != NULL; i = i->child[dir]) {
		int good = (tree->cmp(i->data, data) >= bound);

		if (good)
			ret = i;
		dir = !good;
	}
	return ret;
}

static void
ie_pack(bst_node_t *head, int in_tree, bst_node_t **pack)
{
	for (; head != NULL; head = head->next) {
		if (head->in_tree == in_tree) {
			*pack++ = head;
			if (head->saved && !in_tree)
				; //node was changed and erased; need to restore it
			else
				head->saved = 0;
		}
		head->inserted = 0;
	}
}

bst_rev_t *
bst_commit(bst_t *tree)
{
	if (tree->tip == NULL)
		return tree->tip = create_rev(NULL);

	bst_node_t **inserted = NULL, **erased = NULL;
	int inserted_count = slist_size(tree->inserted, 1);
	int erased_count = slist_size(tree->erased, 0);

	if (inserted_count)
		inserted = malloc(inserted_count * sizeof(bst_node_t *));
	if (erased_count)
		erased = malloc(erased_count * sizeof(bst_node_t *));
	ie_pack(tree->inserted, 1, inserted);
	ie_pack(tree->erased, 0, erased);

	bst_node_t **changed = NULL;
	int changed_count = 0;
	void *changed_data = NULL;
	for (bst_node_t *i = tree->save; i != NULL; i = i->child[1]) {
		assert(!i->in_tree);
		if (i->parent->saved) {
			if (i->parent->in_tree)
				changed_count++;
			else {
				memcpy(i->parent->data, i->data, tree->csize);
				i->parent->saved = 0;
			}
		}
		if (i->child[1] == tree->save)
			break;
	}
	if (changed_count) {
		changed = malloc(changed_count * sizeof(bst_node_t *));
		changed_data = malloc(changed_count * tree->csize);
		int j = 0;
		for (bst_node_t *i = tree->save; i != NULL; i = tree->save) {
			if (i->parent->saved) {
				assert(i->parent->in_tree);
				changed[j] = i->parent;
				memcpy(changed_data + j * tree->csize, i->data, tree->csize);
				i->parent->saved = 0;
				j++;
			}
			dlist_erase(&tree->save, i);
			dlist_insert(&tree->free, i);
		}
		assert(j == changed_count);
	}

	tree->inserted = tree->erased = tree->save = NULL;

	if (inserted || erased || changed) {
		tree->tip = create_rev(tree->tip);

		tree->tip->inserted = inserted;
		tree->tip->erased = erased;
		tree->tip->changed = changed;
		tree->tip->inserted_count = inserted_count;
		tree->tip->erased_count = erased_count;
		tree->tip->changed_count = changed_count;
		tree->tip->changed_data = changed_data;

		free_erased(tree, tree->tip);
	}
	return tree->tip;
}

/**
 * Revert tree to the last committed revision.
 * @return
 *   same as bst_update().
 * @see bst_update()
 */
bst_node_t *
bst_revert(bst_t *tree)
{
	bst_node_t *i, *prev = NULL, *first = NULL;

	for (i = tree->save; i != NULL; i = tree->save) {
		assert(!i->in_tree);
		memcpy(i->parent->data, i->data, tree->csize);
		i->parent->saved = 0;
		dlist_erase(&tree->save, i);
		dlist_insert(&tree->free, i);
	}
	for (i = tree->inserted; i != NULL; i = i->next) {
		if (i->in_tree) {
			erase_node(tree, i);
			dlist_insert(&tree->free, i);
			if (prev == NULL)
				first = i;
			else
				prev->next = i;
			prev = i;
		}
		assert(i->inserted);
		i->inserted = 0;
	}
	for (i = tree->erased; i != NULL; i = i->next) {
		assert(!i->inserted);
		insert_node(tree, i);
	}

	if (first == NULL)
		first = tree->erased;
	else
		prev->next = tree->erased;

	tree->erased = tree->inserted = NULL;
	return first;
}

static int
level(bst_rev_t *rev)
{
	int ret = 0;

	for (; rev->parent != NULL; rev = rev->parent)
		ret++;
	return ret;
}

static bst_rev_t **
path_down(bst_rev_t *from, bst_rev_t *to)
{
	int from_level = level(from);
	int to_level = level(to);

	while (from_level > to_level) {
		from_level--;
		from = from->parent;
	}

	{
		bst_rev_t *i = to;
		int i_level = to_level;

		while (i_level > from_level) {
			i_level--;
			i = i->parent;
		}

		while (from != i) {
			from_level--;
			from = from->parent;
			i = i->parent;
		}
	}

	bst_rev_t **path = malloc((to_level - from_level + 1) * sizeof(bst_rev_t *));

	for (int i = to_level - from_level; i >= 0; i--) {
		path[i] = to;
		to = to->parent;
	}

	return path;
}

static void
memswap(void *_a, void *_b, size_t s)
{
	char *a = _a, *b = _b;
	while (s--) {
		SWAP(*a, *b, char);
		a++;
		b++;
	}
}

/* applies/reverts a changeset */
static void
toggle(bst_t *tree, bst_rev_t *rev, bst_node_t **affected)
{
	unfree_erased(tree, rev);

#define ADD(node, was) \
	if (!node->inserted) { \
		node->was_in_tree = was; \
		node->inserted = 1; \
		slist_push(affected, node); \
	}
	int i;
	for (i = 0; i < rev->erased_count; i++) {
		ADD(rev->erased[i], 0);
		insert_node(tree, rev->erased[i]);
	}
	for (i = 0; i < rev->inserted_count; i++) {
		ADD(rev->inserted[i], 1);
		erase_node(tree, rev->inserted[i]);
	}
#undef ADD
	for (i = 0; i < rev->changed_count; i++) {
		//TODO: optimize
		erase_node(tree, rev->changed[i]);
		memswap(rev->changed[i]->data, rev->changed_data + i * tree->csize, tree->csize);
		insert_node(tree, rev->changed[i]);
	}

	SWAP(rev->erased_count, rev->inserted_count, int);
	SWAP(rev->erased, rev->inserted, void *);
	SWAP(rev->erased_data, rev->uninserted_data, void *);

	free_erased(tree, rev);
}

/* Update the tree to another revision.
 * @return
 *   slist of nodes that changed in_tree
 *   you can check in_tree of each node to determine what happened
 *   example: size_change() in test/bst.c
 */
bst_node_t *
bst_update(bst_t *tree, bst_rev_t *rev)
{
	bst_node_t *affected = bst_revert(tree), *i;
	for (i = affected; i != NULL; i = i->next) {
		i->inserted = 1;
		i->was_in_tree = !i->in_tree;
	}

	bst_rev_t **down = path_down(tree->tip, rev);
	for (bst_rev_t *i = tree->tip; i != down[0]; i = i->parent)
		toggle(tree, i, &affected);
	for (bst_rev_t **j = down + 1; j[-1] != rev; j++)
		toggle(tree, *j, &affected);
	free(down);

	bst_node_t *first = NULL, *prev = NULL;
	for (i = affected; i != NULL; i = i->next) {
		i->inserted = 0;
		if (i->was_in_tree != i->in_tree) {
			if (prev == NULL)
				first = i;
			else
				prev->next = i;
			prev = i;
		}
	}
	if (prev != NULL)
		prev->next = NULL;

	tree->tip = rev;
	return first;
}
