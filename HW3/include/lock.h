#ifndef __LOCK_H
#define __LOCK_H

#include <stdlib.h>
#include <stdbool.h>

bool db_lock_read_region(int fd, size_t offset, size_t size);
void db_lock_read_region_wait(int fd, size_t offset, size_t size);

bool db_lock_write_region(int fd, size_t offset, size_t size);
void db_lock_write_region_wait(int fd, size_t offset, size_t size);

bool db_unlock_region(int fd, size_t offset, size_t size);
void db_unlock_region_wait(int fd, size_t offset, size_t size);

#endif
