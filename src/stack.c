/* (C)opyright 2008 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdlib.h> /* NULL */
#include <memory.h> /* memcpy */
#include "vomid_local.h"

#define BLOCK_SIZE 1024

typedef struct stack_block_t {
	struct stack_block_t *next;
	int used;
	char data[];
} stack_block_t;

void
stack_init(stack_t *stack, size_t dsize)
{
	stack->head = NULL;
	stack->dsize = dsize;
}

void
stack_fini(stack_t *set)
{
	while (set->head != NULL) {
		stack_block_t *next = set->head->next;

		free(set->head);
		set->head = next;
	}
}

void *
stack_push(stack_t *set, const void *data)
{
	if (set->head == NULL || set->head->used == BLOCK_SIZE) {
		stack_block_t *head = malloc(sizeof(stack_block_t) + set->dsize * BLOCK_SIZE);

		head->next = set->head;
		head->used = 0;

		set->head = head;
	}

	void *ret = set->head->data + (set->head->used++) * set->dsize;
	if (data)
		memcpy(ret, data, set->dsize);
	return ret;
}

void *
stack_pop(stack_t *set)
{
	if (set->head == NULL)
		return NULL;

	if (set->head->used == 0) {
		stack_block_t *next = set->head->next;

		free(set->head);
		set->head = next;
	}

	if (set->head == NULL)
		return NULL;
	return set->head->data + (--set->head->used) * set->dsize;
}
