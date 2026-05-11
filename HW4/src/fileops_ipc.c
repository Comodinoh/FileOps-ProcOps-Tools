#include "fileops_ipc.h"

#include "types.h"
#include "util.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

int manager_init(ipc_conn* conn, const char* root, const char* ipc_path, usz workers) {

    int fd;
    if(access(ipc_path, F_OK) != -1) {
        ERRCHECK(remove(ipc_path), "[FileopsManager]: ERROR: Could not remove previously present ipc file %s\n", ipc_path);
    }

    ERRCHECK(fd = open(ipc_path, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR), "[FileopsManager]: ERROR: Could not create ipc file %s\n", ipc_path);

    usz map_size = MAP_JOB_SIZE+sizeof(ipc_result_channel)*workers;
    conn->map_size = map_size;

    ERRCHECK(ftruncate(fd, map_size), "[FileopsManager]: ERROR: Could not truncate the ipc file size to the map size %zu\n", map_size);

    void* map = mmap(NULL, map_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    if(map == MAP_FAILED) {
        fprintf(stderr, "[FileopsManager]: ERROR: Could not map the ipc file to memory\n");
        perror(NULL);
        return 3;
    }
    close(fd);

    memset(map, 0, map_size);

    ipc_header* header = map;
    conn->header = header;
    conn->map = map;
    conn->workers = workers;
    ERRCHECK(sem_init(&header->job_sem, 1, 1), "[FileopsManager]: ERROR: Could not initialize job semaphore\n");
    ERRCHECK(sem_init(&header->queue_write_sem, 1, QUEUE_JOB_LEN-1), "[FileopsManager]: ERROR: Could not initialize queue write semaphore\n");
    ERRCHECK(sem_init(&header->queue_read_sem, 1, 1), "[FileopsManager]: ERROR: Could not initialize queue read semaphore\n");
    ERRCHECK(sem_init(&header->quit_sem, 1, 1), "[FileopsManager]: ERROR: Could not initialize quit semaphore\n");
    ERRCHECK(sem_init(&header->workers_sem, 1, 0), "[FileopsManager]: ERROR: Could not initialize quit semaphore\n");
    header->quitting = false;


    ipc_job* job_queue = (ipc_job*)(&header[1]);

    strncpy(job_queue[header->queue_head++].path, root, DB_STRING_LEN);
    job_queue[header->queue_head-1].path[DB_STRING_LEN-1] = '\0';

    header->jobs_waiting++;

    return 0;
}

void manager_collect(ipc_conn* conn, ipc_header* header) {
    while(1) {

        bool nothing = true;
        ipc_job* queue = (void*)&header[1];
        ipc_result_channel* channels = (void*)&queue[QUEUE_JOB_LEN];
        for(usz i = 0; i < conn->workers; i++) {
            if(sem_trywait(&channels[i].read_sem) == -1) {
                if(errno == EAGAIN) {
                    continue;
                }else {
                    fprintf(stderr, "[FileopsManager]: ERROR: Could not try wait for channel %zu read semaphore\n", i);
                    return;
                }
            }
            nothing = false;
            ERRCHECK(sem_wait(&channels[i].sem), "[FileopsManger]: ERROR: Could not wait for channel %zu semaphore\n", i);

            ipc_result_channel* chan = &channels[i];
            ipc_result_record* record = &chan->records[chan->records_tail];
            chan->records_tail = (chan->records_tail + 1 ) % QUEUE_CHANNEL_LEN;

            // printf("[FileopsManger]: INFO: record %s\n", record->absolute_path);

            ERRCHECK(sem_post(&channels[i].sem), "[FileopsManger]: ERROR: Could not post channel %zu semaphore\n", i);
            ERRCHECK(sem_post(&channels[i].write_sem), "[FileopsManger]: ERROR: Could not post write channel %zu write semaphore\n", i);
        }

        if(sem_trywait(&header->job_sem) == -1) {
            if(errno == EAGAIN) {
                continue;
            }else  {
                fprintf(stderr, "[FileopsManager]: ERROR: Could not wait for job semaphore\n");
                perror(NULL);
                return;
            }
        }
        if(header->jobs_waiting == 0 && header->jobs_running == 0 && nothing) { 
            sem_post(&header->job_sem);
            break;
        }
        sem_post(&header->job_sem);

    }

    ipc_job* queue = (void*)&header[1];
    ipc_result_channel* channels = (void*)&queue[QUEUE_JOB_LEN];
    for(usz i = 0; i < conn->workers; i++) {
        ipc_result_channel* chan = &channels[i];

        while(chan->records_tail != chan->records_head) {
            ipc_result_record* record = &chan->records[chan->records_tail];
            chan->records_tail = (chan->records_tail + 1 ) % QUEUE_CHANNEL_LEN;

            printf("[FileopsManger]: INFO: record %s\n", record->absolute_path);
        }
    }
}

void manager_quit(ipc_conn* conn) {
    ERRCHECK(munmap(conn->map, conn->map_size), "[FileopsManager]: ERROR: Could not unmap ipc\n");
    sem_destroy(&conn->header->job_sem);
    sem_destroy(&conn->header->quit_sem);
    sem_destroy(&conn->header->queue_write_sem);
    sem_destroy(&conn->header->queue_read_sem);
}

void manager_collect_and_wait(ipc_conn* conn) {
    ipc_header* header = conn->header;

    manager_collect(conn, header);

    int val;
    sem_getvalue(&header->workers_sem, &val);
    for(usz i = 0; i < conn->workers-val; i++ )  {
        pid_t pid;
        int stat;
        ERRCHECK(pid = wait(&stat), "[FileopsManager]: ERROR: Could not wait for kid to die\n");
        printf("[FileopsManager]: INFO: kid with pid %d has died with status %d\n", pid, stat);
    }
}
