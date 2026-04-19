#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/protocol.h"

#define BALLOOND_SHM_FILE_DEFAULT "/dev/shm/balloon_ivshmem.bin"
#define BALLOOND_SHM_REGION_SIZE  (1024 * 1024)

static int g_fd = -1;
static void *g_map = NULL;
static struct balloond_shm *g_shm = NULL;

static const char *balloond_shm_path(void) {
    const char *p = getenv("BALLOOND_SHM_FILE");
    if (p && p[0] != '\0') {
        return p;
    }
    return BALLOOND_SHM_FILE_DEFAULT;
}

static int shm_open_and_map(void) {
    const char *path = balloond_shm_path();

    g_fd = open(path, O_CREAT | O_RDWR, 0666);
    if (g_fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (ftruncate(g_fd, (off_t)BALLOOND_SHM_REGION_SIZE) < 0) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    g_map = mmap(NULL, BALLOOND_SHM_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_map == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        g_map = NULL;
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    g_shm = (struct balloond_shm *)g_map;
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
    if (g_map) {
        munmap(g_map, BALLOOND_SHM_REGION_SIZE);
        g_map = NULL;
    }
    g_shm = NULL;

    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

struct balloond_shm *balloond_shm_ptr(void) {
    return g_shm;
}
