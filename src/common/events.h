#ifndef EVENTS_H
#define EVENTS_H

#include "data/directorytree.h"
#include <stdint.h>

#define MAX_SEQ_LEN 1024 // Maximum length of sequence buffer

enum MsgType {
        MSG_NONE,
        MSG_TICK,
        MSG_RENDERED,
        MSG_RESIZE,
        MSG_LOAD_WAITING_MUSIC,
        MSG_PLAY_PAUSE,
        MSG_VOLUME_UP,
        MSG_VOLUME_DOWN,
        MSG_NEXT,
        MSG_PREV,
        MSG_QUIT,
        MSG_TOGGLEREPEAT,
        MSG_TOGGLEVISUALIZER,
        MSG_TOGGLEASCII,
        MSG_ADDTOFAVORITESPLAYLIST,
        MSG_DELETEFROMMAINPLAYLIST,
        MSG_EXPORTPLAYLIST,
        MSG_UPDATELIBRARY,
        MSG_SHUFFLE,
        MSG_KEY_PRESS,
        MSG_SHOWHELP,
        MSG_SHOWPLAYLIST,
        MSG_SHOWSEARCH,
        MSG_ENQUEUE,
        MSG_GOTOBEGINNINGOFPLAYLIST,
        MSG_GOTOENDOFPLAYLIST,
        MSG_CYCLECOLORMODE,
        MSG_SCROLLDOWN,
        MSG_SCROLLUP,
        MSG_SEEK,
        MSG_SEEKBACK,
        MSG_SEEKFORWARD,
        MSG_SHOWLIBRARY,
        MSG_SHOWTRACK,
        MSG_NEXT_PAGE,
        MSG_PREV_PAGE,
        MSG_REMOVE,
        MSG_SEARCH,
        MSG_NEXTVIEW,
        MSG_PREVVIEW,
        MSG_CLEARPLAYLIST,
        MSG_MOVESONGUP,
        MSG_MOVESONGDOWN,
        MSG_ENQUEUEANDPLAY,
        MSG_STOP,
        MSG_SORTLIBRARY,
        MSG_CYCLETHEMES,
        MSG_CYCLEVISUALIZATION,
        MSG_TOGGLENOTIFICATIONS,
        MSG_SHOWLYRICSPAGE,
        MSG_LIBRARY_ROW_SELECTED,
        MSG_SEARCH_ROW_SELECTED,
        MSG_PROGRESS_BARS_SET,
        MSG_START_TITLE_ANIM,
        MSG_TOGGLEFOLDERDISPLAY,
        MSG_LYRICS_UPDATED,
        MSG_CYCLE_VISUALIZER_MODE
};

struct Msg {
        enum MsgType type;
        char key[MAX_SEQ_LEN]; // To store multi-byte characters
        char args[32];

        int chosen_lib_row;
        FileSystemEntry *current_lib_entry;
        int num_lib_rows;

        int chosen_search_result_row;
        FileSystemEntry *current_search_entry;

        int num_progress_bars;

        int lyrics_offset;
};

typedef struct
{
        char *seq;
        enum MsgType eventType;
} EventMapping;

typedef struct {
        uint16_t key; // TB_KEY_* constants, 0 if printable
        uint32_t ch;  // Unicode character for printable keys, 0 otherwise
        uint8_t mods; // MOD_CTRL | MOD_ALT | MOD_SHIFT
        enum MsgType eventType;
        char args[32]; // Optional arguments like "+5%"
} TBKeyBinding;

#endif
