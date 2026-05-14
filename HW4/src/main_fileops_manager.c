#include "db.h"
#include "fileops_ipc.h"
#include "types.h"
#include "util.h"

#include <dirent.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/dir.h>
#include <sys/stat.h>


char* usztostr(usz n) {
    usz len = snprintf(NULL, 0, "%zu", n);
    char* str = malloc(len+1);
    snprintf(str, len+1, "%zu", n);
    str[len] = '\0';
    printf("%s = %zu\n", str, n);
    return str;
}

void send_help(const char* executable) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Inventory Mode:\n");
    fprintf(stderr,  "    %s --root <dir> --workers <N>\n", executable);
    fprintf(stderr,  "       [--ipc data/map]  [--db data/inventory.db]\n");
    fprintf(stderr,  "       [--max-depth <D>] [--simulate-work-ms <ms>]\n");
    fprintf(stderr, "  DB Mode:\n");
    fprintf(stderr,  "    %s --db <database.db> --verify\n", executable);
    fprintf(stderr,  "    %s --db <database.db> --dump\n", executable);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        send_help(argv[0]);
        return 1;
    }
    char* exec = argv[0];

    char* root = NULL;
    char* workers = NULL;
    char* ipc_path = "data/map";
    char* db_path = NULL;

    ipc_mode mode = IPC_NONE;
    ipc_db_action action = IPC_ACTION_NONE;
    usz max_depth = (usz)-1;

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--root") == 0) {
            argv++;
            if(argv[1] != NULL) {
                root = argv[1];
            }
        }else if(strcmp(argv[1], "--workers") == 0) {
            argv++;
            if(argv[1] != NULL) {
                workers = argv[1];
                mode = IPC_INV;
            }
        }else if(strcmp(argv[1], "--ipc") == 0) {
            argv++;
            if(argv[1] != NULL) {
                ipc_path = argv[1];
            }
        }else if(strcmp(argv[1], "--db") == 0) {
            argv++;
            if(argv[1] != NULL) {
                db_path = argv[1];
            }
        }else if(strcmp(argv[1], "--dump") == 0) {
            argv++;
            mode = IPC_DB;
            action = IPC_ACTION_DUMP;
        }else if(strcmp(argv[1], "--verify") == 0) {
            argv++;
            mode = IPC_DB;
            action = IPC_ACTION_VERIFY;
        }else if(strcmp(argv[1], "--max-depth") == 0) {
            argv++;
            if(argv[1] != NULL) {
                max_depth = strtoull(argv[1], NULL, 10);
            }
        }
        argv++;
    }

    if(mode == IPC_NONE) {
        send_help(exec);
        return 1;
    }
    if(mode == IPC_INV) {
        if(workers == NULL || root == NULL) {
            send_help(exec);
            return 1;
        }
        if(db_path == NULL) {
            db_path = "data/inventory.db";
        }
    }else if(mode == IPC_DB) {
        if(action == IPC_ACTION_NONE) {
            fprintf(stderr, "Invalid db action\n");
            send_help(exec);
            return 1;
        }
        if(db_path == NULL) {
            send_help(exec);
            return 1;
        }
    }

    if(mode == IPC_DB){
        int fd;
        ERRCHECK(fd = open(db_path, O_RDONLY), "Could not open file %s\n", db_path);

        ipc_db_header* db_header = mmap(NULL, sizeof(ipc_db_header), PROT_READ, MAP_SHARED, fd, 0);
        if(db_header == MAP_FAILED) {
            fprintf(stderr, "Failed to map header from db %s\n", db_path);
            perror(NULL);
            return 2;
        }
        if(action == IPC_ACTION_DUMP) {
            printf("Database Header:\n");
            printf("  Signature: %s\n", db_header->sig);
            printf("  Version: %d\n", db_header->format);
            printf("  Complete: %d\n", db_header->complete);
            printf("  File Records: %d\n", db_header->file_records);
            printf("  Workers: %d\n", db_header->workers);

            lseek(fd, sizeof(ipc_db_header)+sizeof(ipc_result_record)*db_header->file_records, SEEK_SET);

            ipc_stats stats;
            for(usz i = 0; i < db_header->workers; i++) {
                read(fd, &stats, sizeof(ipc_stats));

                printf("Worker no. %d Statistics:\n", stats.worker_id);
                printf("  PID: %d\n", stats.pid);
                printf("  Exit Status: %d\n", stats.exit_status);
                printf("  Jobs Processed: %d\n", stats.jobs_processed);
                printf("  Files Emitted: %d\n", stats.files_emitted);
                printf("  Bytes Emitted: %zu\n", stats.bytes_emitted);
                printf("  Real Time MS: %zu\n", stats.real_time_ms);
                printf("  User Time MS: %zu\n", stats.user_cpu_us);
                printf("  Sys  Time MS: %zu\n", stats.sys_cpu_us);
                printf("\n");
            }
        }else {
            if(db_header->format != 1) {
                fprintf(stderr, "Invalid format version\n");
                return 1;
            }

            if(db_header->complete == 0) {
                fprintf(stderr, "DB is incomplete\n");
                return 1;
            }
            if(strcmp(db_header->sig, "INV") != 0) {
                fprintf(stderr, "Invalid DB signature\n");
                return 1;
            }

            printf("Approved!\n");

            return 0;
        }


        
        munmap(db_header, sizeof(ipc_db_header));
        close(fd);
        return 0;
    }

    ull workers_num = strtoull(workers, NULL, 10);
    if(workers_num == 0) {
        fprintf(stderr, "Invalid number of workers, must be non-zero.\n");
        return 2;
    }

    ipc_conn conn = {0};

    int status;
    if((status = manager_init(&conn, root, ipc_path, db_path, workers_num, max_depth)) !=  0) {
        fprintf(stderr, "[FileopsManager]: ERROR: Could not initialize manager\n");
        return status;
    }

    ipc_result_channel* channels = (void*)&((ipc_job*)(&conn.header[1]))[QUEUE_JOB_LEN];

    for(usz i = 0; i < workers_num; i++) {
        ERRCHECK(sem_init(&channels[i].sem, 1, 1), "[FileopsManager]: ERROR: Could not initialize semaphore of kid %zu", i);
        ERRCHECK(sem_init(&channels[i].read_sem, 1, 0), "[FileopsManager]: ERROR: Could not initialize read semaphore of kid %zu", i);
        ERRCHECK(sem_init(&channels[i].write_sem, 1, QUEUE_CHANNEL_LEN), "[FileopsManager]: ERROR: Could not initialize write semaphore of kid %zu", i);

        pid_t pid;
        ERRCHECK(pid = fork(), "[FileopsManager]: ERROR: Could not fork parent process to create kids\n");
        if(pid == 0) {
            char* worker_id = usztostr(i);
            execl("bin/fileops_worker", "bin/fileops_worker",
                  "--worker-id", worker_id,
                  "--ipc", strdup(ipc_path),
                  "--workers", strdup(workers),
                  NULL);
        }

        // We are the parent

        printf("[FileopsManager]: INFO: Sir im the parent here sir of Worker no. %zu! What can I do for you!\n", i);
    }

    manager_collect_and_wait(&conn);

    printf("[FileopsManager]: INFO: Waiting for all workers to die...\n");


    printf("[FileopsManager]: INFO: All kids are dead sir! Im gonna die too! Hope you arent gonna miss me...\n");

    manager_quit(&conn);

    return 0;
}
