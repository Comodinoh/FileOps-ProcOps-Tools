#include "fileops_ipc.h"

#include "types.h"
#include "util.h"
#include <openssl/crypto.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <stdlib.h>

volatile sig_atomic_t sigusr1_flag = 0;
volatile sig_atomic_t shutdown_flag = 0;
volatile sig_atomic_t sigchld_flag  = 0;

int manager_init(ipc_conn* conn, const char* root, const char* ipc_path, const char* db_path, usz workers, usz max_depth) {

    snprintf(conn->temp_path, PATH_MAX, "%s.temp", db_path);
    snprintf(conn->og_path, PATH_MAX, "%s", db_path);
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

    header->max_depth = max_depth;


    ipc_job* job_queue = (ipc_job*)(&header[1]);

    strncpy(job_queue[header->queue_head++].path, root, DB_STRING_LEN);
    job_queue[header->queue_head-1].path[DB_STRING_LEN-1] = '\0';
    job_queue[header->queue_head-1].depth = 0;

    header->jobs_waiting++;

    if(access(conn->temp_path, F_OK) != -1) {

        remove(conn->temp_path);
    }

    if((conn->db = fopen(conn->temp_path, "w+")) == NULL) {
        fprintf(stderr, "[FileopsManger]: ERROR: Could not open temp db file %s\n", conn->temp_path);
        perror(NULL);
        return -1;
    }

    ipc_db_header dbhead = {
        .sig = "INV",
        .complete = DB_START,
        .file_records = 0,
        .format = 1,
        .workers = workers
    };
    conn->db_header = mmap(NULL, sizeof(ipc_db_header), PROT_WRITE | PROT_READ, MAP_SHARED, fileno(conn->db), 0);

    if(conn->db_header == MAP_FAILED) {
        fprintf(stderr, "[FileopsManger]: ERROR: Could not map temp db header in memory\n");
        perror(NULL);
        return 1;
    }

    fwrite(&dbhead, sizeof(ipc_db_header), 1, conn->db);
    fflush(conn->db);

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
            ERRCHECK(sem_wait(&channels[i].sem), "[FileopsManger]: ERROR: Could not wait for channel %zu semaphore\n", i);
            ipc_result_channel* chan = &channels[i];

            if(!chan->done) {

                ipc_result_record* record = &chan->records[chan->records_tail];
                chan->records_tail = (chan->records_tail + 1 ) % QUEUE_CHANNEL_LEN;

                nothing = nothing && false;

                fwrite(record, sizeof(ipc_result_record), 1, conn->db);
                conn->db_header->file_records++;
            } else {
                nothing = nothing && true;
            }

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

            fwrite(record, sizeof(ipc_result_record), 1, conn->db);
            conn->db_header->file_records++;
        }
    }

}

void copyfile(int fd_a, int fd_b) {
    usz page_size = sysconf(_SC_PAGE_SIZE);
    
    ssize_t bytes_read = 0;
    char* buf = malloc(page_size);
    while((bytes_read = read(fd_a, buf, page_size)) != 0) {
        if(bytes_read == -1) {
            fprintf(stderr, "[FileopsManager]: ERROR: Could not read from file to copy\n");
            perror(NULL);
            exit(1);
        }
        ERRCHECK(write(fd_b, buf, bytes_read), "[FileopsManager]: ERROR: Could not write to file for copy\n");
    }


    free(buf);
}
void manager_quit(ipc_conn* conn) {
    ERRCHECK(munmap(conn->map, conn->map_size), "[FileopsManager]: ERROR: Could not unmap ipc\n");
    if (conn->db_header->complete == DB_START) {
        conn->db_header->complete = DB_FULL_COMPLETE;
    }
    msync(conn->db_header, sizeof(ipc_db_header), MS_SYNC);
    munmap(conn->db_header, sizeof(ipc_db_header));

    fclose(conn->db);

    ERRCHECK(rename(conn->temp_path, conn->og_path), "[FileopsManager]: ERROR: Could not rename temp db to final db\n");

    sem_destroy(&conn->header->job_sem);
    sem_destroy(&conn->header->quit_sem);
    sem_destroy(&conn->header->queue_write_sem);
    sem_destroy(&conn->header->queue_read_sem);
}

void manager_collect_and_wait(ipc_conn* conn, ipc_collect_params* params) {
    ipc_header* header = conn->header;
    ipc_job* queue = (void*)&header[1];
    ipc_result_channel* channels = (void*)&queue[QUEUE_JOB_LEN];

    manager_worker_state* worker_states = calloc(conn->workers, sizeof(manager_worker_state));

    char pipe_buf[4096];
    int pipe_buf_len = 0;
    time_t shutdown_start = 0;
    int in_shutdown = 0;

    while (1) {
        if (sigchld_flag) {
            sigchld_flag = 0;
            int st;
            pid_t pid;
            while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
                for (usz i = 0; i < conn->workers; i++) {
                    if (params->worker_pids[i] == pid) {
                        params->worker_pids[i] = 0;
                    }
                }
            }
        }

        if (shutdown_flag && !in_shutdown) {
            in_shutdown = 1;
            shutdown_start = time(NULL);
            header->quitting = true;
            for (usz i = 0; i < conn->workers; i++) {
                if (params->worker_pids[i] > 0) {
                    kill(params->worker_pids[i], SIGTERM);
                }
            }
        }

        if (in_shutdown) {
            time_t now = time(NULL);
            if (now - shutdown_start >= params->graceful_timeout) {
                for (usz i = 0; i < conn->workers; i++) {
                    if (params->worker_pids[i] > 0) {
                        kill(params->worker_pids[i], SIGKILL);
                        int st;
                        waitpid(params->worker_pids[i], &st, 0);
                        params->worker_pids[i] = 0;
                    }
                }
            }
        }

        struct pollfd pfd;
        pfd.fd = params->control_pipe_read_fd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 5);

        if (pr > 0 && (pfd.revents & POLLIN)) {
            ssz r = read(params->control_pipe_read_fd, pipe_buf + pipe_buf_len, sizeof(pipe_buf) - pipe_buf_len - 1);
            if (r > 0) {
                pipe_buf_len += r;
                pipe_buf[pipe_buf_len] = '\0';
                char* line_start = pipe_buf;
                char* newline;
                while ((newline = strchr(line_start, '\n')) != NULL) {
                    *newline = '\0';

                    unsigned int wid = 0;
                    unsigned int jobs = 0;
                    unsigned int files = 0;
                    unsigned long bytes = 0;
                    char reason[64] = {0};
                    int err_num = 0;
                    char where[128] = {0};

                    if (sscanf(line_start, "T5MSG type=JOB_DONE worker_id=%u jobs=%u files=%u bytes=%lu", &wid, &jobs, &files, &bytes) == 4) {
                        if (wid < conn->workers) {
                            worker_states[wid].jobs = jobs;
                            worker_states[wid].files = files;
                            worker_states[wid].bytes = (u64)bytes;
                        }
                    } else if (sscanf(line_start, "T5MSG type=WORKER_EXITING worker_id=%u reason=%63s", &wid, reason) == 2) {
                        if (wid < conn->workers) {
                            worker_states[wid].exited = true;
                        }
                    } else if (sscanf(line_start, "T5MSG type=ERROR worker_id=%u errno=%d where=%127s", &wid, &err_num, where) == 3) {
                        fprintf(stderr, "[FileopsManager]: Worker %u error: errno=%d in %s\n", wid, err_num, where);
                    }

                    line_start = newline + 1;
                }
                int processed = (int)(line_start - pipe_buf);
                if (processed > 0) {
                    memmove(pipe_buf, line_start, pipe_buf_len - processed);
                    pipe_buf_len -= processed;
                }
            }
        }

        bool nothing = true;
        for (usz i = 0; i < conn->workers; i++) {
            if (sem_trywait(&channels[i].read_sem) == -1) {
                if (errno == EAGAIN) continue;
                fprintf(stderr, "[FileopsManager]: ERROR: Could not try wait for channel %zu read semaphore\n", i);
                free(worker_states);
                return;
            }
            ERRCHECK(sem_wait(&channels[i].sem), "[FileopsManager]: ERROR: Could not wait for channel %zu semaphore\n", i);
            ipc_result_channel* chan = &channels[i];
            if (!chan->done) {
                ipc_result_record* record = &chan->records[chan->records_tail];
                chan->records_tail = (chan->records_tail + 1) % QUEUE_CHANNEL_LEN;
                nothing = false;
                fwrite(record, sizeof(ipc_result_record), 1, conn->db);
                conn->db_header->file_records++;
            }
            ERRCHECK(sem_post(&channels[i].sem), "[FileopsManager]: ERROR: Could not post channel %zu semaphore\n", i);
            ERRCHECK(sem_post(&channels[i].write_sem), "[FileopsManager]: ERROR: Could not post channel %zu write semaphore\n", i);
        }

        if (sigusr1_flag) {
            sigusr1_flag = 0;
            u32 tot_files = 0;
            u64 tot_bytes = 0;
            int active_workers = 0;
            for (usz w = 0; w < conn->workers; w++) {
                tot_files += worker_states[w].files;
                tot_bytes += worker_states[w].bytes;
                if (params->worker_pids[w] > 0) active_workers++;
            }
            printf("STATUS queued_jobs=%u active_jobs=%u files=%u bytes=%llu workers_alive=%d complete=0\n",
                   header->jobs_waiting, header->jobs_running, tot_files, (unsigned long long)tot_bytes, active_workers);
            fflush(stdout);
        }

        if (in_shutdown) {
            int active_workers = 0;
            for (usz w = 0; w < conn->workers; w++) {
                if (params->worker_pids[w] > 0) active_workers++;
            }
            if (active_workers == 0) break;
        } else {
            sem_wait(&header->job_sem);
            if (header->jobs_waiting == 0 && header->jobs_running == 0 && nothing) {
                sem_post(&header->job_sem);
                break;
            }
            sem_post(&header->job_sem);
        }
    }

    close(params->control_pipe_read_fd);

    for (usz i = 0; i < conn->workers; i++) {
        ipc_result_channel* chan = &channels[i];
        while (chan->records_tail != chan->records_head) {
            ipc_result_record* record = &chan->records[chan->records_tail];
            chan->records_tail = (chan->records_tail + 1) % QUEUE_CHANNEL_LEN;
            fwrite(record, sizeof(ipc_result_record), 1, conn->db);
            conn->db_header->file_records++;
        }
    }

    for (usz i = 0; i < conn->workers; i++) {
        if (params->worker_pids[i] > 0) {
            int st;
            waitpid(params->worker_pids[i], &st, 0);
            params->worker_pids[i] = 0;
        }
    }

    for (usz i = 0; i < conn->workers; i++) {
        ipc_result_channel* chan = &channels[i];
        fwrite(&chan->stats, sizeof(ipc_stats), 1, conn->db);
    }
    fflush(conn->db);

    if (in_shutdown) {
        conn->db_header->complete = DB_PART_COMPLETE;
    } else {
        conn->db_header->complete = DB_FULL_COMPLETE;
    }

    free(worker_states);
}

void manager_signal_sigusr1(void) {
    sigusr1_flag = 1;
}

void manager_signal_shutdown(void) {
    shutdown_flag = 1;
}

void manager_signal_sigchld(void) {
    sigchld_flag = 1;
}
