#include "lock.h"

#include <util.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>

bool db_lock_write_region(int fd, size_t offset, size_t size) {
    static struct flock lock = {
        .l_whence = SEEK_CUR,
        .l_type = F_WRLCK
    };

    lock.l_start = offset;
    lock.l_len = size;

    if(fcntl(fd, F_SETLK, &lock) != -1) {
        return true;
    }

    return false;
}

bool db_lock_read_region(int fd, size_t offset, size_t size) {
    static struct flock lock = {
        .l_whence = SEEK_CUR,
        .l_type = F_RDLCK
    };

    lock.l_start = offset;
    lock.l_len = size;

    if(fcntl(fd, F_SETLK, &lock) != -1) {
        return true;
    }

    return false;
}

void db_lock_write_region_wait(int fd, size_t offset, size_t size) {
    static struct flock lock = {
        .l_whence = SEEK_CUR,
        .l_type = F_WRLCK
    };

    lock.l_start = offset;
    lock.l_len = size;

    ERRCHECK(fcntl(fd, F_SETLKW, &lock), "Could not set wait write lock for region: offset = %zu, size = %zu\n", offset, size);
}

void db_lock_read_region_wait(int fd, size_t offset, size_t size) {
    static struct flock lock = {
        .l_whence = SEEK_CUR,
        .l_type = F_RDLCK
    };

    lock.l_start = offset;
    lock.l_len = size;

    ERRCHECK(fcntl(fd, F_SETLKW, &lock), "Could not set wait read lock for region: offset = %zu, size = %zu\n", offset, size);
}
bool db_unlock_region(int fd, size_t offset, size_t size) {
    static struct flock lock =  {
        .l_whence = SEEK_CUR,
        .l_type = F_UNLCK
    };


    lock.l_start = offset;
    lock.l_len = size;

    if(fcntl(fd, F_SETLK, &lock) != -1) {
        return true;
    }

    return false;
}

void db_unlock_region_wait(int fd, size_t offset, size_t size) {
    struct flock lock = {
        .l_whence = SEEK_CUR,
        .l_type = F_UNLCK,
        .l_start = offset,
        .l_len = size,
    };

    ERRCHECK(fcntl(fd, F_SETLKW, &lock), "Could not unset wait lock for region: offset = %zu, size = %zu\n", offset, size);
}
