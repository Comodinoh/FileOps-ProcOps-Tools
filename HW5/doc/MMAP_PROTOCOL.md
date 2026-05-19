# MMAP IPC Protocol

## Shared Memory Layout
The shared memory segment is created by the manager using `mmap` with a size of:
`sizeof(ipc_header) + sizeof(ipc_job) * 1024 + sizeof(ipc_result_channel) * workers`

Layout diagram:
1. `ipc_header` (starts at offset 0)
2. `ipc_job` queue (starts at `sizeof(ipc_header)`)
3. `ipc_result_channel` array (starts at `sizeof(ipc_header) + sizeof(ipc_job) * 1024`)

## Synchronization
Access to the shared memory structures is synchronized using POSIX semaphores initialized in shared mode:
- `job_sem`: Mutex protecting job counters (`jobs_waiting`, `jobs_running`) and the `quitting` state.
- `queue_read_sem` / `queue_write_sem`: Counting semaphores representing the number of readable jobs and writable slots in the circular job queue.
- `workers_sem`: Semaphore tracked for remaining active workers.
- For each worker result channel `i`:
  - `channels[i].sem`: Mutex protecting channel metadata and records.
  - `channels[i].read_sem` / `channels[i].write_sem`: Counting semaphores for result record slots.

## Job Queue
- Circular buffer of size `QUEUE_JOB_LEN` (1024).
- Workers pull jobs when `queue_read_sem` is posted, or fallback to their local stacks.
- New directories discovered are pushed to the queue if space exists, otherwise pushed to local worker stacks.

## Shutdown Flag
- `header->quitting`: Boolean flag set by the manager. When true, workers stop fetching jobs and exit cleanly.
