#include <stdbool.h>

enum EventType
{
    EVENT_NONE,
    EVENT_PLAY_PAUSE,
    EVENT_VOLUME_UP,
    EVENT_VOLUME_DOWN,
    EVENT_NEXT,
    EVENT_PREV,
    EVENT_QUIT,
    EVENT_TOGGLECOVERS,
    EVENT_TOGGLEREPEAT,
    EVENT_TOGGLEEQUALIZER,
    EVENT_TOGGLEBLOCKS,
    EVENT_ADDTOMAINPLAYLIST,
    EVENT_DELETEFROMMAINPLAYLIST,
    EVENT_EXPORTPLAYLIST,
    EVENT_SHUFFLE,
    EVENT_KEY_PRESS
};

struct Event
{
    enum EventType type;
    char key;
};

