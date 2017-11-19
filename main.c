# include "allocr.h"
# include <stdio.h>
# include "print.h"
extern struct allocr ar;
void pr(struct allocrd*);
void fr(struct allocrd*);
void pa(struct arena*);

struct arena* create_arena(struct allocr*);
void* aa_blk(struct allocr*, struct arena*, mdl_uint_t);
void af_blk(struct allocr*, struct arena*, void*);
# include <time.h>
# include <sys/resource.h>
# include <sys/mman.h>
# include <sys/unistd.h>
void pr_data(void*, mdl_uint_t);

struct random_s {
	mdl_u64_t a,b,c,d,e;
} __attribute__((packed));

int main() {

	print("allocr init.\n");
	ar_init(&ar);

	mdl_u8_t x = 0;
	struct random_s *p1 = ar_alloc(&ar, sizeof(struct random_s));
	struct random_s *p2 = ar_alloc(&ar, sizeof(struct random_s));
	struct random_s *p3 = ar_alloc(&ar, sizeof(struct random_s));
	struct random_s *p4 = ar_alloc(&ar, sizeof(struct random_s));
	*p1 = (struct random_s) {.a = ~(mdl_u64_t)0, .b = ~(mdl_u64_t)0, .c = ~(mdl_u64_t)0, .d = ~(mdl_u64_t)0, .e = ~(mdl_u64_t)0};
	*p2 = (struct random_s) {.a = ~(mdl_u64_t)0, .b = ~(mdl_u64_t)0, .c = ~(mdl_u64_t)0, .d = ~(mdl_u64_t)0, .e = ~(mdl_u64_t)0};
	*p3 = (struct random_s) {.a = ~(mdl_u64_t)0, .b = ~(mdl_u64_t)0, .c = ~(mdl_u64_t)0, .d = ~(mdl_u64_t)0, .e = ~(mdl_u64_t)0};
	*p4 = (struct random_s) {.a = ~(mdl_u64_t)0, .b = ~(mdl_u64_t)0, .c = ~(mdl_u64_t)0, .d = ~(mdl_u64_t)0, .e = ~(mdl_u64_t)0};

//	void *p2 = ar_alloc(&ar, 240);
//	void *p3 = ar_alloc(&ar, 688);
//	void *p4 = ar_alloc(&ar, 1);
//	void *p5 = ar_alloc(&ar, 1);

	ar_free(&ar, p1);
	ar_free(&ar, p2);
	ar_free(&ar, p3);
	ar_free(&ar, p4);
//	ar_free(&ar, p5);


//	struct rlimit rl = {
//		.rlim_cur = 0,
//		.rlim_max = 0
//	};
//	getrlimit(RLIMIT_DATA, &rl);
//	print("cur: %d, max: %d\n", rl.rlim_cur, rl.rlim_max);


	pr(&ar.d);
	fr(&ar.d);
/*
	struct arena *a1 = create_arena(&ar);
	struct arena *a2 = create_arena(&ar);

	struct timespec begin, end;
	clock_gettime(CLOCK_MONOTONIC, &begin);
	aa_blk(&ar, a1, 20);
	clock_gettime(CLOCK_MONOTONIC, &end);
	print("time: %lu\n", (end.tv_nsec-begin.tv_nsec)+((end.tv_sec-begin.tv_sec)*1000000000));

	pa(a1);
	pa(a2);
*/
}
