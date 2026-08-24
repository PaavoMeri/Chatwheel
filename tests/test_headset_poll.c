#include "headset/headset_poll.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_first_poll_is_immediate(void) {
    headset_poll_state_t state;

    headset_poll_state_init(&state);
    assert(headset_poll_is_due(&state, 0));
    assert(headset_poll_is_due(&state, UINT64_MAX));
}

static void test_deadline_boundaries(void) {
    headset_poll_state_t state;

    headset_poll_state_init(&state);
    headset_poll_mark_completed(&state, UINT64_C(1000000000));
    assert(!headset_poll_is_due(&state, UINT64_C(1000000000)));
    assert(!headset_poll_is_due(&state, UINT64_C(1499999999)));
    assert(headset_poll_is_due(&state, UINT64_C(1500000000)));
    assert(headset_poll_is_due(&state, UINT64_C(1500000001)));
}

static void test_completion_reschedules_without_catch_up(void) {
    headset_poll_state_t state;

    headset_poll_state_init(&state);
    headset_poll_mark_completed(&state, UINT64_C(1000000000));
    assert(headset_poll_is_due(&state, UINT64_C(5000000000)));

    /* A late poll completed at 5s; old missed deadlines are discarded. */
    headset_poll_mark_completed(&state, UINT64_C(5000000000));
    assert(!headset_poll_is_due(&state, UINT64_C(5000000000)));
    assert(!headset_poll_is_due(&state, UINT64_C(5499999999)));
    assert(headset_poll_is_due(&state, UINT64_C(5500000000)));
}

static void test_large_monotonic_values_do_not_overflow(void) {
    headset_poll_state_t state;
    uint64_t completion = UINT64_MAX - UINT64_C(1000000000);

    headset_poll_state_init(&state);
    headset_poll_mark_completed(&state, completion);
    assert(!headset_poll_is_due(
        &state,
        completion + HEADSET_POLL_INTERVAL_NS - 1));
    assert(headset_poll_is_due(
        &state,
        completion + HEADSET_POLL_INTERVAL_NS));
    assert(headset_poll_is_due(&state, UINT64_MAX));
}

static void test_decreasing_time_waits_for_previous_completion(void) {
    headset_poll_state_t state;

    headset_poll_state_init(&state);
    headset_poll_mark_completed(&state, UINT64_C(1000000000));
    assert(!headset_poll_is_due(&state, UINT64_C(999999999)));
    assert(!headset_poll_is_due(&state, UINT64_C(1000000000)));
    assert(headset_poll_is_due(&state, UINT64_C(1500000000)));
}

static void test_timespec_conversion_and_invalid_inputs(void) {
    struct timespec timestamp = {
        .tv_sec = 123456789,
        .tv_nsec = 987654321,
    };
    uint64_t nanoseconds = 77;

    assert(headset_monotonic_timespec_ns(&timestamp, &nanoseconds));
    assert(nanoseconds == UINT64_C(123456789987654321));

    assert(!headset_monotonic_timespec_ns(NULL, &nanoseconds));
    assert(!headset_monotonic_timespec_ns(&timestamp, NULL));

    timestamp.tv_sec = -1;
    assert(!headset_monotonic_timespec_ns(&timestamp, &nanoseconds));
    timestamp.tv_sec = 1;
    timestamp.tv_nsec = -1;
    assert(!headset_monotonic_timespec_ns(&timestamp, &nanoseconds));
    timestamp.tv_nsec = 1000000000L;
    assert(!headset_monotonic_timespec_ns(&timestamp, &nanoseconds));

    timestamp.tv_sec =
        (time_t)(UINT64_MAX / UINT64_C(1000000000) + 1);
    timestamp.tv_nsec = 0;
    assert(!headset_monotonic_timespec_ns(&timestamp, &nanoseconds));
}

static void test_null_poll_state_is_safe(void) {
    headset_poll_state_init(NULL);
    headset_poll_mark_completed(NULL, 1000);
    assert(!headset_poll_is_due(NULL, 1000));
}

int main(void) {
    test_first_poll_is_immediate();
    test_deadline_boundaries();
    test_completion_reschedules_without_catch_up();
    test_large_monotonic_values_do_not_overflow();
    test_decreasing_time_waits_for_previous_completion();
    test_timespec_conversion_and_invalid_inputs();
    test_null_poll_state_is_safe();

    printf("headset poll tests passed\n");
    return 0;
}
