#ifndef __DB_H
#define __DB_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

constexpr size_t DB_SIGNATURE_LEN = 32;
constexpr size_t DB_STRING_LEN = 1024;

typedef enum : uint8_t {
    SNAPSHOT_OPEN = 0,
    SNAPSHOT_SEALED = 1
} db_snapshot_state;

constexpr size_t DB_WRITERS_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(db_snapshot_state)+sizeof(uint32_t);
constexpr size_t DB_SNAPSTATE_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(uint32_t);
constexpr size_t DB_RECORD_COUNT_OFFSET = DB_WRITERS_OFFSET+sizeof(uint8_t);

typedef uint32_t(*db_generate_snapshot_func)(const char*);

// nightmare without struct packing
// TODO: organize the fields better for minimum packing
typedef struct __attribute__((packed)) {
    char              signature[DB_SIGNATURE_LEN];
    uint8_t           format_version;
    uint32_t          snapshot_id;
    db_snapshot_state snapshot_state;
    uint8_t           active_writers;
    uint32_t          record_count;
} db_header;

struct db_connection;

typedef struct {
    db_generate_snapshot_func generate_snapshot;
    char*                     filepath;
    int                       fd;
    size_t                    record_size;
} db_connection;

int db_tell(db_connection* connection);

void db_inc_record_count(db_connection* connection);
void db_dec_record_count(db_connection* connection);
uint32_t db_record_count(db_connection* connection);

bool db_open_connection(db_connection* connection, const char* filepath, const char* signature, db_generate_snapshot_func generate_snapshot);
void db_close_connection(db_connection* connection);

#endif
