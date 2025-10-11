#ifndef EVENTS_H
#define EVENTS_H

#define MAX_SEQ_LEN 1024    // Maximum length of sequence buffer

enum EventType
{
        EVENT_NONE,
        EVENT_PLAY_PAUSE,
        EVENT_VOLUME_UP,
        EVENT_VOLUME_DOWN,
        EVENT_NEXT,
        EVENT_PREV,
        EVENT_QUIT,
        EVENT_TOGGLEREPEAT,
        EVENT_TOGGLEVISUALIZER,
        EVENT_TOGGLEASCII,
        EVENT_ADDTOFAVORITESPLAYLIST,
        EVENT_DELETEFROMMAINPLAYLIST,
        EVENT_EXPORTPLAYLIST,
        EVENT_UPDATELIBRARY,
        EVENT_SHUFFLE,
        EVENT_KEY_PRESS,
        EVENT_SHOWKEYBINDINGS,
        EVENT_SHOWPLAYLIST,
        EVENT_SHOWSEARCH,
        EVENT_ENQUEUE,
        EVENT_GOTOBEGINNINGOFPLAYLIST,
        EVENT_GOTOENDOFPLAYLIST,
        EVENT_CYCLECOLORMODE,
        EVENT_SCROLLNEXT,
        EVENT_SCROLLPREV,
        EVENT_SEEKBACK,
        EVENT_SEEKFORWARD,
        EVENT_SHOWLIBRARY,
        EVENT_SHOWTRACK,
        EVENT_NEXTPAGE,
        EVENT_PREVPAGE,
        EVENT_REMOVE,
        EVENT_SEARCH,
        EVENT_NEXTVIEW,
        EVENT_PREVVIEW,
        EVENT_CLEARPLAYLIST,
        EVENT_MOVESONGUP,
        EVENT_MOVESONGDOWN,
        EVENT_ENQUEUEANDPLAY,
        EVENT_STOP,
        EVENT_SORTLIBRARY,
        EVENT_CYCLETHEMES,
        EVENT_TOGGLENOTIFICATIONS,
        EVENT_SHOWLYRICSPAGE
};

struct Event
{
        enum EventType type;
        char key[MAX_SEQ_LEN]; // To store multi-byte characters
};

typedef struct
{
        char *seq;
        enum EventType eventType;
} EventMapping;

#endif
