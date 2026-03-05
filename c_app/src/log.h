#ifndef EF_LOG_H
#define EF_LOG_H

/* Simple file logger for debugging (ncurses captures stdout/stderr). */

void log_open(const char *path);  /* Opens log file. NULL = "edit.log" */
void log_close(void);
void log_write(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* EF_LOG_H */
