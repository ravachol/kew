#include "messages.h"

#define MAX_MSG_QUEUE 256

typedef struct {
        struct Msg msgs[MAX_MSG_QUEUE];
        size_t head;
        size_t tail;
} MsgQueue;

static MsgQueue queue = {0};

void dispatch_msg(struct Msg msg)
{
        size_t next = (queue.tail + 1) % MAX_MSG_QUEUE;

        if (next == queue.head) {
                return;
        }

        queue.msgs[queue.tail] = msg;
        queue.tail = next;
}

bool has_pending_msgs(void)
{
        return queue.head != queue.tail;
}

bool next_msg(struct Msg *msg)
{
        if (queue.head == queue.tail)
                return false;

        *msg = queue.msgs[queue.head];
        queue.head = (queue.head + 1) % MAX_MSG_QUEUE;
        return true;
}

void reset_msg_queue_pointers(void)
{
    size_t i = queue.head;

    while (i != queue.tail) {
        struct Msg *msg = &queue.msgs[i];

        msg->current_lib_entry = NULL;
        msg->current_search_entry = NULL;

        i = (i + 1) % MAX_MSG_QUEUE;
    }
}
