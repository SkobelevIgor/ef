#ifndef EF_VISUAL_H
#define EF_VISUAL_H

#include <stdbool.h>
#include "screen.h"

typedef struct Editor Editor;

bool handle_visual_mode(Editor *ed, EditorEvent *ev);

#endif /* EF_VISUAL_H */
