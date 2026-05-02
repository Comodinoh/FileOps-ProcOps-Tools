#ifndef __PROCS_H
#define __PROCS_H
#include <time.h>
#include <stdint.h>
#include <limits.h>

#define DB_PROCS_SIGNATURE "PROC"

typedef struct  __attribute__((packed)){
    pid_t pid; 
    pid_t ppid; 

    char state[4];
    char comm[16];
    char cmdline[PATH_MAX/256];
    uint64_t rss;
    char rss_source[8];
    uint64_t cpu_time;
} db_procs_row;

#endif
