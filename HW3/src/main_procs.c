#include "db.h"
#include "lock.h"
#include "procs.h"
#include "util.h"
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

uint32_t proc_generate_snapshot(const char*) {
    return time(NULL);
}

void db_procs_upsert(db_connection* connection, db_procs_row* row) {
    int fd = connection->fd;
    int temp = db_tell(connection);

    uint32_t record_count = db_record_count(connection);

    lseek(fd, sizeof(db_header), SEEK_SET);
    db_procs_row irow;
    size_t i;
    for(i = 0; i < record_count; i++) {
        db_lock_read_region_wait(fd, 0, sizeof(db_procs_row));

        ERRCHECK(read(fd, &irow, sizeof(db_procs_row)), "[Procs] UPSERT: Could not read row %zu for %s\n", i, connection->filepath);

        db_unlock_region(fd, -sizeof(db_procs_row), sizeof(db_procs_row));

        if(row->pid == irow.pid) {

            db_lock_write_region_wait(fd, -sizeof(db_procs_row), sizeof(db_procs_row)); 

            lseek(fd, -sizeof(db_procs_row), SEEK_CUR);

            ERRCHECK(write(fd, row, sizeof(db_procs_row)), "[Procs] UPSERT: Could not overwrite row %zu for %s\n", i, connection->filepath);


            db_unlock_region(fd, -sizeof(db_procs_row), sizeof(db_procs_row));

            lseek(fd, temp, SEEK_SET);
            return;
        }
    }
    
    while(!db_lock_write_region(fd, 0, sizeof(db_procs_row))) {
        lseek(fd, sizeof(db_procs_row), SEEK_CUR);
        i++;
    }

    ERRCHECK(write(fd, row, sizeof(db_procs_row)), "[FileIndexer] UPSERT: Could not insert row %zu for %s\n", i, connection->filepath);

    db_unlock_region(fd, -sizeof(db_procs_row), sizeof(db_procs_row));

    db_inc_record_count(connection);

    lseek(fd, temp, SEEK_SET);
}

void traverse_procs(db_connection* connection) {
    DIR* dir = opendir("/proc"); 
    ERRCHECKNULL(dir, "[Procs]: Could not open /proc\n");

    struct dirent* proc;

    while((proc = readdir(dir))) {
        if(proc->d_name[0] < '0' || proc->d_name[0] > '9') {
            continue;
        }

        db_procs_row row = {0};

        pid_t pid = atoi(proc->d_name);

        int fd;
        char buf[PATH_MAX] = {0};
        char p[PATH_MAX] = {0};

        snprintf(p, PATH_MAX, "/proc/%s/stat", proc->d_name);
        
        if((fd = open(p,O_RDONLY)) == -1) {
            continue;
        }

        read(fd, buf, PATH_MAX);
        buf[PATH_MAX-1] = '\0';

        char* stat_fields[53];


        char* op=strchr(buf,'(');
        char* cl=strrchr(buf,')');

        if(op && cl){
            strncpy(row.comm, op+1, sizeof(row.comm)-1);
            row.comm[sizeof(row.comm)-1]='\0';

            char *p=strtok(cl+2," ");
            int i=3;

            while(p!=NULL && i<53){
                stat_fields[i]=p;
                i++;
                p=strtok(NULL, " ");
            }

            row.pid=pid;
            row.ppid=atoi(stat_fields[4]);
            row.state[0]=stat_fields[3][0];
            row.state[1]='\0';
            row.rss=strtoull(stat_fields[24],NULL,10);
            strcpy(row.rss_source,"/stat");
            row.cpu_time=((strtoul(stat_fields[14],NULL,10)+strtoul(stat_fields[15],NULL,10))/sysconf(_SC_CLK_TCK));
        }
            
       
        close(fd);

        memset(p, 0, PATH_MAX);
        snprintf(p, PATH_MAX, "/proc/%s/cmdline", proc->d_name);
        fd = open(p, O_RDONLY);

        int n = read(fd, buf, sizeof(row.cmdline)-1);
        

        if(n>0){
            for(int i = 0; i < n; i++){
                if(buf[i]=='\0'){
                    row.cmdline[i]=' ';
                }else{
                    row.cmdline[i]=buf[i];
                }
            }
            row.cmdline[n] = '\0';
        }else{
            row.cmdline[n] = '\0';
        }

        close(fd);

        db_procs_upsert(connection, &row);
    }
}

int main(int argc, char** argv) {
    char* db = "./data/procs.db";

    while(argv[1] != NULL) {
        if(strcmp(argv[1], "--db") == 0) {
            argv++;
            if(argv[1] != NULL) {
                db = argv[1];
            }
        }
        argv++;
    }

    db_connection connection = {0};
    if(!db_open_connection(&connection, db, DB_PROCS_SIGNATURE, &proc_generate_snapshot)) {
        printf("[Procs]: %s is already sealed\n", db);

        return 2;
    }

    // printf("size of header: %zu\n", sizeof(db_header));
    // printf("size of row: %zu\n", sizeof(db_indexer_row));
    // printf("size of bname: %zu\n", sizeof(db_indexer_bname));
    
    traverse_procs(&connection);

    // printf("Traversed everything\n");

    db_close_connection(&connection);

    return 0;
}
