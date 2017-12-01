# ifndef __mdl__allocr__h
# define __mdl__allocr__h
# include <mdlint.h>
void extern ar_prepare();
void extern ar_cleanup();
void extern* ar_alloc(mdl_uint_t);
void extern ar_free(void*);
void extern* ar_realloc(void*, mdl_uint_t);
void extern pr();
void extern fr();
# endif /*__mdl__allocr__h*/
