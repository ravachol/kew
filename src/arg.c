
#include "arg.h"

void removeElement(char *argv[], int index, int *argc)
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

void handleSwitches(int *argc, char *argv[])
{
    const char *nocoverSwitch = "--nocover";
    int idx = -1;
    for (int i = 0; i < *argc; i++)
    {
        if (strcasestr(argv[i], nocoverSwitch))
        {
            coverEnabled = false;
            idx = i;
        }
    }
    if (idx >= 0)
        removeElement(argv, idx, argc);
}