#ifndef ANIMS_H
#define ANIMS_H

#include "common/model.h"

/**
 * @brief Starts the footer glimmering animation.
 *
 * @param model
 */
void start_glimmer(Model *model);

/**
 * @brief Starts the title animation (in track view).
 *
 * @param model
 */
void start_title_delay(Model *model);

/**
 * @brief Resets an animation.
 *
 * @param anim
 */
void reset_animation(AnimationState *anim);

/**
 * @brief Advances the name scrolling animation.
 *
 * @param model
 */
void advance_name_scroll_anim(Model *model);

/**
 * @brief Advances the footer glimmering animation.
 *
 * @param model
 */
void advance_glimmer_anim(Model *model);

/**
 * @brief Starts the title animation
 *
 * @param model
 */
void advance_title_delay_anim(Model *model);

#endif
