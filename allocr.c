# include "allocr.h"
# include <unistd.h>
//# ifdef __DEBUG
#	include <stdio.h>
//# endif
# include <unistd.h>
# define SHIFT 2
# define PAGE_SIZE (1<<SHIFT)
void *p = NULL;
mdl_u32_t off = 0, page_c = 0;
# define BLK_USED (1<<7)
# define BLK_FREE (1<<6)
# define IS_FLAG(__flags, __flag) ((__flags&__flag) == __flag)
mdl_u32_t first_blk = 0, last_blk = 0;
mdl_u32_t ffree = 0, lastf = 0;

struct blkd {
	mdl_u32_t above, below;
	mdl_u32_t nextf, prevf;
	mdl_u8_t flags;
	mdl_u32_t size;
};

void ar_init() {
	if ((p = sbrk(0)) < 0) {
# ifdef __DEBUG
		print("sbrk error.\n");
# endif
	}
}

void ar_de_init() {
	if (brk(p) < 0) {
# ifdef __DEBUG
		print("brk error.\n");
# endif
	}
}

void static rechain(struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)p;
	if (off < (first_blk>>1) || !(first_blk&0x1)) {
		first_blk = (off<<1)|1;
	}

	if (off > (last_blk>>1) || !(last_blk&0x1)) {
		last_blk = (off<<1)|1;
	}

	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->above>>1)))->below = (off<<1)|1;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->below>>1)))->above = (off<<1)|1;
}

void static unchain(struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)p;
	if (off == first_blk>>1) {
		first_blk = __blk->below;
	}
	if (off == last_blk>>1) {
		last_blk = __blk->above;
	}

	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->above>>1)))->below = __blk->below;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->below>>1)))->above = __blk->above;
}

void static unchain_f(struct blkd*);

void* ar_alloc(mdl_u32_t __bc) {
	if (ffree&0x1) {
		struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(ffree>>1));
		_lagain:
		if (blk->size >= __bc) {
# ifdef __DEBUG
			print("found free space!\n");
# endif
			unchain_f(blk);
			if (blk->size > __bc) {
				mdl_u32_t spare = blk->size-__bc;
				if (spare > sizeof(struct blkd)) {
					blk->size = __bc;
					struct blkd *s = (struct blkd*)((mdl_u8_t*)blk+(sizeof(struct blkd)+__bc));
					*s = (struct blkd) {
						.above = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1, .below = blk->below,
						.nextf = 0, .prevf = 0,
						.flags = BLK_FREE,
						.size = spare-sizeof(struct blkd)
					};
					mdl_u32_t s_off = (mdl_u8_t*)s-(mdl_u8_t*)p;
					blk->below = (s_off<<1)|1;
					if (s->below&0x1)
						((struct blkd*)((mdl_u8_t*)p+(s->below>>1)))->above = blk->below;
					if (IS_FLAG(blk->flags, BLK_FREE))
						blk->flags ^= BLK_FREE;
					blk->flags |= BLK_USED;

					ar_free((mdl_u8_t*)s+sizeof(struct blkd));
					goto _ret;
				}
			}

			if (IS_FLAG(blk->flags, BLK_FREE))
				blk->flags ^= BLK_FREE;
			blk->flags |= BLK_USED;
			_ret:
			rechain(blk);
			return (void*)((mdl_u8_t*)blk+sizeof(struct blkd));
		}

		if (!(blk->nextf&0x1)) goto _nothing;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->nextf>>1));
		goto _lagain;
	}
	_nothing:
	if ((off = (off+(__bc+sizeof(struct blkd)))) > page_c*PAGE_SIZE) {
		page_c+=(off>>SHIFT)+((off-((off>>SHIFT)*PAGE_SIZE))>0);
		if (brk(p+(page_c*PAGE_SIZE)) < 0) {
# ifdef __DEBUG
			print("brk error.\n");
# endif
		}
# ifdef __DEBUG
		print("size: %u\n", page_c*PAGE_SIZE);
# endif
	}

	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(off-(sizeof(struct blkd)+__bc)));
	*blk = (struct blkd) {
		.above = last_blk, .below = 0,
		.nextf = 0, .prevf = 0,
		.flags = BLK_USED,
		.size = __bc
	};
# ifdef __DEBUG
	print("off: %u, last_blk: %u\n", off-(sizeof(struct blkd)+__bc), last_blk>>1);
# endif
	if (!(first_blk&0x1)) {
		first_blk = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;
	}

	if (last_blk&0x1) {
		((struct blkd*)((mdl_u8_t*)p+(last_blk>>1)))->below =
			(((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;
	}

	last_blk = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;

///	print("---> %u, %u, %u\n", off, __bc, sizeof(struct blkd));
//	print("alloc\n");
	return (void*)((mdl_u8_t*)p+(off-__bc));
}

void unchain_f(struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)p;
	if (off == (ffree>>1))
		ffree = __blk->nextf;
	if (off == (lastf>>1))
		lastf = __blk->prevf;

	struct blkd *next = (struct blkd*)((mdl_u8_t*)p+(__blk->nextf>>1));
	struct blkd *prev = (struct blkd*)((mdl_u8_t*)p+(__blk->prevf>>1));

	if (__blk->nextf&0x1)
		next->prevf = __blk->prevf;
	if (__blk->prevf&0x1)
		prev->nextf = __blk->nextf;

	__blk->nextf = 0;
	__blk->prevf = 0;
}

void pr() {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(first_blk>>1));
	if (first_blk&0x1) {
	while(1) {
		print("state: %s\tsize: %u \tis upper? %s{%u}\tis below? %s{%u}\tnextf: %s\tprevf: %s, off: %u\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size,
			(blk->above&0x1)?"yes":"no", blk->above>>1, (blk->below&0x1)?"yes":"no", blk->below>>1, (blk->nextf&0x1)?"yes":"no", (blk->prevf&0x1)?"yes":"no", (mdl_u8_t*)blk-(mdl_u8_t*)p);
		if (!(blk->below&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->below>>1));
	}}
}

# include <unistd.h>
void fr() {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(ffree>>1));
	if (ffree&0x1) {
	while(1) {
		usleep(1000000);
		print("state: %s, size: %u, off: %lu\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size, (mdl_u8_t*)blk-(mdl_u8_t*)p);
		if (!(blk->nextf&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->nextf>>1));
	}}
}

void ar_free(void *__p) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)), *above, *below;
	unchain(blk);
	// search upper&lower blocks  if free and murge all into one free
	if (blk->above&0x1) {
# ifdef __DEBUG
	print("upper block located.\n");
# endif
	above = (struct blkd*)((mdl_u8_t*)p+(blk->above>>1));
	while(IS_FLAG(above->flags, BLK_FREE)) {
# ifdef __DEBUG
		print("found free space above, size: %u\n", above->size);
# endif
		unchain_f(above);
		unchain(above);
		above->size += blk->size+sizeof(struct blkd);
		blk = above;
		if (!(above->above&0x1)) break;
		above = (struct blkd*)((mdl_u8_t*)p+(above->above>>1));
	}
	}

	if (blk->below&0x1) {
# ifdef ___DEBUG
	print("lower block located.\n");
# endif
	below = (struct blkd*)((mdl_u8_t*)p+(blk->below>>1));
	while(IS_FLAG(below->flags, BLK_FREE)) {
# ifdef __DEBUG
		print("found free space below, size: %u\n", below->size);
# endif
		unchain_f(below);
		unchain(below);
		blk->size += below->size+sizeof(struct blkd);
		blk->below = below->below;
		if (!(below->below&0x1)) break;
		below = (struct blkd*)((mdl_u8_t*)p+(below->below>>1));
	}
	}
# ifdef __DEBUG
	print("freed %u bytes.\n", blk->size);
# endif
	if (lastf&0x1) {
		blk->prevf = lastf;
		((struct blkd*)((mdl_u8_t*)p+(lastf>>1)))->nextf = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;
	}

	if (!(ffree&0x1))
		ffree = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;

	lastf = (((mdl_u8_t*)blk-(mdl_u8_t*)p)<<1)|1;
	if (IS_FLAG(blk->flags, BLK_USED)) blk->flags ^= BLK_USED;
	blk->flags |= BLK_FREE;
	rechain(blk);
}

mdl_u8_t static inited = 0;
void* malloc(size_t __n) {
	if (!__n) return NULL;
# ifdef __DEBUG
	print("malloc.\n");
# endif
	if (!inited) {
		ar_init();
		inited = 1;
	}
	void *p = ar_alloc(__n);
	return p;
}

void free(void *__p) {
	if (!__p) return;
# ifdef __DEBUG
	print("free.\n");
# endif
	ar_free(__p);
}

# include <string.h>
void* realloc(void *__p, size_t __n) {
# ifdef __DEBUG
	print("realloc. %u, %u\n", __p, __n);
# endif
	if (!__n) return NULL;
	void *p = malloc(__n&0xFF);
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
