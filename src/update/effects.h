#include "common/model.h"

typedef enum {
        CMD_NONE,
        CMD_TICK,
        CMD_RENDERED,
        CMD_REBUILD_LAYOUT,
        CMD_CHANGE_VIEW,
        CMD_LOAD_WAITING_MUSIC,
        CMD_VIEW_ENQUEUE,
        CMD_PLAY,
        CMD_PAUSE,
        CMD_TOGGLE_PAUSE,
        CMD_TOGGLE_VISUALIZER,
        CMD_TOGGLE_REPEAT,
        CMD_TOGGLE_ASCII,
        CMD_TOGGLE_NOTIFICATIONS,
        CMD_TOGGLE_SHUFFLE,

        CMD_TOGGLE_LYRICS_PAGE,
        CMD_CYCLE_COLOR_MODE,
        CMD_CYCLE_VISUALIZER_MODE,
        CMD_CYCLE_THEMES,
        CMD_CYCLE_VISUALIZATION,

        CMD_QUIT,

        CMD_SCROLL_NEXT,
        CMD_SCROLL_PREV,

        CMD_VOLUME_CHANGE,

        CMD_NEXT,
        CMD_PREV,
        CMD_SEEK_BACK,
        CMD_SEEK_FORWARD,

        CMD_SEARCH,

        CMD_ADD_TO_FAVORITES,
        CMD_SAVEPLAYLIST,
        CMD_UPDATE_LIBRARY,

        CMD_TOGGLE_VIEW,

        CMD_NEXT_PAGE,
        CMD_PREV_PAGE,

        CMD_REMOVE,
        CMD_SHOW_TRACK,

        CMD_NEXT_VIEW,
        CMD_PREV_VIEW,

        CMD_CLEAR_PLAYLIST,

        CMD_MOVE_SONG_UP,
        CMD_MOVE_SONG_DOWN,

        CMD_STOP,
        CMD_SORT_LIBRARY,
        CMD_VIEW_CHANGED,
        CMD_CROSSFADE,
        CMD_TOGGLECROSSFADE

} CmdType;

typedef struct {
        CmdType type;
        int value;      // generic int (volume delta, enqueue flag, etc.)
} Cmd;

typedef struct {
        Model *model;
        Cmd cmd;
} UpdateResult;

/**
 * @brief Runs a command indicated in result.
 *
 * @param result The UpdateResult that contains the command and related info.
 */
void run_command(UpdateResult result);

/**
 * @brief Checks if a resize is needed and resizes if it is.
 *
 * @return True if a resize occured.
 */
bool resize_if_needed(void);
