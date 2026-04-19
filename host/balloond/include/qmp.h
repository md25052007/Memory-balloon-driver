#ifndef BALLOOND_QMP_H
#define BALLOOND_QMP_H

#include <stdint.h>

int qmp_set_target_bytes(const char *sock_path, uint64_t target_bytes);
int qmp_query_actual_bytes(const char *sock_path, uint64_t *actual_bytes);

#endif
