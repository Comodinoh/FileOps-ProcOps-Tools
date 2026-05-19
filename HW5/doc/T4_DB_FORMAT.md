# T4 Database Format

## File Structure
The database file is divided into three consecutive sections:
1. **Database Header** (Offset 0)
2. **File Records Array** (Offset `sizeof(ipc_db_header)`)
3. **Worker Stats Array** (Offset `sizeof(ipc_db_header) + sizeof(ipc_result_record) * file_records`)

## Header
The database header is represented by `ipc_db_header`:
- `sig` (`char[32]`): Signature identifying the inventory database, always `"INV"`.
- `format` (`u32`): Database version format, set to `1`.
- `complete` (`u8`): Completion state flag:
  - `0`: Incomplete / Started (`DB_START`)
  - `1`: Fully completed without interruption (`DB_FULL_COMPLETE`)
  - `2`: Partially completed after graceful shutdown (`DB_PART_COMPLETE`)
- `file_records` (`u32`): Total number of file records stored in the database.
- `workers` (`u32`): Number of workers that contributed to this database.

## Record Structs
Each file record is represented by `ipc_result_record`:
- `absolute_path` (`char[1024]`): The absolute path of the indexed file.
- `hash` (`char[32]`): SHA-256 hash output of the file contents.
- `size` (`u64`): File size in bytes.
- `mode` (`u32`): File access mode and type flags (from `stat`).
- `user_id` (`u32`): Owner user ID.
- `group_id` (`u32`): Owner group ID.
- `last_modification` (`u64`): Last modification epoch timestamp.

## Worker Stats Structs
Each worker's execution stats are stored in `ipc_stats`:
- `worker_id` (`u32`): The ID assigned to the worker.
- `pid` (`u32`): The PID of the worker process.
- `exit_status` (`u32`): The exit code of the worker.
- `jobs_processed` (`u32`): The number of directories processed by the worker.
- `files_emitted` (`u32`): The number of regular files successfully indexed.
- `bytes_emitted` (`u64`): The cumulative bytes written to the result channels.
- `real_time_ms` (`u64`): The elapsed time in milliseconds.
- `user_cpu_us` (`u64`): User CPU consumption in microseconds.
- `sys_cpu_us` (`u64`): System CPU consumption in microseconds.

## Validity Rules
1. Signature must match `"INV"`.
2. Format version must match `1`.
3. Structural verification passes regardless of `complete` status, as long as header fields match and file size is sufficient to contain all records and statistics.
