#ifndef EF_MODE_H
#define EF_MODE_H

typedef enum {
    MODE_NORMAL = 0,
    MODE_INSERT,
    MODE_VISUAL
} Mode;

const char *mode_string(Mode m);

#endif /* EF_MODE_H */
