#include "common/appstate.h"

void removeArgElement(char *argv[], int index, int *argc);
void handleOptions(int *argc, char *argv[], UISettings *ui, bool *exactSearch);
void setMusicPath(UISettings *ui);
