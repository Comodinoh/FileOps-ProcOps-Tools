#include "db.h"
#include "procs.h"
#include "util.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

uint32_t proc_generate_snapshot(const char*) {
    return time(NULL);
}

void traverse_procs(db_connection* connection) {
    DIR* dir = opendir("/proc"); 
    ERRCHECKNULL(dir, "[Procs]: Could not open /proc\n");

    struct dirent* proc;

    while((proc = readdir(dir))) {
        if(proc->d_name[0] < '0' || proc->d_name[0] > '9') {
            continue;
        }

        db_procs_row row = {0};

        pid_t pid = atoi(proc->d_name);




    }
}

int main(int argc, char** argv) {
    if(argc < 3) {
        fprintf(stderr, "Usage: %s --root <dir> [--db <path>]", argv[0]);
        return 1;
    }

    char* db = "./data/procs.db";

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--db") == 0) {
            argv++;
            if(argv[1] != NULL) {
                db = argv[1];
            }
        }
        argv++;
    }

    db_connection connection = {0};
    if(!db_open_connection(&connection, db, DB_PROCS_SIGNATURE, &proc_generate_snapshot)) {
        printf("[Procs]: %s is already sealed\n", db);

        return 2;
    }

    // printf("size of header: %zu\n", sizeof(db_header));
    // printf("size of row: %zu\n", sizeof(db_indexer_row));
    // printf("size of bname: %zu\n", sizeof(db_indexer_bname));
    

    // printf("Traversed everything\n");

    db_close_connection(&connection);

    return 0;
}
