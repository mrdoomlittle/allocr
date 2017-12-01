# ifndef NULL
#	 define NULL ((void*)0)
# endif
# include <mdlint.h>
# include "print.h"
# include <unistd.h>
# include <sys/resource.h>
# include <sys/mman.h>

# define PILE_IPC 2
# define PAGE_SHIFT 7
# define PAGE_SIZE (1<<PAGE_SHIFT)

# define get_blkd(__p)((struct ar_blkd*)((mdl_u8_t*)(__p)-sizeof(struct ar_blkd)))
# define BLK_PADDING
# define BLK_FREE_FLG (1<<7)
# define BLK_USED_FLG (1<<6)
# define BLK_MMAPED_FLG (1<<5)
# define is_flag(__flags, __flag) ((__flags&__flag) == __flag)

# define is_free(__blk) is_flag(__blk->flags, BLK_FREE_FLG)
# define is_used(__blk) is_flag(__blk->flags, BLK_USED_FLG)
# define BNO(__bc) \
	(((mdl_u32_t)(__bc))>>7 >= (1<<7)? ((1<<7)+((((mdl_u32_t)(__bc))-(1<<7))>>31)):(((mdl_u32_t)(__bc))>>7))
# define NO_BINS 128
# define is_set(__v)(__v&0x1)
# define BIN(__pile, __bc) (__pile->bins+BNO(__bc))

# define __DEBUG
typedef mdl_u32_t ar_off_t;
typedef mdl_u32_t ar_size_t;

struct ar_blkd {
	ar_off_t prev, next, fd, bk;
	ar_size_t size;
	ar_off_t off;
	mdl_u8_t flags;
} __attribute__((packed));

struct pile_info {
	mdl_u32_t m_used, m_free;
} __attribute__((packed));

struct ar_pile {
	void *p, *end;
	struct ar_pile *prev, *next;
	ar_off_t off, bins[NO_BINS];
	mdl_uint_t page_c;
	ar_off_t blk, end_blk;
	ar_off_t free;
	mdl_uint_t blk_c;
	struct pile_info info;
} __attribute__((packed));

struct allocr {
	mdl_uint_t no_piles;
	struct ar_pile *pile;
};

static struct allocr ar;

# define ar_set_pile_info_m_used(__pile, __val) \
	__pile->info.m_used = __val;\
	__pile->info.m_free = (__pile->page_c*PAGE_SIZE)-__pile->info.m_used;

# define ar_set_pile_info_m_free(__pile, __val) \
	__pile->info.m_free = __val;\
	__pile->info.m_used = (__pile->page_c*PAGE_SIZE)-__pile->info.m_free;

void ar_prepare() {
	ar = (struct allocr) {
		.no_piles = 0, .pile = NULL
	};
}

void ar_cleanup() {

}

struct ar_pile* suitable_pile(struct allocr *__ar, mdl_uint_t __bc) {
	struct ar_pile *pile = __ar->pile;
	while(pile != NULL) {
		if (pile->info.m_free-(pile->blk_c*sizeof(struct ar_blkd)) >= (__bc+sizeof(struct ar_blkd)) || (pile->page_c*PAGE_SIZE)-pile->off >= (__bc+sizeof(struct ar_blkd))) return pile;
		pile = pile->next;
	}
	return NULL;
}

void static attach_pile(struct allocr *__ar, struct ar_pile *__pile) {
	if (__ar->pile == NULL)
		__ar->pile = __pile;
	else {
		__ar->pile->prev = __pile;
		__pile->next = __ar->pile;
		__ar->pile = __pile;
	}
}

void static detach_pile(struct allocr *__ar, struct ar_pile *__pile) {
	if (__pile == __ar->pile)
		__ar->pile = __pile->next;
	else {
		__pile->next->prev = __pile->prev;
		__pile->prev->next = __pile->next;
	}
}

void static blk_rechain(struct ar_pile *__pile, struct ar_blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off < (__pile->blk>>1) || !is_set(__pile->blk)) __pile->blk = (off<<1)|1;
	if (off > (__pile->end_blk>>1) || !is_set(__pile->end_blk)) __pile->end_blk = (off<<1)|1;
	if (is_set(__blk->prev))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->prev>>1)))->next = (off<<1)|1;
	if (is_set(__blk->next))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->next>>1)))->prev = (off<<1)|1;
}

void static blk_unchain(struct ar_pile *__pile, struct ar_blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off == __pile->blk>>1) __pile->blk = __blk->next;
	if (off == __pile->end_blk>>1) __pile->end_blk = __blk->prev;
	if (is_set(__blk->prev))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->prev>>1)))->next = __blk->next;
	if (is_set(__blk->next))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->next>>1)))->prev = __blk->prev;
}

void dechain_free(struct ar_pile *__pile, struct ar_blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off == __pile->free>>1) __pile->free = __blk->fd;
	mdl_u32_t *bin = BIN(__pile, __blk->size);
	if ((*bin)>>1 == off) *bin = __blk->fd;

	if (is_set(__blk->fd))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->fd>>1)))->bk = __blk->bk;
	if (is_set(__blk->bk))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->bk>>1)))->fd = __blk->fd;
	__blk->fd = 0;
	__blk->bk = 0;
}

void pr() {
	struct ar_pile *pile = ar.pile;
	struct ar_blkd *blk;
	mdl_uint_t pile_no = 0, blk_no;
	while(pile != NULL) {
		blk_no = 0;
		print("-> pile no. %u\n", pile_no++);
		if (is_set(pile->blk)) {
			blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(pile->blk>>1));
			_next_blk:
			print(" blk; state: %s, is_prev: %s, is_next: %s, size: %u\n", is_used(blk)?"used":"free", is_set(blk->prev)?"yes":"no", is_set(blk->next)?"yes":"no", blk->size);
			if (is_set(blk->next)) {
				blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
				goto _next_blk;
			}
		}
		print("/\n");
		pile = pile->next;
	}
}

void fr() {

}

void _ar_free(struct allocr*, void*);
void* _ar_alloc(struct allocr *__ar, mdl_uint_t __bc) {
	if ((__bc+sizeof(struct ar_blkd)) > PILE_IPC*PAGE_SIZE) {
		void *p;
		if ((p = mmap(NULL, sizeof(struct ar_blkd)+__bc, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == (void*)-1) {
			print("failed to map memory region for oversized block.\n");
			return NULL;
		}

		*(struct ar_blkd*)p = (struct ar_blkd) {
			.prev = 0, .next = 0,
			.size = __bc, .off = 0,
			.flags = BLK_MMAPED_FLG
		};

		return (void*)((mdl_u8_t*)p+sizeof(struct ar_blkd));
	}

	struct ar_pile *pile = suitable_pile(__ar, __bc);
	if (pile == NULL) {
# ifdef __DEBUG
		print("new pile.\n");
# endif
		mdl_uint_t page_c = PILE_IPC;
		void *p = sbrk(0), *end;
		brk(end = (p+(sizeof(struct ar_pile)+(page_c*PAGE_SIZE))));
		*(pile = (struct ar_pile*)p) = (struct ar_pile){
			.p = (mdl_u8_t*)p+sizeof(struct ar_pile), .end = end,
			.prev = NULL, .next = NULL,
			.off = 0, .page_c = page_c,
			.free = 0, .blk = 0, .end_blk = 0, .blk_c = 0,
			.info = {.m_used = 0, .m_free = page_c*PAGE_SIZE}
		};
		attach_pile(__ar, pile);
	}

	if (is_set(pile->free)) {
		struct ar_blkd *blk;
		ar_off_t f_off = 0;
		mdl_u32_t *bin = BIN(pile, __bc);
		mdl_u32_t *end = bin+4;
		while(bin != end && bin != bin+NO_BINS) {
			if (is_set(*bin)) {
				f_off = *bin>>1;
				blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+f_off);
				if (blk->size >= __bc) goto _found;
				else {
					while(is_set(blk->fd)) {
						if (bin+1 != bin+NO_BINS)
							if (*(bin+1) == blk->fd) goto _nope;

						f_off = blk->fd>>1;
						blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->fd>>1));
						if (blk->size >= __bc) goto _found;
					}
				}
			}
			bin++;
		}

		goto _nope;
		_found:
		ar_set_pile_info_m_used(pile, pile->info.m_used+blk->size);
		dechain_free(pile, blk);

		if (is_flag(blk->flags, BLK_FREE_FLG))
			blk->flags ^= BLK_FREE_FLG;
		blk->flags |= BLK_USED_FLG;
		blk_rechain(pile, blk);

		mdl_u32_t spare;
		if (blk->size > __bc && (spare = (blk->size-__bc)) > sizeof(struct ar_blkd)) {
			void *p = (mdl_u8_t*)blk+(sizeof(struct ar_blkd)+__bc);
			struct ar_blkd *s = (struct ar_blkd*)p;
			mdl_u32_t s_off;
			*s = (struct ar_blkd) {
				.prev = (((mdl_u8_t*)blk-(mdl_u8_t*)pile->p)<<1)|1, .next = blk->next, .fd = 0, .bk = 0,
				.size = spare-sizeof(struct ar_blkd), .off = (s_off = ((mdl_u8_t*)p-(mdl_u8_t*)pile->p))+sizeof(struct ar_pile),
				.flags = 0x0
			};
			blk->size = __bc;
			blk->next = (s_off<<1)|1;
			if (is_set(s->next)) ((struct ar_blkd*)((mdl_u8_t*)pile->p+(s->next>>1)))->prev = (s_off<<1)|1;
			_ar_free(__ar, (void*)((mdl_u8_t*)s+sizeof(struct ar_blkd)));
			pile->blk_c++;
		}
		return (void*)((mdl_u8_t*)blk+sizeof(struct ar_blkd));
	}

	_nope:

	{
	void *p = (void*)((mdl_u8_t*)pile->p+pile->off);

	struct ar_blkd *blk = (struct ar_blkd*)p;
	mdl_u32_t off = (mdl_u8_t*)blk-(mdl_u8_t*)pile->p;
	*blk = (struct ar_blkd) {
		.prev = 0, .next = 0, .fd = 0, .bk = 0,
		.size = __bc, .off = off+sizeof(struct ar_pile),
		.flags = BLK_USED_FLG
	};

	if (!is_set(pile->blk))
		pile->blk = (off<<1)|1;

	if (is_set(pile->end_blk)) {
		blk->prev = pile->end_blk;
		((struct ar_blkd*)((mdl_u8_t*)pile->p+(pile->end_blk>>1)))->next = (off<<1)|1;
	}

	pile->end_blk = (off<<1)|1;
	pile->off+= sizeof(struct ar_blkd)+__bc;
	ar_set_pile_info_m_used(pile, pile->info.m_used+__bc);
	pile->blk_c++;

//	print("%u - %u - %u\n", pile->off, pile->info.m_free, pile->page_c*PAGE_SIZE);
	return (void*)((mdl_u8_t*)p+sizeof(struct ar_blkd));
	}
}

void* ar_alloc(mdl_uint_t __bc) {
# ifdef __DEBUG
	print("alloc: %u-bytes\n", __bc);
# endif
	return _ar_alloc(&ar, __bc);
}

void _ar_free(struct allocr *__ar, void *__p) {
	struct ar_blkd *blk = get_blkd(__p), *prev, *next;
	if (is_flag(blk->flags, BLK_MMAPED_FLG)) {
		if (munmap((void*)blk, sizeof(struct ar_blkd)+blk->size) < 0) {
			print("failed to unmap memory region.\n");
		}
		return;
	}

	struct ar_pile *pile = (struct ar_pile*)((mdl_u8_t*)blk-blk->off);

	ar_set_pile_info_m_free(pile, pile->info.m_free+blk->size);
	blk_unchain(pile, blk);
	if (is_set(blk->prev)) {
		prev = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->prev>>1));
		while(is_free(prev)) {
# ifdef __DEBUG
			print("found free space above, size: %u\n", prev->size);
# endif
			dechain_free(pile, prev);
			blk_unchain(pile, prev);

			prev->size += blk->size+sizeof(struct ar_blkd);
			ar_set_pile_info_m_free(pile, pile->info.m_free+sizeof(struct ar_blkd));
			blk = prev;
			pile->blk_c--;

			if (!is_set(prev->prev)) break;
			prev = (struct ar_blkd*)((mdl_u8_t*)pile->p+(prev->prev>>1));
		}
	}

	if (is_set(blk->next)) {
		next = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
		while(is_free(next)) {
# ifdef __DEBUG
			print("found free space below, size: %u\n", next->size);
# endif
			dechain_free(pile, next);
			blk_unchain(pile, next);

			blk->size += next->size+sizeof(struct ar_blkd);
			ar_set_pile_info_m_free(pile, pile->info.m_free+sizeof(struct ar_blkd));
			blk->next = next->next;
			pile->blk_c--;

			if (!is_set(next->next)) break;
			next = (struct ar_blkd*)((mdl_u8_t*)pile->p+(next->next>>1));
		}
	}
# ifdef __DEBUG
	print("%u bytes are going to be freed.\n", blk->size);
# endif
	ar_off_t off = (mdl_u8_t*)blk-(mdl_u8_t*)pile->p;
	if ((off+sizeof(struct ar_blkd)+blk->size) == pile->off) {
		pile->off = off;
		return;
	}

	blk_rechain(pile, blk);
	ar_off_t *bin = BIN(pile, blk->size);
	if (!is_set(*bin)) {
		*bin = (off<<1)|1;
		if (is_set(pile->free)) {
			blk->bk = pile->free;
			((struct ar_blkd*)((mdl_u8_t*)pile->p+(pile->free>>1)))->fd = (off<<1)|1;
		}
	} else {
		blk->fd = *bin;
		((struct ar_blkd*)((mdl_u8_t*)pile->p+(*bin>>1)))->bk = (off<<1)|1;
		*bin = (off<<1)|1;
	}

	pile->free = (off<<1)|1;
	if (is_flag(blk->flags, BLK_USED_FLG))
		blk->flags ^= BLK_USED_FLG;
	blk->flags |= BLK_FREE_FLG;
}

void ar_free(void *__p) {
	if (__p == NULL) return;
# ifdef __DEBUG
	print("free: %u-bytes\n", get_blkd(__p)->size);
# endif
	_ar_free(&ar, __p);
}

void* _ar_realloc(struct allocr *__ar, void *__p, mdl_uint_t __bc) {
	void *p = _ar_alloc(__ar, __bc);
	_ar_free(__ar, __p);
	return p;
}

void* ar_realloc(void *__p, mdl_uint_t __bc) {
	return _ar_realloc(&ar, __p, __bc);
}



