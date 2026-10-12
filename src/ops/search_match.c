#include "search_match.h"

#include <glib.h>
#include <stdbool.h>
#include <string.h>

enum {
        SCORE_EXACT_SONG = 0,
        SCORE_EXACT_DIRECTORY = 100,
};

static char *normalize_search_text(const char *text, bool strip_extension)
{
        if (text == NULL)
                return g_strdup("");

        size_t byte_len = strlen(text);
        if (strip_extension) {
                const char *dot = strrchr(text, '.');
                if (dot != NULL && dot != text)
                        byte_len = (size_t)(dot - text);
        }

        char *limited = g_strndup(text, byte_len);
        char *casefolded = g_utf8_casefold(limited, -1);
        g_free(limited);

        if (casefolded == NULL)
                return g_strdup("");

        char *decomposed = g_utf8_normalize(casefolded, -1, G_NORMALIZE_NFD);
        g_free(casefolded);

        if (decomposed == NULL)
                return g_strdup("");

        GString *result = g_string_sized_new(strlen(decomposed));

        for (const char *p = decomposed; *p; p = g_utf8_next_char(p)) {
                gunichar c = g_utf8_get_char(p);
                GUnicodeType type = g_unichar_type(c);

                if (type == G_UNICODE_NON_SPACING_MARK ||
                    type == G_UNICODE_SPACING_MARK ||
                    type == G_UNICODE_ENCLOSING_MARK)
                        continue;

                g_string_append_unichar(result, c);
        }

        g_free(decomposed);
        return g_string_free(result, FALSE);
}

int search_match_entry(const char *query, const FileSystemEntry *entry)
{
        if (query == NULL || entry == NULL || entry->name == NULL)
                return SEARCH_NO_MATCH;

        char *normalized_query = normalize_search_text(query, false);
        char *normalized_name = normalize_search_text(entry->name,
                                                      !entry->is_directory);

        if (normalized_query[0] == '\0' || normalized_name[0] == '\0') {
                g_free(normalized_query);
                g_free(normalized_name);
                return SEARCH_NO_MATCH;
        }

        int score = SEARCH_NO_MATCH;
        if (strcmp(normalized_query, normalized_name) == 0)
                score = entry->is_directory ? SCORE_EXACT_DIRECTORY
                                            : SCORE_EXACT_SONG;

        g_free(normalized_query);
        g_free(normalized_name);
        return score;
}
