#include "pattern_matcher.h"

#include <assert.h>
#include <stdio.h>

static void expect_match(const char *pattern, const char *text, int expected) {
    assert(pattern_matches_text(pattern, text) == expected);
}

static void test_exact_matching(void) {
    expect_match("Discord", "Discord", 1);
    expect_match("discord", "DISCORD", 1);
    expect_match("Discord", "Element", 0);
}

static void test_asterisk_matching(void) {
    expect_match("Game*", "Game", 1);
    expect_match("Game*", "Game1", 1);
    expect_match("Game*", "GameLauncher", 1);

    expect_match("*Launcher", "GameLauncher", 1);
    expect_match("Game*Launcher", "GameLauncher", 1);
    expect_match("Game*Launcher", "GameSuperLauncher", 1);
    expect_match("Game**Launcher", "GameSuperLauncher", 1);
    expect_match("***Game***", "MyGameLauncher", 1);
    expect_match("Game*Launcher", "GameRunner", 0);
}

static void test_question_mark_matching(void) {
    expect_match("Game?", "Game1", 1);
    expect_match("Game?", "Game", 0);
    expect_match("Game?", "Game12", 0);
    expect_match("Game?Client*", "Game1ClientBeta", 1);
    expect_match("*craft?", "Minecraft1", 1);
}

static void test_empty_and_null_inputs(void) {
    expect_match("", "", 1);
    expect_match("", "text", 0);
    expect_match("*", "", 1);
    expect_match("?", "", 0);
    expect_match("*?", "", 0);
    expect_match(NULL, "text", 0);
    expect_match("pattern", NULL, 0);
}

static void test_application_patterns(void) {
    expect_match("Discord*", "Discord Canary", 1);
    expect_match("Counter-Strike*", "Counter-Strike 2", 1);
    expect_match("MyGame*", "MyGameLauncher", 1);

    expect_match("java", "java", 1);
    expect_match("JAVA*", "java", 1);
    expect_match("Minecraft*", "Minecraft Launcher", 1);
    expect_match("java", "Minecraft Launcher", 0);
}

static void test_pattern_text_direction(void) {
    expect_match("Discord*", "Discord Canary", 1);
    expect_match("Discord Canary", "Discord*", 0);
}

int main(void) {
    test_exact_matching();
    test_asterisk_matching();
    test_question_mark_matching();
    test_empty_and_null_inputs();
    test_application_patterns();
    test_pattern_text_direction();
    printf("pattern_matcher tests passed\n");
    return 0;
}
