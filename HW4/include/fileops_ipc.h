#ifndef __FILEOPS_IPC_H
#define __FILEOPS_IPC_H

#include "db.h"
#include "types.h"

#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>

#define QUEUE_JOB_LEN 1024
#define QUEUE_CHANNEL_LEN 64
#define PACKED __attribute__((packed))

typedef struct {
    char    signature[DB_STRING_LEN];

    sem_t   job_sem;
    u32      jobs_running;
    u32      jobs_waiting;

    sem_t   queue_sem;
    sem_t   queue_write_sem;
    sem_t   queue_read_sem;
    u32     queue_head;
    u32     queue_tail;


    u8      version;
    sem_t   quit_sem;
    bool    quitting;

    sem_t   workers_sem;

    //TODO: add worker stats
} ipc_header;

typedef struct {
    char    path[DB_STRING_LEN];
    usz     depth;
} ipc_job;


typedef struct {
    char    absolute_path[DB_STRING_LEN];
    u32     size;
    u32     last_modification;
    mode_t  mode;
    id_t    user_id;
    gid_t   group_id;
    uchar   hash[32];
} ipc_result_record;

typedef struct {
    sem_t               sem;
    sem_t               write_sem;
    sem_t               read_sem;
    u32                 records_head;
    u32                 records_tail;
    bool                done;
    ipc_result_record   records[QUEUE_CHANNEL_LEN];
} ipc_result_channel;

typedef struct {
    void*       map;
    ipc_header* header;
    ipc_job*    queue;
    usz         out_channels;
    usz         workers;
    usz         map_size;
} ipc_conn;


constexpr usz MAP_JOB_SIZE = sizeof(ipc_header)+sizeof(ipc_job)*QUEUE_JOB_LEN;

int manager_init(ipc_conn* conn, const char* root, const char* ipc_path, usz workers);
void manager_collect_and_wait(ipc_conn* conn);
void manager_quit(ipc_conn *conn);

#endif
