#ifndef ANIMS_H
#define ANIMS_H

#include "common/model.h"

void start_glimmer(Model *model);

void reset_animation(AnimationState *anim);

void advance_name_scroll_anim(Model *model);

void advance_glimmer_anim(Model *model);

void advance_title_delay_anim(Model *model);

#endif
