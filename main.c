# include "allocr.h"
# include <stdio.h>
# include "print.h"
void print_bin(mdl_u8_t __val) {
	mdl_u8_t off = 0;
	for (;off != 8;off++) print("%u", __val>>(7-off)&0x1);
	print(" - %u\n", __val);
}

# define MA(__lval, __rval)((__lval&__rval)<<1)
# define MB(__lval, __rval)((lval|rval)^(lval&rval))
extern void pr();
extern void fr();
extern mdl_u32_t ffree;
# include <string.h>
# define PPR print("<---------->\n");pr();
int main(void) {
//	print("Goodbye\n");
//	print("%u\n", 21^21);
//	mdl_u8_t i = 0;
//	for (;i != 0xF;i++) {
//	mdl_u8_t lval = i;
///	mdl_u8_t rval = 0xFF-(i+3);
//	print_bin(MA(lval,rval));
//	print_bin(MB(lval,rval));
//	print("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
//	}

//	ar_init();
//	void *p1 = ar_alloc(1);
//	void *p2 = ar_alloc(1);
//	void *p3 = ar_alloc(1);
//	void *p4 = ar_alloc(1);
//	void *p5 = ar_alloc(1);

//	print("p1\n");
//	ar_free(p1);

//	void *p2 = ar_alloc(1);

//	print("\np2\n");
//	ar_free(p2);

//	print("\np3\n");
//	ar_free(p3);

//	print("\np4\n");
//	ar_free(p4);
//	ar_free(c);
//	ar_free(e);
/*
	print("|-------------------------------------|\n");
	fr(a);
	print("\n");

	ar_free(b);
*/
	print("|-------------------------------------|\n");
	void *a,*b,*c,*d,*e;

//	b = ar_alloc(20);
//	c = ar_alloc(30);
//	d = ar_alloc(40);
//	e = ar_alloc(50);

//	ar_free(e);
//	ar_free(d);

//	ar_free(a);
//	ar_free(b);

//	ar_free(c);
/*
	ar_free(a);
	ar_alloc(2);
	a = ar_alloc(2);

	mdl_u8_t i = 1;
	for (;i != n;i++) {
		b = ar_alloc((i+1)*10);
		ar_free(a);
		a = b;
	}
*/
//	b = ar_alloc(44);
//	c = ar_alloc(45);
//	d = ar_alloc(66);

//	a = ar_alloc(100);
//	memset(a, 0xFF, 1);
/*
	mdl_uint_t i = 0;
	for (;i != n;i++) {
		b = ar_alloc(i+1);
		ar_free(a);
		a = b;
	}

	i = 0;
	for (;i != n;i++) {
		*(((mdl_u8_t*)a+i)) = 0xFF;
	}

	ar_alloc(100);

	i = 0;
	for (;i != n;i++) {
		print("%u\n", *(((mdl_u8_t*)a+i)));
	}
*/
//	b = ar_alloc(20);
//	memset(b, 0xFF, 20);
//	c = ar_alloc(30);
//	d = ar_alloc(40);
//	e = ar_alloc(50);

//	ar_free(a);
//	ar_free(b);
//	ar_free(c);
//	ar_free(d);
//	ar_free(e);


	print("|-------------------------------------|\n");

	//print("freed: %u, alloced: %u\n", f, al);
//	ar_alloc(200);

//	a = ar_alloc(247);
//	b = ar_alloc(44);
//	c = ar_alloc(55);
//	d = ar_alloc(66);
//	e = ar_alloc(77);

//	a = ar_alloc(1);
/*
	mdl_u8_t i = 0;
	for (;i != 8;i++) {
	usleep(100000);
	b = ar_alloc((i+1)*10);
	ar_free(a);
	a = b;
	print("|--------------------|\n");
	}
*/
//	ar_free(ar_alloc(200));
	print("|-------------------------------------|\n");
	fr();
	PPR

//	ar_de_init();
//	for (;;) {
//		usleep(1000000);
//		a = ar_alloc(12);
//		ar_free(a);
//	}

//	fr(p);
///	PPR

//	printf("-----------\n");
//	fr();
//	void *p = ar_alloc(10);
//	p = ar_alloc(10);
//	p = ar_alloc(10);
//	p = ar_alloc(10);
//	ar_free(p);
}

