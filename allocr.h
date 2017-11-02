# ifndef __mdl__allocr__h
# define __mdl__allocr__h
# include <mdlint.h>
# include "print.h"
void ar_init();
void ar_de_init();
void* ar_alloc(mdl_uint_t);
void ar_free(void*);
# endif /*__mdl__allocr__h*/
