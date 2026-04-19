#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "../include/log.h"

static void log_with_level(const char *level, const char *fmt, va_list ap) {
    struct timespec ts;
    struct tm tmv;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmv);

    fprintf(stderr, "%04d-%02d-%02d %02d:%02d:%02d [%s] ",
            tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
            tmv.tm_hour, tmv.tm_min, tmv.tm_sec, level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void balloond_log_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_with_level("INFO", fmt, ap);
    va_end(ap);
}

void balloond_log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_with_level("ERROR", fmt, ap);
    va_end(ap);
}
