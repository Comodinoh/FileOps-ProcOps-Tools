#include "fileops_ipc.h"

#include "util.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

void manager_init(ipc_conn *conn, const char* ipc_path, usz N) {
    if(access(ipc_path, F_OK) != -1) {
        remove(ipc_path);
    }

    int fd;
    ERRCHECK(fd = open(ipc_path, O_CREAT | O_RDWR), "[Fileops Manager] Could not open ipc file %s\n", ipc_path);



}
