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

static const char *resolve_default_qmp_sock(char *buf, size_t buf_sz) {
    const char *env_sock = getenv("BALLOOND_QMP_SOCK");
    const char *home = getenv("HOME");
    int n;

    if (env_sock && env_sock[0] != '\0') {
        return env_sock;
    }

    if (home && home[0] != '\0') {
        n = snprintf(buf, buf_sz, "%s/virtio-balloon/logs/qmp.sock", home);
        if (n > 0 && (size_t)n < buf_sz) {
            return buf;
        }
    }

    return "./logs/qmp.sock";
}

static void publish_target(struct balloond_shm *shm, const char *qmp_sock, uint64_t target) {
    if (shm->ack_seq > shm->cmd_seq) {
        balloond_log_error("protocol violation: ack_seq(%llu) > cmd_seq(%llu)",
                           (unsigned long long)shm->ack_seq,
                           (unsigned long long)shm->cmd_seq);
        return;
    }

    if (target == shm->target_bytes && shm->cmd_seq == shm->ack_seq) {
        balloond_log_info("target unchanged and no pending cmd (target=%llu), skipping publish",
                          (unsigned long long)target);
        return;
    }

    shm->target_bytes = target;
    shm->cmd_seq++;

    if (qmp_set_target_bytes(qmp_sock, target) == 0) {
        balloond_log_info("new target_bytes=%llu cmd_seq=%llu",
                          (unsigned long long)shm->target_bytes,
                          (unsigned long long)shm->cmd_seq);
    } else {
        balloond_log_error("qmp_set_target_bytes failed (errno=%d)", errno);
    }
}

int main(int argc, char **argv) {
    uint64_t target = 2147483648ULL; /* 2 GiB default */
    char qmp_default[PATH_MAX];
    const char *qmp_sock = resolve_default_qmp_sock(qmp_default, sizeof(qmp_default));
    struct balloond_shm *shm = NULL;
    int interactive = isatty(STDIN_FILENO);
    uint64_t observed_actual = 0;
    int qmp_err = 0;

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
            observed_actual = actual;
            qmp_err = 0;
        } else {
            qmp_err = errno;
        }

        balloond_log_info("actual=%llu target=%llu cmd_seq=%llu ack_seq=%llu shm_status=%u shm_err=%u qmp_err=%d",
                          (unsigned long long)observed_actual,
                          (unsigned long long)shm->target_bytes,
                          (unsigned long long)shm->cmd_seq,
                          (unsigned long long)shm->ack_seq,
                          shm->status,
                          shm->last_error,
                          qmp_err);

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
