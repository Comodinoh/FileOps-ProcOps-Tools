#ifndef __FILEOPS_IPC_H
#define __FILEOPS_IPC_H

#include "db.h"
#include "types.h"

#include <linux/limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define QUEUE_JOB_LEN 1024
#define QUEUE_CHANNEL_LEN 64
#define PACKED __attribute__((packed))

#define DB_START 0
#define DB_FULL_COMPLETE 1
#define DB_PART_COMPLETE 2

typedef struct {
  char signature[DB_STRING_LEN];

  usz max_depth;

  sem_t job_sem;
  u32 jobs_running;
  u32 jobs_waiting;

  sem_t queue_sem;
  sem_t queue_write_sem;
  sem_t queue_read_sem;
  u32 queue_head;
  u32 queue_tail;

  u8 version;
  sem_t quit_sem;
  bool quitting;

  sem_t workers_sem;
} ipc_header;

typedef struct {
  char path[DB_STRING_LEN];
  usz depth;
} ipc_job;

typedef struct {
  char absolute_path[DB_STRING_LEN];
  u32 size;
  u32 last_modification;
  mode_t mode;
  id_t user_id;
  gid_t group_id;
  uchar hash[32];
} ipc_result_record;

typedef struct {
  uint32_t worker_id;
  pid_t pid;
  int exit_status;
  u32 jobs_processed;
  u32 files_emitted;
  u64 bytes_emitted;
  u64 real_time_ms;
  u64 user_cpu_us;
  u64 sys_cpu_us;
} ipc_stats;

typedef struct {
  sem_t sem;
  sem_t write_sem;
  sem_t read_sem;
  u32 records_head;
  u32 records_tail;
  bool done;
  ipc_stats stats;
  ipc_result_record records[QUEUE_CHANNEL_LEN];
} ipc_result_channel;

typedef struct {
  char sig[DB_SIGNATURE_LEN];
  u32 format;
  u8 complete;
  u32 file_records;
  u32 workers;
} ipc_db_header;

typedef struct {
  u32 jobs;
  u32 files;
  u64 bytes;
  bool exited;
} manager_worker_state;

typedef struct {
  void *map;
  ipc_header *header;
  ipc_job *queue;
  char temp_path[PATH_MAX];
  char og_path[PATH_MAX];
  usz out_channels;
  usz workers;
  usz map_size;
  ipc_db_header *db_header;
  FILE *db;
} ipc_conn;

typedef struct {
    int     control_pipe_read_fd;
    pid_t*  worker_pids;
    int     graceful_timeout;
} ipc_collect_params;

typedef enum { IPC_NONE = 0, IPC_INV, IPC_DB } ipc_mode;

typedef enum {
  IPC_ACTION_NONE = 0,
  IPC_ACTION_DUMP,
  IPC_ACTION_VERIFY
} ipc_db_action;

constexpr usz MAP_JOB_SIZE =
    sizeof(ipc_header) + sizeof(ipc_job) * QUEUE_JOB_LEN;

int manager_init(ipc_conn *conn, const char *root, const char *ipc_path,
                 const char *db_path, usz workers, usz max_depth);
void manager_collect_and_wait(ipc_conn *conn, ipc_collect_params *params);
void manager_quit(ipc_conn *conn);
void manager_signal_sigusr1();
void manager_signal_shutdown();
void manager_signal_sigchld();

void manager_insert_result(ipc_conn *conn, ipc_result_record *record);

#endif
