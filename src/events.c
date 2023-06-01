#include "events.h"

struct EventQueue eventQueue;

void initEventQueue() {
    eventQueue.front = 0;
    eventQueue.rear = -1;
    eventQueue.full = false;
}

bool isEventQueueEmpty() {
    return (eventQueue.rear == -1);
}

bool isEventQueueFull() {
    return eventQueue.full;
}

void enqueueEvent(const struct Event* event) {
    if (isEventQueueFull()) {
        return;  // Event queue is full, ignore the event
    }

    eventQueue.rear = (eventQueue.rear + 1) % MAX_EVENTS_IN_QUEUE;
    eventQueue.events[eventQueue.rear] = *event;

    if (eventQueue.front == eventQueue.rear) {
        eventQueue.full = true;  // Event queue is full
    }
}

struct Event dequeueEvent() {
    struct Event event;

    if (isEventQueueEmpty()) {
        return event;  // Event queue is empty, return an empty event
    }

    event = eventQueue.events[eventQueue.front];
    if (eventQueue.front == eventQueue.rear) {
        eventQueue.front = 0;
        eventQueue.rear = -1;
        eventQueue.full = false;  // Event queue is empty
    } else {
        eventQueue.front = (eventQueue.front + 1) % MAX_EVENTS_IN_QUEUE;
    }

    return event;
}
