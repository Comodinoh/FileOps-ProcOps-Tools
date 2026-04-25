#include "db.h"

#include "util.h"
#include "lock.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

constexpr size_t FORMAT_VERSION = 1;

constexpr size_t DB_WRITERS_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(db_snapshot_state)+sizeof(uint32_t);
constexpr size_t DB_SNAPSTATE_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(uint32_t);
constexpr size_t DB_RECORD_COUNT_OFFSET = DB_WRITERS_OFFSET+sizeof(uint8_t);

db_strings db_make_strings(bool cleanup, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, NULL);
    va_copy(args_copy, args);

    size_t i = 0;
    
    while(va_arg(args_copy, const char*) != NULL) {
        i++; 
    }
    i++;
    va_end(args_copy);

    char** strings = malloc(sizeof(char*)*i);
    
    const char* str;
    size_t j = 0;
    while((str = va_arg(args, const char*)) != NULL) {
       strings[j++] = strdup(str);
    }
    strings[j++] = NULL;

    va_end(args);

    return (db_strings){.array = strings, .cleanup = cleanup};
}

db_columns db_make_values(bool cleanup, ...) {
    va_list args;
    va_list args_copy;
    va_start(args, NULL);
    va_copy(args_copy, args);

    size_t i = 0;
    
    while(va_arg(args_copy, db_column_name_pair).name[0] != '\0') {
        i++; 
    }
    i++;
    va_end(args_copy);

    db_column* values = malloc(sizeof(db_column)*i);
    db_column_name* names = malloc(sizeof(db_column_name)*i);
    
    db_column_name_pair pair;
    size_t j = 0;
    // Kinda ugly but it is what it is
    while((pair = va_arg(args, db_column_name_pair)).name[0] != '\0'){
        memcpy(names[j].name, pair.name, DB_NAME_LEN);
        values[j++] = pair.column;
    }

    va_end(args);

    return (db_columns){.array = values, .name_array = names, .count = i-1, .cleanup = cleanup};
}

int db_tell(db_connection* connection) {
    // TODO: possible memory leak because we arent closing the FILE stream
    FILE* f;
    ERRCHECKNULL(f = fdopen(connection->fd, "rb"), "Could not associate a stream with the open fd for %s\n", connection->filepath);

    return ftell(f);
}

void db_inc_record_count(db_connection* connection) {
    int fd = connection->fd;
    uint32_t record_count;

    int temp = db_tell(connection);

    lseek(fd, DB_WRITERS_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(record_count));
    ERRCHECK(read(fd, &record_count, sizeof(record_count)), "Could not read writers from header from %s\n", connection->filepath);
    lseek(fd, -sizeof(record_count), SEEK_CUR);
    

    record_count++;

    ERRCHECK(write(fd, &record_count, sizeof(record_count)), "Could not write record_count to header from %s\n", connection->filepath);


    lseek(fd, -sizeof(record_count), SEEK_CUR);
    db_unlock_region(fd, 0, sizeof(record_count));

    lseek(fd, temp, SEEK_SET);
}

void db_dec_record_count(db_connection* connection) {
    
    int fd = connection->fd;
    uint32_t record_count;

    int temp = db_tell(connection);

    lseek(fd, DB_WRITERS_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(record_count));
    ERRCHECK(read(fd, &record_count, sizeof(record_count)), "Could not read writers from header from %s\n", connection->filepath);
    lseek(fd, -sizeof(record_count), SEEK_CUR);
    
    ASSERT(record_count != 0, "Record count cannot be negative");

    record_count--;

    ERRCHECK(write(fd, &record_count, sizeof(record_count)), "Could not write record_count to header from %s\n", connection->filepath);


    lseek(fd, -sizeof(record_count), SEEK_CUR);
    db_unlock_region(fd, 0, sizeof(record_count));

    lseek(fd, temp, SEEK_SET);
}

db_column_name_pair db_make_namecol(const char* name, db_column col) {
    db_column_name_pair pair;

    size_t in_len = strlen(name);
    size_t len = in_len > DB_NAME_LEN ? DB_NAME_LEN-1 : in_len;

    memcpy(pair.name, name, len);

    pair.name[len] = '\0';

    pair.column = col;

    return pair;
}


uint32_t db_record_count(db_connection* connection) {
    int fd = connection->fd;
    
    int temp = db_tell(connection);

    lseek(fd, DB_RECORD_COUNT_OFFSET, SEEK_SET);

    db_lock_read_region_wait(fd, 0, sizeof(uint32_t));

    uint32_t record_count;
    ERRCHECK(read(fd, &record_count, sizeof(record_count)), "Could read record  count from header from %s\n", connection->filepath);

    lseek(fd, -sizeof(record_count), SEEK_CUR);

    db_unlock_region(fd, 0, sizeof(record_count));

    lseek(fd, temp, SEEK_SET);

    return record_count;
}

bool db_is_sealed(db_connection* connection) {
    int fd = connection->fd;
    
    int temp = db_tell(connection);

    lseek(fd, DB_SNAPSTATE_OFFSET, SEEK_SET);

    db_lock_read_region_wait(fd, 0, sizeof(db_snapshot_state));

    db_snapshot_state state;
    ERRCHECK(read(fd, &state, sizeof(state)), "Could read snapshot seal from header from %s\n", connection->filepath);

    lseek(fd, -sizeof(state), SEEK_CUR);

    db_unlock_region(fd, 0, sizeof(state));

    lseek(fd, temp, SEEK_SET);

    return state == SNAPSHOT_SEALED;
}

void db_seal(db_connection* connection) {
    int fd = connection->fd;

    int temp = db_tell(connection);

    lseek(fd, DB_SNAPSTATE_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(db_snapshot_state));

    db_snapshot_state state = SNAPSHOT_SEALED;
    ERRCHECK(write(fd, &state, sizeof(state)), "Could write snapshot seal to header from %s\n", connection->filepath);

    lseek(fd, temp, SEEK_SET);
}

void db_inc_writers(db_connection* connection) {
    int fd = connection->fd;
    uint8_t writers;

    int temp = db_tell(connection);

    lseek(fd, DB_WRITERS_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(writers));
    ERRCHECK(read(fd, &writers, sizeof(writers)), "Could not read writers from header from %s\n", connection->filepath);
    
    writers++;

    lseek(fd, -sizeof(writers), SEEK_CUR);

    ERRCHECK(write(fd, &writers, sizeof(writers)), "Could not write writers to header from %s\n", connection->filepath);

    lseek(fd, -sizeof(writers), SEEK_CUR);

    db_unlock_region(fd, 0, sizeof(writers));

    lseek(fd, temp, SEEK_SET);
}

bool db_dec_writers(db_connection* connection) {
    int fd = connection->fd;
    uint8_t writers;

    int temp = db_tell(connection);

    lseek(fd, DB_WRITERS_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(writers));
    ERRCHECK(read(fd, &writers, sizeof(writers)), "Could not read writers from header from %s\n", connection->filepath);
    lseek(fd, -sizeof(writers), SEEK_CUR);

    if(writers == 0) {
        return false; 
        db_unlock_region(fd, 0, sizeof(writers));
    }
    
    writers--;


    ERRCHECK(write(fd, &writers, sizeof(writers)), "Could not write writers to header from %s\n", connection->filepath);

    if(writers == 0) {
        lseek(fd, -sizeof(writers), SEEK_CUR);
        db_unlock_region(fd, 0, sizeof(writers));

        lseek(fd, temp, SEEK_SET);

        return false;
    }

    lseek(fd, -sizeof(writers), SEEK_CUR);
    db_unlock_region(fd, 0, sizeof(writers));

    lseek(fd, temp, SEEK_SET);
    return true;
}

bool db_open_connection(db_connection* connection, const char* filepath, 
        db_schema* schema, db_generate_snapshot_func generate_snapshot) {
    printf("Opening db connection for %s\n", filepath);

    int exists = 1;
    if(access(filepath, F_OK) == -1) {
       exists = 0; 
    }

    int fd;
    ERRCHECK(fd = open(filepath, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR), "Could not open file %s\n", filepath);

    db_header header = {0};
    if(!exists) {
        printf("%s doesnt exist, creating it and setting up the header...\n", filepath);

        if(db_lock_write_region(fd, 0, sizeof(db_header))) {
            header = (db_header){
                .format_version = FORMAT_VERSION,
                .snapshot_id = generate_snapshot(filepath),
                .snapshot_state = SNAPSHOT_OPEN,
                .active_writers = 1,
                .record_count = 0
            };

            memcpy(header.signature, schema->signature, 32);
            
            ERRCHECK(write(fd, &header, sizeof(db_header)), "Could not write header to %s\n", filepath);
            lseek(fd, -sizeof(header), SEEK_CUR);

            db_unlock_region(fd, 0, sizeof(header));
        }else {
            printf("%s header lock already present. Skipping over...\n", filepath);
        }
    } else {
        if(db_is_sealed(connection)) {
            printf("%s snapshot is already sealed\n", filepath);
            close(fd);
            return false;
        }
        db_inc_writers(connection);
        printf("Connected to %s and incremented writers\n", filepath);
    }

    connection->schema = *schema;
    connection->filepath = strdup(filepath);
    connection->fd = fd;
    connection->generate_snapshot = generate_snapshot;

    for(size_t i = 0; i < schema->count; i++) {
        db_column_schema* col = &schema->columns[i];
        switch(col->kind) {
            case DB_STRING:
                {
                    connection->record_size += DB_STRING_LEN;
                    break;
                }
            case DB_INT:
                {
                    connection->record_size += sizeof(int);
                    break;
                }
        }
    }


    return true;
}

bool db_equal(db_connection* connection, db_column* col1, db_column* col2) {
    char* c1 = (char*)col1;
    char* c2 = (char*)col2;

    for(size_t i = 0; i < connection->record_size; i++) {
        if(c1[i] != c2[i]) return false;
    }

    return true;
}

bool db_insert(db_connection* connection, db_columns values) {
    int fd = connection->fd;
    int temp = db_tell(connection);

    uint32_t record_count = db_record_count(connection);

    ASSERT(values.count == connection->schema.count, "[INSERT] Provided column values count (%zu) does not coincide with the schema column count (%zu)", values.count, connection->schema.count);

    lseek(fd, sizeof(db_header), SEEK_SET);
    for(size_t i = 0; i < record_count; i++) {
        for(size_t j = 0; j < values.count; j++) {

        }
        lseek(fd, connection->record_size, SEEK_CUR);
    }


    lseek(fd, temp, SEEK_SET);

    return true;
}

void db_close_connection(db_connection *connection) {
    if(!db_dec_writers(connection)) {
        db_seal(connection);

        close(connection->fd);
        free(connection->filepath);
    }
}

