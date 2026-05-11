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
#include <openssl/evp.h>

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

void stack_pop_job(ipc_job* restrict job) {
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
    // Term interface is minimal since we dont execute this manually
    if(argc < 3) {
        return 1;
    }

    char* worker_id = NULL;
    char* ipc_path = NULL;
    char* workers = NULL;

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
        }
        argv++;
    }

    if(!ipc_path || !worker_id || !workers) return 2;

    ull id = strtoull(worker_id, NULL, 10);
    ull workers_num = strtoull(workers, NULL, 10);

    int fd;

    ERRCHECK(fd = open(ipc_path, O_RDWR, S_IWUSR | S_IRUSR), "[FileopsWorker %llu]: ERROR: Could not open ipc file %s\n", id, ipc_path);

    usz map_size = MAP_JOB_SIZE+sizeof(ipc_result_channel)*workers_num;

    global_job_capacity = 32;
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

    while(1) {

        if(header->quitting && stack_at_end()) {
            sem_post(&header->queue_read_sem);
            goto cleanup;
        }

        ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for job semaphore\n", id);
        if(header->jobs_waiting == 0 && header->jobs_running == 0 && stack_at_end()) {
            printf("[FileopsWorker %llu] INFO: My job here is done\n", id);
            ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
            sem_post(&header->queue_read_sem);
            sem_wait(&header->quit_sem);
            header->quitting = true;
            sem_post(&header->quit_sem);
            goto cleanup; // :)
        }

        ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);


        sem_wait(&header->queue_read_sem);

        ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for job semaphore\n", id);
        if(header->jobs_waiting == 0 && header->jobs_running == 0 && stack_at_end()) {
            printf("[FileopsWorker %llu] INFO: My job here is done\n", id);
            ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
            sem_post(&header->queue_read_sem);
            sem_wait(&header->quit_sem);
            header->quitting = true;
            sem_post(&header->quit_sem);
            goto cleanup; // :)
        }
        ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);

        ipc_job job;

        if(!stack_at_end()) {
            stack_pop_job(&job);
            ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for job semaphore\n", id);
            header->jobs_running++;
            header->jobs_waiting--;
            ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
        } else {
            ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for job semaphore\n", id);
            memcpy(job.path, job_queue[header->queue_tail].path, DB_STRING_LEN);
            header->queue_tail = (header->queue_tail + 1) % QUEUE_JOB_LEN;


            header->jobs_running++;
            header->jobs_waiting--;

            printf("[FileopsWorker %llu]: INFO: Took on job %s\n",  id, job.path);

            ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
            sem_post(&header->queue_write_sem);
        }


        DIR* dir = opendir(job.path);

        // printf("[FileopsWorker %llu]: INFO: Opened directory %s\n", id, job);

        ERRCHECKNULL(dir, "[FileopsWorker %llu]: ERROR: Could not open directory %s\n", id, job.path);

        struct dirent* entry;
        while((entry = readdir(dir))) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            // printf("[FileopsWorker %llu]: INFO: Reading directory entry %s\n", id, entry->d_name);

            char p[DB_STRING_LEN] = {0};
            snprintf(p, DB_STRING_LEN, "%s/%s", job.path, entry->d_name);

            struct stat stat = {0};

            ERRCHECK(lstat(p, &stat), "[FileopsWorker %llu]: ERROR: Could not read stat of file %s\n", id, p);

            mode_t mode = stat.st_mode;
            if(S_ISLNK(mode)) {
                continue;
            }
            if(S_ISREG(mode)) {
                ipc_result_channel* channels = (void*)&job_queue[QUEUE_JOB_LEN];
                ERRCHECK(sem_wait(&channels[id].write_sem), "[FileopsWorker %llu]: ERROR: Could not wait for channel %llu write semaphore\n", id, id);
                ERRCHECK(sem_wait(&channels[id].sem), "[FileopsWorker %llu]: ERROR: Could not wait for channel %llu semaphore\n", id, id);

                ipc_result_channel* chan = &channels[id];

                ipc_result_record* rec = &chan->records[chan->records_head];
                chan->records_head = (chan->records_head + 1) % QUEUE_CHANNEL_LEN;

                int fd;
                ERRCHECK(fd = open(p, O_RDONLY), "[FileopsWorker %llu]: ERROR: Could not open file %s\n", id, p);
                hash_file(fd, rec->hash);
                close(fd);

                char* abp = abspath(p);
                snprintf(rec->absolute_path, DB_STRING_LEN, "%s", abp);
                free(abp);

                // printf("%s\n", rec->absolute_path);

                rec->group_id = stat.st_gid;
                rec->user_id = stat.st_uid;
                rec->last_modification = stat.st_mtime;
                rec->mode = mode;
                rec->size = stat.st_size;

                ERRCHECK(sem_post(&channels[id].sem), "[FileopsWorker %llu]: ERROR: Could not post channel %llu semaphore\n", id, id);
                ERRCHECK(sem_post(&channels[id].read_sem), "[FileopsWorker %llu]: ERROR: Could not post channel %llu read semaphore\n", id, id);

                continue;
            }
            if(S_ISDIR(mode)) {
                // printf("[FileopsWorker %llu]: INFO: Discovered new job %s\n", id, p);
                ipc_job njob;
                snprintf(njob.path, DB_STRING_LEN, "%s", p);
                njob.depth = job.depth+1;
                if(sem_trywait(&header->queue_write_sem) == -1) {
                    if(errno == EAGAIN) {
                        stack_push_job(&njob);
                        ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for queue semaphore\n", id);
                        header->jobs_waiting++;
                        ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
                        ERRCHECK(sem_post(&header->queue_read_sem), "[FileopsWorker %llu]: ERROR: Could not post queue read semaphore\n", id);
                        printf("[FileopsWorker %llu]: INFO: Registered new internal job %s\n", id, p);
                    }else {
                        fprintf(stderr, "ERROR\n");
                        perror(0);
                        exit(1);
                    }
                } else {
                    ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for queue semaphore\n", id);

                    memcpy(&job_queue[header->queue_head], njob.path, DB_STRING_LEN);
                    job_queue[header->queue_head].depth = njob.depth;
                    // printf("[FileopsWorker %llu]: INFO: Registered new job %s\n", id, p);

                    header->queue_head = (header->queue_head + 1) % QUEUE_JOB_LEN;
                    header->jobs_waiting++;



                    ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
                    ERRCHECK(sem_post(&header->queue_read_sem), "[FileopsWorker %llu]: ERROR: Could not post queue read semaphore\n", id);
                }
                continue;
            }
        }

        closedir(dir);

        printf("[FileopsWorker %llu]: INFO: Consumed job %s\n", id, job.path);

        ERRCHECK(sem_wait(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not wait for job semaphore\n", id);
        header->jobs_running--;
        // printf("[FileopsWorker %llu]: INFO: %d waiting and %d running\n", id, header->jobs_waiting, header->jobs_running);
        ERRCHECK(sem_post(&header->job_sem), "[FileopsWorker %llu]: ERROR: Could not post job semaphore\n", id);
    }

    ASSERT(0, "UNREACHABLE");

cleanup:

    ERRCHECK(sem_post(&header->workers_sem), "[FileopsWorker %llu]: ERROR: Could not post workers semaphore\n", id);
    ERRCHECK(munmap(map, map_size), "[FileopsWorker %llu]: ERROR: Could not unmap ipc\n", id);

    return 0;
}
