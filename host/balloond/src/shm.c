#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/protocol.h"

#define BALLOOND_SHM_NAME "/balloond_shm_v1"

static int g_fd = -1;
static struct balloond_shm *g_shm = NULL;

static int shm_open_and_map(void) {
    g_fd = shm_open(BALLOOND_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_fd < 0) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return -1;
    }

    if (ftruncate(g_fd, (off_t)sizeof(*g_shm)) < 0) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    g_shm = (struct balloond_shm *)mmap(
        NULL, sizeof(*g_shm), PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_shm == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        g_shm = NULL;
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    return 0;
}

static void shm_init_if_needed(void) {
    if (g_shm->magic != BALLOOND_SHM_MAGIC || g_shm->version != BALLOOND_SHM_VERSION) {
        memset(g_shm, 0, sizeof(*g_shm));
        g_shm->magic = BALLOOND_SHM_MAGIC;
        g_shm->version = BALLOOND_SHM_VERSION;
    }
}

int balloond_shm_setup(void) {
    if (shm_open_and_map() < 0) {
        return -1;
    }
    shm_init_if_needed();
    return 0;
}

void balloond_shm_close(void) {
    if (g_shm) {
        munmap(g_shm, sizeof(*g_shm));
        g_shm = NULL;
    }
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

struct balloond_shm *balloond_shm_ptr(void) {
    return g_shm;
}
