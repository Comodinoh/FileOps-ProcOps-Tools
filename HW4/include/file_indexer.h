#ifndef __FILE_INDEXER_H
#define __FILE_INDEXER_H

#include <stdlib.h>
#include <stdint.h>
#include <db.h>

#define DB_FILE_INDEXER_SIGNATURE "IDX"

typedef enum {
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIR,
    FILE_TYPE_SYM,
    FILE_TYPE_FIFO,
} db_indexer_ftype;

typedef struct {
    ino_t st_ino;
    dev_t st_dev;
} db_indexer_bname;

typedef struct __attribute__((packed)) {
    char                absolute_path[DB_STRING_LEN];
    db_indexer_ftype    type; 
    uint32_t            size;
    uint32_t            last_modification;
    size_t              hash;
    // db_indexer_bname    bname;
    bool                symlink;
    char                symlink_target[DB_STRING_LEN];
    ino_t               st_ino;
    dev_t               st_dev;
} db_indexer_row;

#endif
