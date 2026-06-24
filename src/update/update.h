#include "common/model.h"

#include "effects.h"

/**
 * @brief Updates the model, the central function in Model-View-Update. Most state maniuplation lives here.
 *
 * @param model
 *
 * @param msg The message that needs to be executed.
 *
 * @return An UpdateResult that contains eventual commands to run after the update.
 */
UpdateResult update(Model *model, struct Msg *msg);

void switch_view(ViewState view_to_show);
