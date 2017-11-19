# include <mdlint.h>
# include "print.h"
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
			if (*(itr+1) == 'd') {
				mdl_i32_t v = va_arg(args, mdl_i32_t);
				mdl_u32_t vv;
				mdl_u8_t neg = 0;
				if (v < 0) {
					vv = v-(v<<1);
					neg = 1;
				} else
					vv = v;
				if (neg)
					*(buf_itr++) = '-';
				buf_itr+= int_to_str(vv, buf_itr);
				itr++;
			} else if (*(itr+1) == 'u') {
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
			} else if (*(itr+1) == 'c') {
				*(buf_itr++) = (mdl_u8_t)va_arg(args, mdl_u32_t);
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

