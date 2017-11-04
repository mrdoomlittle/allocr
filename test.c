//# include <malloc.h>
# include <string.h>
# include <stdio.h>
# include <mdlint.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
extern void pr();
extern void fr();
int main(int __argc, char const *__argv[]) {
	printf("-------------------------------------------------------\n");
	char const **itr = __argv;
	for (;itr != __argv+__argc;itr++) {
		printf("%s\n", *itr);
	}
	free(malloc(800));

	char *buf = (char*)malloc(200);
	while(1) {
	mdl_uint_t n = read(fileno(stdin), buf, 200);
	*(buf+(n-1)) = '\0';
	char *s = (char*)malloc(n+1);
	strcpy(s, buf);
	printf("%s\n", s);
	if (!strcmp(s, "exit")) break;
	free(s);
	}

	fr();
	pr();
/*
	mdl_u8_t *p = (mdl_u8_t*)malloc(200);
	memset(p, 0xFF, 200);


	p = realloc(p, 400);
	mdl_uint_t i = 0;
	while(i != 400) {
		printf("%u : %u\n", i, p[i]);
		i++;
	}
*/
}
