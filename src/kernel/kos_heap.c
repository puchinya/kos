
#include "kos_local.h"

extern const void *_sheap;
extern const void *_eheap;

#define HEAP_TOP_ADDR		(&_sheap)
#define HEAP_BOTTOM_ADDR	(&_eheap)

typedef struct kos_memblk_chunk_t {
	uint32_t cur_chunk_size;
	uint32_t prev_chunk_size;
	struct kos_memblk_chunk_t *prev_free_chunk;
	struct kos_memblk_chunk_t *next_free_chunk;
} __attribute__((__packed__)) kos_memblk_chunk_t;

#define HEAP_MAX_CHUNK_SIZE		((65535 >> 3))
#define HEAP_MIN_CHUNK_SIZE		16
#define HEAP_CHUNK_DATA_OFFSET	8

#define HEAP_ALLOC_FLAG			1

#define HEAP_CHUNK_TO_DATA_PTR(chunk)		((uint8_t *)(chunk) + HEAP_CHUNK_DATA_OFFSET)
#define HEAP_DATA_PTR_TO_CHUNK(data_ptr)	((kos_memblk_chunk_t *)((uint8_t *)(data_ptr) - HEAP_CHUNK_DATA_OFFSET))

#define HEAP_FIX_ALLOC_SIZE(val)			(((val) + 7) & ~7)
#define HEAP_GET_PREV_CHUNK_SIZE(chunk)	((uint32_t)(chunk)->prev_chunk_size)
#define HEAP_GET_CUR_CHUNK_SIZE(chunk)	((uint32_t)(chunk)->cur_chunk_size)
#define HEAP_SET_PREV_CHUNK_SIZE(chunk, val)	((chunk)->prev_chunk_size = val)
#define HEAP_SET_CUR_CHUNK_SIZE_FREE(chunk, val)		((chunk)->cur_chunk_size = val)
#define HEAP_SET_CUR_CHUNK_SIZE_ALLOC(chunk, val)		((chunk)->cur_chunk_size = val | HEAP_ALLOC_FLAG)
#define HEAP_CHUNK_MIN_ADDR				((uint8_t *)(HEAP_TOP_ADDR))
#define HEAP_CHUNK_MAX_ADDR				((uint8_t *)(HEAP_BOTTOM_ADDR) - HEAP_CHUNK_DATA_OFFSET)
#define HEAP_IS_VALID_CHUNK_ADDR(chunk)	((uintptr_t)HEAP_CHUNK_MIN_ADDR <= (uintptr_t)(chunk) && (uintptr_t)(chunk) <= (uintptr_t)HEAP_CHUNK_MAX_ADDR)
#define HEAP_CLR_CHUNK_ALLOC_FLAG(chunk)	((chunk)->cur_chunk_size &= ~1)
#define HEAP_SET_CHUNK_ALLOC_FLAG(chunk)	((chunk)->cur_chunk_size |= 1)
#define HEAP_IS_FREE_CHUNK(chunk)			(!((chunk)->cur_chunk_size & 1))

kos_memblk_chunk_t *g_kos_heap_free_chunk_top = NULL;

static kos_memblk_chunk_t *get_next_chunk(kos_memblk_chunk_t *chunk);
static kos_memblk_chunk_t *get_prev_chunk(kos_memblk_chunk_t *chunk);
static void add_to_free_list(kos_memblk_chunk_t *chunk);
static void unlink_from_free_list(kos_memblk_chunk_t *chunk);

kos_er_t kos_init_heap(void)
{
	uint32_t heap_size;

	if(HEAP_BOTTOM_ADDR <= HEAP_TOP_ADDR) {
		return KOS_E_NOMEM;
	}

	heap_size = (uint32_t)((uintptr_t)HEAP_BOTTOM_ADDR - (uintptr_t)HEAP_TOP_ADDR);

	if(heap_size < HEAP_MIN_CHUNK_SIZE) {
		return KOS_E_NOMEM;
	}

	kos_memblk_chunk_t *chunk = (kos_memblk_chunk_t *)HEAP_TOP_ADDR;

	HEAP_SET_CUR_CHUNK_SIZE_FREE(chunk, heap_size);
	HEAP_SET_PREV_CHUNK_SIZE(chunk, 0);
	chunk->prev_free_chunk = NULL;
	chunk->next_free_chunk = NULL;

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
	chunk->prev_free_chunk = NULL;

	if(g_kos_heap_free_chunk_top == NULL) {
		chunk->next_free_chunk = NULL;
	} else {
		g_kos_heap_free_chunk_top->prev_free_chunk = chunk;
		chunk->next_free_chunk = g_kos_heap_free_chunk_top;
	}

	g_kos_heap_free_chunk_top = chunk;
}

kos_vp_t kos_alloc_nolock(kos_size_t size)
{
	kos_memblk_chunk_t *chunk;
	uint32_t req_chunk_size;

	req_chunk_size = HEAP_FIX_ALLOC_SIZE(size + HEAP_CHUNK_DATA_OFFSET);
	if(req_chunk_size > HEAP_MAX_CHUNK_SIZE) {
		return NULL;
	}
	if(req_chunk_size < HEAP_MIN_CHUNK_SIZE) {
		req_chunk_size = HEAP_MIN_CHUNK_SIZE;
	}

	chunk = (kos_memblk_chunk_t *)g_kos_heap_free_chunk_top;

	while(chunk != NULL) {
		if((uint32_t)chunk->cur_chunk_size >= req_chunk_size) {
			uint32_t rem_size;

			/* チャンクの分割が必要かを確認 */
			rem_size = HEAP_GET_CUR_CHUNK_SIZE(chunk) - req_chunk_size;

			if(rem_size < HEAP_MIN_CHUNK_SIZE) {
				/* 分割が不要なら、フリーチャンクリストから削除して、確保済にして終わり */

				unlink_from_free_list(chunk);

				HEAP_SET_CHUNK_ALLOC_FLAG(chunk);

				return HEAP_CHUNK_TO_DATA_PTR(chunk);
			} else {
				/* 分割を行う */

				kos_memblk_chunk_t *next_chunk = get_next_chunk(chunk);

				/* 確保するチャンクは分割後の後ろ側にする */
				kos_memblk_chunk_t *alloc_chunk = (kos_memblk_chunk_t *)((uint8_t *)chunk + rem_size);

				HEAP_SET_CUR_CHUNK_SIZE_FREE(chunk, rem_size);
				HEAP_SET_CUR_CHUNK_SIZE_ALLOC(alloc_chunk, req_chunk_size);
				HEAP_SET_PREV_CHUNK_SIZE(alloc_chunk, rem_size);

				if(next_chunk != NULL) {
					HEAP_SET_PREV_CHUNK_SIZE(next_chunk, req_chunk_size);
				}

				return HEAP_CHUNK_TO_DATA_PTR(alloc_chunk);
			}
		}

		chunk = chunk->next_free_chunk;
	}

	return NULL;
}

static kos_memblk_chunk_t *get_prev_chunk(kos_memblk_chunk_t *chunk)
{
	kos_memblk_chunk_t *prev_chunk = (kos_memblk_chunk_t *)((uint8_t *)chunk + HEAP_GET_PREV_CHUNK_SIZE(chunk));
	if((uint8_t *)prev_chunk < HEAP_CHUNK_MIN_ADDR) {
		return NULL;
	}

	return prev_chunk;
}

static kos_memblk_chunk_t *get_next_chunk(kos_memblk_chunk_t *chunk)
{
	kos_memblk_chunk_t *next_chunk = (kos_memblk_chunk_t *)((uint8_t *)chunk + HEAP_GET_CUR_CHUNK_SIZE(chunk));
	if((uint8_t *)next_chunk > HEAP_CHUNK_MAX_ADDR) {
		return NULL;
	}

	return next_chunk;
}

void kos_free_nolock(kos_vp_t p)
{
	kos_memblk_chunk_t *chunk = HEAP_DATA_PTR_TO_CHUNK(p);

	if(!HEAP_IS_VALID_CHUNK_ADDR(chunk)) {
		return;
	}

	if(HEAP_IS_FREE_CHUNK(chunk)) {
		return;
	}

	/* 確保ビットを落とす */
	HEAP_CLR_CHUNK_ALLOC_FLAG(chunk);

	/* 前後のチャンクとマージできるなら、マージする */
	{
		kos_memblk_chunk_t *next_chunk = get_next_chunk(chunk);
		kos_memblk_chunk_t *prev_chunk = get_prev_chunk(chunk);

		int prev_is_free = prev_chunk != NULL && HEAP_IS_FREE_CHUNK(prev_chunk);
		int next_is_free = next_chunk != NULL && HEAP_IS_FREE_CHUNK(next_chunk);

		if(prev_is_free && next_is_free) {
			/* 前後のチャンク両方とのマージ */

			kos_memblk_chunk_t *next_next_chunk = get_next_chunk(next_chunk);

			unlink_from_free_list(next_chunk);

			uint32_t new_chunk_size = HEAP_GET_CUR_CHUNK_SIZE(prev_chunk) + HEAP_GET_CUR_CHUNK_SIZE(chunk) + HEAP_GET_CUR_CHUNK_SIZE(next_chunk);

			HEAP_SET_CUR_CHUNK_SIZE_FREE(prev_chunk, new_chunk_size);

			/* 後方にチャンクがあれば、そのチャンクの前方チャンクサイズを更新 */
			if(next_next_chunk != NULL) {
				HEAP_SET_PREV_CHUNK_SIZE(next_next_chunk, new_chunk_size);
			}
		} else if(prev_is_free) {
			/* 前方のチャンクとマージ */

			uint32_t new_chunk_size = HEAP_GET_CUR_CHUNK_SIZE(prev_chunk) + HEAP_GET_CUR_CHUNK_SIZE(chunk);
			HEAP_SET_CUR_CHUNK_SIZE_FREE(prev_chunk, new_chunk_size);

			/* 後方にチャンクがあれば、そのチャンクの前方チャンクサイズを更新 */
			if(next_chunk != NULL) {
				HEAP_SET_PREV_CHUNK_SIZE(next_chunk, new_chunk_size);
			}
		} else if(next_is_free) {
			/* 後方のチャンクとマージ */
			kos_memblk_chunk_t *next_next_chunk = get_next_chunk(next_chunk);

			unlink_from_free_list(next_chunk);

			uint32_t new_chunk_size = HEAP_GET_CUR_CHUNK_SIZE(chunk) + HEAP_GET_CUR_CHUNK_SIZE(next_chunk);
			HEAP_SET_CUR_CHUNK_SIZE_FREE(chunk, new_chunk_size);

			/* 後方にチャンクがあれば、そのチャンクの前方チャンクサイズを更新 */
			if(next_next_chunk != NULL) {
				HEAP_SET_PREV_CHUNK_SIZE(next_next_chunk, new_chunk_size);
			}

			add_to_free_list(chunk);

		} else {
			/* マージするチャンクがなければ、フリーチャンクリストに繋いで終わり */
			add_to_free_list(chunk);
		}
	}
}

kos_vp_t kos_alloc(kos_size_t size)
{
	kos_vp_t p;

	kos_bool_t sns = kos_sns_dsp();
	kos_dis_dsp();
	p = kos_alloc_nolock(size);
	if(!sns) kos_ena_dsp();

	return p;
}

void kos_free(kos_vp_t p)
{
	kos_bool_t sns = kos_sns_dsp();
	kos_dis_dsp();
	kos_free_nolock(p);
	if(!sns) kos_ena_dsp();
}
