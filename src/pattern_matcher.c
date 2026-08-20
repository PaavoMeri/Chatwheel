#include "pattern_matcher.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

int pattern_matches_text(const char *pattern, const char *text) {
    if (!pattern || !text) return 0;

    if (!strchr(pattern, '*') && !strchr(pattern, '?')) {
        return strcasecmp(pattern, text) == 0;
    }

    const char *pattern_position = pattern;
    const char *text_position = text;

    while (*pattern_position && *text_position) {
        if (*pattern_position == '*') {
            while (*pattern_position == '*') pattern_position++;

            if (!*pattern_position) return 1;

            while (*text_position) {
                if (pattern_matches_text(pattern_position, text_position)) {
                    return 1;
                }
                text_position++;
            }
            return 0;
        }
        if (*pattern_position == '?' ||
            tolower((unsigned char)*pattern_position) ==
                tolower((unsigned char)*text_position)) {
            pattern_position++;
            text_position++;
        } else {
            return 0;
        }
    }

    while (*pattern_position == '*') pattern_position++;

    return *pattern_position == '\0' && *text_position == '\0';
}
