#include "db.h"

#include "util.h"
#include "lock.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


constexpr size_t FORMAT_VERSION = 1;

constexpr size_t DB_WRITERS_OFFSET = DB_SIGNATURE_LEN+sizeof(uint8_t)+sizeof(db_snapshot_state)+sizeof(uint32_t);

void db_inc_writers(db_connection* connection) {
    int fd = connection->fd;
    uint8_t writers;

    lseek(fd, DB_WRITERS_OFFSET, SEEK_SET);

    db_lock_write_region_wait(fd, 0, sizeof(writers));
    ERRCHECK(read(fd, &writers, sizeof(writers)), "Could not read writers from header from %s\n", connection->filepath);
    
    writers++;

    lseek(fd, -sizeof(writers), SEEK_CUR);

    ERRCHECK(write(fd, &writers, sizeof(writers)), "Could not write writers to header from %s\n", connection->filepath);
}

bool db_dec_writers(db_connection* connection) {
    int fd = connection->fd;
    uint8_t writers;

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

        return false;
    }

    lseek(fd, -sizeof(writers), SEEK_CUR);
    db_unlock_region(fd, 0, sizeof(writers));

    return true;
}

void db_open_connection(db_connection* connection, const char* filepath, 
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
    }

    connection->schema = *schema;
    connection->filepath = strdup(filepath);
    connection->fd = fd;
    connection->generate_snapshot = generate_snapshot;
}

int db_select_header(db_connection* connection, db_header* header) {
    int fd = connection->fd;

    struct flock lock = {
        .l_start = 0,
        .l_len = sizeof(db_header),
        .l_whence = SEEK_SET,
        .l_type = F_RDLCK
    };

    if(fcntl(fd, F_SETLKW, &lock) != -1) {
        lseek(fd, 0, SEEK_SET);

        ERRCHECK(read(fd, header, sizeof(db_header)), "Could not read the header");

        return true;
    }

    perror("Failed to acquire lock for reading header");
    return false;
}
void db_varchar_free(db_varchar* varchar) {
    free(varchar->str);
}

void _db_parse_varchar(db_connection* connection, db_varchar* varchar) {
    int fd = connection->fd;
    uint16_t size;
    ERRCHECK(read(fd, &size, sizeof(uint16_t)), "Could not read varchar length");
    char* str = malloc(sizeof(char)*size);
    ERRCHECK(read(fd, str, sizeof(char)*size), "Could not read varchar");

    varchar->size = size;
    varchar->str = str;
}
