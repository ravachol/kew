#include "messages.h"

#include "common/events.h"

#define MAX_MSG_QUEUE 256

typedef struct {
        struct Msg msgs[MAX_MSG_QUEUE];
        int head;
        int tail;
} MsgQueue;

static MsgQueue queue = {0};

void dispatch_msg(struct Msg msg)
{
        int next = (queue.tail + 1) % MAX_MSG_QUEUE;

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

struct Msg next_msg(void)
{
        struct Msg msg = queue.msgs[queue.head];
        queue.head = (queue.head + 1) % MAX_MSG_QUEUE;
        return msg;
}
