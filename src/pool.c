/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#include <stdlib.h>
#include "vomid_local.h"

typedef struct pool_chunk_t pool_chunk_t;

struct pool_chunk_t {
	size_t size;
	size_t free;
	pool_chunk_t *next;
	char data[];
};

static pool_chunk_t *
alloc_chunk(size_t s)
{
	pool_chunk_t *chunk = malloc(sizeof(pool_chunk_t) + s);
	chunk->size = s;
	chunk->free = s;
	return chunk;
}

void
pool_init(pool_t *pool)
{
	pool->chunk = NULL;
}

void
pool_fini(pool_t *pool)
{
	pool_chunk_t *i, *next;
	for (i = pool->chunk; i != NULL; i = next) {
		next = i->next;
		free(i);
	}
}

void *
pool_alloc(pool_t *pool, size_t s)
{
	if (pool->chunk == NULL || pool->chunk->free < s ) {
		pool_chunk_t *chunk = alloc_chunk(MAX(512, s));
		chunk->next = pool->chunk;
		pool->chunk = chunk;
	}

	void *ret = pool->chunk->data + pool->chunk->size - pool->chunk->free;
	pool->chunk->free -= s;
	return ret;
}
