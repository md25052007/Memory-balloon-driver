#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "include/protocol.h"

#define IVSHMEM_RESOURCE "/sys/bus/pci/devices/0000:01:00.0/resource2"
#define IVSHMEM_MAP_SIZE 4096

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(void) {
    int fd;
    void *map;
    struct balloond_shm *shm;
    uint64_t seen_cmd = 0;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    fd = open(IVSHMEM_RESOURCE, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", IVSHMEM_RESOURCE, strerror(errno));
        return 1;
    }

    map = mmap(NULL, IVSHMEM_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    shm = (struct balloond_shm *)map;

    if (shm->magic != BALLOOND_SHM_MAGIC || shm->version != BALLOOND_SHM_VERSION) {
        fprintf(stderr, "protocol mismatch: magic/version invalid (magic=0x%x version=%u)\n",
                shm->magic, shm->version);
        munmap(map, IVSHMEM_MAP_SIZE);
        close(fd);
        return 1;
    }

    printf("shm_agent(ivshmem): started\n");

    while (!g_stop) {
        if (shm->cmd_seq != seen_cmd) {
            seen_cmd = shm->cmd_seq;

            shm->actual_bytes = shm->target_bytes;
            shm->ack_seq = shm->cmd_seq;
            shm->status = 0;
            shm->last_error = 0;

            printf("shm_agent(ivshmem): cmd=%llu target=%llu ack=%llu actual=%llu\n",
                   (unsigned long long)shm->cmd_seq,
                   (unsigned long long)shm->target_bytes,
                   (unsigned long long)shm->ack_seq,
                   (unsigned long long)shm->actual_bytes);
        }

        {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 200000000L;
            nanosleep(&ts, NULL);
        }
    }

    printf("shm_agent(ivshmem): stopping\n");
    munmap(map, IVSHMEM_MAP_SIZE);
    close(fd);
    return 0;
}
