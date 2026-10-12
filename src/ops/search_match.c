#include "search_match.h"

#include <glib.h>
#include <stdbool.h>
#include <string.h>

enum {
        SCORE_EXACT_SONG = 0,
        SCORE_EXACT_DIRECTORY = 100,
        SCORE_NAME_SEQUENCE = 200,
};

static bool is_apostrophe(gunichar c)
{
        return c == '\'' || c == 0x2019 || c == 0x02BC;
}

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
        bool last_was_separator = true;

        for (const char *p = decomposed; *p; p = g_utf8_next_char(p)) {
                gunichar c = g_utf8_get_char(p);
                GUnicodeType type = g_unichar_type(c);

                if (type == G_UNICODE_NON_SPACING_MARK ||
                    type == G_UNICODE_SPACING_MARK ||
                    type == G_UNICODE_ENCLOSING_MARK || is_apostrophe(c)) {
                        continue;
                }

                if (g_unichar_isalnum(c)) {
                        g_string_append_unichar(result, c);
                        last_was_separator = false;
                } else if (!last_was_separator) {
                        g_string_append_c(result, ' ');
                        last_was_separator = true;
                }
        }

        if (result->len > 0 && result->str[result->len - 1] == ' ')
                g_string_truncate(result, result->len - 1);

        g_free(decomposed);
        return g_string_free(result, FALSE);
}

static int token_match_penalty(const char *query_token, const char *candidate_token)
{
        if (strcmp(query_token, candidate_token) == 0)
                return 0;
        if (g_str_has_prefix(candidate_token, query_token))
                return 2;
        if (strstr(candidate_token, query_token) != NULL)
                return 4;
        return SEARCH_NO_MATCH;
}

/*
 * Matches query words in order while allowing candidate words to be skipped.
 * The returned penalty makes exact words and fewer skipped words rank first.
 */
static int ordered_token_score(const char *normalized_query,
                               const char *normalized_candidate)
{
        if (normalized_query[0] == '\0' || normalized_candidate[0] == '\0')
                return SEARCH_NO_MATCH;

        char **query_tokens = g_strsplit(normalized_query, " ", -1);
        char **candidate_tokens = g_strsplit(normalized_candidate, " ", -1);
        int candidate_count = (int)g_strv_length(candidate_tokens);
        int next_candidate = 0;
        int score = 0;

        for (int qi = 0; query_tokens[qi] != NULL; qi++) {
                int best_index = -1;
                int best_cost = G_MAXINT;

                for (int ci = next_candidate; ci < candidate_count; ci++) {
                        int match_penalty = token_match_penalty(query_tokens[qi],
                                                                candidate_tokens[ci]);
                        if (match_penalty == SEARCH_NO_MATCH)
                                continue;

                        int cost = (ci - next_candidate) * 3 + match_penalty;
                        if (cost < best_cost) {
                                best_cost = cost;
                                best_index = ci;
                        }
                }

                if (best_index < 0) {
                        score = SEARCH_NO_MATCH;
                        break;
                }

                score += best_cost;
                next_candidate = best_index + 1;
        }

        g_strfreev(query_tokens);
        g_strfreev(candidate_tokens);
        return score;
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
        if (strcmp(normalized_query, normalized_name) == 0) {
                score = entry->is_directory ? SCORE_EXACT_DIRECTORY
                                            : SCORE_EXACT_SONG;
        } else {
                int name_score = ordered_token_score(normalized_query,
                                                     normalized_name);
                if (name_score != SEARCH_NO_MATCH)
                        score = SCORE_NAME_SEQUENCE + name_score;
        }

        g_free(normalized_query);
        g_free(normalized_name);
        return score;
}
