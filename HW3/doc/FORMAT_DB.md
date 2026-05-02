**C Standard**: GNU C (specifically gnu23)

Note: in the C23 standard the `constexpr` keyword is fully supported as a constant expression at compile time, as such it has been preferred to be used over usual `#define` macros. Only cases where we'd need to use macros is if we want to define strings, in that case it can only be done via `#define` sadly.

## Header Structure
Header struct is defined with the attribute `__attribute__((packed))` directive to prevent padding, it might negatively impact performance on reads and writes but I couldn't be bothered to figure out how to organise all of the fields such that it's properly aligned without wasting too much memory.

Note: `DB_SIGNATURE_LEN` constexpr is defined as `32`.


|   Field        | Type                             | Description |
|----------------|----------------------------------|-------------|
| signature      | `char[DB_SIGNATURE_LEN]`                       | Magic identifier for the db type (`"IDX"` for file indexer, `"PROC"` for procs) |
| format_version | `uint8_t`                        | Version number of the format (currently 1) |
| snapshot_id    | `uint32_t`                       | Unique ID of the current snapshot |
| snapshot_state | `db_snapshot_state` (`uint8_t`)  | State of the snapshot (`0` is `SNAPSHOT_OPEN`, `1` is `SNAPSHOT_SEALED`) |
| active_writers | `uint8_t`                        | Number of concurrent instances contributing to the snapshot |
| record_count   | `uint32_t`                       | Total number of records |

## Record Structures

### File Indexer
Each entry in the file database has a fixed size and is packed without padding.

Note: `DB_STRING_LEN` constexpr is defined as `1024`.

|   Field           | Type                              | Description |
|-------------------|-----------------------------------|-------------|
| absolute_path     | `char[DB_STRING_LEN]`             | Absolute path of the file, directory, or symlink (It doesnt follow symlinks) |
| type              | `db_indexer_ftype` (`uint8_t`)    | File type (`FILE_TYPE_REGULAR`, `FILE_TYPE_DIR`, `FILE_TYPE_SYM`, `FILE_TYPE_FIFO`) |
| size              | `uint32_t`                        | Size of the file in bytes (0 for non-regular files) |
| last_modification | `db_snapshot_state` (`uint8_t`)   | State of the snapshot (`0` is `SNAPSHOT_OPEN`, `1` is `SNAPSHOT_SEALED`) |
| hash              | `size_t`                          | Deterministic hash for the contents of regular files (For more explanations see down below) |
| symlink           | `bool`                            | Flag indicating if the record is a symlink |
| symlink_target    | `char[DB_STRING_LEN]`             | Path that the symlink points to (`NULL` if it's not a symlink) |
| st_ino            | `uint32_t`                        | inode number of the file |
| st_dev            | `uint32_t`                        | device number of the file |

### Procs
Each entry in the procs database has a fixed size and is packed without padding.

|   Field           | Type                              | Description |
|-------------------|-----------------------------------|-------------|
| pid   | `pid_t`     | Process ID        |
| ppid  | `pid_t`    | Parent Process ID |
| state | `char[4]` | Process state (`R` - Running, `X` - Dead, `T` - Stopped, `S` - Asleep waiting, etc.) |
| comm | `char[16]` | Filename of the executable truncated to 16 characters and without parens |
| cmdline | `char[PATH_MAX/256]` | Command-line used to start the process truncated to 16 characters |
| rss | `uint64_t`  | Resident Set Size, the pages the process has in real memory in Kilobytes |
| rss_source | `char[8]` | Source from which the rss value has been fetched (`/stat` in the case of the main_procs.c implementation)|
| cpu_time | `uint64_t` | user time (ticks) + kernel time (ticks) / ticks per second = total time taken by the process in seconds |
## Hashing
Hashing of regular file contents is done through a simple algorithm.

djb2 is a very simple and weak hashing algorithm for strings dating back from the 90s,
it initialises a sum to `5381`, goes through all the string and for each iteration multiplies the current sum by `33` and adds the `char` value of the current character. those numbers are used to provide better avalanching and spread out the digits on small differences in characters in the string.

Fun fact: I first found out about this lil algorithm on a Tsoding video on youtube about hash tables! It was pretty helpful :)
If you want more info check out `https://youtu.be/n-S9DBwPGTo?si=TSFzCvlE-qmKw2fd` (he talks about djb2 around timestamp 1:57:00)

## Update Strategy and Synchronization
The database stores fixed-size records sequentially immediately following the header.
Processes operate under the Single Program, Multiple Data model. They synchronize directly on the database file using exclusive locks of type `fcntl(2)` via the wrapper functions in `lock.c`.

Global header fields like `record_count`, `active_writers`, and `snapshot_state` are protected from concurrent writes by applying region locks directly at their specific byte offsets (e.g., `DB_WRITERS_OFFSET`).

The first instance creates the file, initializing `active_writers=1` and `snapshot_state=SNAPSHOT_OPEN`. Subsequent instances connect, validate the `snapshot_id`, and increment `active_writers`. When an instance finishes, it decrements `active_writers`. The instance that reduces this value to 0 marks the snapshot as `SNAPSHOT_SEALED`.

## Validity Conditions
1.  The db file must exist.
2.  The signature must exactly match `DB_FILE_INDEXER_SIGNATURE` (`"IDX"`) or `DB_PROCS_SIGNATURE` (`"PROC"`).
3.  The format version must match the current format version (`1`).
4.  There must be no duplicate primary keys (`absolute_path` for files; `pid` for procs) within a valid snapshot.

## DBDiff Comparison Rules
This diff utility first loads all records of both dbs into memory. sorts them using `qsort` from the stdlib and uses a list merging similar approach to filter for modified, new and deleted records.

**Sorting:** Records are sorted in ascending alphabetical order (A-Z) using `qsort`
- For files, `idx_row_compare` uses `strcmp` on the `absolute_path`
- For processes, `procs_row_compare` subtracts the two pids to get a comparable value

**Merging:**
- If the old record comes before the new record, the old record is missing from the new database and is marked as **DISAPPEARED**.
- If the new record comes before the old record, it is a new addition and is marked as **APPEARED**.
- If the keys match, the fields are compared based on the following criteria for each type of db:
    - For file databases, a matched record is marked as `MODIFIED` if any of the following differ:    
        - Type (`type`)
        - Size (`size`)
        - Modification time (`last_modification`)
        - Hash (`hash`)
        - Symlink target (`symlink_target` via `strcmp`)
    - For procs databases, a matched record is marked as `SIGNIFICANTLY CHANGED` if any of the following differ in the following manner:
        - RSS1 (`rss`) - RSS2 (`rss`) >= `DB_PROCS_RSS_THRESHOLD` (the chosen thresold in this case is 512KB a.k.a. 0.5MB)
