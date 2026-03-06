#ifndef EF_NORMAL_H
#define EF_NORMAL_H

#include <stdbool.h>
#include "screen.h"

typedef struct Editor Editor;

bool handle_normal_mode(Editor *ed, EditorEvent *ev);

#endif /* EF_NORMAL_H */
