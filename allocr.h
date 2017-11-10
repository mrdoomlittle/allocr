# ifndef __mdl__allocr__h
# define __mdl__allocr__h
# include <mdlint.h>
# include "print.h"
//# define __EXPEMENT
//# define __DEBUG
struct allocr {
	void *p ;
	mdl_u32_t off, page_c;
	mdl_u32_t first_blk, last_blk;
	mdl_u32_t ffree, lastf;
};

void ar_init(struct allocr*);
void ar_de_init(struct allocr*);
void* ar_alloc(struct allocr*, mdl_u32_t);
void ar_free(struct allocr*, void*);
# endif /*__mdl__allocr__h*/
