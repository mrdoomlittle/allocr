# include "allocr.h"
# include <unistd.h>
//# ifdef __DEBUG
#	include <stdio.h>
//# endif
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
	mdl_uint_t size;
};

void ar_init() {
	p = sbrk(0);
}

void ar_de_init() {
	brk(p);
}

void static rechain(struct blkd *__blk) {
	mdl_u32_t off = (mdl_u8_t*)__blk-(mdl_u8_t*)p;
	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->above>>1)))->below = (off<<1)|1;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->below>>1)))->above = (off<<1)|1;
}

void static unchain(struct blkd *__blk) {
	if (__blk->above&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->above>>1)))->below = __blk->below;
	if (__blk->below&0x1)
		((struct blkd*)((mdl_u8_t*)p+(__blk->below>>1)))->above = __blk->above;
}

void static unchain_f(struct blkd*);

void* ar_alloc(mdl_uint_t __bc) {
	if (ffree&0x1) {
		struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(ffree>>1));
		_lagain:
		if (blk->size >= __bc) {
			print("found free space!\n");
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
						.size = spare
					};
					blk->below = (((mdl_u8_t*)s-(mdl_u8_t*)p)<<1)|1;
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
			return (void*)((mdl_u8_t*)blk+sizeof(struct blkd));
		}

		if (!(blk->nextf&0x1)) goto _nothing;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->nextf>>1));
		goto _lagain;
	}
	_nothing:

	if ((off = (off+(__bc+sizeof(struct blkd)))) > page_c*PAGE_SIZE) {
		page_c+=(off>>SHIFT)+((off-((off>>SHIFT)*PAGE_SIZE))>0);
		brk(p+(page_c*PAGE_SIZE));
	}

	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(off-(sizeof(struct blkd)+__bc)));
	*blk = (struct blkd) {
		.above = last_blk, .below = 0,
		.nextf = 0, .prevf = 0,
		.flags = BLK_USED,
		.size = __bc
	};

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
	if (((mdl_u8_t*)__blk-(mdl_u8_t*)p) == (ffree>>1)) {
		ffree = 0;
		lastf = 0;
	}

	struct blkd *next = (struct blkd*)((mdl_u8_t*)p+(__blk->nextf>>1));
	struct blkd *prev = (struct blkd*)((mdl_u8_t*)p+(__blk->prevf>>1));

	if (__blk->nextf&0x1) {
		next->prevf = __blk->prevf;
	}

	if (__blk->prevf&0x1) {
		if (__blk->prevf == ffree) lastf = ffree;
		prev->nextf = __blk->nextf;
	}
	__blk->nextf = 0;
	__blk->prevf = 0;
}

void pr() {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(first_blk>>1));
	while(1) {
		print("state: %s\tsize: %u \tis upper? %s\tis below? %s\tnextf: %s\tprevf: %s\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size,
			(blk->above&0x1)?"yes":"no", (blk->below&0x1)?"yes":"no", (blk->nextf&0x1)?"yes":"no", (blk->prevf&0x1)?"yes":"no");
		if (!(blk->below&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->below>>1));
	}
}

# include <unistd.h>
void fr() {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)p+(ffree>>1));
	while(1) {
		usleep(1000000);
		print("state: %s, size: %u, off: %lu\n", IS_FLAG(blk->flags, BLK_USED)?"used":"free", blk->size, (mdl_u8_t*)blk-(mdl_u8_t*)p);
		if (!(blk->nextf&0x1)) break;
		blk = (struct blkd*)((mdl_u8_t*)p+(blk->nextf>>1));
	}
}

void ar_free(void *__p) {
	struct blkd *blk = (struct blkd*)((mdl_u8_t*)__p-sizeof(struct blkd)), *above, *below;
	unchain(blk);
	// search upper&lower blocks  if free and murge all into one free
	if (blk->above&0x1) {
	print("upper block located.\n");
	above = (struct blkd*)((mdl_u8_t*)p+(blk->above>>1));
	while(IS_FLAG(above->flags, BLK_FREE)) {
		print("found free space above, size: %u\n", above->size);
		unchain_f(above);
		unchain(above);
		above->size += blk->size+sizeof(struct blkd);
		blk = above;
		if (!(above->above&0x1)) break;
		above = (struct blkd*)((mdl_u8_t*)p+(above->above>>1));
	}
	}

	if (blk->below&0x1) {
	print("lower block located.\n");
	below = (struct blkd*)((mdl_u8_t*)p+(blk->below>>1));
	while(IS_FLAG(below->flags, BLK_FREE)) {
		print("found free space below, size: %u\n", below->size);
		unchain_f(below);
		unchain(below);
		blk->size += below->size+sizeof(struct blkd);
		blk->below = below->below;
		if (!(below->below&0x1)) break;
		below = (struct blkd*)((mdl_u8_t*)p+(below->below>>1));
	}
	}

	print("freed %u bytes.\n", blk->size);

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
