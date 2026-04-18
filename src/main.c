#include "config.h"
#include "xalloc.h"
#include "buffer.h"
#include "editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static void parse_file_arg(const char *arg, char **filename, int *line) {
    *line = 0;
    const char *colon = strrchr(arg, ':');
    if (!colon || colon == arg) {
        *filename = xstrdup(arg);
        return;
    }
    /* Check if everything after colon is a number */
    const char *p = colon + 1;
    while (*p) {
        if (*p < '0' || *p > '9') {
            *filename = xstrdup(arg);
            return;
        }
        p++;
    }
    int ln = atoi(colon + 1);
    if (ln < 1) {
        *filename = xstrdup(arg);
        return;
    }
    *line = ln;
    size_t name_len = (size_t)(colon - arg);
    *filename = xmalloc(name_len + 1);
    memcpy(*filename, arg, name_len);
    (*filename)[name_len] = '\0';
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    char **args = argv + 1;
    int nargs = argc - 1;

    SplitMode split = SPLIT_VERTICAL;
    bool read_only = false;

    /* Consume flags: -h and -r (order-independent) */
    while (nargs > 0 && args[0][0] == '-') {
        if (strcmp(args[0], "-h") == 0) {
            split = SPLIT_HORIZONTAL;
        } else if (strcmp(args[0], "-r") == 0) {
            read_only = true;
        } else {
            break;
        }
        args++;
        nargs--;
    }

    if (nargs < 1) {
        fprintf(stderr, "Usage: ef [-h] [-r] <filename[:line]> [filename2[:line]] ...\n");
        return 1;
    }

    FileInfo *files = xmalloc(sizeof(FileInfo) * nargs);
    for (int i = 0; i < nargs; i++) {
        parse_file_arg(args[i], &files[i].filename, &files[i].line);
    }

    if (!read_only) {
        for (int i = 0; i < nargs; i++) {
            if (buffer_check_writable(files[i].filename) != 0) {
                fprintf(stderr, "ef: '%s': permission denied. Use ef -r %s for read-only mode.\n",
                        files[i].filename, files[i].filename);
                for (int j = 0; j < nargs; j++) free(files[j].filename);
                free(files);
                return 1;
            }
        }
    }

    config_ensure_default(NULL);

    Editor *ed = editor_new(files, nargs, split);
    if (!ed) {
        fprintf(stderr, "Error: failed to initialize editor\n");
        for (int i = 0; i < nargs; i++) free(files[i].filename);
        free(files);
        return 1;
    }
    ed->read_only = read_only;

    int ret = editor_run(ed);

    editor_free(ed);
    for (int i = 0; i < nargs; i++) free(files[i].filename);
    free(files);
    return ret;
}
