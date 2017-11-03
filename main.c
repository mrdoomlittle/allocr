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
mdl_uint_t int_to_str(mdl_u64_t __val, char *__buff) {
	if (!__val) {
		*(__buff++) = '0';
		*__buff = '\0';
		return 1;
	}

	mdl_u64_t unit = 1, nol = 0;
	for (;unit <= __val;unit*=10,nol++);
	unit /=10;

	char *itr = __buff;
	mdl_u64_t g = 0;
	while(itr != __buff+nol) {
		switch((__val-(g*(unit*10)))/unit) {
			case 0: *itr = '0';break;
			case 1: *itr = '1';break;
			case 2: *itr = '2';break;
			case 3: *itr = '3';break;
			case 4: *itr = '4';break;
			case 5: *itr = '5';break;
			case 6: *itr = '6';break;
			case 7: *itr = '7';break;
			case 8: *itr = '8';break;
			case 9: *itr = '9';break;
		}

		g = __val/unit;
		unit /=10;
		itr++;
	}

	*itr = '\0';
	return nol;
}

# include <stdarg.h>
void print(char const *__s, ...) {
	char static buf[1024];
	char *buf_itr = buf;
	va_list args;
	va_start(args, __s);

	char *itr = (char*)__s;
	for (;*itr != '\0';itr++) {
		if (*itr == '%') {
			if (*(itr+1) == 'u') {
				mdl_u32_t v = va_arg(args, mdl_u32_t);
				buf_itr+= int_to_str(v, buf_itr);
				itr++;
			} else if (*(itr+1) == 'l') {
				if (*(itr+2) == 'u') {
					mdl_u64_t v = va_arg(args, mdl_u64_t);
					buf_itr+= int_to_str(v, buf_itr);
					itr++;
				}
				itr++;
			} else if (*(itr+1) == 's') {
				char *s = (char*)va_arg(args, char const*);
				for (;*s != '\0';s++) *(buf_itr++) = *s;
				itr++;
			}
			itr++;
		}
		*(buf_itr++) = *itr;
	}

	va_end(args);
	*buf_itr = '\0';
	sprint(buf);
}

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

	ar_init();
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

	void *a,*b,*c;

for(;;) {
	a = ar_alloc(12);
	b = ar_alloc(12);
	c = ar_alloc(12);
	ar_free(a);
	ar_free(b);
	ar_free(c);
}

	print("--------------\n");
	fr(a);
	PPR
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
