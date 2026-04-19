#ifndef BALLOOND_PROTOCOL_H
#define BALLOOND_PROTOCOL_H

#include <stdint.h>

#define BALLOOND_SHM_MAGIC 0x42414C4Eu
#define BALLOOND_SHM_VERSION 1u

#pragma pack(push, 1)
struct balloond_shm {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint64_t target_bytes;
    uint64_t actual_bytes;
    uint64_t cmd_seq;
    uint64_t ack_seq;
    uint32_t status;
    uint32_t last_error;
};
#pragma pack(pop)

/* shm.c API */
int balloond_shm_setup(void);
void balloond_shm_close(void);
struct balloond_shm *balloond_shm_ptr(void);

#endif
