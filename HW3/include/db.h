#ifndef __DB_H
#define __DB_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

constexpr size_t DB_SIGNATURE_LEN = 32;
constexpr size_t DB_STRING_LEN = 64;
constexpr size_t DB_NAME_LEN = 64;

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

typedef enum {
    DB_STRING,
    DB_INT
} db_column_kind;

typedef struct {
    db_column_kind  kind;
    const char*     name;
    bool            pkey;
} db_column_schema;

typedef struct {
    char name[DB_NAME_LEN];
} db_column_name;

typedef struct {
    union{
        char    str_value[DB_STRING_LEN];
        int     int_value;
        double  double_value;
    };
} db_column;


typedef struct {
    char name[DB_NAME_LEN];
    db_column column;
} db_column_name_pair;

static db_column_name_pair NULL_COLUMN_VALUE = {.name = {'\0'}};

struct db_connection;

typedef int(*db_row_filter_func)(db_column*, size_t);
typedef bool(*db_insert_func)(struct db_connection*);

typedef struct {
    char signature[32];
    db_column_schema* columns;

    size_t     count;
} db_schema;

typedef struct {
    db_schema                 schema;
    db_generate_snapshot_func generate_snapshot;
    char*                     filepath;
    int                       fd;
    size_t                    record_size;
} db_connection;

typedef struct {
    char**  array;
    bool    cleanup;
} db_strings;

typedef struct {
    db_column*  array;
    db_column_name*  name_array;
    size_t      count;
    bool        cleanup;
} db_columns;

db_strings db_make_strings(bool cleanup, ...);
db_columns db_make_values(bool cleanup, ...);
db_column_name_pair db_make_namecol(const char* name, db_column col);

int db_tell(db_connection* connection);
bool db_open_connection(db_connection* connection, const char* filepath, db_schema* schema, db_generate_snapshot_func generate_snapshot);
void db_select(db_connection* connection, db_columns* values, db_row_filter_func filter);
void db_update(db_connection* connection, db_columns  values, db_row_filter_func filter);
bool db_insert(db_connection* connection, db_columns  values);
void db_inc_record_count(db_connection* connection);
void db_dec_record_count(db_connection* connection);
uint32_t db_record_count(db_connection* connection);
void db_close_connection(db_connection* connection);

#endif
