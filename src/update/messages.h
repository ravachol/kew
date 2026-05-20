#include "common/events.h"

#include <stdbool.h>

//FIXME docs
void dispatch_msg(struct Msg msg);

/**
 * @brief returns true if there are more messages in the message queue
 *
 * @return true or false
 */
bool has_pending_msgs(void);

/**
 * @brief returns the next message in the message queue
 *
 * @return msg
 */
struct Msg next_msg(void);
