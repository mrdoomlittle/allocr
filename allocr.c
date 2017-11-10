# include "allocr.h"
# include <unistd.h>
//# ifdef __DEBUG
#	include <stdio.h>
//# endif
# include <unistd.h>
# define SHIFT 2
# define PAGE_SIZE (1<<SHIFT)
# define BLK_USED (1<<7)
# define BLK_FREE (1<<6)
# define IS_FLAG(__flags, __flag) ((__flags&__flag) == __flag)

struct allocr ar = {
	.p = NULL,
	.off = 0, .page_c = 0,
	.first_blk = 0, .last_blk = 0,
	.ffree = 0, .lastf = 0,
};

struct blkd {
	mdl_u32_t above, below;
	mdl_u32_t nextf, prevf;
	mdl_u8_t flags;
	mdl_u32_t size;
};

void ppr(mdl_u8_t __a) {
	print("%u\n", __a);
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

void ar_init(struct allocr *__ar) {
	if ((__ar->p = sbrk(0)) < 0) {
# ifdef __DEBUG
		print("sbrk error.\n");
# endif
	}
}

void ar_de_init(struct allocr *__ar) {
	if (brk(__ar->p) < 0) {
# ifdef __DEBUG
		print("brk error.\n");
# endif
	}
}

void static rechain(struct allocr *__ar, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__ar->p;
	if (off < (__ar->first_blk>>1) || !(__ar->first_blk&0x1)) __ar->first_blk = (off<<1)|1;
	if (off > (__ar->last_blk>>1) || !(__ar->last_blk&0x1)) __ar->last_blk = (off<<1)|1;
	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->above>>1)))->below = (off<<1)|1;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->below>>1)))->above = (off<<1)|1;
}

void static unchain(struct allocr *__ar, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__ar->p;
	if (off == __ar->first_blk>>1) __ar->first_blk = __blk->below;
	if (off == __ar->last_blk>>1) __ar->last_blk = __blk->above;
	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->above>>1)))->below = __blk->below;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->below>>1)))->above = __blk->above;
}

void static unchain_f(struct allocr*, struct blkd*);
void* ar_alloc(struct allocr *__ar, mdl_u32_t __bc) {
	if (__ar->ffree&0x1) {
		struct blkd *blk = (struct blkd*)((mdl_u8_t*)__ar->p+(__ar->ffree>>1));
		_lagain:
		if (blk->size >= __bc) {
# ifdef __DEBUG
			print("found free space!\n");
# endif
			unchain_f(__ar, blk);
			if (blk->size > __bc) {
				mdl_u32_t spare = blk->size-__bc;
				if (spare > sizeof(struct blkd)) {
					blk->size = __bc;
					struct blkd *s = (struct blkd*)((mdl_u8_t*)blk+(sizeof(struct blkd)+__bc));
					*s = (struct blkd) {
						.above = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1, .below = blk->below,
						.nextf = 0, .prevf = 0,
						.flags = BLK_FREE,
						.size = spare-sizeof(struct blkd)
					};
					mdl_u32_t s_off = (mdl_u8_t*)s-(mdl_u8_t*)__ar->p;
					blk->below = (s_off<<1)|1;
					if (s->below&0x1) ((struct blkd*)((mdl_u8_t*)__ar->p+(s->below>>1)))->above = blk->below;
					if (IS_FLAG(blk->flags, BLK_FREE))
						blk->flags ^= BLK_FREE;
					blk->flags |= BLK_USED;

					ar_free(__ar, (mdl_u8_t*)s+sizeof(struct blkd));
					goto _ret;
				}
			}

			if (IS_FLAG(blk->flags, BLK_FREE))
				blk->flags ^= BLK_FREE;
			blk->flags |= BLK_USED;
			_ret:
			rechain(__ar, blk);
			return (void*)((mdl_u8_t*)blk+sizeof(struct blkd));
		}

		if (!(blk->nextf&0x1)) goto _nothing;
		blk = (struct blkd*)((mdl_u8_t*)__ar->p+(blk->nextf>>1));
		goto _lagain;
	}
	_nothing:
	if ((__ar->off = (__ar->off+(__bc+sizeof(struct blkd)))) > __ar->page_c*PAGE_SIZE) {
		__ar->page_c += (__ar->off>>SHIFT)+((__ar->off-((__ar->off>>SHIFT)*PAGE_SIZE))>0);
		if (brk(__ar->p+(__ar->page_c*PAGE_SIZE)) < 0) {
# ifdef __DEBUG
			print("brk error.\n");
# endif
		}
# ifdef __DEBUG
		print("size: %u\n", __ar->page_c*PAGE_SIZE);
# endif
	}

	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__ar->p+(__ar->off-(sizeof(struct blkd)+__bc)));
	*blk = (struct blkd) {
		.above = __ar->last_blk, .below = 0,
		.nextf = 0, .prevf = 0,
		.flags = BLK_USED,
		.size = __bc
	};
# ifdef __DEBUG
	print("off: %u, last_blk: %u\n", __ar->off-(sizeof(struct blkd)+__bc), __ar->last_blk>>1);
# endif
	if (!(__ar->first_blk&0x1)) {
		__ar->first_blk = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;
	}

	if (__ar->last_blk&0x1) {
		((struct blkd*)((mdl_u8_t*)__ar->p+(__ar->last_blk>>1)))->below =
			(((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;
	}

	__ar->last_blk = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;

///	print("---> %u, %u, %u\n", off, __bc, sizeof(struct blkd));
//	print("alloc\n");
	return (void*)((mdl_u8_t*)__ar->p+(__ar->off-__bc));
}

void unchain_f(struct allocr *__ar, struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)__ar->p;
	if (off == (__ar->ffree>>1)) __ar->ffree = __blk->nextf;
	if (off == (__ar->lastf>>1)) __ar->lastf = __blk->prevf;
	if (__blk->nextf&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->nextf>>1)))->prevf = __blk->prevf;
	if (__blk->prevf&0x1)
		((struct blkd*)((mdl_u8_t*)__ar->p+(__blk->prevf>>1)))->nextf = __blk->nextf;
	__blk->nextf = 0;
	__blk->prevf = 0;
}

void pr(struct allocr *__ar) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__ar->p+(__ar->first_blk>>1));
	if (__ar->first_blk&0x1) {
	while(1) {
		print("state: %s\tsize: %u \tis upper? %s{%u}\tis below? %s{%u}\tnextf: %s\tprevf: %s, off: %u\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size,
			(blk->above&0x1)?"yes":"no", blk->above>>1, (blk->below&0x1)?"yes":"no", blk->below>>1, (blk->nextf&0x1)?"yes":"no", (blk->prevf&0x1)?"yes":"no", (mdl_u8_t*)blk-(mdl_u8_t*)__ar->p);
		if (!(blk->below&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)__ar->p+(blk->below>>1));
	}}
}

# include <unistd.h>
void fr(struct allocr *__ar) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__ar->p+(__ar->ffree>>1));
	if (__ar->ffree&0x1) {
	while(1) {
		print("state: %s, size: %u, off: %lu\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size, (mdl_u8_t*)blk-(mdl_u8_t*)__ar->p);
		if (!(blk->nextf&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)__ar->p+(blk->nextf>>1));
	}}
}

void ar_free(struct allocr *__ar, void *__p) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)), *above, *below;
	unchain(__ar, blk);
	// search upper&lower blocks  if free and murge all into one free
	if (blk->above&0x1) {
# ifdef __DEBUG
	print("upper block located.\n");
# endif
	above = (struct blkd*)((mdl_u8_t*)__ar->p+(blk->above>>1));
	while(IS_FLAG(above->flags, BLK_FREE)) {
# ifdef __DEBUG
		print("found free space above, size: %u\n", above->size);
# endif
		unchain_f(__ar, above);
		unchain(__ar, above);
		above->size += blk->size+sizeof(struct blkd);
		blk = above;
		if (!(above->above&0x1)) break;
		above = (struct blkd*)((mdl_u8_t*)__ar->p+(above->above>>1));
	}
	}

	if (blk->below&0x1) {
# ifdef ___DEBUG
	print("lower block located.\n");
# endif
	below = (struct blkd*)((mdl_u8_t*)__ar->p+(blk->below>>1));
	while(IS_FLAG(below->flags, BLK_FREE)) {
# ifdef __DEBUG
		print("found free space below, size: %u\n", below->size);
# endif
		unchain_f(__ar, below);
		unchain(__ar, below);
		blk->size += below->size+sizeof(struct blkd);
		blk->below = below->below;
		if (!(below->below&0x1)) break;
		below = (struct blkd*)((mdl_u8_t*)__ar->p+(below->below>>1));
	}
	}
# ifdef __DEBUG
	print("freed %u bytes.\n", blk->size);
# endif
	mdl_u32_t off;
	if ((off = ((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)) == (__ar->off-(blk->size+sizeof(struct blkd))) && __ar->page_c > 1) {
		__ar->page_c = (off>>SHIFT)+((off-((off>>SHIFT)*PAGE_SIZE))>0);
		__ar->off = off;
		if (brk(__ar->p+(__ar->page_c*PAGE_SIZE)) < 0) {
# ifdef __DEBUG
			print("brk error.\n");
# endif
		}
# ifdef __DEBUG
		print("....\n");
# endif
		return;
	}
# ifdef __DEBUG
	print("----| %u, %u, %u\n", __ar->last_blk, off, __ar->page_c);
# endif


	if (__ar->lastf&0x1) {
		blk->prevf = __ar->lastf;
		((struct blkd*)((mdl_u8_t*)__ar->p+(__ar->lastf>>1)))->nextf = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;
	}

	if (!(__ar->ffree&0x1))
		__ar->ffree = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;

	__ar->lastf = (((mdl_u8_t*)blk-(mdl_u8_t*)__ar->p)<<1)|1;
	if (IS_FLAG(blk->flags, BLK_USED)) blk->flags ^= BLK_USED;
	blk->flags |= BLK_FREE;
	rechain(__ar, blk);
}

mdl_u16_t static mutex = 0x0;
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
# ifdef __EXPEMENT
	mutex_lock(&mutex);
# endif
	void *p = ar_alloc(&ar, __n);
# ifdef __EXPEMENT
	mutex_unlock(&mutex);
# endif
	return p;
}

void free(void *__p) {
	if (!__p) return;
# ifdef __DEBUG
	print("free.\n");
# endif
# ifdef __EXPEMENT
	mutex_lock(&mutex);
# endif
	ar_free(&ar, __p);
# ifdef __EXPEMENT
	mutex_unlock(&mutex);
# endif
}

# include <string.h>
void* realloc(void *__p, size_t __n) {
# ifdef __DEBUG
	print("realloc. %u, %u\n", __p, __n);
# endif
	if (!__n) return NULL;
	void *p = malloc(__n);
	if (!__p) return p;
	mdl_u32_t size = ((struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)))->size;
	if (__n < size) size = __n;

	mdl_u8_t *itr = (mdl_u8_t*)__p;
	while(itr != (mdl_u8_t*)__p+size) {
		mdl_u32_t off = itr-(mdl_u8_t*)__p;
		if ((size-off) > sizeof(mdl_u64_t)) {
			*((mdl_u64_t*)((mdl_u8_t*)p+off)) = *(mdl_u64_t*)itr;
			itr+=sizeof(mdl_u64_t);
		} else
			*((mdl_u8_t*)p+off) = *(itr++);
	}

 	free(__p);
	return p;
}
