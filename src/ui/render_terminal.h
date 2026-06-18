#include "common/model.h"

typedef struct {
        uint64_t last_image_id; // what we last emitted
        int last_row, last_col;
        int render_chroma;
} TerminalBackendState;

/**
 * @brief Renders what's in the terminal to the buffer.
 *
 * @param bug
 * @param dirty The dirty flags that are set for this render
 * @param state Rendering-related information.
 */
void terminal_backend_commit(const DrawBuffer *buf,
                             DirtyFlags dirty,
                             TerminalBackendState *state);
