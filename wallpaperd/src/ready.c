#include "ready.h"
#include <stdio.h>
#include <unistd.h>

void wd_set_ready() {
    pid_t pid = getpid();
    char path[128];
    int _ = snprintf(path, sizeof(path), "/tmp/wallpaperd-%d.ready", pid);

    FILE* file = fopen(path, "w");
    if(file == NULL || fclose(file) != 0) {
        printf("warning: failed to set ready\n");
    }
}

void wd_unset_ready() {
    pid_t pid = getpid();
    char path[128];
    int _ = snprintf(path, sizeof(path), "/tmp/wallpaperd-%d.ready", pid);
    remove(path);
}
