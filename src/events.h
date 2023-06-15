#include <stdbool.h>

#define MAX_EVENTS_IN_QUEUE 1

// Define event types
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
    EVENT_TOGGLEVISUALS,
    EVENT_TOGGLEBLOCKS,
    EVENT_SHUFFLE,
    EVENT_KEY_PRESS
};

// Structure to store events
struct Event
{
    enum EventType type;
    char key;
};

struct EventQueue
{
    struct Event events[MAX_EVENTS_IN_QUEUE];
    int front;
    int rear;
    bool full;
};

extern struct EventQueue eventQueue;

void initEventQueue();

bool isEventQueueEmpty();

bool isEventQueueFull();

void enqueueEvent(const struct Event *event);

struct Event dequeueEvent();