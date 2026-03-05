#include "mode.h"

const char *mode_string(Mode m) {
    switch (m) {
    case MODE_NORMAL: return "NORMAL";
    case MODE_INSERT: return "INSERT";
    case MODE_VISUAL: return "VISUAL";
    default:          return "UNKNOWN";
    }
}
