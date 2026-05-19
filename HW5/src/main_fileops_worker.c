#include "fileops_ipc.h"
#include "util.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <openssl/evp.h>
#include <signal.h>
#include <stdio.h>

volatile sig_atomic_t sigterm_received = 0;

void handle_worker_sigterm(int sig) {
    sigterm_received = 1;
}

int safe_sem_wait(sem_t* sem) {
    while (sem_wait(sem) == -1) {
        if (errno == EINTR) {
            if (sigterm_received) return -1;
            continue;
        }
        return -1;
    }
    return 0;
}

ipc_job* global_job_stack = NULL;
static usz global_job_head = 0;
static usz global_job_capacity = 0;

void stack_push_job(ipc_job* job) {
    if(global_job_head >= global_job_capacity-1) {
        global_job_capacity *= 2;
        printf("%zu\n", sizeof(ipc_job)*global_job_capacity);
        global_job_stack = realloc(global_job_stack, sizeof(ipc_job)*global_job_capacity);
    }

    global_job_head++;

    memcpy(&global_job_stack[global_job_head].path, job->path, DB_STRING_LEN);
    global_job_stack[global_job_head].depth = job->depth;

}

bool stack_at_end() {
    return global_job_head == 0;
}

void stack_pop_job(ipc_job* job) {
    memcpy(job->path, &global_job_stack[global_job_head].path, DB_STRING_LEN);
    job->depth = global_job_stack[global_job_head].depth;
    global_job_head--;
}

void hash_file(int fd, uchar* out) {
    long page_size = sysconf(_SC_PAGE_SIZE);
    char* buf = malloc(page_size);

    uint len = 0;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

    ssize_t bytes_read = 0;

    while((bytes_read = read(fd, buf, page_size)) > 0) {
        EVP_DigestUpdate(mdctx, buf, bytes_read);
    }

    EVP_DigestFinal_ex(mdctx, out, &len);
    EVP_MD_CTX_free(mdctx);

    free(buf);
}

char* abspath(const char* path) {
    if (!path || !*path) return NULL;

    char base[PATH_MAX];

    if (path[0] == '/') {
        base[0] = '\0';
    } else {
        if (getcwd(base, sizeof(base)) == NULL) {
            return NULL;
        }
    }

    char full_path[PATH_MAX * 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", base, path);

    char *stack[PATH_MAX / 2];
    int top = 0;

    char *token = strtok(full_path, "/");
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            if (top > 0) {
                top--;
            }
        } else if (strlen(token) > 0) {
            stack[top++] = token;
        }
        token = strtok(NULL, "/");
    }

    char *result = malloc(PATH_MAX);
    if (!result) return NULL;

    if (top == 0) {
        strcpy(result, "/");
        return result;
    }

    size_t len = 0;
    for (int i = 0; i < top; i++) {
        len += snprintf(result + len, PATH_MAX - len, "/%s", stack[i]);
    }

    return result;
}



int main(int argc, char** argv) {
    if(argc < 3) {
        return 1;
    }

    char* worker_id = NULL;
    char* ipc_path = NULL;
    char* workers = NULL;
    int control_fd = -1;
    int simulate_work_ms = 0;

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--worker-id") == 0) {
            argv++;
            if(argv[1] != NULL) {
                worker_id = argv[1];
            }
        }else if(strcmp(argv[1], "--ipc") == 0) {
            argv++;
            if(argv[1] != NULL) {
                ipc_path = argv[1];
            }
        }else if(strcmp(argv[1], "--workers") == 0) {
            argv++;
            if(argv[1] != NULL) {
                workers = argv[1];
            }
        }else if(strcmp(argv[1], "--control-fd") == 0) {
            argv++;
            if(argv[1] != NULL) {
                control_fd = atoi(argv[1]);
            }
        }else if(strcmp(argv[1], "--simulate-work-ms") == 0) {
            argv++;
            if(argv[1] != NULL) {
                simulate_work_ms = atoi(argv[1]);
            }
        }
        argv++;
    }

    if(!ipc_path || !worker_id || !workers) return 2;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_worker_sigterm;
    sigaction(SIGTERM, &sa, NULL);

    ull id = strtoull(worker_id, NULL, 10);
    ull workers_num = strtoull(workers, NULL, 10);

    int fd;

    ERRCHECK(fd = open(ipc_path, O_RDWR, S_IWUSR | S_IRUSR), "[FileopsWorker %llu]: ERROR: Could not open ipc file %s\n", id, ipc_path);

    usz map_size = MAP_JOB_SIZE+sizeof(ipc_result_channel)*workers_num;

    global_job_capacity = 1024*1024;
    global_job_stack = malloc(sizeof(ipc_job)*global_job_capacity);

    void* map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if(map == MAP_FAILED) {
        fprintf(stderr, "[FileopsWorker %llu]: ERROR: Could not map the ipc file %s to memory\n", id, ipc_path);
        perror(NULL);
        return 3;
    }

    ipc_header* header = map;
    ipc_job* job_queue = (void*)&header[1];

    printf("[FileopsWorker %llu]: INFO: Children with pid %d of %d speaking! Yessir!\n", id, getpid(), getppid());
    ipc_result_channel* channels = (void*)&job_queue[QUEUE_JOB_LEN];
    channels[id].stats.worker_id = id;
    channels[id].stats.pid = getpid();

    while(1) {
        if (sigterm_received || header->quitting) {
            goto cleanup;
        }

        if (safe_sem_wait(&header->job_sem) == -1) {
            goto cleanup;
        }
        if(header->jobs_waiting == 0 && header->jobs_running == 0 && stack_at_end()) {
            printf("[FileopsWorker %llu] INFO: My job here is done\n", id);
            sem_post(&header->job_sem);
            sem_post(&header->queue_read_sem);
            goto cleanup;
        }
        sem_post(&header->job_sem);

        if (safe_sem_wait(&header->job_sem) == -1) {
            goto cleanup;
        }
        if(header->jobs_waiting == 0 && header->jobs_running == 0 && stack_at_end()) {
            printf("[FileopsWorker %llu] INFO: My job here is done\n", id);
            sem_post(&header->job_sem);
            sem_post(&header->queue_read_sem);
            goto cleanup;
        }
        sem_post(&header->job_sem);

        ipc_job job;

        if(!stack_at_end()) {
            stack_pop_job(&job);
            if (safe_sem_wait(&header->job_sem) == -1) {
                goto cleanup;
            }
            header->jobs_running++;
            header->jobs_waiting--;
            sem_post(&header->job_sem);
        } else {
            if (safe_sem_wait(&header->queue_read_sem) == -1) {
                goto cleanup;
            }
            if (safe_sem_wait(&header->job_sem) == -1) {
                goto cleanup;
            }

            if(header->jobs_waiting == 0) {
                sem_post(&header->job_sem);
                continue;
            }
            memcpy(job.path, job_queue[header->queue_tail].path, DB_STRING_LEN);
            job.depth = job_queue[header->queue_tail].depth;
            header->queue_tail = (header->queue_tail + 1) % QUEUE_JOB_LEN;

            header->jobs_running++;
            header->jobs_waiting--;

            sem_post(&header->job_sem);
            sem_post(&header->queue_write_sem);
        }

        DIR* dir = opendir(job.path);
        if (dir == NULL) {
            if (control_fd >= 0) {
                char msg_buf[256];
                int len = snprintf(msg_buf, sizeof(msg_buf), "T5MSG type=ERROR worker_id=%llu errno=%d where=opendir\n", id, errno);
                write(control_fd, msg_buf, len);
            }
            if (safe_sem_wait(&header->job_sem) != -1) {
                header->jobs_running--;
                sem_post(&header->job_sem);
            }
            continue;
        }

        struct dirent* entry;
        while((entry = readdir(dir))) {
            if (sigterm_received || header->quitting) {
                closedir(dir);
                goto cleanup;
            }
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            char p[DB_STRING_LEN] = {0};
            snprintf(p, DB_STRING_LEN, "%s/%s", job.path, entry->d_name);

            struct stat stat = {0};

            if (lstat(p, &stat) == -1) {
                if (control_fd >= 0) {
                    char msg_buf[256];
                    int len = snprintf(msg_buf, sizeof(msg_buf), "T5MSG type=ERROR worker_id=%llu errno=%d where=lstat\n", id, errno);
                    write(control_fd, msg_buf, len);
                }
                continue;
            }

            mode_t mode = stat.st_mode;
            if(S_ISLNK(mode)) {
                continue;
            }
            if(S_ISREG(mode)) {
                if (simulate_work_ms > 0) {
                    usleep(simulate_work_ms * 1000);
                }

                ipc_result_channel* channels = (void*)&job_queue[QUEUE_JOB_LEN];
                if (safe_sem_wait(&channels[id].write_sem) == -1) {
                    closedir(dir);
                    goto cleanup;
                }
                if (safe_sem_wait(&channels[id].sem) == -1) {
                    sem_post(&channels[id].write_sem);
                    closedir(dir);
                    goto cleanup;
                }

                ipc_result_channel* chan = &channels[id];
                ipc_result_record* rec = &chan->records[chan->records_head];
                chan->records_head = (chan->records_head + 1) % QUEUE_CHANNEL_LEN;

                int fd = open(p, O_RDONLY);
                if (fd == -1) {
                    if (control_fd >= 0) {
                        char msg_buf[256];
                        int len = snprintf(msg_buf, sizeof(msg_buf), "T5MSG type=ERROR worker_id=%llu errno=%d where=open\n", id, errno);
                        write(control_fd, msg_buf, len);
                    }
                    sem_post(&channels[id].sem);
                    sem_post(&channels[id].write_sem);
                    continue;
                }
                hash_file(fd, rec->hash);
                close(fd);

                char* abp = abspath(p);
                snprintf(rec->absolute_path, DB_STRING_LEN, "%s", abp);
                free(abp);

                rec->group_id = stat.st_gid;
                rec->user_id = stat.st_uid;
                rec->last_modification = stat.st_mtime;
                rec->mode = mode;
                rec->size = stat.st_size;
                chan->stats.files_emitted++;
                chan->stats.bytes_emitted += sizeof(ipc_result_record);

                sem_post(&channels[id].sem);
                sem_post(&channels[id].read_sem);

                continue;
            }
            if(S_ISDIR(mode)) {
                ipc_job njob = {0};
                snprintf(njob.path, DB_STRING_LEN, "%s", p);
                njob.depth = job.depth+1;
                if(njob.depth > header->max_depth) continue;
                if(sem_trywait(&header->queue_write_sem) == -1) {
                    if(errno == EAGAIN) {
                        stack_push_job(&njob);
                        if (safe_sem_wait(&header->job_sem) == -1) {
                            closedir(dir);
                            goto cleanup;
                        }
                        header->jobs_waiting++;
                        sem_post(&header->job_sem);
                        continue;
                    }else {
                        fprintf(stderr, "ERROR\n");
                        perror(0);
                        exit(1);
                    }
                } else {
                    if (safe_sem_wait(&header->job_sem) == -1) {
                        sem_post(&header->queue_write_sem);
                        closedir(dir);
                        goto cleanup;
                    }

                    memcpy(&job_queue[header->queue_head], njob.path, DB_STRING_LEN);
                    job_queue[header->queue_head].depth = njob.depth;

                    header->queue_head = (header->queue_head + 1) % QUEUE_JOB_LEN;
                    header->jobs_waiting++;

                    sem_post(&header->job_sem);
                    sem_post(&header->queue_read_sem);
                }
                continue;
            }
        }

        closedir(dir);

        channels[id].stats.jobs_processed++;

        if (control_fd >= 0) {
            char msg_buf[256];
            int len = snprintf(msg_buf, sizeof(msg_buf), "T5MSG type=JOB_DONE worker_id=%llu jobs=%u files=%u bytes=%llu\n",
                               id, channels[id].stats.jobs_processed, channels[id].stats.files_emitted, channels[id].stats.bytes_emitted);
            write(control_fd, msg_buf, len);
        }

        if (safe_sem_wait(&header->job_sem) == -1) {
            goto cleanup;
        }
        header->jobs_running--;
        printf("[FileopsWorker %llu]: INFO: %d waiting and %d running and %zu stacked\n", id, header->jobs_waiting, header->jobs_running, global_job_head);
        sem_post(&header->job_sem);
    }

    ASSERT(0, "UNREACHABLE");

cleanup:
    if (control_fd >= 0) {
        char msg_buf[128];
        int len = snprintf(msg_buf, sizeof(msg_buf), "T5MSG type=WORKER_EXITING worker_id=%llu reason=%s\n", id, sigterm_received ? "shutdown" : "normal");
        write(control_fd, msg_buf, len);
    }

    ipc_result_channel* chan = &channels[id];
    safe_sem_wait(&chan->sem);

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    chan->stats.user_cpu_us = usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec;
    chan->stats.sys_cpu_us = usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec;
    chan->stats.exit_status = 0;

    chan->done = true;

    sem_post(&chan->sem);
    sem_post(&header->workers_sem);
    munmap(map, map_size);

    return 0;
}
