
#include "common/appstate.h"

#include "common/model.h"
#include "ui/common_ui.h"
#include "utils/utils.h"

void start_glimmer(Model *model)
{
        if (model->state.settings.hideGlimmeringText >= 1)
                return;

        model->glimmer.active = true;
        model->glimmer.frame = -1;

        char text[100] = "";
        get_footer_text(text, sizeof(text));

        model->glimmer.num_frames = strnlen(text, sizeof(text)) + 1; // one extra frame that displays the cursor

        set_dirty(DIRTY_FOOTER);
}

void start_title_delay(Model *model)
{
        if (model->state.settings.titleDelay <= 0 || !model->songdata_ok)
                return;

        if (model->is_paused || model->is_stopped)
                return;

        model->title_delay.active = true;
        model->title_delay.frame = -1;
        model->title_delay.num_frames = strnlen(model->songdata->metadata->title, sizeof(model->songdata->metadata->title));

        set_dirty(DIRTY_TITLE | DIRTY_VISUALIZER);
}

void reset_animation(AnimationState *anim)
{
        anim->active = false;
        anim->color = (PixelData){0};
        anim->frame = -1;
        anim->num_frames = 0;
        anim->pre_anim_delay_frame = 0;
}

void advance_name_scroll_anim(Model *model)
{
        UIState *uis = &model->state.ui;

        static int num_skips = 0;
        int skips_per_frame = 1;
        int start_scrolling_delay = 10;
        bool same_name = false;
        int *previous = &uis->treeCtx.previous_chosen_row;
        int *chosen = &uis->chosen_lib_row;

        if (!(model->state.currentView == PLAYLIST_VIEW || model->state.currentView == LIBRARY_VIEW))
                return;

        if (model->state.currentView == PLAYLIST_VIEW)
        {
                previous = &uis->previous_chosen_song;
                chosen = &uis->chosen_row;

        }

        same_name = (*previous == *chosen);

        if (model->name_scroll.pre_anim_delay_frame < start_scrolling_delay)
                model->name_scroll.pre_anim_delay_frame++;
        else
        {
                if (num_skips > skips_per_frame)
                {
                        num_skips = 0;
                        model->name_scroll.frame++;
                }
                else
                        num_skips++;
        }

        if (!same_name) {
                *previous = *chosen;
                reset_animation(&model->name_scroll);
                model->name_scroll.active = true;
        }

        if (model->name_scroll.active)
                set_dirty(DIRTY_PLAYLIST | DIRTY_LIBRARY | DIRTY_SEARCH);
}

void advance_glimmer_anim(Model *model)
{
        int random_number = get_random_number(1, 808);

        if (model->glimmer.active) {

                model->glimmer.frame += 3;

                if (model->glimmer.frame > model->glimmer.num_frames)
                        model->glimmer.active = false;

                set_dirty(DIRTY_FOOTER);
        }

        if (!model->glimmer.active && random_number == 808 && !model->state.settings.hideGlimmeringText)
                start_glimmer(model);
}

void advance_title_delay_anim(Model *model)
{
        if (model->title_delay.active) {

                model->title_delay.frame += 1;
                if (model->title_delay.frame > model->title_delay.num_frames)
                        model->title_delay.active = false;

                set_dirty(DIRTY_TITLE);
        }
}
