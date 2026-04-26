#ifndef __PROCS_H
#define __PROCS_H
#include <time.h>

#define DB_PROCS_SIGNATURE "PROC"

typedef struct {
   pid_t pid; 
   pid_t ppid; 

   size_t rss;
} db_procs_row;

#endif
