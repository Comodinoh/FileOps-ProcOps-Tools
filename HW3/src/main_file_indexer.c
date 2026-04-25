#include <db.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

uint32_t idx_generate_snapshot(const char* path) {
    return time(NULL);
}


size_t hash_file(int fd) {
    long page_size = sysconf(_SC_PAGE_SIZE);

    char* buf = malloc(page_size);

    size_t sum = 5381;

    ssize_t bytes_read = 0;
    while((bytes_read = read(fd, buf, page_size)) != 0) {
        for(long i = 0; i < bytes_read; i++) {
            sum = (sum*33 ^ buf[i]);
        }
    }


    free(buf);

    return sum;
}

typedef enum {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIR,
    FILE_TYPE_SYM,
    FILE_TYPE_FIFO,
} db_indexer_ftype;

typedef struct {
    char                absolute_path[DB_STRING_LEN];
    db_indexer_ftype    type; 
    uint32_t            size;
    uint32_t            last_modification;
    size_t              hash;
    char                bname[256];
    bool                symlink;
    char                symlink_target[256];
} db_indexer_row;

bool db_indexer_compare(db_indexer_row* r1, db_indexer_row* r2) {
    if(strcmp(r1->absolute_path, r2->absolute_path) == 0) {
        return true;
    } 

    return false;
}

void db_indexer_upsert(db_connection* connection, db_indexer_row* row) {
    int fd = connection->fd;
    int temp = db_tell(connection);

    uint32_t record_count = db_record_count(connection);

    lseek(fd, sizeof(db_header), SEEK_SET);
    db_indexer_row irow;
    for(size_t i = 0; i < record_count; i++) {
        ERRCHECK(read(fd, &irow, sizeof(db_indexer_row)), "[FileIndexer] UPSERT: Could not read row %zu for %s\n", i, connection->filepath);
        if(db_indexer_compare(row, &irow)) {

        }
    }


    lseek(fd, temp, SEEK_SET);

    return true;
}

int main(void) {
    int fd = open("test.txt", O_RDONLY);
    

    //srand(time(NULL));

    /*for(size_t j = 0; j < 100; j++) {
        char test[100] = {0};
        for(size_t i = 0; i < sizeof(test)/sizeof(test[0]); i++) {
            char c = rand() % 255;
            while(c == '\0') {
                c = rand() % 255;
            }

            test[i] = c;
        }
    }*/

    printf("%zu\n", hash_file(fd));


    close(fd);
    return 0;
}

