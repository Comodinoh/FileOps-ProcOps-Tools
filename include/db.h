#ifndef __DB_H
#define __DB_H

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    SNAPSHOT_OPEN = 0,
    SNAPSHOT_SEALED
} db_snapshot_state;

typedef uint32_t(*db_generate_snapshot_func)(const char*);


typedef struct {
    const char*       signature;
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

typedef int(*db_row_filter_func)(db_column_value*, size_t);

typedef struct {
    const char* signature;
    db_column* columns;
    size_t     count;
} db_schema;

typedef struct {
    size_t                    row_size;
    db_schema                 schema;
    db_generate_snapshot_func generate_snapshot;
    const char*               filepath;
} db_connection;

int  db_open_connection(db_connection* connection, const char* filepath, db_schema* schema, db_generate_snapshot_func generate_snapshot);
void db_select_header(db_connection* connection, db_header* header);
void db_update_header(db_connection* connection, db_header new_header);
void db_select(db_connection* connection, db_column* columns, db_row_filter_func filter);
void db_update(db_connection* connection, db_column* columns, db_column_value* values, size_t count, db_row_filter_func filter);
void db_insert(db_connection* connection, db_column* columns, db_column_value* values, size_t count);
void db_close_connection(db_connection* connection);


#endif
