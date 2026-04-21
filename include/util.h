#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>

#define ASSERT(x, ...) do { if(!(x)) {fprintf(stderr, __VA_ARGS__);} } while(0)
#define ERRCHECK(x, ...) do { if((x) == -1) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL); exit(1); } } while(0)
#define ERRCHECKNULL(x, ...) do { if((x) == NULL) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL); exit(1); } } while(0)


#endif
