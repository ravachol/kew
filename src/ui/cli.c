/**
 * @file cli.c
 * @brief Handles command-line related functions
 *
 * Contains the function that shows the welcome screen and sets the path for the first time.
 */

#include "cli.h"

#include "common/appstate.h"

#include "ui/common_ui.h"
#include "ui/player_ui.h"
#include "ui/settings.h"

#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <pwd.h>

void remove_arg_element(char *argv[], int index, int *argc)
{
        if (index < 0 || index >= *argc) {
                // Invalid index
                return;
        }

        // Shift elements after the index
        for (int i = index; i < *argc - 1; i++) {
                argv[i] = argv[i + 1];
        }

        // Update the argument count
        (*argc)--;
}

void handle_options(int *argc, char *argv[], bool *exact_search)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        const char *no_ui_option = "--noui";
        const char *no_cover_option = "--nocover";
        const char *quit_on_stop = "--quitonstop";
        const char *quit_on_stop2 = "-q";
        const char *exact_option = "--exact";
        const char *exact_option2 = "-e";

        int max_len = 1000;

        int idx = -1;
        for (int i = 0; i < *argc; i++) {
                if (c_strcasestr(argv[i], no_ui_option, max_len)) {
                        ui->uiEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++) {
                if (c_strcasestr(argv[i], no_cover_option, max_len)) {
                        ui->coverEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++) {
                if (c_strcasestr(argv[i], quit_on_stop, max_len) ||
                    c_strcasestr(argv[i], quit_on_stop2, max_len)) {
                        ui->quitAfterStopping = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++) {
                if (c_strcasestr(argv[i], exact_option, max_len) ||
                    c_strcasestr(argv[i], exact_option2, max_len)) {
                        *exact_search = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);
}

void set_music_path(void)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);

        clear_screen();

        ui->color.r = ui->kewColorRGB.r;
        ui->color.g = ui->kewColorRGB.g;
        ui->color.b = ui->kewColorRGB.b;

        ui->colorMode = COLOR_MODE_ALBUM;

        int row = 4;
        int col = 1;

        print_logo_art(row, col, ui, true, true, false);

        printf("\033[%d;%dH", row + 4, col);

        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

        int indent = calc_indent_normal() + 1;

        // Music folder names in different languages
        const char *music_folder_names[] = {
            "Music", "Música", "Musique", "Musik", "Musica", "Muziek",
            "Музыка", "音乐", "音楽", "음악", "موسيقى", "संगीत",
            "Müzik", "Musikk", "Μουσική", "Muzyka", "Hudba", "Musiikki",
            "Zene", "Muzică", "เพลง", "מוזיקה"};

        char path[PATH_MAX];
        int found = -1;
        char choice[PATH_MAX];

        AppSettings *settings = get_app_settings();
        for (size_t i = 0;
             i < sizeof(music_folder_names) / sizeof(music_folder_names[0]); i++) {
                snprintf(path, sizeof(path), "~/%s",
                         music_folder_names[i]);

                if (directory_exists(path)) {
                        found = 1;

                        printf("\n\n");

                        print_blank_spaces(indent);
                        printf(_("Music Library: "));
                        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);
                        printf("%s\n\n", path);
                        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);
                        print_blank_spaces(indent);
                        printf(_("Is this correct? Press Enter.\n\n"));
                        print_blank_spaces(indent);
                        printf(_("Or type a path:\n\n"));
                        print_blank_spaces(indent);

                        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        if (fgets(choice, sizeof(choice), stdin) == NULL) {
                                print_blank_spaces(indent);
                                printf(_("Error reading input.\n"));
                                exit(1);
                        }

                        if (choice[0] == '\n' || choice[0] == '\0') {
                                c_strcpy(settings->path, path, sizeof(settings->path));
                                print_blank_spaces(indent);
                                return;
                        } else {
                                break;
                        }
                }
        }

        print_blank_spaces(indent);

        // No standard music folder was found
        if (found < 1) {
                printf(_("Type a path:\n\n"));
                print_blank_spaces(indent);
                apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                if (fgets(choice, sizeof(choice), stdin) == NULL) {
                        print_blank_spaces(indent);
                        printf(_("Error reading input.\n"));
                        exit(1);
                }
        }

        print_blank_spaces(indent);
        choice[strcspn(choice, "\n")] = '\0';

        size_t len = strlen(choice);
        if (len > 1 && choice[0] == '"' && choice[len - 1] == '"') {
                memmove(choice, choice + 1, len - 2);
                choice[len - 2] = '\0';
        }

        // Set the path if the chosen directory exists
        if (directory_exists(choice)) {
                c_strcpy(settings->path, choice, sizeof(settings->path));
                set_path(settings->path);

                found = 1;
        } else {
                found = -1;
        }

        if (found == -1)
                exit(1);
}
