#ifndef __FILEOPS_IPC_H
#define __FILEOPS_IPC_H

#include "db.h"
#include "types.h"

#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>

#define QUEUE_JOB_LEN 16
#define QUEUE_CHANNEL_LEN 4
#define PACKED __attribute__((packed))

typedef struct {
    char    signature[DB_STRING_LEN];

    sem_t   job_sem;
    u8      jobs_running;
    u8      jobs_waiting;

    u8      queue_head;
    u8      queue_tail;

    u8      version;

    //TODO: add worker stats
} ipc_header;

typedef char ipc_job[DB_STRING_LEN];

typedef struct {
    char    absolute_path[DB_STRING_LEN];
    u32     size;
    u32     last_modification;
    mode_t  mode;
    id_t    user_id;
    gid_t   group_id;
    u64     hash;
} ipc_result_record;

typedef struct {
    sem_t               sem;
    ipc_result_record   record[QUEUE_CHANNEL_LEN];
} ipc_result_channel;

typedef struct {
    void* map;
    u32   out_channels;
} ipc_conn;


void manager_init(ipc_conn* conn, const char* ipc_path, usz N);
void manager_quit(ipc_conn *conn);

#endif
