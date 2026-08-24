#include "headset_poll.h"

#include <stddef.h>

void headset_poll_state_init(headset_poll_state_t *state) {
    if (!state) {
        return;
    }
    state->has_completion = false;
    state->last_completion_ns = 0;
}

bool headset_poll_is_due(const headset_poll_state_t *state, uint64_t now_ns) {
    if (!state) {
        return false;
    }
    if (!state->has_completion) {
        return true;
    }
    if (now_ns < state->last_completion_ns) {
        /* CLOCK_MONOTONIC should not decrease; wait until it catches up. */
        return false;
    }
    return now_ns - state->last_completion_ns >= HEADSET_POLL_INTERVAL_NS;
}

void headset_poll_mark_completed(
    headset_poll_state_t *state,
    uint64_t completion_ns) {
    if (!state) {
        return;
    }
    state->has_completion = true;
    state->last_completion_ns = completion_ns;
}

bool headset_monotonic_timespec_ns(
    const struct timespec *timestamp,
    uint64_t *nanoseconds) {
    uint64_t seconds;
    uint64_t partial_ns;

    if (!timestamp || !nanoseconds || timestamp->tv_sec < (time_t)0 ||
        timestamp->tv_nsec < 0 || timestamp->tv_nsec >= 1000000000L) {
        return false;
    }

    seconds = (uint64_t)timestamp->tv_sec;
    partial_ns = (uint64_t)timestamp->tv_nsec;
    if (seconds > (UINT64_MAX - partial_ns) / UINT64_C(1000000000)) {
        return false;
    }

    *nanoseconds = seconds * UINT64_C(1000000000) + partial_ns;
    return true;
}
