#include "db.h"
#include "fileops_ipc.h"
#include "types.h"
#include "util.h"

#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

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

    int fd;
    if(access(ipc_path, F_OK) != -1) {
        ERRCHECK(remove(ipc_path), "[FileopsManager] ERROR: Could not remove previously present ipc file %s\n", ipc_path);
    }

    ERRCHECK(fd = open(ipc_path, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR), "[FileopsManager] ERROR: Could not create ipc file %s\n", ipc_path);

/*
    if((fd = shm_open(ipc_path, O_CREAT | O_RDWR | O_EXCL, 0600)) == -1) {
        if(errno == EEXIST) {
            ERRCHECK(shm_unlink(ipc_path), "[FileopsManager] ERROR: Could not unlink shared memory object %s\n", ipc_path);
            ERRCHECK(fd = shm_open(ipc_path, O_CREAT | O_RDWR, 0600), "[FileopsManager] ERROR: Could not create shared memory object %s\n", ipc_path);
        }  else {
            fprintf(stderr, "[FileopsManager] ERROR: Could not open shared memory object %s\n", ipc_path);
            return 4;
        }
    }*/

    usz map_size = sizeof(ipc_header);
    map_size += sizeof(ipc_job)*QUEUE_JOB_LEN;
    map_size += sizeof(ipc_result_channel)*workers_num;

    ERRCHECK(ftruncate(fd, map_size), "[FileopsManager] ERROR: Could not truncate the ipc file size to the map size %zu\n", map_size);

    void* map = mmap(NULL, map_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        fprintf(stderr, "[FileopsManager] ERROR: Could not map the ipc file to memory\n");
        perror(NULL);
        return 3;
    }
    close(fd);

    memset(map, 0, map_size);

    ipc_header* header = map;
    ERRCHECK(sem_init(&header->job_sem, 1, 1), "[FileopsManager] ERROR: Could not initialize job semaphore\n");

    ipc_job* job_queue = (ipc_job*)(&header[1]);

    strncpy(job_queue[header->queue_head++], root, DB_STRING_LEN);
    job_queue[header->queue_head-1][DB_STRING_LEN-1] = '\0';

    strncpy(job_queue[header->queue_head++], "Test", DB_STRING_LEN);
    header->jobs_waiting++;

    for(usz i = 0; i < workers_num; i++) {
        pid_t pid;
        ERRCHECK(pid = fork(), "[FileopsManager]: ERROR: Could not fork parent process to create kids\n");
        if(pid == 0) {
            // We are a kid
            printf("[FileopsWorker %zu] INFO: Children with pid %d of %d speaking! Yessir!\n", i, getpid(), getppid());

            while(1) {
                ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %zu] ERROR: Could not wait for job semaphore\n", i);
                if(header->jobs_waiting == 0 && header->jobs_running == 0) {
                    printf("[FileopsWorker %zu] INFO: My job here is done\n", i);
                    ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %zu] ERROR: Could not post job semaphore\n", i);
                    ERRCHECK(munmap(map, map_size), "[FileopsManager] ERROR: Could not unmap ipc\n");
                    _exit(0);
                }
                if(header->queue_tail == header->queue_head) continue;

                char job[DB_STRING_LEN];

                strncpy(job, job_queue[header->queue_tail], DB_STRING_LEN);
                header->queue_tail = (header->queue_tail + 1) % QUEUE_JOB_LEN;

                printf("[FileopsWorker %zu] INFO: Took on job %s\n",  i, job);

                header->jobs_running++;
                header->jobs_waiting--;


                ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %zu] ERROR: Could not post job semaphore\n", i);

                printf("[FileopsWorker %zu] INFO: Consumed job %s\n",  i, job);

                ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %zu] ERROR: Could not wait for job semaphore\n", i);
                header->jobs_running--;
                ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %zu] ERROR: Could not post job semaphore\n", i);
                int val;
                sem_getvalue(&header->job_sem, &val);
                printf("[FileopsWorker %zu] INFO: I posted the semaphore yay!! Its now %d\n", i, val);

            }

            ASSERT(0, "UNREACHABLE");
        }

        // We are the parent

        printf("[FileopsManager]: INFO: Sir im the parent here sir of Worker no. %zu! What can I do for you!\n", i);
    }

    for(usz i = 0; i < workers_num; i++ )  {
        printf("[FileopsManager] INFO: Waiting for Worker no. %zu to die...\n", i);
        pid_t pid;
        int stat;
        ERRCHECK(pid = wait(&stat), "[FileopsManager]: ERROR: Could not wait for kid to die\n");
        printf("[FileopsManager]: INFO: kid with pid %d has died with status %d\n", pid, stat);
    }


    printf("[FileopsManager]: INFO: All kids are dead sir! Im gonna die too! Hope you arent gonna miss me...\n");


    ERRCHECK(munmap(map, map_size), "[FileopsManager] ERROR: Could not unmap ipc\n");
    sem_destroy(&header->job_sem);

    return 0;
}
