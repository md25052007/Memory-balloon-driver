#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../include/qmp.h"

static int qmp_connect(const char *sock_path) {
    int fd;
    struct sockaddr_un addr;

    if (!sock_path || !sock_path[0]) {
        errno = EINVAL;
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    return fd;
}

static int read_line(int fd, char *buf, size_t buflen) {
    size_t i = 0;

    if (!buf || buflen < 2) {
        errno = EINVAL;
        return -1;
    }

    while (i + 1 < buflen) {
        char c;
        ssize_t n = read(fd, &c, 1);

        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        buf[i++] = c;
        if (c == '\n') {
            break;
        }
    }

    buf[i] = '\0';
    return (int)i;
}

static int write_all(int fd, const char *s, size_t len) {
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, s + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)n;
    }

    return 0;
}

static int send_json_line(int fd, const char *json) {
    size_t len = strlen(json);
    if (write_all(fd, json, len) < 0) {
        return -1;
    }
    if (write_all(fd, "\n", 1) < 0) {
        return -1;
    }
    return 0;
}

static int wait_return_line(int fd, char *line, size_t line_sz) {
    for (;;) {
        int n = read_line(fd, line, line_sz);
        if (n <= 0) {
            errno = EIO;
            return -1;
        }

        if (strstr(line, "\"error\"")) {
            errno = EIO;
            return -1;
        }

        if (strstr(line, "\"return\"")) {
            return 0;
        }
    }
}

static int qmp_handshake(int fd) {
    char line[2048];

    /* Greeting */
    if (read_line(fd, line, sizeof(line)) <= 0) {
        errno = EIO;
        return -1;
    }

    if (send_json_line(fd, "{\"execute\":\"qmp_capabilities\"}") < 0) {
        return -1;
    }

    if (wait_return_line(fd, line, sizeof(line)) < 0) {
        return -1;
    }

    return 0;
}

int qmp_set_target_bytes(const char *sock_path, uint64_t target_bytes) {
    int fd;
    char cmd[256];
    char line[2048];

    fd = qmp_connect(sock_path);
    if (fd < 0) {
        return -1;
    }

    if (qmp_handshake(fd) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    snprintf(cmd, sizeof(cmd),
             "{\"execute\":\"balloon\",\"arguments\":{\"value\":%" PRIu64 "}}",
             target_bytes);

    if (send_json_line(fd, cmd) < 0 || wait_return_line(fd, line, sizeof(line)) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    close(fd);
    return 0;
}

int qmp_query_actual_bytes(const char *sock_path, uint64_t *actual_bytes) {
    int fd;
    char line[2048];
    char *p, *end;
    unsigned long long v;

    if (!actual_bytes) {
        errno = EINVAL;
        return -1;
    }

    fd = qmp_connect(sock_path);
    if (fd < 0) {
        return -1;
    }

    if (qmp_handshake(fd) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    if (send_json_line(fd, "{\"execute\":\"query-balloon\"}") < 0 ||
        wait_return_line(fd, line, sizeof(line)) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    p = strstr(line, "\"actual\"");
    if (!p) {
        close(fd);
        errno = EPROTO;
        return -1;
    }

    p = strchr(p, ':');
    if (!p) {
        close(fd);
        errno = EPROTO;
        return -1;
    }

    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    errno = 0;
    v = strtoull(p, &end, 10);
    if (errno != 0 || end == p) {
        close(fd);
        errno = EPROTO;
        return -1;
    }

    *actual_bytes = (uint64_t)v;
    close(fd);
    return 0;
}
