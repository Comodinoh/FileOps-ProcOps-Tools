#ifndef __DB_H
#define __DB_H

#include <stdint.h>
#include <stdlib.h>

#define ERRCHECK(x, ...) do { if((x) == -1) {__VA_OPT__(fprintf(stderr, __VA_ARGS__);) perror(NULL);} } while(0)

typedef enum {
    SNAPSHOT_OPEN = 0,
    SNAPSHOT_SEALED
} db_snapshot_state;

typedef uint32_t(*db_generate_snapshot_func)(const char*);

typedef struct {
    char              signature[32];
    uint8_t           format_version;
    uint32_t          snapshot_id;
    db_snapshot_state snapshot_state;
    uint8_t           active_writers;
    uint32_t          record_count;
} db_header;

typedef enum {
    STRING,
    INT,
    DOUBLE
} db_column_kind;

typedef struct {
    db_column_kind kind;
    const char*    name;
} db_column;

typedef struct {
    union {
        const char* str_value;
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
    const char*               filepath;
    int                       fd;
} db_connection;

int  db_open_connection(db_connection* connection, const char* filepath, db_schema* schema, db_generate_snapshot_func generate_snapshot);
int db_select_header(db_connection* connection, db_header* header);
int db_update_header(db_connection* connection, db_header new_header);
int db_select(db_connection* connection, db_column* columns, db_row_filter_func filter);
int db_update(db_connection* connection, db_column* columns, db_column_value* values, size_t count, db_row_filter_func filter);
int db_insert(db_connection* connection, db_column* columns, db_column_value* values, size_t count);
void db_close_connection(db_connection* connection);

void db_varchar_free(db_varchar* varchar);

void _db_parse_varchar(db_connection* connection, db_varchar* varchar);


#endif
