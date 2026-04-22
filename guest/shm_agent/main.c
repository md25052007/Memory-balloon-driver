#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "include/protocol.h"

#define PCI_DEVICES_DIR "/sys/bus/pci/devices"
#define IVSHMEM_VENDOR_ID "0x1af4"
#define IVSHMEM_DEVICE_ID "0x1110"
#define IVSHMEM_MAP_SIZE 4096

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static int read_small_file(const char *path, char *out, size_t out_sz) {
    FILE *f;
    size_t n;

    if (!path || !out || out_sz == 0) {
        return -1;
    }

    f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    n = fread(out, 1, out_sz - 1, f);
    fclose(f);
    out[n] = '\0';

    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ' || out[n - 1] == '\t')) {
        out[n - 1] = '\0';
        --n;
    }

    return 0;
}

static int find_ivshmem_resource(char *out_path, size_t out_sz) {
    DIR *dir;
    struct dirent *de;
    int found = -1;

    dir = opendir(PCI_DEVICES_DIR);
    if (!dir) {
        return -1;
    }

    while ((de = readdir(dir)) != NULL) {
        char vendor_path[PATH_MAX];
        char device_path[PATH_MAX];
        char vendor[32];
        char device[32];

        if (de->d_name[0] == '.') {
            continue;
        }

        if (snprintf(vendor_path, sizeof(vendor_path), "%s/%s/vendor", PCI_DEVICES_DIR, de->d_name) >= (int)sizeof(vendor_path)) {
            continue;
        }
        if (snprintf(device_path, sizeof(device_path), "%s/%s/device", PCI_DEVICES_DIR, de->d_name) >= (int)sizeof(device_path)) {
            continue;
        }

        if (read_small_file(vendor_path, vendor, sizeof(vendor)) != 0) {
            continue;
        }
        if (read_small_file(device_path, device, sizeof(device)) != 0) {
            continue;
        }

        if (strcmp(vendor, IVSHMEM_VENDOR_ID) != 0 || strcmp(device, IVSHMEM_DEVICE_ID) != 0) {
            continue;
        }

        if (snprintf(out_path, out_sz, "%s/%s/resource2", PCI_DEVICES_DIR, de->d_name) >= (int)out_sz) {
            break;
        }

        found = 0;
        break;
    }

    closedir(dir);
    return found;
}

int main(void) {
    int fd;
    void *map;
    struct balloond_shm *shm;
    uint64_t seen_cmd = 0;
    char ivshmem_resource[PATH_MAX];

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    if (find_ivshmem_resource(ivshmem_resource, sizeof(ivshmem_resource)) != 0) {
        fprintf(stderr, "ivshmem device not found under %s (vendor=%s device=%s)\n",
                PCI_DEVICES_DIR, IVSHMEM_VENDOR_ID, IVSHMEM_DEVICE_ID);
        return 1;
    }

    fd = open(ivshmem_resource, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", ivshmem_resource, strerror(errno));
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

    printf("shm_agent(ivshmem): started using %s\n", ivshmem_resource);

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
