#include "common/model.h"

typedef struct {
        uint64_t last_image_id; // what we last emitted
        int last_row, last_col;
        int render_chroma;
} TerminalBackendState;

void terminal_backend_commit(const DrawBuffer *buf,
                             DirtyFlags dirty,
                             TerminalBackendState *state);
