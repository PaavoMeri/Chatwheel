#include "mixer/chatmix_volume.h"
#include "headset/headset.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void expect_targets(float raw,
                           pa_volume_t expected_game,
                           pa_volume_t expected_chat) {
    chatmix_volume_targets_t targets;
    assert(chatmix_volume_targets_calculate(raw, &targets) == 0);
    assert(targets.game.pulse == expected_game);
    assert(targets.chat.pulse == expected_chat);
}

static void expect_unchanged(
    const chatmix_volume_targets_t *actual,
    const chatmix_volume_targets_t *expected) {
    assert(actual->normalized == expected->normalized);
    assert(actual->game.linear == expected->game.linear);
    assert(actual->game.logarithmic == expected->game.logarithmic);
    assert(actual->game.pulse == expected->game.pulse);
    assert(actual->chat.linear == expected->chat.linear);
    assert(actual->chat.logarithmic == expected->chat.logarithmic);
    assert(actual->chat.pulse == expected->chat.pulse);
}

static void expect_target_unchanged(
    const chatmix_volume_target_t *actual,
    const chatmix_volume_target_t *expected) {
    assert(actual->linear == expected->linear);
    assert(actual->logarithmic == expected->logarithmic);
    assert(actual->pulse == expected->pulse);
}

static void expect_targets_failure(float raw) {
    const chatmix_volume_targets_t sentinel = {
        .normalized = 0.25f,
        .game = {
            .linear = 0.75f,
            .logarithmic = 0.5f,
            .pulse = 12345,
        },
        .chat = {
            .linear = 0.25f,
            .logarithmic = 0.125f,
            .pulse = 6789,
        },
    };
    chatmix_volume_targets_t output = sentinel;

    assert(chatmix_volume_targets_calculate(raw, &output) == -1);
    expect_unchanged(&output, &sentinel);
}

static void expect_target_failure(float linear) {
    const chatmix_volume_target_t sentinel = {
        .linear = 0.5f,
        .logarithmic = 0.25f,
        .pulse = 23456,
    };
    chatmix_volume_target_t output = sentinel;

    assert(chatmix_volume_target_calculate(linear, &output) == -1);
    expect_target_unchanged(&output, &sentinel);
}

static void test_endpoints_and_midpoint(void) {
    chatmix_volume_targets_t targets;

    expect_targets((float)CHATMIX_MIN, PA_VOLUME_NORM, PA_VOLUME_MUTED);
    assert(chatmix_volume_targets_calculate(
               (float)CHATMIX_MIN,
               &targets) == 0);
    assert(targets.normalized == 0.0f);
    assert(targets.game.linear == 1.0f);
    assert(targets.chat.linear == 0.0f);

    expect_targets((float)(CHATMIX_MAX / 2), 15745, 15745);
    assert(chatmix_volume_targets_calculate(
               (float)(CHATMIX_MAX / 2),
               &targets) == 0);
    assert(targets.normalized == 0.5f);
    assert(targets.game.linear == 0.5f);
    assert(targets.chat.linear == 0.5f);

    expect_targets((float)CHATMIX_MAX, PA_VOLUME_MUTED, PA_VOLUME_NORM);
    assert(chatmix_volume_targets_calculate(
               (float)CHATMIX_MAX,
               &targets) == 0);
    assert(targets.normalized == 1.0f);
    assert(targets.game.linear == 0.0f);
    assert(targets.chat.linear == 1.0f);
}

static void test_mute_thresholds(void) {
    chatmix_volume_targets_t targets;

    assert(chatmix_volume_targets_calculate(1.0f, &targets) == 0);
    assert(targets.chat.linear == 1.0f / (float)CHATMIX_MAX);
    assert(targets.chat.pulse == PA_VOLUME_MUTED);

    assert(chatmix_volume_targets_calculate(2.0f, &targets) == 0);
    assert(targets.chat.linear == 2.0f / (float)CHATMIX_MAX);
    assert(targets.chat.pulse == 266);

    assert(chatmix_volume_targets_calculate(
               (float)(CHATMIX_MAX - 1),
               &targets) == 0);
    assert(targets.game.linear == 1.0f / (float)CHATMIX_MAX);
    assert(targets.game.pulse == PA_VOLUME_MUTED);

    assert(chatmix_volume_targets_calculate(
               (float)(CHATMIX_MAX - 2),
               &targets) == 0);
    assert(targets.game.linear == 2.0f / (float)CHATMIX_MAX);
    assert(targets.game.pulse == 266);
}

static void test_invalid_input_preserves_output(void) {
    expect_targets_failure((float)(CHATMIX_MIN - 1));
    expect_targets_failure((float)(CHATMIX_MAX + 1));
    expect_targets_failure(NAN);
    expect_targets_failure(INFINITY);
    expect_targets_failure(-INFINITY);
    assert(chatmix_volume_targets_calculate(64.0f, NULL) == -1);

    expect_target_failure(-0.5f);
    expect_target_failure(1.5f);
    expect_target_failure(NAN);
    expect_target_failure(INFINITY);
    expect_target_failure(-INFINITY);
    assert(chatmix_volume_target_calculate(0.5f, NULL) == -1);
}

static void test_valid_targets_do_not_exceed_normal_volume(void) {
    for (int raw = CHATMIX_MIN; raw <= CHATMIX_MAX; raw++) {
        chatmix_volume_targets_t targets;
        assert(chatmix_volume_targets_calculate((float)raw, &targets) == 0);
        assert(targets.game.pulse <= PA_VOLUME_NORM);
        assert(targets.chat.pulse <= PA_VOLUME_NORM);
    }
}

int main(void) {
    test_endpoints_and_midpoint();
    test_mute_thresholds();
    test_invalid_input_preserves_output();
    test_valid_targets_do_not_exceed_normal_volume();

    printf("chatmix_volume tests passed\n");
    return 0;
}
