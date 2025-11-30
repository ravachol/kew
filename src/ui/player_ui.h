/**
 * @file player_ui.h
 * @brief Main player screen rendering.
 *
 * Displays current track info, progress bar, and playback status.
 * Acts as the central visual component of the terminal player.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "common/appstate.h"

#include "data/directorytree.h"

#include <stdbool.h>

int print_logo_art(int row, int col, const UISettings *ui, bool centered, bool print_tag_line, bool use_gradient);
int calc_indent_normal(void);
int print_player(SongData *songdata, double elapsed_seconds);
int get_footer_row(void);
int get_footer_col(void);
int get_indent(void);
int print_about_for_version(SongData *songdata);
int get_chosen_row(void);
void flip_next_page(void);
void flip_prev_page(void);
void show_help(void);
void set_chosen_dir(FileSystemEntry *entry);
void set_current_as_chosen_dir(void);
void scroll_next(void);
void scroll_prev(void);
void toggle_show_view(ViewState VIEW_TO_SHOW);
void show_track(void);
void save_library(void);
void free_library(void);
void reset_chosen_dir(void);
void switch_to_next_view(void);
void switch_to_previous_view(void);
void reset_search_result(void);
void set_chosen_row(int row);
void trigger_redraw_side_cover(void);
void refresh_player();
void set_track_title_as_window_title(void);
char *get_library_file_path(void);
bool init_theme(int argc, char *argv[]);
FileSystemEntry *get_chosen_dir(void);
void request_next_visualization(void);
void request_stop_visualization(void);

#endif
