/**
 * @file cli.c
 * @brief Handles command-line related functions
 *
 * Contains the function that shows the welcome screen and sets the path for the first time.
 */

#include "cli.h"

#include "common/appstate.h"

#include "ui/settings.h"
#include "ui/common_ui.h"
#include "ui/player_ui.h"

#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"

#include <pwd.h>

void remove_arg_element(char *argv[], int index, int *argc)
{
        if (index < 0 || index >= *argc)
        {
                // Invalid index
                return;
        }

        // Shift elements after the index
        for (int i = index; i < *argc - 1; i++)
        {
                argv[i] = argv[i + 1];
        }

        // Update the argument count
        (*argc)--;
}

void handle_options(int *argc, char *argv[], bool *exactSearch)
{
        AppState *state = get_app_state();
        UISettings *ui = &(state->uiSettings);
        const char *noUiOption = "--noui";
        const char *noCoverOption = "--nocover";
        const char *quitOnStop = "--quitonstop";
        const char *quitOnStop2 = "-q";
        const char *exactOption = "--exact";
        const char *exactOption2 = "-e";

        int maxLen = 1000;

        int idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noUiOption, maxLen))
                {
                        ui->uiEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], noCoverOption, maxLen))
                {
                        ui->coverEnabled = false;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], quitOnStop, maxLen) ||
                    c_strcasestr(argv[i], quitOnStop2, maxLen))
                {
                        ui->quitAfterStopping = true;
                        idx = i;
                }
        }
        if (idx >= 0)
                remove_arg_element(argv, idx, argc);

        idx = -1;
        for (int i = 0; i < *argc; i++)
        {
                if (c_strcasestr(argv[i], exactOption, maxLen) ||
                    c_strcasestr(argv[i], exactOption2, maxLen))
                {
                        *exactSearch = true;
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

        printf("\n\n\n\n");

        print_logo_art(ui, 0, true, true, false);

        printf("\n\n\n\n");

        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

        int indent = calc_indent_normal() + 1;

        // Music folder names in different languages
        const char *musicFolderNames[] = {
            "Music", "Música", "Musique", "Musik", "Musica", "Muziek",
            "Музыка", "音乐", "音楽", "음악", "موسيقى", "संगीत",
            "Müzik", "Musikk", "Μουσική", "Muzyka", "Hudba", "Musiikki",
            "Zene", "Muzică", "เพลง", "מוזיקה"};

        char path[MAXPATHLEN];
        int found = -1;
        char choice[MAXPATHLEN];

        AppSettings *settings = get_app_settings();
        for (size_t i = 0;
             i < sizeof(musicFolderNames) / sizeof(musicFolderNames[0]); i++)
        {
                snprintf(path, sizeof(path), "~/%s",
                         musicFolderNames[i]);

                if (directory_exists(path))
                {
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
                        printf(_("Or type a path (no quotes or single-quotes):\n\n"));
                        print_blank_spaces(indent);

                        apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        if (fgets(choice, sizeof(choice), stdin) == NULL)
                        {
                                print_blank_spaces(indent);
                                printf(_("Error reading input.\n"));
                                exit(1);
                        }

                        if (choice[0] == '\n' || choice[0] == '\0')
                        {
                                c_strcpy(settings->path, path, sizeof(settings->path));
                                print_blank_spaces(indent);
                                return;
                        }
                        else
                        {
                                break;
                        }
                }
        }

        print_blank_spaces(indent);

        // No standard music folder was found
        if (found < 1)
        {
                printf(_("Type a path (no quotes or single-quotes):\n\n"));
                print_blank_spaces(indent);
                apply_color(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                if (fgets(choice, sizeof(choice), stdin) == NULL)
                {
                        print_blank_spaces(indent);
                        printf(_("Error reading input.\n"));
                        exit(1);
                }
        }

        print_blank_spaces(indent);
        choice[strcspn(choice, "\n")] = '\0';

        // Set the path if the chosen directory exists
        if (directory_exists(choice))
        {
                c_strcpy(settings->path, choice, sizeof(settings->path));
                set_path(settings->path);

                found = 1;
        }
        else
        {
                found = -1;
        }

        if (found == -1)
                exit(1);
}
