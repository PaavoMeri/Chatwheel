#ifndef PATTERN_MATCHER_H
#define PATTERN_MATCHER_H

/*
 * Returns 1 when text matches pattern and 0 otherwise. Matching is
 * case-insensitive; '*' matches zero or more characters and '?' matches
 * exactly one character. An empty pattern matches only empty text. A NULL
 * pattern or text does not match.
 */
int pattern_matches_text(const char *pattern, const char *text);

#endif
