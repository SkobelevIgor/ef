#include "file_watcher.h"
#include "xalloc.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __APPLE__
#define EF_ST_MTIM st_mtimespec
#else
#define EF_ST_MTIM st_mtim
#endif

static void fw_record(WatchedFile *wf, const struct stat *st) {
    wf->last_mod_time = st->st_mtime;
    wf->last_mod_nsec = st->EF_ST_MTIM.tv_nsec;
    wf->last_size = st->st_size;
}

static bool fw_changed(const WatchedFile *wf, const struct stat *st) {
    return st->st_mtime != wf->last_mod_time
        || st->EF_ST_MTIM.tv_nsec != wf->last_mod_nsec
        || st->st_size != wf->last_size;
}

/* --- Pure functions (testable) ------------------------------------------- */

int fw_find_file(FileWatcher *fw, const char *filename) {
    if (!fw || !filename) return -1;
    for (int i = 0; i < fw->file_count; i++) {
        if (strcmp(fw->files[i].filename, filename) == 0)
            return i;
    }
    return -1;
}

void fw_watch(FileWatcher *fw, const char *filename) {
    if (!fw || !filename || filename[0] == '\0') return;
    if (fw->file_count >= MAX_WATCHED_FILES) return;
    if (fw_find_file(fw, filename) >= 0) return;

    WatchedFile *wf = &fw->files[fw->file_count];
    wf->filename = xstrdup(filename);

    struct stat st;
    if (stat(filename, &st) == 0) {
        fw_record(wf, &st);
    } else {
        wf->last_mod_time = 0;
        wf->last_mod_nsec = 0;
        wf->last_size = 0;
    }
    fw->file_count++;
}

const char *fw_check(FileWatcher *fw) {
    if (!fw) return NULL;
    for (int i = 0; i < fw->file_count; i++) {
        struct stat st;
        if (stat(fw->files[i].filename, &st) != 0)
            continue;
        if (fw_changed(&fw->files[i], &st))
            return fw->files[i].filename;
    }
    return NULL;
}

void fw_update_mod_time(FileWatcher *fw, const char *filename) {
    if (!fw || !filename) return;
    int idx = fw_find_file(fw, filename);
    if (idx < 0) return;

    struct stat st;
    if (stat(filename, &st) == 0) {
        fw_record(&fw->files[idx], &st);
    }
}

/* --- VTable wrappers ----------------------------------------------------- */

static void vt_watch(void *self, const char *filename) {
    fw_watch((FileWatcher *)self, filename);
}

static const char *vt_check(void *self) {
    return fw_check((FileWatcher *)self);
}

static void vt_update(void *self, const char *filename) {
    fw_update_mod_time((FileWatcher *)self, filename);
}

static void vt_close(void *self) {
    (void)self;
}

FileWatcherVTable *file_watcher_new(void) {
    FileWatcher *fw = xcalloc(1, sizeof(FileWatcher));
    FileWatcherVTable *vt = xcalloc(1, sizeof(FileWatcherVTable));
    vt->watch = vt_watch;
    vt->check = vt_check;
    vt->update_mod_time = vt_update;
    vt->close = vt_close;
    vt->impl = fw;
    return vt;
}

void file_watcher_free(FileWatcherVTable *vt) {
    if (!vt) return;
    FileWatcher *fw = (FileWatcher *)vt->impl;
    for (int i = 0; i < fw->file_count; i++)
        free(fw->files[i].filename);
    free(fw);
    free(vt);
}
