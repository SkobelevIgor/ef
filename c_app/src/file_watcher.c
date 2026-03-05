#include "file_watcher.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    wf->filename = strdup(filename);

    struct stat st;
    if (stat(filename, &st) == 0) {
        wf->last_mod_time = st.st_mtime;
    } else {
        wf->last_mod_time = 0;
    }
    fw->file_count++;
}

const char *fw_check(FileWatcher *fw) {
    if (!fw) return NULL;
    for (int i = 0; i < fw->file_count; i++) {
        struct stat st;
        if (stat(fw->files[i].filename, &st) != 0)
            continue;
        if (st.st_mtime > fw->files[i].last_mod_time) {
            fw->files[i].last_mod_time = st.st_mtime;
            return fw->files[i].filename;
        }
    }
    return NULL;
}

void fw_update_mod_time(FileWatcher *fw, const char *filename) {
    if (!fw || !filename) return;
    int idx = fw_find_file(fw, filename);
    if (idx < 0) return;

    struct stat st;
    if (stat(filename, &st) == 0) {
        fw->files[idx].last_mod_time = st.st_mtime;
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
    FileWatcher *fw = calloc(1, sizeof(FileWatcher));
    if (!fw) return NULL;

    FileWatcherVTable *vt = calloc(1, sizeof(FileWatcherVTable));
    if (!vt) { free(fw); return NULL; }

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
