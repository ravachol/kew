#include "search_ui.h"

#define MAX_SEARCH_LEN 32

int numSearchLetters = 0;
int numSearchBytes = 0;

char searchText[MAX_SEARCH_LEN * 4 + 1]; // unicode can be 4 characters

int displaySearchBox()
{
        printf("   [Search]: ");
        // Save cursor position
        printf("\033[s");
        printf("█\n");

        return 0;
}

int addToSearchText(const char *str)
{
        if (str == NULL)
        {
                return -1;
        }

        size_t len = strlen(str);

        // Check if the string can fit into the search text buffer
        if (numSearchLetters + 1 > MAX_SEARCH_LEN)
        {
                return 0; // Not enough space
        }

        // Restore cursor position
        printf("\033[u");

        // Print the string
        printf("%s", str);

        // Save cursor position
        printf("\033[s");

        printf("█\n");

        // Add the string to the search text buffer
        for (size_t i = 0; i < len; i++)
        {
                searchText[numSearchBytes++] = str[i];
        }

        searchText[numSearchBytes + 1] = '\0'; // Null-terminate the buffer

        numSearchLetters++;

        return 0;
}

// Determine the number of bytes in the last UTF-8 character
int getLastCharBytes(const char *str, int len)
{
        if (len == 0)
                return 0;

        int i = len - 1;
        while (i >= 0 && (str[i] & 0xC0) == 0x80)
        {
                i--;
        }
        return len - i;
}

// Remove the preceding character from the search text
int removeFromSearchText()
{
        if (numSearchLetters == 0)
                return 0;

        // Determine the number of bytes to remove for the last character
        int lastCharBytes = getLastCharBytes(searchText, numSearchBytes);
        if (lastCharBytes == 0)
                return 0;

        // Restore cursor position
        printf("\033[u");

        // Move cursor back one step
        printf("\033[D");

        // Overwrite the character with spaces
        for (int i = 0; i < lastCharBytes; i++)
        {
                printf(" ");
        }

        // Move cursor back again to the original position
        for (int i = 0; i < lastCharBytes; i++)
        {
                printf("\033[D");
        }

        // Save cursor position
        printf("\033[s");

        // Print a block character to represent the cursor
        printf("█");

        // Clear the end of the line
        printf("\033[K");

        fflush(stdout);

        // Remove the character from the buffer
        numSearchBytes -= lastCharBytes;
        searchText[numSearchBytes] = '\0';

        numSearchLetters--;

        return 0;
}

int displaySearchResults()
{
        return 0;
}

int displaySearch()
{
        numSearchBytes = 0;
        numSearchLetters = 0;

        displaySearchBox();
        displaySearchResults();

        return 0;
}
