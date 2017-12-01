# ifndef NULL
#	 define NULL ((void*)0)
# endif
# include <mdlint.h>
# include "print.h"
# include <unistd.h>
# include <sys/resource.h>
# include <sys/mman.h>

# define ALIGN 8
# define align_to(__val, __to)((__val+(__to-1))&((~__to)+1))
# define is_aligned_to(__val, __align)(((__val&__align) == __align)||!(__val&__align))
# define PILE_IPC 6
# define PAGE_SHIFT 7
# define PAGE_SIZE (1<<PAGE_SHIFT)
# define DESLICE_THRESH 8

# define BIN_LOOKUP_DEPTH

# define NEW_PILE_FLG (1<<7)
# define BLK_FREE_FLG (1<<7)
# define BLK_USED_FLG (1<<6)
# define BLK_MMAPED_FLG (1<<5)
# define is_flag(__flags, __flag) ((__flags&__flag) == __flag)

# define is_free(__blk) is_flag(__blk->flags, BLK_FREE_FLG)
# define is_used(__blk) is_flag(__blk->flags, BLK_USED_FLG)

# define set_free(__blk) \
	if (is_flag(__blk->flags, BLK_USED_FLG)) \
		__blk->flags ^= BLK_USED_FLG; \
	__blk->flags |= BLK_FREE_FLG;

# define set_used(__blk) \
	if (is_flag(__blk->flags, BLK_FREE_FLG)) \
		__blk->flags ^= BLK_FREE_FLG; \
	__blk->flags |= BLK_USED_FLG;

# define bin_no(__bc) \
	(((mdl_u32_t)(__bc))>>7 >= (1<<7)?((1<<7)/*+((((mdl_u32_t)(__bc))-(1<<7))>>31)*/):(((mdl_u32_t)(__bc))>>7))

# define NO_BINS 128
# define is_set(__v)(__v&0x1)
# define bin_at(__pile, __bc) (__pile->bins+bin_no(__bc))
# define get_off(__p1, __p2) ((mdl_u8_t*)__p2-(mdl_u8_t*)__p1)
# define incr_pile_off(__pile, __by) pile->off+=__by
# define decr_pile_off(__pile, __by) pile->off-=__by
# define __DEBUG
typedef mdl_u32_t ar_off_t;
typedef mdl_u32_t ar_size_t;
typedef mdl_u32_t ar_uint_t;
struct ar_blkd {
	ar_off_t prev, next, fd, bk;
	ar_size_t size;
	ar_off_t off;
	mdl_u8_t flags;
} __attribute__((packed));

# define blkd_size sizeof(struct ar_blkd)
# define get_blkd(__p)((struct ar_blkd*)((mdl_u8_t*)(__p)-sizeof(struct ar_blkd)))

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
	ar_uint_t blk_c;
	struct pile_info info;
} __attribute__((packed));

struct allocr {
	void *sbrk_base;
	ar_uint_t no_mmaps;
	ar_uint_t no_piles;
	struct ar_pile *pile;
};

static struct allocr ar;
# define set_pile_info_m_used(__pile, __val) \
	__pile->info.m_used = __val;\
	__pile->info.m_free = (__pile->page_c*PAGE_SIZE)-__pile->info.m_used;

# define set_pile_info_m_free(__pile, __val) \
	__pile->info.m_free = __val;\
	__pile->info.m_used = (__pile->page_c*PAGE_SIZE)-__pile->info.m_free;

void ar_prepare() {
	ar = (struct allocr) {
		.sbrk_base = sbrk(0),
		.no_mmaps = 0,
		.no_piles = 0,
		.pile = NULL
	};
}

void ar_cleanup() {
	brk(ar.sbrk_base);
}

struct ar_pile* suitable_pile(struct allocr *__ar, mdl_uint_t __bc) {
	struct ar_pile *pile = __ar->pile;
	while(pile != NULL) {
		if (pile->info.m_free >= (__bc+blkd_size) || (pile->page_c*PAGE_SIZE)-pile->off >= (__bc+blkd_size)) return pile;
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
	mdl_u32_t *bin = bin_at(__pile, __blk->size);
	if ((*bin)>>1 == off) *bin = __blk->fd;

	if (is_set(__blk->fd))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->fd>>1)))->bk = __blk->bk;
	if (is_set(__blk->bk))
		((struct ar_blkd*)((mdl_u8_t*)__pile->p+(__blk->bk>>1)))->fd = __blk->fd;
	__blk->fd = 0;
	__blk->bk = 0;
}

void pr() {
	print("program memory size: %u\n", get_off(ar.sbrk_base, sbrk(0)));
	print("no. maped memory regions: %u\n", ar.no_mmaps);

	struct ar_pile *pile = ar.pile;
	struct ar_blkd *blk;
	mdl_uint_t pile_no = 0, blk_no;
	mdl_u32_t cal_used = 0;
	while(pile != NULL) {
		blk_no = 0;
		print("-> pile no. %u, info{m_used: %u, m_free: %u}\n", pile_no++, pile->info.m_used, pile->info.m_free);
		if (is_set(pile->blk)) {
			blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(pile->blk>>1));
			_next_blk:
			if (is_used(blk))
				cal_used+=blk->size;

			print(" blk; state: %s, is_prev: %s, is_next: %s, size: %u\n", is_used(blk)?"used":"free", is_set(blk->prev)?"yes":"no", is_set(blk->next)?"yes":"no", blk->size);
			if (is_set(blk->next)) {
				blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
				goto _next_blk;
			}
		}
		print("cal_used: %u\n", cal_used);
		print("/\n");
		pile = pile->next;
	}
}

void fr() {

}

void _ar_free(struct allocr*, void*);
void* _ar_alloc(struct allocr *__ar, mdl_uint_t __bc, mdl_u8_t __flags) {
	if ((__bc+blkd_size) > PILE_IPC*PAGE_SIZE) {
		print("mmap.\n");
		void *p;
		if ((p = mmap(NULL, blkd_size+__bc, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == (void*)-1) {
			print("failed to map memory region for oversized block.\n");
			return NULL;
		}

		*(struct ar_blkd*)p = (struct ar_blkd) {
			.prev = 0, .next = 0,
			.size = __bc, .off = 0,
			.flags = BLK_MMAPED_FLG
		};

		__ar->no_mmaps++;
		return (void*)((mdl_u8_t*)p+blkd_size);
	}

	struct ar_pile *pile = suitable_pile(__ar, __bc);
	if (pile == NULL || is_flag(__flags, NEW_PILE_FLG)) {
# ifdef __DEBUG
		print("new pile.\n");
# endif
		mdl_uint_t page_c = PILE_IPC;
		void *p = sbrk(0), *end;
		if (p == (void*)-1) {
			print("sbrk failure.\n");
		}
		if (brk(end = (void*)((mdl_u8_t*)p+(sizeof(struct ar_pile)+(page_c*PAGE_SIZE)))) == -1) {
			print("brk failure.\n");
		}
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
		ar_off_t f_off;
		mdl_u32_t *bin = bin_at(pile, __bc);
		mdl_u32_t *end = bin+4;
		while(bin != end && bin != bin+NO_BINS) {
			if (is_set(*bin)) {
				f_off = (*bin)>>1;
				blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+f_off);
				if (blk->size >= __bc) goto _found;
				else {
					while(is_set(blk->fd)) {
						if (bin+1 < bin+NO_BINS)
							if (is_set(*(bin+1)) && *(bin+1) == blk->fd) break;

						f_off = blk->fd>>1;
						blk = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->fd>>1));
						if (blk->size >= __bc) goto _found;
					}
				}
			}
			bin++;
		}

		// new pile
		if ((pile->page_c*PAGE_SIZE)-pile->off < (blkd_size+__bc))
			return _ar_alloc(__ar, __bc, NEW_PILE_FLG);
		goto _nope;

		_found:
		set_pile_info_m_used(pile, pile->info.m_used+blk->size+blkd_size);
		dechain_free(pile, blk);

		set_used(blk);
		blk_rechain(pile, blk);

		mdl_u32_t junk;
		if (blk->size > __bc && (junk = (blk->size-__bc)) > (blkd_size+DESLICE_THRESH)) {
			void *p = (void*)((mdl_u8_t*)blk+(blkd_size+__bc));
			struct ar_blkd *s = (struct ar_blkd*)p;
			mdl_u32_t s_off;
			*s = (struct ar_blkd) {
				.prev = (((mdl_u8_t*)blk-(mdl_u8_t*)pile->p)<<1)|1, .next = blk->next, .fd = 0, .bk = 0,
				.size = junk-blkd_size, .off = (s_off = ((mdl_u8_t*)p-(mdl_u8_t*)pile->p))+sizeof(struct ar_pile),
				.flags = 0x0
			};
			blk->size = __bc;
			blk->next = (s_off<<1)|1;
			if (is_set(s->next)) ((struct ar_blkd*)((mdl_u8_t*)pile->p+(s->next>>1)))->prev = (s_off<<1)|1;
			pile->blk_c++;
			_ar_free(__ar, (void*)((mdl_u8_t*)p+blkd_size));
		}
		return (void*)((mdl_u8_t*)blk+blkd_size);
	}

	_nope:

	{
	void *p = (void*)((mdl_u8_t*)pile->p+pile->off);
	struct ar_blkd *blk = (struct ar_blkd*)p;
	mdl_u32_t off = get_off(pile->p, blk);
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
	incr_pile_off(pile, blkd_size+__bc);
	set_pile_info_m_used(pile, pile->info.m_used+__bc+blkd_size);
	pile->blk_c++;

//	print("%u - %u - %u\n", pile->off, pile->info.m_free, pile->page_c*PAGE_SIZE);
	return (void*)((mdl_u8_t*)p+blkd_size);
	}
}

void* ar_alloc(mdl_uint_t __bc) {
# ifdef __DEBUG
	print("alloc: %u-bytes\n", __bc);
# endif
	return _ar_alloc(&ar, align_to(__bc, ALIGN), 0x0);
}

void _ar_free(struct allocr *__ar, void *__p) {
	struct ar_blkd *blk = get_blkd(__p), *prev, *next;
	if (is_flag(blk->flags, BLK_MMAPED_FLG)) {
		if (munmap((void*)blk, blkd_size+blk->size) < 0) {
			print("failed to unmap memory region.\n");
		}
		__ar->no_mmaps--;
		return;
	}

	struct ar_pile *pile = (struct ar_pile*)((mdl_u8_t*)blk-blk->off);
	set_pile_info_m_free(pile, pile->info.m_free+blk->size+blkd_size);
	blk_unchain(pile, blk);
	if (is_set(blk->prev)) {
		prev = (struct ar_blkd*)((mdl_u8_t*)pile->p+(blk->prev>>1));
		while(is_free(prev)) {
# ifdef __DEBUG
			print("found free space above, size: %u\n", prev->size);
# endif
			dechain_free(pile, prev);
			blk_unchain(pile, prev);

			prev->size += blk->size+blkd_size;
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

			blk->size += next->size+blkd_size;
			blk->next = next->next;
			pile->blk_c--;

			if (!is_set(next->next)) break;
			next = (struct ar_blkd*)((mdl_u8_t*)pile->p+(next->next>>1));
		}
	}
# ifdef __DEBUG
	print("%u bytes are going to be freed.\n", blk->size);
# endif
	ar_off_t off = get_off(pile->p, blk);
	if (pile->off-off == blkd_size+blk->size) {
		pile->off = off;
		return;
	}

	blk_rechain(pile, blk);
	ar_off_t *bin = bin_at(pile, blk->size);
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
	set_free(blk);
}

void ar_free(void *__p) {
	if (__p == NULL) return;
# ifdef __DEBUG
	print("free: %u-bytes\n", get_blkd(__p)->size);
# endif
	_ar_free(&ar, __p);
}

void* _ar_realloc(struct allocr *__ar, void *__p, mdl_uint_t __bc) {
	void *p = _ar_alloc(__ar, __bc, 0x0);
	mdl_u32_t size = get_blkd(__p)->size;
 	if (__bc < size) size = __bc;

	mdl_u8_t *itr = (mdl_u8_t*)__p;
	while(itr != (mdl_u8_t*)__p+size) {
		mdl_u32_t off = itr-(mdl_u8_t*)__p;
		if ((size-off) > sizeof(mdl_u64_t)) {
			*((mdl_u64_t*)((mdl_u8_t*)p+off)) = *(mdl_u64_t*)itr;
			itr+= sizeof(mdl_u64_t);
		} else if ((size-off) > sizeof(mdl_u32_t)) {
			*((mdl_u32_t*)((mdl_u8_t*)p+off)) = *(mdl_u32_t*)itr;
			itr+= sizeof(mdl_u32_t);
		} else
			*((mdl_u8_t*)p+off) = *(itr++);
	}
	_ar_free(__ar, __p);
	return p;
}

void* ar_realloc(void *__p, mdl_uint_t __bc) {
	return _ar_realloc(&ar, __p, __bc);
}

mdl_u8_t static preped = 0;
void* malloc(size_t __n) {
	if (!__n) return NULL;
# ifdef __DEBUG
	print("malloc called.\n");
# endif
	if (!preped) {
		ar_prepare();
		preped = 1;
	}
	void *p = ar_alloc(__n);
	return p;
}

void free(void *__p) {
	if (!__p) return;
# ifdef __DEBUG
	print("free called.\n");
# endif
	ar_free(__p);
}

# include <string.h>
void* realloc(void *__p, size_t __n) {
# ifdef __DEBUG
	print("called realloc.\n");
# endif
	if (!__n) return NULL;
	if (!__p) return ar_alloc(__n);
	return ar_realloc(__p, __n);
}

