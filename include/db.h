#ifndef __DB_H
#define __DB_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

constexpr size_t DB_SIGNATURE_LEN = 32;
constexpr size_t DB_STRING_LEN = 64;

typedef enum {
    SNAPSHOT_OPEN = 0,
    SNAPSHOT_SEALED
} db_snapshot_state;

typedef uint32_t(*db_generate_snapshot_func)(const char*);

typedef struct {
    char              signature[DB_SIGNATURE_LEN];
    uint8_t           format_version;
    uint32_t          snapshot_id;
    db_snapshot_state snapshot_state;
    uint8_t           active_writers;
    uint32_t          record_count;
} db_header;


typedef struct {
    size_t      count;
    size_t      capacity;
    uint32_t*   offsets;
} db_offset_table;

typedef enum {
    DB_STRING,
    DB_INT,
    DB_DOUBLE
} db_column_kind;

typedef struct {
    db_column_kind kind;
    const char*    name;
} db_column;

typedef struct {
    union {
        char        str_value[DB_STRING_LEN];
        int         int_value;
        double      double_value;
    };
} db_column_value;

typedef struct {
    uint16_t size;
    char*    str;
} db_varchar;

typedef int(*db_row_filter_func)(db_column_value*, size_t);

typedef struct {
    char signature[32];
    db_column* columns;
    size_t     count;
} db_schema;

typedef struct {
    db_schema                 schema;
    db_generate_snapshot_func generate_snapshot;
    char*                     filepath;
    int                       fd;
} db_connection;

bool db_open_connection(db_connection* connection, const char* filepath, db_schema* schema, db_generate_snapshot_func generate_snapshot);
void db_select(db_connection* connection, db_column* columns, db_row_filter_func filter);
void db_update(db_connection* connection, db_column* columns, db_column_value* values, size_t count, db_row_filter_func filter);
void db_insert(db_connection* connection, db_column* columns, db_column_value* values, size_t count);
void db_close_connection(db_connection* connection);

#endif
