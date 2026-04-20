#ifndef __UTIL_H
#define __UTIL_H

#include <stdio.h>

#define ERRCHECK(x, ...) do { if((x) == -1) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL); exit(1); } } while(0)

#endif
