#include <linux/limits.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "file_indexer.h"
#include "lock.h"
#include "util.h"

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
            sum = (sum*33 + buf[i]);
        }
    }


    free(buf);

    return sum;
}


bool db_indexer_eq(db_indexer_row* r1, db_indexer_row* r2) {
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
    size_t i;
    for(i = 0; i < record_count; i++) {
        db_lock_read_region_wait(fd, 0, sizeof(db_indexer_row));

        ERRCHECK(read(fd, &irow, sizeof(db_indexer_row)), "[FileIndexer] UPSERT: Could not read row %zu for %s\n", i, connection->filepath);

        db_unlock_region(fd, -sizeof(db_indexer_row), sizeof(db_indexer_row));

        if(db_indexer_eq(row, &irow)) {

            db_lock_write_region_wait(fd, -sizeof(db_indexer_row), sizeof(db_indexer_row)); 

            lseek(fd, -sizeof(db_indexer_row), SEEK_CUR);

            ERRCHECK(write(fd, row, sizeof(db_indexer_row)), "[FileIndexer] UPSERT: Could not overwrite row %zu for %s\n", i, connection->filepath);


            db_unlock_region(fd, -sizeof(db_indexer_row), sizeof(db_indexer_row));

            lseek(fd, temp, SEEK_SET);
            return;
        }
    }
    
    while(!db_lock_write_region(fd, 0, sizeof(db_indexer_row))) {
        lseek(fd, sizeof(db_indexer_row), SEEK_CUR);
        i++;
    }

    ERRCHECK(write(fd, row, sizeof(db_indexer_row)), "[FileIndexer] UPSERT: Could not insert row %zu for %s\n", i, connection->filepath);

    db_unlock_region(fd, -sizeof(db_indexer_row), sizeof(db_indexer_row));

    db_inc_record_count(connection);

    lseek(fd, temp, SEEK_SET);
}

void traverse_file_tree(db_connection* connection, const char* path) {
    struct dirent* entry;
    DIR* dir = opendir(path);

    if(!dir) {
        fprintf(stderr, "[FileIndexer]: Cannot open directory %s:\n", path);
        perror(0);
        return;
    }

    printf("Reading dir %s...\n", path);

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char p[PATH_MAX] = {0};
            snprintf(p, PATH_MAX, "%s/%s", path, entry->d_name);

            struct stat st;

            if(stat(p, &st) == -1) return;

            db_indexer_row row = {0};
            memset(&row, 0, sizeof(row));

            char* str = realpath(p, NULL);

            strncpy(row.absolute_path, str, DB_STRING_LEN);
            row.absolute_path[DB_STRING_LEN-1] = '\0';

            free(str);

            bool is_dir = false;

            if(S_ISLNK(st.st_mode)) {
                row.symlink = true;
                char target[DB_STRING_LEN] = {0};

                if(readlink(p, target, DB_STRING_LEN) == -1) {
                    return;
                }

                memcpy(row.symlink_target, target, DB_STRING_LEN);

                row.type = FILE_TYPE_SYM;
            }
            else if(S_ISDIR(st.st_mode)) {
                row.type = FILE_TYPE_DIR;
                is_dir = true;
            }
            else if(S_ISREG(st.st_mode)) {
                row.type = FILE_TYPE_REGULAR;

                row.last_modification = st.st_mtim.tv_sec;
                int fd = open(p, O_RDONLY);
                row.hash = hash_file(fd);
                close(fd);

                row.size = st.st_size;
            }
            else if(S_ISFIFO(st.st_mode)) {
                row.type = FILE_TYPE_FIFO;
            }


            // row.bname = (db_indexer_bname){.st_dev = st.st_dev, .st_ino = st.st_ino};
            
            row.st_dev = st.st_dev;
            row.st_ino = st.st_ino;

            db_indexer_upsert(connection, &row);

            printf("Upserted %s into %s\n", p, path);

            if(is_dir) {
                traverse_file_tree(connection, p);
            }
        }

    }
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s --root <dir> [--db <path>]", argv[0]);
        return 1;
    }

    char* root = NULL;
    char* db = "index.db";

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--root") == 0) {
            argv++;
            if(argv[1] != NULL) {
                root = argv[1];
            }
        }else if(strcmp(argv[1], "--db") == 0) {
            argv++;
            if(argv[1] != NULL) {
                db = argv[1];
            }
        }
        argv++;
    }

    if(root == NULL) {
        fprintf(stderr, "Usage: %s --root <dir> [--db <path>]\n", argv[0]);
        return 1;
    }

    db_connection connection = {0};
    if(!db_open_connection(&connection, db, DB_FILE_INDEXER_SIGNATURE, &idx_generate_snapshot)) {
        printf("[FileIndexer]: %s is already sealed, replacing it...\n", db);
        ERRCHECK(remove(db), "[FileIndexer]: Could not delete file %s:\n", db);

        db_open_connection(&connection, db, DB_FILE_INDEXER_SIGNATURE, &idx_generate_snapshot);
    }

    printf("size of header: %zu\n", sizeof(db_header));
    printf("size of row: %zu\n", sizeof(db_indexer_row));
    printf("size of bname: %zu\n", sizeof(db_indexer_bname));
    
    traverse_file_tree(&connection, root); 

    printf("Traversed everything\n");

    db_close_connection(&connection);

    return 0;
}

