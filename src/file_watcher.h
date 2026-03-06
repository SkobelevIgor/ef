#ifndef EF_FILE_WATCHER_H
#define EF_FILE_WATCHER_H

#include <stdbool.h>
#include <time.h>

#define MAX_WATCHED_FILES 64

typedef struct {
    char  *filename;
    time_t last_mod_time;
} WatchedFile;

/* FileWatcherVTable abstracts file watching (dependency inversion). */
typedef struct {
    void (*watch)(void *self, const char *filename);
    const char *(*check)(void *self);
    void (*update_mod_time)(void *self, const char *filename);
    void (*close)(void *self);
    void *impl;
} FileWatcherVTable;

/* Concrete file watcher using stat(). */
typedef struct {
    WatchedFile files[MAX_WATCHED_FILES];
    int         file_count;
} FileWatcher;

FileWatcherVTable *file_watcher_new(void);
void               file_watcher_free(FileWatcherVTable *fw);

/* Pure functions for testability. */
void        fw_watch(FileWatcher *fw, const char *filename);
const char *fw_check(FileWatcher *fw);
void        fw_update_mod_time(FileWatcher *fw, const char *filename);
int         fw_find_file(FileWatcher *fw, const char *filename);

#endif /* EF_FILE_WATCHER_H */
