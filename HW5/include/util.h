#ifndef __UTIL_H
#define __UTIL_H

#include "types.h"
#include <stdio.h>

#ifndef NDEBUG

#define ASSERT(x, ...) do { if(!(x)) {fprintf(stderr, __VA_ARGS__);} } while(0)
#define ERRCHECK(x, ...) do { if((x) == -1) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL); exit(1); } } while(0)
#define ERRCHECKNULL(x, ...) do { if((x) == NULL) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL); exit(1); } } while(0)

#else

#define ASSERT(x, ...) (void)(x)
#define ERRCHECK(x, ...) (void)(x)
#define ERRCHECKNULL(x, ...)(void)(x)

#endif



#endif
