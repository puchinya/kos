
#include "kos_local.h"

extern const void *_sheap;
extern const void *_eheap;

#define HEAP_TOP_ADDR		(&_sheap)
#define HEAP_BOTTOM_ADDR	(&_eheap)

#define ILLEGAL_FREE_CHECK_MAGIC 0xFE653245

typedef struct kos_memblk_chunk_t {
#ifdef KOS_CFG_HEAP_ILLEGAL_FREE_CHECK
	uint32_t magic;
#endif
	uint32_t size_status;
	struct kos_memblk_chunk_t *prev_free_chunk;
	struct kos_memblk_chunk_t *next_free_chunk;
} kos_memblk_chunk_t;

#ifdef KOS_CFG_HEAP_ILLEGAL_FREE_CHECK
#define FREE_CHUNK_MIN_SIZE	(16 + 4)
#define FREE_SPACE_OFFSET	8
#define FREE_SIZE_MARGIN	(FREE_SPACE_OFFSET + 4)
#else
#define FREE_CHUNK_MIN_SIZE	(12 + 4)
#define FREE_SPACE_OFFSET	4
#define FREE_SIZE_MARGIN	(FREE_SPACE_OFFSET + 4)
#endif

kos_memblk_chunk_t *g_kos_heap_free_chunk_top = NULL;

kos_er_t kos_init_heap(void)
{
	kos_vp_int_t heap_size;
	uint32_t free_size;

	if(HEAP_BOTTOM_ADDR <= HEAP_TOP_ADDR) {
		return KOS_E_NOMEM;
	}

	heap_size = (kos_vp_int_t)HEAP_BOTTOM_ADDR - (kos_vp_int_t)HEAP_TOP_ADDR;

	if(heap_size < FREE_CHUNK_MIN_SIZE) {
		return KOS_E_NOMEM;
	}

	kos_memblk_chunk_t *chunk = (kos_memblk_chunk_t *)HEAP_TOP_ADDR;
#ifdef KOS_CFG_HEAP_ILLEGAL_FREE_CHECK
	chunk->magic = ILLEGAL_FREE_CHECK_MAGIC;
#endif
	free_size = heap_size - FREE_SIZE_MARGIN;
	chunk->size_status = free_size;
	chunk->prev_free_chunk = NULL;
	chunk->next_free_chunk = NULL;
	*(uint32_t *)((uint8_t *)chunk + free_size) = free_size;

	g_kos_heap_free_chunk_top = chunk;

	return KOS_E_OK;
}

static void unlink_from_free_list(kos_memblk_chunk_t *chunk)
{
	if(chunk->next_free_chunk != NULL) {
		chunk->next_free_chunk->prev_free_chunk = chunk->prev_free_chunk;
	}

	if(chunk->prev_free_chunk != NULL) {
		chunk->prev_free_chunk->next_free_chunk = chunk->next_free_chunk;
	}

	if(g_kos_heap_free_chunk_top == chunk) {
		g_kos_heap_free_chunk_top = chunk->next_free_chunk;
	}
}

static void add_to_free_list(kos_memblk_chunk_t *chunk)
{
	chunk->next_free_chunk = NULL;
	chunk->prev_free_chunk = NULL;

	if(g_kos_heap_free_chunk_top == NULL) {
		g_kos_heap_free_chunk_top = chunk;
	} else {
		g_kos_heap_free_chunk_top->prev_free_chunk = chunk;
		chunk->next_free_chunk = g_kos_heap_free_chunk_top;
		g_kos_heap_free_chunk_top = chunk;
	}
}

kos_vp_t kos_alloc_nolock(kos_size_t size)
{
	kos_memblk_chunk_t *chunk;
	uint32_t alloc_size;

	alloc_size = (uint32_t)((size + 7) & ~7);
	chunk = (kos_memblk_chunk_t *)g_kos_heap_free_chunk_top;

	while(chunk != NULL) {
		if(chunk->size_status >= alloc_size) {
			uint32_t rem_size;

			unlink_from_free_list(chunk);

			rem_size = chunk->size_status - alloc_size;
			if(rem_size <= FREE_CHUNK_MIN_SIZE) {

				chunk->size_status |= 1;

				if(chunk == g_kos_heap_free_chunk_top) {
					if(chunk->next_free_chunk != NULL) {
						g_kos_heap_free_chunk_top = chunk->next_free_chunk;
					} else {
						g_kos_heap_free_chunk_top = NULL;
					}
				}

				return (uint8_t *)chunk + FREE_SPACE_OFFSET;
			} else {
				uint32_t next_chunk_free_size;
				kos_memblk_chunk_t *next_chunk = (kos_memblk_chunk_t *)((uint8_t *)chunk + FREE_SIZE_MARGIN + alloc_size);

				next_chunk_free_size = rem_size - FREE_SIZE_MARGIN;
#ifdef KOS_CFG_HEAP_ILLEGAL_FREE_CHECK
				next_chunk->magic = ILLEGAL_FREE_CHECK_MAGIC;
#endif
				next_chunk->size_status = next_chunk_free_size;
				*(uint32_t *)((uint8_t *)next_chunk + next_chunk_free_size) = next_chunk_free_size;

				add_to_free_list(next_chunk);

				chunk->size_status = alloc_size | 1;
				*(uint32_t *)((uint8_t *)chunk + alloc_size) = alloc_size | 1;

				return (uint8_t *)chunk + FREE_SPACE_OFFSET;
			}
		}

		chunk = chunk->next_free_chunk;
	}

	return NULL;
}

static KOS_INLINE int is_free_chunk(kos_memblk_chunk_t *chunk)
{
	if(chunk == NULL) {
		return 0;
	}
	return (chunk->size_status & 1) == 0 ? 1 : 0;
}

static kos_memblk_chunk_t *get_prev_chunk(kos_memblk_chunk_t *chunk)
{
	if(HEAP_TOP_ADDR < chunk) {
		uint32_t prev_chunk_free_size = *(uint32_t *)((uint8_t *)chunk - 4);
		return (kos_memblk_chunk_t *)((uint8_t *)chunk - prev_chunk_free_size - FREE_SIZE_MARGIN);
	}
	return NULL;
}

static kos_memblk_chunk_t *get_next_chunk(kos_memblk_chunk_t *chunk)
{
	if(chunk < HEAP_BOTTOM_ADDR) {
		uint32_t chunk_size = (chunk->size_status >> 1) << 1;
		return (kos_memblk_chunk_t *)((uint8_t *)chunk + FREE_SPACE_OFFSET + chunk_size + 4);
	}
	return NULL;
}

void kos_free_nolock(kos_vp_t p)
{
	uint32_t free_size;
	kos_memblk_chunk_t *chunk = (kos_memblk_chunk_t *)((uint8_t *)p - FREE_SPACE_OFFSET);
	if(!(HEAP_TOP_ADDR <= chunk && chunk <= HEAP_BOTTOM_ADDR - FREE_SPACE_OFFSET)) {
		return;
	}
#ifdef KOS_CFG_HEAP_ILLEGAL_FREE_CHECK
	if(chunk->magic != ILLEGAL_FREE_CHECK_MAGIC) {
		return;
	}
#endif

	/* 確保ビットを落とす */
	chunk->size_status = (chunk->size_status >> 1) << 1;

	{
		kos_memblk_chunk_t *next_chunk = get_next_chunk(chunk);
		kos_memblk_chunk_t *prev_chunk = get_prev_chunk(chunk);

		int prev_is_free = is_free_chunk(prev_chunk);
		int next_is_free = is_free_chunk(next_chunk);

		if(prev_is_free && next_is_free) {

			unlink_from_free_list(next_chunk);

			prev_chunk->size_status = prev_chunk->size_status +
					FREE_SIZE_MARGIN + chunk->size_status + FREE_SIZE_MARGIN + next_chunk->size_status;

			*(uint32_t *)((uint8_t*)prev_chunk + FREE_SPACE_OFFSET + prev_chunk->size_status) = prev_chunk->size_status;

		} else if(prev_is_free) {
			prev_chunk->size_status = prev_chunk->size_status + FREE_SIZE_MARGIN + chunk->size_status;
			*(uint32_t *)((uint8_t*)prev_chunk + FREE_SPACE_OFFSET + prev_chunk->size_status) = prev_chunk->size_status;
		} else if(next_is_free) {

			unlink_from_free_list(next_chunk);

			chunk->size_status = chunk->size_status + next_chunk->size_status + FREE_SIZE_MARGIN;
			*(uint32_t *)((uint8_t*)chunk + FREE_SPACE_OFFSET + chunk->size_status) = chunk->size_status;

			add_to_free_list(chunk);

		} else {
			add_to_free_list(chunk);
		}
	}
}
