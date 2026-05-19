# T5 Control Plane Protocol

## Communication Channel
- The control plane utilizes an anonymous pipe (`pipe(2)`) created by the manager.
- The read end is configured as non-blocking (`O_NONBLOCK`) in the manager.
- The write end is inherited by the worker processes, with the file descriptor value passed via the `--control-fd <fd>` CLI argument.

## T5MSG Format
Workers write structured, newline-terminated status strings to the control pipe.

### 1. Job Done
`T5MSG type=JOB_DONE worker_id=<id> jobs=<n> files=<n> bytes=<n>`
- Sent by a worker upon completing a directory indexing task.
- Reports the worker's cumulative jobs, files, and bytes emitted.

### 2. Worker Exiting
`T5MSG type=WORKER_EXITING worker_id=<id> reason=<reason>`
- Sent by a worker right before terminating.
- `reason` is `normal` for normal exits and `shutdown` if terminated by `SIGTERM`.

### 3. Error
`T5MSG type=ERROR worker_id=<id> errno=<err> where=<context>`
- Sent when a worker encounters a system call error.
- Contains the error number (`errno`) and the name of the function where it occurred (e.g., `opendir`, `lstat`, `open`).

## Signals
The manager and workers respond to system signals to manage state and report diagnostics.

### Manager Signals
- `SIGUSR1`: Triggers a status report written to `stdout` in the format:
  `STATUS queued_jobs=<n> active_jobs=<n> files=<n> bytes=<n> workers_alive=<n> complete=0`
- `SIGINT` / `SIGTERM`: Triggers graceful shutdown. The manager flags `quitting = true` in shared memory, sends `SIGTERM` to all workers, and waits up to `--graceful-timeout` seconds. Any workers still running after the timeout are terminated with `SIGKILL`.
- `SIGCHLD`: Notifies the manager that a worker has exited, allowing non-blocking reaping of child processes via `waitpid(..., WNOHANG)`.

### Worker Signals
- `SIGTERM`: Interrupts standard semaphore wait operations (`EINTR`), prompting the worker to clean up its resources, write its exit notification to the control pipe, update its final shared statistics, and exit.

## Atomic DB Updates
- The database is modified in a temporary file (e.g., `<db_path>.temp`).
- On exit, the manager closes the file and calls `rename()` to atomically replace the target database path. If the run was incomplete, `complete` is written as `2` (interpreted as `0` in `--dump`), indicating incomplete status.
