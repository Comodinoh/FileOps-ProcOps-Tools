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
    fprintf(stderr, "Usage: %s --root <dir> --workers <N>\n  [--ipc data/map]\n  [--db data/inventory.db]\n", executable);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        send_help(argv[0]);
        return 1;
    }

    char* root = NULL;
    char* workers = NULL;
    char* ipc_path = "data/map";

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
            }
        }else if(strcmp(argv[1], "--ipc") == 0) {
            argv++;
            if(argv[1] != NULL) {
                ipc_path = argv[1];
            }
        }
        argv++;
    }

    if(workers == NULL || root == NULL) {
        send_help(argv[0]);
        return 1;
    }

    ull workers_num = strtoull(workers, NULL, 10);
    if(workers_num == 0) {
        fprintf(stderr, "Invalid number of workers, must be non-zero.\n");
        return 2;
    }

    ipc_conn conn = {0};

    int status;
    if((status = manager_init(&conn, root, ipc_path, workers_num)) !=  0) {
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
