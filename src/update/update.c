#include "update.h"

#include "common/appstate.h"
#include "common/common.h"
#include "common/events.h"
#include "common/model.h"

#include "messages.h"

#include "data/directorytree.h"

#include "ops/library_ops.h"

#include "ops/playback_state.h"
#include "ops/playback_system.h"
#include "ops/playlist_ops.h"
#include "ops/search_ops.h"

#include "sys/sys_integration.h"

#include "ui/anims.h"
#include "ui/common_ui.h"
#include "ui/components.h"
#include "ui/control_ui.h"
#include "ui/visuals.h"

#include "utils/utils.h"

#include <ctype.h>

// kew uses the Model-View-Update pattern.
//
// How Model-View-Update is supposed to work:
//
// Update:
//
// Pure state transitions
// Takes (Model, Msg) -> (Model, Cmds)
// Decides how the model changes
// May emit commands/effects
//
// Commands/Effects:
//
// Side effects
// IO
// Timers
// Network
// File access
// Audio playback
// Threading
// Anything non-deterministic or external


size_t string_hash(const char *str)
{
        if (str == NULL)
                return 0;
        size_t hash = 5381;
        int c;
        while ((c = *str++))
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        return hash;
}

void set_lyrics_line(Model *model)
{
        if (model->songdata_ok)
                model->state.ui.lyrics_line = get_lyrics_line(model->songdata->lyrics, model->elapsed_seconds);

        if (!model->state.ui.lyrics_line || model->state.ui.lyrics_line[0] == '\0')
                return;

        static char prev_line[1024] = "";

        if (strcmp(model->state.ui.lyrics_line, prev_line) == 0)
                return;

        c_strcpy(prev_line, model->state.ui.lyrics_line, sizeof(prev_line) - 1);
        prev_line[sizeof(prev_line) - 1] = '\0';
}

int parse_volume_arg(const char *arg_str)
{
        if (!arg_str || !*arg_str)
                return 0;

        // Make a copy so we can strip characters like '%'
        char buf[64];
        size_t len = 0;

        // Skip leading spaces
        while (*arg_str && isspace((unsigned char)*arg_str))
                arg_str++;

        // Copy allowed characters (+, -, digits)
        while (*arg_str && len < sizeof(buf) - 1) {
                if (*arg_str == '+' || *arg_str == '-' || isdigit((unsigned char)*arg_str))
                        buf[len++] = *arg_str;
                else if (*arg_str == '%')
                        break; // stop at %
                else if (isspace((unsigned char)*arg_str))
                        break; // stop at space
                else
                        break; // stop on anything unexpected
                arg_str++;
        }

        buf[len] = '\0';

        if (len == 0)
                return 0;

        // Convert to integer
        return atoi(buf);
}

void view_changed(Model *model)
{
        if (!model->songdata_ok && model->state.currentView == TRACK_VIEW) {
                model->state.currentView = LIBRARY_VIEW;
        }
        model->state.ui.resetPlaylistDisplay = true;
        model->name_scroll.active = false;
        model->title_delay.active = false;

        set_dirty(DIRTY_ALL);
}

void switch_view(ViewState view_to_show)
{
        Model *model = get_model();

        model->state.last_view = model->state.currentView;

        if (model->state.currentView == view_to_show) {
                model->state.currentView = TRACK_VIEW;
        } else {
                model->state.currentView = view_to_show;
        }

        view_changed(model);
}

void switch_to_prev_view(Model *model)
{
        AppState *state = get_app_state();

        switch (state->currentView) {
        case PLAYLIST_VIEW:
                state->currentView = HELP_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = LIBRARY_VIEW;
                break;
        case SEARCH_VIEW:
                state->currentView =
                    (get_current_song() != NULL) ? TRACK_VIEW : LIBRARY_VIEW;
                break;
        case HELP_VIEW:
                state->currentView = SEARCH_VIEW;
                break;
        }

        view_changed(model);
}

void switch_to_next_view(Model *model)
{
        AppState *state = &model->state;

        switch (state->currentView) {
        case PLAYLIST_VIEW:
                state->currentView = LIBRARY_VIEW;
                break;
        case LIBRARY_VIEW:
                state->currentView =
                    (get_current_song() != NULL) ? TRACK_VIEW : SEARCH_VIEW;
                break;
        case TRACK_VIEW:
                state->currentView = SEARCH_VIEW;
                break;
        case SEARCH_VIEW:
                state->currentView = HELP_VIEW;
                break;
        case HELP_VIEW:
                state->currentView = PLAYLIST_VIEW;
                break;
        }

        view_changed(model);
}

void scroll_next(Model *model)
{
        // FIXME Rewrite all of this garbage
        PlayList *unshuffled_playlist = model->unshuffled_playlist;

        if (model->state.currentView == PLAYLIST_VIEW) {
                model->state.ui.chosen_row++;
                model->state.ui.chosen_row = (model->state.ui.chosen_row >= unshuffled_playlist->count)
                                                 ? unshuffled_playlist->count - 1
                                                 : model->state.ui.chosen_row;
                set_dirty(DIRTY_PLAYLIST);
        } else if (model->state.currentView == LIBRARY_VIEW) {
                model->state.ui.chosen_lib_row++;

                if (model->state.ui.lib_row_count != 0 && model->state.ui.chosen_lib_row > model->state.ui.lib_row_count)
                        model->state.ui.lib_row_count = model->state.ui.lib_row_count - 1;

                if ((model->state.ui.current_lib_entry && (model->state.ui.current_lib_entry->next == NULL || !is_contained_within(model->state.ui.current_lib_entry, model->state.ui.treeCtx.chosen_dir)) &&
                     ((model->state.ui.treeCtx.chosen_dir && model->state.ui.current_lib_entry->id != model->state.ui.treeCtx.chosen_dir->id)))) {

                        if (model->state.settings.collapseTopLevel) {

                                if (!(model->state.ui.current_lib_entry && model->state.ui.current_lib_entry->children &&
                                      model->state.ui.current_lib_entry->parent->id == model->state.ui.treeCtx.chosen_dir->id) ||
                                        (model->state.ui.current_lib_entry->parent->parent && model->state.ui.current_lib_entry->parent->parent->id == model->state.ui.treeCtx.chosen_dir->id) ||
                                (model->state.ui.current_lib_entry->parent->id == model->state.ui.treeCtx.chosen_dir->id && !model->state.ui.current_lib_entry->next))
                                        library_collapse_view(model, 1);
                        } else {
                                library_collapse_view(model, 1);
                        }
                }

                set_dirty(DIRTY_LIBRARY);
        } else if (model->state.currentView == SEARCH_VIEW) {

                model->state.ui.chosen_search_result_row++;

                if ((model->state.ui.current_search_entry && (model->state.ui.current_search_entry->next == NULL || !is_contained_within(model->state.ui.current_search_entry, model->state.ui.chosen_search_dir)) &&
                     ((model->state.ui.chosen_search_dir && model->state.ui.current_search_entry->id != model->state.ui.chosen_search_dir->id))))
                        component_search_helper_collapse_view(model, 1);

                set_dirty(DIRTY_SEARCH);
        } else if (model->state.currentView == TRACK_VIEW && (model->state.ui.showLyricsPage)) {
                model->state.ui.chosen_lyrics_row++;
        }
}

void scroll_prev(Model *model)
{
        // FIXME Rewrite all of this garbage
        if (model->state.currentView == PLAYLIST_VIEW) {

                model->state.ui.chosen_row--;
                model->state.ui.chosen_row = (model->state.ui.chosen_row > 0) ? model->state.ui.chosen_row : 0;
                set_dirty(DIRTY_PLAYLIST);

        } else if (model->state.currentView == LIBRARY_VIEW) {

                model->state.ui.chosen_lib_row = (model->state.ui.chosen_lib_row > 0) ? model->state.ui.chosen_lib_row - 1 : 0;

                if (model->state.ui.current_lib_entry && (model->state.ui.current_lib_entry->parent &&
                                                          (model->state.ui.current_lib_entry == model->state.ui.current_lib_entry->parent->children ||
                                                           (model->state.ui.treeCtx.chosen_dir && model->state.ui.current_lib_entry->id == model->state.ui.treeCtx.chosen_dir->id))))
                        library_collapse_view(model, -1);

                set_dirty(DIRTY_LIBRARY);

        } else if (model->state.currentView == SEARCH_VIEW) {

                model->state.ui.chosen_search_result_row = (model->state.ui.chosen_search_result_row > 0) ? model->state.ui.chosen_search_result_row - 1 : 0;

                if (model->state.ui.current_lib_entry && (model->state.ui.current_search_entry->parent &&
                                                          (model->state.ui.current_search_entry == model->state.ui.current_search_entry->parent->children ||
                                                           (model->state.ui.chosen_search_dir && model->state.ui.current_search_entry->id == model->state.ui.chosen_search_dir->id))))
                        component_search_helper_collapse_view(model, -1);

                set_dirty(DIRTY_SEARCH);
        } else if (model->state.currentView == TRACK_VIEW && (model->state.ui.showLyricsPage)) {
                model->state.ui.chosen_lyrics_row--;
        }
}

void flip_next_page(Model *model)
{
        AppState *state = &model->state;
        PlayList *unshuffled_playlist = get_unshuffled_playlist();

        if (state->currentView == LIBRARY_VIEW) {

                model->state.ui.chosen_lib_row += model->state.ui.max_lib_rows - 1;

                model->state.ui.chosen_lib_row =
                    (model->state.ui.chosen_lib_row >= model->state.ui.lib_row_count)
                        ? model->state.ui.lib_row_count - 1
                        : model->state.ui.chosen_lib_row;

                model->state.ui.check_collapse_top_level = true;
                set_dirty(DIRTY_LIBRARY);

        } else if (state->currentView == PLAYLIST_VIEW) {

                model->state.ui.chosen_row += model->state.ui.max_playlist_rows - 1;
                model->state.ui.chosen_row = (model->state.ui.chosen_row >= unshuffled_playlist->count)
                                                 ? unshuffled_playlist->count - 1
                                                 : model->state.ui.chosen_row;
                set_dirty(DIRTY_PLAYLIST);

        } else if (state->currentView == SEARCH_VIEW) {

                model->state.ui.chosen_search_result_row += model->state.ui.max_search_rows - 1;
                model->state.ui.chosen_search_result_row =
                    (model->state.ui.chosen_search_result_row >= model->state.ui.search_results_count)
                        ? model->state.ui.search_results_count - 1
                        : model->state.ui.chosen_search_result_row;
                set_dirty(DIRTY_SEARCH);
        }
}

void flip_prev_page(Model *model)
{
        if (model->state.currentView == LIBRARY_VIEW) {

                model->state.ui.chosen_lib_row -= model->state.ui.max_lib_rows;
                model->state.ui.chosen_lib_row = (model->state.ui.chosen_lib_row > 0) ? model->state.ui.chosen_lib_row : 0;
                set_dirty(DIRTY_LIBRARY);

        } else if (model->state.currentView == PLAYLIST_VIEW) {

                model->state.ui.chosen_row -= model->state.ui.max_playlist_rows;
                model->state.ui.chosen_row = (model->state.ui.chosen_row > 0) ? model->state.ui.chosen_row : 0;

                set_dirty(DIRTY_PLAYLIST);

        } else if (model->state.currentView == SEARCH_VIEW) {

                model->state.ui.chosen_search_result_row -= model->state.ui.max_search_rows;
                model->state.ui.chosen_search_result_row =
                    (model->state.ui.chosen_search_result_row > 0) ? model->state.ui.chosen_search_result_row : 0;
                set_dirty(DIRTY_SEARCH);
        }
}

void toggle_folder_display(void)
{
        AppState *state = get_app_state();
        AppSettings *settings = get_app_settings();
        state->settings.showFoldersInPlaylist = !state->settings.showFoldersInPlaylist;
        c_strcpy(settings->showFoldersInPlaylist,
                 state->settings.showFoldersInPlaylist ? "1" : "0",
                 sizeof(settings->showFoldersInPlaylist));
        set_dirty(DIRTY_ALL);
}

UpdateResult update(Model *model, struct Msg *msg)
{
        UpdateResult result;
        result.model = model;
        result.cmd.type = CMD_NONE;
        result.cmd.value = 0;

        AppSettings *settings = get_app_settings();

        Node *current = get_current_song();

        switch (msg->type) {

        case MSG_TICK:
                model->songdata = get_current_song_data(model->songdata);

                model->songdata_ok = (model->songdata && model->songdata->metadata &&
                                      !model->songdata->hasErrors && (model->songdata->hasErrors < 1));

#ifndef _WIN32
                advance_title_delay_anim(model);
                advance_name_scroll_anim(model);
                advance_glimmer_anim(model);
#endif

                if (model->songdata_ok)
                        model->song_duration = model->songdata->duration;
                else
                        model->song_duration = 0.0;

                if (!model->songdata_ok && model->state.currentView == TRACK_VIEW &&
                    model->state.settings.repeatState == SOUND_STATE_REPEAT_OFF) {

                        if (!(should_start_playing() &&
                              (model->playbackState.waitingForPlaylist || model->playbackState.waitingForNext) &&
                              determine_next_song(model->playlist))) {
                                // Conditions aren't met for a new song being played soon, switch to library
                                switch_view(LIBRARY_VIEW);
                        }
                }

                if (model->state.currentView == PLAYLIST_VIEW)
                        component_playlist_helper_update_view_state(model);

                if (model->state.currentView == LIBRARY_VIEW)
                        component_library_helper_update_view_state(model);

                model->is_paused = is_paused();
                model->is_stopped = is_stopped();
                model->state.ui.resizeFlag = get_resize_flag();

                model->current_path = model->songdata ? model->songdata->cover_art_path : NULL;
                model->current_hash = model->current_path ? string_hash(model->current_path) : (size_t)-1;

                model->should_refresh = is_refresh_triggered() ||
                                        model->state.ui.isFastForwarding ||
                                        model->state.ui.isRewinding;

                if (!model->is_paused && !model->is_stopped && model->state.ui.render_often)
                        set_dirty(DIRTY_VISUALIZER);

                set_lyrics_line(model);

                if (is_refresh_triggered()) {

                        if (model->songdata_ok) {

                                if (model->songdata->red < 0)
                                        model->songdata->red = model->state.settings.defaultColorRGB.b;

                                if (model->songdata->green < 0)
                                        model->songdata->green = model->state.settings.defaultColorRGB.b;

                                if (model->songdata->blue < 0)
                                        model->songdata->blue = model->state.settings.defaultColorRGB.b;

                                model->state.settings.color.r = (char)model->songdata->red;
                                model->state.settings.color.g = (char)model->songdata->green;
                                model->state.settings.color.b = (char)model->songdata->blue;

                        } else {

                                model->state.settings.color.r = model->state.settings.defaultColorRGB.r;
                                model->state.settings.color.g = model->state.settings.defaultColorRGB.g;
                                model->state.settings.color.b = model->state.settings.defaultColorRGB.b;
                        }
                }

                // FIXME remove this, do all pallette loading in the song loader
                static size_t cached_palette_song_hash = (size_t)-1;
                if (model->current_hash != cached_palette_song_hash) {
                        cached_palette_song_hash = model->current_hash;
                        generate_all_visualizer_palettes(model, model->state.settings.visualizer_height);
                }

                if (model->state.currentView != PLAYLIST_VIEW)
                        model->state.ui.resetPlaylistDisplay = true;

                if (model->state.ui.chroma_start_requested && !model->state.ui.chroma_started &&
                    model->state.currentView == TRACK_VIEW)
                        set_dirty(DIRTY_ALL);

                if (model->library_updated) {
                        set_dirty(DIRTY_ALL);
                        model->library_updated = false;
                }

                if (model->playbackState.notifySwitch)
                        model->state.ui.chosen_lyrics_row = 0;

                result.cmd.type = CMD_TICK;
                break;

        case MSG_RENDERED:
                model->last_cover_path_hash = model->current_hash;
                model->state.last_view = model->state.currentView;
                model->state.ui.resetPlaylistDisplay = false;
                RenderContext *ctx = get_render_context();
                model->state.ui.render_often = ctx->render_often;
                model->state.ui.render_search = ctx->render_search;
                if (model->state.ui.rendered)
                {
                        set_dirty(DIRTY_NONE);
                }
                else {
                        set_dirty(DIRTY_ALL);
                }
                result.cmd.type = CMD_RENDERED;
                break;

        case MSG_LOAD_WAITING_MUSIC:
                result.cmd.type = CMD_LOAD_WAITING_MUSIC;
                break;

        case MSG_ENQUEUE:
                result.cmd.type = CMD_VIEW_ENQUEUE;
                result.cmd.value = 0;
                break;

        case MSG_ENQUEUEANDPLAY:
                result.cmd.type = CMD_VIEW_ENQUEUE;
                result.cmd.value = 1;
                break;

        case MSG_PLAY:
                result.cmd.type = CMD_PLAY;
                break;

        case MSG_PLAY_PAUSE:
                set_dirty(DIRTY_FOOTER | DIRTY_TITLE);
                result.cmd.type = CMD_TOGGLE_PAUSE;
                break;

        case MSG_PAUSE:
                set_dirty(DIRTY_FOOTER | DIRTY_TITLE);
                result.cmd.type = CMD_PAUSE;
                break;

        case MSG_CYCLEVISUALIZERMODE:
                model->state.settings.visualizer_mode++;

                if (model->state.settings.visualizer_mode >= VIZ_OFF)
                        set_dirty(DIRTY_ALL);

                if (model->state.settings.visualizer_mode > VIZ_OFF)
                        model->state.settings.visualizer_mode = 0;

                break;

        case MSG_TOGGLEREPEAT:
                result.cmd.type = CMD_TOGGLE_REPEAT;
                break;

        case MSG_TOGGLEASCII:
                result.cmd.type = CMD_TOGGLE_ASCII;
                break;

        case MSG_TOGGLENOTIFICATIONS:
                model->state.settings.allowNotifications = !model->state.settings.allowNotifications;
                c_strcpy(settings->allowNotifications,
                         model->state.settings.allowNotifications ? "1" : "0",
                         sizeof(settings->allowNotifications));
                break;

        case MSG_SHUFFLE:
                result.cmd.type = CMD_TOGGLE_SHUFFLE;
                break;

        case MSG_SHOWLYRICSPAGE:
                if (model->state.currentView != TRACK_VIEW) {
                        switch_view(TRACK_VIEW);
                        model->state.ui.showLyricsPage = true;
                }
                else {
                        model->state.ui.showLyricsPage = !model->state.ui.showLyricsPage;
                }
                set_dirty(DIRTY_ALL);
                break;

        case MSG_CYCLECOLORMODE:
                result.cmd.type = CMD_CYCLE_COLOR_MODE;
                set_dirty(DIRTY_ALL);
                break;

        case MSG_CYCLETHEMES:
                result.cmd.type = CMD_CYCLE_THEMES;
                break;

        case MSG_CYCLEVISUALIZATION:
                cycle_visualization();
                result.cmd.type = CMD_CYCLE_VISUALIZATION;
                break;

        case MSG_QUIT:
                result.cmd.type = CMD_QUIT;
                break;

        case MSG_SCROLLDOWN:
                scroll_next(model);
                break;

        case MSG_SCROLLUP:
                scroll_prev(model);
                break;

        case MSG_VOLUME_UP:
                result.cmd.type = CMD_VOLUME_CHANGE;
                result.cmd.value = (msg->args[0] != '\0')
                                       ? parse_volume_arg(msg->args)
                                       : 5;
                break;

        case MSG_VOLUME_DOWN:
                result.cmd.type = CMD_VOLUME_CHANGE;
                result.cmd.value = (msg->args[0] != '\0')
                                       ? parse_volume_arg(msg->args)
                                       : -5;
                break;

        case MSG_NEXT:
                result.cmd.type = CMD_NEXT;
                break;

        case MSG_PREV:
                result.cmd.type = CMD_PREV;
                break;

        case MSG_SEEKBACK:
                result.cmd.type = CMD_SEEK_BACK;
                break;

        case MSG_SEEKFORWARD:
                result.cmd.type = CMD_SEEK_FORWARD;
                break;

        case MSG_SEARCH:
                set_dirty(DIRTY_SEARCH);
                result.cmd.type = CMD_SEARCH;
                break;

        case MSG_TOGGLECROSSFADE:
                model->state.settings.always_crossfade = !model->state.settings.always_crossfade;
                set_dirty(DIRTY_FOOTER);
                result.cmd.type = CMD_TOGGLECROSSFADE;
                break;

        case MSG_ADDTOFAVORITESPLAYLIST:
                result.cmd.type = CMD_ADD_TO_FAVORITES;
                break;

        case MSG_EXPORTPLAYLIST:
                result.cmd.type = CMD_EXPORT_PLAYLIST;
                break;

        case MSG_UPDATELIBRARY:
                component_library_helper_reset(model);
                free_search_results();
                set_error_message("Updating Library...");
                result.cmd.type = CMD_UPDATE_LIBRARY;
                break;

        case MSG_NEXT_PAGE:
                flip_next_page(model);
                break;

        case MSG_PREV_PAGE:
                flip_prev_page(model);
                break;
        case MSG_REMOVE:
                result.cmd.type = CMD_REMOVE;
                break;

        case MSG_SHOWPLAYLIST:
                switch_view(PLAYLIST_VIEW);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_SHOWLIBRARY:
                switch_view(LIBRARY_VIEW);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_SHOWTRACK:
                switch_view(TRACK_VIEW);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_SHOWSEARCH:
                switch_view(SEARCH_VIEW);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_SHOWHELP:
                switch_view(HELP_VIEW);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_NEXTVIEW:
                switch_to_next_view(model);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_PREVVIEW:
                switch_to_prev_view(model);
                result.cmd.type = CMD_VIEW_CHANGED;
                break;

        case MSG_CLEARPLAYLIST:
                model->playbackState.waitingForPlaylist = true;
                if (model->state.currentView == TRACK_VIEW)
                        switch_view(LIBRARY_VIEW);
                result.cmd.type = CMD_CLEAR_PLAYLIST;
                break;

        case MSG_CROSSFADE_QUICK:
                if (!current || !current->next)
                        break;
                result.cmd.type = CMD_CROSSFADE;
                result.cmd.value = FADE_QUICK;
                break;

        case MSG_CROSSFADE_MEDIUM:
                if (!current || !current->next)
                        break;

                result.cmd.type = CMD_CROSSFADE;
                result.cmd.value = FADE_MEDIUM;
                break;

        case MSG_CROSSFADE_SLOW:
                if (!current || !current->next)
                        break;

                result.cmd.type = CMD_CROSSFADE;
                result.cmd.value = FADE_SLOW;
                break;

        case MSG_MOVESONGUP:
                result.cmd.type = CMD_MOVE_SONG_UP;
                break;

        case MSG_MOVESONGDOWN:
                result.cmd.type = CMD_MOVE_SONG_DOWN;
                break;

        case MSG_STOP:
                set_dirty(DIRTY_FOOTER);
                result.cmd.type = CMD_STOP;
                break;

        case MSG_SORTLIBRARY:
                component_library_helper_reset(model);
                free_search_results();
                result.cmd.type = CMD_SORT_LIBRARY;
                break;

        case MSG_LIBRARY_ROW_SELECTED:

                model->state.ui.current_lib_entry = msg->current_lib_entry;
                model->state.ui.chosen_lib_row = msg->chosen_lib_row;

                if (model->state.ui.check_collapse_top_level) {

                        FileSystemEntry *first_parent = get_first_parent(model->state.ui.treeCtx.chosen_dir);
                        FileSystemEntry *entry_first_parent = get_first_parent(msg->current_lib_entry);

                        bool same_parent = first_parent && entry_first_parent &&
                                           strcmp(first_parent->full_path, entry_first_parent->full_path) == 0;

                        if (!same_parent) {
                                if (model->state.settings.collapseTopLevel)
                                        library_collapse_directory(model, 1);
                                else
                                        library_collapse_view(model, 1);
                        }
                        model->state.ui.check_collapse_top_level = false;
                }

                model->state.ui.lib_row_count = msg->num_lib_rows;

                if (model->state.ui.current_lib_entry && model->name_scroll.frame > (int)strnlen(model->state.ui.current_lib_entry->name, 256))
                        model->name_scroll.active = false;
                break;

        case MSG_SEARCH_ROW_SELECTED:
                model->state.ui.chosen_search_result_row = msg->chosen_search_result_row;
                model->state.ui.current_search_entry = msg->current_search_entry;
                break;

        case MSG_PROGRESS_ROW_SET:
                model->state.ui.num_progress_bars = msg->region.width / 2;
                model->progressBar.col = msg->region.col + 1;
                model->progressBar.row = msg->region.row + 1;
                model->progressBar.length = msg->region.width;
                if (msg->footer_row != DISABLED_ROW)
                {
                        model->state.ui.footer_row = msg->footer_row + 1;
                        model->state.ui.footer_col = msg->region.col + 1;
                }
                break;

        case MSG_FOOTER_ROW_SET:
                if (msg->footer_row != DISABLED_ROW)
                {
                        model->state.ui.footer_row = msg->footer_row + 1;
                        model->state.ui.footer_col = msg->region.col + 1;
                }
                break;

        case MSG_START_TITLE_ANIM:
#ifndef _WIN32
                start_title_delay(model);
#endif
                break;

        case MSG_TOGGLEFOLDERDISPLAY:
                toggle_folder_display();
                break;

        case MSG_LYRICS_UPDATED:
                model->state.ui.chosen_lyrics_row = msg->lyrics_offset;
                break;

        default:
                model->state.ui.isFastForwarding = false;
                model->state.ui.isRewinding = false;
                break;
        }

        return result;
}
