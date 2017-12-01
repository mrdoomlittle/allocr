# include <mdl/allocr.h>
# include "print.h"
void mem_set(mdl_u8_t __val, void *__p, mdl_uint_t __n) {
	mdl_u8_t *itr = (mdl_u8_t*)__p;
	while(itr != (mdl_u8_t*)__p+__n) *(itr++) = __val;
}

mdl_u8_t mem_cmp(mdl_u8_t __val, void *__p, mdl_uint_t __n) {
	mdl_u8_t *itr = (mdl_u8_t*)__p;
    while(itr != (mdl_u8_t*)__p+__n)
		if (*(itr++) != __val) return 1;
	return 0;
}

void tst0(mdl_uint_t __c, mdl_uint_t __size) {
	void *p1 = ar_alloc(__size);
	mdl_uint_t i = 0;
	while(i++ != __c) {
		ar_free(p1);
		ar_alloc(__size);
	}
	ar_free(p1);
}

void tst1(mdl_uint_t __c, mdl_uint_t __begin, mdl_uint_t __by) {
	void **p = ar_alloc(__c*sizeof(void*));
	mdl_uint_t i = 0, size = __begin;

	// alloc
	while(i != __c) {
		p[i] = ar_alloc(size);
		i++;
		size+= __by;
	}

	i = 0;
	while(i != __c) {
		ar_free(p[i]);
		i++;
	}

	ar_free(p);
}

void tst2() {
	void *p1 = ar_alloc(20);
	void *p2 = ar_alloc(80);
	void *p3 = ar_alloc(20);

	ar_free(p1);
	ar_free(p2);

	print("\n\n");
	p1 = ar_alloc(10);


	ar_free(p1);
	ar_free(p3);
}

void tst3(mdl_uint_t __c, mdl_uint_t __begin, mdl_uint_t __by) {
	mdl_uint_t i = 0, size = __begin;
	void *p = ar_alloc(size);
	mem_set(0xFF, p, size);
	while(i != __c) {
		if (mem_cmp(0xFF, p, size)) {
			print("bad memory.\n");
		}
		size+=__by;
		p = ar_realloc(p, size);
		mem_set(0xFF, p, size);
		i++;
	}

	ar_free(p);
}

void tst4(mdl_uint_t __c, mdl_uint_t __size) {
	mdl_uint_t i = 0;
	void *p = ar_alloc(__size);
	while(i != __c) {
		void *t = ar_alloc(__size);
		ar_free(p);
		p = t;
		i++;
	}
}

void tst5() {
	void *p1 = ar_alloc(1);
	void *p2 = ar_alloc(4);
	void *p3 = ar_alloc(8);
	void *p4 = ar_alloc(16);
	void *p5 = ar_alloc(32);
	void *p6 = ar_alloc(64);
	void *p7 = ar_alloc(128);
	void *p8 = ar_alloc(200);
}

struct fill {
	mdl_u64_t a,b,c,d,e,f,g,h;
};

void tst6() {
	struct fill *p = (struct fill*)ar_alloc(sizeof(struct fill));
	if (p == (void*)0) return;
	*p = (struct fill) {
		.a = (mdl_u32_t)~0, .b = (mdl_u32_t)~0, .c = (mdl_u32_t)~0, .d = (mdl_u32_t)~0,
		.e = (mdl_u32_t)~0, .f = (mdl_u32_t)~0, .g = (mdl_u32_t)~0, .h = (mdl_u32_t)~0,
	};

	p = (struct fill*)ar_realloc(p, 170);

	print("a: %lu\n", p->a);
	print("b: %lu\n", p->b);
	print("c: %lu\n", p->c);
	print("d: %lu\n", p->d);
	print("e: %lu\n", p->e);
	print("f: %lu\n", p->f);
	print("g: %lu\n", p->g);
	print("h: %lu\n", p->h);
}

void tst7() {
	void *p1 = ar_alloc(20);
	void *p2 = ar_alloc(20);
	void *p3 = ar_alloc(20);
	print("p1-p2, off: %u\n", p2-p1);
	print("p2-p3, off: %u\n", p3-p2);

}

mdl_u64_t static rv = 0;
mdl_u64_t static ret;
mdl_u64_t rand() {
	rv+=212+((ret = (rv<<1)&(~rv)));
	return ret;
}

void tst8(mdl_uint_t __c) {
	mdl_uint_t i = 0;
	ar_free(ar_alloc(200));

	mdl_uint_t size;
	void *last = (void*)0;
	while(i != __c) {
		if (last != (void*)0)
			ar_free(last);
		last = ar_alloc((size = (rand()%80)));
		ar_alloc(10);
		//mem_set(i, last, size);
		i++;
	}
	ar_free(last);
}

int main(void) {
	ar_prepare();


	ar_alloc(100);
	ar_alloc(200);
	ar_alloc(300);
	ar_alloc(400);

	ar_alloc(2000);
//	void *p = ar_alloc(100);
//	ar_free(p);
//	void *p1 = ar_alloc(128);
//	p1 = ar_realloc(p1, 800);
//	void *p3 = aras_alloc(20);
//	void *p4 = ar_alloc(20);

//	ar_free(p1);
//	ar_free(p3);
//	ar_free(p4);

	pr();
	ar_cleanup();
}
