#pragma once

enum {
	HASH_OTHER = 0,
	HASH_UNSIGNED /* value in 'data' is an unsigned integer of given size (native endian) */
};

typedef struct hash hash_t;

typedef struct {
	hash_t *(*create)(apr_pool_t *pool);
	int (*process)(hash_t *self, const void *buffer, int size);
	int (*finish)(hash_t *self);
} metahash_t;

struct hash {
	const metahash_t *meta;
	void *data;
	int size;
	int datatype;
};

