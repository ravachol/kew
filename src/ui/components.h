#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "common/model.h"
#include "data/directorytree.h"

extern const char *LOGO[];

static const int LOGO_WIDTH = 24;
static const int LOGO_HEIGHT = 4;
static const int MIN_COVER_SIZE = 5;

ComponentMsg component_side_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_cover_centered(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_now_playing(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_now_playing_and_artist(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_logo_art(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_logo(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_footer(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_playback_status(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_error_row(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

void component_playlist_helper_update_view_state(Model *model);

ComponentMsg component_playlist_rows(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_playlist_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

FileSystemEntry *get_first_parent(FileSystemEntry *entry);

void component_library_helper_collapse_top_level(Model *model, int direction);

void component_library_helper_collapse_view(Model *model, int diff_rows);

void component_library_helper_reset(Model *model);

void component_library_helper_update_view_state(Model *model);

ComponentMsg component_library_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_library_rows(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_metadata(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_progress_bar(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_time(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_time_simple(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_time_simple_and_vol(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_volume(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_visualizer(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_vis_and_progress_bar(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_timestamped_lyrics(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_lyrics_page(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_portrait_lyrics(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_landscape_lyrics(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_landscape_cover(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_landscape_normal(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_landscape(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_portrait_normal(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_portrait(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_empty(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_track(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_version(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

void component_search_helper_collapse_view(Model *model, int diff_rows);

ComponentMsg component_search(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_search_header(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_search_box(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_search_results(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

ComponentMsg component_help(const Model *model, k_Rect region, DrawBuffer *buf, DirtyFlags dirty);

#endif
