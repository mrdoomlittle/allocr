# include <mdl/allocr.h>
# include "print.h"
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
	while(i != __c) {
		size+=__by;
		p = ar_realloc(p, size);
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

int main(void) {
	ar_prepare();
//	tst5();
//	tst0(20, 20);
//	tst1(20, 40);
//	tst2();
	tst3(100, 20, 20);
///	tst4(200, 231);
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
