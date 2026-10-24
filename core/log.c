#include "log.h"
#include <stdarg.h>
#include <stdio.h>

static FILE *file;

void fern_log_open(const char *dir) {
    char path[1024];
    if (file) fclose(file);
    snprintf(path, sizeof(path), "%s/log.txt", dir);
    file = fopen(path, "w");
    if (file) setvbuf(file, NULL, _IOLBF, 0);
}

void fern_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    if (file) {
        va_start(ap, fmt);
        vfprintf(file, fmt, ap);
        va_end(ap);
        fputc('\n', file);
    }
}
