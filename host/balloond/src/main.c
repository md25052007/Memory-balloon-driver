#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/log.h"
#include "../include/protocol.h"
#include "../include/qmp.h"

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void publish_target(struct balloond_shm *shm, const char *qmp_sock, uint64_t target) {
    shm->target_bytes = target;
    shm->cmd_seq++;

    if (qmp_set_target_bytes(qmp_sock, target) == 0) {
        balloond_log_info("new target_bytes=%llu cmd_seq=%llu",
                          (unsigned long long)shm->target_bytes,
                          (unsigned long long)shm->cmd_seq);
    } else {
        shm->status = 1;
        shm->last_error = (uint32_t)errno;
        balloond_log_error("qmp_set_target_bytes failed (errno=%d)", errno);
    }
}

int main(int argc, char **argv) {
    uint64_t target = 2147483648ULL; /* 2 GiB default */
    const char *qmp_sock = "/home/maithreya/virtio-balloon/logs/qmp.sock";
    struct balloond_shm *shm = NULL;
    int interactive = isatty(STDIN_FILENO);

    if (argc > 1) {
        target = strtoull(argv[1], NULL, 10);
    }
    if (argc > 2) {
        qmp_sock = argv[2];
    }

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    if (balloond_shm_setup() < 0) {
        balloond_log_error("shm setup failed");
        return 1;
    }

    shm = balloond_shm_ptr();
    if (!shm) {
        balloond_log_error("shm ptr is null");
        balloond_shm_close();
        return 1;
    }

    publish_target(shm, qmp_sock, target);

    while (!g_stop) {
        uint64_t actual = 0;

        if (qmp_query_actual_bytes(qmp_sock, &actual) == 0) {
            shm->actual_bytes = actual;
            if (shm->actual_bytes == shm->target_bytes) {
                shm->ack_seq = shm->cmd_seq;
            }
            shm->status = 0;
            shm->last_error = 0;
        } else {
            shm->status = 1;
            shm->last_error = (uint32_t)errno;
        }

        balloond_log_info("actual=%llu target=%llu cmd_seq=%llu ack_seq=%llu status=%u err=%u",
                          (unsigned long long)shm->actual_bytes,
                          (unsigned long long)shm->target_bytes,
                          (unsigned long long)shm->cmd_seq,
                          (unsigned long long)shm->ack_seq,
                          shm->status,
                          shm->last_error);

        if (interactive) {
            unsigned long long in_target = ULLONG_MAX;
            int rc;

            printf("enter new target bytes (or -1 to skip): ");
            fflush(stdout);

            rc = scanf("%llu", &in_target);
            if (rc == 1 && in_target != ULLONG_MAX) {
                publish_target(shm, qmp_sock, (uint64_t)in_target);
            } else if (rc != 1) {
                clearerr(stdin);
            }
        }

        sleep(1);
    }

    balloond_log_info("stopping");
    balloond_shm_close();
    return 0;
}
