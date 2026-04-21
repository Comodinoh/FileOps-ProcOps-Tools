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

constexpr size_t FORMAT_VERSION = 1;

constexpr size_t DB_WRITERS_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(db_snapshot_state)+sizeof(uint32_t);
constexpr size_t DB_SNAPSTATE_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(uint32_t);

int db_tell(db_connection* connection) {
    // TODO: possible memory leak because we arent closing the FILE stream
    FILE* f;
    ERRCHECKNULL(f = fdopen(connection->fd, "rb"), "Could not associate a stream with the open fd for %s\n", connection->filepath);

    return ftell(f);
}

void db_try_write(db_connection* connection, size_t offset, void* buf, size_t size)  {
    
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

    return true;
}

void db_insert(db_connection *connection, db_column *columns, db_column_value *values, size_t count) {
    
}

void db_close_connection(db_connection *connection) {
    if(!db_dec_writers(connection)) {
        db_seal(connection);

        close(connection->fd);
        free(connection->filepath);
    }
}

