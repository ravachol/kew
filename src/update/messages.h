#include "common/model.h"

#include <stdbool.h>

/**
 * @brief Dispacthes a message to the update function message queue.
 *
 * @param msg The message
 */
void dispatch_msg(struct Msg msg);

/**
 * @brief returns true if there are more messages in the message queue
 *
 * @return true or false
 */
bool has_pending_msgs(void);

/**
 * @brief returns the next message in the message queue
 * @param msg The message
 * @return True if there is a message.
 */
bool next_msg(struct Msg *msg);

void reset_msg_queue_pointers(void);
