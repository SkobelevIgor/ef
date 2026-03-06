#ifndef EF_INSERT_H
#define EF_INSERT_H

#include <stdbool.h>
#include "screen.h"

typedef struct Editor Editor;

bool handle_insert_mode(Editor *ed, EditorEvent *ev);

#endif /* EF_INSERT_H */
