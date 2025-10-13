#include "cli.h"
#include "ui/common_ui.h"
#include "ui/player_ui.h"
#include "utils/file.h"
#include "utils/term.h"
#include "utils/utils.h"
#include <pwd.h>

void removeArgElement(char *argv[], int index, int *argc)
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

void handleOptions(int *argc, char *argv[], UISettings *ui, bool *exactSearch)
{
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
                removeArgElement(argv, idx, argc);

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
                removeArgElement(argv, idx, argc);

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
                removeArgElement(argv, idx, argc);

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
                removeArgElement(argv, idx, argc);
}

void setMusicPath(UISettings *ui)
{
        struct passwd *pw = getpwuid(getuid());
        char *user = NULL;

        clearScreen();

        if (pw)
        {
                user = pw->pw_name;
        }
        else
        {
                printf("Please set a path to your music library.\n");
                printf("To set it, type: kew path \"~/Music\".\n");
                exit(1);
        }

        ui->color.r = ui->kewColorRGB.r;
        ui->color.g = ui->kewColorRGB.g;
        ui->color.b = ui->kewColorRGB.b;

        printf("\n\n\n\n");

        printLogoArt(ui, 0, true, true, false);

        printf("\n\n\n\n");

        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

        int indent = calcIndentNormal() + 1;

        // Music folder names in different languages
        const char *musicFolderNames[] = {
            "Music", "Música", "Musique", "Musik", "Musica", "Muziek",
            "Музыка", "音乐", "音楽", "음악", "موسيقى", "संगीत",
            "Müzik", "Musikk", "Μουσική", "Muzyka", "Hudba", "Musiikki",
            "Zene", "Muzică", "เพลง", "מוזיקה"};

        char path[MAXPATHLEN];
        int found = -1;
        char choice[MAXPATHLEN];

        AppSettings *settings = getAppSettings();
        for (size_t i = 0;
             i < sizeof(musicFolderNames) / sizeof(musicFolderNames[0]); i++)
        {
#ifdef __APPLE__
                snprintf(path, sizeof(path), "/Users/%s/%s", user,
                         musicFolderNames[i]);
#else
                snprintf(path, sizeof(path), "/home/%s/%s", user,
                         musicFolderNames[i]);
#endif

                if (directoryExists(path))
                {
                        found = 1;

                        printf("\n\n");

                        printBlankSpaces(indent);

                        printf("Music Library: ");

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        printf("%s\n\n", path);

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->defaultColorRGB);

                        printBlankSpaces(indent);

                        printf("Is this correct? Press Enter.\n\n");

                        printBlankSpaces(indent);

                        printf("Or type a path (no quotes or single-quotes):\n\n");

                        printBlankSpaces(indent);

                        applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                        if (fgets(choice, sizeof(choice), stdin) == NULL)
                        {
                                printBlankSpaces(indent);
                                printf("Error reading input.\n");
                                exit(1);
                        }

                        if (choice[0] == '\n' || choice[0] == '\0')
                        {
                                c_strcpy(settings->path, path, sizeof(settings->path));
                                printBlankSpaces(indent);
                                return;
                        }
                        else
                        {
                                break;
                        }
                }
        }

        printBlankSpaces(indent);

        // No standard music folder was found
        if (found < 1)
        {
                printf("Type a path (no quotes or single-quotes):\n\n");

                printBlankSpaces(indent);

                applyColor(COLOR_MODE_ALBUM, ui->theme.text, ui->kewColorRGB);

                if (fgets(choice, sizeof(choice), stdin) == NULL)
                {
                        printBlankSpaces(indent);
                        printf("Error reading input.\n");
                        exit(1);
                }
        }

        printBlankSpaces(indent);
        choice[strcspn(choice, "\n")] = '\0';

        // Set the path if the chosen directory exists
        if (directoryExists(choice))
        {
                char expanded[MAXPATHLEN];

                expandPath(choice, expanded);

                c_strcpy(settings->path, expanded, sizeof(settings->path));

                found = 1;
        }
        else
        {
                found = -1;
        }

        if (found == -1)
                exit(1);
}
