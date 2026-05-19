#include "db.h"
#include "util.h"
#include "file_indexer.h"
#include "procs.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

constexpr size_t DB_PROCS_RSS_THRESHOLD = 512; // 512KB=0.5MB

uint32_t read_all_records_idx(db_connection* connection, db_indexer_row** out) {
    uint32_t rec_count = db_record_count(connection);

    (*out) = malloc(sizeof(db_indexer_row)*rec_count);
    
    lseek(connection->fd, sizeof(db_header), SEEK_SET);

    ERRCHECK(read(connection->fd, *out, sizeof(db_indexer_row)*rec_count), "[DBDiff]: Could not read all of the rows from %s into memory\n", connection->filepath);

    return rec_count;
}

const char* idx_type_to_string(db_indexer_ftype type) {
    switch (type) {
        case FILE_TYPE_DIR:
            {
                return "directory";
            }
        case FILE_TYPE_FIFO:
            {
                return "fifo";
            }
        case FILE_TYPE_REGULAR:
            {
                return "regular";
            }
        case FILE_TYPE_SYM:
            {
                return "symlink";
            }
        default:
            {
                return NULL;
            }
    }

    return NULL;
}

int idx_row_compare(const void* r1, const void* r2) {
    return strcmp(((const db_indexer_row*)r1)->absolute_path, ((const db_indexer_row*)r2)->absolute_path);
}

void generate_diffs_and_store_idx(db_connection* old, db_connection* new, int outfd, const char* signature) {
    db_indexer_row* old_rows;
    db_indexer_row* new_rows;
    uint32_t old_count = read_all_records_idx(old, &old_rows); 
    uint32_t new_count = read_all_records_idx(new, &new_rows);


    qsort(old_rows, old_count, sizeof(db_indexer_row), &idx_row_compare);
    qsort(new_rows, new_count, sizeof(db_indexer_row), &idx_row_compare);

    // I dont wanna hear any complaints
    FILE* f = fdopen(outfd, "w");

    size_t i = 0;
    size_t j = 0;
    while(i < old_count && j < new_count) {
        db_indexer_row* r1 = &old_rows[i];
        db_indexer_row* r2 = &new_rows[j];
        int com = idx_row_compare(r1, r2);

        if(com == 0) {
            bool tdif = r1->type != r2->type;
            bool sdif = r1->size != r2->size;
            bool mdif = r1->last_modification != r2->last_modification;
            bool hdif = r1->hash != r2->hash;
            bool ldif = strcmp(r1->symlink_target, r2->symlink_target) != 0;

            if(tdif || sdif || mdif || hdif || ldif) {
                fprintf(f, "MODIFIED %s\n", r1->absolute_path);
                if(tdif) {
                   fprintf(f, "  TYPE %s -> %s\n", idx_type_to_string(r1->type), idx_type_to_string(r2->type)); 
                }
                if(sdif) {
                   fprintf(f, "  SIZE %dB -> %dB\n", r1->size, r2->size); 
                }
                if(mdif) {
                   fprintf(f, "  LAST_MODIFICATION %ds -> %ds\n", r1->last_modification, r2->last_modification);
                }
                if(hdif) {
                   fprintf(f, "  HASH %zu -> %zu\n", r1->hash, r2->hash);
                }
                if(ldif) {
                   fprintf(f, "  SYMLINK_TARGET %s -> %s\n", r1->symlink_target, r2->symlink_target);
                }
            }

            i++;
            j++;
        } else if(com < 0) {
            fprintf(f, "DISAPPEARED %s\n", r1->absolute_path);
            i++;
        } else {
            fprintf(f, "APPEARED %s\n", r2->absolute_path);
            j++;
        }
    }


    while (i < old_count) {
        fprintf(f, "DISAPPEARED %s\n", old_rows[i].absolute_path);
        i++;
    }

    while (j < new_count) {
        fprintf(f, "APPEARED %s\n", new_rows[i].absolute_path);
        j++;
    }

    fflush(f);
}

uint32_t read_all_records_procs(db_connection* connection, db_procs_row** out) {
    uint32_t rec_count = db_record_count(connection);

    (*out) = malloc(sizeof(db_procs_row)*rec_count);
    
    lseek(connection->fd, sizeof(db_header), SEEK_SET);

    ERRCHECK(read(connection->fd, *out, sizeof(db_procs_row)*rec_count), "[DBDiff]: Could not read all of the rows from %s into memory\n", connection->filepath);

    return rec_count;
}

int procs_row_compare(const void* r1, const void* r2) {
    return (int)((const db_procs_row*)r1)->pid - (int)((const db_procs_row*)r2)->pid;
}

void generate_diffs_and_store_procs(db_connection* old, db_connection* new, int outfd, const char* signature) {
    db_procs_row* old_rows;
    db_procs_row* new_rows;
    uint32_t old_count = read_all_records_procs(old, &old_rows); 
    uint32_t new_count = read_all_records_procs(new, &new_rows);

    qsort(old_rows, old_count, sizeof(db_procs_row), &procs_row_compare);
    qsort(new_rows, new_count, sizeof(db_procs_row), &procs_row_compare);

    FILE* f = fdopen(outfd, "w");

    size_t i = 0;
    size_t j = 0;
    while(i < old_count && j < new_count) {
        db_procs_row* r1 = &old_rows[i];
        db_procs_row* r2 = &new_rows[j];
        int com = procs_row_compare(r1, r2);

        if(com == 0) {

            if((r1->rss > r2->rss ? (r1->rss - r2->rss) : (r2->rss - r1->rss)) >= DB_PROCS_RSS_THRESHOLD) {
                printf("Found process that changed\n");
                fprintf(f, "SIGNIFICANTLY CHANGED %d\n", r1->pid);
                fprintf(f, "  RSS %zuKB -> %zuKB\n", r1->rss, r2->rss);
            }

            i++;
            j++;
        } else if(com < 0) {
            printf("Found process that disappeared\n");
            fprintf(f, "DISAPPEARED %d\n", r1->pid);
            i++;
        } else {
            printf("Found process that appeared\n");
            fprintf(f, "APPEARED %d\n", r2->pid);
            j++;
        }
    }


    while (i < old_count) {
        fprintf(f, "DISAPPEARED %d\n", old_rows[i].pid);
        i++;
    }

    while (j < new_count) {
        fprintf(f, "APPEARED %d\n", new_rows[j].pid);
        j++;
    }

    fflush(f);
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s --old <db1> --new <db2> --out <path>", argv[0]);
        return 1;
    }

    char* old = NULL;
    char* new = NULL;
    char* out = NULL;

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--old") == 0) {
            argv++;
            if(argv[1] != NULL) {
                old = argv[1];
            }
        }else if(strcmp(argv[1], "--new") == 0) {
            argv++;
            if(argv[1] != NULL) {
                new = argv[1];
            }

        }else if(strcmp(argv[1], "--out") == 0) {
            argv++;
            if(argv[1] != NULL) {
                out = argv[1];
            }
        }
        argv++;
    }

    if(old == NULL || new == NULL || out == NULL) {
        fprintf(stderr, "Usage: %s --old <db1> --new <db2> --out <path>\n", argv[0]);
        return 1;
    }

    db_connection db1 = {0};
    db_connection db2 = {0};

    if(!db_open_connection(&db1, old, NULL, NULL)) {
        fprintf(stderr, "[DBDiff]: %s db does not exist\n", old);
        return 1;
    }

    if(!db_open_connection(&db2, new, NULL, NULL)) {
        fprintf(stderr, "[DBDiff]: %s db does not exist\n", old);
        return 1;
    }
 
    char* sig1;
    char* sig2;

    db_signature(&db1, &sig1);
    db_signature(&db2, &sig2);

    if(strcmp(sig1, sig2) != 0) {
        fprintf(stderr, "[DBDiff]: %s old db signature does not match with %s new db signature\n", sig1, sig2);
        return 1;
    }

    free(sig2);

    uint8_t fmt1, fmt2;
    if((fmt1 = db_format_version(&db1)) != (fmt2 = db_format_version(&db2))) {
        fprintf(stderr, "[DBDiff]: %d old db format version match with %d new db format version\n", fmt1, fmt2);
        return 1;
    }

    if(access(out, F_OK) != -1) {
        remove(out);
    }

    int outfd;
    ERRCHECK(outfd = open(out, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR), "Could not open out %s file\n", out);

    if(strcmp(sig1, DB_FILE_INDEXER_SIGNATURE) == 0) {
        generate_diffs_and_store_idx(&db1, &db2, outfd, sig1);
    }else if (strcmp(sig1, DB_PROCS_SIGNATURE) == 0) {
        generate_diffs_and_store_procs(&db1, &db2, outfd, sig1);
    }

    close(outfd);

    free(sig1);
    return 0;
}
