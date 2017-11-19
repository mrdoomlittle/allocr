# ifndef __mdl__allocr__h
# define __mdl__allocr__h
# include <mdlint.h>
# include "print.h"
//# define __EXPEMENT
# define __DEBUG
# ifndef NULL
# define NULL ((void*)0)
# endif
# define MUTEX_INIT 0x0
# define AR_FLG_BRK (1<<7)
# define AR_FLG_MMAP (1<<6)
# define AR_FLG_FIXED (1<<5)
# define NO_BINS 128
# define BIN_MAX_LUP 4
# define BIN_LUP_DEPTH 4

# define PILE_PAGE_C 12
# define MAX_UU_PILE 2
# define UO_PILES_TO_KEEP 4
//# define TEST
# define PAGE_DIRECTORS 3
struct ar_page {
	mdl_u16_t blk_c, no_dead_blks;
	mdl_u32_t top;
};

struct ar_pile {
	void *p, *end;
	mdl_u32_t bins[NO_BINS];
	mdl_u32_t off, page_c;
	mdl_u32_t first_blk, last_blk;
	mdl_u32_t ffree, lfree;
	mdl_u32_t m_used, m_free;
	struct ar_pile *prev, *next;
	mdl_u8_t flags;
};

struct allocrd {
	mdl_uint_t no_piles;
	struct ar_pile *pile;
	mdl_u16_t no_uu_piles;
};

struct allocr {
	struct allocrd d;
};

/*
struct arena_chunk;
struct arena_blk {
	void *p;
	mdl_uint_t size;
	mdl_u8_t is_free;
	struct arena_blk *prev, *next;
	struct arena_blk *prevf, *nextf;
	struct arena_chunk *chunk;
};

struct arena_chunk {
	void *p, *end;
	mdl_uint_t full_size;
	struct arena_chunk *prev, *next;
	mdl_u32_t used, free, off;
	struct arena_blk *free_blks;
	struct arena_blk *last_blk;
};

struct arena {
	struct arena_chunk *chunks;
	struct arena *prev, *next;
	mdl_u32_t used, free;
	mdl_u16_t mutex;
};
*/

void ar_init(struct allocr*);
void ar_de_init(struct allocr*);
void* ar_grow(void*, mdl_u32_t);
void* ar_shrink(void*, mdl_u32_t);
void* ar_alloc(struct allocr*, mdl_u32_t);
void ar_free(struct allocr*, void*);
void* ar_realloc(struct allocr*, void*, mdl_u32_t);
# endif /*__mdl__allocr__h*/
