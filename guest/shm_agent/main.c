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

#define BALLOOND_SHM_NAME "/balloond_shm_v1"

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(void) {
    int fd;
    struct balloond_shm *shm;
    uint64_t seen_cmd = 0;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    fd = shm_open(BALLOOND_SHM_NAME, O_RDWR, 0666);
    if (fd < 0) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return 1;
    }

    shm = (struct balloond_shm *)mmap(NULL, sizeof(*shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    if (shm->magic != BALLOOND_SHM_MAGIC || shm->version != BALLOOND_SHM_VERSION) {
        fprintf(stderr, "protocol mismatch: magic/version invalid\n");
        munmap(shm, sizeof(*shm));
        close(fd);
        return 1;
    }

    printf("shm_agent: started\n");

    while (!g_stop) {
        if (shm->cmd_seq != seen_cmd) {
            seen_cmd = shm->cmd_seq;

            shm->actual_bytes = shm->target_bytes;
            shm->ack_seq = shm->cmd_seq;
            shm->status = 0;
            shm->last_error = 0;

            printf("shm_agent: cmd=%llu target=%llu ack=%llu actual=%llu\n",
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

    printf("shm_agent: stopping\n");
    munmap(shm, sizeof(*shm));
    close(fd);
    return 0;
}
