#include "vomid_test.h"

typedef struct elem_t {
	int v;
	bst_node_t *child[2];
	int size;
} elem_t;

extern bst_t *tree;
extern int idatalen;
extern int *idata;

void assert_eq(bst_node_t *s1, bst_node_t *e1, int *s2, int *e2);
void verify_tree(bst_t *tree);
int int_cmp(const void *, const void *);
void updator(bst_node_t *node);
void change_tree(bst_t *tree);
int size_change(bst_node_t *l);
