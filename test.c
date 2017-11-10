//# include <malloc.h>
# define _GNU_SOURCE
# include <string.h>
# include <stdio.h>
# include <mdlint.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sched.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <stdatomic.h>
# include <pthread.h>
extern struct allocr ar;
extern void pr(struct allocr*);
extern void fr(struct allocr*);
extern void mutex_lock(mdl_u16_t*);
extern void mutex_unlock(mdl_u16_t*);
mdl_u16_t mutex = 0x0;
mdl_u32_t val;
// testing stuff
int thr(void *__arg) {
	mdl_u32_t i = 0;
	while(i < 2222) {
//		free(malloc(1));
		mutex_lock(&mutex);
		val++;
		mutex_unlock(&mutex);
		i++;
	}
	printf("finished.\n");
}

int main(int __argc, char const *__argv[]) {
	printf("-------------------------------------------------------\n");
	val = 0x0;
//while(1) {
	mutex_lock(&mutex);
	mutex_unlock(&mutex);
	printf("%u\n", (mdl_u8_t)mutex);
//}
//	return 0;
/*
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
*/
	void *sp = malloc(2048);
	pid_t pid = clone(&thr, (mdl_u8_t*)sp+2048, CLONE_VM|CLONE_SIGHAND|CLONE_FILES|CLONE_FS|SIGCHLD, NULL);
	mdl_u32_t i = 0;
	while(i < 2222) {
//		free(malloc(1));
		mutex_lock(&mutex);
        val++;
		mutex_unlock(&mutex);
		i++;
	}

	printf("----\n");
	waitpid(pid, NULL, 0);
	printf("%d\n", val);

	free(sp);
	fr(&ar);
	pr(&ar);
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
