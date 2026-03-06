#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_file = NULL;

void log_open(const char *path) {
    if (log_file) return;
    log_file = fopen(path ? path : "edit.log", "a");
}

void log_close(void) {
    if (!log_file) return;
    fclose(log_file);
    log_file = NULL;
}

void log_write(const char *fmt, ...) {
    if (!log_file) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "%02d:%02d:%02d ", t->tm_hour, t->tm_min, t->tm_sec);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);

    fputc('\n', log_file);
    fflush(log_file);
}
