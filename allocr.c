# include "allocr.h"
# include <unistd.h>
# include <sys/resource.h>
# include <sys/mman.h>

// trim oversized piles after space becomes unused
# define TRIM_OS_PILES
# define PILE_IPC 2

# define PAGE_SHIFT 7
# define PAGE_SIZE (1<<PAGE_SHIFT)

# define BLK_USED (1<<7)
# define BLK_FREE (1<<6)
# define IS_FLAG(__flags, __flag) ((__flags&__flag) == __flag)

# define IS_OK(__v)((__v)&0x1)
# define UFREE(__pile) __pile->m_free = (__pile->page_c*PAGE_SIZE)-__pile->m_used
# define EMPTY_PILE(__pile) (__pile->m_used == 0)

# define BNO(__bc) \
	((__bc>>9) >= 127? 127+(__bc>>31):(__bc>>9))
# define BIN(__pile, __bc) (__pile->bins+BNO(__bc))

# define PAGE_NO(__off) (__off>>PAGE_SHIFT)
struct allocr ar = {
	.d = {
		.no_piles = 0, .pile = NULL, .no_uu_piles = 0
	}
};

struct blkd {
	mdl_u32_t prev, next;
	mdl_u32_t nextf, prevf;
	mdl_u8_t flags;
	mdl_u32_t size, off;
};

void unchain_pile(struct allocrd *__ard, struct ar_pile *__pile) {
	if (__ard->pile == __pile)
		__ard->pile = __pile->next;
	else {
		__pile->next->prev = __pile->prev;
		__pile->prev->next = __pile->next;
	}
}

void mutex_lock(mdl_u16_t *__mutex) {
	__asm__ volatile(
		"_nope:\n"
		"cmpb $1, (%0)\n"
		"je _nope\n"
		"mov $1, %%sil\n"
		"lock xchg %%sil, (%0)\n"
		"cmpb $0, %%sil\n"
		"jne _nope\n"
		: : "r"(__mutex) : "%rax"
	);
}

void mutex_unlock(mdl_u16_t *__mutex) {
	__asm__ volatile(
		"movb (%0), %%sil\n"
		"lock xorb %%sil, (%0)\n"
		: : "r"(__mutex) : "%rax"
	);
}

void _ar_init(struct allocrd *__ard) {
# ifdef TEST
	if ((__ard->p = sbrk(0)) == (void*)-1) {
# ifdef __DEBUG
		print("sbrk error.\n");
# endif
	}
# endif
}

void _ar_de_init(struct allocrd *__ard) {
# ifdef TEST
	if (brk(__ard->p) < 0) {
# ifdef __DEBUG
		print("brk error.\n");
# endif
	}
# endif
}

void ar_init(struct allocr *__ar) {
	_ar_init(&__ar->d);
}

void ar_de_init(struct allocr *__ar) {
	_ar_de_init(&__ar->d);
}

void static ublk_off(struct ar_pile *__pile, struct blkd *__blk, mdl_u32_t __old_off) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (__pile->first_blk>>1 == __old_off)
		__pile->first_blk = (off<<1)|1;
	if (__pile->last_blk>>1 == __old_off)
			__pile->last_blk = (off<<1)|1;

	if (IS_OK(__blk->prev))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->prev>>1)))->next = (off<<1)|1;
	if (IS_OK(__blk->next))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->next>>1)))->prev = (off<<1)|1;
	if (IS_FLAG(__blk->flags, BLK_FREE)) {
		if (__pile->ffree>>1 == __old_off)
				__pile->ffree = (off<<1)|1;
		if (__pile->lfree>>1 == __old_off)
				__pile->lfree = (off<<1)|1;


		mdl_u32_t *bin = BIN(__pile, __blk->size);
		if ((*bin)>>1 == __old_off)
			*bin = (off<<1)|1;

		if (IS_OK(__blk->nextf))
			((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->nextf>>1)))->prevf = (off<<1)|1;
		if (IS_OK(__blk->prevf))
			((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->prevf>>1)))->nextf = (off<<1)|1;
	}
}

void static rechain(struct ar_pile *__pile, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off < (__pile->first_blk>>1) || !IS_OK(__pile->first_blk)) __pile->first_blk = (off<<1)|1;
	if (off > (__pile->last_blk>>1) || !IS_OK(__pile->last_blk)) __pile->last_blk = (off<<1)|1;
	if (IS_OK(__blk->prev))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->prev>>1)))->next = (off<<1)|1;
	if (IS_OK(__blk->next))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->next>>1)))->prev = (off<<1)|1;
}

void static unchain(struct ar_pile *__pile, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off == __pile->first_blk>>1) __pile->first_blk = __blk->next;
	if (off == __pile->last_blk>>1) __pile->last_blk = __blk->prev;
	if (IS_OK(__blk->prev))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->prev>>1)))->next = __blk->next;
	if (IS_OK(__blk->next))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->next>>1)))->prev = __blk->prev;
}

mdl_u8_t growable_or_shrinkable(void *__p) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd));
	struct ar_pile *pile = (struct ar_pile*)((mdl_u8_t*)blk-blk->off);
	if (IS_OK(blk->next))
		if (IS_FLAG(((struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1)))->flags, BLK_FREE)) return 1;
	if (IS_OK(blk->prev))
		if (IS_FLAG(((struct blkd*)((mdl_u8_t*)pile->p+(blk->prev>>1)))->flags, BLK_FREE)) return 1;
	return (__p == (void*)((mdl_u8_t*)pile->p+(pile->off-blk->size)));
}

void static unchain_f(struct ar_pile*, struct blkd*);
void* _ar_grow(void *__p, mdl_u32_t __by) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd));
	struct ar_pile *pile = (struct ar_pile*)((mdl_u8_t*)blk-blk->off);
	if (__p == (void*)((mdl_u8_t*)pile->p+(pile->off-blk->size))) {
		blk->size+= __by;
		pile->off+= __by;
		pile->m_used+= __by;
		UFREE(pile);
		return __p;
	}

	struct blkd *next = NULL, *end = NULL;
	mdl_u32_t got = 0;
	if (blk->next&0x1) {
	next = (struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
	while(IS_FLAG(next->flags, BLK_FREE) && got < __by) {
		got += next->size+sizeof(struct blkd);
		end = next;
		if (!(next->next&0x1)) break;
	}
	}

	if (end != NULL && got >= __by) {
		next = (struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
		_next:

		unchain_f(pile, next);

		if (next == end) goto _done;
		next = (struct blkd*)((mdl_u8_t*)pile->p+(next->next>>1));
		goto _next;
		_done:
		end->prev = (((mdl_u8_t*)blk-(mdl_u8_t*)pile->p)<<1)|1;
		blk->next = end->next;
	} else
		return NULL;
	pile->m_used+= __by;
	UFREE(pile);
	return __p;
}

void* _ar_shrink(void *__p, mdl_u32_t __by) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd));
	struct ar_pile *pile = (struct ar_pile*)((mdl_u8_t*)blk-blk->off);
	pile->m_used-= __by;
	UFREE(pile);

	if (__p == (void*)((mdl_u8_t*)pile->p+(pile->off-blk->size))) {
		blk->size-= __by;
		pile->off-= __by;
		return __p;
	}

	struct blkd *prev = (struct blkd*)((mdl_u8_t*)pile->p+(blk->prev>>1));
	struct blkd *next = (struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
	struct blkd temp;
	mdl_u32_t old_off;
	if (blk->prev&0x1) {
		if (IS_FLAG(prev->flags, BLK_FREE)) {
			prev->size+= __by;
			blk->size-= __by;

			temp = *blk;
			old_off = (mdl_u8_t*)blk-(mdl_u8_t*)pile->p;
			blk = (struct blkd*)((mdl_u8_t*)blk+__by);
			*blk = temp;

			ublk_off(pile, blk, old_off);
			blk->off+= __by;
			return (void*)((mdl_u8_t*)blk+sizeof(struct blkd));
		}
	}

	if (blk->next&0x1) {
		if (IS_FLAG(next->flags, BLK_FREE)) {
			next->size+= __by;
			blk->size-= __by;

			temp = *next;
			old_off = (mdl_u8_t*)next-(mdl_u8_t*)pile->p;
			next = (struct blkd*)((mdl_u8_t*)blk-__by);
			*next = temp;

			ublk_off(pile, next, old_off);
			next->off-= __by;
			return __p;
		}
	}

	pile->m_used+= __by;
	UFREE(pile);
	return NULL;
}

void* ar_grow(void *__p, mdl_u32_t __by) {
	return _ar_grow(__p, __by);
}

void* ar_shrink(void *__p, mdl_u32_t __by) {
	return _ar_shrink(__p, __by);
}

struct ar_pile* suitable_pile(struct allocrd *__ard, mdl_uint_t __bc) {
	if (!__ard->pile) return NULL;
	struct ar_pile *pile = __ard->pile;
	while(pile != NULL) {
		print("---------- %u\n", pile->m_free);
		if (pile->m_free >= __bc) return pile;
		pile = pile->next;
	}

	return NULL;
}

struct ar_page* associated_page(void *__p, mdl_u32_t __off) {
	return ((struct ar_page*)((mdl_u8_t*)__p+sizeof(struct ar_pile)))+PAGE_NO(__off);
}

void _ar_free(struct allocrd*, struct ar_pile*, void*);
void* _ar_alloc(struct allocrd *__ard, struct ar_pile *__pile, mdl_u32_t __bc) {
	struct ar_pile *pile;
	if (__pile != NULL)
		pile = __pile;
	else if (!(pile = suitable_pile(__ard, __bc+sizeof(struct blkd)))) {
		mdl_u32_t bc = __bc+sizeof(struct blkd);
		mdl_u16_t page_c = __bc > (PILE_IPC*PAGE_SIZE)? (bc>>PAGE_SHIFT)+((bc-((bc>>PAGE_SHIFT)*PAGE_SIZE))>0):PILE_IPC;
		print("%u\n", page_c);
		void *p;
		if ((p = mmap(NULL, sizeof(struct ar_pile)+(page_c*PAGE_SIZE)+(page_c*sizeof(struct ar_page)), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == (void*)-1) {
			print("failed to map memory.\n");
			return NULL;
		}

		struct ar_page *begin = (struct ar_page*)((mdl_u8_t*)p+sizeof(struct ar_pile));
		struct ar_page *page = begin;
		while(page != begin+page_c)
			*(page++) = (struct ar_page) {
				.blk_c =  0, .no_dead_blks = 0
			};

		__ard->no_piles++;
		*(pile = (struct ar_pile*)p) = (struct ar_pile) {
			.p = (mdl_u8_t*)p+sizeof(struct ar_pile)+(page_c*sizeof(struct ar_page)),
			.off = 0, .page_c = page_c,
			.first_blk = 0, .last_blk = 0,
			.ffree = 0, .lfree = 0,
			.m_used = 0, .m_free = page_c*PAGE_SIZE,
			.prev = NULL, .next = NULL
		};

		pile->end = (mdl_u8_t*)pile->p+(page_c*PAGE_SIZE);

		mdl_u32_t *itr = pile->bins;
		while(itr != pile->bins+NO_BINS) *(itr++) = 0;
		if (__ard->pile == NULL)
			__ard->pile = pile;
		else {
			pile->next = __ard->pile;
			__ard->pile->prev = pile;
			__ard->pile = pile;
		}
	}

	if (IS_OK(pile->ffree)) {
		struct blkd *blk;
		mdl_u32_t *bin = BIN(pile, __bc), f_off = 0;
		while(bin != pile->bins+BIN_MAX_LUP && bin < pile->bins+NO_BINS) {
# ifdef __DEBUG
			print("looking for free memory blocks in bin %u\n", bin-pile->bins);
# endif
			if (IS_OK((f_off = *bin))) {
				blk = (struct blkd*)((mdl_u8_t*)pile->p+(f_off>>1));
				if (blk->size >= __bc) goto _found;
				else {
					f_off = 0;
					while(IS_OK(blk->nextf)) {
						if (blk->nextf == *(pile->bins+NO_BINS)) goto _nope;
						if (blk->nextf == *(bin+1)) goto _nope;

						f_off = blk->nextf;
						blk = (struct blkd*)((mdl_u8_t*)pile->p+(blk->nextf>>1));
						if (blk->size >= __bc) goto _found;
						f_off = 0;
					}
				}
			} else bin++;
		}

		_found:
		if (!IS_OK(f_off)) goto _nope;
# ifdef __DEBUG
		print("found free space!\n");
# endif
		pile->m_used += blk->size+sizeof(struct blkd);
		UFREE(pile);

		unchain_f(pile, blk);
		if (blk->size > __bc && (blk->size-__bc) > sizeof(struct blkd)) {
			struct blkd *s = (struct blkd*)((mdl_u8_t*)blk+(sizeof(struct blkd)+__bc));
			*s = (struct blkd) {
				.prev = (((mdl_u8_t*)blk-(mdl_u8_t*)pile->p)<<1)|1, .next = blk->next,
				.nextf = 0, .prevf = 0,
				.flags = BLK_FREE,
				.size = (blk->size-__bc)-sizeof(struct blkd),
			};
			blk->size = __bc;
			mdl_u32_t s_off = (mdl_u8_t*)s-(mdl_u8_t*)pile->p;
			s->off = s_off+sizeof(struct ar_pile)+(pile->page_c*sizeof(struct ar_page));
			blk->next = (s_off<<1)|1;
			if (s->next&0x1) ((struct blkd*)((mdl_u8_t*)pile->p+(s->next>>1)))->prev = blk->next;
			if (IS_FLAG(blk->flags, BLK_FREE))
				blk->flags ^= BLK_FREE;
			blk->flags |= BLK_USED;
			_ar_free(__ard, pile, (mdl_u8_t*)s+sizeof(struct blkd));
			goto _ret;
		}

		if (IS_FLAG(blk->flags, BLK_FREE))
			blk->flags ^= BLK_FREE;
		blk->flags |= BLK_USED;
		_ret:
		rechain(pile, blk);
		return (void*)((mdl_u8_t*)blk+sizeof(struct blkd));
//		}
//
//		if (!(blk->nextf&0x1)) goto _nothing;
//		blk = (struct blkd*)((mdl_u8_t*)pile->p+(blk->nextf>>1));
//		goto _lagain;
	}
	_nope:
# ifdef TEST
	if ((__ard->off = (__ard->off+(__bc+sizeof(struct blkd)))) > __ard->page_c*PAGE_SIZE) {
		__ard->page_c = (__ard->off>>PAGE_SHIFT)+((__ard->off-((__ard->off>>PAGE_SHIFT)*PAGE_SIZE))>0);
		if (brk((__ard->end = (__ard->p+(__ard->page_c*PAGE_SIZE)))) < 0) {
# ifdef __DEBUG
			print("brk error.\n");
# endif
		}
# ifdef __DEBUG
	//	print("size: %u\n", __ard->page_c*PAGE_SIZE);
# endif
	}
# endif
	__asm__("nop");
	mdl_u32_t off = pile->off;
	if ((pile->off = (pile->off+(__bc+sizeof(struct blkd)))) > pile->page_c*PAGE_SIZE) {
		// err
		print("error.\n");
		return NULL;
	}

	struct blkd *blk = (struct blkd*)((mdl_u8_t*)pile->p+off);
	*blk = (struct blkd) {
		.prev = pile->last_blk, .next = 0,
		.nextf = 0, .prevf = 0,
		.flags = BLK_USED,
		.size = __bc, .off = off+sizeof(struct ar_pile)+(pile->page_c*sizeof(struct ar_page))
	};

	mdl_u16_t pages = PAGE_NO(pile->off-1)-PAGE_NO(off);
	struct ar_page *begin = associated_page((void*)pile, off);
	if (pages > 0) {
	struct ar_page *page = begin;
	while(page <= begin+pages) {
		page->blk_c++;
		page++;
	}
	} else begin->blk_c++;

	print("pages: %u - %u\n", pages, pile->off>>PAGE_SHIFT);
# ifdef __DEBUG
	//print("off: %u, last_blk: %u\n", off, __pile->last_blk>>1);
# endif
	if (!IS_OK(pile->first_blk)) pile->first_blk = (off<<1)|1;
	if (IS_OK(pile->last_blk))
		((struct blkd*)((mdl_u8_t*)pile->p+(pile->last_blk>>1)))->next = (off<<1)|1;

	pile->last_blk = (off<<1)|1;
	void *ret = (void*)((mdl_u8_t*)pile->p+(off+sizeof(struct blkd)));

	pile->m_used += __bc+sizeof(struct blkd);
	UFREE(pile);
	return ret;
}

void unchain_f(struct ar_pile *__pile, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__pile->p;
	if (off == __pile->ffree>>1) __pile->ffree = __blk->nextf;
	if (off == __pile->lfree>>1) __pile->lfree = __blk->prevf;
	mdl_u32_t *bin = BIN(__pile, __blk->size);
	if ((*bin)>>1 == off) *bin = __blk->nextf;

	if (IS_OK(__blk->nextf))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->nextf>>1)))->prevf = __blk->prevf;
	if (IS_OK(__blk->prevf))
		((struct blkd*)((mdl_u8_t*)__pile->p+(__blk->prevf>>1)))->nextf = __blk->nextf;
	__blk->nextf = 0;
	__blk->prevf = 0;
}

char btrc(mdl_u8_t __data) {
	if (__data  < (1<<1))
		return 0x2E;
	else if (__data >= (1<<1) && __data < (1<<2))
		return 0x2C;
	else if (__data >= (1<<2) && __data < (1<<3))
		return 0x27;
	else if (__data >= (1<<3) && __data < (1<<4))
		return 0x2D;
	else if (__data >= (1<<4) && __data < (1<<5))
		return 0x2F;
	else if (__data >= (1<<5) && __data < (1<<6))
		return 0x3A;
	else if (__data >= (1<<6) && __data < (1<<7))
		return 0x3B;
	else
		return '#';
	return ' ';
}

void pr_data(void *__p, mdl_uint_t __bc) {
	mdl_u8_t *itr = (mdl_u8_t*)__p;
	while(itr != (mdl_u8_t*)__p+__bc) {
		mdl_uint_t off = itr-(mdl_u8_t*)__p;
		if (((off>>4)-((off-1)>>4)) && off > 0) {
			print("[ %c ]\n", btrc(*itr));
		} else
			print("[ %c ] ", btrc(*itr));
		itr++;
	}
	if (((__bc-1)-(((__bc-1)>>4)*(1<<4)))>0)
		print("\n");
}

void pr(struct allocrd *__ard) {
	struct ar_pile *pile = __ard->pile;
	struct blkd *blk;
	struct ar_page *begin, *page;
	mdl_u32_t *bin;
	mdl_u16_t pile_no = 0, page_no;

	while(pile != NULL) {
	print("pile - %u, used: %u, free: %u\n", pile_no++, pile->m_used, pile->m_free);
	page = begin = (struct ar_page*)((mdl_u8_t*)pile+sizeof(struct ar_pile));
	page_no = 0;
	while(page != begin+pile->page_c) {
		print(" page: %u, blk count: %u, dead blks: %u.\n", page_no, page->blk_c, page->no_dead_blks);
		pr_data((mdl_u8_t*)pile->p+(page_no*PAGE_SIZE), PAGE_SIZE);
		page_no++;
		page++;
	}

	blk = (struct blkd*)((mdl_u8_t*)pile->p+(pile->first_blk>>1));
	if (IS_OK(pile->first_blk)) {
	bin = pile->bins;
	while(bin != pile->bins+NO_BINS) {
		if (IS_OK(*bin)) {
			print("bin: %lu\n", (*bin)>>1);
		}
		bin++;
	}

	while(1) {
		print("   state: %s\tsize: %u \tis upper? %s{%u}\tis next? %s{%u}\tnextf: %s\tprevf: %s, off: %u\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size,
			IS_OK(blk->prev)?"yes":"no", blk->prev>>1, IS_OK(blk->next)?"yes":"no", blk->next>>1, IS_OK(blk->nextf)?"yes":"no", IS_OK(blk->prevf)?"yes":"no", (mdl_u8_t*)blk-(mdl_u8_t*)pile->p);
		if (!IS_OK(blk->next)) break;
		blk = (struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
	}}pile = pile->next;}

}

void fr(struct allocrd *__ard) {
	struct ar_pile *pile = __ard->pile;
	struct blkd *blk;

	while(pile != NULL) {
	blk = (struct blkd*)((mdl_u8_t*)pile->p+(pile->ffree>>1));
	if (IS_OK(pile->ffree)) {
	while(1) {
		print("state: %s, size: %u, off: %lu\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size, (mdl_u8_t*)blk-(mdl_u8_t*)pile->p);
		if (!IS_OK(blk->nextf)) break;
		blk = (struct blkd*)((mdl_u8_t*)pile->p+(blk->nextf>>1));
	}}pile = pile->next;}
}

void _ar_free(struct allocrd *__ard, struct ar_pile *__pile, void *__p) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)), *prev, *next;
	struct ar_pile *pile = __pile != NULL?__pile:(struct ar_pile*)((mdl_u8_t*)blk-blk->off);
	if (!(pile->m_used-(blk->size+sizeof(struct blkd))) && __ard->no_uu_piles > UO_PILES_TO_KEEP) {
		unchain_pile(__ard, pile);
		if (munmap((void*)pile, sizeof(struct ar_pile)+(pile->page_c*PAGE_SIZE)) < 0) {
			print("failed to unmap memory at address{0x%08x}.\n", (void*)pile);
		}
		return;
	}

	struct ar_page *begin, *page;
	mdl_u32_t off;
	mdl_u16_t pages;
	unchain(pile, blk);
# ifdef TRIM_OS_PILES
	if (pile->page_c > PILE_IPC) {
		if ((mdl_u8_t*)blk > ((mdl_u8_t*)pile->p+(PILE_IPC*PAGE_SIZE))) {
			if (munmap((void*)blk, sizeof(struct blkd)+blk->size) < 0) {
				print("failed to unmap memory at address{0x%08x}.\n", (void*)pile);
			}
			return;
		}
	}
# endif
	pile->m_used -= blk->size+sizeof(struct blkd);
	UFREE(pile);

	if (EMPTY_PILE(pile)) __ard->no_uu_piles++;

	off = (mdl_u8_t*)blk-(mdl_u8_t*)pile->p;
	pages = PAGE_NO(off+blk->size+sizeof(struct blkd))-PAGE_NO(off);
	begin = associated_page((void*)pile, off);
	page = begin;
    while(page <= begin+pages) {
	//	page->no_dead_blks--;
		page->blk_c--;
		page++;
	}
	// search upper&lower blocks  if free and murge all into one block
	if (IS_OK(blk->prev)) {
# ifdef __DEBUG
//	print("upper block located.\n");
# endif
	prev = (struct blkd*)((mdl_u8_t*)pile->p+(blk->prev>>1));
	while(IS_FLAG(prev->flags, BLK_FREE)) {
# ifdef __DEBUG
//		print("found free space prev, size: %u\n", prev->size);
# endif
		unchain_f(pile, prev);
		unchain(pile, prev);

		mdl_u32_t off = (mdl_u8_t*)prev-(mdl_u8_t*)pile->p;
		mdl_u16_t pages = PAGE_NO(off+prev->size+sizeof(struct blkd))-PAGE_NO(off);
		begin = associated_page((void*)pile, off);
		page = begin;
		while(page <= begin+pages) {
			page->no_dead_blks--;
			page->blk_c--;
			page++;
		}

		prev->size += blk->size+sizeof(struct blkd);
		blk = prev;

		if (!IS_OK(prev->prev)) break;
		prev = (struct blkd*)((mdl_u8_t*)pile->p+(prev->prev>>1));
	}
	}

	if (IS_OK(blk->next)) {
# ifdef ___DEBUG
//	print("lower block located.\n");
# endif
	next = (struct blkd*)((mdl_u8_t*)pile->p+(blk->next>>1));
	while(IS_FLAG(next->flags, BLK_FREE)) {
# ifdef __DEBUG
//		print("found free space next, size: %u\n", next->size);
# endif
		unchain_f(pile, next);
		unchain(pile, next);

		mdl_u32_t off = (mdl_u8_t*)next-(mdl_u8_t*)pile->p;
		mdl_u16_t pages = PAGE_NO(off+next->size+sizeof(struct blkd))-PAGE_NO(off);
		begin = associated_page((void*)pile, off);
		page = begin;
		while(page <= begin+pages) {
			page->no_dead_blks--;
			page->blk_c--;
			page++;
		}

		blk->size += next->size+sizeof(struct blkd);
		blk->next = next->next;

		if (!IS_OK(next->next)) break;
		next = (struct blkd*)((mdl_u8_t*)pile->p+(next->next>>1));
	}
	}
# ifdef __DEBUG
	print("freed %u bytes.\n", blk->size);
# endif
# ifdef TEST
	mdl_u32_t off;
	if ((off = ((mdl_u8_t*)blk-(mdl_u8_t*)__ard->p)) == (__ard->off-(blk->size+sizeof(struct blkd))) && __ard->page_c > 1) {
		__ard->page_c = (off>>PAGE_SHIFT)+((off-((off>>PAGE_SHIFT)*PAGE_SIZE))>0);
		__ard->off = off;
		if (brk((__ard->end = (__ard->p+(__ard->page_c*PAGE_SIZE)))) < 0) {
# ifdef __DEBUG
			print("brk error.\n");
# endif
		}
# ifdef __DEBUG
		print("....\n");
# endif
		return;
	}
# endif
# ifdef __DEBUG
	//print("----| %u, %u, %u\n", __pile->last_blk, off, __pile->page_c);
# endif
	off = (mdl_u8_t*)blk-(mdl_u8_t*)pile->p;
	pages = PAGE_NO(off+blk->size+sizeof(struct blkd))-PAGE_NO(off);
	begin = associated_page((void*)pile, off);
	page = begin;
	while(page <= begin+pages) {
		page->no_dead_blks++;
		page->blk_c++;
		page++;
	}

	mdl_u32_t *bin = BIN(pile, blk->size);
	if (!IS_OK(*bin))
		*bin = (off<<1)|1;
	else if (IS_OK(pile->lfree)) {
		blk->nextf = *bin;
		((struct blkd*)((mdl_u8_t*)pile->p+((*bin)>>1)))->prevf = (off<<1)|1;
	}

	if (!IS_OK(pile->ffree)) pile->ffree = (off<<1)|1;
	pile->lfree = (off<<1)|1;

	if (IS_FLAG(blk->flags, BLK_USED)) blk->flags ^= BLK_USED;
	blk->flags |= BLK_FREE;
	rechain(pile, blk);
}

void* _ar_realloc(struct allocrd *__ard, struct ar_pile *__pile, void *__p, mdl_u32_t __bc) {
	void *p = _ar_alloc(__ard, __pile, __bc);
	mdl_u32_t size = ((struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)))->size;
	if (__bc < size) size = __bc;

	mdl_u8_t *itr = (mdl_u8_t*)__p;
	while(itr != (mdl_u8_t*)__p+size) {
		mdl_u32_t off = itr-(mdl_u8_t*)__p;
		if ((size-off) > sizeof(mdl_u64_t)) {
			*((mdl_u64_t*)((mdl_u8_t*)p+off)) = *(mdl_u64_t*)itr;
			itr+=sizeof(mdl_u64_t);
		} else if ((size-off) > sizeof(mdl_u32_t)) {
			*((mdl_u32_t*)((mdl_u8_t*)p+off)) = *(mdl_u32_t*)itr;
			itr+=sizeof(mdl_u32_t);
		} else
			*((mdl_u8_t*)p+off) = *(itr++);
	}
	_ar_free(__ard, __pile, __p);
	return p;
}

void* ar_alloc(struct allocr *__ar, mdl_uint_t __bc) {
	return _ar_alloc(&__ar->d, NULL,  __bc);
}

void ar_free(struct allocr *__ar, void *__p) {
	_ar_free(&__ar->d, NULL, __p);
}

void* ar_realloc(struct allocr *__ar, void *__p, mdl_u32_t __bc) {
	return _ar_realloc(&__ar->d, NULL, __p, __bc);
}

# ifdef DEV
mdl_u8_t static inited = 0;
void* malloc(size_t __n) {
	if (!__n) return NULL;
# ifdef __DEBUG
	print("malloc.\n");
# endif
	if (!inited) {
		ar_init(&ar);
		inited = 1;
	}
	void *p = ar_alloc(&ar, __n);
	return p;
}

void free(void *__p) {
	if (!__p) return;
# ifdef __DEBUG
	print("free.\n");
# endif
	ar_free(&ar, __p);
}

# include <string.h>
void* realloc(void *__p, size_t __n) {
# ifdef __DEBUG
	print("realloc. %u, %u\n", __p, __n);
# endif
	if (!__n) return NULL;
	if (!__p) return ar_alloc(&ar, __n);
	return ar_realloc(&ar, __p, __n);
}
# endif
